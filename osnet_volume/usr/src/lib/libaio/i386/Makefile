#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.22	99/01/25 SMI"
#
#
# libaio/i386/Makefile

MAPDIR=	../spec/i386

MOBJS=	sys.o
MDIR=.

include ../Makefile.com

CPPFLAGS += -I../../libc/inc
ASDEFS=	-D__STDC__ -D_ASM $(CPPFLAGS)
ASFLAGS= -P $(ASDEFS)

BUILD.s=$(AS)	$(ASFLAGS) $< -o $@

.KEEP_STATE:

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
