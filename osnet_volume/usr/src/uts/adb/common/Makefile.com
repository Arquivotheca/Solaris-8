#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.125	99/11/05 SMI"
#
# uts/adb/common/Makefile.com
#

PROGS		= adbgen adbgen1 adbgen3 adbgen4
OBJS		= adbsub.o
MACROGEN	= macrogen
MGENPP		= mgenpp
MACTMP		= ./.tmp

# NOTE: The following have been at least temporarily removed:
#	dir.adb
#	dir.nxt.adb
#	fpu.adb
#	mount.adb
#	pme.adb
#	stat.adb

SRCS += \
	anon_hdr.dbg		\
	anon_map.dbg		\
	as.dbg			\
	au_buf.dbg		\
	au_queue.dbg		\
	bootobj.dbg		\
	buf.dbg			\
	bufctl.dbg		\
	bufctl_audit.dbg	\
	buflist.dbg		\
	buflist.nxt.dbg		\
	buflistiter.nxt.dbg	\
	cache_label.dbg		\
	cache_usage.dbg		\
	cachefs_workq.dbg	\
	cachefsfsc.dbg		\
	cachefsmeta.dbg		\
	callbparams.dbg		\
	callout.dbg		\
	callout.nxt.dbg		\
	callout_bucket.nxt.dbg	\
	callout_table.dbg	\
	callouts.dbg		\
	calltrace.dbg		\
	calltrace.nxt.dbg	\
	cb_ops.dbg		\
	cg.dbg			\
	cglist.dbg		\
	cglist.nxt.dbg		\
	cglistchk.nxt.dbg	\
	cglistiter.nxt.dbg	\
	cirbuf.dbg		\
	cnode.dbg		\
	cons_polledio.dbg	\
	cpc_ctx.dbg		\
	cpc_event.dbg		\
	cpu.dbg			\
	cpu_dptbl.dbg		\
	cpu_dptbl.nxt.dbg	\
	cpu_stat.dbg		\
	cpu_sysinfo.dbg		\
	cpu_syswait.dbg		\
	cpu_vminfo.dbg		\
	cpun.dbg		\
	cpupart.dbg		\
	cpus.dbg		\
	cpus.nxt.dbg		\
	cred.dbg		\
	csdata.dbg		\
	csmethods.dbg		\
	csum.dbg		\
	cwrd.dbg		\
	cyc_cpu.dbg		\
	cyc_pcbuffer.dbg	\
	cyc_softbuf.dbg		\
	cyclic.dbg		\
	cyclic_id.dbg		\
	dblk.dbg		\
	ddi_dma_attr.dbg	\
	ddi_dma_cookie.dbg	\
	dev_ops.dbg		\
	devinfo.dbg		\
	dino.dbg		\
	direct.dbg		\
	disp.dbg		\
	dispq.dbg		\
	dispq.nxt.dbg		\
	dispqtrace.dbg		\
	dispqtrace.list.dbg	\
	dispqtrace.nxt.dbg	\
	dk_geom.dbg		\
	door.dbg		\
	door_arg.dbg		\
	door_data.dbg		\
	dp_entry.dbg		\
	dqblk.dbg		\
	dquot.dbg		\
	dumphdr.dbg		\
	edge.dbg		\
	entity_attribute.dbg	\
	entity_item.dbg		\
	eucioc.dbg		\
	exdata.dbg		\
	fad.dbg			\
	fdbuffer.dbg		\
	fifonode.dbg		\
	file.dbg		\
	filsys.dbg		\
	findthreads.dbg		\
	findthreads.nxt.dbg	\
	fnnode.dbg		\
	fpollinfo.dbg		\
	fs.dbg			\
	graph.dbg		\
	hash2ints.dbg		\
	hcca.dbg		\
	hc_ed.dbg		\
	hc_gtd.dbg		\
	hcr_regs.dbg		\
	hid_default_pipe.dbg	\
	hid_pipe_reset.dbg	\
	hid_polled_input_callback.dbg	\
	hid_power.dbg		\
	hid_req.dbg		\
	hid_state.dbg		\
	hidparser_tok.dbg	\
	hidpers_hdle.dbg	\
	hmelist.dbg		\
	hub_power.dbg		\
	hubd.dbg		\
	ic_acl.dbg		\
	ifnet.dbg		\
	ifrt.dbg		\
	ill.dbg			\
	in6_addr.dbg		\
	inode.dbg		\
	inodelist.dbg		\
	inodelist.nxt.dbg	\
	inodelistiter.nxt.dbg	\
	iocblk.dbg		\
	iovec.dbg		\
	ipc.dbg			\
	ipc_perm.dbg		\
	ipif.dbg		\
	ire.dbg			\
	itimer.dbg		\
	itimerspec.dbg		\
	itimerval.dbg		\
	iulp.dbg		\
	kmastat.dbg		\
	kmastat.nxt.dbg		\
	kmem_cache.dbg		\
	kmem_cpu.dbg		\
	kmem_cpu.nxt.dbg	\
	kmem_cpu_log.dbg	\
	kmem_cpu_log.nxt.dbg	\
	kmem_log.dbg		\
	kmutex_t.dbg		\
	ksiginfo.dbg		\
	kstat_char.dbg		\
	kstat_i32.dbg		\
	kstat_i64.dbg		\
	kstat_named.dbg		\
	kstat_ui32.dbg		\
	kstat_ui64.dbg		\
	ldtermstd_state.dbg	\
	lock_descriptor.dbg	\
	lockfs.dbg		\
	loinfo.dbg		\
	lwp.dbg			\
	major.dbg		\
	mblk.dbg		\
	mblk.nxt.dbg		\
	memlist.dbg		\
	memlist.list.dbg	\
	memlist.nxt.dbg		\
	ml_odunit.dbg		\
	ml_unit.dbg		\
	mntinfo.dbg		\
	modctl.brief.dbg	\
	modctl.dbg		\
	modctl_list.dbg		\
	modinfo.dbg		\
	modlinkage.dbg		\
	module.dbg		\
	modules.brief.dbg	\
	modules.brief.nxt.dbg	\
	modules.dbg		\
	modules.nxt.dbg		\
	mount.dbg		\
	mount.nxt.dbg		\
	ms_softc.dbg		\
	msgbuf.dbg		\
	msgtext.dbg		\
	mt_map.dbg		\
	ncache.dbg		\
	nce.dbg			\
	netbuf.dbg		\
	ohci_pipe_private.dbg	\
	ohci_polled.dbg		\
	ohci_save_intr_status.dbg	\
	ohci_trans_wrapper.dbg	\
	opaque_auth.dbg		\
	openhci_state.dbg	\
	pad.dbg			\
	page.dbg		\
	panicbuf.dbg		\
	pathname.dbg		\
	pcb.dbg			\
	pid.dbg			\
	pid.print.dbg		\
	pid2proc.chain.dbg	\
	pid2proc.dbg		\
	pm_request.dbg		\
	poll_keystate.dbg	\
	pollcache.dbg		\
	pollcacheset.dbg	\
	polldat.dbg		\
	pollfd.dbg		\
	pollhead.dbg		\
	pollstate.dbg		\
	prcommon.dbg		\
	prgregset.dbg		\
	prnode.dbg		\
	proc.dbg		\
	proc2u.dbg		\
	proc_edge.dbg		\
	proc_tlist.dbg		\
	proc_tlist.nxt.dbg	\
	proc_vertex.dbg		\
	procargs.dbg		\
	procthreads.dbg		\
	procthreads.list.dbg	\
	pt_ttys.dbg		\
	putbuf.dbg		\
	putbuf.wrap.dbg		\
	qinit.dbg		\
	qproc.info.dbg		\
	qthread.info.dbg	\
	queue.dbg		\
	refstr.dbg		\
	rlimit.dbg		\
	rlimit64.dbg		\
	rnode.dbg		\
	root_hub.dbg		\
	rpcerr.dbg		\
	rpctimer.dbg		\
	rtproc.dbg		\
	schedctl.dbg		\
	scsi_addr.dbg		\
	scsi_arq_status.dbg	\
	scsi_dev.dbg		\
	scsi_hba_tran.dbg	\
	scsi_pkt.dbg		\
	scsi_tape.dbg		\
	scsa2usb_state.dbg	\
	scsa2usb_cmd.dbg	\
	seg.dbg			\
	segdev.dbg		\
	segkp.dbg		\
	segkp_data.dbg		\
	seglist.dbg		\
	seglist.nxt.dbg		\
	segmap.dbg		\
	segvn.dbg		\
	sem.nxt.dbg		\
	semid.dbg		\
	session.dbg		\
	setproc.dbg		\
	setproc.done.dbg	\
	setproc.nop.dbg		\
	setproc.nxt.dbg		\
	sgen_state.dbg		\
	shmid.dbg		\
	shminfo.dbg		\
	si.dbg			\
	sigaltstack.dbg		\
	slab.dbg		\
	sleepq.dbg		\
	sleepq.nxt.dbg		\
	slpqtrace.dbg		\
	slpqtrace.list.dbg	\
	slpqtrace.nxt.dbg	\
	smap.dbg		\
	smaphash.dbg		\
	snode.dbg		\
	sobj.dbg		\
	sonode.dbg		\
	st_drivetype.dbg	\
	stack.dbg		\
	stackregs.dbg		\
	stacktrace.dbg		\
	stacktrace.nxt.dbg	\
	stat.dbg		\
	stat64.dbg		\
	stdata.dbg		\
	strtab.dbg		\
	svcfh.dbg		\
	svcmasterxprt.dbg	\
	svcmasterxprt_list.dbg	\
	svcmasterxprt_list.nxt.dbg	\
	svcpool.dbg		\
	svcpool_list.dbg	\
	svcpool_list.nxt.dbg	\
	svcxprt.dbg		\
	syncq.dbg		\
	sysinfo.dbg		\
	tad.dbg			\
	task.dbg		\
	taskq.dbg		\
	tcp.dbg			\
	tcpb.dbg		\
	tcpcb.dbg		\
	tcpip.dbg		\
	termios.dbg		\
	thread.brief.dbg	\
	thread.dbg		\
	thread.link.dbg		\
	thread.trace.dbg	\
	threadlist.dbg		\
	threadlist.link.dbg	\
	threadlist.nxt.dbg	\
	tmount.dbg		\
	tmpnode.dbg		\
	traceall.nxt.dbg	\
	tsdpent.dbg		\
	tsproc.dbg		\
	tune.dbg		\
	turnstile.dbg		\
	ud_ext.dbg		\
	ud_inode.dbg		\
	ud_map.dbg		\
	ud_part.dbg		\
	ud_vfs.dbg		\
	u.dbg			\
	u.sizeof.dbg		\
	ucalltrace.dbg		\
	ucalltrace.nxt.dbg	\
	uf_entry.dbg		\
	uf_info.dbg		\
	ufs_acl.dbg		\
	ufs_acllist.dbg		\
	ufs_aclmask.dbg		\
	ufsq.dbg		\
	ufsvfs.dbg		\
	uio.dbg			\
	ulockfs.dbg		\
	usb_config_descr.dbg	\
	usb_config_pwr_descr.dbg	\
	usb_dev.dbg		\
	usb_device.dbg		\
	usb_device_descr.dbg	\
	usb_endpoint_descr.dbg	\
	usb_hcdi_ops.dbg	\
	usb_hid_descr.dbg	\
	usb_hub_descr.dbg	\
	usb_interface_descr.dbg	\
	usb_interface_pwr_descr.dbg	\
	usb_mid_power.dbg	\
	usb_mid.dbg		\
	usb_pipe_async_request.dbg	\
	usb_pipe_handle_impl.dbg	\
	usb_pipe_policy.dbg	\
	usba_list_entry.dbg	\
	usbkbm_state.dbg	\
	usbms_state.dbg		\
	ustack.dbg		\
	utsname.dbg		\
	v.dbg			\
	v_call.dbg		\
	v_proc.dbg		\
	vattr.dbg		\
	vfs.dbg			\
	vfslist.dbg		\
	vfslist.nxt.dbg		\
	vmem.dbg		\
	vmem_list.dbg		\
	vmem_seg.dbg		\
	vnode.dbg		\
	vpages.dbg		\
	vpages.nxt.dbg		\
	winsize.dbg		\
	xdr.dbg			\
	xref.dbg

