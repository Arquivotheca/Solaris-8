#
#ident	"@(#)Makefile	1.13	98/01/08 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= pr
XPG4PROG= pr

include ../Makefile.cmd

SED = sed
DCFILE = $(PROG).dc

$(XPG4):= CPPFLAGS += -DXPG4
$(POFILE) := XGETFLAGS += -a -x pr.xcl

.KEEP_STATE:

all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

clean:

lint:	lint_PROG

$(DCFILE):
	$(RM) messages.po
	$(COMPILE.cpp) $(PROG).c | \
	$(XGETTEXT) $(XGETFLAGS) -t -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../Makefile.targ
