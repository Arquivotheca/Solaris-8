#
#ident	"@(#)Makefile	1.12	96/03/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		ufs
LIBPROG=	volcopy
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

# for messaging catalog
#
POFILE= volcopy.po
XGETFLAGS += -a  -x volcopy.xcl

catalog:        $(POFILE)


LDLIBS += -lgenIO -lmapmalloc

lint:		lint_PROG
