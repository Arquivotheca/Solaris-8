#
#pragma ident	"@(#)Makefile	1.5	99/05/04 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.

include		$(SRC)/cmd/sgs/librtld/Makefile.com
CPPFLAGS += -I../../../../uts/sparc/krtld


.KEEP_STATE:

ROOTDYNLIB64 =	$(DYNLIB:%=$(ROOTLIBDIR64)/%)

LINTFLAGS64 +=	-D__sparcv9

all:		$(DYNLIB) $(LIBLINKS)

install:	all $(ROOTDYNLIB64)

lint:		$(LINTLIB) $(LINTOUT64) $(SGSLINTOUT)

CPPFLAGS +=	-D_ELF64


include		$(SRC)/cmd/sgs/librtld/Makefile.targ
include		../../Makefile.sub.64

DBGLIB =	-L ../../liblddbg/$(MACH64)
ELFLIB =	-L ../../libelf/$(MACH64)


