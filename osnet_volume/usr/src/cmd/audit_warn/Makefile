# @(#)Makefile 1.5 97/12/05 SMI
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/audit_warn/Makefile
#

PROG = audit_warn
SRCS = audit_warn.sh

include ../Makefile.cmd

ETCSECURITYFILES =	audit_warn
ROOTETCSECURITY =	$(ROOT)/etc/security
$(ROOTETCSECURITY) :=   DIRMODE = 0755
$(ROOTETCSECURITY) :=   OWNER = root
$(ROOTETCSECURITY) :=   GROUP = sys
ROOTETCSECURITYFILES =  $(ETCSECURITYFILES:%=$(ROOTETCSECURITY)/%)
$(ROOTETCSECURITYFILES) := FILEMODE = 0740
$(ROOTETCSECURITYFILES) := OWNER = root
$(ROOTETCSECURITYFILES) := GROUP = sys

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTETCSECURITY) $(ROOTETCSECURITYFILES)

$(ROOTETCSECURITY):
	$(INS.dir)

$(ROOTETCSECURITY)/%:%
	$(INS.file)

clean:
	$(RM) -f $(PROG)

clobber: clean

include ../Makefile.targ
