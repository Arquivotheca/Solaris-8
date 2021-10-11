#
#ident	"@(#)Makefile	1.4	95/08/03 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/fs.d/cachefs/umount
#

FSTYPE=		cachefs
LIBPROG=	umount
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	umount.o fslib.o

include		../Makefile.cachefs

CPPFLAGS += -I../.. -o $@

fslib.o: ../../fslib.c 
	$(COMPILE.c) ../../fslib.c
$(LIBPROG) :	$(CFSLIB)

$(PROGOBJS) :	$(CACHEFSDIR)/subr.h $(CACHEFSDIR)/cachefsd.h
