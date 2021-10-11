#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/fn/context/x500/ia64/Makefile
#

LDFLAGS += -L$(FNLIBDIR64)

include ../Makefile.com
include ../../../../Makefile.lib.64

FNRPATH	=	/usr/lib/fn/ia64
 
.KEEP_STATE:

BUILDCCC.SO += $(CCFLAGS)

debug install tcov: all $(ROOTLIBDIR64) $(ROOTETC) $(ROOTETCFN) \
			$(ROOTLIBS64) $(ROOTETCFNFILES)
