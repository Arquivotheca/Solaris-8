#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	98/05/01 SMI"

PROG = dumpadm

OBJS = main.o dconf.o minfree.o utils.o swap.o
SRCS = $(OBJS:.o=.c)

lint := LINTFLAGS = -mx

include ../Makefile.cmd

CFLAGS += -v

FILEMODE = 0555
OWNER = root
GROUP = bin

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
