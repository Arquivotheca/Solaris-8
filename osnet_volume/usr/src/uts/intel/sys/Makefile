#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.68	00/09/14 SMI"
#
# uts/intel/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

# NOTE: hrtcntl.h and hrtsys.h are present in this directory so that the
#	hrtsys system call can be built to facilitate transportability of
#	stock SVr4 programs.  Every effort is to be made to prevent objects
#	from being built, so these headers are not exported (installed).

HDRS	=			\
	archsystm.h		\
	asm_linkage.h		\
	bootconf.h		\
	bootsvcs.h		\
	cpu.h			\
	ddi_isa.h		\
	debugreg.h		\
	dnet.h			\
	emap.h			\
	fp.h			\
	frame.h			\
	ia64_archext.h		\
	ieef.h			\
	immu.h			\
	inline.h		\
	kd.h			\
	machelf.h		\
	machlock.h		\
	machsig.h		\
	machtypes.h		\
	mii.h			\
	miipriv.h		\
	miiregs.h		\
	mmu.h			\
	mouse.h			\
	mutex_impl.h		\
	obpdefs.h		\
	old_procfs.h		\
	pcb.h			\
	procfs_isa.h		\
	prom_emul.h		\
	prom_isa.h		\
	prom_plat.h		\
	promif.h		\
	promimpl.h		\
	psw.h			\
	pte.h			\
	reg.h			\
	regset.h		\
	sbpro.h			\
	segment.h		\
	spl.h			\
	stack.h			\
	stat_impl.h		\
	synch32.h		\
	sysconfig_impl.h	\
	sysi86.h		\
	sysia64.h		\
	trap.h			\
	traptrace.h		\
	tss.h			\
	ucontext.h		\
	vmparam.h		\
	x86_archext.h

HDRS += elx.h

SUBHDRS	=			\
	debug/debug.h		\
	debug/debugger.h	\
	dktp/cdtypes.h		\
	dktp/dpt/dpt.h		\
	dktp/dpt/dptioctl.h	\
	dktp/dpt/dptsig.h	\
	dktp/mlx/debug.h	\
	dktp/mlx/mlx.h		\
	dktp/mlx/mlx_dac.h	\
	dktp/mlx/mlx_dacioc.h	\
	dktp/mlx/mlx_raid.h	\
	dktp/mlx/mlx_scsi.h	\
	dktp/mlx/mlxdefs.h	\
	dktp/mlx/mlxops.h	\
	dktp/mscsi.h		\
	dktp/scdk.h		\
	dktp/scdkwatch.h	\
	dktp/sctarget.h		\
	dktp/tgcd.h		\
	dktp/tgpassthru.h	\
	fs/seg_xmem.h		\
	fs/xmem.h		\
	scsi/targets/sddef.h

ROOTDIR=	$(ROOT)/usr/include/sys
SCSIDIR=	$(ROOTDIR)/scsi
SCSIDIRS=	$(SCSIDIR) $(SCSIDIR)/conf $(SCSIDIR)/generic \
		$(SCSIDIR)/impl $(SCSIDIR)/targets
DKTPDIR=	$(ROOTDIR)/dktp
DKTPDIRS=	$(DKTPDIR) $(DKTPDIR)/dpt $(DKTPDIR)/mlx
ROOTDIRS=	$(ROOTDIR) $(SCSIDIRS) $(ROOTDIR)/debug $(DKTPDIRS)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%) $(SUBHDRS:%=$(ROOTDIR)/%)

CHECKHDRS= $(HDRS:%.h=%.check) $(SUBHDRS:%.h=%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)

