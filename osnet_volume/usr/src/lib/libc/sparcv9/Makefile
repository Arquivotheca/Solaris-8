#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.74	99/08/05 SMI"
#
# lib/libc/sparcv9/Makefile
#
# This builds all objects needed for libc, including those derived
# from source under ../port.  crt0.o and friends belong to the compiler.

LIBRARY= libc.a
LIB_PIC= libc_pic.a
VERS=	 .1
M4=	m4
CPP=	/usr/lib/cpp

# values deliverables
VALUES=	values-Xa.o values-Xc.o values-Xs.o values-Xt.o values-xpg4.o

# objects are grouped by source directory, except for the few objects
# requiring special treatment in the ordering of the library.  These
# few are found in the FIRSTOBJS and LASTOBJS macros.

# local objects
#
STRETS=

CRTOBJS=			\
	$(STRETS)		\
	_ftou.o			\
	__align_cpy_2.o		\
	__align_cpy_4.o		\
	__align_cpy_8.o

DYNOBJS=

FPOBJS=				\
	_Q_add.o		\
	_Q_cmp.o		\
	_Q_cmpe.o		\
	_Q_div.o		\
	_Q_dtoq.o		\
	_Q_fcc.o		\
	_Q_get_rp_rd.o		\
	_Q_itoq.o		\
	_Q_mul.o		\
	_Q_neg.o		\
	_Q_qtod.o		\
	_Q_qtoi.o		\
	_Q_qtos.o		\
	_Q_qtou.o		\
	_Q_sqrt.o		\
	_Q_stoq.o		\
	_Q_sub.o		\
	_Q_utoq.o		\
	_Qp_qtox.o		\
	_Qp_qtoux.o		\
	_Qp_xtoq.o		\
	_Qp_uxtoq.o		\
	__dtoul.o		\
	__floatprint.o		\
	__flt_decim.o		\
	__flt_rounds.o		\
	__ftoul.o		\
	__gconvert.o		\
	__quad_mag64.o		\
	__tbl_10_b.o		\
	__tbl_10_h.o		\
	__tbl_10_s.o		\
	__tbl_2_b.o		\
	__tbl_2_h.o		\
	__tbl_2_s.o		\
	__tbl_baselg.o		\
	__tbl_fdq.o		\
	__tbl_ntens.o		\
	__tbl_tens.o		\
	__x_power.o		\
	_base_conv.o		\
	_base_il.o		\
	_base_sup.o		\
	_sprintf_sup.o		\
	_unpack_dble.o		\
	decimal_bin.o		\
	double_decim.o		\
	econvert.o		\
	file_decim.o		\
	finite.o		\
	fp_data.o		\
	fpgetmask.o		\
	fpgetrnd.o		\
	fpgetsticky.o		\
	fpsetrnd.o		\
	fpsetsticky.o		\
	func_decim.o		\
	gconvert.o		\
	ieee_globals.o		\
	isnand.o		\
	isnanf.o		\
	logb.o			\
	pack_float.o		\
	qeconvert.o		\
	scalb.o			\
	seconvert.o		\
	sigfpe.o		\
	string_decim.o		\
	unpack_float.o

GENOBJS=			\
	_getsp.o		\
	_xregs_clrptr.o		\
	abs.o			\
	alloca.o		\
	atof.o			\
	ecvt.o			\
	getctxt.o		\
	gethrtime.o		\
	lconstants.o		\
	ldivide.o		\
	lexp10.o		\
	llabs.o			\
	llog10.o		\
	lmath.o			\
	makectxt.o		\
	memcmp.o		\
	memcpy.o		\
	memmove.o		\
	memset.o		\
	setjmp.o		\
	siginfolst.o		\
	sigsetjmp.o		\
	sparc_data.o		\
	strcmp.o		\
	strcpy.o		\
	strlen.o		\
	strncmp.o		\
	strncpy.o		\
	strlcpy.o		\
	strtod.o		\
	swapctxt.o		\
	sync_instruction_memory.o

GENOBJS_R=			\
	errno.o			\
	sh_locks.o		\
	tsdalloc.o

LWP_MULK=			\
	_lwp_mutex_unlock.o	\
	lock.o

# Preserved solely to ease maintenance of 32-bit and 64-bit library builds
# This macro should ALWAYS be empty; native APIs are already 'large file'.
SYSOBJS64=

