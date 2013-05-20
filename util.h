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
exec_hooks(const char *fpath);

/* Returns full path of the file */
void
lazfs_fullpath(char fpath[PATH_MAX], const char *path);

/* Decompresses file from opened fd and caches it */
int
decompress(const char *name, int fd);

#endif
