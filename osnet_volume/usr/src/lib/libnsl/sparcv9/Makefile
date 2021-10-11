#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.13	99/01/25 SMI"

#
# lib/libnsl/sparcv9/Makefile

MAPDIR=	../spec/sparcv9

include ../Makefile.com
include ../../Makefile.lib.64

# We do a blanket override of these here because it's simpler to compile
# the whole 64-bit library with PIC instead of "generically" compiling
# large parts of the 32-bit library with PIC unnecessarily.

sparcv9_C_PICFLAGS = -K PIC
sparcv9_CC_PICFLAGS = -KPIC

#MAPOPTS=	$(MAPFILES64:%=-M %)

LIBS= $(DYNLIB64) $(LINTLIB)

.KEEP_STATE:

all:	$(DYNLIB) $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
