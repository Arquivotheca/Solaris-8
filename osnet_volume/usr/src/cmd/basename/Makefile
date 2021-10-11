#
#ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= basename
XPG4PROG= basename

SRCS= $(PROG:%=%.sh)

include ../Makefile.cmd

$(XPG4) := CFLAGS += -v

.KEEP_STATE:

all: $(PROG) $(XPG4)

$(PROG): $(PROG).sh
	$(RM) $@
	cat $(PROG).sh > $@
	chmod +x $@

install: all $(ROOTPROG) $(ROOTXPG4PROG)

clean:

lint: lint_PROG

include ../Makefile.targ
