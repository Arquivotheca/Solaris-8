#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/libmtmalloc/ia64/Makefile

MAPDIR=	../spec/ia64
include ../Makefile.com
include ../../Makefile.lib.64

LIBS    	= $(DYNLIB)
ROOTLINTDIR	= $(ROOTLIBDIR64)
ROOTLINT64	= $(ROOTLINTDIR)/$(LINTLIB)
ROOTLINT	= $(ROOTLINT64)

.KEEP_STATE:

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT)
