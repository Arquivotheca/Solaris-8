#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.1	99/01/25 SMI"
#
# cmd/abi/Makefile.com

.KEEP_STATE:

i386_ARCH=	i86
sparc_ARCH=	sparc

ARCH=$($(MACH)_ARCH)

ROOTLIBABI=	$(ROOT)/usr/lib/abi
ROOTLIBABI64=	$(ROOT)/usr/lib/abi/sparcv9

ABIHOME=	$(SRC)/cmd/abi
DAA=		$(ABIHOME)/daa2x/daa2x
#DAAFLAGS=	-b $(ABIHOME)/daa2x/truss.so.1 -n
