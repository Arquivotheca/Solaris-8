/* ONC_PLUS EXTRACT START */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/* ONC_PLUS EXTRACT END */

#pragma ident	"@(#)sysent.c	1.113	99/08/15 SMI"	/* SVr4.0 1.51	*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>

/* ONC_PLUS EXTRACT START */
/*
 * This table is the switch used to transfer to the appropriate
 * routine for processing a system call.  Each row contains the
 * number of arguments expected, a switch that tells systrap()
 * in trap.c whether a setjmp() is not necessary, and a pointer
 * to the routine.
 */
/* ONC_PLUS EXTRACT END */

int	access();
int	alarm();
int	auditsys();
int	brk();
int	chdir();
int	chmod();
int	chown();
int	chroot();
int	cladm();
int	close();
int	creat();
int	dup();
int	exec();
int	exece();
int	fcntl();
int64_t	fork();
int64_t	fork1();
int	fstat();
int	fdsync();
int64_t	getgid();
int64_t	getpid();
int64_t	getuid();
time_t	gtime();
int	getloadavg();
int	gtty();
#if defined(i386)
int	hrtsys();
#endif /* defined(i386) */
int	ioctl();
int	kill();
int	link();
off32_t	lseek32();
off_t	lseek64();
int	mknod();
int	mount();
int	nice();
int	nullsys();
int	open();
int	pause();
long	pcsample();
int	profil();
ssize_t	pread();
ssize_t	pwrite();
ssize_t	read();
int	rename();
void	rexit();
int	semsys();
int	setgid();
int	setpgrp();
int	setuid();
uintptr_t	shmsys();
int	ssig();
int	sigprocmask();
int	sigsuspend();
int	sigaltstack();
int	sigaction();
int	sigpending();
int	sigtimedwait();
int	lwp_sigtimedwait();
int	getsetcontext();
int	stat();
int	stime();
int	stty();
int	syssync();
int	sysacct();
clock_t	times();
long	ulimit();
int	getrlimit32();
int	setrlimit32();
int	umask();
int	umount();
int	umount2();
int	unlink();
int	utime();
int64_t	utssys32();
int64_t	utssys64();
int64_t	wait();
ssize_t	write();
ssize_t	readv();
ssize_t	writev();
int	rmdir();
int	mkdir();
int	getdents32();
int	statfs32();
int	fstatfs32();
int	sysfs();
int	getmsg();
int	poll();
int	putmsg();
int	uadmin();
int	lstat();
int	symlink();
int	readlink();
int	resolvepath();
int	setgroups();
int	getgroups();
int	fchdir();
int	fchown();
int	fchmod();
int	statvfs();
int	fstatvfs();
offset_t llseek32();

#if defined(i386) || defined(__ia64)
int	sysi86();
#endif

int	acl();
int	facl();
long	priocntlsys();
int	waitsys();
int	sigsendsys();
int	mincore();
caddr_t	smmap64();
caddr_t smmap32();
int	smmaplf32();
int	mprotect();
int	munmap();
int64_t	vfork();
int	xstat();
int	lxstat();
int	fxstat();
int	xmknod();
int	uname();
int	lchown();
int	getpmsg();
int	putpmsg();
int	memcntl();
long	sysconfig();
int	adjtime();
long	systeminfo();
int	setegid();
int	seteuid();

int	setreuid();
int	setregid();
int	install_utrap();
#ifdef __sparcv9
int	sparc_utrap_install();
#endif

int	syslwp_create();
void	syslwp_exit();
longlong_t	syslwp_suspend();
int	syslwp_continue();
int	lwp_info();
int	lwp_kill();
int	lwp_self();
int	yield();
int	lwp_wait();
int	lwp_setprivate();
long	lwp_getprivate();

int	lwp_mutex_lock();
int	lwp_mutex_wakeup();
int	lwp_mutex_unlock();
int	lwp_mutex_trylock();
int	lwp_mutex_init();
int	lwp_sema_post();
int	lwp_sema_wait();
int	lwp_sema_trywait();
int	lwp_cond_wait();
int	lwp_cond_signal();
int	lwp_cond_broadcast();
int	lwp_alarm();		/* will be EOL'ed in a post-2.5 release */
int	schedctl();

long	pathconf();
long	fpathconf();
int	processor_bind();
int	processor_info();
int	p_online();

/*
 *	System Calls used by the ASLWP.
 */
int	signotifywait();
int	lwp_sigredirect();

/*
 *	POSIX .4 system calls *
 */
int	clock_gettime();
int	clock_settime();
int	clock_getres();
int	timer_create();
int	timer_delete();
int	timer_settime();
int	timer_gettime();
int	timer_getoverrun();
int	nanosleep();
int	sigqueue();
int	signotify();

int	getdents64();
int	stat64();
int	lstat64();
int	fstat64();
int	statvfs64();
int	fstatvfs64();
int	setrlimit64();
int	getrlimit64();
int	pread64();
int	pwrite64();
int	creat64();
int	open64();

/*
 * NTP syscalls
 */

int ntp_gettime();
int ntp_adjtime();

/*
 *	++++++++++++++++++++++++
 *	++  SunOS4.1 Buyback  ++
 *	++++++++++++++++++++++++
 *
 *	fchroot, utimes, vhangup, gettimeofday
 */

int	fchroot();
int	utimes();
int	vhangup();
int	gettimeofday();
int	getitimer();
int	setitimer();
#ifdef	TRACE
int	vtrace();
#endif

#ifdef MEGA
int	uexch();
#endif /* MEGA */

int	corectl();
int	modctl();
int64_t loadable_syscall();
int64_t indir();

int	so_socket();
int	so_socketpair();
int	bind();
int	listen();
int	accept();
int	connect();
int	shutdown();
ssize_t	recv();		/* XXX_LP64 ?? */
ssize_t	recvfrom();	/* XXX_LP64 ?? */
ssize_t	recvmsg();	/* XXX_LP64 ?? */
ssize_t	send();		/* XXX_LP64 ?? */
ssize_t	sendmsg();	/* XXX_LP64 ?? */
ssize_t	sendto();	/* XXX_LP64 ?? */
int	getpeername();
int	getsockname();
int	getsockopt();
int	setsockopt();
int	sockconfig();

typedef int64_t	(*llfcn_t)();	/* for casting one-word returns */

/*
 * Sysent initialization macros.
 * 	These take the name string of the system call even though that isn't
 *	currently used in the sysent entry.  This might be useful someday.
 *
 * Initialization macro for system calls which take their args in the C style.
 * These system calls return the longlong_t return value and must call
 * set_errno() to return an error.  For SPARC, narg must be at most six.
 * For more args, use the SYSENT_AP() routine.
 *
 * We are able to return two distinct values to userland via the rval_t.
 * At this time, that corresponds to one 64-bit quantity, or two 32-bit
 * quantities.  The kernel does not currently need to return two 64-bit
 * values, or one 128 bit value(!), but we may do one day, so the calling
 * sequence between userland and the kernel should permit it.
 *
 * The interpretation of rval_t is provided by the sy_flags field
 * which is used to determine how to arrange the results in registers
 * (or on the stack) for return userland.
 */
	/* returns a 64-bit quantity for both ABIs */
