#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.246	00/09/14 SMI"
#
# uts/common/sys/Makefile
#	This makefile installs all common system header files.
#
# include global definitions
include ../../../Makefile.master

#
#	Note that the following headers are present in the kernel but
#	neither installed or shipped as part of the product:
#		unix_bb_info.h:		Private interface to kcov
#		sunos_dhcp_class.h	Private between inetboot & nfs_dlboot
#		uwidth.h		Private interface to ldterm
#

i386_HDRS=			\
	asy.h			\
	fd_debug.h		\
	fdc.h			\
	fdmedia.h		\
	lp.h			\
	mse.h			\
	vuidmice.h

sparc_HDRS= 

HDRS=				\
	acct.h			\
	acl.h			\
	aio.h			\
	aio_impl.h		\
	aio_req.h		\
	aiocb.h			\
	ascii.h			\
	atomic.h		\
	audio.h			\
	audio_impl.h		\
	audiodebug.h		\
	audioio.h		\
	audiovar.h		\
	autoconf.h		\
	auxv.h			\
	battery.h		\
	bitmap.h		\
	bpp_io.h		\
	buf.h			\
	bufmod.h		\
	bustypes.h		\
	byteorder.h		\
	callb.h			\
	callo.h			\
	cdio.h			\
	cdump.h			\
	cladm.h			\
	class.h			\
	clconf.h		\
	cmn_err.h		\
	compress.h		\
	condvar.h		\
	condvar_impl.h		\
	conf.h			\
	consdev.h		\
	console.h		\
	copyops.h		\
	core.h			\
	corectl.h		\
	cpc_event.h		\
	cpc_impl.h		\
	cpr.h			\
	cpupart.h		\
	cpuvar.h		\
	cred.h			\
	crtctl.h		\
	csiioctl.h		\
	cyclic.h		\
	cyclic_impl.h		\
	dacf.h			\
	dacf_impl.h		\
	dc_ki.h			\
	ddi.h			\
	ddi_impldefs.h		\
	ddi_implfuncs.h		\
	ddidevmap.h		\
	ddidmareq.h		\
	ddimapreq.h		\
	ddipropdefs.h		\
	dditypes.h		\
	debug.h			\
	des.h			\
	devctl.h		\
	devinfo_impl.h		\
	devops.h		\
	devpoll.h		\
	dirent.h		\
	disp.h			\
	dkbad.h			\
	dkio.h			\
	dklabel.h		\
	dl.h			\
	dlpi.h			\
	dnlc.h			\
	door.h			\
	door_data.h		\
	dumpadm.h		\
	dumphdr.h		\
	ecppsys.h		\
	elf.h			\
	elf_386.h		\
	elf_M32.h		\
	elf_SPARC.h		\
	elf_ia64.h		\
	elf_notes.h		\
	elftypes.h		\
	epm.h			\
	err.h			\
	errno.h			\
	ertyp.h			\
	esunddi.h		\
	ethernet.h		\
	euc.h			\
	eucioctl.h		\
	exec.h			\
	exechdr.h		\
	fault.h			\
	fbio.h			\
	fbuf.h			\
	fcntl.h			\
	fdbuffer.h		\
	fdio.h			\
	feature_tests.h		\
	file.h			\
	filio.h			\
	flock.h			\
	flock_impl.h		\
	fsid.h			\
	fstyp.h			\
	ftrace.h		\
	g711.h			\
	hdio.h			\
	hwconf.h		\
	ia.h			\
	iapriocntl.h		\
	id32.h			\
	ieeefp.h		\
	instance.h		\
	int_const.h		\
	int_fmtio.h		\
	int_limits.h		\
	int_types.h		\
	inttypes.h		\
	ioccom.h		\
	ioctl.h			\
	ioreq.h			\
	ipc.h			\
	ipd_extern.h		\
	ipd_ioctl.h		\
	ipd_sys.h		\
	isa_defs.h		\
	jioctl.h		\
	kbd.h			\
	kbdreg.h		\
	kbio.h			\
	klwp.h			\
	kmem.h			\
	kmem_impl.h		\
	kobj.h			\
	kobj_impl.h		\
	kstat.h			\
	kstr.h			\
	ksyms.h			\
	ksynch.h		\
	ldterm.h		\
	link.h			\
	llc1.h			\
	llc2.h			\
	loadavg.h		\
	lock.h			\
	lockfs.h		\
	lockstat.h		\
	lofi.h			\
	log.h			\
	logindmux.h		\
	devfs_log_event.h	\
	lwp.h			\
	lwp_upimutex_impl.h	\
	map.h			\
	md5.h			\
	md5_consts.h		\
	mem.h			\
	mem_config.h		\
	memlist.h		\
	mkdev.h			\
	mhd.h			\
	mixer.h			\
	mixer_impl.h		\
	mman.h			\
	mntent.h		\
	mntio.h			\
	mnttab.h		\
	modctl.h		\
	mode.h			\
	model.h			\
	modhash.h		\
	mount.h			\
	msacct.h		\
	msg.h			\
	msio.h			\
	msreg.h			\
	mtio.h			\
	mutex.h			\
	ndi_impldefs.h		\
	netconfig.h		\
	nexusdefs.h		\
	nexusintr.h		\
	note.h			\
	open.h			\
	openpromio.h		\
	panic.h			\
	param.h			\
	pathconf.h		\
	pathname.h		\
	pbio.h			\
	pccard.h		\
	pci.h			\
	pci_impl.h		\
	pcmcia.h		\
	pctypes.h		\
	pem.h			\
	pfmod.h			\
	pm.h			\
	poll.h			\
	poll_impl.h		\
	ppp_chap.h		\
	ppp_diag.h		\
	ppp_extern.h		\
	ppp_ioctl.h		\
	ppp_lqm.h		\
	ppp_pap.h		\
	ppp_sys.h		\
	priocntl.h		\
	proc.h			\
	processor.h		\
	procfs.h		\
	procset.h		\
	protosw.h		\
	prsystm.h		\
	pset.h			\
	ptem.h			\
	ptms.h			\
	ptrace.h		\
	ptyvar.h		\
	rce.h			\
	reboot.h		\
	refstr.h		\
	refstr_impl.h		\
	resource.h		\
	rlioctl.h		\
	rt.h			\
	rtpriocntl.h		\
	rwlock.h		\
	rwlock_impl.h		\
	sad.h			\
	schedctl.h		\
	seg_drv.h		\
	select.h		\
	sem.h			\
	sema_impl.h		\
	semaphore.h		\
	ser_sync.h		\
	session.h		\
	share.h			\
	shm.h			\
	siginfo.h		\
	signal.h		\
	sleepq.h		\
	sobject.h		\
	socket.h		\
	socketvar.h		\
	sockio.h		\
	sockmod.h		\
	sservice.h		\
	stat.h			\
	statfs.h		\
	statvfs.h		\
	stermio.h		\
	stream.h		\
	strft.h			\
	strick.h		\
	strlog.h		\
	strmdep.h		\
	stropts.h		\
	strredir.h		\
	strstat.h		\
	strsubr.h		\
	strsun.h		\
	strtty.h		\
	sunddi.h		\
	sunndi.h		\
	sunos_dhcp_class.h	\
	sunpm.h			\
	suntpi.h		\
	suntty.h		\
	swap.h			\
	synch.h			\
	syscall.h		\
	sysconf.h		\
	sysconfig.h		\
	sysinfo.h		\
	syslog.h		\
	sysmacros.h		\
	sysmsg_impl.h		\
	systeminfo.h		\
	systm.h			\
	taskq.h			\
	taskq_impl.h		\
	t_kuser.h		\
	t_lock.h		\
	telioctl.h		\
	termio.h		\
	termios.h		\
	termiox.h		\
	thread.h		\
	ticlts.h		\
	ticots.h		\
	ticotsord.h		\
	tihdr.h			\
	time.h			\
	time_impl.h		\
	time_std_impl.h		\
	timeb.h			\
	timer.h			\
	times.h			\
	timex.h			\
	timod.h			\
	tirdwr.h		\
	tiuser.h		\
	tl.h			\
	tnf.h			\
	tnf_com.h		\
	tnf_probe.h		\
	tnf_writer.h		\
	todio.h			\
	tpicommon.h		\
	ts.h			\
	tspriocntl.h		\
	ttcompat.h		\
	ttold.h			\
	tty.h			\
	ttychars.h		\
	ttydev.h		\
	tuneable.h		\
	turnstile.h		\
	types.h			\
	types32.h		\
	uadmin.h		\
	uio.h			\
	ulimit.h		\
	un.h			\
	unistd.h		\
	user.h			\
	ustat.h			\
	utime.h			\
	utrap.h			\
	utsname.h		\
	utssys.h		\
	va_list.h		\
	var.h			\
	varargs.h		\
	vfs.h			\
	vfstab.h		\
	vgareg.h		\
	visual_io.h		\
	vjcomp.h		\
	vm.h			\
	vmem.h			\
	vmem_impl.h		\
	vmmeter.h		\
	vmsystm.h		\
	vnode.h			\
	vol.h			\
	vtoc.h			\
	vtrace.h		\
	vuid_event.h		\
	vuid_queue.h		\
	vuid_state.h		\
	vuid_store.h		\
	wait.h			\
	watchpoint.h		\
	winlockio.h		\
	xti_inet.h		\
	xti_osi.h		\
	xti_xtiopt.h

