bin_PROGRAMS = globaltime

globaltime_SOURCES =						\
	globaltime.c							\
	globaltime.h							\
	gt_parfile.c							\
	gt_prefs.c								\
	timezone_selection.c                    \
	timezone_selection.h                    \
	../src/tz_zoneinfo_read.c               \
	../src/tz_zoneinfo_read.h               \
	../src/functions.c                      \
	../src/functions.h

man_MANS =                             		 \
	globaltime.1

if MAINTAINER_MODE
globaltime.1: globaltime.xml
	xsltproc -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
endif

desktopdir = $(datadir)/applications
desktop_in_files = globaltime.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@


globaltime_CFLAGS =							\
    $(LIBGTK_CFLAGS)                        \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"		\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"

globaltime_LDADD =							\
    $(LIBGTK_LIBS)                          \
	-lX11		            				\
	$(INTLLIBS) 

EXTRA_DIST =								\
	$(man_MANS)								\
	$(desktop_in_files)						\
	globaltime.xml

DISTCLEANFILES =                            \
    $(desktop_DATA)


# vi:set ts=8 sw=8 noet ai:
