#
# ident	"@(#)Makefile	1.7	94/03/30 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/dfs.cmds/dfshares/Makefile
#

PROG=	dfshares

OBJS=	dfshares.o

ROOTLINK= $(ROOTUSRSBIN)/dfmounts

CFLAGS += -s

include ../../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

$(PROG): dfshares.o
	$(LINK.c) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG) $(ROOTLINK)

$(ROOTLINK): $(ROOTUSRSBINPROG)
	$(RM) $@; $(LN) $(ROOTUSRSBINPROG) $@

clean:
	$(RM) $(OBJS)

lint: lint_PROG

include ../../Makefile.targ
