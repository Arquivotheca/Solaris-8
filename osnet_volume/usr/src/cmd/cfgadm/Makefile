#
# Copyright (c) 1996-1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	99/02/17 SMI"
#
# cmd/cfgadm/Makefile

PROG= cfgadm 

include ../Makefile.cmd

SED= sed
DCFILE= $(PROG).dc

XGETFLAGS += -a -x cfgadm.xcl

LDLIBS += -lcfgadm 
LINTFLAGS += $(ENVLDFLAGS1) $(ENVLDFLAGS2) $(ENVLDFLAGS3) $(LDLIBS)

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:	lint_PROG

$(DCFILE): $(PROG).c
	$(RM) $(DCFILE)
	$(COMPILE.cpp) $(PROG).c | \
	$(XGETTEXT) -t -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../Makefile.targ
