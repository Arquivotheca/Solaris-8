#
#ident	"@(#)Makefile	1.2	96/10/17 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/symorder/Makefile

PROG=		symorder

SRCS=		$(PROG:%=%.sh)

include		../../Makefile.cmd

.KEEP_STATE:

all:		$(PROG)

install:	all $(ROOTCCSBINPROG)

clean \
clobber \
lint:

include		../../Makefile.targ
