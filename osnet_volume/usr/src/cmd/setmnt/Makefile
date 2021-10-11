#
#ident	"@(#)Makefile	1.9	99/05/24 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

SHFILES=	setmnt
ALL=		$(SHFILES)
CLOBBERFILES=	$(ALL)
PROG=		setmnt

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

install: all  $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	-$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

include ../Makefile.targ
