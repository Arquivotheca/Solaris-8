#
#ident	"@(#)Makefile	1.18	96/04/18 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/cfsadmin
#

FSTYPE=		cachefs
LIBPROG=	cfsadmin
ATTMK=		$(LIBPROG)

OTHERINSTALL=   $(ROOTUSRSBIN)/$(LIBPROG)
LINKVALUE=      ../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	cfsadmin.o

include		../Makefile.cachefs

CPPFLAGS += -D_LARGEFILE64_SOURCE

$(LIBPROG) :	$(CFSLIB)

$(PROGOBJS) :	$(CACHEFSDIR)/subr.h $(CACHEFSDIR)/cachefsd.h

$(ROOTUSRSBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUE) $@

