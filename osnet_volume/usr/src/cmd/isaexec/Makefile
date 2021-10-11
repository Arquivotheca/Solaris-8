#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	97/09/06 SMI"
#

PROG= isaexec

include ../Makefile.cmd

CFLAGS += -v

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTLIBPROG)

clean:

lint:
	$(LINT.c) $(PROG).c $(LDLIBS)

include ../Makefile.targ
