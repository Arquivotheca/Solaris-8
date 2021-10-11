#
#ident	"@(#)Makefile	1.3	94/11/02 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

FSTYPE=		cachefs
LIBPROG=	cachefsstat
ATTMK=		$(LIBPROG)

OTHERINSTALL=   $(ROOTBIN)/$(LIBPROG)
LINKVALUE=      ../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	cachefsstat.o
COMMONOBJS=	$(CACHEFSDIR)/subr.o ../lib/libcachefs.a

include		../Makefile.cachefs

LDLIBS +=	-lkstat
CPPFLAGS +=	-I../common

$(ROOTBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUE) $@
