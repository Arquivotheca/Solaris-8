#
#pragma ident	"@(#)Makefile	1.53	99/11/05 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/sparcv9/Makefile
#

ADB_BASE_DIR	= ..
ISADIR		= .
COMMONDIR	= $(ADB_BASE_DIR)/common
SYSDIR		= $(ADB_BASE_DIR)/..
NISADIR		= $(ADB_BASE_DIR)/sparc

MACROGEN= macrogen
MGENPP= mgenpp
MACTMP= ./.tmp

OBJS		= adbsub.o
CFLAGS64	+= -D__sparc -D__sparcv9 -D__sparcv9cpu

include $(SYSDIR)/Makefile.uts
include $(SRC)/Makefile.master.64
ALWAYS_DEFS	+= -D__sparc -D__sparcv9 -D__sparcv9cpu
ALWAYS_DEFS	+= -D_SYSCALL32_IMPL -D_ELF64
CFLAGS64	+= $(INLINES)
CCMODE		= -Xa

# NOTE: The following have been at least temporarily removed:
#	dir.dbg
#	dir.nxt.dbg
#	fpu.dbg
#	mount.dbg
#	pme.dbg
#	stat.dbg

# Macros common to all arch's.
#
#
DBGSRCS= \
	anon_map.dbg		\
	anon_hdr.dbg		\
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
	cons_polledio.dbg	\
	cnode.dbg		\
	cpc_ctx.dbg		\
	cpc_event.dbg		\
	cpu.dbg			\
	cpu_dptbl.dbg		\
	cpu_dptbl.nxt.dbg	\
	cpu_stat.dbg		\
	cpu_sysinfo.dbg		\
	cpu_syswait.dbg		\
	cpu_vminfo.dbg		\
	cpupart.dbg		\
	cpun.dbg		\
	cpus.dbg		\
	cred.dbg		\
	csdata.dbg		\
	csmethods.dbg		\
	csum.dbg		\
	cpus.nxt.dbg		\
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
	entity_item.dbg		\
	entity_attribute.dbg	\
	eucioc.dbg		\
	exdata.dbg		\
	fad.dbg	\
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
	hidparser_tok.dbg	\
	hidpers_hdle.dbg	\
	hid_polled_input_callback.dbg	\
	hid_power.dbg		\
	hid_req.dbg		\
	hid_pipe_reset.dbg	\
	hid_default_pipe.dbg	\
	hid_state.dbg		\
	hmelist.dbg		\
	hub_power.dbg		\
	hubd.dbg        	\
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
	kmutex_t.dbg		\
	ksiginfo.dbg		\
	kstat_char.dbg		\
	kstat_i32.dbg		\
	kstat_i64.dbg		\
	kstat_named.dbg		\
	kstat_ui32.dbg		\
	kstat_ui64.dbg		\
	kmem_cache.dbg		\
	kmem_cpu.dbg		\
	kmem_cpu.nxt.dbg	\
	kmem_cpu_log.dbg	\
	kmem_cpu_log.nxt.dbg	\
	kmem_log.dbg		\
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
	modctl.dbg		\
	modctl.brief.dbg	\
	modctl_list.dbg		\
	modinfo.dbg		\
	modlinkage.dbg		\
	module.dbg		\
	modules.dbg		\
	modules.brief.dbg	\
	modules.brief.nxt.dbg	\
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
	proc_edge.dbg		\
	proc_tlist.dbg		\
	proc_tlist.nxt.dbg	\
	proc_vertex.dbg		\
	procargs.dbg		\
	procthreads.dbg		\
	procthreads.list.dbg	\
	proc2u.dbg		\
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
	svcxprt.dbg             \
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
	thread.dbg		\
	thread.brief.dbg	\
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
	u.dbg			\
	u.sizeof.dbg		\
	ucalltrace.dbg		\
	ucalltrace.nxt.dbg	\
	ud_ext.dbg		\
	ud_inode.dbg		\
	ud_map.dbg		\
	ud_part.dbg		\
	ud_vfs.dbg		\
	uf_entry.dbg		\
	uf_info.dbg		\
	ufs_acl.dbg		\
	ufs_acllist.dbg		\
	ufs_aclmask.dbg		\
	ufsq.dbg		\
	ufsvfs.dbg		\
	uio.dbg			\
	ulockfs.dbg		\
	ustack.dbg		\
	utsname.dbg		\
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
	usb_pipe_handle_impl.dbg \
	usb_pipe_policy.dbg	\
	usba_list_entry.dbg	\
	usbkbm_state.dbg	\
	usbms_state.dbg		\
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

