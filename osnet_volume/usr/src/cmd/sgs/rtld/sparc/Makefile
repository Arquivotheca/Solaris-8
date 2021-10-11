#
#pragma ident	"@(#)Makefile	1.59	99/09/14 SMI"
#
#	Copyright (c) 1999 by Sun Microsystems, Inc.
#	All rights reserved.

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. a.out support).

P_COMOBJS=	analyze.o	elf.o		globals.o	malloc.o \
		mutex.o		paths.o		setup.o		util.o \
		dlfcns.o	config_elf.o	locale.o	profile.o \
		remove.o	tsort.o		getcwd.o	sunwmove.o

S_COMOBJS=	debug.o		object.o	audit.o \
		cache_a.out.o	a.out.o

G_MACHOBJS=	doreloc.o

P_MACHOBJS=	sparc_elf.o	_setup.o

CP_MACHOBJS=	common_sparc.o

S_MACHOBJS=	sparc_a.out.o

P_ASOBJS=	boot.o		boot_elf.o	caller.o	zero.o

S_ASOBJS=	boot_a.out.o	note.o

CRTI=		pics/crti.o
CRTN=		pics/crtn.o
CRTS=		$(CRTI)		$(CRTN)

CHKSRCS=	$(SRCBASE)/uts/common/krtld/reloc.h

PLAT=		sparc

include		$(SRC)/cmd/sgs/rtld/Makefile.com

CPPFLAGS += -I../../../../uts/sparc/krtld

# Add any a.out and machine specific flags.

CPPFLAGS +=	-DA_OUT
CFLAGS +=	-xregs=no%appl
ASFLAGS +=	-K pic
LINTFLAGS +=	-DA_OUT
ADBGENFLAGS +=	-erroff=%all

SGSMSGTARG +=	$(SGSMSGSPARC) $(SGSMSGSPARC32) $(SGSMSG32)

.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB)

lint:		$(LINTOUT32)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
