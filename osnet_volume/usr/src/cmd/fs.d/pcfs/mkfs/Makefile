#
#ident "@(#)Makefile 1.5 98/11/20"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		pcfs
LIBPROG=	mkfs
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

OBJS=		$(LIBPROG).o
SRCS=		$(LIBPROG).c

# for messaging catalog
#
POFILE= mkfs.po

catalog:        $(POFILE)

CPPFLAGS +=	-D_LARGEFILE64_SOURCE

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_SRCS

clean:
		${RM} ${OBJS} ${LIBPROG} core

clobber:	clean
