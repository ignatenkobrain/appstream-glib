ACLOCAL_AMFLAGS = -I m4

SUBDIRS = 						\
	libappstream-glib				\
	client						\
	data						\
	docs

DISTCLEANFILES =					\
	appstream-glib-*.tar.xz

MAINTAINERCLEANFILES =					\
	*~						\
	aclocal.m4					\
	ChangeLog					\
	compile						\
	config.guess					\
	config.h.*					\
	config.sub					\
	configure					\
	depcomp						\
	gtk-doc.make					\
	INSTALL						\
	install-sh					\
	ltmain.sh					\
	Makefile.in					\
	missing						\
	mkinstalldirs					\
	test-driver

EXTRA_DIST =						\
	COPYING						\
	MAINTAINERS					\
	AUTHORS						\
	README.md					\
	NEWS						\
	autogen.sh					\
	config.h

distclean-local:
	if test $(srdcir) = .; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log AS_0_0_X.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
