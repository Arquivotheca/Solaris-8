#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/prstat/Makefile.com
#

#ident	"@(#)Makefile.com	1.3	99/11/03 SMI"

PROG = prstat
OBJS = prstat.o prfile.o prtable.o prsort.o prutil.o
SRCS = $(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS += -v
CFLAGS64 += -v
LDLIBS += -ltermcap -lcurses
LINTFLAGS += $(LDLIBS) -u
LINTFLAGS64 += -mux

FILEMODE = 0555
GROUP = bin

.KEEP_STATE:

.PARALLEL : $(OBJS)

all: $(PROG)

clean:
	$(RM) $(OBJS)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

%.o:	../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

lint:
	$(LINT.c) $(SRCS)

include ../../Makefile.targ
