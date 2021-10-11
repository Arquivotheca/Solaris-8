#
#ident	"@(#)Makefile	1.11	96/03/18 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved.
#

PROG= ed
XPG4PROG= ed
OBJS= ed.o compile.o
SRCS= $(OBJS:%.o=%.c)

POFILES= $(OBJS:%.o=%.po)

include ../Makefile.cmd

CFLAGS += -v
LDLIBS += -lmapmalloc -lcrypt_i -lgen
XGETFLAGS += -a -x ed.xcl


POFILE= ed_cmd.po

%.o: ../expr/%.c
	$(COMPILE.c) -o $@ $<

%.po: ../expr/%.c
	$(COMPILE.cpp) $< > $*.c.i
	$(XGETTEXT) $(XGETFLAGS) $*.c.i
	$(MV) messages.po $@

.KEEP_STATE:

all: $(PROG) $(XPG4)

$(PROG) $(XPG4): $$(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

install: all $(ROOTPROG) $(ROOTXPG4PROG)
	$(RM) $(ROOTBIN)/red
	$(LN) $(ROOTBIN)/ed $(ROOTBIN)/red

values-xpg4.o: ../../lib/libc/port/gen/values-xpg4.c
	$(COMPILE.c) -o $@ ../../lib/libc/port/gen/values-xpg4.c

clean:
	$(RM) $(OBJS)

lint:	lint_PROG

include ../Makefile.targ

clean $(XPG4) := OBJS += values-xpg4.o
