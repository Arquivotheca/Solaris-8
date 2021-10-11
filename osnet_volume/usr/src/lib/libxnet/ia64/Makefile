#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/libxnet/ia64/Makefile

MAPDIR=	../spec/ia64
include ../Makefile.com
include ../../Makefile.lib.64

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS64) $(PICS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

LIBS=	$(DYNLIB) $(LINTLIB)

.KEEP_STATE:

install: all $(ROOTLIBS64) $(ROOTLINKS64)
