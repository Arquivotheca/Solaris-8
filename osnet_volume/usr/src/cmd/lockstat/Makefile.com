#
# Copyright (c) 1997,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	99/03/30 SMI"
#

PROG= lockstat
OBJS= lockstat.o sym.o
SRCS= $(OBJS:%.o=../%.c)

include ../../Makefile.cmd

LDLIBS += -lelf -lkstat
CFLAGS += -v
CFLAGS64 += -v

FILEMODE= 0555
GROUP= bin

CLEANFILES += $(OBJS)

.KEEP_STATE:

all: $(PROG)

$(PROG):	$(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

clean:
	-$(RM) $(CLEANFILES)

lint:	lint_SRCS

%.o:    ../%.c
	$(COMPILE.c) $<

include ../../Makefile.targ
