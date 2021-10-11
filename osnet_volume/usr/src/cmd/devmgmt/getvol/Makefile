#
#ident	"@(#)Makefile	1.6	96/03/18 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/devmgmt/getvol/Makefile
#

PROG= getvol

include ../../Makefile.cmd

LDLIBS +=	-ladm -lpkg

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:   lint_PROG

include ../../Makefile.targ