# These sparc-specific macros existed in $(ISADIR) or $(COMMONDIR). Leave
# them here for now. Eventually, we should move everything to $(COMMONDIR),
# to reduce complication.
#
#
DBGSRCS += \
	le.dbg			\
	lestr.dbg		\
	stackcalls.dbg		\
	zscom.dbg

# This sparc-specific list existed only in $(ADB_BASE_DIR)/sparc. Pick
# them up from there for now. Eventually, even these should be moved
# to $(COMMONDIR).
#
#
SSDBGSRC= \
	ata_controller.dbg	\
	ata_drive.dbg		\
	ata_pkt.dbg		\
	audio_apm_info.dbg	\
	audio_ch.dbg		\
	audio_channel.dbg	\
	audio_state.dbg		\
	audiotrace.dbg		\
	audiotrace.nxt.dbg	\
	cmd_ctl.dbg		\
	common_service.dbg	\
	cq.dbg			\
	cq_hdr.dbg		\
	cqe.dbg			\
	ct_header.dbg		\
	d_id_hash.dbg		\
	dcd_address.dbg		\
	dcd_cmd.dbg		\
	dcd_device.dbg		\
	dcd_disk.dbg		\
	dcd_hba_tran.dbg	\
	dcd_identify.dbg	\
	dcd_pkt.dbg		\
	dk_label.dbg		\
	dk_vtoc.dbg		\
	esp.dbg			\
	esp_cb_info.dbg		\
	esp_cmd.dbg		\
	esp_tagQ.dbg		\
	fas.dbg			\
	fas_cmd.dbg		\
	fas_regs.dbg		\
	fas_tagQ.dbg		\
	fc_dataseg.dbg		\
	fc_device.dbg		\
	fc_fca_bind.dbg		\
	fc_fca_port_info.dbg	\
	fc_lilpmap.dbg		\
	fc_packet.dbg		\
	fc_port.dbg		\
	fc_port_clist.dbg	\
	fc_port_dev.dbg		\
	fc_portmap.dbg		\
	fca_port.dbg		\
	fca_tran.dbg		\
	fcio.dbg		\
	fcp_cmd.dbg		\
	fcp_rsp.dbg		\
	fp_cmd.dbg		\
	frame_header.dbg	\
	ghd_cmd.dbg		\
	glm.dbg			\
	glm_cmd.dbg		\
	glm_dsa.dbg		\
	glm_tagQ.dbg		\
	glm_unit.dbg		\
	hard_addr.dbg		\
        ifp.dbg                 \
        ifp_cmd.dbg             \
        ifp_full.dbg            \
        ifp_icb.dbg             \
        ifp_portdb.dbg          \
        ifp_request.dbg         \
        ifp_response.dbg        \
        ifp_target.dbg          \
	intrspec.dbg		\
	isp.dbg			\
	isp_cmd.dbg		\
	isp_request.dbg		\
	isp_response.dbg	\
	job_request.dbg		\
	la_els_adisc.dbg	\
	la_els_logi.dbg		\
	la_els_logo.dbg		\
	la_wwn.dbg		\
	ls_code.dbg		\
	mixer_ad_info.dbg	\
	ns_resp_gan.dbg		\
	nwwn_elem.dbg		\
	nwwn_list.dbg		\
	pci.dbg			\
	port_device.dbg		\
	port_id.dbg		\
	port_type.dbg		\
	pwwn_hash.dbg		\
	rwindow.dbg		\
	scsi_disk.dbg		\
	service_param.dbg	\
	sf.dbg			\
	soft_attach.dbg		\
	ssa_disk.dbg		\
	ssfcp_ipkt.dbg		\
	ssfcp_lun.dbg		\
	ssfcp_pkt.dbg		\
	ssfcp_port.dbg		\
	ssfcp_tgt.dbg		\
	systemdump		\
	ulp_modinfo.dbg		\
	ulp_module.dbg		\
	ulp_port_info.dbg	\
	unsolicited_buffer.dbg	\
	usoc_cmdonly_request.dbg\
	usoc_data_request.dbg	\
	usoc_diag_request.dbg	\
	usoc_hdr.dbg		\
	usoc_idinfo.dbg		\
	usoc_kernel_cq.dbg	\
	usoc_pkt_priv.dbg	\
	usoc_pool_request.dbg	\
	usoc_port.dbg		\
	usoc_priv_cmd.dbg	\
	usoc_request.dbg	\
	usoc_request_descriptor.dbg	\
	usoc_response.dbg	\
	usoc_state.dbg		\
	usoc_timetag.dbg	\
	usoc_ub_priv.dbg	\
	usoc_unsol_cb.dbg

