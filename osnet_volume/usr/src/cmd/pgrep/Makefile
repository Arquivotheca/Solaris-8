#
# Copyright (c) 1997,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/03/30 SMI"

PROG = pgrep
PKILLFILE = pkill

ROOTLINKS = $(ROOTBIN)/$(PKILLFILE)

OBJS = pgrep.o idtab.o psexp.o utils.o
SRCS = $(OBJS:.o=.c)
POFILES = $(OBJS:.o=.po)
lint := LINTFLAGS = -ux

include ../Makefile.cmd

CLOBBERFILES += $(PKILLFILE)
CFLAGS += -v
POFILE = ppgrep.po
XGETFLAGS = -a -x pgrep.xcl

FILEMODE = 0555
GROUP = bin

.KEEP_STATE:

all: $(PROG) $(PKILLFILE)

$(PKILLFILE): $(PROG)
	$(RM) $@
	$(LN) $(PROG) $@

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(POFILE):	$(POFILES)
	$(RM) $@; cat $(POFILES) > $@

install: all $(ROOTLINKS)

$(ROOTLINKS): $(ROOTPROG)
	$(RM) $@
	$(LN) $(ROOTPROG) $@

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
