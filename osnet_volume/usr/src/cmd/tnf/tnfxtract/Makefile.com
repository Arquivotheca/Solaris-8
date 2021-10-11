#
# ident	"@(#)Makefile.com 1.1 97/08/06 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/tnf/tnfxtract/Makefile
#

PROG=		tnfxtract

OBJS.c=		tnfxtract.o

OBJS=		$(OBJS.c)

SRCS= $(OBJS.c:%.o=../%.c)

include	../../../Makefile.cmd

LFLAGS=		-v
LDLIBS +=	-lkvm -lelf

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

include	../../../Makefile.targ
