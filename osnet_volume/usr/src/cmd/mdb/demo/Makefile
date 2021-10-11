#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/11/19 SMI"

#
# Note that this Makefile is *not* the Makefile that is installed in the proto
# area's /usr/demo/mdb directory -- that file is Makefile.demo.  This Makefile
# copies Makefile.demo and the other files in this directory to /usr/demo/mdb.
#

include ../../Makefile.cmd

DEMOFILES = \
	common/example1.c \
	common/example2.c \
	Makefile \
	Makefile.common \
	Makefile.i386 \
	Makefile.sparc \
	Makefile.sparcv9 \
	README

DEMOSUBDIRS = common i386 sparc sparcv9

ROOTDEMODIR = $(ROOT)/usr/demo/mdb
ROOTDEMOFILES = $(DEMOFILES:%=$(ROOTDEMODIR)/%)
ROOTDEMOSUBDIRS = $(DEMOSUBDIRS:%=$(ROOTDEMODIR)/%)

all clean.lint clean clobber dmods install_h lint:

install: $(ROOTDEMOFILES)

$(ROOTDEMODIR):
	$(INS.dir)

$(ROOTDEMOSUBDIRS):
	$(INS.dir)

$(ROOTDEMODIR)/%: %.demo
	$(INS.rename)

$(ROOTDEMODIR)/%: %
	$(INS.file)

$(ROOTDEMOFILES): $(ROOTDEMOSUBDIRS)

$(ROOTDEMOSUBDIRS): $(ROOTDEMODIR)

$(ROOTDEMODIR) := OWNER = root
$(ROOTDEMODIR) := GROUP = bin
$(ROOTDEMODIR) := DIRMODE = 755

$(ROOTDEMOSUBDIRS) := OWNER = root
$(ROOTDEMOSUBDIRS) := GROUP = bin
$(ROOTDEMOSUBDIRS) := DIRMODE = 755

$(ROOTDEMOFILES) := OWNER = root
$(ROOTDEMOFILES) := GROUP = bin
$(ROOTDEMOFILES) := FILEMODE = 644
