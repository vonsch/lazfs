/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 */

#include "compress_lrzip.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <Lrzip.h>

static void
lrzip_log(void *data, unsigned int level, unsigned int line, const char *file,
	  const char *format, va_list args)
{
	return; /* TODO: We don't support logging now */
	/* We know only two error modes - debug or error */
	if (level == LRZIP_LOG_LEVEL_ERROR)
		log_errorv(format, args);
	else
		log_debugv(format, args);
	
}

static int
lrzip_processfile(int sfd, int dfd, char compress)
{
	static char once = 0;
	int ret = 0;
	FILE *sfp = NULL;
	FILE *dfp = NULL;
	Lrzip *lr = NULL;

	if (!once) {
		once = 1;
		lrzip_init();
	}

	sfp = fdopen(sfd, "rb");
	if (sfp == NULL) {
		ret = -errno;
		return ret;
	}

	dfp = fdopen(dfd, "wb");
	if (dfp == NULL) {
		ret = -errno;
		goto cleanup;
	}

	lr = lrzip_new(compress ? LRZIP_MODE_COMPRESS_LZMA : LRZIP_MODE_DECOMPRESS);
	if (lr == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}

	lrzip_config_env(lr);
	lrzip_log_cb_set(lr, &lrzip_log, NULL);
	lrzip_log_level_set(lr, debug ? LRZIP_LOG_LEVEL_DEBUG : LRZIP_LOG_LEVEL_ERROR);

	if (compress) {
		/* Set the best compression */
		lrzip_flags_set(lr, lrzip_flags_get(lr) | LRZIP_FLAG_UNLIMITED_RAM);
	}

	if (lrzip_file_add(lr, sfp) != true) {
		ret = -ENOMEM;
		goto cleanup;
	}

	lrzip_outfile_set(lr, dfp);
	if (!lrzip_run(lr)) {
		ret = -ENOMEM; /* FIXME: Return better error? */
		goto cleanup;
	}

	lrzip_free(lr);
	fflush(dfp);
	fflush(sfp);

	return 0;

cleanup:
	if (lr != NULL)
		lrzip_free(lr);
	if (dfp != NULL)
		fclose(dfp);
	if (sfp != NULL)
		fclose(sfp);

	return ret;
}

int
lazfs_lrzip_decompress(int sfd, int dfd)
{
	return lrzip_processfile(sfd, dfd, 0);
}

int
lazfs_lrzip_compress(int sfd, int dfd)
{
	return lrzip_processfile(sfd, dfd, 1);
}

