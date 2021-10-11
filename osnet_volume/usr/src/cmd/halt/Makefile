#
# Copyright (c) 1989, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.15	99/03/30 SMI"

PROG = halt
ROOTLINKS = $(ROOTUSRSBIN)/poweroff $(ROOTUSRSBIN)/reboot
ROOTSYMLINKS= $(ROOTETC)/halt $(ROOTETC)/reboot

lint := LINTFLAGS = -u

include ../Makefile.cmd

FILEMODE = 0755
GROUP = bin

.KEEP_STATE:

all: $(PROG) 

LDLIBS += -lbsm -lsocket -lnsl

install: all $(ROOTUSRSBINPROG) $(ROOTLINKS) $(ROOTSYMLINKS)

$(ROOTLINKS): $(ROOTUSRSBINPROG)
	$(RM) $@
	$(LN) $(ROOTUSRSBINPROG) $@

$(ROOTSYMLINKS):
	$(RM) $@
	$(SYMLINK) ../usr/sbin/$(PROG) $@

clean:

lint:	lint_PROG

include ../Makefile.targ
