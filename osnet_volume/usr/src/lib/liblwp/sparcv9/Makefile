#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/10/14 SMI"
#
# lib/liblwp/sparcv9/Makefile

ASFLAGS += -DGREGSIZE=8 -D__STDC__ -D_ASM -DPIC -K PIC -P -I.

XARCH = -Xarch=v9

MAPDIR=	../spec/sparcv9

include ../Makefile.com
include ../../Makefile.lib.64

SRCS = $(CMNOBJS:%.o=../common/%.c) $(ISAOBJS:%.o=../$(MACH)/%.c)

.KEEP_STATE:

install: all $(ROOTLIBDIR64) .WAIT $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64)