SYSOBJS=			\
	$(LWP_MULK)		\
	$(SYSOBJS64)		\
	__clock_timer.o		\
	__fcntl.o		\
	__getloadavg.o		\
	__signotify.o		\
	__sigrt.o		\
	_nfssys.o		\
	_pset.o			\
	_rename.o		\
	_rpcsys.o		\
	_semsys.o		\
	_sigaction.o		\
	_so_accept.o		\
	_so_bind.o		\
	_so_connect.o		\
	_so_getpeername.o	\
	_so_getsockname.o	\
	_so_getsockopt.o	\
	_so_listen.o		\
	_so_recv.o		\
	_so_recvfrom.o		\
	_so_recvmsg.o		\
	_so_send.o		\
	_so_sendmsg.o		\
	_so_sendto.o		\
	_so_setsockopt.o	\
	_so_shutdown.o		\
	_so_socket.o		\
	_so_socketpair.o	\
	_sockconfig.o		\
	access.o		\
	acct.o			\
	acl.o			\
	adjtime.o		\
	alarm.o			\
	brk.o			\
	cladm.o			\
	chdir.o			\
	chmod.o			\
	chown.o			\
	chroot.o		\
	close.o			\
	creat.o			\
	door.o			\
	dup.o			\
	execve.o		\
	exit.o			\
	facl.o			\
	fchdir.o		\
	fchmod.o		\
	fchown.o		\
	fchroot.o		\
	fdsync.o		\
	fork.o			\
	fork1.o			\
	fpathconf.o		\
	fstat.o			\
	fstatfs.o		\
	fstatvfs.o		\
	getdents.o		\
	getegid.o		\
	geteuid.o		\
	getgid.o		\
	getgroups.o		\
	getitimer.o		\
	getmsg.o		\
	getpmsg.o		\
	getppid.o		\
	getrlimit.o		\
	gettimeofday.o		\
	getuid.o		\
	gtty.o			\
	install_utrap.o		\
	ioctl.o			\
	kaio.o			\
	lchown.o		\
	link.o			\
	llseek.o		\
	lseek.o			\
	lstat.o			\
	machlwp.o		\
	memcntl.o		\
	mincore.o		\
	mkdir.o			\
	mknod.o			\
	mmap.o			\
	modctl.o		\
	mount.o			\
	mprotect.o		\
	munmap.o		\
	nice.o			\
	ntp_adjtime.o		\
	ntp_gettime.o		\
	open.o			\
	pathconf.o		\
	pause.o			\
	pcsample.o		\
	pipe.o			\
	poll.o			\
	pread.o			\
	priocntlset.o		\
	profil.o		\
	ptrace.o		\
	putmsg.o		\
	putpmsg.o		\
	pwrite.o		\
	read.o			\
	readlink.o		\
	readv.o			\
	resolvepath.o		\
	rmdir.o			\
	seteguid.o		\
	setgid.o		\
	setgroups.o		\
	setitimer.o		\
	setpgrp.o		\
	setrlimit.o		\
	setsid.o		\
	setuid.o		\
	sigaction.o		\
	sigaltstk.o		\
	signotifywait.o		\
	sigpending.o		\
	sigprocmsk.o		\
	sigsendset.o		\
	sigsuspend.o		\
	sigwait.o		\
	stat.o			\
	statfs.o		\
	statvfs.o		\
	stime.o			\
	stty.o			\
	symlink.o		\
	sync.o			\
	syscall.o		\
	sysconfig.o		\
	sysfs.o			\
	sysinfo.o		\
	syslwp.o		\
	syssun.o		\
	time.o			\
	times.o			\
	uadmin.o		\
	ucontext.o		\
	ulimit.o		\
	umask.o			\
	umount.o		\
	umount2.o		\
	uname.o			\
	unlink.o		\
	ustat.o			\
	utime.o			\
	utimes.o		\
	utssys.o		\
	vfork.o			\
	vhangup.o		\
	wait.o			\
	waitid.o		\
	write.o			\
	writev.o		\
	yield.o

# Preserved solely to ease maintenance of 32-bit and 64-bit library builds
# This macro should ALWAYS be empty; native APIs are already 'large file'.
PORTGEN64=

