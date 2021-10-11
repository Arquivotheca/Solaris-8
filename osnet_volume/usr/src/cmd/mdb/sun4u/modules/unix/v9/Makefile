#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

MODULE = unix.so
MDBTGT = kvm

MODSRCS = unix.c

include ../../../../../Makefile.cmd
include ../../../../../Makefile.cmd.64
include ../../../../sparc/Makefile.sparcv9

ROOTMOD = $(ROOT)/usr/platform/sun4u/lib/mdb/$(MDBTGT)/sparcv9

$(ROOT)/usr/platform/sun4u/lib/mdb:
	$(INS.dir)

$(ROOT)/usr/platform/sun4u/lib/mdb/$(MDBTGT): $(ROOT)/usr/platform/sun4u/lib/mdb
	$(INS.dir)

$(ROOTMOD): $(ROOT)/usr/platform/sun4u/lib/mdb/$(MDBTGT)

$(ROOTMOD)/%: % 
	$(INS.file)

include ../../../../Makefile.module

CPPFLAGS += -DMP -D_MACHDEP
CPPFLAGS += -I../../../../common
CPPFLAGS += -I$(SRC)/uts/sun4u
CPPFLAGS += -I$(SRC)/uts/sfmmu
CPPFLAGS += -I$(SRC)/uts/sparc/v9
