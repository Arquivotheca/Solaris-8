#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile 1.24	99/09/21 SMI"
#
# ucblib/libucb/sparc/Makefile

SYSOBJS=	 	\
	setjmp.o	\
	signal.o	\
	syscall.o

MAPDIR=	../spec/sparc
include ../Makefile.com

DYNFLAGS += $(DYNFLAGS32)
BUILD.s=	$(AS) $(ASFLAGS) $< -o $@

# sparc-specific rule
objs/%.o pics/%.o: sys/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

.KEEP_STATE:

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