# objects from source under ../port
# the last two objects in PORTGEN come from derived C source
# DYP added for libc routines that are ifdef'd YP.
#
PORTGEN=			\
	$(PORTGEN64)		\
	_env_data.o		\
	_xftw.o			\
	a64l.o			\
	abort.o			\
	addsev.o		\
	assert.o		\
	atoi.o			\
	atol.o			\
	atoll.o			\
	basename.o		\
	bcmp.o			\
	bcopy.o			\
	bsearch.o		\
	bzero.o			\
	calloc.o		\
	catgets.o		\
	catopen.o		\
	cfgetispeed.o		\
	cfgetospeed.o		\
	cfree.o			\
	cfsetispeed.o		\
	cfsetospeed.o		\
	cftime.o		\
	clock.o			\
	closedir.o		\
	confstr.o		\
	crypt.o			\
	csetlen.o		\
	ctime.o			\
	ctime_r.o		\
	directio.o		\
	dirname.o		\
	div.o			\
	drand48.o		\
	dup2.o			\
	env_data.o		\
	errlst.o		\
	euclen.o		\
	execvp.o		\
	fattach.o		\
	fdetach.o		\
	ffs.o			\
	fmtmsg.o		\
	frexp.o			\
	ftime.o			\
	ftok.o			\
	ftw.o			\
	gcvt.o			\
	getcwd.o		\
	getdate_err.o		\
	getdtblsize.o		\
	getenv.o		\
	getexecname.o		\
	getgrnam.o		\
	gethostid.o		\
	gethostname.o		\
	gethz.o			\
	getloadavg.o		\
	getlogin.o		\
	getmntent.o		\
	getnetgrent.o		\
	getopt.o		\
	getpagesize.o		\
	getpw.o			\
	getpwnam.o		\
	getrusage.o		\
	getspent.o		\
	getsubopt.o		\
	gettxt.o		\
	getusershell.o		\
	getut.o			\
	getutx.o		\
	getvfsent.o		\
	getwd.o			\
	getwidth.o		\
	getxby_door.o		\
	gtxt.o			\
	hsearch.o		\
	iconv.o			\
	import_def.o		\
	index.o			\
	initgroups.o		\
	insque.o		\
	isaexec.o		\
	isastream.o		\
	isatty.o		\
	killpg.o		\
	l64a.o			\
	lckpwdf.o		\
	ldexp.o			\
	lfind.o			\
	lfmt.o			\
	lfmt_log.o		\
	lldiv.o			\
	lltostr.o		\
	lsearch.o		\
	madvise.o		\
	malloc.o		\
	memalign.o		\
	memccpy.o		\
	memchr.o		\
	mkdev.o			\
	mkfifo.o		\
	mkstemp.o		\
	mktemp.o		\
	mlock.o			\
	mlockall.o		\
	modf.o			\
	modff.o			\
	mon.o			\
	msync.o			\
	munlock.o		\
	munlockall.o		\
	ndbm.o			\
	new_list.o		\
	nftw.o			\
	nsparse.o		\
	nss_common.o		\
	nss_dbdefs.o		\
	nss_deffinder.o		\
	opendir.o		\
	opt_data.o		\
	perror.o		\
	pfmt.o			\
	pfmt_data.o		\
	pfmt_print.o		\
	plock.o			\
	priocntl.o		\
	psiginfo.o		\
	psignal.o		\
	pt.o			\
	putenv.o		\
	putpwent.o		\
	putspent.o		\
	qsort.o			\
	raise.o			\
	rand.o			\
	random.o		\
	readdir.o		\
	realpath.o		\
	reboot.o		\
	regexpr.o		\
	rename.o		\
	rewinddir.o		\
	rindex.o		\
	scrwidth.o		\
	seekdir.o		\
	select.o		\
	setlabel.o		\
	setpriority.o		\
	setreid.o		\
	settimeofday.o		\
	sigflag.o		\
	siglist.o		\
	signal.o		\
	sigsend.o		\
	sigsetops.o		\
	sleep.o			\
	ssignal.o		\
	str2sig.o		\
	strcat.o		\
	strchr.o		\
	strcspn.o		\
	strdup.o		\
	strerror.o		\
	stricmp.o		\
	strncat.o		\
	strlcat.o		\
	strpbrk.o		\
	strrchr.o		\
	strsignal.o		\
	strspn.o		\
	strstr.o		\
	strtok.o		\
	strtol.o		\
	strtoll.o		\
	strtoul.o		\
	strtoull.o		\
	swab.o			\
	swapctl.o		\
	sysconf.o		\
	syslog.o		\
	tcdrain.o		\
	tcflow.o		\
	tcflush.o		\
	tcgetattr.o		\
	tcgetpgrp.o		\
	tcgetsid.o		\
	tcsendbreak.o		\
	tcsetattr.o		\
	tcsetpgrp.o		\
	tell.o			\
	telldir.o		\
	tfind.o			\
	time_comm.o		\
	time_data.o		\
	time_gdata.o		\
	truncate.o		\
	tsd.o			\
	syncinit.o		\
	tsearch.o		\
	ttyname.o		\
	ttyslot.o		\
	ualarm.o		\
	usleep.o		\
	valloc.o		\
	vlfmt.o			\
	vpfmt.o			\
	wait3.o			\
	wait4.o			\
	waitpid.o		\
	wdata.o			\
	xgetwidth.o		\
	xpg4.o			\
	yp_stubs.o

PORTGEN_R=			\
	getgrnam_r.o		\
	getpwnam_r.o		\
	getspent_r.o		\
	rand_r.o		\
	readdir_r.o

