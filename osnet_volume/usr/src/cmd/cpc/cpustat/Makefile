#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/10/14 SMI"

PROG =		cpustat
OBJS =		$(PROG).o caps.o time.o eventset.o
SRCS =		$(OBJS:%.o=../common/%.c)

include ../../Makefile.cmd

CFLAGS	+=	-v
LINTFLAGS =	-mux
LDLIBS +=	-lcpc -lposix4 -lthread -R/usr/lib/lwp

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

strip: all
	$(STRIP) $(PROG)

%.o:	../common/%.c
	$(COMPILE.c) $<

POFILES =	../common/$(PROG).po ../common/caps.po
POFILE =	$(PROG)_cmd.po

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

include ../../Makefile.targ
