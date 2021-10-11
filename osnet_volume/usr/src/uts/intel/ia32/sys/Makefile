#
#pragma ident	"@(#)Makefile	1.3	99/06/05 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sparc/ia32/sys/Makefile
#
# include global definitions
include ../../../../Makefile.master

HDRS=  \
	asm_linkage.h	\
	machtypes.h	\
	mmu.h		\
	psw.h		\
	pte.h		\
	reg.h		\
	stack.h		\
	trap.h		\
	traptrace.h

FSHDRS=	\
	xmem.h		\
	seg_xmem.h

ROOTINCIA32= $(ROOT)/usr/include/ia32
ROOTDIR= $(ROOTINCIA32)/sys
ROOTFSDIR= $(ROOTDIR)/fs
ROOTDIRS= $(ROOTDIR) $(ROOTFSDIR)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%)
ROOTFSHDRS= $(FSHDRS:%=$(ROOTDIR)/fs/%)

CHECKHDRS=				\
	$(HDRS:%.h=%.check)		\
	$(FSHDRS:%.h=fs/%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS) $(ROOTFSHDRS)

install_h: $(ROOTINCIA32) .WAIT $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTFSHDRS)

$(ROOTINCIA32):
	$(INS.dir)

$(ROOTDIRS):	$(ROOTINCIA32)
	$(INS.dir)

check:	$(CHECKHDRS)
