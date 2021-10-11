#
#ident	"@(#)Makefile	1.1	99/10/03 SMI"
#
# Copyright (c) 1991-1996, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sun4u/sunfire/sys/Makefile
#
UTSBASE	= ../../..

#
# include global definitions
#
include ../Makefile.sunfire

#
# Override defaults.
#
FILEMODE	 = 644
GROUP = bin

HDRS=	\
	ac.h environ.h fhc.h simmstat.h sram.h sysctrl.h

ROOTHDRS=		$(HDRS:%=$(USR_SUNFIRE_ISYS_DIR)/%)

ROOTDIR=		$(ROOT)/usr/share/src
ROOTDIRS=		$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=		$(ROOTDIR)/uts/$(PLATFORM)/sys
LINKDEST=		../../../../platform/$(PLATFORM)/include/sys

CHECKHDRS=		$(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTLINK)

check:	$(CHECKHDRS)

#
# install rules
#
$(USR_SUNFIRE_ISYS_DIR)/%:	% $(USR_SUNFIRE_ISYS_DIR)
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir.root.bin)

# -r because this used to be a directory and is now a link.
$(ROOTLINK):	$(ROOTDIRS)
	-$(RM) -r $@; $(SYMLINK) $(LINKDEST) $@ $(CHOWNLINK) $(CHGRPLINK)

FRC:

include ../Makefile.targ
