#
#ident	"@(#)Makefile	1.19	99/03/29 SMI"
#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.bin/netstat/Makefile

SGIDPROG=	netstat
PROG=		$(SGIDPROG)

OBJS=		netstat.o unix.o
SRCS=		$(OBJS:%.o=%.c)

include ../../../Makefile.cmd

LDLIBS += -ldhcpagent -lcmd -lsocket -lnsl -lkstat
LINTFLAGS += -m

.KEEP_STATE:

all: $(SGIDPROG) $(NPROG) 

ROOTSGIDPROG=   $(SGIDPROG:%=$(ROOTBIN)/%)
$(ROOTSGIDPROG) :=      FILEMODE=       02555
$(ROOTSGIDPROG) :=      GROUP=          sys

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTSGIDPROG) 

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ

