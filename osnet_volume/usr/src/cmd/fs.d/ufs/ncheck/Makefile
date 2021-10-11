#
#ident	"@(#)Makefile	1.12	97/06/09 SMI"
#
# Copyright (c) 1989,1996,1997 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	ncheck
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype
include		../Makefile.roll

OBJS=		$(LIBPROG).o $(ROLLOBJS)
SRCS=		$(LIBPROG).c $(ROLLSRCS)

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

CPPFLAGS +=	-D_LARGEFILE64_SOURCE

lint:		lint_SRCS
