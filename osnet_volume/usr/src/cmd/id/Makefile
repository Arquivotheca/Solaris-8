#
#ident	"@(#)Makefile	1.7	96/03/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= id
XPG4PROG= id

include ../Makefile.cmd

.KEEP_STATE:

CFLAGS +=	-v
$(XPG4) := CFLAGS += -DXPG4

all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
