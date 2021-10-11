#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	99/01/25 SMI"
#

MAPDIR=	../spec/sparcv9

include ../Makefile.com
include ../../../Makefile.lib.64

.KEEP_STATE:

BUILDCCC.SO += $(CCFLAGS)

install debug tcov: $(ROOTLIBDIR64) \
      $(ROOTLIBS64) $(ROOTLINKSCCC64)
