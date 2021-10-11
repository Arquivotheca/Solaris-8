#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	99/01/25 SMI"
#
# lib/libadm/sparcv9/Makefile
#

MAPDIR=	../spec/sparcv9

include ../Makefile.com
include ../../Makefile.lib.64

LIBS = $(DYNLIB) $(LINTLIB)

.KEEP_STATE:

all:	$(LIBS) $(TXTS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
