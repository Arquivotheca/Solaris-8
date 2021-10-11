#
#ident	"@(#)Makefile	1.10	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= printf

include ../Makefile.cmd

LDLIBS += -lgen

CFLAGS += -v
CCMODE=	-Xa

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
