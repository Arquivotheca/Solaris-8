#
#ident	"@(#)Makefile	1.6	96/07/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= zdump

OBJS= zdump.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

.KEEP_STATE:

CFLAGS += -v
XGETFLAGS += -a -x zdump.xcl

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
