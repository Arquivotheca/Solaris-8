#
#ident	"@(#)Makefile	1.2	99/10/19 SMI"
#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libprtdiag/sparc/sun4u/Makefile
#
# Platform specific Makefile for libprtdiag.
#
# PLATFORM is the target for the binary installation.
#
SRCDIR		= ../..
PLATFORM	= sun4u

include		$(SRCDIR)/Makefile.com

OBJECTS		= io.o cpu.o memory.o kstat.o prom.o \
			pdevinfo_funcs.o display_funcs.o \
			pdevinfo_sun4u.o display_sun4u.o

SRCS		= $(OBJECTS:%.o=../../common/%.c)

CFLAGS	+= -K pic

.KEEP_STATE:

all:		$(LIBS)

install:	all $(USR_PSM_LIBS)

lint:		$(LINTLIB)

include		$(SRCDIR)/Makefile.targ