ISOHDRS=			\
	signal_iso.h

FSHDRS=				\
	autofs.h		\
	cachefs_dir.h		\
	cachefs_dlog.h		\
	cachefs_filegrp.h	\
	cachefs_fs.h		\
	cachefs_fscache.h	\
	cachefs_ioctl.h		\
	cachefs_log.h		\
	fifonode.h		\
	hsfs_isospec.h		\
	hsfs_node.h		\
	hsfs_rrip.h		\
	hsfs_spec.h		\
	hsfs_susp.h		\
	lofs_info.h		\
	lofs_node.h		\
	mntdata.h		\
	namenode.h		\
	pc_dir.h		\
	pc_fs.h			\
	pc_label.h		\
	pc_node.h		\
	pxfs_ki.h		\
	s5_fblk.h		\
	s5_fs.h			\
	s5_fsdir.h		\
	s5_inode.h		\
	s5_lockfs.h		\
	s5_mount.h		\
	snode.h			\
	swapnode.h		\
	tmp.h			\
	tmpnode.h		\
	udf_inode.h		\
	udf_volume.h		\
	ufs_acl.h		\
	ufs_bio.h		\
	ufs_filio.h		\
	ufs_fs.h		\
	ufs_fsdir.h		\
	ufs_inode.h		\
	ufs_lockfs.h		\
	ufs_log.h		\
	ufs_mount.h		\
	ufs_panic.h		\
	ufs_prot.h		\
	ufs_quota.h		\
	ufs_trans.h