SCRIPTS		= $(SRCS:.dbg=)

include $(ADB_BASE_DIR)/../Makefile.uts

# Following grossness is added because the x86 people can't follow the
# naming guidelines...
# Should be simply:
# INCLUDES	= -I${SYSDIR}/${MACH} -I${SYSDIR}/sun
INCLUDES-ia64	= -I${SYSDIR}/intel
INCLUDES-i386	= -I${SYSDIR}/intel -I${SYSDIR}/i86pc
INCLUDES-sparc	= -I${SYSDIR}/${MACH} -I${SYSDIR}/sun
INCLUDES	= ${INCLUDES-${MACH}}
INCDIR		= ${SYSDIR}/common

# to pick up dummy.h for macrogen
INCLUDES += -I$(COMMONDIR)

ROOTUSRDIR	= $(ROOT)/usr
ROOTLIBDIR	= $(ROOTUSRDIR)/lib
ROOTADBDIR	= $(ROOTLIBDIR)/adb

ROOTPROGS	= $(PROGS:%=$(ROOTADBDIR)/%)
ROOTOBJS	= $(OBJS:%=$(ROOTADBDIR)/%)
ROOTSCRIPTS	= $(SCRIPTS:%=$(ROOTADBDIR)/%)

LDLIBS 		= $(ENVLDLIBS1)  $(ENVLDLIBS2)  $(ENVLDLIBS3)
LDFLAGS 	= $(STRIPFLAG) $(ENVLDFLAGS1) $(ENVLDFLAGS2) $(ENVLDFLAGS3)
CPPFLAGS	= $(CPPFLAGS.master)

