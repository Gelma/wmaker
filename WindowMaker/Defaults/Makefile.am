pkgconf_DATA = WMRootMenu WMWindowAttributes WindowMaker WMState WMGLOBAL

wpexecbindir = @wprefs_bindir@

EXTRA_DIST = WMGLOBAL WMWindowAttributes.in WindowMaker.in WMState.in

CLEANFILES = WMWindowAttributes WindowMaker WMState WMRootMenu

WMWindowAttributes: $(srcdir)/WMWindowAttributes.in
	$(AM_V_GEN)sed -e "s:#extension#:@ICONEXT@:" \
		$(srcdir)/WMWindowAttributes.in > WMWindowAttributes ; \
	chmod 644 WMWindowAttributes

# The *.in files here are PropertyLists, not C: the parser rejects comments
# ("Comments are not allowed inside WindowMaker owned domain files",
# WINGs/proplist.c) and a single /* ... */ makes the WHOLE domain fail to
# load.  Document new keys in SpecAI.md, never inside the .in.
WindowMaker: $(srcdir)/WindowMaker.in
	$(AM_V_GEN)sed -e "s:#pkgdatadir#:$(pkgdatadir):" \
		-e "s:#pixmapdir#:$(pixmapdir):" $(srcdir)/WindowMaker.in \
		> WindowMaker ; \
	chmod 644 WindowMaker

WMState: $(srcdir)/WMState.in
	$(AM_V_GEN)sed -e "s:#wprefs#:$(wpexecbindir)/WPrefs:" $(srcdir)/WMState.in > WMState ; \
	chmod 644 WMState

WMRootMenu: ./../plmenu
	$(AM_V_GEN)cp -f ./../plmenu WMRootMenu
