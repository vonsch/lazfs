/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 */

#include "compress_laz.h"
#include "log.h"
#include <errno.h>
#include <liblas/capi/liblas.h>

static int
laz_processfile(int sfd, int dfd, char compress)
{
	LASReaderH reader = NULL;
	LASWriterH writer = NULL;
	LASHeaderH wheader = NULL;
	LASPointH p = NULL;
	int ret = 0;

	/*
	 * FIXME: No logging works here because this function is not called from fuse
	 * thread. It is called from workq thread. */
	//log_debug("\nlaz_processfile: sfd: \"%d\", dfd:\"%d\"\n", sfd, dfd);

	reader = LASReader_CreateFromFile(fdopen(sfd, "r"));
	if (reader == NULL) {
		log_error("    ERROR: LASReader_CreateFromFile failed: %s\n",
		LASError_GetLastErrorMsg());
		/* FIXME: We should return more codes */
		ret = -ENOMEM;
		goto cleanup;
	}

	wheader = LASReader_GetHeader(reader);
	if (wheader == NULL) {
		log_error("    ERROR: LASReaded_GetHeader failed: %s\n",
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

	writer = LASWriter_CreateFromFile(fdopen(dfd, "w"), wheader, LAS_MODE_WRITE);
	if (writer == NULL) {
		log_error("    ERROR: LASWriter_CreateFromFile failed: %s\n",
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
	LASHeader_Destroy(wheader);
	LASReader_Destroy(reader);

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
lazfs_laz_decompress(int sfd, int dfd)
{
	return laz_processfile(sfd, dfd, 0);
}

int
lazfs_laz_compress(int sfd, int dfd)
{
	return laz_processfile(sfd, dfd, 1);
}

