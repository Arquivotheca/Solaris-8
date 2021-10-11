#
#ident	"@(#)Makefile	1.8	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= rmdir

include ../Makefile.cmd

LDLIBS += -lgen
CFLAGS += -v

XGETFLAGS += -a -x rmdir.xcl

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
