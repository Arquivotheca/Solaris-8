#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/libgss/ia64/Makefile

MAPDIR=	../spec/ia64
include ../Makefile.com
include ../../Makefile.lib.64

GSSMECH_DIR_V9 = $(ROOT)/usr/lib/ia64/gss

LIBS=   $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(GSSMECH_DIR_V9) $(ROOTLIBS64) $(ROOTLINKS64)

$(GSSMECH_DIR_V9):
	$(INS.dir)

$(DYNLIB):	$(MAPFILE)
