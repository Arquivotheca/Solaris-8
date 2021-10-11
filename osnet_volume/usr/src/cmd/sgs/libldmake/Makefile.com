#
#ident	"@(#)Makefile.com	1.10	98/12/04 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

LIBRARY=	libldmake.a
VERS=		.1

include		../../../../lib/Makefile.lib

ROOTLIBDIR=	$(ROOT)/opt/SUNWonld/lib
ROOTLIBDIR64=	$(ROOT)/opt/SUNWonld/lib/$(MACH64)

SGSPROTO=	../../proto/$(MACH)

COMOBJS=	ld_file.o lock.o

OBJECTS=	$(COMOBJS)

MAPFILE=	../common/mapfile-vers

DYNFLAGS +=	-Yl,$(SGSPROTO) -M $(MAPFILE)
CPPFLAGS=	-I../common -I../../include \
		-I../../include/$(MACH) $(CPPFLAGS.master) \
		-D_TS_ERRNO

CFLAGS +=	-K pic

CFLAGS64 +=	-K pic
LINTFLAGS =	-mu
LINTFLAGS64 =	-mu -errchk=longptr64

SRCS=		$(OBJECTS:%.o=../common/%.c)
LDLIBS +=	-lelf -lc

CLEANFILES +=
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(LINTOUT)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
ROOTDYNLIB64=	$(DYNLIB:%=$(ROOTLIBDIR64)/%)
