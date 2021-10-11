#
#ident	"@(#)Makefile	1.3	98/08/28 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

DUPOBJS=	stab64.o

include ../Makefile.com

.KEEP_STATE:

all:		$(DYNLIB)
install:	all $(ROOTDYNLIB)
lint:		$(LINTOUT64)

DYNFLAGS +=	-M mapfile-sparc

include ../Makefile.targ

