#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.121	99/07/28 SMI"
#
# lib/libc/i386/Makefile
#
# This builds all objects needed for libc, including those derived
# from source under ../port.  crt0.o and friends belong to the compiler.
#
LIBRARY= libc.a
LIB_PIC= libc_pic.a
VERS=	.1
M4=	m4
CPP=	/usr/lib/cpp

# values deliverables
VALUES = values-Xa.o values-Xc.o values-Xs.o values-Xt.o values-xpg4.o

# objects are grouped by source directory, except for the few objects
# requiring special treatment in the ordering of the library.  These
# few are found in the FIRSTOBJS and LASTOBJS macros.

# local objects
#
STRETS= # stret1.o stret2.o stret4.o

CRTOBJS= \
$(STRETS)	_ftou.o

DYNOBJS=

FPOBJS= \
finite.o	fp_data.o	fpgetmask.o	fpgetround.o	fpgetsticky.o \
fpsetround.o	fpsetsticky.o	fpstart.o	isnand.o	isnanf.o \
logb.o		scalb.o 	sigfpe.o  \
__floatprint.o	__flt_decim.o	__flt_rounds.o	__gconvert.o	__tbl_10_b.o \
__tbl_10_h.o	__tbl_10_s.o	__tbl_2_b.o	__tbl_2_h.o	__tbl_2_s.o \
__tbl_baselg.o	__tbl_fdq.o	__tbl_ntens.o	__tbl_tens.o	__x_power.o \
_base_il.o	_base_il4.o	_base_sup.o	_sprintf_sup.o	_unpack_dble.o \
decimal_bin.o	double_decim.o	econvert.o	file_decim.o	func_decim.o \
gconvert.o	ieee_globals.o	pack_float.o \
qeconvert.o	seconvert.o	string_decim.o	unpack_float.o	__swapFLAGS.o \
_xtoll.o	_xtoull.o	__xgetRD.o	fpcw.o		exception.o

GENOBJS= \
_div64.o	_mul64.o	\
abs.o         alloca.o	atof.o  \
byteorder.o   ecvt.o        gethrtime.o	  ladd.o        lconstants.o  \
ldivide.o     lexp10.o      llog10.o	  lmul.o        lshiftl.o  \
lsign.o       lsub.o        ltostr.o      memcmp.o  \
memcpy.o      memmove.o	    memset.o      setjmp.o  \
siglongjmp.o  sigsetjmp.o   i386_data.o  strcmp.o      strcpy.o  \
strlen.o      strncmp.o     strncpy.o     strlcpy.o	strtod.o siginfolst.o  \
sync_instruction_memory.o   _getsp.o      swapctxt.o  \
makectxt.o    ldt.o

GENOBJS_R= \
errno.o       sh_locks.o    tsdalloc.o

ASSYMDEP_OBJS= \
lwp_mutex_unlock.o getcontext.o

# sysobjs that contain large-file interfaces that are ifdef'ed
SYSOBJS64= \
creat64.o	fstat64.o	fstatvfs64.o	getdents64.o \
lstat64.o	lseek64.o	open64.o	mmap64.o \
getrlimit64.o	setrlimit64.o	stat64.o	statvfs64.o \
pread64.o	pwrite64.o

