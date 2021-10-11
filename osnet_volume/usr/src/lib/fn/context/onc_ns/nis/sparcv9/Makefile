# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.5 97/11/10 SMI"
#
# lib/fn/context/onc_ns/nis/sparcv9/Makefile
#

FNRPATH=	/usr/lib/fn/sparcv9
FNSPLIB= 	-L$(FNLIBDIR64) -lfnsp

include ../Makefile.com
include ../../../../../Makefile.lib.64

.KEEP_STATE:

BUILDCCC.SO += $(CCFLAGS)

debug install tcov: $(ROOTLIBDIR64) $(ROOTLIBS64) $(ROOTFNLINKS64)
