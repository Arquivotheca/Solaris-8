#
#ident	"@(#)Makefile	1.10	97/01/23 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= cut

include ../Makefile.cmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64

XGETFLAGS += -a -x $(PROG).xcl

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
