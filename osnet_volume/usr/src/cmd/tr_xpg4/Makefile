#
#ident	"@(#)Makefile	1.1	98/12/04 SMI"
#
# Copyright (c) 1989,1998 by Sun Microsystems, Inc.
# All rights reserved.

PROG= tr
XPG4PROG= tr

include ../Makefile.cmd

POFILE= tr_xpg4.po
POFILES= tr.po

.KEEP_STATE:

CFLAGS += -v

all: $(XPG4)

install: all $(ROOTXPG4PROG)

$(POFILE):	$(POFILES)
	$(RM) $@
	$(CP) $(POFILES) $@

clean:

lint:	lint_PROG

include ../Makefile.targ