#define	SYSENT_C(name, call, narg)	\
	{ (narg), SE_64RVAL, NULL, NULL, (llfcn_t)(call) }

	/* returns one 32-bit value for both ABIs: r_val1 */
#define	SYSENT_CI(name, call, narg)	\
	{ (narg), SE_32RVAL1, NULL, NULL, (llfcn_t)(call) }

	/* returns 2 32-bit values: r_val1 & r_val2 */
#define	SYSENT_2CI(name, call, narg)	\
	{ (narg), SE_32RVAL1|SE_32RVAL2, NULL, NULL, (llfcn_t)(call) }

/*
 * Initialization macro for system calls which take their args in the standard
 * Unix style of a pointer to the arg structure and a pointer to the rval_t.
 *
 * Deprecated wherever possible (slower on some architectures, and trickier
 * to maintain two flavours).
 */
#define	SYSENT_AP(name, call, narg)	\
	{ (narg), SE_64RVAL, (call), NULL, syscall_ap }

/*
 * Conditional constructors to build the tables without #ifdef clutter
 */
#ifdef	_LP64
#define	IF_LP64(true, false)	true
#else
#define	IF_LP64(true, false)	false
#endif

#ifdef	i386
#define	IF_i386(true, false)	true
#else
#define	IF_i386(true, false)	false
#endif

#if defined(i386) || defined(__ia64)
#define	IF_intel(true, false)	true
#else
#define	IF_intel(true, false)	false
#endif

/*
 * Define system calls that return a native 'long' quantity i.e. a 32-bit
 * or 64-bit integer - depending on how the kernel is itself compiled
 * e.g. read(2) returns 'ssize_t' in the kernel and in userland.
 */
#define	SYSENT_CL(name, call, narg)	\
	IF_LP64(SYSENT_C(name, call, narg), SYSENT_CI(name, call, narg))

/*
 * Initialization macro for loadable native system calls.
 */
#define	SYSENT_LOADABLE()	\
	{ 0, SE_LOADABLE, (int (*)())nosys, NULL, loadable_syscall }
/* ONC_PLUS EXTRACT END */

/*
 * Initialization macro for loadable 32-bit compatibility system calls.
 */
#define	SYSENT_LOADABLE32()	SYSENT_LOADABLE()

#define	SYSENT_NOSYS()		SYSENT_C("nosys", nosys, 0)

struct sysent nosys_ent = SYSENT_NOSYS();

/* ONC_PLUS EXTRACT START */
/*
 * Native sysent table.
 */
struct sysent sysent[NSYSCALL] =
{
/* ONC_PLUS EXTRACT END */
	/*  0 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_C("indir",	indir,		1)),
	/*  1 */ SYSENT_CI("exit",		rexit,		1),
	/*  2 */ SYSENT_2CI("fork",		fork,		0),
	/*  3 */ SYSENT_CL("read",		read,		3),
	/*  4 */ SYSENT_CL("write",		write,		3),
	/*  5 */ SYSENT_CI("open",		open,		3),
	/*  6 */ SYSENT_CI("close",		close,		1),
	/*  7 */ SYSENT_2CI("wait",		wait,		0),
	/*  8 */ SYSENT_CI("creat",		creat,		2),
	/*  9 */ SYSENT_CI("link",		link,		2),
	/* 10 */ SYSENT_CI("unlink",		unlink,		1),
	/* 11 */ SYSENT_CI("exec",		exec,		2),
	/* 12 */ SYSENT_CI("chdir",		chdir,		1),
	/* 13 */ SYSENT_CL("time",		gtime,		0),
	/* 14 */ SYSENT_CI("mknod",		mknod,		3),
	/* 15 */ SYSENT_CI("chmod",		chmod,		2),
	/* 16 */ SYSENT_CI("chown",		chown,		3),
	/* 17 */ SYSENT_CI("brk",		brk,		1),
	/* 18 */ SYSENT_CI("stat",		stat,		2),
	/* 19 */ IF_LP64(
			SYSENT_CL("lseek",	lseek64,	3),
			SYSENT_CL("lseek",	lseek32,	3)),
	/* 20 */ SYSENT_2CI("getpid",		getpid,		0),
	/* 21 */ SYSENT_AP("mount",		mount,		8),
	/* 22 */ SYSENT_CI("umount",		umount,		1),
	/* 23 */ SYSENT_CI("setuid",		setuid,		1),
	/* 24 */ SYSENT_2CI("getuid",		getuid,		0),
	/* 25 */ SYSENT_CI("stime",		stime,		1),
	/* 26 */ SYSENT_CL("pcsample",		pcsample,	2),
	/* 27 */ SYSENT_CI("alarm",		alarm,		1),
	/* 28 */ SYSENT_CI("fstat",		fstat,		2),
	/* 29 */ SYSENT_CI("pause",		pause,		0),
	/* 30 */ SYSENT_CI("utime",		utime,		2),
	/* 31 */ SYSENT_CI("stty",		stty,		2),
	/* 32 */ SYSENT_CI("gtty",		gtty,		2),
	/* 33 */ SYSENT_CI("access",		access,		2),
	/* 34 */ SYSENT_CI("nice",		nice,		1),
	/* 35 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("statfs",	statfs32,	4)),
	/* 36 */ SYSENT_CI("sync",		syssync,	0),
	/* 37 */ SYSENT_CI("kill",		kill,		2),
	/* 38 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("fstatfs",	fstatfs32,	4)),
	/* 39 */ SYSENT_CI("setpgrp",		setpgrp,	3),
	/* 40 */ SYSENT_LOADABLE(),			/* (was cxenix) */
	/* 41 */ SYSENT_CI("dup",		dup,		1),
	/* 42 */ SYSENT_LOADABLE(),			/* (was pipe ) */
	/* 43 */ SYSENT_CL("times",		times,		1),
	/* 44 */ SYSENT_CI("prof",		profil,		4),
	/* 45 */ SYSENT_LOADABLE(),			/* (was proc lock) */
	/* 46 */ SYSENT_CI("setgid",		setgid,		1),
	/* 47 */ SYSENT_2CI("getgid",		getgid,		0),
	/* 48 */ SYSENT_CI("sig",		ssig,		2),
	/* 49 */ SYSENT_LOADABLE(),			/* (was msgsys) */
	/* 50 */ IF_intel(
			SYSENT_AP("sysi86",	sysi86,		4),
			SYSENT_LOADABLE()),		/* (was sys3b) */
	/* 51 */ SYSENT_LOADABLE(),			/* sysacct */
	/* 52 */ SYSENT_LOADABLE(),			/* shmsys */
	/* 53 */ SYSENT_LOADABLE(),			/* semsys */
	/* 54 */ SYSENT_CI("ioctl",		ioctl,		3),
	/* 55 */ SYSENT_CI("uadmin",		uadmin,		3),
#ifdef MEGA
	/* 56 */ SYSENT_AP("uexch",		uexch,		3),
#else
	/* 56 */ SYSENT_LOADABLE(),
#endif /* MEGA */
	/* 57 */ IF_LP64(
			SYSENT_2CI("utssys",	utssys64,	4),
			SYSENT_2CI("utssys",	utssys32,	4)),
	/* 58 */ SYSENT_CI("fdsync",		fdsync,		2),
	/* 59 */ SYSENT_CI("exece",		exece,		3),
	/* 60 */ SYSENT_CI("umask",		umask,		1),
	/* 61 */ SYSENT_CI("chroot",		chroot,		1),
	/* 62 */ SYSENT_CI("fcntl",		fcntl,		3),
	/* 63 */ SYSENT_CI("ulimit",		ulimit,		2),

