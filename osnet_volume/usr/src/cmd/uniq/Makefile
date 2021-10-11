#
#ident	"@(#)Makefile	1.11	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= uniq

include ../Makefile.cmd

CFLAGS += -v

XGETFLAGS += -a -x uniq.xcl
.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
