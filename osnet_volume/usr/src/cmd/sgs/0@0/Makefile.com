#
#ident	"@(#)Makefile.com	1.2	96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	0@0.a
VERS=		.1

OBJECTS=	0@0.o

include 	$(SRC)/lib/Makefile.lib

DYNFLAGS +=	-Wl,-Blocal -Wl,-znoversion
ZDEFS=

SRCS=		$(OBJECTS:%.o=../common/%.c)

CLEANFILES +=	$(LINTOUT)
CLOBBERFILES +=	$(DYNLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
