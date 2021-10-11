#
#ident	"@(#)Makefile	1.4	96/04/20 SMI"
#
# Copyright (c) 1994, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cksum/Makefile

PROG= cksum

OBJS= cksum.o

CLOBBERFILES= $(PROG) $(PROG2)

include ../Makefile.cmd

CPPFLAGS= -I. $(CPPFLAGS.master)

CPPFLAGS += -D_FILE_OFFSET_BITS=64


.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

lint: lint_SRCS

clean:
	$(RM) $(OBJS)

include ../Makefile.targ
