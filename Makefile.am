sbin_PROGRAMS = lazfs

lazfs_SOURCES = cache.h cache.c compress_laz.h compress_laz.c \
	compress_lrzip.h compress_lrzip.c lazfs.c log.h log.c params.h util.h \
	util.c

lazfs_CFLAGS = $(FUSE_CFLAGS)

lazfs_LDADD = $(FUSE_LIBS) # $(LIBS)
