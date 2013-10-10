ACLOCAL_AMFLAGS = -I m4
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess config.h.in \
		       config.sub configure depcomp install-sh ltmain.sh \
		       missing module.desktop config.rpath mkinstalldirs

SUBDIRS = src

#if HAVE_PO

#SUBDIRS += po

#endif

EDJE_FLAGS = -v \
	     -id $(top_srcdir)/images

filesdir = $(module_dir)/$(PACKAGE)
files_DATA = module.desktop # e-module-desksanity.edj images/icon.png

EXTRA_DIST = module.desktop.in \
	     e_modules-desksanity.spec.in \
	     e-module-desksanity.edc \
	     images/trophy.png

#e-module-desksanity.edj: Makefile e-module-desksanity.edc images/icon.png

#e-module-desksanity.edj:  e-module-desksanity.edc
#	$(EDJE_CC) $(EDJE_FLAGS) $< $@

clean-local:
	 rm -rf *.edj module.desktop e_modules-desksanity.spec *~

uninstall-local:
	 rm -rf $(DESTDIR)$(module_dir)/$(PACKAGE)

