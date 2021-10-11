#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.9	99/01/25 SMI"
#

LIBRARY=	libpthread.a
VERS=		.1

include		../../Makefile.lib

COMOBJS=	pthread.o	sys.o		thr.o
OBJECTS=	$(COMOBJS)
SRCS=		$(COMOBJS:%.o=../common/%.c)

MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%=-M %)

# -F option normally consumed by the cc driver, so use the -W option of
# the cc driver to make sure this makes it to ld.

CFLAGS +=	-K pic
DYNFLAGS +=	-W l,-Flibthread.so.1 -zinitfirst -zloadfltr $(MAPOPTS)

ROOTLINTLIB=    $(LINTLIB:%=$(ROOTLIBDIR)/%)

CLEANFILES +=	$(LINTOUT)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(MAPFILE)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

pics/%.o:	../common/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

.KEEP_STATE:

all:		$(DYNLIB) $(LINTLIB)

install:	all $(ROOTDYNLIB) $(ROOTLINKS) $(ROOTLINTLIB)

lint:		$(LINTLIB)

#include library targets
include		../../Makefile.targ
