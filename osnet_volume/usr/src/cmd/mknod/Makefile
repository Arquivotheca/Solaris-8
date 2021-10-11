#
#ident	"@(#)Makefile	1.7	97/08/12 SMI"
#
# Copyright (c) 1989-1997 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= mknod 

include ../Makefile.cmd

CFLAGS += -v

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
