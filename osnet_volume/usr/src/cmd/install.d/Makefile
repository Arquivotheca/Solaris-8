#
#ident	"@(#)Makefile	1.8	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= install.d

include ../Makefile.cmd

ROOTUSRSBINTPROG= $(PROG:%=$(ROOTUSRSBIN)/%)

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINTPROG)
	$(MV) $(ROOTUSRSBINPROG) $(ROOTUSRSBIN)/install
	$(RM) $(ROOTETC)/install
	$(SYMLINK) ../usr/sbin/install $(ROOTETC)/install

$(PROG): install.sh
	$(RM) $(PROG)
	$(CP) install.sh $(PROG)

clean:

lint:

include ../Makefile.targ
