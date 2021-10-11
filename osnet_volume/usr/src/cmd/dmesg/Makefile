#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.7	98/09/30 SMI"
#

PROG= dmesg

include ../Makefile.cmd

ROOTSYMLINK=$(ROOTPROG)

.KEEP_STATE:

all: $(PROG)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../sbin/$(PROG) $@

install: all $(ROOTUSRSBINPROG) $(ROOTSYMLINK)

clean:

lint:

include ../Makefile.targ
