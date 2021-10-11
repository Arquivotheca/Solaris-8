#
#ident	"@(#)Makefile	1.13	96/03/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		proc
ATTMK=		$(FSTYPE)
LIBPROG=	mount
OBJS=		mount.o fslib.o

include		../Makefile.fstype
#
# Message catalog
#
POFILE= mount.po

CPPFLAGS += -I.. 

$(LIBPROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_PROG
#
# message catalog
#
catalog: $(POFILE)
	$(RM) proc.po
	cp $(POFILE) proc.po

fslib.o: ../fslib.c
	$(COMPILE.c) ../fslib.c
