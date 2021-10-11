#
#ident	"@(#)Makefile	1.1	97/08/28 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#

PROG= infocmp

include ../Makefile.cmd

.KEEP_STATE:

CFLAGS += -v
XGETFLAGS += -a -x infocmp.xcl
LDLIBS +=  -lcurses
CPPFLAGS += -I../../lib/libcurses/screen


all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
