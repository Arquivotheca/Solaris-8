#
#ident	"@(#)Makefile	1.8	96/04/18 SMI"
#
# Copyright (c) 1989,1996, by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	tunefs
ATTMK=		$(LIBPROG)

OTHERINSTALL=	$(ROOTUSRSBIN)/$(LIBPROG)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

LDLIBS	+=	-ladm
CPPFLAGS +=	-D_LARGEFILE64_SOURCE

lint:		lint_PROG

$(ROOTUSRSBIN)/$(LIBPROG):	FRC
		-$(RM) $@; $(SYMLINK) $(LINKVALUE) $@

FRC:

