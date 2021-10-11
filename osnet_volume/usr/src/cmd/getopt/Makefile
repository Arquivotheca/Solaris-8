#
#ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= getopt
SHFILES= getoptcvt
CLOBBERFILES = $(SHFILES)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) $(SHFILES) 

install: all $(ROOTPROG) $(ROOTLIBSHFILES)

clean:

lint:	lint_PROG

include ../Makefile.targ
