#
#ident	"@(#)Makefile	1.5	94/11/02 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		cachefs
LIBPROG=	cachefslog
ATTMK=		$(LIBPROG)

OTHERINSTALL=   $(ROOTUSRSBIN)/$(LIBPROG)
LINKVALUE=      ../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	cachefslog.o
COMMONOBJS=	$(CACHEFSDIR)/subr.o ../lib/libcachefs.a

include		../Makefile.cachefs

LDLIBS +=	-lkstat
CPPFLAGS +=	-I../common

$(ROOTUSRSBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUE) $@
