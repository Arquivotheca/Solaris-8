#
#ident	"@(#)Makefile	1.2	98/08/28 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

include ../Makefile.com

.KEEP_STATE:

LINTFLAGS64 +=	-D__sparcv9

all:		$(DYNLIB)
install:	all $(ROOTDYNLIB64)

include ../Makefile.targ
include ../../Makefile.sub.64
