#
#ident	"@(#)Makefile	1.7	96/03/18 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= env
XPG4PROG= env
OBJS= env.o

include ../Makefile.cmd

clean $(XPG4) := OBJS += values-xpg4.o

.KEEP_STATE:

all: $(PROG) $(XPG4)

$(PROG) $(XPG4): $$(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

values-xpg4.o: ../../lib/libc/port/gen/values-xpg4.c
	$(COMPILE.c) -o $@ ../../lib/libc/port/gen/values-xpg4.c

clean:
	$(RM) $(OBJS)

lint:	lint_PROG

include ../Makefile.targ
