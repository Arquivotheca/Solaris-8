#
# ident	"@(#)Makefile	1.1	99/07/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5/sparcv9/Makefile
#

MAPDIR= ../spec/sparcv9
include ../Makefile.com
include $(SRC)/lib/Makefile.lib.64

DYNFLAGS += -Wl,-f/usr/platform/\$$PLATFORM/lib/sparcv9/$(DYNLIBPSR)


.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT)
