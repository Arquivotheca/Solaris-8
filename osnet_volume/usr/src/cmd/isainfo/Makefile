#
# Copyright (c) 1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma	ident	"@(#)Makefile	1.1	98/01/30 SMI"
#

PROG= isainfo

include ../Makefile.cmd

CFLAGS	+=	-v
LDLIBS	+=	-lelf

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
