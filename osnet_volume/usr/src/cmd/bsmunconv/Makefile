#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# @(#)Makefile 1.6 98/01/23 SMI
#
# Makefile for bsmunconv
#

PROG = bsmunconv
SRCS = bsmunconv.sh

include ../Makefile.cmd

ETCSECURITYFILES =	bsmunconv
ROOTETCSECURITY =	$(ROOT)/etc/security
ROOTETCSECURITYSPOOL =	$(ROOT)/etc/security/spool
$(ROOTETCSECURITY) :=   DIRMODE = 0755
$(ROOTETCSECURITY) :=   OWNER = root
$(ROOTETCSECURITY) :=   GROUP = sys
$(ROOTETCSECURITYSPOOL) :=   DIRMODE = 0755
$(ROOTETCSECURITYSPOOL) :=   OWNER = root
$(ROOTETCSECURITYSPOOL) :=   GROUP = sys
ROOTETCSECURITYFILES =  $(ETCSECURITYFILES:%=$(ROOTETCSECURITY)/%)
$(ROOTETCSECURITYFILES) := FILEMODE = 0740
$(ROOTETCSECURITYFILES) := OWNER = root
$(ROOTETCSECURITYFILES) := GROUP = sys

all: $(PROG)

install: all $(ROOTETCSECURITY) $(ROOTETCSECURITYFILES) $(ROOTETCSECURITYSPOOL)

$(ROOTETCSECURITY):
	$(INS.dir)

$(ROOTETCSECURITYSPOOL):
	$(INS.dir)

$(ROOTETCSECURITY)/%:%
	$(INS.file)

clean:
	$(RM) -f $(PROG)

clobber: clean

include ../Makefile.targ

.KEEP_STATE:
