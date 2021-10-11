#
#ident	"@(#)Makefile	1.7	99/02/05 SMI"
#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= link 
XPG4PROG= link
OBJS= link.o

include ../Makefile.cmd

clean $(XPG4) := OBJS += values-xpg4.o

FILEMODE= 0555

.KEEP_STATE:

all: $(PROG) $(XPG4)

$(PROG) $(XPG4): $$(OBJS)
	$(LINK.c) -o $@ $(OBJS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG) $(ROOTXPG4PROG)
	-$(RM) $(ROOTETCPROG)
	-$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

values-xpg4.o: ../../lib/libc/port/gen/values-xpg4.c
	$(COMPILE.c) -o $@ ../../lib/libc/port/gen/values-xpg4.c

clean:
	$(RM) $(OBJS)

lint:	lint_PROG

include ../Makefile.targ
