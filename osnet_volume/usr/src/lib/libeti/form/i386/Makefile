#
#ident	"@(#)Makefile	1.2	99/01/25 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libeti/form/sparc/Makefile
#

MAPDIR=	../spec/i386
include ../Makefile.com

.KEEP_STATE:

# The static library libform.a only gets installed for 32-bit, and is 
# installed in /usr/ccs/lib rather than /usr/lib, hence the separate 
# rule. In addition, we need to link /usr/ccs/lib/libform.so with
# /usr/lib/libform.so.1 to accomodate the order search of the linker.
#
install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)
