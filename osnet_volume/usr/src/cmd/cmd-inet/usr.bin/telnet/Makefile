#
#ident	"@(#)Makefile	1.3	94/12/07 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG=		telnet
OBJS=		telnet.o authenc.o commands.o main.o network.o ring.o \
		sys_bsd.o terminal.o utilities.o genget.o herror.o
SRCS=		$(OBJS:%.o=%.c)

include		../../../Makefile.cmd

CPPFLAGS +=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOLD_ENVIRON
CPPFLAGS +=	-DLINEMODE -DFILIO_H -DSYSV
LDLIBS +=	-lsocket -lnsl -ltermlib

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include		../../../Makefile.targ
