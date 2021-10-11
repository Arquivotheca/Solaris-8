#
#ident	"@(#)Makefile	1.3	99/08/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		mntfs
ATTMK=		$(FSTYPE)
LIBPROG=	mount
OBJS=		mount.o

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
	$(RM) mntfs.po
	cp $(POFILE) mntfs.po
