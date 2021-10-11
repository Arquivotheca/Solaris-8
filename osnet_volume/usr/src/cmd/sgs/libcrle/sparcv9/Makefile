#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/13 SMI"

include		../Makefile.com

ROOTDYNLIB64=	$(DYNLIB:%=$(ROOTLIBDIR64)/%)

LINTFLAGS64 +=	-D__sparcv9
CPPFLAGS +=	-D_ELF64

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)

install:	all $(ROOTDYNLIB64)

lint:		$(LINTOUT64) $(SGSLINTOUT)

include		../Makefile.targ
include		../../Makefile.sub.64
