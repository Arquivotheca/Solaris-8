#
#ident	"@(#)Makefile	1.4	96/04/20 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= mkfile

include ../Makefile.cmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
