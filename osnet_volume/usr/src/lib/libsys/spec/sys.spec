#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)sys.spec	1.1	99/01/25 SMI"
#
# lib/libsys/spec/sys.spec

function	access extends libc/spec/sys.spec access
version		SYSVABI_1.2
end		

function	acct extends libc/spec/sys.spec acct
version		SYSVABI_1.2
end		

function	alarm extends libc/spec/sys.spec alarm
version		SYSVABI_1.2
end		

function	atexit extends libc/spec/gen.spec atexit
version		SYSVABI_1.2
end		

function	.div extends libc/spec/gen.spec .div
arch		sparc
version		SYSVABI_1.2
end		

function	.mul extends libc/spec/gen.spec
arch		sparc
version		SYSVABI_1.2
end		

function	.rem extends libc/spec/gen.spec
arch		sparc
version		SYSVABI_1.2
end		

function	.stret1 extends libc/spec/sys.spec .stret1
arch		sparc
version		SYSVABI_1.2
end		

function	.stret2 extends libc/spec/sys.spec .stret2
arch		sparc
version		SYSVABI_1.2
end		

function	.stret4 extends libc/spec/sys.spec .stret4
arch		sparc
version		SYSVABI_1.2
end		

function	.stret8 extends libc/spec/sys.spec .stret8
arch		sparc
version		SYSVABI_1.2
end		

function	.udiv extends libc/spec/sys.spec
arch		sparc
version		SYSVABI_1.2
end		

function	.umul extends libc/spec/sys.spec
arch		sparc
version		SYSVABI_1.2
end		

function	.urem extends libc/spec/gen.spec .rem
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_add extends libc/spec/sys.spec _Q_add
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_cmp extends libc/spec/sys.spec _Q_cmp
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_cmpe extends libc/spec/sys.spec _Q_cmpe
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_div extends libc/spec/gen.spec .div
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_dtoq extends libc/spec/sys.spec _Q_dtoq
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_feq extends libc/spec/sys.spec _Q_feq
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_fge extends libc/spec/sys.spec _Q_fge
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_fgt extends libc/spec/sys.spec _Q_fgt
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_fle extends libc/spec/sys.spec _Q_fle
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_flt extends libc/spec/sys.spec _Q_flt
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_fne extends libc/spec/sys.spec _Q_fne
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_itoq extends libc/spec/sys.spec _Q_itoq
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_mul extends libc/spec/gen.spec .mul
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_neg extends libc/spec/sys.spec _Q_neg
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_qtod extends libc/spec/sys.spec _Q_qtod
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_qtoi extends libc/spec/sys.spec _Q_qtoi
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_qtos extends libc/spec/sys.spec _Q_qtos
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_qtou extends libc/spec/sys.spec _Q_qtou
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_sqrt extends libc/spec/sys.spec _Q_sqrt
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_stoq extends libc/spec/sys.spec _Q_stoq
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_sub extends libc/spec/sys.spec _Q_sub
arch		sparc
version		SYSVABI_1.2
end		

function	_Q_utoq extends libc/spec/sys.spec _Q_utoq
arch		sparc
version		SYSVABI_1.2
end		

function	chdir extends libc/spec/sys.spec chdir
version		SYSVABI_1.2
end		

function	fchdir extends libc/spec/sys.spec fchdir
version		SYSVABI_1.2
end		

function	chmod extends libc/spec/sys.spec chmod
version		SYSVABI_1.2
end		

function	fchmod extends libc/spec/sys.spec fchmod
version		SYSVABI_1.2
end		

function	chown extends libc/spec/sys.spec chown
version		SYSVABI_1.2
end		

function	lchown extends libc/spec/sys.spec lchown
version		SYSVABI_1.2
end		

function	fchown extends libc/spec/sys.spec fchown
version		SYSVABI_1.2
end		

function	chroot extends libc/spec/sys.spec chroot
version		SYSVABI_1.2
end		

function	close extends libc/spec/sys.spec close
version		SYSVABI_1.2
end		

function	creat extends libc/spec/sys.spec creat
version		SYSVABI_1.2
end		

function	closedir extends libc/spec/gen.spec closedir
version		SYSVABI_1.2
end		

function	opendir extends libc/spec/gen.spec opendir
version		SYSVABI_1.2
end		

function	readdir extends libc/spec/gen.spec readdir
version		SYSVABI_1.2
end		

function	rewinddir extends libc/spec/gen.spec rewinddir
include		"sys_spec.h"
version		SYSVABI_1.2
end		

function	seekdir extends libc/spec/gen.spec seekdir
version		SYSVABI_1.2
end		

