#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

SRCS = bits.c tables.c ia32dis.c kvm_ia32dep.c proc_isadep.c

%.o: ../../mdb/%.c
	$(COMPILE.c) $<

%.ln: ../../mdb/%.c
	$(LINT.c) -c $<

include ../../../../Makefile.cmd
include ../../Makefile.ia32
include ../../../Makefile.mdb

CPPFLAGS += -I../../mdb

install: all $(ISAEXEC) $(ROOTPROG32)
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)
