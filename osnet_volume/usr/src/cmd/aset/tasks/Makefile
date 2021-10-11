#
#ident	"@(#)Makefile	1.1	91/07/26 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# cmd/aset/tasks/Makefile

PROG= cklist create_cklist eeprom env firewall sysconf tune usrgrp \
      firewall.restore sysconf.restore tune.restore usrgrp.restore

FILES= gpchk.awk pwchk.awk swchk.awk

include ../../Makefile.cmd

# we assume the Makefile above created ROOTDIR
ROOTDIR= $(ROOT)/usr/aset/tasks

ROOTPROG= $(PROG:%=$(ROOTDIR)/%)
ROOTFILES= $(FILES:%=$(ROOTDIR)/%)

DIRMODE= 700
FILEMODE= 700
OWNER= root

# install rule
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(FILES)

install: all $(ROOTPROG) $(ROOTFILES)

lint clean:

include ../../Makefile.targ
