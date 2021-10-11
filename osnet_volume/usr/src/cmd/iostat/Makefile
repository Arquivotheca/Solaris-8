#
# Copyright (c) 1992-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.9	99/08/10 SMI"
#

PROG=	iostat
OBJS=	iostat.o dsr.o mnt.o nfs.o
SRCS=	$(OBJS:.o=.c)

include ../Makefile.cmd

LDLIBS += -lkstat -ldevinfo -lrt
LINTFLAGS += -u
FILEMODE= 0555

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

$(PROG):	$(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)
	
clean:
	-$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../Makefile.targ
