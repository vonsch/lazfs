/*
 * LazFS filesystem
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 *
 * Based on Big Brother File System:
 * Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
 * http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 *
 * This code is derived from function prototypes found /usr/include/fuse/fuse.h
 * Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 * His code is licensed under the LGPLv2.
 * A copy of that code is included in the file fuse.h
 * 
 */

#include "params.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "log.h"
#include "util.h"

/*
 * Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int
lazfs_getattr(const char *path, struct stat *statbuf)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char fpath_laz[PATH_MAX];
	laz_cache_t *cache = LAZFS_DATA->cache;
	char locked = 0;
	off_t size;

	log_debug("\nlazfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
		  path, statbuf);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		/* We got request for .las file */
		strncpy(fpath_laz, fpath, PATH_MAX);
		fpath_laz[PATH_MAX - 1] = '\0';
		fpath_laz[strlen(fpath_laz) - 1] = 'z';

		cache_lock(cache);
		locked = 1;

		retstat = lstat(fpath_laz, statbuf);
		if (retstat != 0) {
			retstat = lazfs_error("lazfs_getattr lstat");
			goto cleanup;
		}

		retstat = lazfs_getsize(fpath_laz, &size);
		if (retstat != 0)
			goto cleanup;

		cache_unlock(cache);
		locked = 0;

		statbuf->st_size = size;
	} else {
		retstat = lstat(fpath, statbuf);
		if (retstat != 0) {
			retstat = lazfs_error("lazfs_getattr lstat");
			goto cleanup;
		}
	}

	log_stat(statbuf);
	retstat = 0;

cleanup:
	if (locked)
		cache_unlock(cache);

	return retstat;
}

/*
 * Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 *
 * Note the system readlink() will truncate and lose the terminating
 * null.  So, the size passed to to the system readlink() must be one
 * less than the size passed to lazfs_readlink()
 * lazfs_readlink() code by Bernardo F Costa (thanks!)
 */
int
lazfs_readlink(const char *path, char *link, size_t size)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_debug("lazfs_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
		  path, link, size);
	lazfs_fullpath(fpath, path);
    
	retstat = readlink(fpath, link, size - 1);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_readlink readlink");
	else  {
		link[retstat] = '\0';
		retstat = 0;
	}

	return retstat;
}

/*
 * Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 *
 * FIXME: shouldn't that comment be "if" there is no.... ?
 */
int
lazfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	lazfs_ugid_t ugid;
    
	log_debug("\nlazfs_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
		  path, mode, dev);
	lazfs_fullpath(fpath, path);

	lazfs_setugid(&ugid);
	// On Linux this could just be 'mknod(path, mode, rdev)' but this
	// is more portable
	if (S_ISREG(mode)) {
		retstat = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (retstat < 0)
			retstat = lazfs_error("lazfs_mknod open");
		else {
			retstat = close(retstat);
			if (retstat < 0)
				retstat = lazfs_error("lazfs_mknod close");
		}
	} else
		if (S_ISFIFO(mode)) {
			retstat = mkfifo(fpath, mode);
			if (retstat < 0)
				retstat = lazfs_error("lazfs_mknod mkfifo");
		} else {
			retstat = mknod(fpath, mode, dev);
			if (retstat < 0)
				retstat = lazfs_error("lazfs_mknod mknod");
	}

	lazfs_restoreugid(&ugid);

	return retstat;
}

/* Create a directory */
int
lazfs_mkdir(const char *path, mode_t mode)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	lazfs_ugid_t ugid;

	log_debug("\nlazfs_mkdir(path=\"%s\", mode=0%3o)\n",
		  path, mode);
	lazfs_fullpath(fpath, path);

	lazfs_setugid(&ugid);

	retstat = mkdir(fpath, mode);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_mkdir mkdir");

	lazfs_restoreugid(&ugid);

	return retstat;
}