	/*
	 * The following 6 entries were reserved for the UNIX PC.
	 */
	/* 64 */ SYSENT_LOADABLE(),
	/* 65 */ SYSENT_LOADABLE(),
	/* 66 */ SYSENT_LOADABLE(),
	/* 67 */ SYSENT_LOADABLE(),		/* file locking call */
	/* 68 */ SYSENT_LOADABLE(),		/* local system calls */
	/* 69 */ SYSENT_LOADABLE(),		/* inode open */

	/* 70 */ SYSENT_NOSYS(),		/* 70 reserved */
	/* 71 */ SYSENT_NOSYS(),		/* 71 reserved */
	/* 72 */ SYSENT_NOSYS(),		/* 72 reserved */
	/* 73 */ SYSENT_NOSYS(),		/* 73 reserved */
	/* 74 */ SYSENT_NOSYS(),		/* 74 reserved */
	/* 75 */ SYSENT_NOSYS(),		/* 75 reserved */
	/* 76 */ SYSENT_NOSYS(),		/* 76 reserved */
	/* 77 */ SYSENT_NOSYS(),		/* 77 reserved */
	/* 78 */ SYSENT_NOSYS(),		/* 78 reserved */
	/* 79 */ SYSENT_CI("rmdir",		rmdir,		1),
	/* 80 */ SYSENT_CI("mkdir",		mkdir,		2),
	/* 81 */ IF_LP64(
			SYSENT_CI("getdents",	getdents64,	3),
			SYSENT_CI("getdents",	getdents32,	3)),
	/* 82 */ SYSENT_LOADABLE(),		/* (was libattach) */
	/* 83 */ SYSENT_LOADABLE(),		/* (was libdetach) */
	/* 84 */ SYSENT_CI("sysfs",		sysfs,		3),
	/* 85 */ SYSENT_CI("getmsg",		getmsg,		4),
	/* 86 */ SYSENT_CI("putmsg",		putmsg,		4),
	/* 87 */ SYSENT_CI("poll",		poll,		3),
	/* 88 */ SYSENT_CI("lstat",		lstat,		2),
	/* 89 */ SYSENT_CI("symlink",		symlink,	2),
	/* 90 */ SYSENT_CI("readlink",		readlink,	3),
	/* 91 */ SYSENT_CI("setgroups",		setgroups,	2),
	/* 92 */ SYSENT_CI("getgroups",		getgroups,	2),
	/* 93 */ SYSENT_CI("fchmod",		fchmod,		2),
	/* 94 */ SYSENT_CI("fchown",		fchown,		3),
	/* 95 */ SYSENT_CI("sigprocmask",	sigprocmask,	3),
	/* 96 */ SYSENT_CI("sigsuspend",	sigsuspend,	1),
	/* 97 */ SYSENT_CI("sigaltstack",	sigaltstack,	2),
	/* 98 */ SYSENT_CI("sigaction",		sigaction,	3),
	/* 99 */ SYSENT_CI("sigpending",	sigpending,	2),

