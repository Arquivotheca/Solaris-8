#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident   "@(#)Makefile 1.1     99/04/21 SMI"

PROG= busstat 

include ../Makefile.cmd

LDLIBS += -lkstat -lrt

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:
	$(RM) $(PROG)

lint:	lint_PROG

include ../Makefile.targ
