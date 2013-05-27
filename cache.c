/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 *
 * This file contains framework for caching decompressed las files
 */

#include "cache.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

typedef struct file_entry file_entry_t;
struct file_entry {
	char *name; /* Name of requested .las file */
	char *tmpname; /* Name of temporary decompressed .las */
	int fd; /* Open fd to compressed .laz */
	int tmpfd; /* Open fd of the temporary decompressed .las */
	int refs; /* Number of external references to this entry */
	char dirty; /* Tracks if compressed file need to be updated */
	LIST_ENTRY(file_entry) link;
};

typedef struct laz_cache {
	pthread_mutex_t lock;
	LIST_HEAD(file_entries, file_entry) entries;
} laz_cache_t;

static void
file_entry_destroy(file_entry_t **entryp)
{
	file_entry_t *entry;

	assert(entryp != NULL && *entryp != NULL);

	entry = *entryp;
	if (entry->tmpname != NULL)
		free(entry->tmpname);
	if (entry->name != NULL)
		free(entry->name);
	free(entry);
	*entryp = NULL;
}

static int
file_entry_create(file_entry_t **entryp, const char *filename,
		  const char *tmpfilename, int fd, int tmpfd)
{
	file_entry_t *entry;
	int err;

	assert(entryp != NULL && *entryp == NULL);
	assert(filename != NULL);
	assert(tmpfilename != NULL);

	entry = malloc(sizeof(*entry));
	if (entry == NULL)
		return -errno;

	memset(entry, 0, sizeof(*entry));

	entry->name = strdup(filename);
	if (entry->name == NULL) {
		err = errno;
		goto cleanup;
	}

	entry->tmpname = strdup(tmpfilename);
	if (entry->tmpname == NULL) {
		err = errno;
		goto cleanup;
	}

	entry->fd = fd;
	entry->tmpfd = tmpfd;
	*entryp = entry;

	return 0;

cleanup:
	file_entry_destroy(&entry);

	return -err;
}

int
cache_create(laz_cache_t **cachep)
{
	laz_cache_t *cache;
	int ret;

	assert(cachep != NULL && *cachep == NULL);

	cache = malloc(sizeof(*cache));
	if (cache == NULL)
		return -errno;

	LIST_INIT(&cache->entries);
	ret = pthread_mutex_init(&cache->lock, NULL);
	assert(ret == 0); /* This should't fail */

	*cachep = cache;

	return 0;
}

void
cache_destroy(laz_cache_t **cachep)
{
	laz_cache_t *cache;
	file_entry_t *entry;
	int ret;

	assert(cachep != NULL && *cachep != NULL);

	cache = *cachep;

	assert(cache->entries.lh_first == NULL);

	while (cache->entries.lh_first != NULL) {
		entry = cache->entries.lh_first;
		LIST_REMOVE(cache->entries.lh_first, link);
		file_entry_destroy(&entry);
	}

	ret = pthread_mutex_destroy(&cache->lock);
	assert(ret == 0); /* This shouldn't fail */

	free(cache);
	*cachep = NULL;
}

int
cache_add(laz_cache_t *cache, const char *filename, const char *tmpfilename,
	  int fd, int tmpfd)
{
	int err = 0;
	file_entry_t *entry = NULL;

	assert(cache != NULL);

	err = file_entry_create(&entry, filename, tmpfilename, fd, tmpfd);
	if (err)
		goto cleanup;

	entry->refs++;
	LIST_INSERT_HEAD(&cache->entries, entry, link);

cleanup:
	return err;
}

void
cache_remove(laz_cache_t *cache, const char *filename)
{
	file_entry_t *entry;

	assert(cache != NULL);
	assert(filename != NULL);

	/* FIXME: Should we treat empty cache as error? */
	if (cache->entries.lh_first == NULL)
		return;

	for (entry = cache->entries.lh_first; entry != NULL; entry = entry->link.le_next) {
		if (strcmp(entry->name, filename) == 0) {
			entry->refs--;
			assert(entry->refs >= 0);
			if (entry->refs == 0) {
				LIST_REMOVE(entry, link);
				file_entry_destroy(&entry);
			}
			return;
		}
	}

	/* FIXME: Treat cache notfound as error? */
}

void
cache_dirty(laz_cache_t *cache, const char *filename)
{
	file_entry_t *entry;

	assert(cache != NULL);
	assert(filename != NULL);

	for (entry = cache->entries.lh_first; entry != NULL; entry = entry->link.le_next) {
		if (strcmp(entry->name, filename) == 0) {
			entry->dirty = 1;
			return;
		}
	}
}

int
cache_get(laz_cache_t *cache, const char *filename, char increfs, laz_cachestat_t *cstat)
{
	file_entry_t *entry;

	assert(cache != NULL);
	assert(filename != NULL);
	assert(cstat != NULL);

	for (entry = cache->entries.lh_first; entry != NULL; entry = entry->link.le_next) {
		if (strcmp(entry->name, filename) == 0) {
			if (increfs)
				entry->refs++;

			cstat->tmppath = entry->tmpname;
			cstat->fd = entry->fd;
			cstat->tmpfd = entry->tmpfd;
			cstat->dirty = entry->dirty;
			cstat->lastref = (entry->refs == 1) ? 1 : 0;
			break;
		}
	}

	return (entry != NULL) ? 0 : 1;
}

void
cache_lock(laz_cache_t *cache)
{
	LOCK(cache->lock);
}

void
cache_unlock(laz_cache_t *cache)
{
	UNLOCK(cache->lock);
}

