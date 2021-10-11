#
#pragma ident	"@(#)Makefile	1.37	99/10/22 SMI"
#
# Copyright (c) 1991-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sun4m/sys/Makefile
#
UTSBASE	= ../..

#
# include global definitions
#
include ../Makefile.sun4m

#
# Override defaults.
#
FILEMODE	 = 644
GROUP = bin

HDRS=	\
	async.h		auxio.h		bcopy_if.h	bt.h		\
	clock.h		cpr_impl.h			\
	ddi_subrdefs.h	devaddr.h	eeprom.h	enable.h	\
	hat_sx.h	idprom.h	immu.h		intreg.h	\
	iommu.h		machcpuvar.h	machparam.h	\
	machsystm.h	machthread.h	memctl.h	memerr.h	\
	memlist_plat.h \
	micio.h		micreg.h	micvar.h	module.h	\
	module_ross625.h physaddr.h	pmcio.h		pmcreg.h	\
	pmcvar.h	prom_plat.h	scb.h		sx_cmemio.h	\
	sxio.h		sxreg.h		vm_machparam.h  x_call.h	\
	xc_levels.h	zsmach.h

MONHDRS=	password.h

SRMMUHDRS=	mmu.h	pte.h

USR_PSM_MON_DIR=	$(USR_PSM_ISYS_DIR)/mon

ROOTHDRS=		$(HDRS:%=$(USR_PSM_ISYS_DIR)/%)
ROOTMONHDRS=		$(MONHDRS:%=$(USR_PSM_MON_DIR)/%)
ROOTSRMMUHDRS=		$(SRMMUHDRS:%=$(USR_PSM_ISYS_DIR)/%)

ROOTDIR=		$(ROOT)/usr/share/src
ROOTDIRS=		$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=		$(ROOTDIR)/uts/$(PLATFORM)/sys
LINKDEST=		../../../../platform/$(PLATFORM)/include/sys

CHECKHDRS=		$(HDRS:%.h=%.check) \
			$(SRMMUHDRS:%.h=%.check) \
			$(MONHDRS:%.h=mon/%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS) $(ROOTSRMMUHDRS) $(ROOTMONHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTSRMMUHDRS) $(ROOTMONHDRS) $(ROOTLINK)

check:	$(CHECKHDRS)

#
# install rules
#
$(USR_PSM_MON_DIR):	$(USR_PSM_ISYS_DIR)
	$(INS.dir.root.bin)

$(USR_PSM_ISYS_DIR)/%:	../../srmmu/sys/% $(USR_PSM_ISYS_DIR)
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

%.check:	../../srmmu/sys/%.h
	$(DOT_H_CHECK)

FRC:

include ../../Makefile.targ
