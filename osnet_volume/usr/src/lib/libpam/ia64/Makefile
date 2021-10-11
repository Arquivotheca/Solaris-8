#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/libpam/ia64/Makefile

MAPDIR=	../spec/ia64

include ../Makefile.com
include ../../Makefile.lib.64

OBJDIR=objs
PICDIR=pics

.KEEP_STATE:

LIBS = 	$(DYNLIB) $(LINTLIB)

all: $(OBJDIR) $(PICDIR) $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
