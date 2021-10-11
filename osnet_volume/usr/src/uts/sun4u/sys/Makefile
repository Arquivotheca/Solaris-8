#
#ident	"@(#)Makefile	1.44	99/10/03 SMI"
#
# Copyright (c) 1991-1996, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sun4u/sys/Makefile
#
UTSBASE	= ../..

#
# include global definitions
#
include ../Makefile.sun4u

#
# Override defaults.
#
FILEMODE	 = 644
GROUP = bin

HDRS=	\
	clock.h		cpc_ultra.h	\
	cpr_impl.h	cpu_sgnblk_defs.h \
	ddi_subrdefs.h	\
	dvma.h		eeprom.h	envctrl.h	\
	envctrl_gen.h	envctrl_ue250.h	envctrl_ue450.h	\
	idprom.h	intr.h		intreg.h	\
	iocache.h	iommu.h		ivintr.h	machcpuvar.h	\
	machparam.h	machsystm.h	machthread.h	memlist_plat.h  \
	memnode.h	nexusdebug.h	nexusintr_impl.h		\
	prom_debug.h	prom_plat.h	scb.h		\
	spitasi.h	spitregs.h	\
	starfire.h	sysioerr.h	sysiosbus.h	\
	tod.h		todmostek.h	vis.h		vm_machparam.h	\
	xc_impl.h	x_call.h	zsmach.h

MONHDRS=
#MONHDRS=	eeprom.h    idprom.h    keyboard.h  password.h

SFMMUHDRS=	mmu.h	pte.h

USR_PSM_MON_DIR=	$(USR_PSM_ISYS_DIR)/mon

ROOTHDRS=		$(HDRS:%=$(USR_PSM_ISYS_DIR)/%)
ROOTMONHDRS=		$(MONHDRS:%=$(USR_PSM_MON_DIR)/%)
ROOTSFMMUHDRS=		$(SFMMUHDRS:%=$(USR_PSM_ISYS_DIR)/%)

ROOTDIR=		$(ROOT)/usr/share/src
ROOTDIRS=		$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=		$(ROOTDIR)/uts/$(PLATFORM)/sys
LINKDEST=		../../../../platform/$(PLATFORM)/include/sys

CHECKHDRS=		$(HDRS:%.h=%.check) \
			$(SFMMUHDRS:%.h=%.check) \
			$(MONHDRS:%.h=mon/%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS) $(ROOTSFMMUHDRS) $(ROOTMONHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTSFMMUHDRS) $(ROOTMONHDRS) $(ROOTLINK)

check:	$(CHECKHDRS)

#
# install rules
#
$(USR_PSM_MON_DIR):	$(USR_PSM_ISYS_DIR)
	$(INS.dir.root.bin)

$(USR_PSM_ISYS_DIR)/%:	../../sfmmu/sys/% $(USR_PSM_ISYS_DIR)
	$(INS.file)

$(USR_PSM_MON_DIR)/%:	mon/% $(USR_PSM_MON_DIR)
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir.root.bin)

# -r because this used to be a directory and is now a link.
$(ROOTLINK):	$(ROOTDIRS)
	-$(RM) -r $@; $(SYMLINK) $(LINKDEST) $@ $(CHOWNLINK) $(CHGRPLINK)

mon/%.check:	mon/%.h
	$(DOT_H_CHECK)

%.check:	../../sfmmu/sys/%.h
	$(DOT_H_CHECK)

FRC:

include ../../Makefile.targ
