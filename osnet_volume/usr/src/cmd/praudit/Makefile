#
# @(#)Makefile 1.14 99/12/06 SMI
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/praudit/Makefile

PROG = praudit

include ../Makefile.cmd

TEXT_DOMAIN=SUNW_OST_OSCMD


LDLIBS += -lbsm -lsocket -lnsl

OBJS = praudit.o
SRCS = praudit.c
POFILE = praudit_all.po
POFILES = praudit.po
# needed because POFILES is deleted by clobber, but it's under
# SCCS control here.
OPOFILES = structures.po

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

$(POFILE):	$(POFILES) $(OPOFILES)
	$(RM) -f $@
	cat $(POFILES) $(OPOFILES) > $@

lint:	lint_PROG

clean:
	rm -f $(OBJS) $(PROG).po $(POFILE)

include ../Makefile.targ
