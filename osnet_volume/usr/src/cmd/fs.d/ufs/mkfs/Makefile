#
#ident	"@(#)Makefile	1.13	97/06/09 SMI"
#
# Copyright (c) 1989,1996,1997 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	mkfs
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype
include		../Makefile.roll

OBJS=		$(LIBPROG).o $(ROLLOBJS)
SRCS=		$(LIBPROG).c $(ROLLSRCS)


# for messaging catalog
#
POFILE= mkfs.po

catalog:        $(POFILE)


CPPFLAGS +=	-D_LARGEFILE64_SOURCE
LDLIBS +=	-lm -ladm

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_SRCS
