#
#ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= fstyp

include ../Makefile.cmd

OWNER = root
GROUP = sys

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(PROG).sh
	$(RM) $@
	sed -e "s/TEXT_DOMAIN/${TEXT_DOMAIN}/g" $(PROG).sh > $@
	chmod +x $@

install: all $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	-$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
