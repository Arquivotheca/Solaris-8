#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#
#pragma ident "@(#)Makefile	1.2 97/11/07 SMI"
#
# lib/fn/context/x500/sparcv9/Makefile
#

LDFLAGS += -L$(FNLIBDIR64)

include ../Makefile.com
include ../../../../Makefile.lib.64

FNRPATH	=	/usr/lib/fn/sparcv9
 
.KEEP_STATE:

BUILDCCC.SO += $(CCFLAGS)

debug install tcov: all $(ROOTLIBDIR64) $(ROOTETC) $(ROOTETCFN) \
			$(ROOTLIBS64) $(ROOTETCFNFILES)
