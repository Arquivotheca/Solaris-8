#
#ident	"@(#)Makefile	1.6	94/08/25 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= mvdir

SRCS= $(PROG:%=%.sh)

include ../Makefile.cmd

FILEMODE= 0555
OWNER= root

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	-$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:

include ../Makefile.targ
