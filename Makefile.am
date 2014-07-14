bin_PROGRAMS=psplash psplash-write

AM_CFLAGS = $(GCC_FLAGS) -D_GNU_SOURCE

psplash_SOURCES = psplash.c psplash.h psplash-fb.c psplash-fb.h \
                  psplash-console.c psplash-console.h           \
		  psplash-colors.h				\
		  psplash-poky-img.h psplash-bar-img.h radeon-font.h customizations.c customizations.h settings-img.h configos-img.h calib-img.h

psplash_write_SOURCES = psplash-write.c psplash.h

EXTRA_DIST = make-image-header.sh
 
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

