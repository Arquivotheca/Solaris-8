#
# ident	"@(#)Makefile	1.5	94/03/30 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/soelim/Makefile
#

include		../Makefile.cmd

PROG =		soelim

OBJS =		soelim.o

SRCS =		$(OBJS:%.o=%.c)


all :		$(PROG)

$(PROG) :	soelim.o
		$(LINK.c) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
		$(POST_PROCESS)
	
install :	all $(ROOTPROG)

clean:
		$(RM) $(OBJS)

strip :		$(PROG)
		$(STRIP) $(PROG)

lint :		lint_SRCS

include		../Makefile.targ
