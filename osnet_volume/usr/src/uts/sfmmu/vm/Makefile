#
#ident	"@(#)Makefile	1.3	96/05/22 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# uts/sfmmu/vm/Makefile
#
# Note that this sfmmu specific file knows the machines which have this
# style of mmu. It would be better if this were not the case, but this will
# be fixed by the generic kernel project so short term improvements are
# not worth it.
#
# include global definitions
include ../../../Makefile.master

HDRS=		hat_sfmmu.h mach_page.h

ROOTDIR=	$(ROOT)/usr/share/src
ROOTDIRS=	$(ROOTDIR)/uts \
		$(ROOTDIR)/uts/sun4u $(ROOTDIR)/uts/sun4u/vm

ROOTHDRS=	$(HDRS:%=$(ROOTDIR)/uts/sun4u/vm/%)

CHECKHDRS=	$(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/uts/sun4u/vm/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
