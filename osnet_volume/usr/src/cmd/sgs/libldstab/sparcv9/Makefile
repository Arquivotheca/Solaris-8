#
#ident	"@(#)Makefile	1.2	98/08/28 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

DUPOBJS=	stab64.o

include ../Makefile.com

.KEEP_STATE:

ROOTLIBDIR=	$(ROOT)/usr/lib/$(MACH64)
ROOTDYNLIB64=	$(DYNLIB:%=$(ROOTLIBDIR)/%)

DYNFLAGS +=	-M ../$(MACH)/mapfile-sparc

LINTFLAGS64 += -D__sparcv9

all:		$(DYNLIB)
install \
package:	all $(ROOTDYNLIB64)
lint:		$(LINTOUT64)

include	../Makefile.targ
include	../../Makefile.sub.64

