#
#ident	"@(#)Makefile	1.13	99/04/28 SMI"
#
# Copyright (c) 1991, 1995 by Sun Microsystems, Inc.
#
# cmd/prtdiag/sparc/sun4d/Makefile
#
# Platform specific Makefile for the prtdiag command.
#
# PLATFORM is the target for the binary installation.
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.

SRCDIR		= ../..
PLATFORM	= sun4d
PLATLINKS	=
KARCHOBJS	= pdevinfo_sun4d.o display_sun4d.o pdevinfo.o display.o anlyzerr.o

include $(SRCDIR)/Makefile.com

.KEEP_STATE:

all:	$(PROG)

include $(SRCDIR)/Makefile.targ