# GENTXTS = portable source unused because of sparc-specific source, see TXTS
GENTXTS=			\
	abs.c			\
	atof.c			\
	ecvt.c			\
	llabs.c			\
	memcmp.c		\
	memcpy.c		\
	memmove.c		\
	memset.c		\
	strcmp.c		\
	strcpy.c		\
	strlen.c		\
	strncmp.c		\
	strncpy.c		\
	strtod.c

PORTPRINT_W=			\
	doprnt_w.o

PORTPRINT=			\
	$(PORTPRINT_W)		\
	doprnt.o		\
	fprintf.o		\
	printf.o		\
	snprintf.o		\
	sprintf.o		\
	vfprintf.o		\
	vprintf.o		\
	vsnprintf.o		\
	vsprintf.o		\
	wprintf.o

# Preserved solely to ease maintenance of 32-bit and 64-bit library builds
# This macro should ALWAYS be empty; native APIs are already 'large file'.
PORTSTDIO64=

PORTSTDIO_W=			\
	doscan_w.o

PORTSTDIO=			\
	$(PORTSTDIO64)		\
	$(PORTSTDIO_W)		\
	__extensions.o		\
	_endopen.o		\
	_filbuf.o		\
	_findbuf.o		\
	_flsbuf.o		\
	_wrtchk.o		\
	clearerr.o		\
	ctermid.o		\
	cuserid.o		\
	data.o			\
	doscan.o		\
	fdopen.o		\
	feof.o			\
	ferror.o		\
	fgetc.o			\
	fgets.o			\
	fileno.o		\
	flush.o			\
	fopen.o			\
	fpos.o			\
	fputc.o			\
	fputs.o			\
	fread.o			\
	fseek.o			\
	fseeko.o		\
	ftell.o			\
	ftello.o		\
	fwrite.o		\
	getc.o			\
	getchar.o		\
	getpass.o		\
	gets.o			\
	getw.o			\
	popen.o			\
	putc.o			\
	putchar.o		\
	puts.o			\
	putw.o			\
	rewind.o		\
	scanf.o			\
	setbuf.o		\
	setbuffer.o		\
	setvbuf.o		\
	system.o		\
	tempnam.o		\
	tmpfile.o		\
	tmpnam.o		\
	ungetc.o		\
	mse.o			\
	wscanf.o

PORTSTDIO_R=			\
	ctermid_r.o		\
	flockf.o		\
	tmpnam_r.o

PORTI18N=			\
	$(CHARMAP)		\
	$(COLLATE)		\
	$(CTYPE)		\
	$(LOCALE)		\
	$(MONETARY)		\
	$(REGEX)		\
	$(SETLOCALE)		\
	$(TIME)			\
	$(WIDEC)		\
	$(INTL)

WIDEC=				\
	fgetws.o		\
	fputwc.o		\
	fputws.o		\
	getwchar.o		\
	getws.o			\
	putwchar.o		\
	putws.o			\
	strtows.o		\
	ungetwc.o		\
	wcstoul.o		\
	wcswcs.o		\
	wscasecmp.o		\
	wscat.o			\
	wschr.o			\
	wscmp.o			\
	wscol.o			\
	wscpy.o			\
	wscspn.o		\
	wsdup.o			\
	wslen.o			\
	wsncasecmp.o		\
	wsncat.o		\
	wsncmp.o		\
	wsncpy.o		\
	wspbrk.o		\
	wsprintf.o		\
	wsrchr.o		\
	wsscanf.o		\
	wsspn.o			\
	wstod.o			\
	wstok.o			\
	wstol.o			\
	wstoll.o		\
	wsxfrm.o		\
	wmemchr.o		\
	wmemcmp.o		\
	wmemcpy.o		\
	wmemmove.o		\
	wmemset.o		\
	wcsstr.o		\
	fwide.o

INTL=				\
	gettext.o		\
	wdresolve.o

COLLATE=			\
	__fnmatch_sb.o		\
	__fnmatch_std.o		\
	__strcoll_sb.o		\
	__strcoll_std.o		\
	__strxfrm_sb.o		\
	__strxfrm_std.o		\
	__wcscoll_bc.o		\
	__wcscoll_std.o		\
	__wcsxfrm_bc.o		\
	__wcsxfrm_std.o		\
	back_pos.o		\
	backward.o		\
	colval.o		\
	fnmatch.o		\
	forw_pos.o		\
	forward.o		\
	replacement.o		\
	strcoll.o		\
	strxfrm.o		\
	wcscoll.o		\
	wcsxfrm.o

TIME=				\
	strftime.o		\
	strptime.o		\
	wcsftime.o

REGEX=				\
	__regcomp_C.o		\
	__regcomp_std.o		\
	__regerror_std.o	\
	__regexec_C.o		\
	__regexec_std.o		\
	__regfree_std.o		\
	regcomp.o		\
	regerror.o		\
	regexec.o		\
	regfree.o

