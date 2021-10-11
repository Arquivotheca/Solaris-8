#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.41	99/05/04 SMI"
#
# uts/sparc/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=  \
	archsystm.h	asm_linkage.h	cpu.h		\
	ddi_isa.h	frame.h		fsr.h		inline.h	\
	machelf.h	\
	machlock.h	machsig.h	machtypes.h	old_procfs.h	\
	pcb.h		\
	procfs_isa.h	psw.h		reg.h		regset.h	\
	spl.h		stack.h		stat_impl.h			\
	synch32.h	trap.h		ucontext.h      vmparam.h	\
	sysconfig_impl.h

DBGHDRS=   debug.h

FPUHDRS=   fpu_simulator.h	fpusystm.h	globals.h	ieee.h

ROOTDIR=	$(ROOT)/usr/include/sys
ROOTDIRS=	$(ROOTDIR) $(ROOTDIR)/debug $(ROOTDIR)/fpu

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%)
ROOTDBGHDRS= $(DBGHDRS:%=$(ROOTDIR)/debug/%)
ROOTFPUHDRS= $(FPUHDRS:%=$(ROOTDIR)/fpu/%)

debug/%.check:	debug/%.h
	$(DOT_H_CHECK)

fpu/%.check:	fpu/%.h
	$(DOT_H_CHECK)

CHECKHDRS= $(HDRS:%.h=%.check) \
	$(DBGHDRS:%.h=debug/%.check) \
	$(FPUHDRS:%.h=fpu/%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

$(ROOTDIR)/debug/%: debug/%
	$(INS.file)

$(ROOTDIR)/fpu/%: fpu/%
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS) $(ROOTDBGHDRS) $(ROOTFPUHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTDBGHDRS) $(ROOTFPUHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
