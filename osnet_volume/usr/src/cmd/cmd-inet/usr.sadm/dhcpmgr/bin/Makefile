#
#ident	"@(#)Makefile	1.4	99/05/12 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/bin/Makefile
#
include		$(SRC)/cmd/Makefile.cmd

PROG =		dhcpmgr

ROOTBIN =	$(ROOT)/usr/sadm/admin/bin

ROOTBINPROG = 	$(PROG:%=$(ROOTBIN)/%)

.KEEP_STATE:

all:		$(PROG)
install:	all $(ROOTBIN) $(ROOTBINPROG)

$(ROOTBIN)/%:	%
	$(INS.file)

$(ROOTBIN):
	$(INS.dir)

clean clobber:
	-$(RM) $(PROG)

lint _msg:
