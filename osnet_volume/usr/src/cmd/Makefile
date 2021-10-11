#
#ident	"@(#)Makefile	1.221	00/09/14 SMI"
#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/Makefile
#
# include global definitions
include ../Makefile.master

#
#	Note that the commands 'agents', 'lp', and 'man' are first in
#	the list, violating alphabetical order.  This is because they
#	are very long-running and should be given the most wall-clock
#	time for a parallel build.
#
#	Commands in the FIRST_SUBDIRS list are built before starting the
#	build of other commands.  Currently this includes only 'isaexec'.
#	This is necessary because $(ROOT)/usr/lib/isaexec must exist
#	when some other commands are built because their 'make install'
#	must create a hard link to $(ROOT)/usr/lib/isaexec.
#
#	Commands are listed one per line so that TeamWare can
#	auto-merge most changes.

FIRST_SUBDIRS=		\
	isaexec	

COMMON_SUBDIRS=		\
	agents		\
	lp		\
	perl		\
	man		\
	Adm		\
	abi		\
	acct		\
	adb		\
	ami		\
	arch		\
	asa		\
	aset		\
	asppp		\
	auths		\
	autopush	\
	awk		\
	awk_xpg4	\
	backup		\
	banner		\
	basename	\
	bash		\
	bc		\
	bdiff		\
	bfs		\
	bnu		\
	busstat		\
	bzip2		\
	cal		\
	calendar	\
	captoinfo	\
	cat		\
	cfgadm		\
	checkeq		\
	checknr		\
	chgrp		\
	chmod		\
	chown		\
	chroot		\
	cksum		\
	clear		\
	clinfo		\
	cmd-inet	\
	cmp		\
	col		\
	comm		\
	compress	\
	consadm		\
	coreadm		\
	cpio		\
	cpc		\
	crash		\
	cron		\
	crypt		\
	csh		\
	csplit		\
	cut		\
	date		\
	dc		\
	dd		\
	deroff		\
	des		\
	devfsadm	\
	devfseventd	\
	devinfo		\
	devmgmt		\
	dfs.cmds	\
	diff		\
	diff3		\
	diffmk		\
	dircmp		\
	dirname		\
	dispadmin	\
	dmesg		\
	du		\
	dumpadm		\
	dumpcs		\
	echo		\
	ed		\
	eeprom		\
	egrep		\
	env		\
	eqn		\
	expand		\
	expr		\
	exstr		\
	face		\
	factor		\
	false		\
	fdetach		\
	fdformat	\
	fdisk		\
	filesync	\
	fgrep		\
	file		\
	find		\
	fmli		\
	fmt		\
	fmthard		\
	fmtmsg		\
	fn		\
	fold		\
	format		\
	fs.d		\
	fstyp		\
	fuser		\
	gcore		\
	gencat		\
	geniconvtbl	\
	genmsg		\
	getconf		\
	getent		\
	getfacl		\
	getmajor	\
	getopt		\
	gettext		\
	gettxt		\
	graph		\
	grep		\
	grep_xpg4	\
	groups		\
	grpck		\
	gss		\
	gpatch		\
	gzip		\
	halt		\
	head		\
	hostid		\
	hostname	\
	iconv		\
	id		\
	infocmp		\
	init		\
	initpkg		\
	install.d	\
	iostat		\
	ipcrm		\
	ipcs		\
	isainfo		\
	isalist		\
	join		\
	kbd		\
	keyserv		\
	killall		\
	krb5		\
	ksh		\
	kstat		\
	last		\
	lastcomm	\
	ldap		\
	ldapcachemgr	\
	less		\
	line		\
	link		\
	listen		\
	llc2		\
	loadkeys	\
	locale		\
	localedef	\
	localedef_xsh3	\
	lockstat	\
	lofiadm		\
	logger		\
	login		\
	logins		\
	logname		\
	look		\
	ls		\
	mach		\
	machid		\
	mail		\
	mailx		\
	makekey		\
	mdb		\
	mesg		\
	mkdir		\
	mkfifo		\
	mkfile		\
	mkisofs		\
	mkmsgs		\
	mknod		\
	modload		\
	more		\
	more_xpg4	\
	mpstat		\
	msgfmt		\
	msgid		\
	mt		\
	mv		\
	mvdir		\
	netadm		\
	netfiles	\
	newform		\
	newgrp		\
	news		\
	nice		\
	nl		\
	nlsadmin	\
	nohup		\
	nsadmin		\
	nsadmin3b2	\
	nscd		\
	oampkg		\
	oamuser		\
	oawk		\
	od		\
	osadmin		\
	pack		\
	pagesize	\
	passmgmt	\
	passwd		\
	paste		\
	patch		\
	pathchk		\
	pax		\
	pbind		\
	pcmciad		\
	pfexec		\
	pgrep		\
	plimit		\
	power		\
	pg		\
	pr		\
	print		\
	printf		\
	priocntl	\
	profiles	\
	prstat		\
	prtconf		\
	prtvtoc		\
	ps		\
	psradm		\
	psrinfo		\
	psrset		\
	pt_chmod	\
	ptools		\
	pwck		\
	pwconv		\
	pwd		\
	rcm_daemon	\
	refer		\
	regcmp		\
	renice		\
	rexd		\
	rm		\
	rmdir		\
	rmt		\
	roles		\
	rpcbind		\
	rpcgen		\
	rpcinfo		\
	rpcsvc		\
	rpld		\
	rpm2cpio	\
	sa		\
	saf		\
	savecore	\
	script		\
	sdiff		\
	sed		\
	sed_xpg4	\
	sendmail	\
	setfacl		\
	setmnt		\
	setpgrp		\
	setuname	\
	sgs		\
	sh		\
	sleep		\
	smartcard	\
	soelim		\
	sort		\
	spell		\
	spline		\
	split		\
	srchtxt		\
	streams		\
	strings		\
	su		\
	sulogin		\
	sum		\
	sunpc		\
	swap		\
	sync		\
	sysdef		\
	syslogd		\
	tabs		\
	tail		\
	tar		\
	tbl		\
	tcopy		\
	tcsh		\
	tee		\
	terminfo	\
	tic		\
	time		\
	tip		\
	tnf		\
	touch		\
	tplot		\
	tput		\
	tr		\
	tr_xpg4		\
	troff		\
	true		\
	truss		\
	tty		\
	ttymon		\
	uadmin		\
	ul		\
	uname		\
	uniq		\
	units		\
	unlink		\
	unpack		\
	users		\
	utmp_update	\
	utmpd		\
	.WAIT		\
	apache		\
	valtools	\
	vgrind		\
	vi		\
	vmstat		\
	volmgt		\
	w		\
	wall		\
	wc		\
	which		\
	who		\
	whodo		\
	write		\
	xntpd		\
	xargs		\
	xstr		\
	ypcmd		\
	zdump		\
	zic		\
	zip		\
	zsh

