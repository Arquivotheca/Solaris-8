#
#ident	"@(#)Makefile	1.1	98/07/26 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ps/i386/Makefile
#

PROG=	ps

OBJS=	ps.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS	+= -v

lint	:= LINTFLAGS = -x

FILEMODE= 04555
OWNER= root
GROUP= sys

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