PCMCIAHDRS=		\
	pcata.h		\
	pcgld.h		\
	pcramio.h	\
	pcramvar.h	\
	pcser_conf.h	\
	pcser_io.h	\
	pcser_reg.h	\
	pcser_manuspec.h \
	pcser_var.h

PCMCIAHDRS += pcelx.h

SCSIHDRS=		\
	scsi.h		\
	scsi_address.h	\
	scsi_ctl.h	\
	scsi_params.h	\
	scsi_pkt.h	\
	scsi_resource.h	\
	scsi_types.h	\
	scsi_watch.h

SCSICONFHDRS=		\
	autoconf.h	\
	device.h

SCSIGENHDRS=		\
	commands.h	\
	dad_mode.h	\
	inquiry.h	\
	message.h	\
	mode.h		\
	sense.h		\
	status.h

SCSIIMPLHDRS=		\
	commands.h	\
	inquiry.h	\
	mode.h		\
	pkt_wrapper.h	\
	sense.h		\
	services.h	\
	status.h	\
	transport.h	\
	scsi_reset_notify.h \
	types.h		\
	uscsi.h

SCSITARGETSHDRS=	\
	osesio.h	\
	ses.h		\
	sesio.h		\
	stdef.h		\
	sgendef.h

SCSICADHDRS=		\
	glmreg.h	\
	glmvar.h

USBHDRS=	\
	usba.h	\
	usbai.h

USBHUBDHDRS=		\
	hub.h

