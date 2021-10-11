#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	99/08/24 SMI"
#
# lib/libc_psr/sparcv9/spec/sun4u/Makefile

.KEEP_STATE:

LIBRARY	=	libc_psr.a
VERS	=	.1

PLATFORM=	sun4u
CLASS=		64

OBJECTS	=	c_psr-$(PLATFORM).o

SPECVERS=	-$(PLATFORM)

include	$(SRC)/lib/Makefile.lib
include	$(SRC)/lib/Makefile.lib.64
include $(SRC)/lib/libc_psr/spec/Makefile.com

# Uncomment the following if the linker complains
#sparcv9_C_PICFLAGS  = -K PIC

include	$(SRC)/lib/Makefile.spec
include $(SRC)/lib/Makefile.spec.psm

install: links $(USR_PSM_ABILIB64)
