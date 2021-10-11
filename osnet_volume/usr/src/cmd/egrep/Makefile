#
#ident	"@(#)Makefile	1.9	00/09/14 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= egrep

include ../Makefile.cmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)


	
clean:
	$(RM) $(PROG).o $(PROG).c

lint:

include ../Makefile.targ