i386_SUBDIRS=		\
	addbadsec	\
	devconfig	\
	diskscan	\
	OWconfig	\
	rtc

sparc_SUBDIRS=		\
	audio		\
	cvcd		\
	dr_daemon	\
	flashprom	\
	lux		\
	prtdiag		\
	ssa		\
	sx

#
# Commands that are messaged.  Note that 'lp' and 'man' come first
# (see previous comment about 'lp' and 'man').
#
MSGSUBDIRS=		\
	lp		\
	man		\
	asa		\
	auths		\
	autopush	\
	awk		\
	awk_xpg4	\
	backup		\
	banner		\
	basename	\
	bnu		\
	busstat		\
	cal		\
	cat		\
	cfgadm		\
	checkeq		\
	checknr		\
	chgrp		\
	chmod		\
	chown		\
	cksum		\
	cmd-inet	\
	cmp		\
	col		\
	comm		\
	compress	\
	consadm		\
	coreadm		\
	cpio		\
	cpc		\
	cron		\
	csh		\
	csplit		\
	cut		\
	date		\
	dc		\
	dd		\
	deroff		\
	devfsadm	\
	diff		\
	diffmk		\
	du		\
	dumpcs		\
	ed		\
	env		\
	eqn		\
	expand		\
	expr		\
	fgrep		\
	file		\
	filesync	\
	find		\
	fold		\
	fs.d		\
	geniconvtbl	\
	genmsg		\
	getconf		\
	getent		\
	gettext		\
	gettxt		\
	grep		\
	grep_xpg4	\
	grpck		\
	gss		\
	halt		\
	head		\
	hostname	\
	iconv		\
	id		\
	isaexec		\
	join		\
	krb5		\
	ksh		\
	kstat		\
	last		\
	ldap		\
	ldapcachemgr	\
	llc2		\
	locale		\
	lofiadm		\
	logger		\
	logins		\
	logname		\
	ls		\
	mailx		\
	mesg		\
	mkdir		\
	more		\
	more_xpg4	\
	mv		\
	newgrp		\
	nice		\
	nohup		\
	oampkg		\
	oawk		\
	od		\
	pack		\
	passwd		\
	passmgmt	\
	paste		\
	patch		\
	pathchk		\
	pax		\
	pfexec		\
	pg		\
	pgrep		\
	power		\
	pr		\
	praudit		\
	print		\
	printf		\
	profiles	\
	prstat		\
	prtdiag		\
	ps		\
	pwconv		\
	pwd		\
	rcm_daemon	\
	refer		\
	regcmp		\
	renice		\
	roles		\
	rm		\
	rmdir		\
	script		\
	sdiff		\
	sed		\
	sed_xpg4	\
	sgs		\
	sh		\
	sleep		\
	sort		\
	split		\
	strings		\
	su		\
	sum		\
	swap		\
	tabs		\
	tail		\
	tar		\
	tbl		\
	tee		\
	time		\
	tnf		\
	touch		\
	tput		\
	tr		\
	tr_xpg4		\
	troff		\
	tty		\
	ttymon		\
	ul		\
	uname		\
	uniq		\
	units		\
	unlink		\
	unpack		\
	valtools	\
	vgrind		\
	vi		\
	volmgt		\
	w		\
	wc		\
	whodo		\
	write		\
	xargs

