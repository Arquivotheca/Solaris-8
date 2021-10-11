#
#ident "@(#)Makefile	1.1	93/02/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		s5fs
LIBPROG=	fsck
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

FSCKOBJS=	fsck.o fsckinit.o l3.o
OBJS=		$(FSCKOBJS)
SRCS=		$(OBJS:%.o=%.c)

LDLIBS += -lc2 -ladm

fsck:		$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

%.o:	%.c
	$(COMPILE.c) $<

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)
