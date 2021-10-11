#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	99/08/15 SMI"

include	../../../Makefile.cmd

PROG =		cputrack
OBJS =		$(PROG).o caps.o time.o eventset.o
SRCS =		$(OBJS:%.o=../../common/%.c)
LDLIBS +=	-lcpc -lpctx -lproc

CFLAGS +=	-v
CFLAGS64 +=	-v

LINTFLAGS =	-mux

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

strip:
	$(STRIP) $(PROG)

%.o:	../../common/%.c
	$(COMPILE.c) $<

include	../../../Makefile.targ
