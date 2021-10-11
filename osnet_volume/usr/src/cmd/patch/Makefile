#
#ident	"@(#)Makefile	1.4	97/01/24 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=	patch

SRCS=	patch.c pch.c util.c file.c

OBJS=	$(SRCS:%.c=%.o)

include ../Makefile.cmd

CFLAGS += -v

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG)

%.po: %.c
	$(COMPILE.cpp) $< > $*.c.i
	$(XGETTEXT) $(XGETFLAGS) $*.c.i ;\
	$(RM)   $@ ;\
	sed "/^domain/d" < messages.po  > $@ ;\
	$(RM) messages.po $*.c.i

POFILES= $(OBJS:%.o=%.po)
POFILE= $(PROG)_cmd.po

XGETFLAGS += -a -x patch.xcl

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

install: all $(ROOTPROG)

lint:	lint_SRCS

clean:
	$(RM) $(OBJS)

$(PROG):	$(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

include		../Makefile.targ

