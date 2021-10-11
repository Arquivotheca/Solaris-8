#
#ident	"@(#)Makefile	1.15	96/04/20 SMI"
#
# Copyright (c) 1989,1996, by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= grep 

include ../Makefile.cmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64
LDLIBS += -lgen
XGETFLAGS += -a -x grep.xcl


.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
