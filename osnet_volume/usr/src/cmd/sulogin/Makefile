#
#ident	"@(#)Makefile	1.34	00/09/14 SMI"
#
# Copyright (c) 1993, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= sulogin

include ../Makefile.cmd

CPPFLAGS += -I../../lib/libcmd/inc

LDLIBS_LIST = -lcrypt_i -lcmd -lnsl

LDLIBS = -Bstatic -lc $(LDLIBS_LIST) -Bdynamic -ldl -Bstatic

LDFLAGS = $(LDLIBS.cmd) -dy -R/etc/lib $(LDFLAGS.cmd)

FILEMODE = 0555
OWNER = root
GROUP = bin

.KEEP_STATE:

all: $(PROG)


install: all $(ROOTSBINPROG)
	$(RM) $(ROOTETCPROG)
	$(SYMLINK) ../sbin/$(PROG) $(ROOTETCPROG)


clean:

lint:	lint_PROG

include ../Makefile.targ
