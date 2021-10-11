#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/10/14 SMI"
#
# lib/liblwp/sparc/Makefile

ASFLAGS += -DGREGSIZE=4 -D__STDC__ -D_ASM -DPIC -K PIC -P -I.

MAPDIR=	../spec/sparc

include ../Makefile.com

SRCS = $(CMNOBJS:%.o=../common/%.c) $(ISAOBJS:%.o=../$(MACH)/%.c)

.KEEP_STATE:

install: all $(ROOTLIBDIR) .WAIT $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
