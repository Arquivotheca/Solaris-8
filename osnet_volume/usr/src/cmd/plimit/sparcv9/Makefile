#
#ident	"@(#)Makefile	1.1	98/07/13 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/plimit/sparcv9/Makefile
#

PROG=	plimit

OBJS=	plimit.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd
include ../../Makefile.cmd.64

CFLAGS64	+= -v

LDLIBS	+= -lproc

lint	:= LINTFLAGS64 = -x -D__sparcv9

CPPFLAGS += -D_LARGEFILE64_SOURCE=1

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG64)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.targ
