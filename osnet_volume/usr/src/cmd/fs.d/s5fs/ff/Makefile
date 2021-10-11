#
#ident "@(#)Makefile	1.2	93/02/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		s5fs
LIBPROG=	ff
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

FFOBJS=		ff.o ../fsck/l3.o 
S5DIR=		$(SRC)/uts/common/fs/S5fs
OBJS=		$(FFOBJS)
SRCS=		$(OBJS:%.o=%.c)

LDLIBS += -lc2 -ladm

ff:		$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

%.o:	%.c
	$(COMPILE.c) $<

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)