/* Remove a file */
int
lazfs_unlink(const char *path)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char path_las[PATH_MAX];

	log_debug("lazfs_unlink(path=\"%s\")\n",
		  path);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		strncpy(path_las, fpath, PATH_MAX);
		path_las[PATH_MAX - 1] = '\0';
		path_las[strlen(path_las) - 1] = 'z';

		retstat = unlink(path_las);
	} else
		retstat = unlink(fpath);

	if (retstat < 0)
		retstat = lazfs_error("lazfs_unlink unlink");

	return retstat;
}

/* Remove a directory */
int
lazfs_rmdir(const char *path)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_debug("lazfs_rmdir(path=\"%s\")\n",
		  path);
	lazfs_fullpath(fpath, path);

	retstat = rmdir(fpath);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_rmdir rmdir");

	return retstat;
}

/* 
 * Create a symbolic link
 * The parameters here are a little bit confusing, but do correspond
 * to the symlink() system call.  The 'path' is where the link points,
 * while the 'link' is the link itself.  So we need to leave the path
 * unaltered, but insert the link into the mounted directory.
 */
int
lazfs_symlink(const char *path, const char *link)
{
	int retstat = 0;
	char flink[PATH_MAX];

	log_debug("\nlazfs_symlink(path=\"%s\", link=\"%s\")\n",
		  path, link);
	lazfs_fullpath(flink, link);

	retstat = symlink(path, flink);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_symlink symlink");

	return retstat;
}

/* Rename a file. Both path and newpath are fs-relative. */
int
lazfs_rename(const char *path, const char *newpath)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char fnewpath[PATH_MAX];

	log_debug("\nlazfs_rename(fpath=\"%s\", newpath=\"%s\")\n",
		  path, newpath);
	lazfs_fullpath(fpath, path);
	lazfs_fullpath(fnewpath, newpath);

	retstat = rename(fpath, fnewpath);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_rename rename");

	return retstat;
}

/* Create a hard link to a file */
int
lazfs_link(const char *path, const char *newpath)
{
	int retstat = 0;
	char fpath[PATH_MAX], fnewpath[PATH_MAX];

	log_debug("\nlazfs_link(path=\"%s\", newpath=\"%s\")\n",
		  path, newpath);
	lazfs_fullpath(fpath, path);
	lazfs_fullpath(fnewpath, newpath);

	retstat = link(fpath, fnewpath);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_link link");

	return retstat;
}

/* Change the permission bits of a file */
int
lazfs_chmod(const char *path, mode_t mode)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_debug("\nlazfs_chmod(fpath=\"%s\", mode=0%03o)\n",
		  path, mode);
	lazfs_fullpath(fpath, path);

	retstat = chmod(fpath, mode);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_chmod chmod");

	return retstat;
}

/* Change the owner and group of a file */
int
lazfs_chown(const char *path, uid_t uid, gid_t gid)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_debug("\nlazfs_chown(path=\"%s\", uid=%d, gid=%d)\n",
		  path, uid, gid);
	lazfs_fullpath(fpath, path);

	retstat = chown(fpath, uid, gid);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_chown chown");

	return retstat;
}

/* Change the size of a file */
int
lazfs_truncate(const char *path, off_t newsize)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_debug("\nlazfs_truncate(path=\"%s\", newsize=%lld)\n",
		  path, newsize);
	lazfs_fullpath(fpath, path);

	retstat = truncate(fpath, newsize);
	if (retstat < 0)
		lazfs_error("lazfs_truncate truncate");

	return retstat;
}

/*
 * Change the access and/or modification times of a file
 * Note: I'll want to change this as soon as 2.6 is in debian testing 
 */