function	telldir extends libc/spec/gen.spec telldir
version		SYSVABI_1.2
end		

function	dup extends libc/spec/sys.spec dup
version		SYSVABI_1.2
end		

function	__dtou
arch		sparc
version		SYSVABI_1.2
end		

function	__flt_rounds
arch		i386
version		SYSVABI_1.2
end		

function	__ftou
arch		sparc
version		SYSVABI_1.2
end		

function	_alarm
version		SYSVABI_1.2
end		

function	_fcntl
version		SYSVABI_1.2
end		

function	_fork
version		SYSVABI_1.2
end		

function	_rename
version		SYSVABI_1.2
end		

function	_sigaction
version		SYSVABI_1.2
end		

function	_siglongjmp
version		SYSVABI_1.2
end		

function	_sigprocmask
version		SYSVABI_1.2
end		

function	exit extends libc/spec/sys.spec exit
version		SYSVABI_1.2
end		

function	_exit extends libc/spec/sys.spec _exit
version		SYSVABI_1.2
end		

function	fattach extends libc/spec/gen.spec fattach
version		SYSVABI_1.2
end		

function	fdetach extends libc/spec/gen.spec fdetach
version		SYSVABI_1.2
end		

function	fcntl extends libc/spec/sys.spec fcntl
version		SYSVABI_1.2
end		

function	fork extends libc/spec/sys.spec fork
version		SYSVABI_1.2
end		

function	fpathconf extends libc/spec/sys.spec fpathconf
version		SYSVABI_1.2
end		

function	pathconf extends libc/spec/sys.spec pathconf
version		SYSVABI_1.2
end		

function	fsync extends libc/spec/sys.spec fsync
version		SYSVABI_1.2
end		

function	ftok extends libc/spec/gen.spec ftok
version		SYSVABI_1.2
end		

function	getcontext extends libc/spec/sys.spec getcontext
version		SYSVABI_1.2
end		

function	setcontext extends libc/spec/sys.spec setcontext
version		SYSVABI_1.2
end		

function	getcwd extends libc/spec/gen.spec getcwd
version		SYSVABI_1.2
end		

function	getgrnam extends libc/spec/gen.spec getgrnam
version		SYSVABI_1.2
end		

function	getgrgid extends libc/spec/gen.spec getgrgid
version		SYSVABI_1.2
end		

function	getgroups extends libc/spec/sys.spec getgroups
version		SYSVABI_1.2
end		

function	setgroups extends libc/spec/sys.spec setgroups
version		SYSVABI_1.2
end		

function	getlogin extends libc/spec/gen.spec getlogin
version		SYSVABI_1.2
end		

function	getmsg extends libc/spec/sys.spec getmsg
version		SYSVABI_1.2
end		

function	getpmsg extends libc/spec/sys.spec getpmsg
version		SYSVABI_1.2
end		

function	getpid extends libc/spec/sys.spec getpid
version		SYSVABI_1.2
end		

function	getpgrp extends libc/spec/sys.spec getpgrp
version		SYSVABI_1.2
end		

function	getppid extends libc/spec/sys.spec getppid
version		SYSVABI_1.2
end		

function	getpgid extends libc/spec/sys.spec getpgid
version		SYSVABI_1.2
end		

function	getpwnam extends libc/spec/gen.spec getpwnam
version		SYSVABI_1.2
end		

function	getpwuid extends libc/spec/gen.spec getpwuid
version		SYSVABI_1.2
end		

function	getrlimit extends libc/spec/sys.spec getrlimit
version		SYSVABI_1.2
end		

function	setrlimit extends libc/spec/sys.spec setrlimit
version		SYSVABI_1.2
end		

function	getsid extends libc/spec/sys.spec getsid
version		SYSVABI_1.2
end		

function	gettxt extends libc/spec/gen.spec gettxt
version		SYSVABI_1.2
end		

function	getuid extends libc/spec/sys.spec getuid
version		SYSVABI_1.2
end		

function	geteuid extends libc/spec/sys.spec geteuid
version		SYSVABI_1.2
end		

function	getgid extends libc/spec/sys.spec getgid
version		SYSVABI_1.2
end		

function	getegid extends libc/spec/sys.spec getegid
version		SYSVABI_1.2
end		

function	grantpt extends libc/spec/gen.spec grantpt
version		SYSVABI_1.2
end		

function	initgroups extends libc/spec/gen.spec initgroups
version		SYSVABI_1.2
end		

function	ioctl extends libc/spec/sys.spec ioctl
version		SYSVABI_1.2
end		

function	isastream extends libc/spec/gen.spec isastream
version		SYSVABI_1.2
end		

