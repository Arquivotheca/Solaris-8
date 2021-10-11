#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/libadm/ia64/Makefile
#

MAPDIR=	../spec/ia64

include ../Makefile.com
include ../../Makefile.lib.64

# workaround for public/bugs/0024
pics/pkgparam.o	:= COPTFLAG64 = -O0

LIBS = $(DYNLIB) $(LINTLIB)

.KEEP_STATE:

all:	$(LIBS) $(TXTS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