DBGSRCS += $(SSDBGSRC)
SSDBGS= $(SSDBGSRC:.dbg=)

# extra scripts generated by same source files
# excluded from the build but need to be included with installation
MACROS_EXT = pci_fault_handle.dbg pci_common.dbg cb.dbg ib.dbg pbm.dbg \
	iommu.dbg sc.dbg config_header.dbg ecc.dbg pci_ispec.dbg \
	config_header_state.dbg ib_ino_info.dbg dvma_rec.dbg \
	pci_bus_range_t ddi_dma_impl.dbg i_ddi_soft_state.dbg \
	sfpkt sf_stats sftarget sftargets
SCRIPTS_EXT = $(MACROS_EXT:.dbg=)
SCRIPTS64 = $(DBGSRCS:.dbg=) $(SCRIPTS_EXT)

# compilation defines
#
DEFINES= -D_KERNEL -D$(MACH) -D__$(MACH) -D$(MACH64) -D__$(MACH64) -D_ELF64
MGENDEFINES= $(DEFINES) -D_LP64 -D_BIG_ENDIAN
INCDIR= ${SYSDIR}/common
INCS= -I$(SYSDIR)/$(MACH64) -I$(SYSDIR)/$(MACH) -I$(SYSDIR)/sun -I$(SRC)/head \
	-I${SYSDIR}/${MACH}/v9 -I${SYSDIR}/sun4u

# this is for macrogen to pickup dummy.h (to determine data model)
#
INCS += -I$(COMMONDIR)


# install paths and conditionals
#
ROOTUSRDIR  = $(ROOT)/usr
ROOTLIBDIR  = $(ROOTUSRDIR)/lib
ROOTADBDIR  = $(ROOTLIBDIR)/adb
ROOTADBDIR64  = $(ROOTADBDIR)/$(MACH64)

ROOTOBJS	= $(OBJS:%=$(ROOTADBDIR64)/%)
$(ROOTOBJS)	:= FILEMODE = 644

ROOTSCRIPTS64  = $(SCRIPTS64:%=$(ROOTADBDIR64)/%)
$(ROOTSCRIPTS64)	:= FILEMODE = 644


.KEEP_STATE:

.PARALLEL: $(OBJS) $(SCRIPTS64) $(SSDBGS)


# macrogen path
#
NATIVEMGENDIR  = $(ADB_BASE_DIR)/macrogen/${NATIVE_MACH}
NATIVEMACROGEN  = $(MACROGEN:%=$(NATIVEMGENDIR)/%)
NATIVEMGENPP	= $(MGENPP:%=$(NATIVEMGENDIR)/%)

# targets
#
all lint: $(OBJS) $(NATIVEMACROGEN) $(NATIVEMGENPP) $(MACTMP) .WAIT $(SCRIPTS64)

install: all .WAIT $(ROOTADBDIR64) .WAIT $(ROOTOBJS) .WAIT $(ROOTSCRIPTS64)

clean:
	-$(RM) $(OBJS)
	-$(RM) $(SCRIPTS64)
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)


clobber: clean
	-$(RM) $(MACTMP)/*.res $(MACTMP)/*.tmp $(MACTMP)/*.s \
				    $(MACTMP)/*.c $(MACTMP)/*.tdbg
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)


# installation rules
#
$(ROOTADBDIR64)/%: %
	$(INS.file)

$(ROOTUSRDIR) $(ROOTLIBDIR) $(ROOTADBDIR) $(ROOTADBDIR64):
	$(INS.dir.root.sys)

#
# rules to generate objects
#
adbsub.o:	$(COMMONDIR)/adbsub.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

# macrogen rules
#
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
	$(NATIVEMGENPP) $(MGENDEFINES) < $< > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(DEFINES) $(INCS) $(CCYFLAG)$(INCDIR) \
		$(CFLAGS) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

%:	$(COMMONDIR)/%.dbg
	$(NATIVEMGENPP) $(MGENDEFINES) < $< > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(DEFINES) $(INCS) $(CCYFLAG)$(INCDIR) \
		$(CFLAGS) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

$(SSDBGS):	$(NISADIR)/$$@.dbg
	$(NATIVEMGENPP) < $(NISADIR)/$@.dbg $(MGENDEFINES) > \
		$(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(DEFINES) $(INCS) $(CCYFLAG)$(INCDIR) \
		$(CFLAGS) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

$(SCRIPTS_EXT):

check:

# macro list generation for kadb (platform-architecture specific)
#
maclist1:
	@(dir=`pwd`; \
	for i in $(SCRIPTS64); do \
		echo "$$dir $$i"; \
	done)

$(MACTMP):
	@pwd; mkdir -p $@

FRC:	
