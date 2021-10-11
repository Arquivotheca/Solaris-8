#
#ident	"@(#)Makefile	1.5	96/07/01 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/dhcp/pntadm/Makefile
#

PROG =		pntadm
OBJS=		main.o dd_error.o
SRCS=		main.c ../common/dd_error.c

include ../../../../Makefile.cmd

LDLIBS += -ldhcp -lsocket -lnsl

.KEEP_STATE:
POFILE = $(PROG).po
POFILES = $(SRCS:%.c=%.po)

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE):	$(POFILES)
	$(RM)	$@
	cat $(POFILES) > $@

%.o: ../common/%.c
	$(COMPILE.c) $<

install:	all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../../Makefile.targ
