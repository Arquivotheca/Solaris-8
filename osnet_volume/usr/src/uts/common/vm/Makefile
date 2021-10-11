#
# Copyright (c) 1989-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.19	98/07/21 SMI"
#
# include global definitions
include ../../../Makefile.master

HDRS=	anon.h as.h faultcode.h hat.h page.h pvn.h rm.h seg.h vpage.h \
	seg_dev.h seg_enum.h seg_kmem.h seg_kp.h seg_map.h seg_vn.h seg_spt.h

ROOTDIRS= $(ROOT)/usr/include/vm

ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

# install rule
$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
