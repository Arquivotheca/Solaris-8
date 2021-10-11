#
#pragma ident	"@(#)Makefile	1.4	99/01/25 SMI"
#
# Copyright (c) 1995,1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# libpam/sparc/Makefile

MAPDIR=	../spec/sparc

include ../Makefile.com

OBJDIR=objs
PICDIR=pics

.KEEP_STATE:

DYNFLAGS += $(DYNFLAGS32)

LIBS += 	$(DYNLIB) $(LINTLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