sparc_MSGSUBDIRS=	\
	audio

#
# commands that use dcgettext for localized time, LC_TIME
#
DCSUBDIRS=		\
	cal		\
	cfgadm		\
	diff		\
	ls		\
	pax		\
	pr		\
	tar		\
	w		\
	who		\
	whodo		\
	write

#
# commands that belong only to the basic security module
#
BSMSUBDIRS=		\
	allocate	\
	audit		\
	audit_warn	\
	auditconfig	\
	auditd		\
	auditreduce	\
	auditstat	\
	dminfo		\
	praudit		\
	bsmconv		\
	bsmunconv

#
# commands not owned by the systems group
#
BWOSDIRS=

all :=		TARGET= all
install :=	TARGET= install
install_h :=	TARGET= install_h
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg
_dc :=		TARGET= _dc

.KEEP_STATE:

SUBDIRS = $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS)

.PARALLEL:	$(BWOSDIRS) $(SUBDIRS) $(MSGSUBDIRS) $(BSMSUBDIRS)

all install clean clobber lint: $(FIRST_SUBDIRS) .WAIT $(SUBDIRS) \
    $(BSMSUBDIRS)

install_h:
	@cd krb5/kwarn; pwd; $(MAKE) $@

#
# The .WAIT directive works around an apparent bug in parallel make.
# Evidently make was getting the target _msg vs. _dc confused under
# some level of parallelization, causing some of the _dc objects
# not to be built.
#
_msg: $(MSGSUBDIRS) $($(MACH)_MSGSUBDIRS) .WAIT _dc

_dc: $(DCSUBDIRS)

$(FIRST_SUBDIRS) $(BWOSDIRS) $(SUBDIRS) $(BSMSUBDIRS): FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	 else \
		true; \
	 fi

FRC:
