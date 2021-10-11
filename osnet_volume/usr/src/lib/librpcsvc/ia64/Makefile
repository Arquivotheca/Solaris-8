#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/librpcsvc/ia64/Makefile

MAPDIR=	../spec/ia64

include ../Makefile.com
include ../../Makefile.lib.64

LIBS=   $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)

# This needs to be restated, otherwise we'll be dependent
# on the 32bit mapfile...
$(DYNLIB):	$(MAPFILE)
