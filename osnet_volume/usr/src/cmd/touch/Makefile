#
#ident	"@(#)Makefile	1.10	96/04/20 SMI"
#
# Copyright (c) 1989,1996, by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= touch

include ../Makefile.cmd

LDLIBS += -lcmd
CFLAGS += -v
XGETFLAGS += -a -x touch.xcl
CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)
	-$(RM) $(ROOTBIN)/settime
	$(LN) -f $(ROOTPROG) $(ROOTBIN)/settime

clean:

lint:	lint_PROG

include ../Makefile.targ
