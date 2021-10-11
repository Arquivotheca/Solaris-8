#
#pragma ident	"@(#)Makefile	1.19	96/04/20 SMI"
#
# Copyright (c) 1989-1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= cpio

OBJS= cpio.o cpiostat.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd
CPPFLAGS += -D_FILE_OFFSET_BITS=64

LDLIBS += -lmapmalloc -lsec
XGETFLAGS += -a -x cpio.xcl

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
