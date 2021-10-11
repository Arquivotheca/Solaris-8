#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.6	99/01/25 SMI"
#
# lib/libaio/sparcv9/Makefile

MAPDIR=	../spec/sparcv9

MOBJS=sys.o
MDIR=.

include ../Makefile.com
include ../../Makefile.lib.64

CPPFLAGS += -Dsparc -I../../libc/inc -I../../libc/sparcv9/inc
ASDEFS=	-D__STDC__ -D_ASM $(CPPFLAGS)
ASFLAGS= -P $(ASDEFS) $(sparcv9_XARCH) -D__sparcv9

BUILD.s=$(AS)	$(ASFLAGS) $< -o $@

MAPFILES=       $(MAPFILES64)
MAPOPTS=        $(MAPFILES:%=-M %)

DYNFLAGS += $(MAPOPTS)

.KEEP_STATE:

install: all $(ROOTLIBS64) $(ROOTLINKS64)
