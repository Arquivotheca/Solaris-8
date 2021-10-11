#
#pragma ident	"@(#)Makefile.com	1.20	99/10/12 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

LIBRARY=	libconv.a

COMOBJS_MSG=	arch.o			data.o \
		deftag.o		dl.o \
		dynamic.o 		config.o \
		elf.o			globals.o \
		phdr.o			relocate_i386.o \
		relocate_sparc.o	relocate_ia64.o \
		sections.o		segments.o \
		symbols.o		symbols_sparc.o \
		version.o

COMOBJS_NOMSG=	relocate.o		tokens.o

OBJECTS=	$(COMOBJS_MSG) $(COMOBJS_NOMSG)

include 	$(SRC)/lib/Makefile.lib
include 	$(SRC)/cmd/sgs/Makefile.com

PICS=		$(OBJECTS:%=pics/%)

CPPFLAGS=	-I. -I../common -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys $(CPPFLAGS.master)
ARFLAGS=	cr

BLTDATA=	$(COMOBJS_MSG:%.o=%_msg.h)

SRCS=		../common/llib-lconv
LINTSRCS=	$(OBJECTS:%.o=../common/%.c) ../common/lintsup.c

SGSMSGTARG=	$(COMOBJS_MSG:%.o=../common/%.msg)

LINTFLAGS =	-mu $(CPPFLAGS)
LINTFLAGS64 =	-mu -errchk=longptr64 $(CPPFLAGS)

CLEANFILES +=	$(BLTDATA) $(LINTOUTS)
CLOBBERFILES +=	$(LINTLIB)
