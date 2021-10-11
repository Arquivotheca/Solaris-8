#
#pragma ident	"@(#)Makefile	1.7	99/12/06 SMI"
#
# Copyright (c) 1989,1999 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= crypt

include ../Makefile.cmd

LDLIBS += -lcrypt_i

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ

