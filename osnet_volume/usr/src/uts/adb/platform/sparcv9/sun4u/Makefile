#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.13	99/10/07 SMI"
#
# uts/adb/platform/sparcv9/sun4u/Makefile
#

MACROGENDIR=	../../../macrogen
ADB_BASE_DIR =  ../../..
ADB_PLAT_DIR =  ../..
COMMONDIR =     $(ADB_BASE_DIR)/common

KARCH =         sun4u
PLATFORM =      $(KARCH)
ARCHOPTS =      -D__sparcv9cpu
MODULE =	adb

LINKED_PLATFORMS	= SUNW,Ultra-1
LINKED_PLATFORMS	+= SUNW,Ultra-4
LINKED_PLATFORMS	+= SUNW,Ultra-250
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise-10000

LINKED_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%)
LINKED_LIB_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib)
LINKED_ADB_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/adb)

INCLUDES =      -I${SYSDIR}/${KARCH} -I${SYSDIR}/srmmu \
                -I${SYSDIR}/${MACH} -I${SYSDIR}/${MACH}/v9 -I${SYSDIR}/sun

SRCS= adaptive_mutex.dbg machpcb.dbg mutex.dbg regs.dbg \
	rwlock.dbg sema.dbg spin_mutex.dbg
DUMMIES= ctx

include $(ADB_PLAT_DIR)/Makefile.psm

# v9 includes and defines
#
include $(SRC)/Makefile.master.64
ALWAYS_DEFS	+= -D__sparc -D__sparcv9 -D__sparcv9cpu
ALWAYS_DEFS	+= -D_SYSCALL32_IMPL -D_ELF64
CFLAGS64	+= -Wc,-xcode=abs32 $(INLINES)
CCMODE		= -Xa
NATIVEDEFS += -D$(MACH64) -D__$(MACH64)

ISADIR =        $(ADB_BASE_DIR)/sparcv9

.KEEP_STATE:

.PARALLEL:      $(SCRIPTS) $(DUMMIES)

all: $(MACTMP) .WAIT $(SCRIPTS) $(DUMMIES)

links:	$(LINKED_DIRS) $(LINKED_LIB_DIRS) $(LINKED_ADB_DIRS)

install: all $(USR_PSM_ADB_DIR_64) links .WAIT $(ROOTSCRIPTS64) $(ROOTDUMMIES64)

include $(ADB_PLAT_DIR)/Makefile.targ
