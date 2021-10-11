#
#ident	"@(#)Makefile	1.14	96/03/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		fd
ATTMK=		$(FSTYPE)
NULLPROG=	fsck
LIBPROG=	mount $(NULLPROG)
OBJS=		mount.o fslib.o

include		../Makefile.fstype

CPPFLAGS += -I.. 
# for messaging catalog
#
POFILE= fd.po
POFILES= mount.po

ROOTLIBNULLPROG= $(NULLPROG:%=$(ROOTLIBFSTYPE)/%)

mount:		$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_PROG

catalog:        $(POFILE)

$(POFILE):      $(POFILES)
	$(RM) $@
	cat $(POFILES)  > $@

$(NULLPROG):
	$(CP) /dev/null $(NULLPROG)

fslib.o: ../fslib.c
	$(COMPILE.c) ../fslib.c