function	kill extends libc/spec/sys.spec kill
version		SYSVABI_1.2
end		

function	link extends libc/spec/sys.spec link
version		SYSVABI_1.2
end		

function	localeconv extends libc/spec/i18n.spec localeconv
version		SYSVABI_1.2
end		

function	lseek extends libc/spec/sys.spec lseek
version		SYSVABI_1.2
end		

function	makecontext extends libc/spec/gen.spec makecontext
version		SYSVABI_1.3
end		

function	swapcontext extends libc/spec/gen.spec swapcontext
version		SYSVABI_1.3
end		

function	malloc extends libc/spec/gen.spec malloc
version		SYSVABI_1.2
end		

function	calloc extends libc/spec/gen.spec calloc
version		SYSVABI_1.2
end		

function	free extends libc/spec/gen.spec free
version		SYSVABI_1.2
end		

function	realloc extends libc/spec/gen.spec realloc
version		SYSVABI_1.2
end		

function	memcntl extends libc/spec/sys.spec memcntl
version		SYSVABI_1.2
end		

function	mkdir extends libc/spec/sys.spec mkdir
version		SYSVABI_1.2
end		

function	mknod extends libc/spec/sys.spec mknod
version		SYSVABI_1.2
end		

function	mlock extends libc/spec/gen.spec mlock
version		SYSVABI_1.2
end		

function	munlock extends libc/spec/gen.spec munlock
version		SYSVABI_1.2
end		

function	mmap extends libc/spec/sys.spec mmap
version		SYSVABI_1.2
end		

function	mount extends libc/spec/sys.spec mount
version		SYSVABI_1.2
end		

function	mprotect extends libc/spec/sys.spec mprotect
version		SYSVABI_1.2
end		

function	msgctl extends libc/spec/sys.spec msgctl
version		SYSVABI_1.2
end		

function	msgget extends libc/spec/sys.spec msgget
version		SYSVABI_1.2
end		

function	msgrcv extends libc/spec/sys.spec msgrcv
version		SYSVABI_1.2
end		

function	msgsnd extends libc/spec/sys.spec msgsnd
version		SYSVABI_1.2
end		

function	msync extends libc/spec/gen.spec msync
version		SYSVABI_1.2
end		

function	munmap extends libc/spec/sys.spec munmap
version		SYSVABI_1.2
end		

function	nice extends libc/spec/sys.spec nice
version		SYSVABI_1.2
end		

function	open extends libc/spec/sys.spec open
version		SYSVABI_1.2
end		

function	pause extends libc/spec/sys.spec pause
version		SYSVABI_1.2
end		

function	pipe extends libc/spec/sys.spec pipe
version		SYSVABI_1.2
end		

function	poll extends libc/spec/sys.spec poll
version		SYSVABI_1.2
end		

function	profil extends libc/spec/sys.spec profil
version		SYSVABI_1.2
end		

function	ptrace extends libc/spec/gen.spec ptrace
version		SYSVABI_1.2
end		

function	ptsname extends libc/spec/gen.spec ptsname
version		SYSVABI_1.2
end		

function	putmsg extends libc/spec/gen.spec putmsg
version		SYSVABI_1.2
end		

function	putpmsg extends libc/spec/gen.spec putpmsg
version		SYSVABI_1.2
end		

function	read extends libc/spec/sys.spec read
version		SYSVABI_1.2
end		

function	readv extends libc/spec/sys.spec readv
version		SYSVABI_1.2
end		

function	readlink extends libc/spec/sys.spec readlink
version		SYSVABI_1.2
end		

function	remove extends libc/spec/gen.spec remove
version		SYSVABI_1.2
end		

function	rename extends libc/spec/gen.spec rename
version		SYSVABI_1.2
end		

function	rmdir extends libc/spec/sys.spec rmdir
version		SYSVABI_1.2
end		

function	setlocale extends libc/spec/i18n.spec setlocale
version		SYSVABI_1.2
end		

function	setpgid extends libc/spec/gen.spec setpgid
version		SYSVABI_1.2
end		

function	setpgrp extends libc/spec/sys.spec setpgrp
version		SYSVABI_1.2
end		

function	setsid extends libc/spec/sys.spec setsid
version		SYSVABI_1.2
end		

function	setuid extends libc/spec/sys.spec setuid
version		SYSVABI_1.2
end		

function	setgid extends libc/spec/sys.spec setgid
version		SYSVABI_1.2
end		

function	shmctl extends libc/spec/sys.spec shmctl
version		SYSVABI_1.2
end		

function	shmget extends libc/spec/sys.spec shmget
version		SYSVABI_1.2
end		

function	shmat extends libc/spec/sys.spec shmat
version		SYSVABI_1.2
end		

