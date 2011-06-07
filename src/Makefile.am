## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" $(MOGILE_FS_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g

lib_LTLIBRARIES = libmogile_fs.la


libmogile_fs_la_SOURCES = \
	lib.c            \
	tracker.c            \
	logger.c            \
	logger.h            \
	request.c            \
	pool.c            \
	file.c            \
	file_upload.c            \
	file_download.c            \
	watch.c

libmogile_fs_la_LDFLAGS = \
	-lvanessa_logger

libmogile_fs_la_CFLAGS = \
	-lm

libmogile_fs_la_LIBADD = $(MOGILE_FS_LIBS)

include_HEADERS = \
	mogile_fs.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmogile_fs-0.1.pc

EXTRA_DIST = \
	libmogile_fs-0.1.pc.in
