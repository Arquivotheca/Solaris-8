#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sparc/ia64/sys/Makefile
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
	trap.h

ROOTINCIA64= $(ROOT)/usr/include/ia64
ROOTDIR= $(ROOTINCIA64)/sys
ROOTDIRS= $(ROOTDIR)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTINCIA64) .WAIT $(ROOTDIRS) .WAIT $(ROOTHDRS) 

$(ROOTINCIA64):
	$(INS.dir)

$(ROOTDIRS):	$(ROOTINCIA64)
	$(INS.dir)

check:	$(CHECKHDRS)
