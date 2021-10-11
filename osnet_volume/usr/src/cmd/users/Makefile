#
#ident	"@(#)Makefile	1.7	94/03/30 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= listusers
OBJS= users.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) pkginfo

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS)

include ../Makefile.targ