	/* 100 */ SYSENT_CI("getsetcontext",	getsetcontext,	2),
	/* 101 */ SYSENT_LOADABLE(),		/* (was evsys) */
	/* 102 */ SYSENT_LOADABLE(),		/* (was evtrapret) */
	/* 103 */ SYSENT_CI("statvfs",		statvfs,	2),
	/* 104 */ SYSENT_CI("fstatvfs",		fstatvfs,	2),
	/* 105 */ SYSENT_CI("getloadavg",	getloadavg,	2),
/* ONC_PLUS EXTRACT START */
	/* 106 */ SYSENT_LOADABLE(),		/* nfssys */
/* ONC_PLUS EXTRACT END */
	/* 107 */ SYSENT_CI("waitsys",		waitsys,	4),
	/* 108 */ SYSENT_CI("sigsendset",	sigsendsys,	2),
	/* 109 */ IF_i386(
			SYSENT_AP("hrtsys",	hrtsys,		5),
			SYSENT_LOADABLE()),
	/* 110 */ SYSENT_LOADABLE(),		/* was acancel */
	/* 111 */ SYSENT_LOADABLE(),		/* was async */
	/* 112 */ SYSENT_CL("priocntlsys",	priocntlsys,	4),
	/* 113 */ SYSENT_CL("pathconf",		pathconf,	2),
	/* 114 */ SYSENT_CI("mincore",		mincore,	3),
	/* 115 */ IF_LP64(
			SYSENT_CL("mmap",	smmap64,	6),
			SYSENT_CL("mmap",	smmap32,	6)),
	/* 116 */ SYSENT_CI("mprotect",		mprotect,	3),
	/* 117 */ SYSENT_CI("munmap",		munmap,		2),
	/* 118 */ SYSENT_CL("fpathconf",	fpathconf,	2),
	/* 119 */ SYSENT_2CI("vfork",		vfork,		0),
	/* 120 */ SYSENT_CI("fchdir",		fchdir,		1),
	/* 121 */ SYSENT_CL("readv",		readv,		3),
	/* 122 */ SYSENT_CL("writev",		writev,		3),
	/* 123 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("xstat",	xstat,		3)),
	/* 124 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("lxstat",	lxstat,		3)),
	/* 125 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("fxstat",	fxstat,		3)),
	/* 126 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("xmknod",	xmknod,		4)),
	/* 127 */ SYSENT_LOADABLE(),		/* was clocal */
	/* 128 */ IF_LP64(
			SYSENT_CI("setrlimit",	setrlimit64,	2),
			SYSENT_CI("setrlimit",	setrlimit32,	2)),
	/* 129 */ IF_LP64(
			SYSENT_CI("getrlimit",	getrlimit64,	2),
			SYSENT_CI("getrlimit",	getrlimit32,	2)),
	/* 130 */ SYSENT_CI("lchown",		lchown,		3),
	/* 131 */ SYSENT_CI("memcntl",		memcntl,	6),
	/* 132 */ SYSENT_CI("getpmsg",		getpmsg,	5),
	/* 133 */ SYSENT_CI("putpmsg",		putpmsg,	5),
	/* 134 */ SYSENT_CI("rename",		rename,		2),
	/* 135 */ SYSENT_CI("uname",		uname,		1),
	/* 136 */ SYSENT_CI("setegid",		setegid,	1),
	/* 137 */ SYSENT_CL("sysconfig",	sysconfig,	1),
	/* 138 */ SYSENT_CI("adjtime",		adjtime,	2),
	/* 139 */ SYSENT_CL("systeminfo",	systeminfo,	3),
	/* 140 */ SYSENT_LOADABLE(),		/* reserved */
	/* 141 */ SYSENT_CI("seteuid",		seteuid,	1),
#ifdef	TRACE
	/* 142 */ SYSENT_AP("vtrace",		vtrace,		3),
#else
	/* 142 */ SYSENT_LOADABLE(),		/* vtrace */
#endif	/* TRACE */
	/* 143 */ SYSENT_2CI("fork1",		fork1,		0),
	/* 144 */ SYSENT_CI("sigtimedwait",	sigtimedwait,	3),
	/* 145 */ SYSENT_CI("lwp_info",		lwp_info,	1),
	/* 146 */ SYSENT_CI("yield",		yield,		0),
	/* 147 */ SYSENT_CI("lwp_sema_wait",	lwp_sema_wait,	1),
	/* 148 */ SYSENT_CI("lwp_sema_post",	lwp_sema_post,	1),
	/* 149 */ SYSENT_CI("lwp_sema_trywait",	lwp_sema_trywait, 1),
	/* 150 */ SYSENT_LOADABLE(),		/* reserved */
	/* 151 */ SYSENT_CI("corectl",		corectl,	4),
	/* 152 */ SYSENT_CI("modctl",		modctl,		6),
	/* 153 */ SYSENT_CI("fchroot",		fchroot,	1),
	/* 154 */ SYSENT_CI("utimes",		utimes,		2),
	/* 155 */ SYSENT_CI("vhangup",		vhangup,	0),
	/* 156 */ SYSENT_CI("gettimeofday",	gettimeofday,	1),
	/* 157 */ SYSENT_CI("getitimer",	getitimer,	2),
	/* 158 */ SYSENT_CI("setitimer",	setitimer,	3),
	/* 159 */ SYSENT_CI("lwp_create",	syslwp_create,	3),
	/* 160 */ SYSENT_CI("lwp_exit",	(int (*)())syslwp_exit,	0),
	/* 161 */ SYSENT_2CI("lwp_stop",	syslwp_suspend,	1),
	/* 162 */ SYSENT_CI("lwp_continue",	syslwp_continue, 1),
	/* 163 */ SYSENT_CI("lwp_kill",		lwp_kill,	2),
	/* 164 */ SYSENT_CI("lwp_get_id",	lwp_self,	0),
	/* 165 */ SYSENT_CI("lwp_setprivate",	lwp_setprivate,	1),
	/* 166 */ SYSENT_CL("lwp_getprivate",	lwp_getprivate,	0),
	/* 167 */ SYSENT_CI("lwp_wait",		lwp_wait,	2),
	/* 168 */ SYSENT_CI("lwp_mutex_wakeup",	lwp_mutex_wakeup,	1),
	/* 169 */ SYSENT_CI("lwp_mutex_lock",	lwp_mutex_lock,		1),
	/* 170 */ SYSENT_CI("lwp_cond_wait",	lwp_cond_wait,		3),
	/* 171 */ SYSENT_CI("lwp_cond_signal",	lwp_cond_signal,	1),
	/* 172 */ SYSENT_CI("lwp_cond_broadcast", lwp_cond_broadcast,	1),
	/* 173 */ SYSENT_CL("pread",		pread,		4),
	/* 174 */ SYSENT_CL("pwrite ",		pwrite,		4),
	/*
	 * The 64-bit C library maps llseek() to lseek(), so this
	 * is needed as a native syscall only on the 32-bit kernel.
	 */
	/* 175 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_C("llseek",	llseek32,	4)),
	/* 176 */ SYSENT_LOADABLE(),		/* inst_sync */
	/* 177 */ SYSENT_LOADABLE(),		/* srmlimitsys */
	/* 178 */ SYSENT_LOADABLE(),		/* kaio */
	/* 179 */ SYSENT_LOADABLE(),		/* cpc */
	/* 180 */ SYSENT_LOADABLE(),
	/* 181 */ SYSENT_LOADABLE(),
	/* 182 */ SYSENT_LOADABLE(),
	/* 183 */ SYSENT_LOADABLE(),
	/* 184 */ SYSENT_LOADABLE(),		/* tsolsys */
	/* 185 */ SYSENT_CI("acl",		acl,		4),
	/* 186 */ SYSENT_AP("auditsys",		auditsys,	2),
	/* 187 */ SYSENT_CI("processor_bind",	processor_bind,	4),
	/* 188 */ SYSENT_CI("processor_info",	processor_info,	2),
	/* 189 */ SYSENT_CI("p_online",		p_online,	2),
	/* 190 */ SYSENT_CI("sigqueue",		sigqueue,	4),
	/* 191 */ SYSENT_CI("clock_gettime",	clock_gettime,	2),
	/* 192 */ SYSENT_CI("clock_settime",	clock_settime,	2),
	/* 193 */ SYSENT_CI("clock_getres",	clock_getres,	2),
	/* 194 */ SYSENT_CI("timer_create",	timer_create,	3),
	/* 195 */ SYSENT_CI("timer_delete",	timer_delete,	1),
	/* 196 */ SYSENT_CI("timer_settime",	timer_settime,	4),
	/* 197 */ SYSENT_CI("timer_gettime",	timer_gettime,	2),
	/* 198 */ SYSENT_CI("timer_getoverrun",	timer_getoverrun, 1),
	/* 199 */ SYSENT_CI("nanosleep",	nanosleep,	2),
	/* 200 */ SYSENT_CI("facl",		facl,		4),
	/* 201 */ SYSENT_LOADABLE(),		/* door */
	/* 202 */ SYSENT_CI("setreuid",		setreuid,	2),
	/* 203 */ SYSENT_CI("setregid",		setregid,	2),
	/* 204 */ SYSENT_CI("install_utrap",	install_utrap,	3),
	/* 205 */ SYSENT_CI("signotify",	signotify,	3),
	/* 206 */ SYSENT_CI("schedctl",		schedctl,	3),
	/* 207 */ SYSENT_LOADABLE(),		/* pset */
#ifdef __sparcv9
	/* 208 */ SYSENT_CI("sparc_utrap_install", sparc_utrap_install, 5),
#else
	/* 208 */ SYSENT_NOSYS(),
#endif
	/* 209 */ SYSENT_CI("resolvepath",	resolvepath,	3),
	/* 210 */ SYSENT_CI("signotifywait",	signotifywait, 0),
	/* 211 */ SYSENT_CI("lwp_sigredirect",	lwp_sigredirect, 3),
	/* 212 */ SYSENT_CI("lwp_alarm",	lwp_alarm, 1),
	/*
	 * Syscalls 213-225: 32-bit system call support for large files.
	 *
	 * (The 64-bit C library transparently maps these system calls
	 * back to their native versions, so almost all of them are only
	 * needed as native syscalls on the 32-bit kernel).
	 */
	/* 213 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("getdents64",	getdents64,	3)),
	/* 214 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_AP("smmaplf32",	smmaplf32,	7)),
	/* 215 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("stat64",	stat64, 	2)),
	/* 216 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("lstat64",	lstat64,	2)),
	/* 217 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("fstat64", 	fstat64, 	2)),
	/* 218 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("statvfs64", 	statvfs64, 	2)),
	/* 219 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("fstatvfs64",	fstatvfs64, 	2)),
	/* 220 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("setrlimit64", setrlimit64, 	2)),
	/* 221 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("getrlimit64", getrlimit64, 	2)),
	/* 222 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("pread64",	pread64, 	5)),
	/* 223 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("pwrite64", 	pwrite64, 	5)),
	/* 224 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("creat64",	creat64,	2)),
	/* 225 */ IF_LP64(
			SYSENT_NOSYS(),
			SYSENT_CI("open64",	open64,		3)),
	/* 226 */ SYSENT_LOADABLE(),		/* rpcsys */
	/* 227 */ SYSENT_LOADABLE(),
	/* 228 */ SYSENT_LOADABLE(),
	/* 229 */ SYSENT_LOADABLE(),
	/* 230 */ SYSENT_CI("so_socket",	so_socket,	5),
	/* 231 */ SYSENT_CI("so_socketpair",	so_socketpair,	1),
	/* 232 */ SYSENT_CI("bind",		bind,		4),
	/* 233 */ SYSENT_CI("listen",		listen,		3),
	/* 234 */ SYSENT_CI("accept",		accept,		4),
	/* 235 */ SYSENT_CI("connect",		connect,	4),
	/* 236 */ SYSENT_CI("shutdown",		shutdown,	3),
	/* 237 */ SYSENT_CL("recv",		recv,		4),
	/* 238 */ SYSENT_CL("recvfrom",		recvfrom,	6),
	/* 239 */ SYSENT_CL("recvmsg",		recvmsg,	3),
	/* 240 */ SYSENT_CL("send",		send,		4),
	/* 241 */ SYSENT_CL("sendmsg",		sendmsg,	3),
	/* 242 */ SYSENT_CL("sendto",		sendto,		6),
	/* 243 */ SYSENT_CI("getpeername",	getpeername,	4),
	/* 244 */ SYSENT_CI("getsockname",	getsockname,	4),
	/* 245 */ SYSENT_CI("getsockopt",	getsockopt,	6),
	/* 246 */ SYSENT_CI("setsockopt",	setsockopt,	6),
	/* 247 */ SYSENT_CI("sockconfig",	sockconfig,	4),
	/* 248 */ SYSENT_CI("ntp_gettime",	ntp_gettime,	1),
	/* 249 */ SYSENT_CI("ntp_adjtime",	ntp_adjtime,	1),
	/* 250 */ SYSENT_CI("lwp_mutex_unlock", lwp_mutex_unlock,	1),
	/* 251 */ SYSENT_CI("lwp_mutex_trylock", lwp_mutex_trylock,	1),
	/* 252 */ SYSENT_CI("lwp_mutex_init", lwp_mutex_init,		2),
	/* 253 */ SYSENT_CI("cladm",		cladm,		3),
	/* 254 */ SYSENT_CI("lwp_sigtimedwait",	lwp_sigtimedwait,	4),
	/* 255 */ SYSENT_CI("umount2",		umount2,	2)
/* ONC_PLUS EXTRACT START */
};
/* ONC_PLUS EXTRACT END */


