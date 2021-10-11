#
#ident	"@(#)Makefile	1.2	97/09/04 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= captoinfo

OBJS= captoinfo.o otermcap.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

LDLIBS += -lcurses
CPPFLAGS += -I../../lib/libcurses/screen


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