function	shmdt extends libc/spec/sys.spec shmdt
version		SYSVABI_1.2
end		

function	sigaction extends libaio/spec/aio.spec sigaction
version		SYSVABI_1.2
end		

function	sigaltstack extends libc/spec/sys.spec sigaltstack
version		SYSVABI_1.2
end		

function	signal extends libc/spec/gen.spec signal
version		SYSVABI_1.2
end		

function	sigset extends libc/spec/gen.spec sigset
version		SYSVABI_1.2
end		

function	sighold extends libc/spec/gen.spec sighold
version		SYSVABI_1.2
end		

function	sigrelse extends libc/spec/gen.spec sigrelse
version		SYSVABI_1.2
end		

function	sigignore extends libc/spec/gen.spec sigignore
version		SYSVABI_1.2
end		

function	sigpause extends libc/spec/sys.spec sigpause
version		SYSVABI_1.2
end		

function	sigemptyset extends libc/spec/gen.spec sigemptyset
version		SYSVABI_1.2
end		

function	sigfillset extends libc/spec/gen.spec sigfillset
version		SYSVABI_1.2
end		

function	sigaddset extends libc/spec/gen.spec sigaddset
version		SYSVABI_1.2
end		

function	sigdelset extends libc/spec/gen.spec sigdelset
version		SYSVABI_1.2
end		

function	sigismember extends libc/spec/gen.spec sigismember
version		SYSVABI_1.2
end		

function	sigpending extends libc/spec/sys.spec sigpending
version		SYSVABI_1.2
end		

function	sigprocmask extends libc/spec/sys.spec sigprocmask
version		SYSVABI_1.2
end		

function	sigsend extends libc/spec/gen.spec sigsend
version		SYSVABI_1.2
end		

function	sigsendset extends libc/spec/gen.spec sigsendset
version		SYSVABI_1.2
end		

function	sigsuspend extends libc/spec/sys.spec sigsuspend
version		SYSVABI_1.2
end		

function	stat extends libc/spec/sys.spec stat
version		SYSVABI_1.2
end		

function	lstat extends libc/spec/sys.spec lstat
version		SYSVABI_1.2
end		

function	fstat extends libc/spec/sys.spec fstat
version		SYSVABI_1.2
end		

function	statvfs extends libc/spec/sys.spec statvfs
version		SYSVABI_1.2
end		

function	fstatvfs extends libc/spec/sys.spec fstatvfs
version		SYSVABI_1.2
end		

function	stime extends libc/spec/sys.spec stime
version		SYSVABI_1.2
end		

function	strcoll extends libc/spec/i18n.spec strcoll
version		SYSVABI_1.2
end		

function	strerror extends libc/spec/gen.spec strerror
version		SYSVABI_1.2
end		

function	strftime extends libc/spec/gen.spec strftime
version		SYSVABI_1.2
end		

function	strxfrm extends libc/spec/i18n.spec strxfrm
version		SYSVABI_1.2
end		

function	symlink extends libc/spec/sys.spec symlink
version		SYSVABI_1.2
end		

function	sync extends libc/spec/sys.spec sync
version		SYSVABI_1.2
end		

function	sysconf extends libc/spec/gen.spec sysconf
version		SYSVABI_1.2
end		

function	system extends libc/spec/stdio.spec system
version		SYSVABI_1.2
end		

function	time extends libc/spec/sys.spec time
version		SYSVABI_1.2
end		

function	times extends libc/spec/sys.spec times
version		SYSVABI_1.2
end		

function	ttyname extends libc/spec/gen.spec ttyname
version		SYSVABI_1.2
end		

function	ulimit extends libc/spec/sys.spec ulimit
version		SYSVABI_1.2
end		

function	umask extends libc/spec/sys.spec umask
version		SYSVABI_1.2
end		

function	umount extends libc/spec/sys.spec umount
version		SYSVABI_1.2
end		

function	uname extends libc/spec/sys.spec uname
version		SYSVABI_1.2
end		

function	unlink extends libc/spec/sys.spec unlink
version		SYSVABI_1.2
end		

function	unlockpt extends libc/spec/gen.spec unlockpt
version		SYSVABI_1.2
end		

function	utime extends libc/spec/sys.spec utime
version		SYSVABI_1.2
end		

function	wait extends libc/spec/sys.spec wait
version		SYSVABI_1.2
end		

function	waitid extends libc/spec/sys.spec waitid
version		SYSVABI_1.2
end		

function	waitpid extends libc/spec/gen.spec waitpid
version		SYSVABI_1.2
end		

function	write extends libc/spec/sys.spec write
version		SYSVABI_1.2
end		

