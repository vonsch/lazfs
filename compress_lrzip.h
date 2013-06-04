/*
 * Copyright (C) 2013 Adam Tkac <vonsch@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPLv3.
 * See the file COPYING.
 */

#ifndef _COMPRESS_LRZIP_H_
#define _COMPRESS_LRZIP_H_

int
lazfs_lrzip_decompress(int sfd, int dfd);

int
lazfs_lrzip_compress(int sfd, int dfd);

#endif
