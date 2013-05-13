/*
  Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _UTIL_H_
#define _UTIL_H_
#include <string.h>

/* Returns non-zero value if filename ends with ".las" */
char is_lasfile(const char *filename);

/* Decompresses file from opened fd and caches it */
int decompress(const char *name, int fd);

#endif
