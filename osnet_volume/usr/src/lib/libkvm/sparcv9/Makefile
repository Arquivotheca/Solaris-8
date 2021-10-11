#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.5	99/01/25 SMI"
#

MAPDIR=	../spec/sparcv9
include ../Makefile.com
include ../../Makefile.lib.64

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64)
