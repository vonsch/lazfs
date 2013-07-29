/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 */

#ifndef _CACHE_H_
#define _CACHE_H_

#include "workq.h"
#include <sys/types.h>

/* All cache_* functions below returns -errno as errors */

typedef struct laz_cache laz_cache_t;

typedef struct laz_cachestat {
	char *tmppath;
	int fd;
	int tmpfd;
	char dirty;
	char lastref;
} laz_cachestat_t;

/* Creates and initializes file cache. Returns zero on success */
int
cache_create(laz_cache_t **cachep);

/* Destroys cache */
void
cache_destroy(laz_cache_t **cachep);

/*
 * Adds file which is not yet decompressed + it's open fd to cache and run
 * decompression in separate thread (via workq). Initial cache
 * external references count is 1.
 */
int
cache_add(laz_cache_t *cache, const char *filename, const char *tmpfilename,
	  int fd, int tmpfd, lazfs_workq_t *workq);

/* Removes file from cache. */
void
cache_remove(laz_cache_t *cache, const char *filename);

/* Mark file in cache as dirty (i.e. it was written to it) */
void
cache_dirty(laz_cache_t *cache, const char *filename);

/* Get item from cache. Returns zero if found. */
int
cache_get(laz_cache_t *cache, const char *filename, char increfs,
	  laz_cachestat_t *cstat);

/* Lock the cache */
void
cache_lock(laz_cache_t *cache);

/* Unlock the cache */
void
cache_unlock(laz_cache_t *cache);

#endif
