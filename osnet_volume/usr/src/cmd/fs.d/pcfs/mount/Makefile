#
#ident	"@(#)Makefile	1.10	99/04/27 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#

FSTYPE=		pcfs
LIBPROG=	mount
ATTMK=		$(LIBPROG)
OBJS=		$(LIBPROG).o
SRCS=		$(LIBPROG).c

include		../../Makefile.fstype

CPPFLAGS += -v -I../.. -o $@

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_SRCS
