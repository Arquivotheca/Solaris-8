#
#ident	"@(#)Makefile	1.6	92/12/15 SMI"	/* SVr4.0 1.6.6.2 */
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/prtvtoc/Makefile
#

PROG= prtvtoc 

include ../Makefile.cmd

RELUSRSBIN=     ../usr/sbin
ROOTSYMLINK=    $(ROOTETC)/$(PROG)
OWNER=		root
GROUP=		sys
LDLIBS +=	-ladm

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	-$(RM) $@; $(SYMLINK) $(RELUSRSBIN)/$(PROG) $@

clean:

lint:   lint_PROG

include ../Makefile.targ

