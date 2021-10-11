#
#ident	"@(#)Makefile	1.6	95/08/26 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/fuser/Makefile

PROG= fuser

include ../Makefile.cmd

#LDFLAGS = -L$(ROOT)/usr/ccs/lib -s
LDLIBS += -lkstat

ROOTSYMLINK= $(ROOTETCPROG)

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../usr/sbin/$(PROG) $@

clean:

lint:	lint_PROG

include ../Makefile.targ
