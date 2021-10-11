#
#ident	"@(#)Makefile	1.15	96/04/18 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/mount
#

FSTYPE=		cachefs
LIBPROG=	mount
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	mount.o fslib.o

include		../Makefile.cachefs

CPPFLAGS += -D_LARGEFILE64_SOURCE -I../.. -o $@

fslib.o: ../../fslib.c
	$(COMPILE.c) ../../fslib.c
$(LIBPROG) :	$(CFSLIB)

$(PROGOBJS) :	$(CACHEFSDIR)/subr.h $(CACHEFSDIR)/cachefsd.h
