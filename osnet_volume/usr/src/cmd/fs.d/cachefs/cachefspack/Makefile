#
#ident	"@(#)Makefile	1.6	96/04/20 SMI"
#
# Copyright (c) 1995,1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/fs.d/cachefs/cachefspack
#

FSTYPE=		cachefs
LIBPROG=	cachefspack
ATTMK=		$(LIBPROG)

OTHERINSTALL=   $(ROOTBIN)/$(LIBPROG)
LINKVALUE=      ../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	docmds.o elfrd.o funcs.o subr.o rules.o main.o

include		../Makefile.cachefs

CPPFLAGS += -D_LARGEFILE64_SOURCE -I../.. -o $@
LDLIBS += -lelf -lgen

$(LIBPROG) :	$(CFSLIB)

$(PROGOBJS) :	$(CACHEFSDIR)/subr.h $(CACHEFSDIR)/cachefsd.h

$(ROOTBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUE) $@
