#
#pragma ident	"@(#)Makefile	1.22	99/01/25 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# libaio/sparc/Makefile

MAPDIR=	../spec/sparc

MOBJS=		sys.o
MDIR=.

include ../Makefile.com

CPPFLAGS += -Dsparc -I../../libc/inc -I../../libc/sparc/inc
ASDEFS=	-D__STDC__ -D_ASM $(CPPFLAGS)
ASFLAGS= -P $(ASDEFS)

BUILD.s=$(AS)	$(ASFLAGS) $< -o $@

MAPFILES= $(MAPFILES32)
MAPOPTS= $(MAPFILES:%=-M %)

DYNFLAGS += $(MAPOPTS)

.KEEP_STATE:

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
