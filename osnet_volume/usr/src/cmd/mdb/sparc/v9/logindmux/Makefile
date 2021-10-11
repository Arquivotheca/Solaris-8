#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

MODULE = logindmux.so
MDBTGT = kvm

MODSRCS = logindmux.c

include ../../../../Makefile.cmd
include ../../../../Makefile.cmd.64
include ../../Makefile.sparcv9
include ../../../Makefile.module

$(ROOTMOD): $(ROOT)/usr/lib/mdb/$(MDBTGT)

$(ROOT)/usr/lib/mdb/$(MDBTGT): $(ROOT)/usr/lib/mdb
	$(INS.dir)
