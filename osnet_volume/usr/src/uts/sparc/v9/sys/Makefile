#
# Copyright (c) 1996, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.11	99/08/14 SMI"
#
# uts/sparc/v9/sys/Makefile
#
# include global definitions
include ../../../../Makefile.master

HDRS=	\
	asi.h		\
	atomic_prim.h	\
	machpcb.h	\
	machtrap.h	\
	membar.h	\
	mutex_impl.h	\
	privregs.h	\
	prom_isa.h	\
	psr_compat.h	\
	traptrace.h	\
	vis_simulator.h

ROOTINCV9= $(ROOT)/usr/include/v9
ROOTDIR= $(ROOTINCV9)/sys
ROOTDIRS= $(ROOTDIR)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTINCV9) .WAIT $(ROOTDIRS) .WAIT $(ROOTHDRS) 

$(ROOTINCV9):
	$(INS.dir)

$(ROOTDIRS):	$(ROOTINCV9)
	$(INS.dir)

check:	$(CHECKHDRS)
