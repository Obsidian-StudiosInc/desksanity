ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess config.h.in \
		       config.sub configure compile depcomp install-sh ltmain.sh \
		       missing config.rpath mkinstalldirs

#if HAVE_PO

#SUBDIRS += po

#endif

filesdir = $(module_dir)/$(PACKAGE)
files_DATA = module.desktop e-module-desksanity.edj # images/icon.png

EXTRA_DIST = module.desktop.in \
	     e_modules-desksanity.spec.in \
	     e-module-desksanity.edc \
	     epic-e-256.png \
		win_shadow.png \
glow_bottom.png \
glow_top.png


include src/Makefile.mk

clean-local:
	rm rf e_modules-desksanity.spec *~

uninstall-local:
	 rm -rf $(DESTDIR)$(module_dir)/$(PACKAGE)