SETLOCALE=			\
	_ctype.o		\
	loadtab.o		\
	loc_setup.o		\
	setlocale.o

LOCALE=				\
	__localeconv_std.o	\
	__nl_langinfo_std.o	\
	localeconv.o		\
	nl_langinfo.o

MONETARY=			\
	__strfmon_std.o		\
	strfmon.o

CTYPE=				\
	__iswctype_bc.o		\
	__iswctype_sb.o		\
	__iswctype_std.o	\
	__towctrans_bc.o	\
	__towlower_bc.o		\
	__towupper_bc.o		\
	_tolower.o		\
	_toupper.o		\
	_trwctype.o		\
	fgetwc.o		\
	get_lcbind.o		\
	isalnum.o		\
	isalpha.o		\
	isascii.o		\
	iscntrl.o		\
	isdigit.o		\
	isenglish.o		\
	isgraph.o		\
	isideogram.o		\
	islower.o		\
	isnumber.o		\
	isphonogram.o		\
	isprint.o		\
	ispunct.o		\
	isspace.o		\
	isspecial.o		\
	isupper.o		\
	iswalnum.o		\
	iswalpha.o		\
	iswcntrl.o		\
	iswctype.o		\
	iswdigit.o		\
	iswgraph.o		\
	iswlower.o		\
	iswprint.o		\
	iswpunct.o		\
	iswspace.o		\
	iswupper.o		\
	iswxdigit.o		\
	isxdigit.o		\
	toascii.o		\
	tolower.o		\
	toupper.o		\
	towctrans.o		\
	towlower.o		\
	towupper.o		\
	wctrans.o		\
	wctype.o

CHARMAP=			\
	__btowc_dense.o		\
	__btowc_euc.o		\
	__btowc_sb.o		\
	__eucpctowc_gen.o	\
	__fgetwc_dense.o	\
	__fgetwc_sb.o		\
	__mbftowc_dense.o	\
	__mbftowc_sb.o		\
	__mblen_gen.o		\
	__mblen_sb.o		\
	__mbsinit_gen.o		\
	__mbsinit_sb.o		\
	__mbrlen_gen.o		\
	__mbrlen_sb.o		\
	__mbrtowc_dense.o	\
	__mbrtowc_euc.o		\
	__mbrtowc_sb.o		\
	__mbsrtowcs_dense.o	\
	__mbsrtowcs_euc.o	\
	__mbsrtowcs_sb.o	\
	__mbstowcs_dense.o	\
	__mbstowcs_euc.o	\
	__mbstowcs_sb.o		\
	__mbtowc_dense.o	\
	__mbtowc_euc.o		\
	__mbtowc_sb.o		\
	__wcrtomb_dense.o	\
	__wcrtomb_euc.o		\
	__wcrtomb_sb.o		\
	__wcsrtombs_dense.o	\
	__wcsrtombs_euc.o	\
	__wcsrtombs_sb.o	\
	__wcstombs_dense.o	\
	__wcstombs_euc.o	\
	__wcstombs_sb.o		\
	__wcswidth_dense.o	\
	__wcswidth_euc.o	\
	__wcswidth_sb.o		\
	__wctob_dense.o		\
	__wctob_euc.o		\
	__wctob_sb.o		\
	__wctoeucpc_gen.o	\
	__wctomb_dense.o	\
	__wctomb_euc.o		\
	__wctomb_sb.o		\
	__wcwidth_dense.o	\
	__wcwidth_euc.o		\
	__wcwidth_sb.o		\
	btowc.o			\
	euc_info.o		\
	mbftowc.o		\
	mblen.o			\
	mbrlen.o		\
	mbrtowc.o		\
	mbsinit.o		\
	mbsrtowcs.o		\
	mbstowcs.o		\
	mbtowc.o		\
	wcrtomb.o		\
	wcsrtombs.o		\
	wcstombs.o		\
	wcswidth.o		\
	wctob.o			\
	wctomb.o		\
	wcwidth.o

STUBOBJS=			\
	thr_stub.o

# Preserved solely to ease maintenance of 32-bit and 64-bit library builds
# This macro should ALWAYS be empty; native APIs are already 'large file'.
PORTSYS64=

PORTSYS=			\
	$(PORTSYS64)		\
	bsd_signal.o		\
	corectl.o		\
	execl.o			\
	execle.o		\
	execv.o			\
	fcntl.o			\
	fsync.o			\
	libc_link.o		\
	libc_open.o		\
	lockf.o			\
	lwp.o			\
	lwp_cond.o		\
	msgsys.o		\
	nfssys.o		\
	p_online.o		\
	posix_sigwait.o		\
	processor_bind.o	\
	processor_info.o	\
	psetsys.o		\
	putmsgxpg.o		\
	sbrk.o			\
	semsys.o		\
	shmsys.o		\
	siginterrupt.o		\
	sigstack.o

