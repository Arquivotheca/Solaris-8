#
#ident	"@(#)Makefile	1.10	97/05/19 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	df
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

CPPFLAGS +=	-D_LARGEFILE64_SOURCE

# for messaging catalogue file
#
POFILE= df.po
XGETFLAGS += -a -x df.xcl

catalog:        $(POFILE)

SRCS= $(LIBPROG).c

lint:		lint_SRCS
