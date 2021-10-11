#
#ident	"@(#)Makefile.com	1.20	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

LIBRARY=	ldprof.a
VERS=		.1

include		../../../../lib/Makefile.lib
include		../../Makefile.com

ROOTLIBDIR=	$(ROOT)/usr/lib/link_audit

SGSPROTO=	../../proto/$(MACH)

COMOBJS=	profile.o
BLTOBJ=		msg.o

OBJECTS=	$(COMOBJS) $(BLTOBJ)

DYNFLAGS +=	-Yl,$(SGSPROTO)
CPPFLAGS=	-I. -I../common -I../../include \
		-I../../rtld/common \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/common/krtld \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)

CFLAGS +=	-K pic
LINTFLAGS =	-mu
LINTFLAGS64 =	-mu -errchk=longptr64

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/ldprof

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/ldprof.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n ldprof_msg

#
# This cruft is here because the lint library for libmapmalloc doesn't agree
# with the lint library for libc on all prototypes (a bug in the lint lib for
# libc, not in either implementation, see bug 4167233 and others).
#
MAPMALLOC=	-lmapmalloc
lint :=		MAPMALLOC=

SRCS=		$(COMOBJS:%.o=../common/%.c) $(BLTDATA)
LDLIBS +=	-L../../proto/$(MACH) $(MAPMALLOC) -lc
LINTSRCS=	$(SRCS) ../common/lintsup.c

CLEANFILES +=	$(LINTOUTS) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
