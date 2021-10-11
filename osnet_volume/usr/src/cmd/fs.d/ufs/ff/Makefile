#
#ident  "@(#)Makefile 1.8     97/12/12 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	ff
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype
include		../Makefile.roll

OBJS=           $(LIBPROG).o $(ROLLOBJS)
SRCS=           $(LIBPROG).c $(ROLLSRCS)

CPPFLAGS +=	-D_LARGEFILE64_SOURCE

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_SRCS
