#
#ident	"@(#)Makefile.com	1.2	99/10/07 SMI"
#
# Copyright (c) 1990, 1997, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ipcs/Makefile
#

PROG=		ipcs
XPG4PROG=	ipcs
OBJS=		$(PROG).o
XPG4OBJS=	$(OBJS:%.o=%.xpg4.o)
SRCS=		$(OBJS:%.o=../%.c)
CLEANFILES =	$(OBJS) $(XPG4OBJS)

include ../../Makefile.cmd

CPPFLAGS +=	-D_KMEMUSER
CFLAGS	+=	-v
$(XPG4)	:=	CPPFLAGS += -DXPG4
CFLAGS64 +=	-v
LINTFLAGS =	-x
LDLIBS +=	-lkvm -lelf

FILEMODE = 2555
GROUP = sys

.KEEP_STATE:

all:	$(PROG) $(XPG4)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(XPG4): $(XPG4OBJS)
	$(LINK.c) $(XPG4OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

lint:	lint_SRCS

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ

%.o: ../%.c
	$(COMPILE.c) -o $@ $<
 
%.xpg4.o: ../%.c
	$(COMPILE.c) -o $@ $<
