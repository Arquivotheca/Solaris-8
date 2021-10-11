#
#pragma ident	"@(#)Makefile	1.11	99/12/06 SMI"
#
# Copyright (c) 1989,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/des/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=	des.h		desdata.h	softdes.h

ROOTDIRS= $(ROOT)/usr/include/des

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
