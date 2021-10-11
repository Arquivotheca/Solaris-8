#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/09/23 SMI"

MODULE = ipc.so
MDBTGT = kvm

MODSRCS = ipc.c

include ../../../../Makefile.cmd
include ../../../../Makefile.cmd.64
include ../../Makefile.sparcv9
include ../../../Makefile.module

$(ROOTMOD): $(ROOT)/usr/lib/mdb/$(MDBTGT)

$(ROOT)/usr/lib/mdb/$(MDBTGT): $(ROOT)/usr/lib/mdb
	$(INS.dir)
