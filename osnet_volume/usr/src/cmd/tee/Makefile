#
#ident	"@(#)Makefile	1.9	96/04/20 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= tee

include ../Makefile.cmd

CFLAGS += -v
CPPFLAGS += -D_FILE_OFFSET_BITS=64

#
# For messaging catalogue file
XGETFLAGS += -a -x tee.xcl

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
