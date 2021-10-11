#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.8	99/04/13 SMI"
#
# psm/stand/kadb/sun4u/Makefile
#
# a kernel-architecture-specific kadb Makefile
#

# override global macros as necessary
TARGET_ARCH =	sun4u
ARCH =		sun4u
KARCH =		sun4u
MACH=		sparc
CSMACH=		sparc
KSUN= 		sun
MMU=		sfmmu
ARCHVERS =	v9
PROMTYPE=	ieee1275
PROMLIB=	libprom.a
PLATLIB=        $(PLATDIR)/libplat.a
CSPROM=		$(PROMTYPE)/common $(PROMTYPE)/$(KARCH)
ARCHOPTS=	-D__sparcv9cpu

ARCHOBJ= $(SPECIAL) machdep.o watchpt.o physmem.o invoke.o

# include architecture specific kadb definitions
include ../common/Makefile.sparc
# include common kadb definitions and initial targets
include ../../common/Makefile.com

#
# so we can use privileged V9 instructions
#

ASFLAGS	+=	-xarch=v8plus

$(ARCHOBJ): assym.s

assym.s: genassym 
	./genassym > $@

genassym: genassym.c
	(unset LD_LIBRARY_PATH; \
	$(NATIVECC) $(CFLAGS) $(CPPINCS) -o $@ genassym.c)
