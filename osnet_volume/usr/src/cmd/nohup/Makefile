#
#ident	"@(#)Makefile	1.8	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

XPG4PROG= nohup
PROG= nohup

include ../Makefile.cmd
CFLAGS += -v
$(XPG4) := CFLAGS += -DXPG4

.KEEP_STATE:

all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

clean:

# do not lint nohup.xpg4
lint:	lint_PROG

include ../Makefile.targ