SYSOBJS= \
__clock_timer.o	\
__fcntl.o	\
__getloadavg.o	\
__signotify.o	\
__sigrt.o	\
__uadmin.o	\
_nfssys.o	\
_rpcsys.o	\
_pset.o		\
_semsys.o	\
_rename.o      _sigaction.o  \
access.o       acl.o          facl.o  \
acct.o         adjtime.o      alarm.o        brk.o          chdir.o \
chmod.o  \
chown.o        chroot.o       cladm.o \
close.o        creat.o        door.o  \
dup.o	\
execve.o       exit.o         fchdir.o       fchmod.o       fchown.o  \
fork.o         fork1.o	      fpathconf.o    fstat.o \
fxstat.o       fstatfs.o      fstatvfs.o     fdsync.o       getdents.o \
getegid.o      geteuid.o      getgid.o       getgroups.o    getmsg.o  \
getpmsg.o      getppid.o      getrlimit.o    getuid.o       gtty.o  \
ioctl.o        lchown.o       link.o         llseek.o	    lseek.o \
lstat.o        lxstat.o	      memcntl.o      mincore.o	    mkdir.o \
mknod.o        mmap.o         modctl.o       mount.o        mprotect.o \
munmap.o       nice.o         ntp_gettime.o  ntp_adjtime.o  nuname.o \
open.o         pathconf.o     pause.o	     pcsample.o     pipe.o \
poll.o \
priocntlset.o  profil.o       pread.o        ptrace.o       pwrite.o  \
kaio.o         putmsg.o       putpmsg.o      read.o         readlink.o  \
readv.o        resolvepath.o  rmdir.o        seteguid.o  \
setgid.o       setgroups.o    setpgrp.o      setrlimit.o    setsid.o  \
setuid.o       sigaction.o    sigaltstk.o                   sigpending.o  \
sigprocmsk.o   sigsendset.o   sigsuspend.o   stat.o         xstat.o \
statfs.o       statvfs.o      stime.o        stty.o         symlink.o \
sync.o         syscall.o      sysconfig.o    sysfs.o        sysinfo.o \
sysi86.o       time.o         times.o        uadmin.o       ucontext.o \
ulimit.o       umask.o        umount.o       umount2.o      uname.o \
unlink.o \
ustat.o        utime.o        utssys.o       vfork.o        wait.o \
waitid.o       write.o        writev.o       xmknod.o	    xstat.o \
utimes.o       fchroot.o      vhangup.o      gettimeofday.o getitimer.o \
setitimer.o    syslwp.o	      lock.o	     sigwait.o	    signotifywait.o \
machlwp.o      yield.o	      lwp_cond_signal.o  lwp_cond_broadcast.o \
install_utrap.o	\
$(ASSYMDEP_OBJS) $(SYSOBJS64) \
_so_socket.o   _so_socketpair.o	_so_bind.o   _so_listen.o   _so_accept.o \
_so_connect.o  _so_shutdown.o _so_recv.o     _so_recvfrom.o _so_recvmsg.o \
_so_send.o     _so_sendmsg.o  _so_sendto.o   _so_getpeername.o \
_so_getsockname.o _so_getsockopt.o _so_setsockopt.o  _sockconfig.o


# objects under ../port which contain transitional large file interfaces

PORTGEN64= \
_xftw64.o	ftw64.o		mkstemp64.o	nftw64.o	tell64.o \
truncate64.o

# objects from source under ../port
# the last two objects in PORTGEN come from derived C source
# DYP added for libc routines that are ifdef'd YP.
#
PORTGEN= \
_env_data.o \
_xftw.o \
a64l.o		abort.o		addsev.o	assert.o  \
atoi.o          atol.o \
basename.o \
bcmp.o		bcopy.o		bsearch.o	bzero.o	\
calloc.o        catgets.o       catopen.o	cfgetispeed.o   cfgetospeed.o \
cfree.o         cfsetispeed.o   cfsetospeed.o	cftime.o        clock.o \
closedir.o	confstr.o       crypt.o \
csetlen.o 	ctime.o \
directio.o \
dirname.o	div.o		drand48.o  \
dup2.o          env_data.o  \
		euclen.o        execvp.o        fattach.o       fdetach.o  \
