#
#ident	"@(#)Makefile	1.6	96/04/18 SMI"
#
# Copyright (c) 1991,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/lockfs/Makefile

FSTYPE=		ufs
LIBPROG=	lockfs
ATTMK=		$(LIBPROG)

OTHERINSTALL=	$(ROOTUSRSBIN)/$(LIBPROG)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

LDLIBS += -lc2
CPPFLAGS +=	-D_LARGEFILE64_SOURCE

lint:		lint_PROG

$(ROOTUSRSBIN)/$(LIBPROG):
	$(RM) $@; $(SYMLINK) $(LINKVALUE) $@