PORTREGEX=			\
	glob.o			\
	regcmp.o		\
	regex.o			\
	wordexp.o


# XXX - Note: the lists of FIRSTOBJS and LASTOBJS are taken directly
#	from the original AT&T source makefile. This ordering might
#	not have made any sense for a 3b2 and it is even less likely
#	to make sense for SunOS/sparc. The lists are maintained (for
#	now) to maintain the mechanism. One should also consider that
#	the ordering which might optimize search time for a static
#	archive probably has no relationship to the ordering which
#	will minimize memory usage for a dynamic library.
FIRSTOBJS=			\
	atexit.o

REENTRANTOBJS=			\
	$(GENOBJS_R)		\
	$(PORTGEN_R)		\
	$(PORTSTDIO_R)		\
	$(STUBOBJS)

MOSTOBJS=			\
	$(CRTOBJS)		\
	$(DYNOBJS)		\
	$(FPOBJS)		\
	$(GENOBJS)		\
	$(PORTGEN)		\
	$(PORTI18N)		\
	$(PORTPRINT)		\
	$(PORTREGEX)		\
	$(PORTSTDIO)		\
	$(PORTSYS)		\
	$(REENTRANTOBJS)	\
	$(SYSOBJS)

LASTOBJS=			\
	cerror.o		\
	cuexit.o		\
	fpsetmask.o		\
	getpid.o		\
	kill.o

OBJECTS= $(FIRSTOBJS) $(MOSTOBJS) $(LASTOBJS)

# libc build rules
objs/%.o profs/%.o pic_profs/%.o pics/%.o: crt/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: crt/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: gen/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: fp/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: fp/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../sparc/fp/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../sparc/fp/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: sys/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../sparc/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../sparc/sys/%.s
	$(BUILD.s)
	$(POST_PROCESS_O)

# rules to build large file aware objects (xxx64.o from xxx.c)

#objs/%64.o profs/%64.o pic_profs/%64.o pics/%64.o: sys/%.s
#	$(BUILD.s)
#	$(POST_PROCESS_O)

#objs/%64.o profs/%64.o pic_profs/%64.o pics/%64.o: gen/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

#objs/%64.o profs/%64.o pic_profs/%64.o pics/%64.o: ../port/gen/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

#objs/%64.o profs/%64.o pic_profs/%64.o pics/%64.o: ../port/sys/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

#objs/%64.o profs/%64.o pic_profs/%64.o pics/%64.o: ../port/print/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

#objs/%64.o profs/%64.o pic_profs/%64.o pics/%64.o: ../port/stdio/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

#objs/%64.o profs/%64.o pic_profs/%64.o pics/%64.o: ../port/stubs/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

objs/%_w.o profs/%_w.o pic_profs/%_w.o pics/%_w.o: ../port/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%_w.o profs/%_w.o pic_profs/%_w.o pics/%_w.o: ../port/print/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#
# libc build rules for objects built from "portable" source in ../port

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/i18n/%.c
	$(COMPILE.c) \
	-I../port/i18n -o $@ $<
	$(POST_PROCESS_O)

# gen rules
objs/%.o profs/%.o pic_profs/%.o pics/%.o %.o: ../port/gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# intl rules
#objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/intl/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O)

# print rules
objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/print/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# regex rules
objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/regex/%.c
	$(COMPILE.c) -DM_I18N_MB -DI18N \
	-I../port/regex \
	-I../port/gen -o $@ $<
	$(POST_PROCESS_O)

# stdio rules
objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/stubs/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# widec rules
#objs/%.o profs/%.o pic_profs/%.o pics/%.o: ../port/widec/%.c
#	$(COMPILE.c) \
#	-I../port/widec \
#	-I../port/gen -o $@ $<
#        $(POST_PROCESS_O)

# special case for widec
objs/wstod.o pic_profs/wstod.o pics/wstod.o profs/wstod.o := CPPFLAGS += -I../sparc/fp

# include common library definitions and 64-bit overrides
include ../../Makefile.lib
include ../../Makefile.lib.64

MAPDIR=		../spec/sparcv9
MAPFILE=	$(MAPDIR)/mapfile

CPPFLAGS=	-D_REENTRANT -Dsparc -Iinc -I../inc $(CPPFLAGS.master)
ASDEFS=		-D__STDC__ -D_ASM
$(PROFS) :=	ASDEFS += -DPROF
ASDEFS +=	$(CPPFLAGS)
ASFLAGS=	-P $(ASDEFS) $(sparcv9_XARCH) -D__sparcv9
DYNFLAGS +=	-M $(MAPFILE) \
		-Wl,-f/usr/platform/\$$PLATFORM/lib/sparcv9/$(DYNLIBPSR)
