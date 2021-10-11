#
#pragma ident	"@(#)Makefile	1.28	99/09/14 SMI"
#
#	Copyright (c) 1999 by Sun Microsystems, Inc.
#	All rights reserved.

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. profiling support).

P_COMOBJS=	analyze.o	elf.o		globals.o	malloc.o \
		mutex.o		paths.o		setup.o		util.o \
		dlfcns.o	config_elf.o	locale.o	profile.o \
		tsort.o		getcwd.o	remove.o	sunwmove.o

S_COMOBJS=	debug.o		object.o	audit.o

G_MACHOBJS=	doreloc.o

P_MACHOBJS=	i386_elf.o	_setup.o	zero.o

S_MACHOBJS=

P_ASOBJS=	boot.o		boot_elf.o	caller.o

S_ASOBJS=	note.o

PLAT=		intel/ia32

include		$(SRC)/cmd/sgs/rtld/Makefile.com

CPPFLAGS +=	-I../../../../uts/intel/ia32/krtld

SGSMSGTARG +=	$(SGSMSGINTEL) $(SGSMSGINTEL32) $(SGSMSG32)

.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB)

lint:		$(LINTOUT32)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

pics/%.o:       $(SRCBASE)/uts/intel/ia32/krtld/%.c
		$(COMPILE.c) -o $@ $<  $(WARNFLAGS)
		$(POST_PROCESS_O)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