ffs.o           fmtmsg.o        frexp.o         ftime.o		ftok.o \
ftw.o           gcvt.o          getcwd.o        getdate_err.o  \
getdtblsize.o	\
getenv.o	getexecname.o \
getgrnam.o	gethostid.o	gethostname.o	gethz.o	\
getloadavg.o	\
getlogin.o  \
getmntent.o     getnetgrent.o   getopt.o	getpagesize.o	getpw.o	\
getpwnam.o	getrusage.o \
getspent.o      getsubopt.o     gettxt.o        getusershell.o \
getwidth.o \
getut.o         getutx.o        getvfsent.o     getwd.o		\
getxby_door.o	gtxt.o \
hsearch.o       iconv.o		import_def.o    index.o		\
initgroups.o	insque.o        isaexec.o	\
isastream.o     isatty.o        killpg.o \
l64a.o  \
lckpwdf.o       ldexp.o         lfind.o \
lfmt.o		lfmt_log.o \
lsearch.o  \
madvise.o       malloc.o        memalign.o \
memccpy.o       memchr.o                                        \
mkdev.o         mkfifo.o        mktemp.o        mkstemp.o       mlock.o \
mlockall.o  \
modf.o          modff.o         mon.o           msync.o         munlock.o  \
munlockall.o    ndbm.o          nftw.o          \
opendir.o       opt_data.o      perror.o   \
pfmt.o		pfmt_data.o	pfmt_print.o \
plock.o		priocntl.o      psiginfo.o 	putspent.o \
psignal.o       pt.o            putenv.o        putpwent.o      qsort.o  \
raise.o		rand.o		random.o	readdir.o	realpath.o \
reboot.o	regexpr.o	\
rename.o        rewinddir.o	rindex.o	scrwidth.o	seekdir.o \
select.o	select_large_fdset.o	setlabel.o \
setpriority.o	setreid.o \
sigflag.o	siglist.o	signal.o \
sigsend.o       sigsetops.o     sleep.o         ssignal.o  \
str2sig.o       strcat.o        strchr.o        settimeofday.o \
strcspn.o       strdup.o        strerror.o      \
stricmp.o       strncat.o 	strlcat.o                       strpbrk.o  \
strrchr.o       strsignal.o     strspn.o        strstr.o        strtok.o  \
strtol.o        strtoul.o	swab.o \
swapctl.o  \
sysconf.o       syslog.o        tcdrain.o       tcflow.o        tcflush.o  \
tcgetattr.o     tcgetpgrp.o     tcgetsid.o      tcsendbreak.o   tcsetattr.o  \
tcsetpgrp.o     tell.o          telldir.o       tfind.o         time_comm.o  \
time_data.o     time_gdata.o    truncate.o  \
tsd.o syncinit.o \
tsearch.o       ttyname.o       ttyslot.o	ualarm.o	usleep.o \
valloc.o        waitpid.o  \
wait3.o		wait4.o	\
errlst.o        new_list.o      \
nss_common.o    nss_dbdefs.o	nss_deffinder.o	nsparse.o	\
vlfmt.o		vpfmt.o         xpg4.o \
xgetwidth.o     wdata.o         yp_stubs.o  \
atoll.o		lltostr.o	\
llabs.o		lldiv.o		strtoll.o \
strtoull.o \
$(PORTGEN64)

PORTGEN_R= \
getgrnam_r.o	getpwnam_r.o	getspent_r.o	\
ctime_r.o       rand_r.o        readdir_r.o

# GENTXTS = portable source unused because of sparc-specific source, see TXTS
GENTXTS= \
abs.c     atof.c    ecvt.c     memcmp.c   memcpy.c  memmove.c	memset.c  \
strcmp.c  strcpy.c  strlen.c  strncmp.c  strncpy.c  strtod.c

PORTPRINT_W=			\
	doprnt_w.o

PORTPRINT= \
$(PORTPRINT_W) \
doprnt.o \
fprintf.o  printf.o sprintf.o vfprintf.o  vprintf.o  vsprintf.o \
snprintf.o vsnprintf.o wprintf.o

# portable stdio objects that contain transitional large file interfaces.
# fopen64 is a special case because we build it small.

PORTSTDIO64=	fopen64.o	fpos64.o

PORTSTDIO_W=			\
	doscan_w.o

