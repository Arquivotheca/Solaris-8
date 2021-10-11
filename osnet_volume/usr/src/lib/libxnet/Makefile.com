#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.4	99/03/26 SMI"
#
# lib/libxnet/Makefile.com

LIBRARY= libxnet.a
VERS= .1

OBJECTS= data.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
MAPFILES=	$(MAPFILE) $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)

CLOBBERFILES += $(MAPFILE)

SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS=		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=	-u -I..
LINTFLAGS64=	-u -I..
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES += 	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I..
CFLAGS64 +=	-v -I..
DYNFLAGS +=     -Flibsocket.so.1 -Flibnsl.so.1 -Flibc.so.1 -zloadfltr $(MAPOPTS)
DYNFLAGS64 +=   -Flibsocket.so.1 -Flibnsl.so.1 -Flibc.so.1 -zloadfltr $(MAPOPTS)
LDLIBS +=	-lc

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/data.o pics/data.o:

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
