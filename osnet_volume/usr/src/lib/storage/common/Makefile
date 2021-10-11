#
#pragma ident	"@(#)Makefile	1.6	99/07/29 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/common/Makefile
#

include ../Makefile.storage

# definitions for install_h target
HDRS=           stgcom.h

ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: ./common/hdrs/%
	$(INS.file)

install_h: $(ROOTHDRDIR)

check: $(CHECKHDRS)

.KEEP_STATE:
 
all debug install: install_h

clean clobber lint: 

include ../Makefile.targ