int
lazfs_utime(const char *path, struct utimbuf *ubuf)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char path_las[PATH_MAX];

	log_debug("\nlazfs_utime(path=\"%s\", ubuf=0x%08x)\n",
		  path, ubuf);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		strncpy(path_las, fpath, PATH_MAX);
		path_las[PATH_MAX - 1] = '\0';
		path_las[strlen(path_las) - 1] = 'z';

		retstat = utime(path_las, ubuf);
	} else
		retstat = utime(fpath, ubuf);

	if (retstat < 0)
		retstat = lazfs_error("lazfs_utime utime");

	return retstat;
}

/*
 * File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int
lazfs_open(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	int fd = -1, tmpfd = -1;
	char fpath[PATH_MAX], fpath_laz[PATH_MAX];
	char tmppath[] = "/tmp/lazfs.XXXXXX";
	laz_cache_t *cache = LAZFS_DATA->cache;
	char decompressed = 0, locked = 0;
	laz_cachestat_t cstat;

	log_debug("\nlazfs_open(path\"%s\", fi=0x%08x)\n",
		  path, fi);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		/* We got request for .las file */

		cache_lock(cache);
		locked = 1;
		retstat = cache_get(cache, path, 1, &cstat);
		if (retstat == 0) {
			fd = cstat.fd;
			tmpfd = cstat.tmpfd;
			goto cached;
		}

		strncpy(fpath_laz, fpath, PATH_MAX);
		fpath_laz[PATH_MAX - 1] = '\0';
		fpath_laz[strlen(fpath_laz) - 1] = 'z';

		log_debug("\nlazfs_open: opening laz file \"%s\"\n", fpath_laz);

		/* FIXME: We shouldn't ignore fi->flags */
		retstat = lazfs_prepare_tmpfile(fpath_laz, tmppath, fi->flags, -1, &fd, &tmpfd);
		if (retstat != 0) {
			log_error("lazfs_open: lazfs_prepare_tmpfile failed");
			goto cleanup;
		}

		decompressed = 1;

		retstat = lazfs_decompress(fd, tmpfd);
		if (retstat != 0) {
			log_error("lazfs_open: lazfs_decompress failed");
			goto cleanup;
		}

		retstat = cache_add(cache, path, tmppath, fd, tmpfd);
		if (retstat != 0) {
			log_error("lazfs_open: cache_add failed");
			goto cleanup;
		}
	} else {
		fd = open(fpath, fi->flags);
		if (fd < 0) {
			retstat = lazfs_error("lazfs_open open");
			goto cleanup;
		}
	}
cached:
	cache_unlock(cache);
	fi->fh = fd;
	log_fi(fi);

	return 0;

cleanup:
	if (locked)
		cache_unlock(cache);
	if (decompressed)
		lazfs_finish_tmpfile(tmppath, &fd, &tmpfd);
	if (fd != -1)
		close(fd);

	return retstat;
}

/*
 * Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 *
 * Note:
 * I don't fully understand the documentation above -- it doesn't
 * match the documentation for the read() system call which says it
 * can return with anything up to the amount of data requested. nor
 * with the fusexmp code which returns the amount of data also
 * returned by read.
 */
int
lazfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int retstat = 0;
	int tmpfd = -1;
	laz_cache_t *cache = LAZFS_DATA->cache;
	char fpath[PATH_MAX];
	laz_cachestat_t cstat;
#if 0
	log_debug("\nlazfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
		  path, buf, size, offset, fi);
	log_fi(fi);
#endif
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		cache_lock(cache);
		retstat = cache_get(cache, path, 1, &cstat);
		cache_unlock(cache);
		/* Every read file must have been opened & cached */
		assert(retstat == 0);
		tmpfd = cstat.tmpfd;
	} else
		tmpfd = fi->fh;

	retstat = pread(tmpfd, buf, size, offset);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_read read");

	if (lazfs_exec_hooks(fpath, ".las")) {
		cache_lock(cache);
		cache_remove(cache, path);
		cache_unlock(cache);
	}

	return retstat;
}

/*
 * Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 *
 * Note:
 * As  with read(), the documentation above is inconsistent with the
 * documentation for the write() system call.
 */
