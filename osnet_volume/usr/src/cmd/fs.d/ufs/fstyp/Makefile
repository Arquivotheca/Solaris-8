#
#ident	"@(#)Makefile	1.10	98/05/07 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	fstyp
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

# for messaging catalog
#
POFILE= fstyp.po

catalog:        $(POFILE)

CFLAGS +=	-v
CPPFLAGS +=	-D_LARGEFILE64_SOURCE
LDLIBS += -ladm

lint :=		PROG = $(LIBPROG)
lint:		lint_PROG