USBHIDHDRS=		\
	hid.h

USBMSHDRS=		\
	usb_bulkonly.h

HOTPLUGHDRS=	\
	hpcsvc.h	\
	hpctrl.h

HOTPLUGPCIHDRS=	\
	pcicfg.h	\
	pcihp.h

# "cmdk" headers used on sparc
SDKTPHDRS=		\
	dadkio.h	\
	fdisk.h

# "cmdk" headers used on i386
DKTPHDRS=		\
	altsctr.h	\
	bbh.h		\
	cm.h		\
	cmdev.h		\
	cmdk.h		\
	cmpkt.h		\
	controller.h	\
	dadev.h		\
	dadk.h		\
	dadkio.h	\
	dklb.h		\
	fctypes.h	\
	fdisk.h		\
	flowctrl.h	\
	gda.h		\
	hba.h		\
	objmgr.h	\
	quetypes.h	\
	queue.h		\
	snlb.h		\
	tgcom.h		\
	tgdk.h

# "pc" header files used on i386
PCHDRS=			\
	avintr.h	\
	dma_engine.h	\
	dma_i8237A.h	\
	gld.h		\
	gldpriv.h	\
	i8272A.h	\
	pcic_reg.h	\
	pcic_var.h	\
	pic.h		\
	pit.h		\
	rtc.h

# I2O header files (currently used only on i386)
I2OHDRS=		\
	i2oadptr.h	\
	i2obscsi.h	\
	i2oexec.h	\
	i2omsg.h	\
	i2omstr.h	\
	i2outil.h

# SVVS headers and other excluded headers
XHDRS=		\
	lo.h	\
	tidg.h	\
	tivc.h	\
	tmux.h

# check files really don't exist

fs/%.check:	fs/%.h
	$(DOT_H_CHECK)

pcmcia/%.check:	pcmcia/%.h
	$(DOT_H_CHECK)

scsi/%.check:		scsi/%.h
	$(DOT_H_CHECK)

scsi/conf/%.check:	scsi/conf/%.h
	$(DOT_H_CHECK)

scsi/generic/%.check:	scsi/generic/%.h
	$(DOT_H_CHECK)

scsi/impl/%.check:	scsi/impl/%.h
	$(DOT_H_CHECK)

scsi/targets/%.check:	scsi/targets/%.h
	$(DOT_H_CHECK)


usb/%.check:		usb/%.h
	$(DOT_H_CHECK)

usb/hubd/%.check:	usb/hubd/%.h
	$(DOT_H_CHECK)

usb/clients/hid/%.check:	usb/clients/hid/%.h
	$(DOT_H_CHECK)

usb/clients/mass_storage/%.check:	usb/clients/mass_storage/%.h
	$(DOT_H_CHECK)

usb/clients/hid/%.check:	usb/clients/hid/%.h
	$(DOT_H_CHECK)


CHECKHDRS=						\
	$($(MACH)_HDRS:%.h=%.check)			\
	$(HDRS:%.h=%.check)				\
	$(ISOHDRS:%.h=iso/%.check)				\
	$(FSHDRS:%.h=fs/%.check)			\
	$(XHDRS:%.h=%.check)				\
	$(PCMCIAHDRS:%.h=pcmcia/%.check)		\
	$(SCSIHDRS:%.h=scsi/%.check)			\
	$(SCSICONHDRS:%.h=scsi/conf/%.check)		\
	$(SCSIIMPLHDRS:%.h=scsi/impl/%.check)		\
	$(SCSITARGETSHDRS:%.h=scsi/targets/%.check)	\
	$(USBHUBDHDRS:%.h=usb/hubd/%.check)		\
	$(USBHIDHDRS:%.h=usb/clients/hid/%.check)	\
	$(USBMSHDRS:%.h=usb/clients/mass_storage/%.check)	\
	$(USBHDRS:%.h=usb/%.check)


ROOTDIR=	$(ROOT)/usr/include/sys

ROOTDKTPDIR=	$(ROOTDIR)/dktp

sparc_ROOTDIRS=	$(ROOTDKTPDIR) $(ROOTDIR)/scsi/adapters \
		$(ROOTDIR)/scsi/targets


