#
#ident	"@(#)Makefile	1.6	96/05/03 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG=		in.talkd
OBJS=		in.talkd.o announce.o process.o table.o print.o
SRCS=		$(OBJS:%.o=%.c)

include		../../../Makefile.cmd

CPPFLAGS +=	-v
LDLIBS +=	-lsocket -lnsl

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include		../../../Makefile.targ
