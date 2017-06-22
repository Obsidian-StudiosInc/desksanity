AM_CPPFLAGS = \
-Isrc \
-I$(top_srcdir) \
-I$(includedir) \
-DLOCALEDIR=\"$(datadir)/locale\" \
-DPACKAGE_DATA_DIR=\"$(module_dir)/$(PACKAGE)\" \
@E_CFLAGS@

pkgdir = $(module_dir)/$(PACKAGE)/$(MODULE_ARCH)
pkg_LTLIBRARIES = module.la
module_la_SOURCES = \
src/e_mod_main.h \
src/e_mod_main.c \
src/ds_config.c \
src/maximize.c \
src/moveresize.c \
src/pip.c \
src/zoom.c \
src/magnify.c \
src/desksanity.c

if BUILD_RUNNER
module_la_SOURCES += src/runner.c
endif

module_la_LIBADD = @E_LIBS@
module_la_LDFLAGS = -module -avoid-version
module_la_DEPENDENCIES = $(top_builddir)/config.h