function	writev extends libc/spec/sys.spec writev
version		SYSVABI_1.2
end		

function	__ctype extends libc/spec/sys.spec _ctype
version		SYSVABI_1.2
end		

function	__huge_val extends libc/spec/data.spec __huge_val
version		SYSVABI_1.2
end		

function	_access extends libc/spec/sys.spec access
version		SYSVABI_1.2
end		

function	_acct extends libc/spec/sys.spec acct
version		SYSVABI_1.2
end		

function	_altzone extends libc/spec/sys.spec altzone
version		SYSVABI_1.2
end		

function	_catclose extends libc/spec/gen.spec catclose
version		SYSVABI_1.2
end		

function	_catgets extends libc/spec/gen.spec catgets
version		SYSVABI_1.2
end		

function	_catopen extends libc/spec/gen.spec catopen
version		SYSVABI_1.2
end		

function	_chdir extends libc/spec/sys.spec chdir
version		SYSVABI_1.2
end		

function	_chmod extends libc/spec/sys.spec chmod
version		SYSVABI_1.2
end		

function	_chown extends libc/spec/sys.spec chown
version		SYSVABI_1.2
end		

function	_chroot extends libc/spec/sys.spec chroot
version		SYSVABI_1.2
end		

function	_close extends libc/spec/sys.spec close
version		SYSVABI_1.2
end		

function	_closedir extends libc/spec/gen.spec closedir
version		SYSVABI_1.2
end		

function	_creat extends libc/spec/sys.spec creat
version		SYSVABI_1.2
end		

function	_daylight extends libc/spec/sys.spec daylight
version		SYSVABI_1.2
end		

function	_dup extends libc/spec/sys.spec dup
version		SYSVABI_1.2
end		

function	_environ extends libc/spec/sys.spec environ
version		SYSVABI_1.2
end		

function	_execl extends libc/spec/gen.spec execl
version		SYSVABI_1.2
end		

function	_execle extends libc/spec/gen.spec execle
version		SYSVABI_1.2
end		

function	_execlp extends libc/spec/gen.spec execlp
version		SYSVABI_1.2
end		

function	_execv extends libc/spec/gen.spec execv
version		SYSVABI_1.2
end		

function	_execve extends libc/spec/gen.spec execve
version		SYSVABI_1.2
end		

function	_execvp extends libc/spec/gen.spec execvp
version		SYSVABI_1.2
end		

function	_fattach extends libc/spec/gen.spec fattach
version		SYSVABI_1.2
end		

function	_fchdir extends libc/spec/sys.spec	fchdir
version		SYSVABI_1.2
end		

function	_fchmod extends libc/spec/sys.spec fchmod
version		SYSVABI_1.2
end		

function	_fchown extends libc/spec/sys.spec fchown
version		SYSVABI_1.2
end		

function	_fdetach extends libc/spec/gen.spec fdetach
version		SYSVABI_1.2
end		

function	_fp_hw extends libc/spec/missing.spec _fp_hw
arch		i386
version		SYSVABI_1.2
end		

function	_fpathconf extends libc/spec/sys.spec fpathconf
version		SYSVABI_1.2
end		

function	_fpstart	extends	libc/spec/fp.spec _fpstart
arch		i386
version		SYSVABI_1.2
end		

function	_fstat extends libc/spec/sys.spec fstat
version		SYSVABI_1.2
end		

function	_fstatvfs extends libc/spec/sys.spec fstatvfs
version		SYSVABI_1.2
end		

function	_fsync extends libc/spec/sys.spec fsync
version		SYSVABI_1.2
end		

function	_ftok extends libc/spec/gen.spec ftok
version		SYSVABI_1.2
end		

function	_fxstat extends libc/spec/missing.spec _fxstat
arch		i386
version		SYSVABI_1.2
end		

function	_getcontext extends libc/spec/sys.spec getcontext
version		SYSVABI_1.2
end		

function	_getcwd extends libc/spec/gen.spec getcwd
version		SYSVABI_1.2
end		

function	_getegid extends libc/spec/sys.spec getegid
version		SYSVABI_1.2
end		

function	_geteuid extends libc/spec/sys.spec geteuid
version		SYSVABI_1.2
end		

function	_getgid extends libc/spec/sys.spec getgid
version		SYSVABI_1.2
end		

function	_getgrgid extends libc/spec/gen.spec getgrgid
version		SYSVABI_1.2
end		

function	_getgrnam extends libc/spec/gen.spec getgrnam
version		SYSVABI_1.2
end		

function	_getgroups extends libc/spec/sys.spec getgroups
version		SYSVABI_1.2
end		

