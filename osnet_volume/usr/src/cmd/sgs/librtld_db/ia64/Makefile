#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
include ../Makefile.com

LINTFLAGS64 +=	-D__ia64
CPPFLAGS +=	-D_SYSCALL32
OBJECTS +=	$(COMOBJS64)

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)
install:	all $(ROOTDYNLIB64) $(ROOTLINKS64)
lint:		$(LINTOUT64)

include	../Makefile.targ
include	../../Makefile.sub.64
