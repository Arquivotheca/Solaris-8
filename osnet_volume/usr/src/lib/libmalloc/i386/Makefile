#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	99/01/25 SMI"
#
# lib/libmalloc/i386/Makefile
#

MAPDIR=	../spec/i386
include ../Makefile.com

.KEEP_STATE:

# The static library libgen.a only gets installed for 32-bit, and is
# installed in /usr/ccs/lib rather than /usr/lib, hence the separate
# rule. In addition, we need to link /usr/ccs/lib/libgen.so with
# /usr/lib/libgen.so.1 to accomodate the order search of the linker.

all: $(LIBS)

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)
