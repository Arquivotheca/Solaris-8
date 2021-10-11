#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.4	99/03/31 SMI"
#
# lib/libdevice/Makefile
#
LIBRARY=	libdevice.a
VERS=		.1
OBJECTS=	devctl.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%= -M %)
CLOBBERFILES +=	$(MAPFILE)

SRCS=		$(OBJECTS:%.o=../%.c)

$(LINTLIB):= SRCS=../llib-ldevice
$(LINTLIB):= LINTFLAGS = -u -I..
$(LINTLIB):= LINTFLAGS64 = -u -D__sparcv9 -I..

LIBS =	$(DYNLIB) $(LINTLIB)

LDLIBS += -lc

CPPFLAGS +=	-v -D_REENTRANT
DYNFLAGS +=	$(MAPOPTS)

# definitions for lint

LINTFLAGS=	-u -I..
LINTFLAGS64=	-u -I.. -D__sparcv9
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES=	$(LINTOUT) $(LINTLIB)

lint: $(LINTLIB)

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

.KEEP_STATE:

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o:	../%.c
	$(COMPILE.c) -o $@ $<

# install rule for lint library target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)
