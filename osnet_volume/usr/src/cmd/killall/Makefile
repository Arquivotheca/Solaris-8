#
#ident	"@(#)Makefile	1.7	94/08/25 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= killall 

include ../Makefile.cmd

FILEMODE=555
.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
