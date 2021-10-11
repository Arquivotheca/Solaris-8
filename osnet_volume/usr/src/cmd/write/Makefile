#
#ident	"@(#)Makefile	1.11	99/03/30 SMI"
#
# Copyright (c) 1994,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/write/Makefile

PROG= write

include ../Makefile.cmd

SED= sed
DCFILE= $(PROG).dc

FILEMODE = 02555
GROUP = tty
XGETFLAGS += -a -x write.xcl

.KEEP_STATE:

CFLAGS += -v

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

$(DCFILE):
	$(RM) messages.po
	$(COMPILE.cpp) $(PROG).c | \
	$(XGETTEXT) -c TRANSLATION_NOTE -t -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../Makefile.targ
