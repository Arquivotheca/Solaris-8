#
#ident	"@(#)Makefile	1.8	98/06/16 SMI"
#
# Copyright (c) 1989,1990,1992,1996 by Sun Microsystems, Inc.
#

PROG=		in.routed
OBJS=		af.o if.o inet.o input.o main.o output.o \
		startup.o tables.o timer.o trace.o
SRCS=		$(OBJS:%.o=%.c)
TOOLS=		tools/query.c tools/trace.c

include		../../../Makefile.cmd

CPPFLAGS +=	-v
LDLIBS +=	-lsocket -lnsl

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG) $(TOOLS)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include		../../../Makefile.targ
