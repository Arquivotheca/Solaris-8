#
#ident	"@(#)Makefile	1.9	96/04/20 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= more
DATA= more.help

include ../Makefile.cmd

LDLIBS += -lcmd -lcurses

ROOTLIBDATA= $(DATA:%=$(ROOTLIB)/%)
CPPFLAGS += -D_FILE_OFFSET_BITS=64

$(ROOTLIBDATA) :=	FILEMODE =	0644

CFLAGS += -v

.KEEP_STATE:

all: $(PROG) $(DATA) 

install: all $(ROOTPROG) $(ROOTLIBDATA)
	$(RM) $(ROOTBIN)/page
	$(LN) $(ROOTPROG) $(ROOTBIN)/page

clean:

lint:	lint_PROG

include ../Makefile.targ
