#
#ident	"@(#)Makefile	1.4	96/03/19 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/rmt/Makefile
#

PROG= 		rmt

include ../Makefile.cmd

CCMODE=	-Xa

RELUSRSBIN=	../usr/sbin
ROOTSYMLINK=	$(ROOTETC)/$(PROG)

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	-$(RM) $@; $(SYMLINK) $(RELUSRSBIN)/$(PROG) $@

clean:

lint:	lint_PROG

include ../Makefile.targ