#ifdef _SYSCALL32_IMPL

extern int ulimit32();
extern ssize32_t read32();
extern ssize32_t write32();
extern ssize32_t pread32();
extern ssize32_t pwrite32();
extern ssize32_t readv32();
extern ssize32_t writev32();
extern int open32();
extern int creat32();
extern int xstat32();
extern int lxstat32();
extern int fxstat32();
extern int stat32();
extern int lstat32();
extern int fstat32();
extern int stat64_32();
extern int lstat64_32();
extern int fstat64_32();
extern int getmsg32();
extern int putmsg32();
extern int getpmsg32();
extern int putpmsg32();
extern int getsetcontext32();
extern int statvfs32();
extern int fstatvfs32();
extern int statvfs64_32();
extern int fstatvfs64_32();
extern int sigaction32();
extern clock32_t times32();
extern int stime32();


extern int sigaltstack32();
extern int sigqueue32();
extern offset_t	llseek32();
extern int waitsys32();

/* ONC_PLUS EXTRACT START */
/*
 * sysent table for ILP32 processes running on
 * a LP64 kernel.
 */
struct sysent sysent32[NSYSCALL] =
{
/* ONC_PLUS EXTRACT END */
	/*  0 */ SYSENT_C("indir",		indir,		1),
	/*  1 */ SYSENT_CI("exit",	(int (*)())rexit,	1),
	/*  2 */ SYSENT_2CI("fork",		fork,		0),
	/*  3 */ SYSENT_CI("read",		read32,		3),
	/*  4 */ SYSENT_CI("write",		write32,	3),
	/*  5 */ SYSENT_CI("open",		open32,		3),
	/*  6 */ SYSENT_CI("close",		close,		1),
	/*  7 */ SYSENT_2CI("wait",		wait,		0),
	/*  8 */ SYSENT_CI("creat",		creat32,	2),
	/*  9 */ SYSENT_CI("link",		link,		2),
	/* 10 */ SYSENT_CI("unlink",		unlink,		1),
	/* 11 */ SYSENT_CI("exec",		exec,		2),
	/* 12 */ SYSENT_CI("chdir",		chdir,		1),
	/* 13 */ SYSENT_CI("time",		gtime,		0),
	/* 14 */ SYSENT_CI("mknod",		mknod,		3),
	/* 15 */ SYSENT_CI("chmod",		chmod,		2),
	/* 16 */ SYSENT_CI("chown",		chown,		3),
	/* 17 */ SYSENT_CI("brk",		brk,		1),
	/* 18 */ SYSENT_CI("stat",		stat32,		2),
	/* 19 */ SYSENT_CI("lseek",		lseek32,	3),
	/* 20 */ SYSENT_2CI("getpid",		getpid,		0),
	/* 21 */ SYSENT_AP("mount",		mount,		8),
	/* 22 */ SYSENT_CI("umount",		umount,		1),
	/* 23 */ SYSENT_CI("setuid",		setuid,		1),
	/* 24 */ SYSENT_2CI("getuid",		getuid,		0),
	/* 25 */ SYSENT_CI("stime",		stime32,	1),
	/* 26 */ SYSENT_CI("pcsample",		pcsample,	2),
	/* 27 */ SYSENT_CI("alarm",		alarm,		1),
	/* 28 */ SYSENT_CI("fstat",		fstat32,	2),
	/* 29 */ SYSENT_CI("pause",		pause,		0),
	/* 30 */ SYSENT_CI("utime",		utime,		2),
	/* 31 */ SYSENT_CI("stty",		stty,		2),
	/* 32 */ SYSENT_CI("gtty",		gtty,		2),
	/* 33 */ SYSENT_CI("access",		access,		2),
	/* 34 */ SYSENT_CI("nice",		nice,		1),
	/* 35 */ SYSENT_CI("statfs",		statfs32,	4),
	/* 36 */ SYSENT_CI("sync",		syssync,	0),
	/* 37 */ SYSENT_CI("kill",		kill,		2),
	/* 38 */ SYSENT_CI("fstatfs",		fstatfs32,	4),
	/* 39 */ SYSENT_CI("setpgrp",		setpgrp,	3),
	/* 40 */ SYSENT_LOADABLE32(),			/* (was cxenix) */
	/* 41 */ SYSENT_CI("dup",		dup,		1),
	/* 42 */ SYSENT_LOADABLE32(),			/* (was pipe ) */
	/* 43 */ SYSENT_CI("times",		times32,	1),
	/* 44 */ SYSENT_CI("prof",		profil,		4),
	/* 45 */ SYSENT_LOADABLE32(),			/* (was proc lock) */
	/* 46 */ SYSENT_CI("setgid",		setgid,		1),
	/* 47 */ SYSENT_2CI("getgid",		getgid,		0),
	/* 48 */ SYSENT_CI("sig",		ssig,		2),
	/* 49 */ SYSENT_LOADABLE32(),			/* (was msgsys) */
	/* 50 */ IF_intel(
			SYSENT_AP("sysi86",	sysi86,		4),
			SYSENT_LOADABLE()),		/* (was sys3b) */
	/* 51 */ SYSENT_LOADABLE32(),			/* sysacct */
	/* 52 */ SYSENT_LOADABLE32(),			/* shmsys */
	/* 53 */ SYSENT_LOADABLE32(),			/* semsys */
	/* 54 */ SYSENT_CI("ioctl",		ioctl,		3),
	/* 55 */ SYSENT_CI("uadmin",		uadmin,		3),
#ifdef MEGA
	/* 56 */ SYSENT_AP("uexch",		uexch,		3),
#else
	/* 56 */ SYSENT_LOADABLE32(),
#endif /* MEGA */
	/* 57 */ SYSENT_2CI("utssys",		utssys32,	4),
	/* 58 */ SYSENT_CI("fdsync",		fdsync,		2),
	/* 59 */ SYSENT_CI("exece",		exece,		3),
	/* 60 */ SYSENT_CI("umask",		umask,		1),
	/* 61 */ SYSENT_CI("chroot",		chroot,		1),
	/* 62 */ SYSENT_CI("fcntl",		fcntl,		3),
	/* 63 */ SYSENT_CI("ulimit",		ulimit32,	2),

