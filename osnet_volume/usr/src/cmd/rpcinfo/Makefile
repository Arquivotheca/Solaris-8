#
#ident	"@(#)Makefile	1.8	92/07/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/rpcinfo/Makefile

PROG= rpcinfo

SRCS= $(PROG).c

include ../Makefile.cmd

CPPFLAGS= -DPORTMAP $(CPPFLAGS.master)
LDLIBS += -lnsl

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
