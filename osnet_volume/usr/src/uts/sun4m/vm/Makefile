#
#ident	"@(#)Makefile	1.7	99/03/30 SMI"
#
# Copyright (c) 1993,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sun4m/vm/Makefile
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

HDRS=	mach_srmmu.h hat_srmmu.h mach_page.h

ROOTHDRS=		$(HDRS:%=$(USR_PSM_IVM_DIR)/%)

ROOTDIR=		$(ROOT)/usr/share/src
ROOTDIRS=		$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=		$(ROOTDIR)/uts/$(PLATFORM)/vm
LINKDEST=		../../../../platform/$(PLATFORM)/include/vm

CHECKHDRS=		$(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTLINK)

check:	$(CHECKHDRS)

#
# install rules
#
$(USR_PSM_IVM_DIR)/%: ../../srmmu/vm/% $(USR_PSM_IVM_DIR)
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir.root.bin)

# -r because this used to be a directory and is now a link.
$(ROOTLINK):	$(ROOTDIRS)
	-$(RM) -r $@; $(SYMLINK) $(LINKDEST) $@ $(CHOWNLINK) $(CHGRPLINK)

%.check:	../../srmmu/vm/%.h
	$(DOT_H_CHECK)

CLEANFILES=

CLOBBERFILES=

CLEANLINTFILES=

include ../../Makefile.targ
