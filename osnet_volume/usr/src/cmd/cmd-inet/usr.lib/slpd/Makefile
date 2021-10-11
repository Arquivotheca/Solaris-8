#
# ident	"@(#)Makefile	1.1	99/03/31 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/slpd/Makefile
#

PROG = slpd

.KEEP_STATE:

all:	$(PROG)

include ../../../Makefile.cmd
include ../Makefile.lib

LDLIBS += -lsocket -lnsl -lslp

install:	all $(ROOTLIBINETPROG)

clean:

include ../../../Makefile.targ

lint:	lint_PROG
