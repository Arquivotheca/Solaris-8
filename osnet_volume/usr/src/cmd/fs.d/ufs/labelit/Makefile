#
#ident	"@(#)Makefile	1.8	96/04/18 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/labelit/Makefile

FSTYPE=		ufs
LIBPROG=	labelit
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

LDLIBS += -lm
CPPFLAGS +=	-D_LARGEFILE64_SOURCE

lint :=		PROG=$(LIBPROG)

lint:		lint_PROG
