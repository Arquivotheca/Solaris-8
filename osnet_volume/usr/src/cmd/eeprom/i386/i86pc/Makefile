#
#ident	"@(#)Makefile	1.1	95/02/03 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/eeprom/i386/i86pc/Makefile
#
# Platform specific Makefile for the eeprom command.
#
# PLATFORM is the target for the binary instalation.
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.
#
SRCDIR		= ../..
PLATFORM	= i86pc
PLATLINKS	=

include $(SRCDIR)/Makefile.com

.KEEP_STATE:

all:	$(PROG)

include $(SRCDIR)/Makefile.targ

LDLIBS += -lkvm -lelf -lgen
