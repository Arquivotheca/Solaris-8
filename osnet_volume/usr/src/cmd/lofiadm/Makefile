#
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/27 SMI"
#

PROG= lofiadm
OBJS= main.o utils.o
SRCS= $(OBJS:%.o=%.c)
POFILES= $(OBJS:%.o=%.po) 

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../Makefile.targ
