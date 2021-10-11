#
#ident	"@(#)Makefile	1.18	99/06/15 SMI"
#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= who
XPG4PROG= who
DCFILE=$(PROG).dc
UTMPS= utmpx
ROOTADM= $(ROOT)/var/adm
ROOTUTMPS= $(UTMPS:%=$(ROOTADM)/%)
SYMSETC= $(UTMPS:%=$(ROOTETC)/%)
CLOBBERFILES += $(UTMPS)

include ../Makefile.cmd

SED= sed

$(ROOTUTMPS):= FILEMODE= 644
$(ROOTUTMPS):= OWNER= root
$(ROOTUTMPS):= GROUP= bin

$(ROOTADM)/%: %
	$(INS.file)

.KEEP_STATE:

CFLAGS += -v
$(XPG4):= CPPFLAGS += -DXPG4

all: $(PROG) $(UTMPS) $(XPG4)

$(UTMPS):
	touch $@

install: all $(ROOTPROG) $(ROOTUTMPS) $(SYMSETC) $(ROOTXPG4PROG)

$(SYMSETC):
	-$(RM) $@; $(SYMLINK) ../var/adm/$(@F) $@
	$(CH)@-$(CHOWN) root $@
	$(CH)@-$(CHGRP) bin $@

clean:

lint:	lint_PROG

$(DCFILE): $(PROG).c
	$(RM) $(DCFILE)
	$(COMPILE.cpp) $(PROG).c | \
	$(XGETTEXT) $(XGETFLAGS) -t -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../Makefile.targ