PORTSTDIO= \
$(PORTSTDIO_W) \
_filbuf.o   _findbuf.o  _flsbuf.o   _wrtchk.o   clearerr.o  \
_endopen.o \
__extensions.o \
ctermid.o   cuserid.o   data.o      doscan.o    fdopen.o    feof.o  \
ferror.o    fgetc.o     fgets.o     fileno.o    flush.o     fopen.o  \
fpos.o      fputc.o     fputs.o     fread.o     fseek.o     ftell.o  \
fseeko.o    ftello.o \
fwrite.o    getc.o      getchar.o   getpass.o   gets.o      getw.o  \
popen.o     putc.o      putchar.o   puts.o      putw.o  \
rewind.o    scanf.o     setbuffer.o \
setbuf.o    setvbuf.o   \
system.o    tempnam.o \
tmpfile.o   tmpnam.o    ungetc.o \
$(PORTSTDIO64) \
mse.o wscanf.o

PORTSTDIO_R= ctermid_r.o   flockf.o    tmpnam_r.o

PORTI18N= $(CTYPE) $(CHARMAP) $(LOCALE) $(MONETARY) \
	$(SETLOCALE) $(REGEX) $(TIME) $(COLLATE) \
	$(WIDEC) $(INTL)

WIDEC = \
strtows.o \
wscasecmp.o	wscat.o         wschr.o         wscmp.o \
wscol.o         wscpy.o         wscspn.o        wsdup.o \
wslen.o         wsncasecmp.o \
wsncat.o        wsncmp.o        wsncpy.o        wspbrk.o\
wsrchr.o        wsspn.o \
wstok.o         wstol.o         wstoll.o        wstod.o \
wsxfrm.o \
wcstoul.o       wcswcs.o \
fgetws.o        fputwc.o        fputws.o \
getwchar.o      getws.o         putwchar.o      putws.o \
ungetwc.o       wsprintf.o \
wsscanf.o \
wmemchr.o	wmemcmp.o		\
wmemcpy.o	wmemmove.o		\
wmemset.o	wcsstr.o		\
fwide.o


INTL= gettext.o wdresolve.o

COLLATE=\
__fnmatch_sb.o		__fnmatch_std.o			fnmatch.o	\
__strcoll_sb.o		__strcoll_std.o			strcoll.o	\
__strxfrm_sb.o		__strxfrm_std.o			strxfrm.o	\
__wcscoll_std.o		wcscoll.o			__wcsxfrm_std.o	\
wcsxfrm.o		colval.o			replacement.o	\
forw_pos.o		forward.o			back_pos.o	\
backward.o		__wcscoll_bc.o			__wcsxfrm_bc.o		

TIME=\
strftime.o		strptime.o			wcsftime.o

REGEX= \
__regcomp_std.o		__regerror_std.o		__regexec_std.o	\
__regfree_std.o		regcomp.o			regerror.o	\
__regexec_C.o 		__regcomp_C.o \
regexec.o		regfree.o 

SETLOCALE= \
_ctype.o \
loc_setup.o	setlocale.o	loadtab.o

LOCALE= \
__localeconv_std.o	__nl_langinfo_std.o		localeconv.o	\
nl_langinfo.o

MONETARY= \
__strfmon_std.o		strfmon.o

CTYPE= \
__iswctype_sb.o	__iswctype_std.o	\
_tolower.o	_toupper.o	isalnum.o	\
isalpha.o		isascii.o	iscntrl.o	isdigit.o	\
isgraph.o		islower.o	isprint.o	ispunct.o	\
isspace.o		isupper.o	iswalnum.o	iswalpha.o	\
iswcntrl.o		iswctype.o	iswdigit.o	iswgraph.o	\
iswlower.o		iswprint.o	iswpunct.o	iswspace.o	\
iswupper.o		iswxdigit.o	isxdigit.o	toascii.o	\
tolower.o		toupper.o	towlower.o	towupper.o	\
wctype.o		get_lcbind.o	\
fgetwc.o		isenglish.o	isideogram.o	isphonogram.o	\
isspecial.o		isnumber.o	\
__iswctype_bc.o		__towlower_bc.o		__towupper_bc.o \
towctrans.o		__towctrans_bc.o	wctrans.o	_trwctype.o