i386_ROOTDIRS=	$(ROOTDKTPDIR) $(ROOTDIR)/scsi/targets $(ROOTDIR)/i2o \
		$(ROOTDIR)/hotplug $(ROOTDIR)/hotplug/pci


ROOTDIRS=			\
	$(ROOTDIR)		\
	$(ROOTDIR)/iso		\
	$(ROOTDIR)/fs		\
	$(ROOTDIR)/pcmcia	\
	$(ROOTDIR)/scsi		\
	$(ROOTDIR)/scsi/conf	\
	$(ROOTDIR)/scsi/generic	\
	$(ROOTDIR)/scsi/impl	\
	$(ROOTDIR)/usb		\
	$(ROOTDIR)/usb/hubd	\
	$(ROOTDIR)/usb/clients	\
	$(ROOTDIR)/usb/clients/hid \
	$(ROOTDIR)/usb/clients/mass_storage	\
	$($(MACH)_ROOTDIRS)



ROOTHDRS=				\
	$($(MACH)_HDRS:%=$(ROOTDIR)/%)	\
	$(HDRS:%=$(ROOTDIR)/%)

ROOTISOHDRS= $(ISOHDRS:%=$(ROOTDIR)/iso/%)

ROOTFSHDRS= $(FSHDRS:%=$(ROOTDIR)/fs/%)

ROOTPCMCIAHDRS= $(PCMCIAHDRS:%=$(ROOTDIR)/pcmcia/%)

ROOTSCSIHDRS= $(SCSIHDRS:%=$(ROOTDIR)/scsi/%)
ROOTSCSICONFHDRS= $(SCSICONFHDRS:%=$(ROOTDIR)/scsi/conf/%)
ROOTSCSIGENHDRS= $(SCSIGENHDRS:%=$(ROOTDIR)/scsi/generic/%)
ROOTSCSIIMPLHDRS= $(SCSIIMPLHDRS:%=$(ROOTDIR)/scsi/impl/%)
ROOTSCSITARGETSHDRS= $(SCSITARGETSHDRS:%=$(ROOTDIR)/scsi/targets/%)
ROOTSCSICADHDRS= $(SCSICADHDRS:%=$(ROOTDIR)/scsi/adapters/%)

ROOTXHDRS= $(XHDRS:%=$(ROOTDIR)/%)

ROOTUSBHDRS= $(USBHDRS:%=$(ROOTDIR)/usb/%)
ROOTUSBHUBDHDRS= $(USBHUBDHDRS:%=$(ROOTDIR)/usb/hubd/%)
ROOTUSBHIDHDRS = $(USBHIDHDRS:%=$(ROOTDIR)/usb/clients/hid/%)
ROOTUSBMSHDRS= $(USBMSHDRS:%=$(ROOTDIR)/usb/clients/mass_storage/%)

ROOTSDKTPHDRS= $(SDKTPHDRS:%=$(ROOTDIR)/dktp/%)
ROOTDKTPHDRS= $(DKTPHDRS:%=$(ROOTDIR)/dktp/%)
ROOTPCHDRS= $(PCHDRS:%=$(ROOTDIR)/%)
ROOTI2OHDRS= $(I2OHDRS:%=$(ROOTDIR)/i2o/%)
ROOTHOTPLUGHDRS= $(HOTPLUGHDRS:%=$(ROOTDIR)/hotplug/%)
ROOTHOTPLUGPCIHDRS= $(HOTPLUGPCIHDRS:%=$(ROOTDIR)/hotplug/pci/%)


sparc_ROOTHDRS=	$(ROOTSDKTPHDRS) $(ROOTSCSICADHDRS) $(ROOTSCSITARGETSHDRS) \
		$(ROOTUSBHDRS) $(ROOTUSBHUBDHDRS) $(ROOTPCMCIAHDRS) \
		$(ROOTUSBHIDHDRS) $(ROOTUSBMSHDRS)


i386_ROOTHDRS=	$(ROOTDKTPHDRS) $(ROOTPCHDRS) $(ROOTSCSITARGETSHDRS)	\
		$(ROOTI2OHDRS) $(ROOTPCMCIAHDRS) $(ROOTHOTPLUGHDRS) \
		$(ROOTHOTPLUGPCIHDRS)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

