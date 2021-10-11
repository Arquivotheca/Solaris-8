#
#ident	"@(#)Makefile	1.23	99/10/04 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/kadb/sparc/sun4d/Makefile
#
# a kernel-architecture-specific kadb Makefile
#

# override global macros as necessary
TARGET_ARCH =	sun4d
ARCH =		sun4d
KARCH =		sun4d
MACH=		sparc
CSMACH=		sparc
KSUN=   	sun
MMU=		srmmu
ARCHVERS=	v7
PROMTYPE=	obp
PROMLIB=	libprom.a
CSPROM=		$(PROMTYPE)

ARCHOBJ= $(SPECIAL) machdep.o map.o physmem.o invoke.o

# include architecture specific kadb definitions
include ../common/Makefile.sparc

# include common kadb definitions and initial targets
include ../../common/Makefile.com

$(ARCHOBJ): assym.s 

assym.s: genassym 
	./genassym > $@ 

genassym: genassym.c
	(unset LD_LIBRARY_PATH; \
	$(NATIVECC) $(CFLAGS) $(CPPINCS) -o $@ genassym.c)
