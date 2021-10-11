#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

include ../../../Makefile.cmd

HDRS = mdb_modapi.h
ROOTHDRDIR = $(ROOT)/usr/include/sys
ROOTHDRS = $(HDRS:%=$(ROOTHDRDIR)/%)

$(ROOTHDRS) := FILEMODE= 0644

$(ROOTHDRDIR)/%.h: %.h
	$(INS.file)

.KEEP_STATE:
.SUFFIXES:

install_h: $(ROOTHDRS)
