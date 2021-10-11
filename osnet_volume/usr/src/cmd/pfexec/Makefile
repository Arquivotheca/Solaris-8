#
# Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/04/23 SMI"
#

PROG= pfexec
SRCS= $(PROG:%=%.c)
PFEXEC= pfexec

include ../Makefile.cmd

FILEMODE =	04555

ROOTBINPFEXEC = $(PFEXEC:%=$(ROOTBIN)/%)

.KEEP_STATE:

CPPFLAGS +=	-D_REENTRANT
LDLIBS +=	-lsecdb

all: $(PROG) 

install: all $(ROOTBINPFEXEC)

clean:

lint:	lint_SRCS

include ../Makefile.targ
