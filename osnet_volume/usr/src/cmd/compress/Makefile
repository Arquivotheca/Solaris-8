#
#ident	"@(#)Makefile	1.8	96/04/20 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= compress

include ../Makefile.cmd

.KEEP_STATE:

CFLAGS += -v
CPPFLAGS += -D_FILE_OFFSET_BITS=64

all: $(PROG) 

install: all $(ROOTPROG)
	$(RM) $(ROOTBIN)/uncompress $(ROOTBIN)/zcat
	$(LN) $(ROOTPROG) $(ROOTBIN)/uncompress
	$(LN) $(ROOTPROG) $(ROOTBIN)/zcat

clean:

lint:	lint_PROG

include ../Makefile.targ
