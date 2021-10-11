#
#ident	"@(#)Makefile	1.9	97/10/17 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= fgrep

include ../Makefile.cmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64


.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
