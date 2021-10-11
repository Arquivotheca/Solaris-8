#
#ident	"@(#)Makefile	1.6	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= ul

include ../Makefile.cmd

LDLIBS += -lcurses

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
