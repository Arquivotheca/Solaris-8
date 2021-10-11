#
#ident	"@(#)Makefile	1.4	95/02/14 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# uts/sfmmu/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=		mmu.h		pte.h

ROOTDIR=	$(ROOT)/usr/share/src
ROOTDIRS=	$(ROOTDIR)/uts \
		$(ROOTDIR)/uts/sun4u $(ROOTDIR)/uts/sun4u/sys

ROOTHDRS=	$(HDRS:%=$(ROOTDIR)/uts/sun4u/sys/%)

CHECKHDRS=	$(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/uts/sun4u/sys/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
