#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

include ../../Makefile.cmd

SCRIPTS = mdb mkmodules mkroot
ROOTOPTPKGBIN = $(ROOT)/opt/SUNWonmdb/bin
ROOTSCRIPTS = $(SCRIPTS:%=$(ROOTOPTPKGBIN)/%)

all clean.lint clean clobber dmods install_h lint:

install pkg: $(ROOTSCRIPTS)

$(ROOTOPTPKGBIN):
	$(INS.dir)

$(ROOTSCRIPTS): $(ROOTOPTPKGBIN)

$(ROOTOPTPKGBIN)/%: %.sh
	$(INS.rename)
