/*
  Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This file contains various helper functions.
*/

#include "params.h"
#include "cache.h"
#include "compress_laz.h"
#include "compress_lrzip.h"
#include "log.h"
#include "util.h"
#include <assert.h>
#include <attr/xattr.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsuid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

char
lazfs_exec_hooks(const char *fpath, const char *suffix)
{
	size_t len;
	int ret;
	const char *laz_suffix;
	struct stat stbuf;

	assert(strlen(suffix) == 4);

	/* Don't exec hooks if requested file exists */
	ret = lstat(fpath, &stbuf);
	if (ret == 0)
		return 0;

	len = strlen(fpath);
	if (len < 4)
		return 0;

	laz_suffix = fpath + len - 4;
	assert(laz_suffix >= fpath);
	if (strncmp(laz_suffix, suffix, 4) == 0)
		return 1;

	return 0;
}

/*
 * All the paths I see are relative to the root of the mounted
 * filesystem. In order to get to the underlying filesystem, I need to
 * have the mountpoint. I'll save it away early on in main(), and then
 * whenever I need a path for something I'll call this to construct
 * it.
 */
void
lazfs_fullpath(char fpath[PATH_MAX], const char *path)
{
	strcpy(fpath, LAZFS_DATA->rootdir);
	strncat(fpath, path, PATH_MAX); /* FIXME: long paths will break here */

	log_debug("    lazfs_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
		  LAZFS_DATA->rootdir, path, fpath);
}

int
lazfs_error(const char *str)
{
	int ret = -errno;

	log_error("    ERROR %s: %s\n", str, strerror(errno));

	return ret;
}

int
lazfs_decompress(int sfd, int dfd)
{
	return lazfs_laz_decompress(sfd, dfd);
}

int
lazfs_compress(int sfd, int dfd)
{
	return lazfs_laz_compress(sfd, dfd);
}

int
lazfs_prepare_tmpfile(const char *path, char *tmppath, int flags, int mode,
		      int *fdp, int *tmpfdp)
{
	int fd = -1, tmpfd = -1, ret;

	assert(path != NULL);
	assert(tmppath != NULL);
	assert(flags != -1 || mode != -1);
	assert(fdp != NULL);
	assert(tmpfdp != NULL);

	log_debug("\nprepare_tmpfile: \"p: %s\", tmpp: \"%s\", fd: \"%d\", "
		  "tmpfd: \"%d\"\n", path, tmppath, *fdp, *tmpfdp);

	if (flags != -1) {
		fd = open(path, flags);
		if (fd < 0) {
			ret = lazfs_error("prepare_tmpfile open");
			goto cleanup;
		}
	} else if (mode != -1) {
		fd = creat(path, mode);
		if (fd == -1) {
			ret = lazfs_error("prepare_tmpfile creat");
			goto cleanup;
		}
	}

	tmpfd = mkstemp(tmppath);
	if (tmpfd == -1) {
		ret = lazfs_error("prepare_tmpfile mkstemp");
		goto cleanup;
	}

	*fdp = fd;
	*tmpfdp = tmpfd;

	return 0;

cleanup:
	if (fd != -1)
		close(fd);
	if (tmpfd != -1) {
		unlink(tmppath);
		close(tmpfd);
	}

	return ret;
}

void
lazfs_finish_tmpfile(char *tmppath, int *fd, int *tmpfd)
{
	int ret;

	assert(tmppath != NULL);
	assert(fd != NULL && *fd > 0);
	assert(tmpfd != NULL && *tmpfd > 0);

	log_debug("\nfinish_tmpfile: tmppath: \"%s\", fd: \"%d\", "
		  "tmpfd: \"%d\"\n", tmppath, *fd, *tmpfd);

	ret = close(*fd);
	assert(ret == 0); /* Close failure indicates a bug */
	*fd = -1;

	ret = close(*tmpfd);
	assert(ret == 0); /* Ditto */
	*tmpfd = -1;

	ret = unlink(tmppath);
	assert(ret == 0); /* Ditto */
}

void
lazfs_setugid(lazfs_ugid_t *ugid)
{
	struct fuse_context *fc;

	fc = fuse_get_context();
	ugid->uid = setfsuid(fc->uid);
	ugid->gid = setfsgid(fc->gid);
}

void
lazfs_restoreugid(const lazfs_ugid_t *ugid)
{
	setfsuid(ugid->uid);
	setfsgid(ugid->gid);
}

#define SIZEATTR "lazfs.size"

int
lazfs_setsize(const char *path, off_t size)
{
	int ret;

	ret = setxattr(path, SIZEATTR, &size, sizeof(size), 0);
	if (ret)
		ret = lazfs_error("lazfs_setsize setxattr");

	return ret;
}

int
lazfs_getsize(const char *path, off_t *size)
{
	int ret;

	ret = getxattr(path, SIZEATTR, size, sizeof(size));
	if (ret)
		ret = lazfs_error("lazfs_getsize getxattr");

	return ret;
}

