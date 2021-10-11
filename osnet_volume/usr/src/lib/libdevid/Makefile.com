#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.3	99/01/25 SMI"
#
# lib/libdevid/Makefile
#
LIBRARY=	libdevid.a
VERS=		.1

OBJECTS=	deviceid.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%=-M %)
CLOBBERFILES +=	$(MAPFILE)

SRCS=		$(OBJECTS:%.o=../%.c)

LIBS =	$(DYNLIB) $(LINTLIB)

LDLIBS += -lc

CPPFLAGS +=	-v
DYNFLAGS +=	$(MAPOPTS)

# definitions for lint

LINTFLAGS=	-u -I..
LINTFLAGS64=	-u -I..
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
