#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/01/25 SMI"
#
# lib/librac/i386/Makefile

MAPDIR=	../spec/i386
include ../Makefile.com

LIBS += $(DYNLIB) $(LINTLIB)
CFLAGS += -v

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)

