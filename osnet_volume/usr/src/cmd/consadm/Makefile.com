#
#ident	"@(#)Makefile.com	1.1	98/12/14 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/consadm/Makefile.com
#

PROG=		consadm

COMMONOBJS=	$(PROG).o utils.o
SRCS=           ../$(PROG).c
OBJS=		$(COMMONOBJS)

lint := LINTFLAGS = -mx

include ../../Makefile.cmd

CFLAGS	+=	-v
CPPFLAGS +=

FILEMODE = 0555
OWNER = root
GROUP = sys

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)
	$(RM) $(ROOTUSRSBIN)/consadmd
	$(LN) $(ROOTUSRSBINPROG) $(ROOTUSRSBIN)/consadmd

clean:
	-$(RM) $(OBJS)

lint:	lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

%.o:	./%.c
	$(COMPILE.c) $<

include ../../Makefile.targ
