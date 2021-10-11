#
#pragma ident	"@(#)Makefile	1.11	99/05/04 SMI"
#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/platform/i386/i86pc/Makefile
#

ADB_PLAT_DIR =	../..

KARCH =		i86pc
PLATFORM =	$(KARCH)

INCLUDES =	-I${SYSDIR}/${KARCH} -I${SYSDIR}/intel \
		-I${SYSDIR}/${MACH} -I${SYSDIR}/sun

SRCS =	regs.dbg rwlock.dbg sema.dbg \
	mutex.dbg adaptive_mutex.dbg spin_mutex.dbg

DUMMIES= ctx

include $(ADB_PLAT_DIR)/Makefile.psm

.KEEP_STATE:

.PARALLEL:	$(SCRIPTS) $(DUMMIES)

all: $(MACTMP) .WAIT $(SCRIPTS) $(DUMMIES)

install: all $(USR_PSM_ADB_DIR) .WAIT $(ROOTSCRIPTS) $(ROOTDUMMIES)

include $(ADB_PLAT_DIR)/Makefile.targ
