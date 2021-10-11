#
#pragma ident	"@(#)Makefile	1.2	99/09/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5_psr/sparc/spec/sun4u/Makefile

.KEEP_STATE:

LIBRARY	=	libmd5_psr.a
VERS	=	.1

PLATFORM=	sun4u
CLASS=		32

OBJECTS	=	md5_psr-$(PLATFORM).o

SPECVERS =	-$(PLATFORM)

include	$(SRC)/lib/Makefile.lib

# Uncomment the following if the linker complains
#sparc_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec
include $(SRC)/lib/Makefile.spec.psm
include $(SRC)/lib/libmd5_psr/spec/Makefile.com

install: links $(USR_PSM_ABILIB)
