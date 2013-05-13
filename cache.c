/*
  Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This file contains framework for caching decompressed las files
*/

#include "cache.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

typedef struct file_entry file_entry_t;
struct file_entry {
    char *name; /* Name of requested .las file */
    char *tmpname; /* Name of temporary decompressed .las */
    int tmpfd; /* Open fd of the temporary decompressed .las */
    LIST_ENTRY(file_entry) link;
};

typedef LIST_HEAD(las_cache, file_entry) las_cache_t;

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
		  const char *tmpfilename, int tmpfd)
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

    entry->tmpfd = tmpfd;
    *entryp = entry;

    return 0;

cleanup:
    file_entry_destroy(&entry);

    return -err;
}

int
cache_create(las_cache_t **cachep)
{
    las_cache_t *cache;

    assert(cachep != NULL && *cachep == NULL);

    cache = malloc(sizeof(*cache));
    if (cache == NULL)
	return -errno;

    LIST_INIT(cache);

    *cachep = cache;

    return 0;
}

void
cache_destroy(las_cache_t **cachep)
{
    las_cache_t *cache;
    file_entry_t *entry;

    assert(cachep != NULL && *cachep != NULL);

    cache = *cachep;

    while (cache->lh_first != NULL) {
	entry = cache->lh_first;
	LIST_REMOVE(cache->lh_first, link);
	file_entry_destroy(&entry);
    }

    free(cache);
    *cachep = NULL;
}

int
cache_add(las_cache_t *cache, const char *filename, const char *tmpfilename,
	  int tmpfd)
{
    int err;
    file_entry_t *entry = NULL;

    assert(cache != NULL);

    err = file_entry_create(&entry, filename, tmpfilename, tmpfd);
    if (err)
	return err;

    LIST_INSERT_HEAD(cache, entry, link);

    return 0;
}

void
cache_remove(las_cache_t *cache, const char *filename)
{
    file_entry_t *entry;

    assert(cache != NULL);
    assert(filename != NULL);

    /* FIXME: Should we treat empty cache as error? */
    if (cache->lh_first == NULL)
	return;

    for (entry = cache->lh_first; entry != NULL; entry = entry->link.le_next) {
	if (strcmp(entry->name, filename) == 0) {
	    LIST_REMOVE(entry, link);
	    file_entry_destroy(&entry);
        }
    }

    /* FIXME: Treat cache notfound as error? */
}

int
cache_get(las_cache_t *cache, const char *filename, char **tmpfilename,
	  int *tmpfd)
{
    file_entry_t *entry;

    assert(cache != NULL);
    assert(filename != NULL);
    assert(tmpfd != NULL);

    for (entry = cache->lh_first; entry != NULL; entry = entry->link.le_next) {
	if (strcmp(entry->name, filename) == 0) {
	    *tmpfd = entry->tmpfd;
	    if (tmpfilename)
		*tmpfilename = entry->tmpname;
	    break;
	}
    }

    return (entry != NULL) ? 0 : 1;
}
