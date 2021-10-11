# @(#)Makefile 1.9 97/11/14 SMI
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/audit/Makefile
#

PROG = audit

include ../Makefile.cmd

TEXT_DOMAIN=SUNW_OST_OSCMD
POFILE=$(PROG).po

OBJS = audit.o
SRCS = $(OBJS:.o=.c)

CFLAGS += -DNO_AUDIT_WRITE

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

clean:
	rm -f $(OBJS) $(POFILE) $(PROG)

lint:	lint_PROG

include ../Makefile.targ
