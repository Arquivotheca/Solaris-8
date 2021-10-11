#
#ident	"@(#)Makefile	1.6	93/03/29 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# uts/srmmu/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=	mmu.h		pte.h

ROOTDIR=	$(ROOT)/usr/share/src
ROOTDIRS=	$(ROOTDIR)/uts \
		$(ROOTDIR)/uts/sun4m $(ROOTDIR)/uts/sun4m/sys \
		$(ROOTDIR)/uts/sun4d $(ROOTDIR)/uts/sun4d/sys

ROOTHDRS=	$(HDRS:%=$(ROOTDIR)/uts/sun4m/sys/%) \
		$(HDRS:%=$(ROOTDIR)/uts/sun4d/sys/%)

CHECKHDRS=	$(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/uts/sun4m/sys/%: %
	$(INS.file)

$(ROOTDIR)/uts/sun4d/sys/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
