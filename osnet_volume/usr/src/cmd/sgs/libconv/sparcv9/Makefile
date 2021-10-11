#
# Copyright (c) 1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	98/08/28 SMI"
#
# cmd/sgs/libconv/sparcv9/Makefile

include 	../Makefile.com

LINTFLAGS64 +=	-D__sparcv9

.PARALLEL:	$(PICS)
.KEEP_STATE:

install all:	$(LIBRARY) .WAIT

include 	../Makefile.targ
include		../../Makefile.sub.64
