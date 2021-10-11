#
#ident	"@(#)Makefile	1.9	96/04/20 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= pg

include ../Makefile.cmd

CFLAGS += -DSINGLE -v
LDLIBS += -lgen -lcurses
XGETFLAGS += -a -x $(PROG).xcl
CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
