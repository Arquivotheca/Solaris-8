#
#ident	"@(#)Makefile	1.15	99/10/04 SMI"
#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/platform/sparc/sun4d/Makefile
#

ADB_BASE_DIR =	../../..
ADB_PLAT_DIR =	../..
COMMONDIR =	$(ADB_BASE_DIR)/common
ISADIR =	$(ADB_BASE_DIR)/sparc

KARCH =		sun4d
PLATFORM =	$(KARCH)

INCLUDES =	-I${SYSDIR}/${KARCH} -I${SYSDIR}/srmmu \
		-I${SYSDIR}/${MACH} -I${SYSDIR}/${MACH}/v7 -I${SYSDIR}/sun

SRCS =		ctx.dbg \
		mutex.dbg regs.dbg rwlock.dbg sema.dbg machpcb.dbg \
		srmmu.dbg ptbl.dbg srhme.dbg hme.dbg \
                adaptive_mutex.dbg spin_mutex.dbg

include $(ADB_PLAT_DIR)/Makefile.psm

.KEEP_STATE:

.PARALLEL:	$(SCRIPTS) $(DUMMIES)

all: $(MACTMP) .WAIT $(SCRIPTS) $(DUMMIES)

install: all $(USR_PSM_ADB_DIR) .WAIT $(ROOTSCRIPTS) $(ROOTDUMMIES)

include $(ADB_PLAT_DIR)/Makefile.targ
