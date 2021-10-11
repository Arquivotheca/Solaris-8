#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.5	99/10/14 SMI"
#
# cmd/cmd-inet/sbin/netstrategy/Makefile
#

PROG    = netstrategy
OBJS    = $(PROG).o

include ../../../Makefile.cmd

LDLIBS  += -lsocket -lnsl -lc
LDFLAGS += -dy -znodefaultlib -R/etc/lib $(LDFLAGS.cmd)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		LD_OPTIONS=-I/etc/lib/ld.so.1	\
			$(LINK.c) $(OBJS) -o $@ -ldl -Bstatic $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTSBINPROG)

clean:
		$(RM) $(OBJS)

lint:		lint_PROG

include ../../../Makefile.targ
