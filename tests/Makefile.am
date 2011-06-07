
bin_PROGRAMS = \
	tests

tests_SOURCES = \
	tracker_tests.c\
	main.c \
	tracker_tests.h \
	common.h \
	../src/mogile_fs.h \
	test_server.c \
	test_server.h \
	test_pools.c \
	test_pools.h \
	test_request.c \
	test_request.h \
	test_http_server.c \
	test_http_server.h \
	test_file.c \
	test_file.h \
	test_file_download.c \
	test_file_download.h \
	test_file_upload.c \
	test_file_upload.h \
	test_file_system_download.c \
	test_file_system_download.h \
	test_real_server.c \
	test_real_server.h \
	test_watch.c \
	test_watch.h

tests_LDFLAGS =  \
	-lcunit  \
	-L../src  \
	-lmogile_fs  \
	-lm  \
	-lmicrohttpd  \
	-lvanessa_logger

tests_LDADD = \
	$(MOGILE_FS_LIBS)

tests_CFLAGS = \
	-I$(abs_top_srcdir)/src

AM_CPPFLAGS = \
	$(MOGILE_FS_CFLAGS)\
	$(MOGILE_FS_TESTING_CFLAGS)

## File created by the gnome-build tools

