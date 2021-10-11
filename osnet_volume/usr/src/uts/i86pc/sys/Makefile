#
#pragma ident	"@(#)Makefile	1.36	99/05/04 SMI"
#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/i86pc/sys/Makefile
#
UTSBASE = ../..

#
# include global definitions
#
include ../Makefile.i86pc

#
# Override defaults.
#
FILEMODE = 644
OWNER = root
GROUP = bin

HDRS=  \
	asm_misc.h	\
	clock.h		\
	cram.h		\
	ddi_subrdefs.h	\
	eisarom.h	\
	machcpuvar.h	\
	machparam.h	\
	machsystm.h	\
	machthread.h	\
	mcdma.h		\
	nvm.h		\
	psm.h		\
	psm_defs.h	\
	psm_modctl.h	\
	psm_types.h	\
	rm_platter.h	\
	smp_impldefs.h	\
	vm_machparam.h	\
	x_call.h	\
	xc_levels.h	\
	xque.h

ROOTHDRS=	$(HDRS:%=$(USR_PSM_ISYS_DIR)/%)

ROOTDIR=	$(ROOT)/usr/share/src
ROOTDIRS=	$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=	$(ROOTDIR)/uts/$(PLATFORM)/sys
LINKDEST=	../../../../platform/$(PLATFORM)/include/sys

CHECKHDRS=	$(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL:	$(CHECKHDRS) $(ROOTHDRS)

install_h:	$(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTLINK)

check:		$(CHECKHDRS)

$(ROOTDIRS):
	$(INS.dir.root.bin)

$(ROOTLINK):	$(ROOTDIRS)
	-$(RM) -r $@; $(SYMLINK) $(LINKDEST) $@ $(CHOWNLINK) $(CHGRPLINK)

FRC:

include ../../Makefile.targ
