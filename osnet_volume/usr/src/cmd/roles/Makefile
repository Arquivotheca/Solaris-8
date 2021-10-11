#
# Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
#
#"@(#)Makefile	1.1	99/04/07 SMI"
#

PROG= roles
SRCS= $(PROG:%=%.c)
ROLES= roles

include ../Makefile.cmd

ROOTBINROLES = $(ROLES:%=$(ROOTBIN)/%)

.KEEP_STATE:

CPPFLAGS +=	-D_REENTRANT
LDLIBS +=	-lsecdb

all: $(PROG) 

install: all $(ROOTBINROLES)

clean:

lint:	lint_SRCS

include ../Makefile.targ
