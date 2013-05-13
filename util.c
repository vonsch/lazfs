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
#include <unistd.h>

char
is_lasfile(const char *filename)
{
    size_t len;
    char *laz_suffix;

    len = strlen(filename);
    if (len < 4)
	return 0;

    laz_suffix = filename + len - 4;
    assert(laz_suffix >= filename);
    if (strncmp(laz_suffix, ".las", 4) == 0)
	return 1;

    return 0;
}

int
bb_error(const char *str)
{
    int ret = -errno;

    log_msg("    ERROR %s: %s\n", str, strerror(errno));

    return ret;
}

int
decompress(const char *name, int fd)
{
    LASReaderH reader = NULL;
    LASWriterH writer = NULL;
    LASHeaderH wheader = NULL;
    LASPointH p = NULL;
    las_cache_t *cache = BB_DATA->cache;
    char tmpfilename[] = "/tmp/lazfs.XXXXXX";
    int tmpfd = -1;
    int retstat = 0;

    log_msg("\ndecompressing file \"%s\"\n", name);

    /* Check if we already decompressed it */
    if (cache_get(cache, name, NULL, &tmpfd) == 0)
	return 0;

    tmpfd = mkstemp(tmpfilename);
    if (tmpfd == -1) {
	log_msg("    ERROR: mkstemp failed failed: %s\n",
		LASError_GetLastErrorMsg());
	retstat = -errno;
	goto cleanup;
    }

    reader = LASReader_CreateFd(fd);
    if (reader == NULL) {
	log_msg("    ERROR: LASReader_CreateFd failed: %s\n",
		LASError_GetLastErrorMsg());
	/* FIXME: We should return more codes */
	retstat = -ENOMEM;
	goto cleanup;
    }

    wheader = LASHeader_Copy(LASReader_GetHeader(reader));
    if (wheader == NULL) {
	log_msg("    ERROR: LASHeader_Copy failed: %s\n",
		LASError_GetLastErrorMsg());
	/* FIXME: Return more codes? */
	retstat = -ENOMEM;
	goto cleanup;
    }

    if (LASHeader_SetCompressed(wheader, 0) != 0) {
	log_msg("    ERROR: LASHeader_SetCompressed failed: %s\n",
	LASError_GetLastErrorMsg());
	retstat = -ENOMEM; /* FIXME: What's more appropriate errno? */
	goto cleanup;
    }

    writer = LASWriter_CreateFd(tmpfd, wheader, LAS_MODE_WRITE);
    if (writer == NULL) {
	log_msg("    ERROR: LASWriter_CreateFd failed: %s\n",
	LASError_GetLastErrorMsg());
	retstat = -ENOMEM;
	goto cleanup;
    }

    /* Decompress point-by-point */
    p = LASReader_GetNextPoint(reader);
    while (p) {
	if (LASWriter_WritePoint(writer, p) != LE_None) {
	    log_msg("    ERROR: LASWriter_WritePoint failed: %s\n",
		     LASError_GetLastErrorMsg());
	    /* FIXME: Is there more appropriate errno? */
	    retstat = -ENOSPC;
	    goto cleanup;
	}
	p = LASReader_GetNextPoint(reader);
    }

    LASWriter_Destroy(writer);
    LASReader_Destroy(reader);
    LASHeader_Destroy(wheader);

    /* Cache that this .laz file is already decompressed */
    retstat = cache_add(cache, name, tmpfilename, tmpfd);
    if (retstat != 0) {
	log_msg("    ERROR: bb_open - cache_add failed\n");
	goto cleanup;
    }

    return 0;

cleanup:
    if (writer != NULL)
        LASWriter_Destroy(writer);
    if (wheader != NULL)
        LASHeader_Destroy(wheader);
    if (reader != NULL)
        LASReader_Destroy(reader);
    if (tmpfd != -1) {
        close(tmpfd);
        unlink(tmpfilename);
    }

    return retstat;
}
