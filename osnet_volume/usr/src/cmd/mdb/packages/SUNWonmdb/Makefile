#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

include ../../../Makefile.cmd
include ../Makefile.pkg

.KEEP_STATE:

ROOTOPTPKGBIN = $(ROOTOPTPKG)/bin

$(ROOTOPTPKG):
	$(INS.dir)

$(ROOTOPTPKGBIN): $(ROOTOPTPKG)
	@cd ../../scripts; pwd; $(MAKE) install

install: all $(ROOTOPTPKGBIN) $(ROOTOPTPKG)/SUNWonmdb-README .WAIT pkg