function	_getlogin	extends libc/spec/gen.spec getlogin
version		SYSVABI_1.2
end		

function	_getmsg extends libc/spec/sys.spec getmsg
version		SYSVABI_1.2
end		

function	_getpgid extends libc/spec/sys.spec getpgid
version		SYSVABI_1.2
end		

function	_getpgrp extends libc/spec/sys.spec getpgrp
version		SYSVABI_1.2
end		

function	_getpid extends libc/spec/sys.spec	getpid
version		SYSVABI_1.2
end		

function	_getpmsg extends libc/spec/sys.spec getpmsg
version		SYSVABI_1.2
end		

function	_getppid extends libc/spec/sys.spec getppid
version		SYSVABI_1.2
end		

function	_getpwnam extends libc/spec/gen.spec getpwnam
version		SYSVABI_1.2
end		

function	_getpwuid extends libc/spec/gen.spec getpwuid
version		SYSVABI_1.2
end		

function	_getrlimit extends libc/spec/sys.spec getrlimit
version		SYSVABI_1.2
end		

function	_getsid extends libc/spec/sys.spec getsid
version		SYSVABI_1.2
end		

function	_gettxt extends libc/spec/gen.spec gettxt
version		SYSVABI_1.2
end		

function	_getuid extends libc/spec/sys.spec getuid
version		SYSVABI_1.2
end		

function	_grantpt extends libc/spec/gen.spec grantpt
version		SYSVABI_1.2
end		

function	_initgroups extends libc/spec/gen.spec initgroups
version		SYSVABI_1.2
end		

function	_ioctl extends libc/spec/sys.spec ioctl
version		SYSVABI_1.2
end		

function	_isastream extends libc/spec/gen.spec isastream
version		SYSVABI_1.2
end		

function	_kill extends libc/spec/sys.spec kill
version		SYSVABI_1.2
end		

function	_lchown extends libc/spec/sys.spec lchown
version		SYSVABI_1.2
end		

function	_link extends libc/spec/sys.spec link
version		SYSVABI_1.2
end		

function	_lseek extends libc/spec/sys.spec lseek
version		SYSVABI_1.2
end		

function	_lstat extends libc/spec/sys.spec lstat
version		SYSVABI_1.2
end		

function	_lxstat extends libc/spec/missing.spec _lxstat
arch		i386
version		SYSVABI_1.2
end		

function	_makecontext extends libc/spec/gen.spec makecontext
version		SYSVABI_1.3
end		

function	_memcntl extends libc/spec/sys.spec memcntl
version		SYSVABI_1.2
end		

function	_mkdir extends libc/spec/sys.spec mkdir
version		SYSVABI_1.2
end		

function	_mknod extends libc/spec/sys.spec mknod
version		SYSVABI_1.2
end		

function	_mlock extends libc/spec/gen.spec mlock
version		SYSVABI_1.2
end		

function	_mmap extends libc/spec/sys.spec mmap
version		SYSVABI_1.2
end		

function	_mount extends libc/spec/sys.spec mount
version		SYSVABI_1.2
end		

function	_mprotect extends libc/spec/sys.spec mprotect
version		SYSVABI_1.2
end		

function	_msgctl extends libc/spec/sys.spec msgctl
version		SYSVABI_1.2
end		

function	_msgget extends libc/spec/sys.spec msgget
version		SYSVABI_1.2
end		

function	_msgrcv extends libc/spec/sys.spec msgrcv
version		SYSVABI_1.2
end		

function	_msgsnd extends libc/spec/sys.spec msgsnd
version		SYSVABI_1.2
end		

function	_msync extends libc/spec/gen.spec msync
version		SYSVABI_1.2
end		

function	_munlock extends libc/spec/gen.spec munlock
version		SYSVABI_1.2
end		

function	_munmap extends libc/spec/sys.spec munmap
version		SYSVABI_1.2
end		

function	_nice extends libc/spec/sys.spec nice
version		SYSVABI_1.2
end		

function	_numeric extends libc/spec/data.spec _numeric
version		SYSVABI_1.2
end		

function	_nuname extends libc/spec/missing.spec _nuname
arch		i386
version		SYSVABI_1.2
end		

function	_open extends libc/spec/sys.spec open
version		SYSVABI_1.2
end		

function	_opendir extends libc/spec/gen.spec opendir
version		SYSVABI_1.2
end		

function	_pathconf extends libc/spec/sys.spec pathconf
version		SYSVABI_1.2
end		

function	_pause extends libc/spec/sys.spec pause
version		SYSVABI_1.2
end		

function	_pipe extends libc/spec/sys.spec pipe
version		SYSVABI_1.2
end		

