#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)Makefile	1.14	96/08/16 SMI"

# cmd/msgfmt/Makefile

include ../Makefile.cmd

OBJS= xgettext.o xgettext.lx.o

PROG=  msgfmt xgettext
SRCS= $(PROG:%=%.c)

CCMODE= -Xa

CLOBBERFILES += read_mo

.KEEP_STATE:

all: $(PROG) read_mo

xgettext: $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:  
	$(RM) $(OBJS)

lint: lint_SRCS
	
include ../Makefile.targ