int
lazfs_write(const char *path, const char *buf, size_t size, off_t offset,
	    struct fuse_file_info *fi)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	laz_cache_t *cache = LAZFS_DATA->cache;
	laz_cachestat_t cstat;
#if 0
	log_debug("\nlazfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
		  path, buf, size, offset, fi);
	log_fi(fi);
#endif
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		cache_lock(cache);
		retstat = cache_get(cache, path, 1, &cstat);
		cache_unlock(cache);
		assert(retstat == 0);
		if (cstat.dirty == 0)
			cache_dirty(cache, path);

		retstat = pwrite(cstat.tmpfd, buf, size, offset);
		if (retstat < 0)
			retstat = lazfs_error("lazfs_write pwrite");
		cache_lock(cache);
		cache_remove(cache, path);
		cache_unlock(cache);
	} else {
		retstat = pwrite(fi->fh, buf, size, offset);
		if (retstat < 0)
			retstat = lazfs_error("lazfs_write pwrite");
	}

	return retstat;
}

/*
 * Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int
lazfs_statfs(const char *path, struct statvfs *statv)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_debug("\nlazfs_statfs(path=\"%s\", statv=0x%08x)\n",
		  path, statv);
	lazfs_fullpath(fpath, path);

	// get stats for underlying filesystem
	retstat = statvfs(fpath, statv);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_statfs statvfs");

	log_statvfs(statv);

	return retstat;
}

/*
 * Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int
lazfs_flush(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_debug("\nlazfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
	// no need to get fpath on this one, since I work from fi->fh not the path
	log_fi(fi);

	return retstat;
}

/*
 * Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int
lazfs_release(const char *path, struct fuse_file_info *fi)
{
	int ret, retstat = 0, compressfd = -1;
	laz_cache_t *cache = LAZFS_DATA->cache;
	char fpath[PATH_MAX];
	laz_cachestat_t cstat;
	char cpath[PATH_MAX];
	char fpath_laz[PATH_MAX];
	struct stat statbuf;

	log_debug("\nlazfs_release(path=\"%s\", fi=0x%08x)\n",
		  path, fi);
	log_fi(fi);
	lazfs_fullpath(fpath, path);

	// We need to close the file.  Had we allocated any resources
	// (buffers etc) we'd need to free them here as well.

	if (lazfs_exec_hooks(fpath, ".las")) {
		cache_lock(cache);
		cache_get(cache, path, 0, &cstat);
		/* NOTE: tmpfilename becomes invalid after cache_remove call. */
		if (cstat.lastref) {
			if (cstat.dirty) {
				strncpy(fpath_laz, fpath, PATH_MAX);
				fpath_laz[PATH_MAX - 1] = '\0';
				fpath_laz[strlen(fpath_laz) - 1] = 'z';

				/* FIXME: PATH_MAX can be too short */
				ret = snprintf(cpath, PATH_MAX, "%s/lazfs.XXXXXX", LAZFS_DATA->rootdir);
				if (ret + 1 + 6 > PATH_MAX) {
					retstat = -ENAMETOOLONG;
					goto cleanup;
				}

				compressfd = mkstemp(cpath);
				if (compressfd == -1) {
					retstat = -errno;
					goto cleanup;
				}

				ret = lazfs_compress(cstat.tmpfd, compressfd);
				if (ret != 0) {
					retstat = ret;
					goto cleanup;
				}

				ret = fstat(cstat.fd, &statbuf);
				if (ret != 0) {
					retstat = -errno;
					goto cleanup;
				}

				ret = fchown(compressfd, statbuf.st_uid, statbuf.st_gid);
				if (ret != 0) {
					retstat = -errno;
					goto cleanup;
				}

				ret = fchmod(compressfd, statbuf.st_mode);
				if (ret != 0) {
					retstat = -errno;
					goto cleanup;
				}

				ret = rename(cpath, fpath_laz);
				if (ret != 0) {
					retstat = -errno;
					goto cleanup;
				}

				ret = fstat(cstat.tmpfd, &statbuf);
				if (ret != 0) {
					retstat = -errno;
					goto cleanup;
				}

				retstat = lazfs_setsize(fpath_laz, statbuf.st_size);
				if (retstat != 0)
					goto cleanup;
			}
cleanup:
			if (compressfd != -1) {
				ret = close(compressfd);
				if (ret)
					retstat = -ret;
			}
			lazfs_finish_tmpfile(cstat.tmppath, &cstat.fd, &cstat.tmpfd);
		}
		cache_remove(cache, path);
		cache_unlock(cache);
	} else {
		ret = close(fi->fh);
		if (ret)
			retstat = ret;
	}

	return retstat;
}

