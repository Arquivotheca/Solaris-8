#
#ident	"@(#)Makefile 1.3 93/04/19 SMI;"
#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#
# cmd/auditstat/Makefile

PROG= auditstat

include ../Makefile.cmd
include ../Makefile.cmd.bsm

LDLIBS += -lnsl -lbsm 

.KEEP_STATE:

all: $(PROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
