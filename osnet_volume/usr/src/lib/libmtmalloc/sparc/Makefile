#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	99/01/25 SMI"
#
# lib/libmtmalloc/sparc/Makefile

MAPDIR=	../spec/sparc
include ../Makefile.com

CFLAGS		= -v -K pic -I../common -xspace -xcg92 -xregs=no%appl $(COPTFLAG)
LIBS		= $(DYNLIB)
ROOTLINTDIR	= $(ROOTLIBDIR)
ROOTLINT	= $(ROOTLINTDIR)/$(LINTLIB)


.KEEP_STATE:

all:	$(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
