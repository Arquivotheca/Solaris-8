#
#ident	"@(#)Makefile	1.10	96/04/18 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/fsck
#

FSTYPE=		cachefs
LIBPROG=	fsck
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	fsck.o res.o dlog_ck.o

include		../Makefile.cachefs
CPPFLAGS +=	-D_LARGEFILE64_SOURCE

$(LIBPROG) :	$(CFSLIB)

$(PROGOBJS) :	$(CACHEFSDIR)/subr.h $(CACHEFSDIR)/cachefsd.h
