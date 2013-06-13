/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 */

#ifndef _UTIL_H_
#define _UTIL_H_
#include <limits.h>
#include <string.h>
#include <pthread.h>

#define LOCK(mutex) \
	do { \
		if (pthread_mutex_lock(&mutex) != 0) { \
			abort(); /* Failing to acquire lock means a bug */ \
		} \
	} while (0)

#define UNLOCK(mutex) \
	do { \
		if (pthread_mutex_unlock(&mutex) != 0) { \
			abort(); /* Failing to release a lock means a bug */ \
		} \
	} while (0)

/*
 * Returns non-zero value if fpath should be handled specially (i.e.
 * decompressed to /tmp/ background file). Note that fpath _must_ be full path.
 */
char
lazfs_exec_hooks(const char *fpath, const char *suffix);

/* Returns full path of the file */
void
lazfs_fullpath(char fpath[PATH_MAX], const char *path);

/* Decompresses file from source fd to destination fd */
int
lazfs_decompress(int sfd, int dfd);

/* Compresses file from source fd to destination fd */
int
lazfs_compress(int sfd, int dfd);

/*
 * Prepare background decompressed tmpfile
 * 1. Open compressed "path" and return it's fd in "fd"
 * 2. Create temporary file and return it's path in "tmppath" and it's fd in
 *    "tmpfd"
 *
 * Either flags or mode can be -1. In case flags != -1, open() is called on
 * path. In case mode != -1, creat() is called on path.
 *
 * Returns 0 in case of success or -errno in case of failure.
 */
int
lazfs_prepare_tmpfile(const char *path, char *tmppath, int flags, int mode, int *fd,
		      int *tmpfd);

/*
 * Finish decompression and clean all temporary resources (i.e. temporary file).
 * After this call file is no longer decompressed.
 */
void
lazfs_finish_tmpfile(char *tmppath, int *fd, int *tmpfd);

typedef struct {
	uid_t	uid;
	gid_t	gid;
} lazfs_ugid_t;

/* Set and restore user/group ID per user which accessing filesystem */
void
lazfs_setugid(lazfs_ugid_t *ugid);

void
lazfs_restoreugid(const lazfs_ugid_t *ugid);

#endif
