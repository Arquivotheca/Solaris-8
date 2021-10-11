#
# Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
#
#"@(#)Makefile	1.1	99/04/07 SMI"
#

PROG= profiles
SRCS= $(PROG:%=%.c)
PROFILES= profiles

include ../Makefile.cmd

ROOTBINPROFILES = $(PROFILES:%=$(ROOTBIN)/%)

.KEEP_STATE:

CPPFLAGS +=	-D_REENTRANT
LDLIBS +=	-lsecdb

all: $(PROG) 

install: all $(ROOTBINPROFILES)

clean:

lint:	lint_SRCS

include ../Makefile.targ