/*
 * Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int
lazfs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_debug("\nlazfs_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
		  path, datasync, fi);
	log_fi(fi);

	if (datasync)
		retstat = fdatasync(fi->fh);
	else
		retstat = fsync(fi->fh);

	if (retstat < 0)
		lazfs_error("lazfs_fsync fsync");

	return retstat;
}

/* Set extended attributes */
int
lazfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char fpath_laz[PATH_MAX];

	log_debug("\nlazfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
		  path, name, value, size, flags);

	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		/* We got request for .las file */
		strncpy(fpath_laz, fpath, PATH_MAX);
		fpath_laz[PATH_MAX - 1] = '\0';
		fpath_laz[strlen(fpath_laz) - 1] = 'z';
		retstat = lsetxattr(fpath_laz, name, value, size, flags);
	} else
		retstat = lsetxattr(fpath, name, value, size, flags);
	
	if (retstat < 0)
		retstat = lazfs_error("lazfs_setxattr lsetxattr");

	return retstat;
}

/* Get extended attributes */
int
lazfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char fpath_laz[PATH_MAX];

	log_debug("\nlazfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
		  path, name, value, size);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		/* We got request for .las file */
		strncpy(fpath_laz, fpath, PATH_MAX);
		fpath_laz[PATH_MAX - 1] = '\0';
		fpath_laz[strlen(fpath_laz) - 1] = 'z';
		retstat = lgetxattr(fpath_laz, name, value, size);
	} else
		retstat = lgetxattr(fpath, name, value, size);
	
	if (retstat < 0)
		retstat = lazfs_error("lazfs_getxattr lgetxattr");
	else
		log_debug("    value = \"%s\"\n", value);

	return retstat;
}

/* List extended attributes */
int
lazfs_listxattr(const char *path, char *list, size_t size)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char fpath_laz[PATH_MAX];
	char *ptr;

	log_debug("lazfs_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
		  path, list, size);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		/* We got request for .las file */
		strncpy(fpath_laz, fpath, PATH_MAX);
		fpath_laz[PATH_MAX - 1] = '\0';
		fpath_laz[strlen(fpath_laz) - 1] = 'z';
		retstat = llistxattr(fpath_laz, list, size);
	} else
		retstat = llistxattr(fpath, list, size);
	
	if (retstat < 0)
		retstat = lazfs_error("lazfs_listxattr llistxattr");

	log_debug("    returned attributes (length %d):\n", retstat);
	if (size != 0 && list != NULL)
		for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
			log_debug("    \"%s\"\n", ptr);

	return retstat;
}

/* Remove extended attributes */
int
lazfs_removexattr(const char *path, const char *name)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char fpath_laz[PATH_MAX];

	log_debug("\nlazfs_removexattr(path=\"%s\", name=\"%s\")\n",
		  path, name);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		/* We got request for .las file */
		strncpy(fpath_laz, fpath, PATH_MAX);
		fpath_laz[PATH_MAX - 1] = '\0';
		fpath_laz[strlen(fpath_laz) - 1] = 'z';
		retstat = lremovexattr(fpath_laz, name);
	} else
		retstat = lremovexattr(fpath, name);
	
	if (retstat < 0)
		retstat = lazfs_error("lazfs_removexattr lrmovexattr");

	return retstat;
}

