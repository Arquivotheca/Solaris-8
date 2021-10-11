#
# Copyright (c) 1992,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.7	99/03/30 SMI"
#

PROG= vmstat
OBJS= vmstat.o

include ../Makefile.cmd

LDLIBS += -lkstat
CFLAGS += -v
FILEMODE= 0555
GROUP= bin

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

$(PROG):	$(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)
	
clean:
	-$(RM) $(OBJS)

lint:
	$(LINT.c) $(PROG).c $(LDLIBS)

include ../Makefile.targ
