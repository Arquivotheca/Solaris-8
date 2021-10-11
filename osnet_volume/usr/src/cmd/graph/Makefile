#
#ident	"@(#)Makefile	1.2	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= graph

include ../Makefile.cmd

LDLIBS += -lm -lplot

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
