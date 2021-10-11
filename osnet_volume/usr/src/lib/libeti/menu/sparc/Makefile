#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	99/01/25 SMI"
#
# lib/libeti/menu/sparc/Makefile
#

MAPDIR=	../spec/sparc
include ../Makefile.com

.KEEP_STATE:

# The static library libmenu.a only gets installed for 32-bit, and is
# installed in /usr/ccs/lib rather than /usr/lib, hence the separate
# rule. In addition, we need to link /usr/ccs/lib/libmenu.so with
# /usr/lib/libmenu.so.1 to accomodate the order search of the linker.
#
install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)