$(ROOTDIR)/fs/%: fs/%
	$(INS.file)

$(ROOTDIR)/pcmcia/%:		pcmcia/%
	$(INS.file)

$(ROOTDIR)/scsi/%:		scsi/%
	$(INS.file)


$(ROOTDIR)/scsi/conf/%:		scsi/conf/%
	$(INS.file)


$(ROOTDIR)/scsi/generic/%:	scsi/generic/%
	$(INS.file)

$(ROOTDIR)/scsi/impl/%:		scsi/impl/%
	$(INS.file)

$(ROOTDIR)/scsi/targets/%:	scsi/targets/%
	$(INS.file)

$(ROOTDIR)/usb/%:		usb/%
	$(INS.file)

$(ROOTDIR)/usb/hubd/%:		usb/hubd/%
	$(INS.file)

$(ROOTDIR)/usb/clients/hid/%:	usb/clients/hid/%
	$(INS.file)

$(ROOTDIR)/usb/clients/mass_storage/%:	usb/clients/mass_storage/%
	$(INS.file)

$(ROOTDIR)/dktp/%: dktp/%
	$(INS.file)

.KEEP_STATE:

.PARALLEL:			\
	$(CHECKHDRS)		\
	$(ROOTHDRS)		\
	$(ROOTISOHDRS)		\
	$(ROOTFSHDRS)		\
	$(ROOTPCMCIAHDRS)	\
	$(ROOTSCSIHDRS)		\
	$(ROOTSCSICONFHDRS)	\
	$(ROOTSCSIGENHDRS)	\
	$(ROOTSCSIIMPLHDRS)	\
	$(ROOTUSBHDRS)		\
	$(ROOTUSBHUBDHDRS)	\
	$(ROOTUSBHIDHDRS)	\
	$(ROOTUSBMSHDRS)	\
	$($(MACH)_ROOTHDRS)


install_h:			\
	$(ROOTDIRS)		\
	.WAIT			\
	$(ROOTHDRS)		\
	$(ROOTISOHDRS)		\
	$(ROOTFSHDRS)		\
	$(ROOTPCMCIAHDRS)	\
	$(ROOTSCSIHDRS)		\
	$(ROOTSCSICONFHDRS)	\
	$(ROOTSCSIGENHDRS)	\
	$(ROOTSCSIIMPLHDRS)	\
	$(ROOTUSBHDRS)		\
	$(ROOTUSBHUBDHDRS)	\
	$(ROOTUSBHIDHDRS)	\
	$(ROOTUSBMSHDRS)	\
	$($(MACH)_ROOTHDRS)	\
	svvs_h

svvs_h: FRC
	@if [ -f lo.h  ]; then \
		$(MAKE) install_X_h; \
	else \
		true; \
	fi

install_X_h: $(ROOTXHDRS)

$(ROOTDIRS):
	$(INS.dir)

# Special rules for several PPP headers for which the source is shared by the
# UNBUNDLED 4.x product and the BUNDLED 5.x product.  Once the 4.x product is
# uninteresting, this special case should be removed.

$(ROOTDIR)/ppp_extern.h:	ppp_extern.h
	$(RM) $@
	unifdef -U_SunOS4 ppp_extern.h > $@
	$(CH)chmod $(FILEMODE) $@
	$(CH)$(CHGRP) $(GROUP) $@
	$(CH)$(CHOWN) $(OWNER) $@

$(ROOTDIR)/ppp_ioctl.h:		ppp_ioctl.h
	$(RM) $@
	unifdef -U_SunOS4 ppp_ioctl.h > $@
	$(CH)chmod $(FILEMODE) $@
	$(CH)$(CHGRP) $(GROUP) $@
	$(CH)$(CHOWN) $(OWNER) $@

$(ROOTDIR)/ppp_sys.h:		ppp_sys.h
	$(RM) $@
	unifdef -U_SunOS4 ppp_sys.h > $@
	$(CH)chmod $(FILEMODE) $@
	$(CH)$(CHGRP) $(GROUP) $@
	$(CH)$(CHOWN) $(OWNER) $@

check:	$(CHECKHDRS)

FRC:

