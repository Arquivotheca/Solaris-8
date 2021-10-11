#
#ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= strchg strconf
SRCS= $(PROG:%=%.c)

include ../../Makefile.cmd

OWNER= root
GROUP= root

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint: lint_SRCS

include ../../Makefile.targ