function	_poll extends libc/spec/sys.spec poll
version		SYSVABI_1.2
end		

function	_profil extends libc/spec/sys.spec profil
version		SYSVABI_1.2
end		

function	_ptrace extends libc/spec/gen.spec ptrace
version		SYSVABI_1.2
end		

function	_ptsname extends libc/spec/gen.spec ptsname
version		SYSVABI_1.2
end		

function	_putmsg extends libc/spec/gen.spec putmsg
version		SYSVABI_1.2
end		

function	_putpmsg extends libc/spec/gen.spec putpmsg
version		SYSVABI_1.2
end		

function	_read extends libc/spec/sys.spec read
version		SYSVABI_1.2
end		

function	_readdir extends libc/spec/gen.spec readdir
version		SYSVABI_1.2
end		

function	_readlink extends libc/spec/sys.spec readlink
version		SYSVABI_1.2
end		

function	_readv extends libc/spec/sys.spec readv
version		SYSVABI_1.2
end		

function	_rewinddir extends libc/spec/gen.spec rewinddir
version		SYSVABI_1.2
end		

function	_rmdir extends libc/spec/sys.spec rmdir
version		SYSVABI_1.2
end		

function	_sbrk extends libc/spec/sys.spec sbrk
arch		i386
version		SYSVABI_1.2
end		

function	_seekdir extends libc/spec/gen.spec seekdir
version		SYSVABI_1.2
end		

function	_semctl extends libc/spec/sys.spec semctl
version		SYSVABI_1.2
end		

function	_semget extends libc/spec/sys.spec semget
version		SYSVABI_1.2
end		

function	_semop extends libc/spec/sys.spec semop
version		SYSVABI_1.2
end		

function	_setcontext extends libc/spec/sys.spec setcontext
version		SYSVABI_1.2
end		

function	_setgid extends libc/spec/sys.spec setgid
version		SYSVABI_1.2
end		

function	_setgroups extends libc/spec/sys.spec setgroups
version		SYSVABI_1.2
end		

function	_setpgid extends libc/spec/gen.spec setpgid
version		SYSVABI_1.2
end		

function	_setpgrp extends libc/spec/sys.spec setpgrp
version		SYSVABI_1.2
end		

function	_setrlimit extends libc/spec/sys.spec setrlimit
version		SYSVABI_1.2
end		

function	_setsid extends libc/spec/sys.spec setsid
version		SYSVABI_1.2
end		

function	_setuid extends libc/spec/sys.spec setuid
version		SYSVABI_1.2
end		

function	_shmat extends libc/spec/sys.spec shmat
version		SYSVABI_1.2
end		

function	_shmctl extends libc/spec/sys.spec shmctl
version		SYSVABI_1.2
end		

function	_shmdt extends libc/spec/sys.spec shmdt
version		SYSVABI_1.2
end		

function	_shmget extends libc/spec/sys.spec shmget
version		SYSVABI_1.2
end		

function	_sigaddset extends libc/spec/gen.spec sigaddset
version		SYSVABI_1.2
end		

function	_sigaltstack extends libc/spec/sys.spec sigaltstack
version		SYSVABI_1.2
end		

function	_sigdelset extends libc/spec/gen.spec sigdelset
version		SYSVABI_1.2
end		

function	_sigemptyset extends libc/spec/gen.spec sigemptyset
version		SYSVABI_1.2
end		

function	_sigfillset extends libc/spec/gen.spec sigfillset
version		SYSVABI_1.2
end		

function	_sighold extends libc/spec/gen.spec sighold
version		SYSVABI_1.2
end		

function	_sigignore extends libc/spec/gen.spec sigignore
version		SYSVABI_1.2
end		

function	_sigismember extends libc/spec/gen.spec sigismember
version		SYSVABI_1.2
end		

function	_sigpause extends libc/spec/sys.spec sigpause
version		SYSVABI_1.2
end		

function	_sigpending extends libc/spec/sys.spec sigpending
version		SYSVABI_1.2
end		

function	_sigrelse extends libc/spec/gen.spec sigrelse
version		SYSVABI_1.2
end		

function	_sigsend extends libc/spec/gen.spec sigsend
version		SYSVABI_1.2
end		

function	_sigsendset extends libc/spec/gen.spec sigsendset
version		SYSVABI_1.2
end		

function	_sigset extends libc/spec/gen.spec sigset
version		SYSVABI_1.2
end		

function	_sigsetjmp extends libc/spec/gen.spec sigsetjmp
version		SYSVABI_1.2
end		

function	_sigsuspend extends libc/spec/sys.spec sigsuspend
version		SYSVABI_1.2
end		

