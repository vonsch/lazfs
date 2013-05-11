/*
  Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This file contains various helper functions.
*/

#include "util.h"
#include <assert.h>
#include <string.h>

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
