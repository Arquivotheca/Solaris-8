#
#ident	"@(#)Makefile.com	1.2	98/02/23 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

LIBRARY=	libldfeature.a

include		../../../../lib/Makefile.lib
include		../../Makefile.com

ROOTLIBDIR=	$(ROOT)/usr/lib

SGSPROTO=	../../proto/$(MACH)

COMOBJS=	check_rtld_feature.o

OBJECTS=	$(COMOBJS)
PICS=		$(OBJECTS:%=pics/%)

CPPFLAGS=	-I../common -I../../include \
		-I../../include/$(MACH) $(CPPFLAGS.master)

ARFLAGS=	r
CFLAGS +=	-K pic

SRCS=		$(OBJECTS:%.o=../common/%.c)

LINTOUT=	lint.out

CLEANFILES +=	$(LINTOUT)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)