/*
 * Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int
lazfs_opendir(const char *path, struct fuse_file_info *fi)
{
	DIR *dp;
	int retstat = 0;
	char fpath[PATH_MAX];

	log_debug("\nlazfs_opendir(path=\"%s\", fi=0x%08x)\n",
		  path, fi);
	lazfs_fullpath(fpath, path);

	dp = opendir(fpath);
	if (dp == NULL)
		retstat = lazfs_error("lazfs_opendir opendir");

	fi->fh = (intptr_t) dp;

	log_fi(fi);

	return retstat;
}

/*
 * Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int
lazfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	      struct fuse_file_info *fi)
{
	int retstat = 0;
	DIR *dp;
	struct dirent *de;
	char path_las[PATH_MAX];

	log_debug("\nlazfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
		  path, buf, filler, offset, fi);
	// once again, no need for fullpath -- but note that I need to cast fi->fh
	dp = (DIR *) (uintptr_t) fi->fh;

	// Every directory contains at least two entries: . and ..  If my
	// first call to the system readdir() returns NULL I've got an
	// error; near as I can tell, that's the only condition under
	// which I can get an error from readdir()
	de = readdir(dp);
	if (de == 0) {
		retstat = lazfs_error("lazfs_readdir readdir");
		return retstat;
	}

	// This will copy the entire directory into the buffer.  The loop exits
	// when either the system readdir() returns NULL, or filler()
	// returns something non-zero.  The first case just means I've
	// read the whole directory; the second means the buffer is full.
	do {
		if (lazfs_exec_hooks(de->d_name, ".laz")) {
			strncpy(path_las, de->d_name, PATH_MAX);
			path_las[PATH_MAX - 1] = '\0';
			path_las[strlen(path_las) - 1] = 's';

			log_debug("calling filler with name %s\n", path_las);
			if (filler(buf, path_las, NULL, 0) != 0) {
				log_error("    ERROR lazfs_readdir filler:  buffer full");
				return -ENOMEM;
			}
		} else {
			log_debug("calling filler with name %s\n", de->d_name);
			if (filler(buf, de->d_name, NULL, 0) != 0) {
				log_error("    ERROR lazfs_readdir filler:  buffer full");
				return -ENOMEM;
			}
		}
	} while ((de = readdir(dp)) != NULL);

	log_fi(fi);

	return retstat;
}

/*
 * Release directory
 *
 * Introduced in version 2.3
 */
int
lazfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_debug("\nlazfs_releasedir(path=\"%s\", fi=0x%08x)\n",
		  path, fi);
	log_fi(fi);

	closedir((DIR *) (uintptr_t) fi->fh);

	return retstat;
}

/*
 * Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 *
 * Note:
 * When exactly is this called?  when a user calls fsync and it
 * happens to be a directory?
 */
int
lazfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_debug("\nlazfs_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
		  path, datasync, fi);
	log_fi(fi);

	return retstat;
}

/*
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 *
 * Undocumented but extraordinarily useful fact:  the fuse_context is
 * set up before this function is called, and
 * fuse_get_context()->private_data returns the user_data passed to
 * fuse_main().  Really seems like either it should be a third
 * parameter coming in here, or else the fact should be documented
 * (and this might as well return void, as it did in older versions of
 * FUSE).
 */
void *
lazfs_init(struct fuse_conn_info *conn)
{
	log_debug("\nlazfs_init()\n");

	return LAZFS_DATA;
}

