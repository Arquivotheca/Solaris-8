#
#ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= diff3prog
SHFILES= diff3
CLOBBERFILES = $(SHFILES)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) $(SHFILES) 

$(SHFILES): $(SHFILES).sh
	$(RM) $(SHFILES)
	$(CP) $(SHFILES).sh $(SHFILES)

install: all $(ROOTSHFILES) $(ROOTLIBPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
