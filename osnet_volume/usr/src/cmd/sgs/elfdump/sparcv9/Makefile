#
#ident	"@(#)Makefile	1.4	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

include		../Makefile.com

.KEEP_STATE:

LLDFLAGS =	$(LLDFLAGS64)
LDDBGFLAG=	$(LDDBGFLAG64)
CONVFLAG=	$(CONVFLAG64)
LINTFLAGS64 +=	-D__sparcv9

ROOTCCSBINPROG= $(ROOTCCSBINPROG64)

include		../Makefile.targ
include		../../Makefile.sub.64

package :=	INTLLIB=

install \
package:	$(ROOTCCSBINPROG64)