/*
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void
lazfs_destroy(void *userdata)
{
	log_debug("\nlazfs_destroy(userdata=0x%08x)\n", userdata);
}

/*
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int
lazfs_access(const char *path, int mask)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char path_las[PATH_MAX];

	log_debug("\nlazfs_access(path=\"%s\", mask=0%o)\n",
		  path, mask);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		strncpy(path_las, fpath, PATH_MAX);
		path_las[PATH_MAX - 1] = '\0';
		path_las[strlen(path_las) - 1] = 'z';

		retstat = access(path_las, mask);
	} else
		retstat = access(fpath, mask);

	if (retstat < 0)
		retstat = lazfs_error("lazfs_access access");

	return retstat;
}

/*
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int
lazfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int retstat = 0;
	char fpath[PATH_MAX], fpath_laz[PATH_MAX];
	int fd = -1, tmpfd = -1;
	char tmppath[] = "/tmp/lazfs.XXXXXX";
	laz_cache_t *cache = LAZFS_DATA->cache;
	lazfs_ugid_t ugid;

	log_debug("\nlazfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
		  path, mode, fi);
	lazfs_fullpath(fpath, path);

	lazfs_setugid(&ugid);

	if (lazfs_exec_hooks(fpath, ".las")) {
		strncpy(fpath_laz, fpath, PATH_MAX);
		fpath_laz[PATH_MAX - 1] = '\0';
		fpath_laz[strlen(fpath_laz) - 1] = 'z';

		log_debug("\nlazfs_create: creating laz file \"%s\"\n", fpath_laz);

		/* FIXME: We shouldn't ignore fi->flags */
		retstat = lazfs_prepare_tmpfile(fpath_laz, tmppath, -1, mode, &fd, &tmpfd);
		if (retstat != 0) {
			log_error("lazfs_open: lazfs_prepare_tmpfile failed");
			goto cleanup;
		}

		retstat = cache_add(cache, path, tmppath, fd, tmpfd);
		if (retstat != 0) {
			log_error("lazfs_open: cache_add failed");
			goto cleanup;
		}
	} else {
		fd = creat(fpath, mode);
		if (fd < 0)
			retstat = lazfs_error("lazfs_create creat");
	}

	lazfs_restoreugid(&ugid);

	fi->fh = fd;

	log_fi(fi);

	return 0;

cleanup:
	lazfs_restoreugid(&ugid);
	if (fd != -1)
		close(fd);
	if (tmpfd != -1)
		close(tmpfd);
	return retstat;

	return -ENOSYS;
}

/*
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int
lazfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_debug("\nlazfs_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
		  path, offset, fi);
	log_fi(fi);

	retstat = ftruncate(fi->fh, offset);
	if (retstat < 0)
		retstat = lazfs_error("lazfs_ftruncate ftruncate");

	return retstat;
}

/*
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 *
 * Since it's currently only called after lazfs_create(), and lazfs_create()
 * opens the file, I ought to be able to just use the fd and ignore
 * the path...
 */
int
lazfs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
	int retstat = 0;
	laz_cache_t *cache = LAZFS_DATA->cache;
	struct stat tmpstatbuf;
	char fpath[PATH_MAX];
	laz_cachestat_t cstat;

	log_debug("\nlazfs_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
		  path, statbuf, fi);
	log_fi(fi);
	lazfs_fullpath(fpath, path);

	if (lazfs_exec_hooks(fpath, ".las")) {
		cache_lock(cache);
		retstat = cache_get(cache, path, 1, &cstat);
		cache_unlock(cache);
		/* File must have been already opened via open() */
		assert(retstat == 0);
		retstat = fstat(cstat.tmpfd, &tmpstatbuf);
		if (retstat != 0) {
			retstat = lazfs_error("lazfs_fgetattr, tmpfd fstat");
			goto cleanup;
		}

		retstat = fstat(cstat.fd, statbuf);
		if (retstat < 0) {
			retstat = lazfs_error("lazfs_fgetattr fstat");
			goto cleanup;
		}

		/* Merge attributes to output statbuf */
		statbuf->st_size = tmpstatbuf.st_size;
		statbuf->st_atime = tmpstatbuf.st_atime;
		statbuf->st_mtime = tmpstatbuf.st_mtime;
		statbuf->st_ctime = tmpstatbuf.st_ctime;

cleanup:
		cache_lock(cache);
		cache_remove(cache, path);
		cache_unlock(cache);
	} else {
		retstat = fstat(fi->fh, statbuf);
		if (retstat < 0) {
			retstat = lazfs_error("lazfs_fgetattr fstat");
			return retstat;
		}
	}

	log_stat(statbuf);

	return retstat;
}

