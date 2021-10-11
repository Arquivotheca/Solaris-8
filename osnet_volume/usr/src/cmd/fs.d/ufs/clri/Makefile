#
#ident	"@(#)Makefile	1.13	98/09/16 SMI"
#
# Copyright (c) 1989,1996,1997 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		ufs
LIBPROG=	clri
DCOPY=		dcopy
ATTMK=		$(LIBPROG)

OTHERINSTALL=	$(ROOTLIBFSTYPE)/$(DCOPY)
		
include		../../Makefile.fstype
include		../Makefile.roll

OBJS=		$(LIBPROG).o $(ROLLOBJS)
SRCS=		$(LIBPROG).c $(ROLLSRCS)

CFLAGS +=	-v
CPPFLAGS +=	-D_LARGEFILE64_SOURCE

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint :=		PROG=$(LIBPROG)
lint:		lint_PROG

$(ROOTLIBFSTYPE)/$(DCOPY):	$(ROOTLIBFSTYPE)/$(LIBPROG)
		-$(RM) $@; $(LN) $(ROOTLIBFSTYPE)/$(LIBPROG) $@
