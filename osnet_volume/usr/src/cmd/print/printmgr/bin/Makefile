#
# ident	"@(#)Makefile	1.6	99/07/12 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for print manager wrapper script
#
include		$(SRC)/cmd/Makefile.cmd

PROG =		printmgr

ROOTBIN =	$(ROOT)/usr/sadm/admin/bin

ROOTBINPROG = 	$(PROG:%=$(ROOTBIN)/%)

ROOTDIRS =	$(ROOT)/usr/sadm/admin \
		$(ROOT)/usr/sadm/admin/bin

$(ROOTBINPROG):= OWNER	=	root
$(ROOTBINPROG):= GROUP	=	lp

.KEEP_STATE:

all:		$(PROG)
install:	all $(ROOTDIRS) $(ROOTBINPROG)	

$(ROOTBIN)/%:	%
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir)

lint strip _msg:

clean clobber:
	-$(RM) $(PROG)