LDLIBS +=	-ldl

BUILD.s=	$(AS) $(ASFLAGS) $< -o $@

# libc method of building an archive, using AT&T ordering
BUILD.AR= $(RM) $@ ; \
	$(AR) q $@ $(FIRSTOBJS:%=$(DIR)/%) && \
	$(AR) q $@ `$(LORDER) $(MOSTOBJS:%=$(DIR)/%)| $(TSORT)` && \
	$(AR) q $@ $(LASTOBJS:%=$(DIR)/%)

# extra files for the clean target
CLEANFILES=			\
	../port/gen/errlst.c	\
	../port/gen/new_list.c	\
	assym.s			\
	crt/_rtld.s		\
	$(VALUES)

CLOBBERFILES +=	$(MAPFILE)

# the TXTS macro is used for NSE bootstrapping.
TXTS=				\
	inc/SYS.h		\
	inc/PIC.h		\
	$(GENTXTS:%=../port/gen/%)

# list of C source for lint
SRCS=						\
	$(PORTGEN:%.o=../port/gen/%.c)		\
	$(PORTI18N:%.o=../port/i18n/%.c)	\
	$(PORTPRINT:%.o=../port/print/%.c)	\
	$(PORTREGEX:%.o=../port/regex/%.c)	\
	$(PORTSTDIO:%.o=../port/stdio/%.c)	\
	$(PORTSYS:%.o=../port/sys/%.c)		\
	../port/gen/atexit.c			\
	gen/atof.c				\
	gen/ecvt.c				\
	gen/getctxt.c				\
	gen/lconstants.c			\
	gen/ldivide.c				\
	gen/lexp10.c				\
	gen/llog10.c				\
	gen/lmul.c				\
	gen/makectxt.c				\
	gen/siginfolst.c			\
	gen/sigsetjmp.c				\
	gen/strtod.c				\
	gen/swapctxt.c				\
	sys/_signal.c				\
	sys/sigaction.c

# conditional assignments
# $(DYNLIB) $(LIB_PIC):= DYNOBJS = _rtbootld.o
pic_profs/%.o pics/%.o:=  ASFLAGS += -K PIC

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# Files in fp subdirectory which need base.il inline template
IL=				\
	__floatprint.o		\
	__gconvert.o		\
	__tbl_fdq.o		\
	__x_power.o		\
	_base_conv.o		\
	_base_sup.o		\
	_sprintf_sup.o		\
	decimal_bin.o		\
	double_decim.o

$(IL:%=objs/%) $(IL:%=pic_profs/%) $(IL:%=pics/%) $(IL:%=profs/%) := CFLAGS64 += fp/base.il

# Files in fp subdirectory which need __quad.il inline template
QIL=				\
	_Q_add.o		\
	_Q_cmp.o		\
	_Q_cmpe.o		\
	_Q_div.o		\
	_Q_dtoq.o		\
	_Q_fcc.o		\
	_Q_mul.o		\
	_Q_qtod.o		\
	_Q_qtoi.o		\
	_Q_qtos.o		\
	_Q_qtou.o		\
	_Q_sqrt.o		\
	_Q_stoq.o		\
	_Q_sub.o		\
	_Qp_qtox.o		\
	_Qp_qtoux.o

$(QIL:%=objs/%) $(QIL:%=pic_profs/%) $(QIL:%=pics/%) $(QIL:%=profs/%) := CFLAGS64 += ../sparc/fp/__quad.il
objs/_Qp%.o pic_profs/_Qp%.o pics/_Qp%.o profs/_Qp%.o := CFLAGS64 += -I../sparc/fp
objs/_Q%.o pic_profs/_Q%.o pics/_Q%.o profs/_Q%.o := sparcv9_COPTFLAG = -xO4 -xchip=ultra

# objs/mul64.o pic_profs/mul64.o pics/mul64.o profs/mul64.o := CFLAGS += crt/mul64.il

# large-file-aware components that should be built large.

#$(SYSOBJS64:%=objs/%) $(SYSOBJS64:%=pic_profs/%) $(SYSOBJS64:%=pics/%) \
#	$(SYSOBJS64:%=profs/%) := CPPFLAGS += -D_LARGEFILE_SOURCE \
#	-D_FILE_OFFSET_BITS=64

#$(PORTGEN64:%=objs/%) $(PORTGEN64:%=pic_profs/%) $(PORTGEN64:%=pics/%) \
#	$(PORTGEN64:%=profs/%) := CPPFLAGS += -D_LARGEFILE_SOURCE \
#	-D_FILE_OFFSET_BITS=64

