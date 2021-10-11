#
#ident	"@(#)Makefile 1.4 92/12/07 SMI;"
#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#
# Makefile for auditconfig

PROG= auditconfig
TEXT_DOMAIN=SUNW_OST_OSCMD

include ../Makefile.cmd

LDLIBS += -lnsl -lbsm

OBJS = auditconfig.o
SRCS = auditconfig.c

.KEEP_STATE:

install: all $(ROOTUSRSBINPROG)
all: $(PROG)

clean:
	rm -f $(PROG) $(PROG).po

lint:	lint_PROG

include ../Makefile.targ
