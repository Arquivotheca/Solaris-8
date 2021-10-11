#
#ident	"@(#)Makefile.com	1.7	98/09/30 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/dis/Makefile.com
#

PROG=		dis

include 	$(SRC)/cmd/Makefile.cmd
include 	$(SRC)/cmd/sgs/Makefile.com

COMOBJS=	debug.o extn.o lists.o main.o utls.o

OBJS=		$(COMOBJS) $(MACHOBJS)
LINTSRCS=	$(COMOBJS:%.o=../common/%.c) $(MACHOBJS:.o=.c)

INCLIST=	-I. -I../common -I../../include -I../../include/$(MACH)
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	-L ../../sgsdemangler/$(MACH) -ldemangle -lelf -ldl
LINTFLAGS =	-mx
CLEANFILES +=	$(LINTOUTS)

.KEEP_STATE:
