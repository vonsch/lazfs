/*
  Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This file contains various helper functions.
*/

#include "params.h"
#include "cache.h"
#include "log.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <liblas/capi/liblas.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char
lazfs_exec_hooks(const char *fpath)
{
	size_t len;
	int ret;
	const char *laz_suffix;
	struct stat stbuf;

	/* Don't exec hooks if requested file exists */
	ret = lstat(fpath, &stbuf);
	if (ret == 0)
		return 0;

	len = strlen(fpath);
	if (len < 4)
		return 0;

	laz_suffix = fpath + len - 4;
	assert(laz_suffix >= fpath);
	if (strncmp(laz_suffix, ".las", 4) == 0)
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

static int
lazfs_processfile(int sfd, int dfd, char compress)
{
	LASReaderH reader = NULL;
	LASWriterH writer = NULL;
	LASHeaderH wheader = NULL;
	LASPointH p = NULL;
	int ret = 0;

	log_debug("\nprocessing file: sfd: \"%d\", dfd:\"%d\"\n", sfd, dfd);

	reader = LASReader_CreateFd(sfd);
	if (reader == NULL) {
		log_error("    ERROR: LASReader_CreateFd failed: %s\n",
		LASError_GetLastErrorMsg());
		/* FIXME: We should return more codes */
		ret = -ENOMEM;
		goto cleanup;
	}

	wheader = LASHeader_Copy(LASReader_GetHeader(reader));
	if (wheader == NULL) {
		log_error("    ERROR: LASHeader_Copy failed: %s\n",
		LASError_GetLastErrorMsg());
		/* FIXME: Return more codes? */
		ret = -ENOMEM;
		goto cleanup;
	}

	if (LASHeader_SetCompressed(wheader, compress) != 0) {
		log_error("    ERROR: LASHeader_SetCompressed failed: %s\n",
		LASError_GetLastErrorMsg());
		ret = -ENOMEM; /* FIXME: What's more appropriate errno? */
		goto cleanup;
	}

	writer = LASWriter_CreateFd(dfd, wheader, LAS_MODE_WRITE);
	if (writer == NULL) {
		log_error("    ERROR: LASWriter_CreateFd failed: %s\n",
		LASError_GetLastErrorMsg());
		ret = -ENOMEM;
		goto cleanup;
	}

	/* Process point-by-point */
	p = LASReader_GetNextPoint(reader);
	while (p) {
		if (LASWriter_WritePoint(writer, p) != LE_None) {
			log_error("    ERROR: LASWriter_WritePoint failed: %s\n",
			LASError_GetLastErrorMsg());
			/* FIXME: Is there more appropriate errno? */
			ret = -ENOSPC;
			goto cleanup;
		}
		p = LASReader_GetNextPoint(reader);
	}

	LASWriter_Destroy(writer);
	LASReader_Destroy(reader);
	LASHeader_Destroy(wheader);

	return 0;

cleanup:
	if (writer != NULL)
		LASWriter_Destroy(writer);
	if (wheader != NULL)
		LASHeader_Destroy(wheader);
	if (reader != NULL)
		LASReader_Destroy(reader);

	return ret;
}

int
lazfs_decompress(int sfd, int dfd)
{
	return lazfs_processfile(sfd, dfd, 0);
}

int
lazfs_compress(int sfd, int dfd)
{
	return lazfs_processfile(sfd, dfd, 1);
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
