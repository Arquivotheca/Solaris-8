#
#ident "@(#)Makefile	1.4	96/03/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		s5fs
PROG=		mount
ATTMK= $(PROG)

include		../../Makefile.fstype

CPPFLAGS += -I../.. -o $@
OBJS=		$(PROG).o fslib.o
SRCS=		$(OBJS:%.o=%.c)

$(PROG) $(STATIC):	$(OBJS)
		$(LINK.c) $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:	lint_SRCS

clean:     
	-$(RM) $(OBJS)

fslib.o: ../../fslib.c
	$(COMPILE.c) ../../fslib.c
