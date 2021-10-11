#
#ident	"@(#)Makefile	1.1	99/09/27 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/inet/nca/Makefile
#
# include global definitions
include ../../../../Makefile.master

HDRS= bean.h nca.h ncadoorhdr.h ncakmem.h ncalogd.h ncandd.h osvers.h

ROOTDIRS= $(ROOT)/usr/include/inet/nca

ROOTHDRS= $(HDRS:%=$(ROOT)/usr/include/inet/nca/%)

CHECKHDRS= $(HDRS:%.h=%.check)

$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
