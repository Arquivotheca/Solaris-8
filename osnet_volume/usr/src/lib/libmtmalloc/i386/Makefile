#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	99/01/25 SMI"
#
# lib/libmtmalloc/i386/Makefile

MAPDIR=	../spec/i386
include ../Makefile.com

COPTFLAG	= -xO3
CFLAGS 		= -v -xpentium -xspace -I../common -K pic $(COPTFLAG)
LIBS		= $(DYNLIB)
ROOTLINTDIR	= $(ROOTLIBDIR)
ROOTLINT	= $(ROOTLINTDIR)/$(LINTLIB)

.KEEP_STATE:

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
