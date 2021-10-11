#
#ident	"@(#)Makefile	1.7	96/10/15 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#

TOPDIR =	../../../../..
PLATFORM =	sun4d
ARCHVERS =	v7
ARCHMMU  =	srmmu
PROMVERS =	obp
ARCHOPTS =

PLAT_C_SRC =	machdep.c
ARCH_C_SRC =	sun4x_memlist.c sun4x_standalloc.c
ARCH_S_SRC =	sparcv7_subr.s
SRT0_S =	sun4x_srt0.s
INLINES =

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