$(ROOTOBJS)	:= FILEMODE = 644
$(ROOTSCRIPTS)	:= FILEMODE = 644

.KEEP_STATE:

MACH_FLAG=	__$(MACH)
NATIVEDEFS-ia64=
NATIVEDEFS-i386=
NATIVEDEFS-sparc=
NATIVEDEFS	= -D${MACH} -D__${MACH} -D_KERNEL
NATIVEDEFS +=	${NATIVEDEFS-${MACH}}
MGENDEFS-ia64	= -D_LP64 -D_LITTLE_ENDIAN
MGENDEFS-i386	= -D_ILP32 -D_LITTLE_ENDIAN
MGENDEFS-sparc	= -D_ILP32 -D_BIG_ENDIAN
MGENDEFS	= -D${MACH} -D__${MACH}
MGENDEFS	+= ${MGENDEFS-${MACH}}
NATIVEDIR	= $(ADB_BASE_DIR)/native/${NATIVE_MACH}
NATIVEPROGS	= $(PROGS:%=$(NATIVEDIR)/%)
NATIVEOBJS	= $(OBJS:%=$(NATIVEDIR)/%)

NATIVEMGENDIR	= $(ADB_BASE_DIR)/macrogen/${NATIVE_MACH}
NATIVEMACROGEN	= $(MACROGEN:%=$(NATIVEMGENDIR)/%)
NATIVEMGENPP	= $(MGENPP:%=$(NATIVEMGENDIR)/%)

