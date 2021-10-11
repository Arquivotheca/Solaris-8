#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

MODULE = unix.so
MDBTGT = kvm

MODSRCS = unix.c

include ../../../../../Makefile.cmd
include ../../../../intel/Makefile.ia32

ROOTMOD = $(ROOT)/usr/platform/i86pc/lib/mdb/$(MDBTGT)

$(ROOT)/usr/platform/i86pc/lib/mdb:
	$(INS.dir)

$(ROOTMOD): $(ROOT)/usr/platform/i86pc/lib/mdb

$(ROOTMOD)/%: %
	$(INS.file) 

include ../../../../Makefile.module

CPPFLAGS += -DMP -D_MACHDEP
CPPFLAGS += -I../../../../common
CPPFLAGS += -I$(SRC)/uts/intel
CPPFLAGS += -I$(SRC)/uts/i86pc