struct fuse_operations lazfs_oper = {
	.getattr = lazfs_getattr,
	.readlink = lazfs_readlink,
	// no .getdir -- that's deprecated
	.getdir = NULL,
	.mknod = lazfs_mknod,
	.mkdir = lazfs_mkdir,
	.unlink = lazfs_unlink,
	.rmdir = lazfs_rmdir,
	.symlink = lazfs_symlink,
	.rename = lazfs_rename,
	.link = lazfs_link,
	.chmod = lazfs_chmod,
	.chown = lazfs_chown,
	.truncate = lazfs_truncate,
	.utime = lazfs_utime,
	.open = lazfs_open,
	.read = lazfs_read,
	.write = lazfs_write,
	/** Just a placeholder, don't set */ // huh???
	.statfs = lazfs_statfs,
	.flush = lazfs_flush,
	.release = lazfs_release,
	.fsync = lazfs_fsync,
	.setxattr = lazfs_setxattr,
	.getxattr = lazfs_getxattr,
	.listxattr = lazfs_listxattr,
	.removexattr = lazfs_removexattr,
	.opendir = lazfs_opendir,
	.readdir = lazfs_readdir,
	.releasedir = lazfs_releasedir,
	.fsyncdir = lazfs_fsyncdir,
	.init = lazfs_init,
	.destroy = lazfs_destroy,
	.access = lazfs_access,
	.create = lazfs_create,
	.ftruncate = lazfs_ftruncate,
	.fgetattr = lazfs_fgetattr
};

void
lazfs_usage()
{
	fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
	abort();
}

int
main(int argc, char *argv[])
{
	int fuse_stat;
	struct lazfs_state *lazfs_data;

#if 0
	// bbfs doesn't do any access checking on its own (the comment
	// blocks in fuse.h mention some of the functions that need
	// accesses checked -- but note there are other functions, like
	// chown(), that also need checking!).  Since running bbfs as root
	// will therefore open Metrodome-sized holes in the system
	// security, we'll check if root is trying to mount the filesystem
	// and refuse if it is.  The somewhat smaller hole of an ordinary
	// user doing it with the allow_other flag is still there because
	// I don't want to parse the options string.
	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
		return 1;
	}
#endif

	// Perform some sanity checking on the command line:  make sure
	// there are enough arguments, and that neither of the last two
	// start with a hyphen (this will break if you actually have a
	// rootpoint or mountpoint whose name starts with a hyphen, but so
	// will a zillion other programs)
	if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
		lazfs_usage();

	lazfs_data = malloc(sizeof(struct lazfs_state));
	if (lazfs_data == NULL) {
		perror("main calloc");
		abort();
	}

	/* Initialize .las file cache */
	lazfs_data->cache = NULL;
	if (cache_create(&lazfs_data->cache) != 0) {
		perror("Failed to create .las cache");
		abort();
	}

	// Pull the rootdir out of the argument list and save it in my
	// internal data
	lazfs_data->rootdir = realpath(argv[argc-2], NULL);
	argv[argc-2] = argv[argc-1];
	argv[argc-1] = NULL;
	argc--;

	lazfs_data->logfile = log_open();

	// turn over control to fuse
	fprintf(stderr, "about to call fuse_main\n");
	fuse_stat = fuse_main(argc, argv, &lazfs_oper, lazfs_data);
	fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

	return fuse_stat;
}
