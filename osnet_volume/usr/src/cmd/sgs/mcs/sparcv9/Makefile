#
#ident	"@(#)Makefile	1.2	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.

include		../Makefile.com

.KEEP_STATE:

LLDFLAGS=	$(LLDFLAGS64)
CONVFLAG=	$(CONVFLAG64)
LINTFLAGS64 +=	-D__sparcv9

ROOTCCSBINPROG=	$(ROOTCCSBINPROG64)
ROOTLINKS=	$(ROOTCCSBIN64)/$(STRIPFILE)

package :=	INTLLIB=

include		../Makefile.targ
include		../../Makefile.sub.64
