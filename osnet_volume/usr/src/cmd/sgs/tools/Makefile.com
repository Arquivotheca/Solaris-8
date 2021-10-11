#
#ident  "@(#)Makefile.com 1.10     97/10/14 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile to support tools used for linker development:
#
#  o	sgsmsg creates message headers/arrays/catalogs (a native tool).
#
# Note, these tools are not part of the product.
#
# cmd/sgs/tools/Makefile.com

include		$(SRC)/cmd/Makefile.cmd

include		$(SRC)/cmd/sgs/Makefile.com

SGSPROTO=	../../proto/$(MACH)

COMOBJS=	comment_filter.o

NATOBJS=	sgsmsg.o

OBJECTS=	$(COMOBJS)  $(NATOBJS)

PROGS=		$(COMOBJS:%.o=%)
NATIVE=		$(NATOBJS:%.o=%)
SRCS=		$(COMOBJS:%.o=../common/%.c)  $(NATOBJS:%.o=../common/%.c)

CPPFLAGS +=	-I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys
LDFLAGS +=	-Yl,$(SGSPROTO)
CLEANFILES +=	$(LINTOUT)
LINTFLAGS=	-ax

ROOTDIR=	$(ROOT)/opt/SUNWonld
ROOTPROGS=	$(PROGS:%=$(ROOTDIR)/bin/%)

FILEMODE=	0755