	/*
	 * The following 6 entries were reserved for the UNIX PC.
	 */
	/* 64 */ SYSENT_LOADABLE32(),
	/* 65 */ SYSENT_LOADABLE32(),
	/* 66 */ SYSENT_LOADABLE32(),
	/* 67 */ SYSENT_LOADABLE32(),		/* file locking call */
	/* 68 */ SYSENT_LOADABLE32(),		/* local system calls */
	/* 69 */ SYSENT_LOADABLE32(),		/* inode open */

	/* 70 */ SYSENT_NOSYS(),		/* 70 reserved */
	/* 71 */ SYSENT_NOSYS(),		/* 71 reserved */
	/* 72 */ SYSENT_NOSYS(),		/* 72 reserved */
	/* 73 */ SYSENT_NOSYS(),		/* 73 reserved */
	/* 74 */ SYSENT_NOSYS(),		/* 74 reserved */
	/* 75 */ SYSENT_NOSYS(),		/* 75 reserved */
	/* 76 */ SYSENT_NOSYS(),		/* 76 reserved */
	/* 77 */ SYSENT_NOSYS(),		/* 77 reserved */
	/* 78 */ SYSENT_NOSYS(),		/* 78 reserved */
	/* 79 */ SYSENT_CI("rmdir",		rmdir,		1),
	/* 80 */ SYSENT_CI("mkdir",		mkdir,		2),
	/* 81 */ SYSENT_CI("getdents",		getdents32,	3),
	/* 82 */ SYSENT_LOADABLE32(),		/* (was libattach) */
	/* 83 */ SYSENT_LOADABLE32(),		/* (was libdetach) */
	/* 84 */ SYSENT_CI("sysfs",		sysfs,		3),
	/* 85 */ SYSENT_CI("getmsg",		getmsg32,	4),
	/* 86 */ SYSENT_CI("putmsg",		putmsg32,	4),
	/* 87 */ SYSENT_CI("poll",		poll,		3),
	/* 88 */ SYSENT_CI("lstat",		lstat32,	2),
	/* 89 */ SYSENT_CI("symlink",		symlink,	2),
	/* 90 */ SYSENT_CI("readlink",		readlink,	3),
	/* 91 */ SYSENT_CI("setgroups",		setgroups,	2),
	/* 92 */ SYSENT_CI("getgroups",		getgroups,	2),
	/* 93 */ SYSENT_CI("fchmod",		fchmod,		2),
	/* 94 */ SYSENT_CI("fchown",		fchown,		3),
	/* 95 */ SYSENT_CI("sigprocmask",	sigprocmask,	3),
	/* 96 */ SYSENT_CI("sigsuspend",	sigsuspend,	1),
	/* 97 */ SYSENT_CI("sigaltstack",	sigaltstack32,	2),
	/* 98 */ SYSENT_CI("sigaction",		sigaction32,	3),
	/* 99 */ SYSENT_CI("sigpending",	sigpending,	2),

