#
#ident	"@(#)Makefile	1.7	96/05/22 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# uts/srmmu/vm/Makefile
#
# Note that this srmmu specific file knows the machines which have this
# style of mmu. It would be better if this were not the case, but this will
# be fixed by the generic kernel project so short term improvements are
# not worth it.
#
# include global definitions
include ../../../Makefile.master

HDRS=		hat_srmmu.h mach_page.h

ROOTDIR=	$(ROOT)/usr/share/src
ROOTDIRS=	$(ROOTDIR)/uts \
		$(ROOTDIR)/uts/sun4m $(ROOTDIR)/uts/sun4m/vm \
		$(ROOTDIR)/uts/sun4d $(ROOTDIR)/uts/sun4d/vm

ROOTHDRS=	$(HDRS:%=$(ROOTDIR)/uts/sun4m/vm/%) \
		$(HDRS:%=$(ROOTDIR)/uts/sun4d/vm/%)

CHECKHDRS=	$(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/uts/sun4m/vm/%: %
	$(INS.file)

$(ROOTDIR)/uts/sun4d/vm/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
