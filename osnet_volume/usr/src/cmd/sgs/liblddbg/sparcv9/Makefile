#
#	Copyright (c) 1997 by Sun Microsystems, Inc.
#	All rights reserved.
#
#ident	"@(#)Makefile	1.6	98/02/04 SMI"
#


include		$(SRC)/cmd/sgs/liblddbg/Makefile.com

ROOTLIBDIR=	$(ROOT)/usr/lib/$(MACH64)
ROOTDYNLIB64=	$(DYNLIB:%=$(ROOTLIBDIR)/%)

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)

install:	all $(ROOTDYNLIB64)

include		$(SRC)/cmd/sgs/liblddbg/Makefile.targ
include		../../Makefile.sub.64

CONVLIB	=	-L../../libconv/sparcv9

LINTFLAGS64 +=	-D__sparcv9