#$(PORTSTDIO64:%=objs/%) $(PORTSTDIO64:%=pic_profs/%) $(PORTSTDIO64:%=pics/%) \
#	$(PORTSTDIO64:%=profs/%) := CPPFLAGS += -D_LARGEFILE_SOURCE \
#	-D_FILE_OFFSET_BITS=64

#$(PORTSYS64:%=objs/%) $(PORTSYS64:%=pic_profs/%) $(PORTSYS64:%=pics/%) \
#	$(PORTSYS64:%=profs/%) := CPPFLAGS += -D_LARGEFILE_SOURCE \
#	-D_FILE_OFFSET_BITS=64

$(PORTSTDIO_W:%=objs/%) $(PORTSTDIO_W:%=pic_profs/%) $(PORTSTDIO_W:%=pics/%) \
	$(PORTSTDIO_W:%=profs/%) := CPPFLAGS += -D_WIDE

$(PORTPRINT_W:%=objs/%) $(PORTPRINT_W:%=pic_profs/%) $(PORTPRINT_W:%=pics/%) \
	$(PORTPRINT_W:%=profs/%) := CPPFLAGS += -D_WIDE


.KEEP_STATE:

all: $(TXTS) $(LIBS) $(PLIB) $(LIB_PIC) values

values: $(VALUES)

$(LINTLIB):= SRCS=../port/llib-lc
$(LINTLIB):= LINTFLAGS64=-nvx -D__sparcv9
$(LINTLIB):= TARGET_ARCH=

# object files that depend on inline template.
$(IL:%=objs/%) $(IL:%=pic_profs/%) $(IL:%=pics/%) $(IL:%=profs/%): ../sparc/fp/base.il
$(QIL:%=objs/%) $(QIL:%=pic_profs/%) $(QIL:%=pics/%) $(QIL:%=profs/%): ../sparc/fp/__quad.il
#objs/mul64.o pic_profs/mul64.o pics/mul64.o profs/mul64.o: crt/mul64.il

# include common library targets
include ../../Makefile.targ

pics/getxby_door.o := CCMODE =
pic_profs/getxby_door.o := CCMODE =

$(LIB_PIC) := DIR = pics
$(LIB_PIC): pics $$(PICS)
	$(BUILD.AR)
	$(POST_PROCESS_A)


# special cases
#$(STRETS:%=objs/%) \
#$(STRETS:%=profs/%) \
#$(STRETS:%=pic_profs/%) \
#$(STRETS:%=pics/%): crt/stret.s
#	$(AS) $(ASFLAGS) -DSTRET$(@F:stret%.o=%) crt/stret.s -o $@
#	$(POST_PROCESS_O)

#crt/_div.s:	crt/divrem.m4.s
#	$(M4) -DANSWER=quotient crt/divrem.m4.s > $@

#crt/_rem.s:	crt/divrem.m4.s
#	$(M4) -DANSWER=remainder crt/divrem.m4.s > $@

# delete this stuff - libc-is-not-an-interpreter
#crt/_rtbootld.s:	crt/_rtboot.s crt/_rtld.c
#	$(CC) $(CPPFLAGS) $(CFLAGS) -xO0 -S crt/_rtld.c -o crt/_rtld.s
#	$(CAT) crt/_rtboot.s crt/_rtld.s > $@
#	$(RM) crt/_rtld.s

../port/stubs/thr_stub.c:	../port/stubs/thr_stub.m4
	$(M4) ../port/stubs/thr_stub.m4 > $@

$(LWP_MULK:%=objs/%) \
$(LWP_MULK:%=profs/%) \
$(LWP_MULK:%=pic_profs/%) \
$(LWP_MULK:%=pics/%): assym.s

# XX64 WRONG (this is building in ILP32 structure offsets!)

assym.s: genassym
	./genassym > $@

# XXX	A hack.  Perhaps this should be 'CPPFLAGS.native' and
#	live in Makefile.master

CPPFLAGS.genassym = \
$(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)

genassym: genassym.c
	$(NATIVECC) -Dsparcv9 -Iinc $(CPPFLAGS.genassym) -o $@ genassym.c

# derived C source and related explicit dependencies
#
../port/gen/errlst.c + \
../port/gen/new_list.c: ../port/gen/errlist ../port/gen/errlist.awk
	cd ../port/gen; pwd; $(AWK) -f errlist.awk < errlist

objs/errlst.o \
pics/errlst.o \
pic_profs/errlst.o \
profs/errlst.o: ../port/gen/errlst.c

objs/new_list.o \
pics/new_list.o \
pic_profs/new_list.o \
profs/new_list.o: ../port/gen/new_list.c