function	_stat extends libc/spec/sys.spec stat
version		SYSVABI_1.2
end		

function	_statvfs extends libc/spec/sys.spec statvfs
version		SYSVABI_1.2
end		

function	_stime extends libc/spec/sys.spec stime
version		SYSVABI_1.2
end		

function	_swapcontext extends libc/spec/gen.spec swapcontext
version		SYSVABI_1.3
end		

function	_symlink extends libc/spec/sys.spec symlink
version		SYSVABI_1.2
end		

function	_sync extends libc/spec/sys.spec sync
version		SYSVABI_1.2
end		

function	_sysconf extends libc/spec/gen.spec sysconf
version		SYSVABI_1.2
end		

function	_telldir extends libc/spec/gen.spec telldir
version		SYSVABI_1.2
end		

function	_time extends libc/spec/sys.spec time
version		SYSVABI_1.2
end		

function	_times extends libc/spec/sys.spec times
version		SYSVABI_1.2
end		

function	_timezone extends libc/spec/sys.spec timezone
version		SYSVABI_1.2
end		

function	_ttyname extends libc/spec/gen.spec ttyname
version		SYSVABI_1.2
end		

function	_tzname extends libc/spec/sys.spec tzname
version		SYSVABI_1.2
end		

function	_ulimit extends libc/spec/sys.spec ulimit
version		SYSVABI_1.2
end		

function	_umask extends libc/spec/sys.spec umask
version		SYSVABI_1.2
end		

function	_umount extends libc/spec/sys.spec umount
version		SYSVABI_1.2
end		

function	_uname extends libc/spec/sys.spec uname
version		SYSVABI_1.2
end		

function	_unlink extends libc/spec/sys.spec unlink
version		SYSVABI_1.2
end		

function	_unlockpt extends libc/spec/gen.spec unlockpt
version		SYSVABI_1.2
end		

function	_utime extends libc/spec/sys.spec utime
version		SYSVABI_1.2
end		

function	_wait extends libc/spec/sys.spec wait
version		SYSVABI_1.2
end		

function	_waitid extends libc/spec/sys.spec waitid
version		SYSVABI_1.2
end		

function	_waitpid extends libc/spec/gen.spec waitpid
version		SYSVABI_1.2
end		

function	_write extends libc/spec/sys.spec write
version		SYSVABI_1.2
end		

function	_writev extends libc/spec/sys.spec writev
version		SYSVABI_1.2
end		

function	_xmknod extends libc/spec/missing.spec _xmknod
arch		i386
version		SYSVABI_1.2
end		

function	_xstat extends libc/spec/missing.spec _xstat
arch		i386
version		SYSVABI_1.2
end		

function	catclose extends libc/spec/gen.spec catclose
version		SYSVABI_1.2
end		

function	catgets extends libc/spec/gen.spec catgets
version		SYSVABI_1.2
end		

function	catopen extends libc/spec/gen.spec catopen
version		SYSVABI_1.2
end		

function	daylight extends libc/spec/sys.spec daylight
version		SYSVABI_1.2
end		

function	environ extends libc/spec/sys.spec environ
version		SYSVABI_1.2
end		

function	execl extends libc/spec/gen.spec execl
version		SYSVABI_1.2
end		

function	execle extends libc/spec/gen.spec execle
version		SYSVABI_1.2
end		

function	execlp extends libc/spec/gen.spec execlp
version		SYSVABI_1.2
end		

function	execv extends libc/spec/gen.spec execv
version		SYSVABI_1.2
end		

function	execve extends libc/spec/gen.spec execve
version		SYSVABI_1.2
end		

function	execvp extends libc/spec/gen.spec execvp
version		SYSVABI_1.2
end		

function	nuname extends libc/spec/missing.spec nuname
arch		i386
version		SYSVABI_1.2
end		

function	sbrk extends libc/spec/sys.spec sbrk
arch		i386
version		SYSVABI_1.2
end		

function	semctl extends libc/spec/sys.spec semctl
version		SYSVABI_1.2
end		

function	semget extends libc/spec/sys.spec semget
version		SYSVABI_1.2
end		

function	semop extends libc/spec/sys.spec semop
version		SYSVABI_1.2
end		

function	siglongjmp extends libc/spec/gen.spec siglongjmp
version		SYSVABI_1.2
end		

function	sigsetjmp extends libc/spec/gen.spec sigsetjmp
version		SYSVABI_1.2
end		

function	timezone extends libc/spec/sys.spec timezone
version		SYSVABI_1.2
end		

function	tzname extends libc/spec/sys.spec tzname
version		SYSVABI_1.2
end		

