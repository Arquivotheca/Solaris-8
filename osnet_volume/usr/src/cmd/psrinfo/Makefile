#
#ident	"@(#)Makefile	1.4	99/01/05 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/psrinfo/Makefile
#

PROG=	psrinfo

include	../Makefile.cmd

LDLIBS += -lkstat
OWNER = root
GROUP = sys

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

clean:

lint: lint_PROG

include	../Makefile.targ
