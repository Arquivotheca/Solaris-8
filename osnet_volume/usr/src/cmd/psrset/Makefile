#
#ident	"@(#)Makefile	1.2	98/11/03 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/psrset/Makefile
#

PROG=	psrset

include	../Makefile.cmd

OWNER = root
GROUP = sys

CFLAGS += -v

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

clean:

lint: lint_PROG

include	../Makefile.targ
