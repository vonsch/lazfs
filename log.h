/*
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _LOG_H_
#define _LOG_H_
#include "params.h"
#include <fuse.h>
#include <stdio.h>

extern char debug;

//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_debug("    " #field " = " #format "\n", typecast st->field)

FILE *log_open(void);
void log_fi (struct fuse_file_info *fi);
void log_stat(struct stat *si);
void log_statvfs(struct statvfs *sv);
void log_utime(struct utimbuf *buf);

void log_debug(const char *format, ...);
void log_error(const char *format, ...);

// Report errors to logfile and give -errno to caller
int lazfs_error(const char *str);

#endif
