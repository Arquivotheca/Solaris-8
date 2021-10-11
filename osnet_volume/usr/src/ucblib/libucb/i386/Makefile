#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile 1.20	99/09/21 SMI"
#
# ucblib/libucb/i386/Makefile

SYSOBJS=	 	\
	_setjmp.o	\
	setjmp.o	\
	signal.o	\
	syscall.o	\
	longjmp.o	\
	_times.o	\
	ucbcerror.o

MAPDIR=	../spec/i386

include ../Makefile.com

DYNFLAGS += $(DYNFLAGS32)
ASOBJDEFS=      -m m4.def sys.def ansi.def nonpic.def -DMCOUNT=/
ASPICDEFS=      -m -DDSHLIB m4.def sys.def ansi.def pic.def -DMCOUNT=/

BUILD_OBJS.s=  $(AS) $(ASOBJDEFS) $< && mv $(@F) $@
BUILD_PICS.s=  $(AS) $(ASPICDEFS) $< && mv $(@F) $@

# i386-specific rule
objs/%.o: sys/%.s
	$(BUILD_OBJS.s)

pics/%.o: sys/%.s
	$(BUILD_PICS.s)

.KEEP_STATE:

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
