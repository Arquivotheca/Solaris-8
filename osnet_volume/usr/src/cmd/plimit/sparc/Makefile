#
#ident	"@(#)Makefile	1.1	98/07/13 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/plimit/sparc/Makefile
#

PROG=	plimit

OBJS=	plimit.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS	+= -v

LDLIBS	+= -lproc

lint	:= LINTFLAGS = -x

CPPFLAGS += -D_LARGEFILE64_SOURCE=1

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG32)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.targ
