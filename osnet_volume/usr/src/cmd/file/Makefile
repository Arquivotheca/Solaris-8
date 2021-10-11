#
#ident	"@(#)Makefile	1.11	97/07/10 SMI"
#
# Copyright (c) 1989,1996,1997 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= file
MAGIC= magic

include ../Makefile.cmd

LDLIBS += -lcmd -lelf
CPPFLAGS += -I../../lib/libcmd/inc

ROOTETCMAGIC= $(MAGIC:%=$(ROOTETC)/%)

$(ROOTETCMAGIC) :=	FILEMODE =	$(LIBFILEMODE)

.KEEP_STATE:

all: $(PROG) $(MAGIC) 

install: all $(ROOTPROG) $(ROOTETCMAGIC)

clean:

lint: lint_PROG

include ../Makefile.targ
