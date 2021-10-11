#
#ident	"@(#)Makefile	1.10	96/03/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		tmpfs
LIBPROG=	mount
ATTMK=		$(LIBPROG)
OBJS=		mount.o fslib.o

include		../Makefile.fstype

# for messaging catalog file
#
POFILE= mount.po

catalog:        $(POFILE)
	cp $(POFILE)	tmpfs.po

CPPFLAGS += -I.. 

$(LIBPROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_PROG

fslib.o: ../fslib.c
	$(COMPILE.c) ../fslib.c
