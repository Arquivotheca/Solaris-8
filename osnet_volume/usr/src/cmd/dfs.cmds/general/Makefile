#
# ident	"@(#)Makefile	1.8	97/09/17 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/dfs.cmds/general/Makefile
#
# "general" is intended to be a generic way to invoke a filesystem-specific
# command, but at the moment, only /usr/sbin/unshare uses this facility.
# Until someone else needs this functionality, this will be turned off
# so that we don't wind up with a spurious /usr/sbin/general.  To use
# "general" more fully in future, remove the comments from the lines below.
#

PROG=	unshare
#PROG=	general

OBJS=	general.o

#ROOTLINK= $(ROOTUSRSBIN)/unshare

CFLAGS += -s

include ../../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

$(PROG): general.o
	$(LINK.c) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG) $(ROOTLINK)

#$(ROOTLINK): $(ROOTUSRSBINPROG)
#	$(RM) $@; $(LN) $(ROOTUSRSBINPROG) $@

clean:
	$(RM) $(OBJS)

lint: lint_PROG

include ../../Makefile.targ

