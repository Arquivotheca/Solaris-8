#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

MODULE = unix.so
MDBTGT = kvm

MODSRCS = ttrace.c unix.c

include ../../../../../Makefile.cmd
include ../../../../sparc/Makefile.sparcv7

ROOTMOD = $(ROOT)/usr/platform/sun4m/lib/mdb/$(MDBTGT)

$(ROOT)/usr/platform/sun4m/lib/mdb:
	$(INS.dir)

$(ROOTMOD): $(ROOT)/usr/platform/sun4m/lib/mdb

$(ROOTMOD)/%: %
	$(INS.file) 

include ../../../../Makefile.module

%.o: ../../../../sparc/v7/unix/%.c
	$(COMPILE.c) $<

%.ln: ../../../../sparc/v7/unix/%.c
	$(LINT.c) -c $<

CPPFLAGS += -DMP -D_MACHDEP
CPPFLAGS += -I../../../../sparc/v7/unix
CPPFLAGS += -I../../../../common
CPPFLAGS += -I$(SRC)/uts/sparc
CPPFLAGS += -I$(SRC)/uts/sparc/v7
CPPFLAGS += -I$(SRC)/uts/sun4m
