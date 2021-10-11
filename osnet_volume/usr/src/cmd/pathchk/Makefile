#
#ident	"@(#)Makefile	1.3	97/01/23 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

PROG= pathchk

include ../Makefile.cmd

CFLAGS		+=	-v

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

XGETFLAGS += -a -x $(PROG).xcl

include ../Makefile.targ
