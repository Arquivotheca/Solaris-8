#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/01/25 SMI"
#
# lib/librpcsvc/sparcv9/Makefile

MAPDIR=	../spec/sparcv9

include ../Makefile.com
include ../../Makefile.lib.64

LIBS=   $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)

# This needs to be restated, otherwise we'll be dependent
# on the 32bit mapfile...
$(DYNLIB):	$(MAPFILE)
