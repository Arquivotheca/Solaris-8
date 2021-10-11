#
#pragma ident	"@(#)Makefile	1.25	99/09/14 SMI"
#
#	Copyright (c) 1999 by Sun Microsystems, Inc.
#	All rights reserved.

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. a.out support).

P_COMOBJS=	analyze.o	elf.o		globals.o	malloc.o \
		mutex.o		paths.o		setup.o		util.o \
		dlfcns.o	config_elf.o	locale.o	profile.o \
		tsort.o		getcwd.o	remove.o	sunwmove.o

S_COMOBJS=	debug.o		object.o	audit.o


G_MACHOBJS=	doreloc.o

P_MACHOBJS=	sparc_elf.o	_setup.o

CP_MACHOBJS=	common_sparc.o

S_MACHOBJS=	

P_ASOBJS=	boot.o		boot_elf.o	caller.o	zero.o

S_ASOBJS=	note.o

CRTI=		pics/crti.o
CRTN=		pics/crtn.o
CRTS=		$(CRTI)		$(CRTN)

CHKSRCS=	$(SRCBASE)/uts/common/krtld/reloc.h

PLAT=sparc

include		$(SRC)/cmd/sgs/rtld/Makefile.com
include		$(SRC)/lib/Makefile.lib.64

CPPFLAGS +=	-I../../../../uts/sparc/krtld


SGSMSGTARG +=	$(SGSMSGSPARC) $(SGSMSGSPARC64) $(SGSMSG64)


# Add any machine specific flags.

CFLAGS +=	-xregs=no%appl
ASFLAGS +=	-s -K pic -D__sparcv9 -D_ELF64 $(sparcv9_XARCH)
#ADBGENFLAGS +=	-erroff=%all
CPPFLAGS +=	-D_ELF64
LINTFLAGS64 +=	-D__sparcv9

DYNFLAGS=	-i -h /usr/lib/sparcv9/ld.so.1 -z text $(ZDEFS)
DYNFLAGS +=	-e _rt_boot -Bsymbolic -zcombreloc -M $(MAPFILE) -zlazyload \
		'-R$$ORIGIN'

LIBPROTOS =	-L ../../../../../../proto/root_sparc/usr/lib/sparcv9
CPICLIB =	$(LIBPROTOS) -L $(ROOT)/usr/lib/pics/$(MACH64)
LDLIB =		-L ../../libld/$(MACH64)
DBGLIB =	-L ../../liblddbg/$(MACH64)
RTLDLIB =	-L ../../librtld/$(MACH64)
CONVLIB =	-L ../../libconv/$(MACH64)


.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB64)

lint:		$(LINTOUT64)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

pics/%.o:	../sparc/%.c
		$(COMPILE.c) -o $@ $< $(WARNFLAGS)
		$(POST_PROCESS_O)

pics/%.o:	$(SRCBASE)/uts/sparc/krtld/%.c
		$(COMPILE.c) -o $@ $<  $(WARNFLAGS)
		$(POST_PROCESS_O)


include		$(SRC)/cmd/sgs/rtld/Makefile.targ
include		../../Makefile.sub.64
