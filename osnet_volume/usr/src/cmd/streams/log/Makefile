#
#ident	"@(#)Makefile	1.8	99/09/23 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG=	strace strerr strclean

include ../../Makefile.cmd

CFLAGS += -v

FILEMODE= 0555
OWNER= root
GROUP= sys

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTBIN)/strace
	-$(RM) $(ROOTBIN)/strerr
	-$(RM) $(ROOTBIN)/strclean
	-$(SYMLINK) ../sbin/strace $(ROOTBIN)/strace
	-$(SYMLINK) ../sbin/strerr $(ROOTBIN)/strerr
	-$(SYMLINK) ../sbin/strclean $(ROOTBIN)/strclean

clean:

lint:	lint_PROG

include ../../Makefile.targ
