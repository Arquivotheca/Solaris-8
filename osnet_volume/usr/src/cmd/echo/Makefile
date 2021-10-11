#
#ident	"@(#)Makefile	1.8	96/06/24 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= echo

include ../Makefile.cmd

CFLAGS += -v

# This flag is being added only for SCO (x86) compatibility  
CFLAGS += $(iBCS2FLAG)

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
