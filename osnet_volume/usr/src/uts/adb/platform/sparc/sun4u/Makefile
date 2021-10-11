#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.14	99/10/07 SMI"
#
# uts/adb/platform/sparc/sun4u/Makefile
#

ADB_BASE_DIR =	../../..
ADB_PLAT_DIR =	../..
COMMONDIR =	$(ADB_BASE_DIR)/common
ISADIR =	$(ADB_BASE_DIR)/sparc

KARCH =		sun4u
PLATFORM =	$(KARCH)
ARCHOPTS =	-D__sparcv9cpu
MODULE =	adb

LINKED_PLATFORMS	= SUNW,Ultra-1
LINKED_PLATFORMS	+= SUNW,Ultra-4
LINKED_PLATFORMS	+= SUNW,Ultra-250
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise-10000

LINKED_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%)
LINKED_LIB_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib)
LINKED_ADB_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/adb)

INCLUDES =	-I${SYSDIR}/${KARCH} -I${SYSDIR}/srmmu \
		-I${SYSDIR}/${MACH} -I${SYSDIR}/${MACH}/v9 -I${SYSDIR}/sun

SRCS =		mutex.dbg regs.dbg rwlock.dbg sema.dbg machpcb.dbg \
                adaptive_mutex.dbg spin_mutex.dbg

DUMMIES= ctx

include $(ADB_PLAT_DIR)/Makefile.psm

.KEEP_STATE:

.PARALLEL:	$(SCRIPTS) $(DUMMIES)

all: $(MACTMP) .WAIT $(SCRIPTS) $(DUMMIES)

links:	$(LINKED_DIRS) $(LINKED_LIB_DIRS) $(LINKED_ADB_DIRS)

install: all $(USR_PSM_ADB_DIR) links .WAIT $(ROOTSCRIPTS) $(ROOTDUMMIES)

include $(ADB_PLAT_DIR)/Makefile.targ
