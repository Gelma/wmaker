SUBDIRS = po

AUTOMAKE_OPTIONS =

pkgdatadir  = $(datadir)/@PACKAGE@

bin_PROGRAMS = wxcopy wxpaste wdwrite wdread getstyle setstyle convertfonts \
	seticons geticonset wmsetbg wmagnify wmgenmenu wmmenugen wmiv \
	wmcpugraph wmmemgraph wmbattery wmnetgraph wmclock wmeyes \
	wlcpugraph wlmemgraph wlbattery wlnetgraph wlclock wleyes \
	wlmcpugraph wlmmemgraph wlmbattery wlmnetgraph wlmclock wlmeyes

bin_SCRIPTS = wmaker.inst wm-oldmenu2new wkdemenu.pl

EXTRA_DIST = wmaker.inst.in bughint wm-oldmenu2new wkdemenu.pl

AM_CPPFLAGS = \
	 $(DFLAGS) -I$(top_srcdir)/WINGs -I$(top_builddir)/wrlib \
	@HEADER_SEARCH_PATH@ \
	-DETCDIR=\"sysconfdir\" -DDATADIR=\"pkgdatadir\"

liblist= @LIBRARY_SEARCH_PATH@ @INTLIBS@

wdwrite_LDADD = $(top_builddir)/WINGs/libWUtil.la $(liblist)

wdread_LDADD = $(top_builddir)/WINGs/libWUtil.la $(liblist)

wxcopy_LDADD = @XLFLAGS@ @XLIBS@

wxpaste_LDADD = @XLFLAGS@ @XLIBS@

getstyle_LDADD = $(top_builddir)/WINGs/libWUtil.la $(liblist)

getstyle_SOURCES = getstyle.c fontconv.c common.h

setstyle_LDADD = \
	$(top_builddir)/WINGs/libWUtil.la \
	@XLFLAGS@ @XLIBS@ $(liblist)

setstyle_SOURCES = setstyle.c fontconv.c common.h

convertfonts_LDADD = $(top_builddir)/WINGs/libWUtil.la $(liblist)

convertfonts_SOURCES = convertfonts.c fontconv.c common.h

seticons_LDADD= $(top_builddir)/WINGs/libWUtil.la $(liblist)

geticonset_LDADD= $(top_builddir)/WINGs/libWUtil.la $(liblist)

wmagnify_LDADD = \
	$(top_builddir)/WINGs/libWINGs.la \
	$(top_builddir)/WINGs/libWUtil.la \
	$(top_builddir)/wrlib/libwraster.la \
	@XLFLAGS@ @XLIBS@ @INTLIBS@

wmsetbg_LDADD = \
	$(top_builddir)/WINGs/libWINGs.la \
	$(top_builddir)/WINGs/libWUtil.la \
	$(top_builddir)/wrlib/libwraster.la \
	@XLFLAGS@ @LIBXINERAMA@ @XLIBS@ @INTLIBS@

wmgenmenu_LDADD = \
	$(top_builddir)/WINGs/libWUtil.la \
	@INTLIBS@

wmgenmenu_SOURCES = wmgenmenu.c wmgenmenu.h

wmmenugen_LDADD = \
	$(top_builddir)/WINGs/libWUtil.la \
	@INTLIBS@

wmmenugen_SOURCES = wmmenugen.c wmmenugen.h wmmenugen_misc.c \
	wmmenugen_parse_wmconfig.c \
	wmmenugen_parse_xdg.c

wmiv_CFLAGS = @PANGO_CFLAGS@ @PTHREAD_CFLAGS@

wmiv_LDADD = \
	$(top_builddir)/wrlib/libwraster.la \
	$(top_builddir)/WINGs/libWINGs.la \
	@XLFLAGS@ @XLIBS@ @GFXLIBS@ \
	@PANGO_LIBS@ @PTHREAD_LIBS@ @LIBEXIF@ \
	@LIBARCHIVE_LIBS@ @LIBM@ @LIBXRANDR@

wmiv_SOURCES = wmiv.c wmiv.h xdnd.c xdnd.h

wmcpugraph_SOURCES = wmcpugraph.c
wmcpugraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlcpugraph_SOURCES = wmcpugraph.c
wlcpugraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlmcpugraph_SOURCES = wmcpugraph.c
wlmcpugraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm

wmmemgraph_SOURCES = wmmemgraph.c
wmmemgraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlmemgraph_SOURCES = wmmemgraph.c
wlmemgraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlmmemgraph_SOURCES = wmmemgraph.c
wlmmemgraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm

wmbattery_SOURCES = wmbattery.c
wmbattery_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlbattery_SOURCES = wmbattery.c
wlbattery_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlmbattery_SOURCES = wmbattery.c
wlmbattery_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm

wmnetgraph_SOURCES = wmnetgraph.c
wmnetgraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlnetgraph_SOURCES = wmnetgraph.c
wlnetgraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlmnetgraph_SOURCES = wmnetgraph.c
wlmnetgraph_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm

wmclock_SOURCES = wmclock.c
wmclock_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlclock_SOURCES = wmclock.c
wlclock_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlmclock_SOURCES = wmclock.c
wlmclock_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm

wmeyes_SOURCES = wmeyes.c
wmeyes_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wleyes_SOURCES = wmeyes.c
wleyes_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm
wlmeyes_SOURCES = wmeyes.c
wlmeyes_LDADD = $(top_builddir)/WINGs/libWUtil.la @XLFLAGS@ @XLIBS@ $(liblist) -lm

CLEANFILES = wmaker.inst

wmaker.inst: $(srcdir)/wmaker.inst.in ./Makefile
	$(AM_V_GEN)sed -e "s|#pkgdatadir#|$(pkgdatadir)|" \
		-e "s|#sysconfdir#|$(sysconfdir)/WindowMaker|" \
		-e "s|#version#|$(VERSION)|" \
		-e "s|#bindir#|$(bindir)|" \
			$(srcdir)/wmaker.inst.in >wmaker.inst ; \
	chmod 755 wmaker.inst