.PARALLEL: $(PROGS) $(NATIVEPROGS) $(OBJS) $(NATIVEOBJS) $(SCRIPTS)

all lint: $(PROGS) $(OBJS) \
	$(NATIVEDIR) .WAIT \
	$(NATIVEPROGS) $(NATIVEOBJS) .WAIT \
	$(NATIVEMACROGEN) $(NATIVEMGENPP) .WAIT \
	$(MACTMP) .WAIT \
	$(SCRIPTS)

install: all .WAIT $(ROOTADBDIR) .WAIT $(ROOTPROGS) $(ROOTOBJS) $(ROOTSCRIPTS)

clean:
	-$(RM) $(OBJS) $(NATIVEOBJS)
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)

clobber: clean
	-$(RM) $(PROGS) $(NATIVEPROGS)
	-$(RM) $(SCRIPTS)
	-$(RM) $(MACTMP)/*.tdbg $(MACTMP)/*.c $(MACTMP)/*.s \
					$(MACTMP)/*.tmp $(MACTMP)/*.res
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)

# installation things

$(ROOTADBDIR)/%: %
	$(INS.file)

$(ROOTUSRDIR) $(ROOTLIBDIR) $(ROOTADBDIR):
	$(INS.dir)

# specific build rules

adbgen:		$(COMMONDIR)/adbgen.sh
	$(RM) $@
	cat $(COMMONDIR)/adbgen.sh >$@
	$(CHMOD) +x $@

adbgen%:	$(COMMONDIR)/adbgen%.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(POST_PROCESS)

adbsub.o:	$(COMMONDIR)/adbsub.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

$(NATIVEDIR):
	$(INS.dir)

$(NATIVEDIR)/adbgen:	$(COMMONDIR)/adbgen.sh
	$(RM) $@
	cat $(COMMONDIR)/adbgen.sh >$@
	$(CHMOD) +x $@

$(NATIVEDIR)/adbgen%:	$(COMMONDIR)/adbgen%.c
	$(NATIVECC) -O -o $@ $<
	$(POST_PROCESS)

$(NATIVEDIR)/adbsub.o:	$(COMMONDIR)/adbsub.c $(NATIVEDIR)
	$(NATIVECC) -c -o $@ $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

$(NATIVEMACROGEN) + $(NATIVEMGENPP): FRC
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE))


#
# rules to generate adb scripts using macrogen
#
# If it cannot find a match, grep returns 1 and make stops. For this
# reason, we need to ensure that both the grep's below always find
# a match (by explicitly including dummy.h in all dbg's).
#
%:	$(ISADIR)/%.dbg
	$(NATIVEMGENPP) $(MGENDEFS) < $< > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(NATIVEDEFS) $(INCLUDES) \
		$(CCYFLAG)$(INCDIR) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

%:	$(COMMONDIR)/%.dbg
	$(NATIVEMGENPP) $(MGENDEFS) < $< > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(NATIVEDEFS) $(INCLUDES) \
		$(CCYFLAG)$(INCDIR) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

check:
	@echo $(SCRIPTS) | tr ' ' '\012' | sed 's/$$/&.dbg/' |\
		sort > script.files
	@(cd $(ADB_BASE_DIR); ls *.dbg) > actual.files
	diff script.files actual.files
	-$(RM) script.files actual.files

# the macro list is platform-architecture specific too.
maclist1:
	@(dir=`pwd`; \
	for i in $(SCRIPTS); do \
		echo "$$dir $$i"; \
	done)

$(MACTMP):
	@pwd; mkdir -p $@

FRC:
