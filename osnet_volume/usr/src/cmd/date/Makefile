#
#ident	"@(#)Makefile	1.10	96/03/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= date
XPG4PROG= date
OBJS= date.o

include ../Makefile.cmd

CFLAGS += -v

clean $(XPG4) := OBJS += values-xpg4.o
XGETFLAGS += -a -x $(PROG).xcl

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

# do not lint date.xpg4
lint:	lint_PROG

include ../Makefile.targ
