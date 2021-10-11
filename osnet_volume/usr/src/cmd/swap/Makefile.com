#
#ident	"@(#)Makefile.com	1.1	98/06/17 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/swap/Makefile.com

PROG=	swap
OBJS=	$(PROG).o
SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS +=	-v
CPPFLAGS +=	-D_LARGEFILE64_SOURCE
CFLAGS64 +=	-v

GROUP=sys
FILEMODE=02555

CLEANFILES += $(OBJS)

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@
	$(POST_PROCESS)

lint:	lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ
