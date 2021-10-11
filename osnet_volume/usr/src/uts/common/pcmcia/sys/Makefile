#
#ident	"@(#)Makefile	1.7	96/05/23 SMI"
#
# Copyright (c) 1995-1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/pcmcia/sys/Makefile
#
# include global definitions
include ../../../../Makefile.master

HDRS=	cis.h			\
	cis_handlers.h		\
	cis_protos.h		\
	cs.h			\
	cs_priv.h		\
	cs_strings.h		\
	cs_types.h		\
	cs_stubs.h

ROOTDIR=	$(ROOT)/usr/include/sys
ROOTHDRS=	$(HDRS:%=$(ROOTDIR)/%)

# install rules

$(ROOTDIR)/%: %
	$(INS.file)

CHECKHDRS= $(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIR) .WAIT $(ROOTHDRS)

$(ROOTDIR):
	$(INS.dir)

check:	$(CHECKHDRS)
