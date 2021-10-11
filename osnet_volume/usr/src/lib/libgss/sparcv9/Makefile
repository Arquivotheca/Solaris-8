#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	99/01/25 SMI"
#
# lib/libgss/sparcv9/Makefile

MAPDIR=	../spec/sparcv9
include ../Makefile.com
include ../../Makefile.lib.64

GSSMECH_DIR_V9 = $(ROOT)/usr/lib/sparcv9/gss

LIBS=   $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(GSSMECH_DIR_V9) $(ROOTLIBS64) $(ROOTLINKS64)

$(GSSMECH_DIR_V9):
	$(INS.dir)

$(DYNLIB):	$(MAPFILE)
