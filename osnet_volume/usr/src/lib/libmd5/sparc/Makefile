#
# ident	"@(#)Makefile	1.1	99/07/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5/sparc/Makefile
#

MAPDIR= ../spec/sparc
include ../Makefile.com

DYNFLAGS += -Wl,-f/usr/platform/\$$PLATFORM/lib/$(DYNLIBPSR)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
