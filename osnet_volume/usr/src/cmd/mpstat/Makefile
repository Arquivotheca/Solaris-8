#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/03/30 SMI"
#

PROG= mpstat
OBJS= mpstat.o

include ../Makefile.cmd

LINTFLAGS = -mux
LDLIBS += -lkstat
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

lint:	lint_PROG

include ../Makefile.targ
