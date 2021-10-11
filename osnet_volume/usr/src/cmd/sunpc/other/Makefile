#
#ident	"@(#)Makefile	1.3	92/07/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/sunpc/other/Makefile

PROG= dos2unix unix2dos
SRCS= ${PROG:%=%.c}

include ../../Makefile.cmd

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

clean:

# the clobber target is in Makefile.targ and removes PROG

lint:	lint_SRCS

include ../../Makefile.targ
