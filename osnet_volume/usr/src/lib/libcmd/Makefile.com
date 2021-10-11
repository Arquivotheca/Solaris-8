#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.7	99/08/18 SMI"
#
# lib/libcmd/Makefile.com
#

LIBRARY=	libcmd.a
VERS=		.1

OBJECTS=  \
	deflt.o \
	getterm.o \
	magic.o \
	sum.o

# include library definitions
include ../../Makefile.lib

MAPFILE=		$(MAPDIR)/mapfile	
MAPOPTS=		$(MAPFILE:%=-M %)

SRCS=           $(OBJECTS:%.o=../common/%.c)

LIBS +=			$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=      -u -I../inc
LINTFLAGS64=    -u -I../inc

LINTSRC=		$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=		$(LINTSRC:%=$(ROOTLINTDIR)/%)
$(LINTLIB) := SRCS=	../common/$(LINTSRC)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)
CLOBBERFILES +=	$(MAPFILE)

CFLAGS +=		-v -I../inc
CFLAGS64 +=		-v -I../inc
CPPFLAGS +=		-D_REENTRANT
CPPFLAGS64 +=	-D_REENTRANT
DYNFLAGS +=		$(MAPOPTS)
LDLIBS +=		-lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):		$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

#
# create message catalogue files
#
TEXT_DOMAIN=	SUNW_OST_OSLIB

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)
