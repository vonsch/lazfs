/*
  Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _CACHE_H_
#define _CACHE_H_

#include <sys/types.h>

/* All cache_* functions below returns -errno as errors */

typedef struct las_cache las_cache_t;

/* Creates and initializes file cache. Returns zero on success */
int cache_create(las_cache_t **cachep);

/* Destroys cache */
void cache_destroy(las_cache_t **cachep);

/* Adds existing decompressed file + it's open fd to cache */
int cache_add(las_cache_t *cache, const char *filename,
	      const char *tmpfilename, int tmpfd);

/* Removes file from cache */
int cache_remove(las_cache_t *cache, const char *filename);

/* Get item from cache. Returns zero if found. */
int cache_get(las_cache_t *cache, const char *filename, char **tmpfilename,
	      int *tmpfd);

#endif
