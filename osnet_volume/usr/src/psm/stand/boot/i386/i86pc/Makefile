#
#ident	"@(#)Makefile	1.5	96/05/17 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#

TOPDIR =	../../../../..
PLATFORM =	i86pc
KARCH =		$(PLATFORM)
ARCHVERS =
PROMVERS =	boot
ARCHOPTS =	-Di386 -D__i386 -DBOOTI386

PLAT_C_SRC =	machdep.c
ARCH_C_SRC =
ARCH_S_SRC =	cpu_id.s cache.s endsrt0.s idttab.s intr.s
SRT0_S =	srt0.s
INLINES =	i86.il

#
# The following libraries are build in LIBPLAT_DIR
#
LIBPLAT_DIR =
LIBPLAT_LIBS =
LIBPLAT_L_LIBS = $(LIBPLAT_LIBS:lib%.a=llib-l%.ln)

#
# Platform specific libraries
#
PSMLIBS +=      $(LIBPLAT_LIBS:lib%.a=-l%)
PSMLIB_DIRS +=  $(LIBPLAT_DIR)


include ../Makefile.com
