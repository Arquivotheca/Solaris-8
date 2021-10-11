#
#ident	"@(#)Makefile	1.15	99/10/19 SMI"
#
# Copyright (c) 1995, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/prtdiag/sparc/sun4u/Makefile
#
# Platform specific Makefile for the prtdiag command.
#
# PLATFORM is the target for the binary installation.
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.

SRCDIR		= ../..
PLATFORM	= sun4u
PLATLINKS	=

include $(SRCDIR)/Makefile.com

CFLAGS		+= -R /usr/platform/$(PLATFORM)/lib

LDLIBS		+= -L $(ROOT)/usr/platform/$(PLATFORM)/lib
LDLIBS		+= -lkstat -lprtdiag
LINTFLAGS	+= -u

.KEEP_STATE:

all:	libprtdiag $(PROG)

libprtdiag:
	cd $(SRCDIR)/../../lib/libprtdiag ; make install

include $(SRCDIR)/Makefile.targ
