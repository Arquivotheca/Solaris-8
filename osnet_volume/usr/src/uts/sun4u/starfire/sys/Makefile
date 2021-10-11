#
# ident	"@(#)Makefile	1.10	99/03/30 SMI"
#
# Copyright (c) 1997-1998, by Sun Microsystems, Inc.
# All Rights Reserved.
#
UTSBASE	= ../../..

#
# include global definitions
#
include ../Makefile.starfire

#
# Override defaults.
#
FILEMODE	 = 644
GROUP = bin

HDRS=	cvc.h dr.h sfdr.h

ROOTHDRS=	$(HDRS:%=$(USR_STARFIRE_ISYS_DIR)/%)

ROOTDIR=	$(ROOT)/usr/share/src
ROOTDIRS=	$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=	$(ROOTDIR)/uts/$(PLATFORM)/sys
LINKDEST=	../../../../platform/$(PLATFORM)/include/sys

CHECKHDRS=		$(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTLINK)

check:	$(CHECKHDRS)

#
# install rules
#
$(USR_STARFIRE_ISYS_DIR)/%:	% $(USR_STARFIRE_ISYS_DIR)
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir.root.bin)

# -r because this used to be a directory and is now a link.
$(ROOTLINK):	$(ROOTDIRS)
	-$(RM) -r $@; $(SYMLINK) $(LINKDEST) $@ $(CHOWNLINK) $(CHGRPLINK)

FRC:

include ../Makefile.targ
