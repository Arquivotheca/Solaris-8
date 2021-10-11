#
#ident	"@(#)Makefile	1.7	99/04/27 SMI"	/* 	*/
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		lofs
LIBPROG=		mount
ATTMK= $(PROG)
OBJS=		mount.o

include		../../Makefile.fstype

CPPFLAGS += -I../.. -o $@

lint := PROG =$(LIBPROG)

$(LIBPROG):	$(OBJS)
		$(LINK.c) $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:	lint_PROG

