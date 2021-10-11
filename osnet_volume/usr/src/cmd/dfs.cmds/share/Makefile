#
# ident	"@(#)Makefile	1.6	94/03/30 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/dfs.cmds/share/Makefile
#

PROG=	share

OBJS=	share.o

CFLAG += -s

include ../../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

$(PROG): share.o
	$(LINK.c) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint: lint_PROG

include ../../Makefile.targ
