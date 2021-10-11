#
#ident	"@(#)Makefile	1.8	96/03/18 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmt/Makefile

PROG= fmt

OBJS= fmt.o
HEAD= ../mailx/head.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS) $(HEAD)
	$(LINK.c) $(OBJS) $(HEAD) -o $@ $(LDLIBS) -ldl
	$(POST_PROCESS)

$(HEAD): FRC
	cd $(@D); pwd; $(MAKE) $(@F)
	@pwd

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ

FRC:
