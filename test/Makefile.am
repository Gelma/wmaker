## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies

EXTRA_DIST = notest.c run_exit_test.sh run_stress.sh

noinst_PROGRAMS = wtest wm_fsm_test wm_stress wrlib_blend_test wrlib_scale_bench wrlib_xpm_test wings_bag_test

# Standalone tests with a pass/fail exit code and no X server requirement,
# run by `make check`.
TESTS = wrlib_blend_test wrlib_xpm_test wings_bag_test

wtest_SOURCES = wtest.c

wtest_LDADD = $(top_builddir)/wmlib/libWMaker.la @XLFLAGS@ @XLIBS@

wings_bag_test_SOURCES = wings_bag_test.c
wings_bag_test_CPPFLAGS = @XCFLAGS@ -D_DEFAULT_SOURCE -D_GNU_SOURCE \
	-I$(top_srcdir)/WINGs -I$(top_builddir)/WINGs
wings_bag_test_LDADD = $(top_builddir)/WINGs/libWUtil.la \
	$(top_builddir)/wrlib/libwraster.la @XLFLAGS@ @XLIBS@ -lm

wm_stress_SOURCES = wm_stress.c
wm_stress_LDADD = @XLFLAGS@ @XLIBS@ @PTHREAD_LIBS@

AM_CPPFLAGS = -g -D_DEFAULT_SOURCE -D_GNU_SOURCE @XCFLAGS@ -I$(top_srcdir)/wmlib

wm_fsm_test_SOURCES = wm_fsm_test.c

wm_fsm_test_CPPFLAGS = $(shell pkg-config --cflags gtk+-3.0)

wm_fsm_test_LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -lX11

wrlib_blend_test_SOURCES = wrlib_blend_test.c

wrlib_blend_test_CPPFLAGS = @XCFLAGS@ -D_DEFAULT_SOURCE -D_GNU_SOURCE \
	-I$(top_srcdir)/wrlib -I$(top_builddir)/wrlib

wrlib_blend_test_LDADD = $(top_builddir)/wrlib/libwraster.la @XLFLAGS@ @XLIBS@ -lm

wrlib_scale_bench_SOURCES = wrlib_scale_bench.c

wrlib_scale_bench_CPPFLAGS = @XCFLAGS@ -D_DEFAULT_SOURCE -D_GNU_SOURCE \
	-I$(top_srcdir)/wrlib -I$(top_builddir)/wrlib

wrlib_scale_bench_LDADD = $(top_builddir)/wrlib/libwraster.la @XLFLAGS@ @XLIBS@ -lm

wrlib_xpm_test_SOURCES = wrlib_xpm_test.c

wrlib_xpm_test_CPPFLAGS = @XCFLAGS@ -D_DEFAULT_SOURCE -D_GNU_SOURCE \
	-I$(top_srcdir)/wrlib -I$(top_builddir)/wrlib

wrlib_xpm_test_LDADD = $(top_builddir)/wrlib/libwraster.la @XLFLAGS@ @XLIBS@ -lm
