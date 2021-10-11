#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	99/01/25 SMI"

LIBRARY=	libpthread.a
VERS=		.1

include		../../Makefile.lib
include		../../Makefile.lib.64

COMOBJS=	pthread.o	sys.o		thr.o
OBJECTS=	$(COMOBJS)
SRCS=		$(COMOBJS:%.o=../common/%.c)

MAPDIR=		../spec/sparcv9
MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%=-M %)

# -F option normally consumed by the cc driver, so use the -W option of
# the cc driver to make sure this makes it to ld.

CFLAGS64 += -K pic -D__sparcv9
DYNFLAGS +=	-W l,-Flibthread.so.1 -zinitfirst -zloadfltr $(MAPOPTS)
CPPFLAGS += -Dsparc -D__sparcv9

ROOTLINTLIB64=    $(LINTLIB:%=$(ROOTLIBDIR64)/%)

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

install:	all $(ROOTDYNLIB64) $(ROOTLINKS64) $(ROOTLINTLIB64)

lint:		$(LINTLIB)

#include library targets
include		../../Makefile.targ