CHARMAP= \
__mblen_sb.o		__mbstowcs_sb.o	__mbtowc_sb.o	__wcstombs_sb.o	\
__wctomb_sb.o		__wcswidth_sb.o			__wcwidth_sb.o	\
mbftowc.o \
__mbftowc_sb.o		__fgetwc_sb.o \
mblen.o		mbstowcs.o	mbtowc.o	\
wcstombs.o		wcswidth.o	wctomb.o	wcwidth.o \
__mbtowc_dense.o	__mbstowcs_dense.o	__wctomb_dense.o \
__wcstombs_dense.o	__mbftowc_dense.o	__fgetwc_dense.o \
__eucpctowc_gen.o	__wctoeucpc_gen.o \
__wcwidth_dense.o	__wcswidth_dense.o \
__mbtowc_euc.o		__mbstowcs_euc.o	__wctomb_euc.o \
__wcstombs_euc.o	__wcwidth_euc.o		__wcswidth_euc.o \
__mblen_gen.o	euc_info.o \
__btowc_dense.o		__btowc_euc.o		\
__btowc_sb.o		__mbsinit_gen.o		\
__mbsinit_sb.o		__mbrlen_gen.o		\
__mbrlen_sb.o		__mbrtowc_dense.o	\
__mbrtowc_euc.o		__mbrtowc_sb.o		\
__mbsrtowcs_dense.o	__mbsrtowcs_euc.o	\
__mbsrtowcs_sb.o	__wcrtomb_dense.o	\
__wcrtomb_euc.o		__wcrtomb_sb.o		\
__wcsrtombs_dense.o	__wcsrtombs_euc.o	\
__wcsrtombs_sb.o	__wctob_dense.o		\
__wctob_euc.o		__wctob_sb.o		\
btowc.o			mbrlen.o		\
mbrtowc.o		mbsinit.o		\
mbsrtowcs.o		wcrtomb.o		\
wcsrtombs.o		wctob.o


STUBOBJS =	thr_stub.o

PORTSYS64 = \
lockf64.o

PORTSYS= \
corectl.o \
execl.o     execle.o  execv.o   fsync.o  lockf.o           msgsys.o  \
nfssys.o    p_online.o  processor_bind.o    processor_info.o  sbrk.o \
semsys.o  shmsys.o   \
libc_link.o	libc_open.o	lwp.o       lwp_cond.o	posix_sigwait.o \
putmsgxpg.o bsd_signal.o	siginterrupt.o	sigstack.o \
psetsys.o  fcntl.o \
$(PORTSYS64)

PORTREGEX= \
wordexp.o	glob.o \
regex.o		regcmp.o


# XXX - Note: the lists of FIRSTOBJS and LASTOBJS are taken directly
#	from the original AT&T source makefile. This ordering might
#	not have made any sense for a 3b2 and it is even less likely
#	to make sense for SunOS/i386. The lists are maintained (for
#	now) to maintain the mechanism. One should also consider that
#	the ordering which might optimize search time for a static
#	archive probably has no relationship to the ordering which
#	will minimize memory usage for a dynamic library.
FIRSTOBJS= atexit.o
REENTRANTOBJS= $(PORTGEN_R) $(PORTSTDIO_R) $(GENOBJS_R) $(STUBOBJS)
MOSTOBJS= $(PORTWIDEC) $(PORTINTL) \
	$(CRTOBJS) $(PORTREGEX) $(DYNOBJS) $(GENOBJS) $(FPOBJS) $(SYSOBJS) \
	$(PORTGEN) $(PORTPRINT) $(PORTSTDIO) $(PORTSYS) $(PORTI18N) \
	$(REENTRANTOBJS)
LASTOBJS= fpsetmask.o kill.o getpid.o cerror.o cerror64.o cuexit.o

OBJECTS= $(FIRSTOBJS) $(MOSTOBJS) $(LASTOBJS)

