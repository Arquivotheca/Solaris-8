#
#ident	"@(#)Makefile	1.1	98/07/30 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/w/sparcv9/Makefile
#

PROG=	w

OBJS=	w.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd
include ../../Makefile.cmd.64

CFLAGS64	+= -v

lint	:= LINTFLAGS64 = -x -D__sparcv9

ROOTLINK= $(ROOTBIN64)/uptime

FILEMODE= 4555
OWNER= root		# w needs to read /proc entries
GROUP= bin

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG64) $(ROOTLINK)

$(ROOTLINK):
	$(RM) $@
	$(LN) $(ROOTPROG64) $@

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.targ
