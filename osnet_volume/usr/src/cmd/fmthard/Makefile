#
#ident	"@(#)Makefile	1.2	92/01/13 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmthard/Makefile
#

PROG= fmthard 

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

