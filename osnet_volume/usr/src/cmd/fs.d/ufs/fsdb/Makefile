#
#ident	"@(#)Makefile	1.10	97/05/12 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		ufs
LIBPROG=	fsdb

include		../../Makefile.fstype

CPPFLAGS +=     -D_LARGEFILE64_SOURCE
LDLIBS += -lc2

SRCS=	$(LIBPROG).c

lint:	lint_SRCS
