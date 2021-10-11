#
#ident	"@(#)Makefile	1.6	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= news 

include ../Makefile.cmd

VARNEWSD= $(ROOT)/var/news
DIRS= $(VARNEWSD)

# owner and group set in Targetdirs
#DIRS := OWNER = root
#DIRS := GROUP = sys

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

# Don't re-install dirs already installed by Targetdirs
#$(DIRS):
#	$(INS.dir)

clean:

lint:	lint_PROG

include ../Makefile.targ
