#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

include		../Makefile.com

.KEEP_STATE:

LLDFLAGS=	$(LLDFLAGS64)
CONVFLAG=	$(CONVFLAG64)
LINTFLAGS64 +=	-D__ia64

ROOTCCSBINPROG=	$(ROOTCCSBINPROG64)
ROOTLINKS=	$(ROOTCCSBIN64)/$(STRIPFILE)

package :=	INTLLIB=

include		../Makefile.targ
include		../../Makefile.sub.64
