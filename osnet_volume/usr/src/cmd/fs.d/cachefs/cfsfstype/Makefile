#
#ident	"@(#)Makefile	1.3	96/04/18 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		cachefs
LIBPROG=	cfsfstype
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	cfsfstype.o

include		../Makefile.cachefs

CPPFLAGS += -D_LARGEFILE64_SOURCE
