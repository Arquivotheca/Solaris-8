#
#ident	"@(#)Makefile	1.5	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= mkstr

include ../Makefile.ucbcmd
#
# For messaging
XGETFLAGS += -a -x mkstr.xcl

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
