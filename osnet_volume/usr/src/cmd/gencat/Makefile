#
#ident	"@(#)Makefile	1.9	96/03/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= gencat

OBJS= gencat.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
