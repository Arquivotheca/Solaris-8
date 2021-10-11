#
#ident	"@(#)Makefile	1.3	96/03/18 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

PROG= getconf
OBJS= getconf.o

include ../Makefile.cmd

OBJS 		+= 	values-xpg4.o
CFLAGS		+=	-v

.KEEP_STATE:

all: $(PROG)

$(PROG): $$(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

values-xpg4.o: ../../lib/libc/port/gen/values-xpg4.c
	$(COMPILE.c) -o $@ ../../lib/libc/port/gen/values-xpg4.c

clean:

lint:	lint_PROG

include ../Makefile.targ
