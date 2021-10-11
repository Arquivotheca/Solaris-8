#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/11 SMI"

SRCS = disasm.c sparcdis.c kvm_v9dep.c proc_isadep.c

%.o: ../../mdb/%.c
	$(COMPILE.c) $<

%.ln: ../../mdb/%.c
	$(LINT.c) -c $<

include ../../../../Makefile.cmd
include ../../../../Makefile.cmd.64
include ../../Makefile.sparcv9
include ../../../Makefile.mdb

CPPFLAGS += -I../../mdb

install: all $(ISAEXEC) $(ROOTPROG64)
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)
