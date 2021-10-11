#
#ident	"@(#)Makefile	1.6	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/strings/Makefile

PROG= strings

include ../Makefile.cmd

LDLIBS += -lelf

CFLAGS += -v

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
