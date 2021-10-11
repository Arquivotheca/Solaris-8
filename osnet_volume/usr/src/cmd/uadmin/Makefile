#
# Copyright (c) 1989, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.15	99/08/25 SMI"

# build two versions, dynamic and static
PROG= uadmin
STATPROG= uadmin

# install rule for static version
# deliberately ahead of a similar rule in ../Makefile.cmd
#
ROOT= /proto
$(ROOT)/sbin/%: %.static
	$(INS.rename)

include ../Makefile.cmd

OWNER = root
GROUP = sys
SLDLIBS_LIST = -lbsm -lnsl
LDLIBS_LIST = -lbsm
LDLIBS += $(LDLIBS_LIST)
CLOBBERFILES= $(STATIC)
LINTFLAGS = -ux

# conditional assignments for the static version
#
$(STATIC) := DLLIB =

$(STATIC) := LDLIBS = -Bstatic -lc $(SLDLIBS_LIST) -Bdynamic -ldl -Bstatic

$(STATIC) := LDFLAGS = $(LDFLAGS.cmd) -dy -R/etc/lib $(LDLIBS.cmd)

.KEEP_STATE:

all: $(PROG) $(STATIC)

$(STATIC): $(PROG).c
	LD_OPTIONS=-I/etc/lib/ld.so.1 $(LINK.c) $(PROG).c -o $@ $(LDLIBS)

install: all $(ROOTSBINPROG) $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	-$(SYMLINK) ../sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