	/* 100 */ SYSENT_CI("getsetcontext",	getsetcontext32, 2),
	/* 101 */ SYSENT_LOADABLE32(),		/* (was evsys) */
	/* 102 */ SYSENT_LOADABLE32(),		/* (was evtrapret) */
	/* 103 */ SYSENT_CI("statvfs",		statvfs32,	2),
	/* 104 */ SYSENT_CI("fstatvfs",		fstatvfs32,	2),
	/* 105 */ SYSENT_CI("getloadavg",	getloadavg,	2),
/* ONC_PLUS EXTRACT START */
	/* 106 */ SYSENT_LOADABLE32(),		/* nfssys */
/* ONC_PLUS EXTRACT END */
	/* 107 */ SYSENT_CI("waitsys",		waitsys32,	4),
	/* 108 */ SYSENT_CI("sigsendset",	sigsendsys,	2),
	/* 109 */ IF_i386(
			SYSENT_AP("hrtsys",	hrtsys,		5),
			SYSENT_LOADABLE32()),
	/* 110 */ SYSENT_LOADABLE32(),		/* was acancel */
	/* 111 */ SYSENT_LOADABLE32(),		/* was async */
	/* 112 */ SYSENT_CI("priocntlsys",	priocntlsys,	4),
	/* 113 */ SYSENT_CI("pathconf",		pathconf,	2),
	/* 114 */ SYSENT_CI("mincore",		mincore,	3),
	/* 115 */ SYSENT_CI("mmap",		smmap32,	6),
	/* 116 */ SYSENT_CI("mprotect",		mprotect,	3),
	/* 117 */ SYSENT_CI("munmap",		munmap,		2),
	/* 118 */ SYSENT_CI("fpathconf",	fpathconf,	2),
	/* 119 */ SYSENT_2CI("vfork",		vfork,		0),
	/* 120 */ SYSENT_CI("fchdir",		fchdir,		1),
	/* 121 */ SYSENT_CI("readv",		readv32,	3),
	/* 122 */ SYSENT_CI("writev",		writev32,	3),
	/* 123 */ SYSENT_CI("xstat",		xstat32,	3),
	/* 124 */ SYSENT_CI("lxstat",		lxstat32,	3),
	/* 125 */ SYSENT_CI("fxstat",		fxstat32,	3),
	/* 126 */ SYSENT_CI("xmknod",		xmknod,		4),
	/* 127 */ SYSENT_LOADABLE32(),		/* was clocal */
	/* 128 */ SYSENT_CI("setrlimit",	setrlimit32,	2),
	/* 129 */ SYSENT_CI("getrlimit",	getrlimit32,	2),
	/* 130 */ SYSENT_CI("lchown",		lchown,		3),
	/* 131 */ SYSENT_CI("memcntl",		memcntl,	6),
	/* 132 */ SYSENT_CI("getpmsg",		getpmsg32,	5),
	/* 133 */ SYSENT_CI("putpmsg",		putpmsg32,	5),
	/* 134 */ SYSENT_CI("rename",		rename,		2),

