#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/01/25 SMI"
#
# lib/libmtmalloc/sparcv9/Makefile

MAPDIR=	../spec/sparcv9
include ../Makefile.com
include ../../Makefile.lib.64

LIBS    	= $(DYNLIB)
ROOTLINTDIR	= $(ROOTLIBDIR64)
ROOTLINT64	= $(ROOTLINTDIR)/$(LINTLIB)
ROOTLINT	= $(ROOTLINT64)

.KEEP_STATE:

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT)
