#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	99/09/21 SMI"
#
# ucblib/libucb/sparcv9/Makefile

SYSOBJS=	 	\
	setjmp.o	\
	signal.o

MAPDIR=	../spec/sparcv9

include ../Makefile.com
include $(SRC)/lib/Makefile.lib.64

DYNFLAGS += $(DYNFLAGS64)
BUILD.SO= $(CC) $(CFLAGS) -o $@ -G $(DYNFLAGS) $(PICS) -L $(ROOT)/usr/lib/$(MACH64) $(LDLIBS)
BUILD.s=	$(AS) $(ASFLAGS) -xarch=v9 -D__sparcv9 $< -o $@

# sparcv9-specific rule
objs/%.o pics/%.o: ../sparc/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: sys/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

.KEEP_STATE:

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64)
