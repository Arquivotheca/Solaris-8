#
#ident	"@(#)Makefile	1.1	94/02/16 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# uts/common/pcmcia/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=

ROOTDIRS=	$(ROOT)/usr/include/sys

ROOTHDRS=	$(HDRS:%=$(ROOTDIRS)/%)

# install rules
$(ROOTDIRS)/%: %
	$(INS.file)

CHECKHDRS= $(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
