#
#ident	"@(#)Makefile	1.10	99/03/30 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/last/Makefile
#

PROG= last
UTMPS= wtmpx
ROOTADM= $(ROOT)/var/adm
ROOTUTMPS= $(UTMPS:%=$(ROOTADM)/%)
SYMSETC= $(UTMPS:%=symlinks_%)
ETCTOVARADM= ../var/adm

include ../Makefile.cmd

$(ROOTUTMPS):= FILEMODE= 644
$(ROOTUTMPS):= OWNER= adm
$(ROOTUTMPS):= GROUP= adm

$(ROOTADM)/%: %
	$(INS.file)

$(UTMPS):
	touch $@

symlinks_% : %
	-$(RM) $(ROOTETC)/$< ; $(SYMLINK) $(ETCTOVARADM)/$< $(ROOTETC)/$<
	$(CH)@-$(CHOWN) adm $(ROOTETC)/$< ; $(CHGRP) adm $(ROOTETC)/$<

.KEEP_STATE:

all: $(PROG) $(UTMPS)

install: all $(ROOTPROG) $(SYMSETC) $(ROOTUTMPS)

clean:
	$(RM) $(UTMPS)

lint:	lint_PROG

include ../Makefile.targ
