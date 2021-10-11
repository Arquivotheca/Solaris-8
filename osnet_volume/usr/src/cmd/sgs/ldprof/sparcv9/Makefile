#
#ident	"@(#)Makefile	1.4	98/08/28 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

include ../Makefile.com

ROOTLIBDIR64=	$(ROOT)/usr/lib/link_audit/$(MACH64)
ROOTDYNLIB64=	$(DYNLIB:%=$(ROOTLIBDIR64)/%)

CFLAGS64 +=	-K pic -D_ELF64
LINTFLAGS64 +=	-D__sparcv9

.KEEP_STATE:

all:		$(BLTDEFS) .WAIT $(DYNLIB)
install:	all $(ROOTDYNLIB64)
	@echo doing make install $(ROOTDYNLIB64)
lint:		$(LINTOUT64)

include ../Makefile.targ
include ../../Makefile.sub.64