	/* 135 */ SYSENT_CI("uname",		uname,		1),
	/* 136 */ SYSENT_CI("setegid",		setegid,	1),
	/* 137 */ SYSENT_CI("sysconfig",	sysconfig,	1),
	/* 138 */ SYSENT_CI("adjtime",		adjtime,	2),
	/* 139 */ SYSENT_CI("systeminfo",	systeminfo,	3),
	/* 140 */ SYSENT_LOADABLE32(),		/* reserved */
	/* 141 */ SYSENT_CI("seteuid",		seteuid,	1),
#ifdef	TRACE
	/* 142 */ SYSENT_AP("vtrace",		vtrace,		3),
#else
	/* 142 */ SYSENT_LOADABLE32(),		/* vtrace */
#endif	/* TRACE */
	/* 143 */ SYSENT_2CI("fork1",		fork1,		0),
	/* 144 */ SYSENT_CI("sigtimedwait",	sigtimedwait,	3),
	/* 145 */ SYSENT_CI("lwp_info",		lwp_info,	1),
	/* 146 */ SYSENT_CI("yield",		yield,		0),
	/* 147 */ SYSENT_CI("lwp_sema_wait",	lwp_sema_wait,	1),
	/* 148 */ SYSENT_CI("lwp_sema_post",	lwp_sema_post,	1),
	/* 149 */ SYSENT_CI("lwp_sema_trywait",	lwp_sema_trywait, 1),
	/* 150 */ SYSENT_LOADABLE32(),		/* reserved */
	/* 151 */ SYSENT_CI("corectl",		corectl,	4),
	/* 152 */ SYSENT_CI("modctl",		modctl,		6),
	/* 153 */ SYSENT_CI("fchroot",		fchroot,	1),
	/* 154 */ SYSENT_CI("utimes",		utimes,		2),
	/* 155 */ SYSENT_CI("vhangup",		vhangup,	0),
	/* 156 */ SYSENT_CI("gettimeofday",	gettimeofday,	1),
	/* 157 */ SYSENT_CI("getitimer",	getitimer,	2),
	/* 158 */ SYSENT_CI("setitimer",	setitimer,	3),
	/* 159 */ SYSENT_CI("lwp_create",	syslwp_create,	3),
	/* 160 */ SYSENT_CI("lwp_exit",	(int (*)())syslwp_exit,	0),
	/* 161 */ SYSENT_2CI("lwp_stop",	syslwp_suspend,	1),
	/* 162 */ SYSENT_CI("lwp_continue",	syslwp_continue, 1),
	/* 163 */ SYSENT_CI("lwp_kill",		lwp_kill,	2),
	/* 164 */ SYSENT_CI("lwp_get_id",	lwp_self,	0),
	/* 165 */ SYSENT_CI("lwp_setprivate",	lwp_setprivate,	1),
	/* 166 */ SYSENT_CI("lwp_getprivate",	lwp_getprivate,	0),
	/* 167 */ SYSENT_CI("lwp_wait",		lwp_wait,	2),
	/* 168 */ SYSENT_CI("lwp_mutex_wakeup",	lwp_mutex_wakeup,	1),
	/* 169 */ SYSENT_CI("lwp_mutex_lock",	lwp_mutex_lock,		1),
	/* 170 */ SYSENT_CI("lwp_cond_wait",	lwp_cond_wait,		3),
	/* 171 */ SYSENT_CI("lwp_cond_signal",	lwp_cond_signal,	1),
	/* 172 */ SYSENT_CI("lwp_cond_broadcast", lwp_cond_broadcast,	1),
	/* 173 */ SYSENT_CI("pread",		pread32,		4),
	/* 174 */ SYSENT_CI("pwrite",		pwrite32,		4),
	/* 175 */ SYSENT_C("llseek",		llseek32,	4),
	/* 176 */ SYSENT_LOADABLE32(),		/* inst_sync */
	/* 177 */ SYSENT_LOADABLE32(),		/* srmlimitsys */
	/* 178 */ SYSENT_LOADABLE32(),		/* kaio */
	/* 179 */ SYSENT_LOADABLE32(),		/* cpc */
	/* 180 */ SYSENT_LOADABLE32(),
	/* 181 */ SYSENT_LOADABLE32(),
	/* 182 */ SYSENT_LOADABLE32(),
	/* 183 */ SYSENT_LOADABLE32(),
	/* 184 */ SYSENT_LOADABLE32(),		/* tsolsys */
	/* 185 */ SYSENT_CI("acl",		acl,		4),
	/* 186 */ SYSENT_AP("auditsys",		auditsys,	2),
	/* 187 */ SYSENT_CI("processor_bind",	processor_bind,	4),
	/* 188 */ SYSENT_CI("processor_info",	processor_info,	2),
	/* 189 */ SYSENT_CI("p_online",		p_online,	2),
	/* 190 */ SYSENT_CI("sigqueue",		sigqueue32,	4),
	/* 191 */ SYSENT_CI("clock_gettime",	clock_gettime,	2),
	/* 192 */ SYSENT_CI("clock_settime",	clock_settime,	2),
	/* 193 */ SYSENT_CI("clock_getres",	clock_getres,	2),
	/* 194 */ SYSENT_CI("timer_create",	timer_create,	3),
	/* 195 */ SYSENT_CI("timer_delete",	timer_delete,	1),
	/* 196 */ SYSENT_CI("timer_settime",	timer_settime,	4),
	/* 197 */ SYSENT_CI("timer_gettime",	timer_gettime,	2),
	/* 198 */ SYSENT_CI("timer_getoverrun",	timer_getoverrun, 1),
	/* 199 */ SYSENT_CI("nanosleep",	nanosleep,	2),
	/* 200 */ SYSENT_CI("facl",		facl,		4),
	/* 201 */ SYSENT_LOADABLE32(),		/* door */
	/* 202 */ SYSENT_CI("setreuid",		setreuid,	2),
	/* 203 */ SYSENT_CI("setregid",		setregid,	2),
	/* 204 */ SYSENT_CI("install_utrap",	install_utrap,	3),
	/* 205 */ SYSENT_CI("signotify",	signotify,	3),
	/* 206 */ SYSENT_CI("schedctl",		schedctl,	3),
	/* 207 */ SYSENT_LOADABLE32(),		/* pset */
	/* 208 */ SYSENT_LOADABLE32(),
	/* 209 */ SYSENT_CI("resolvepath",	resolvepath,	3),
	/* 210 */ SYSENT_CI("signotifywait",	signotifywait, 0),
	/* 211 */ SYSENT_CI("lwp_sigredirect",	lwp_sigredirect, 3),
	/* 212 */ SYSENT_CI("lwp_alarm",	lwp_alarm, 1),
	/*
	 * Syscalls 213-225: 32-bit system call support for large files.
	 */
	/* 213 */ SYSENT_CI("getdents64",	getdents64,	3),
	/* 214 */ SYSENT_AP("smmaplf32", 	smmaplf32, 	7),
	/* 215 */ SYSENT_CI("stat64", 		stat64_32, 	2),
	/* 216 */ SYSENT_CI("lstat64", 		lstat64_32,	2),
	/* 217 */ SYSENT_CI("fstat64", 		fstat64_32, 	2),
	/* 218 */ SYSENT_CI("statvfs64", 	statvfs64_32, 	2),
	/* 219 */ SYSENT_CI("fstatvfs64", 	fstatvfs64_32, 	2),
	/* 220 */ SYSENT_CI("setrlimit64", 	setrlimit64, 	2),
	/* 221 */ SYSENT_CI("getrlimit64", 	getrlimit64, 	2),
	/* 222 */ SYSENT_CI("pread64", 		pread64, 	5),
	/* 223 */ SYSENT_CI("pwrite64", 	pwrite64, 	5),
	/* 224 */ SYSENT_CI("creat64",		creat64,	2),
	/* 225 */ SYSENT_CI("open64",		open64,		3),
	/* 226 */ SYSENT_LOADABLE32(),	/* rpcsys */
	/* 227 */ SYSENT_LOADABLE32(),
	/* 228 */ SYSENT_LOADABLE32(),
	/* 229 */ SYSENT_LOADABLE32(),
	/* 230 */ SYSENT_CI("so_socket",	so_socket,	5),
	/* 231 */ SYSENT_CI("so_socketpair",	so_socketpair,	1),
	/* 232 */ SYSENT_CI("bind",		bind,		4),
	/* 233 */ SYSENT_CI("listen",		listen,		3),
	/* 234 */ SYSENT_CI("accept",		accept,		4),
	/* 235 */ SYSENT_CI("connect",		connect,	4),
	/* 236 */ SYSENT_CI("shutdown",		shutdown,	3),
	/* 237 */ SYSENT_CI("recv",		recv,		4),
	/* 238 */ SYSENT_CI("recvfrom",		recvfrom,	6),
	/* 239 */ SYSENT_CI("recvmsg",		recvmsg,	3),
	/* 240 */ SYSENT_CI("send",		send,		4),
	/* 241 */ SYSENT_CI("sendmsg",		sendmsg,	3),
	/* 242 */ SYSENT_CI("sendto",		sendto,		6),
	/* 243 */ SYSENT_CI("getpeername",	getpeername,	4),
	/* 244 */ SYSENT_CI("getsockname",	getsockname,	4),
	/* 245 */ SYSENT_CI("getsockopt",	getsockopt,	6),
	/* 246 */ SYSENT_CI("setsockopt",	setsockopt,	6),
	/* 247 */ SYSENT_CI("sockconfig",	sockconfig,	4),
	/* 248 */ SYSENT_CI("ntp_gettime",	ntp_gettime,	1),
	/* 249 */ SYSENT_CI("ntp_adjtime",	ntp_adjtime,	1),
	/* 250 */ SYSENT_CI("lwp_mutex_unlock", lwp_mutex_unlock,	1),
	/* 251 */ SYSENT_CI("lwp_mutex_trylock", lwp_mutex_trylock,	1),
	/* 252 */ SYSENT_CI("lwp_mutex_init", lwp_mutex_init,		2),
	/* 253 */ SYSENT_CI("cladm",		cladm,		3),
	/* 254 */ SYSENT_CI("lwp_sigtimedwait",	lwp_sigtimedwait,	4),
	/* 255 */ SYSENT_CI("umount2",		umount2,	2)
/* ONC_PLUS EXTRACT START */
};
/* ONC_PLUS EXTRACT END */
#endif /* _SYSCALL32_IMPL */

/*
 * Space allocated and initialized in init_syscallnames().
 */
char **syscallnames;
