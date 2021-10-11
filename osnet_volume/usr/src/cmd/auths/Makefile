#
# Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
#
#"@(#)Makefile	1.1	99/04/07 SMI"
#

PROG= auths
SRCS= $(PROG:%=%.c)
AUTHS= auths

include ../Makefile.cmd

ROOTBINAUTHS = $(AUTHS:%=$(ROOTBIN)/%)

.KEEP_STATE:

CPPFLAGS +=	-D_REENTRANT
LDLIBS +=	-lsecdb -lcmd

all: $(PROG) 

install: all $(ROOTBINAUTHS)

clean:

lint:	lint_SRCS

include ../Makefile.targ