# libc build rules
objs/%.o profs/%.o pics/%.o: crt/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)


# regex rules
objs/%.o profs/%.o pics/%.o: ../port/regex/%.c
	$(COMPILE.c) -DM_I18N_MB -DI18N \
	-I../port/regex \
	-I../port/gen -o $@ $<
	$(POST_PROCESS_O)

#objs/%.o profs/%.o pics/%.o: crt/%.s
#	$(BUILD.s)
#	$(POST_PROCESS_O)

objs/%.o: crt/%.s
	$(BUILD_OBJS.s)
	$(POST_PROCESS_O)

profs/%.o: crt/%.s
	$(BUILD_PROFS.s)
	$(POST_PROCESS_O)

pics/%.o: crt/%.s
	$(BUILD_PICS.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o: gen/%.s
	$(BUILD_OBJS.s)
	$(POST_PROCESS_O)

profs/%.o: gen/%.s
	$(BUILD_PROFS.s)
	$(POST_PROCESS_O)

pics/%.o: gen/%.s
	$(BUILD_PICS.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: fp/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o: fp/%.s
	$(BUILD_OBJS.s)
	$(POST_PROCESS_O)

profs/%.o: fp/%.s
	$(BUILD_PROFS.s)
	$(POST_PROCESS_O)

pics/%.o: fp/%.s
	$(BUILD_PICS.s)
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o: sys/%.s
	$(BUILD_OBJS.s)
	$(POST_PROCESS_O)

profs/%.o: sys/%.s
	$(BUILD_PROFS.s)
	$(POST_PROCESS_O)

pics/%.o: sys/%.s
	$(BUILD_PICS.s)
	$(POST_PROCESS_O)

# rules to build large file aware objects (xxx64.o from xxx.s or xxx.c)
objs/%64.o: sys/%.s
	$(BUILD_OBJS.s)
	$(POST_PROCESS_O)

profs/%64.o: sys/%.s
	$(BUILD_PROFS.s)
	$(POST_PROCESS_O)

pics/%64.o: sys/%.s
	$(BUILD_PICS.s)
	$(POST_PROCESS_O)

objs/%64.o profs/%64.o pics/%64.o: gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%64.o profs/%64.o pics/%64.o: ../port/gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%64.o profs/%64.o pics/%64.o: ../port/print/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%64.o profs/%64.o pics/%64.o: ../port/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%64.o profs/%64.o pics/%64.o: ../port/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%64.o profs/%64.o pics/%64.o: ../port/stubs/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%_w.o profs/%_w.o pics/%_w.o: ../port/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%_w.o profs/%_w.o pics/%_w.o: ../port/print/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#
# libc build rules for objects built from "portable" source in ../port

objs/%.o profs/%.o pics/%.o: ../port/i18n/%.c
	$(COMPILE.c) \
	-I../port/i18n -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o %.o: ../port/gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#objs/%.o profs/%.o pics/%.o: ../port/intl/%.c
#	$(COMPILE.c) -o $@ $<
#	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../port/print/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../port/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../port/stubs/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../port/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#objs/%.o profs/%.o pics/%.o: ../port/widec/%.c
#	$(COMPILE.c) \
#	-I../port/widec \
#	-I../port/gen -o $@ $<
#	$(POST_PROCESS_O)

# special case for widec
objs/wstod.o pics/wstod.o profs/wstod.o := CPPFLAGS += -I../$(MACH)/fp

# include common library definitions
include ../../Makefile.lib

MAPDIR=		../spec/i386
MAPFILE=	$(MAPDIR)/mapfile

CPPFLAGS=	-D_REENTRANT -Di386 -Iinc -I../inc $(CPPFLAGS.master)
ASDEFS=		# -D__STDC__ -D_ASM $(CPPFLAGS)
EXTRADEFS=
ASOBJDEFS=	-m m4.def sys.def ansi.def nonpic.def -DMCOUNT=/ $(EXTRADEFS)
ASPICDEFS=	-m -DDSHLIB m4.def sys.def ansi.def pic.def -DMCOUNT=/ $(EXTRADEFS)
ASPROFDEFS=	-m m4.def sys.def ansi.def nonpic.def mcount.def $(EXTRADEFS)
ASFLAGS=	$(ASDEFS) # -P
DYNFLAGS +=	-e __rtboot -M $(MAPFILE)
LDLIBS +=	-ldl

# The following three lines ought to work, but parallel make has a bug:
#$(ASSYMDEP_OBJS:%=objs/%) := ASOBJDEFS += assym.def
#$(ASSYMDEP_OBJS:%=profs/%) := ASPROFDEFS += assym.def
#$(ASSYMDEP_OBJS:%=pics/%) := ASPICDEFS += assym.def
# This works around the bug:
$(ASSYMDEP_OBJS:%=objs/%) := EXTRADEFS = assym.def
$(ASSYMDEP_OBJS:%=profs/%) := EXTRADEFS = assym.def
$(ASSYMDEP_OBJS:%=pics/%) := EXTRADEFS = assym.def

# BUILD.s=  $(AS) $(ASFLAGS) $< -o $@
BUILD_OBJS.s=  $(AS) $(ASFLAGS) $(ASOBJDEFS) $< -o $@
BUILD_PICS.s=  $(AS) $(ASFLAGS) $(ASPICDEFS) $< -o $@
BUILD_PROFS.s=  $(AS) $(ASFLAGS) $(ASPROFDEFS) $< -o $@

# libc method of building an archive, using AT&T ordering
BUILD.AR= $(RM) $@ ; \
	$(AR) q $@ $(FIRSTOBJS:%=$(DIR)/%) && \
	$(AR) q $@ `$(LORDER) $(MOSTOBJS:%=$(DIR)/%)| $(TSORT)` && \
	$(AR) q $@ $(LASTOBJS:%=$(DIR)/%)

# extra files for the clean target
CLEANFILES= crt/_div.s crt/_rem.s crt/_rtld.s ../port/gen/errlst.c \
	    ../port/gen/new_list.c $(VALUES)

CLOBBERFILES +=	$(MAPFILE)

# the TXTS macro is used for NSE bootstrapping.
TXTS=	$(GENTXTS:%=../port/gen/%)

# list of C source for lint
SRCS= \
gen/atof.c        gen/ecvt.c	\
gen/lconstants.c  gen/ldivide.c     gen/lexp10.c      gen/llog10.c  \
gen/lmul.c        gen/ltostr.c      gen/makectxt.c    \
                  gen/siginfolst.c  gen/sigsetjmp.c   gen/strtod.c  \
gen/swapctxt.c  \
sys/sigaction.c  sys/_signal.c  \
$(PORTGEN:%.o=../port/gen/%.c)      $(PORTPRINT:%.o=../port/print/%.c)  \
$(PORTSTDIO:%.o=../port/stdio/%.c)  $(PORTSYS:%.o=../port/sys/%.c)  \
$(PORTREGEX:%.o=../port/regex/%.c)  \
$(PORTI18N:%.o=../port/i18n/%.c) \
../port/gen/atexit.c

# conditional assignments
# $(DYNLIB) $(LIB_PIC):= PORTGEN += values-Xt.o
$(DYNLIB) $(LIB_PIC):= DYNOBJS = _rtbootld.o

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# Files in fp subdirectory which need base.il inline template
# IL=	_base_sup.o	decimal_bin.o	double_decim.o	__floatprint.o \
# 	__gconvert.o	__tbl_fdq.o	__x_power.o
# $(IL:%=objs/%) $(IL:%=pics/%) $(IL:%=profs/%) := CFLAGS += fp/base.il

# objs/mul64.o pics/mul64.o profs/mul64.o := CFLAGS += crt/mul64.il

# large-file-aware components that should be built large.

$(SYSOBJS64:%=objs/%) $(SYSOBJS64:%=pics/%) \
	$(SYSOBJS64:%=profs/%) := ASDEFS += -D_LARGEFILE_INTERFACE \
	-D_FILE_OFFSET_BITS=64

$(PORTGEN64:%=objs/%) $(PORTGEN64:%=pics/%) \
	$(PORTGEN64:%=profs/%) := CPPFLAGS += -D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64
  
$(PORTSTDIO64:%=objs/%) $(PORTSTDIO64:%=pics/%) \
	$(PORTSTDIO64:%=profs/%) := CPPFLAGS += -D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64
  
$(PORTSYS64:%=objs/%) $(PORTSYS64:%=pics/%) \
	$(PORTSYS64:%=profs/%) := CPPFLAGS += -D_LARGEFILE_SOURCE \
	-D_FILE_OFFSET_BITS=64  

$(PORTSTDIO_W:%=objs/%) $(PORTSTDIO_W:%=pics/%) \
	$(PORTSTDIO_W:%=profs/%) := CPPFLAGS += -D_WIDE

$(PORTPRINT_W:%=objs/%) $(PORTPRINT_W:%=pics/%) \
	$(PORTPRINT_W:%=profs/%) := CPPFLAGS += -D_WIDE

.KEEP_STATE:

all: $(TXTS) $(LIBS) $(PLIB) $(LIB_PIC) values

values: $(VALUES)

$(LINTLIB):= SRCS=../port/llib-lc
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

# object files that depend on inline template.
# $(IL:%=objs/%) $(IL:%=pics/%) $(IL:%=profs/%): fp/base.il
# objs/mul64.o pics/mul64.o profs/mul64.o: crt/mul64.il

# include common library targets
include ../../Makefile.targ

$(LIB_PIC) := DIR = pics
$(LIB_PIC): pics $$(PICS)
	$(BUILD.AR)
	$(POST_PROCESS_A)


# special cases
# $(STRETS:%=objs/%) \
# $(STRETS:%=profs/%) \
# $(STRETS:%=pics/%): crt/stret.s
#	$(AS) $(ASFLAGS) -DSTRET$(@F:stret%.o=%) crt/stret.s -o $@
#	$(POST_PROCESS_O)

crt/_div.s:	crt/divrem.m4.s
	$(M4) -DANSWER=quotient crt/divrem.m4.s > $@

crt/_rem.s:	crt/divrem.m4.s
	$(M4) -DANSWER=remainder crt/divrem.m4.s > $@

../port/stubs/thr_stub.c:	../port/stubs/thr_stub.m4
	$(M4) ../port/stubs/thr_stub.m4 > $@

crt/_rtbootld.s:	crt/_rtboot.s crt/_rtld.c
	$(CC) $(CPPFLAGS) -O -S -K pic crt/_rtld.c -o crt/_rtld.s
	$(RM) -rf crt/_rtbootld.s
	$(CAT) crt/_rtboot.s crt/_rtld.s > $@
	$(RM) crt/_rtld.s

$(ASSYMDEP_OBJS:%=objs/%) \
$(ASSYMDEP_OBJS:%=profs/%) \
$(ASSYMDEP_OBJS:%=pics/%): assym.def

assym.def: genassym
	./genassym > $@

# XXX	A hack.  Perhaps this should be 'CPPFLAGS.native' and
#	live in Makefile.master

CPPFLAGS.genassym = \
$(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)

genassym: genassym.c
	$(NATIVECC) -O -D$(MACH) -Iinc $(CPPFLAGS.genassym) -o $@ genassym.c

# derived C source and related explicit dependencies
#
../port/gen/errlst.c + \
../port/gen/new_list.c: ../port/gen/errlist ../port/gen/errlist.awk
	cd ../port/gen; pwd; $(AWK) -f errlist.awk < errlist

objs/errlst.o \
pics/errlst.o \
profs/errlst.o: ../port/gen/errlst.c

objs/new_list.o \
pics/new_list.o \
profs/new_list.o: ../port/gen/new_list.c
