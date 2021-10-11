#
#ident	"@(#)Makefile	1.3	99/04/20 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		xmemfs
LIBPROG=	mount
ATTMK=		$(LIBPROG)
OBJS=		mount.o fslib.o

include		../Makefile.fstype

# for messaging catalog file
#
POFILE= mount.po

catalog:        $(POFILE)
	cp $(POFILE)	xmemfs.po

CPPFLAGS += -I.. -I../../../uts/intel -I../../../uts/i86pc

$(LIBPROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_PROG

clean:
		$(RM) $(OBJS) $(LIBPROG)

fslib.o: ../fslib.c
	$(COMPILE.c) ../fslib.c
