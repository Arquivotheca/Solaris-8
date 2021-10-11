#
#ident	"@(#)Makefile	1.6	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/cmd-inet/usr.sbin/htable/Makefile

PROG=	htable
OBJS=	htable.o parse.o scan.o
SRCS=	htable.c parse.y scan.l

include ../../../Makefile.cmd

CPPFLAGS +=	-DSYSV -DSTRNET -DBSD_COMP
LDLIBS +=	-lsocket -lnsl
YFLAGS +=		-d

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS) y.tab.h y.tab.c parse.c scan.c

lint:	lint_PROG

include ../../../Makefile.targ

# explicit target for the NSE
y.tab.h: parse.y
	$(YACC.y) $?
