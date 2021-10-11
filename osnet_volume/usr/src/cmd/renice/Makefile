#
#ident	"@(#)Makefile	1.13	96/04/10 SMI"
#
# Copyright (c) 1989, 1994 by Sun Microsystems, Inc.
#

PROG= renice

include ../Makefile.cmd

SRCS= renice.c
OBJS= renice.o

CFLAGS += -v

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS) $(PROG)

lint:	lint_SRCS

include ../Makefile.targ
