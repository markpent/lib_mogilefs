## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src \
	tests

mogile_fsdocdir = ${prefix}/doc/mogile_fs
mogile_fsdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL


EXTRA_DIST = $(mogile_fsdoc_DATA) \
	build_coverage_report.sh


# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(mogile_fsdocdir)
