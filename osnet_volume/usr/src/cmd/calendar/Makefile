#
#ident	"@(#)Makefile	1.8	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= calprog
SHFILES= calendar
CLOBBERFILES = $(SHFILE)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) $(SHFILES) 

$(SHFILES): $(SHFILES).sh
	-$(RM) $(SHFILES)
	$(CP) $(SHFILES).sh $(SHFILES)

install: all $(ROOTSHFILES) $(ROOTLIBPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
