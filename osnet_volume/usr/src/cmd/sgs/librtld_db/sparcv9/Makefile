#
#ident	"@(#)Makefile	1.7	99/03/04 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.com

MAPDIR=		../spec/$(MACH64)
LINTFLAGS64 +=	-D__sparcv9
CPPFLAGS +=	-D_SYSCALL32
OBJECTS +=	$(COMOBJS64)

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS) $(ROOTLIBDIR64)/$(LINTLIB)
install:	all $(ROOTDYNLIB64) $(ROOTLINKS64)
lint:		$(LINTOUT64)

include	../Makefile.targ
include	../../Makefile.sub.64
