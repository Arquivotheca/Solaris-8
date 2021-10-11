#
#ident	"@(#)Makefile	1.9	97/01/23 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/grpck/Makefile
#

PROG= grpck

include ../Makefile.cmd

RELUSRSBIN=	../usr/sbin
ROOTSYMLINK=	$(ROOTETC)/$(PROG)

.KEEP_STATE:

all: $(PROG)

XGETFLAGS= -a -x grpck.xcl

install: all $(ROOTUSRSBINPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	-$(RM) $@; $(SYMLINK) $(RELUSRSBIN)/$(PROG) $@

clean:

lint:	lint_PROG

include ../Makefile.targ
