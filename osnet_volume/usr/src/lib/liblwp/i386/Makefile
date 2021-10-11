#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/10/14 SMI"
#
# lib/liblwp/i386/Makefile

ASFLAGS += -D__STDC__ -D_ASM -DPIC -P -I.

MAPDIR=	../spec/i386

include ../Makefile.com

SRCS = $(CMNOBJS:%.o=../common/%.c) $(ISAOBJS:%.o=../$(MACH)/%.c)

.KEEP_STATE:

install: all $(ROOTLIBDIR) .WAIT $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
