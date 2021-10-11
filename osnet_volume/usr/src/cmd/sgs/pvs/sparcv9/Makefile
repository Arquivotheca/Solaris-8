#
#ident	"@(#)Makefile	1.2	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

include		../Makefile.com

.KEEP_STATE:

LLDFLAGS =	$(LLDFLAGS64)
CONVFLAG=	$(CONVFLAG64)
LINTFLAGS64 +=	-D__sparcv9

ROOTPROG=$(ROOTPROG64)

package :=	INTLLIB=

include		../Makefile.targ
include		../../Makefile.sub.64
