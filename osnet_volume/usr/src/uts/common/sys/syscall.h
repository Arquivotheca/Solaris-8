/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SYSCALL_H
#define	_SYS_SYSCALL_H

#pragma ident	"@(#)syscall.h	1.72	99/08/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	system call numbers
 *		syscall(SYS_xxxx, ...)
 */

	/* syscall enumeration MUST begin with 1 */

	/*
	 * SunOS/SPARC uses 0 for the indirect system call SYS_syscall
	 * but this doesn't count because it is just another way
	 * to specify the real system call number.
	 */

#define	SYS_syscall	0
#define	SYS_exit	1
#define	SYS_fork	2
#define	SYS_read	3
#define	SYS_write	4
#define	SYS_open	5
#define	SYS_close	6
#define	SYS_wait	7
#define	SYS_creat	8
#define	SYS_link	9
#define	SYS_unlink	10
#define	SYS_exec	11
#define	SYS_chdir	12
#define	SYS_time	13
#define	SYS_mknod	14
#define	SYS_chmod	15
#define	SYS_chown	16
#define	SYS_brk		17
#define	SYS_stat	18
#define	SYS_lseek	19
#define	SYS_getpid	20
#define	SYS_mount	21
#define	SYS_umount	22
#define	SYS_setuid	23
#define	SYS_getuid	24
#define	SYS_stime	25
#define	SYS_pcsample	26
#define	SYS_alarm	27
#define	SYS_fstat	28
#define	SYS_pause	29
#define	SYS_utime	30
#define	SYS_stty	31
#define	SYS_gtty	32
#define	SYS_access	33
#define	SYS_nice	34
#define	SYS_statfs	35
#define	SYS_sync	36
#define	SYS_kill	37
#define	SYS_fstatfs	38
#define	SYS_pgrpsys	39
	/*
	 * subcodes:
	 *	getpgrp()	  :: syscall(39,0)
	 *	setpgrp()	  :: syscall(39,1)
	 *	getsid(pid)	  :: syscall(39,2,pid)
	 *	setsid()	  :: syscall(39,3)
	 *	getpgid(pid)	  :: syscall(39,4,pid)
	 *	setpgid(pid,pgid) :: syscall(39,5,pid,pgid)
	 */
#define	SYS_xenix	40
	/*
	 * subcodes:
	 *	syscall(40, code, ...)
	 */
#define	SYS_dup		41
#define	SYS_pipe	42
#define	SYS_times	43
#define	SYS_profil	44
#define	SYS_plock	45
#define	SYS_setgid	46
#define	SYS_getgid	47
#define	SYS_signal	48
	/*
	 * subcodes:
	 *	signal(sig, f) :: signal(sig, f)    ((sig&SIGNO_MASK) == sig)
	 *	sigset(sig, f) :: signal(sig|SIGDEFER, f)
	 *	sighold(sig)   :: signal(sig|SIGHOLD)
	 *	sigrelse(sig)  :: signal(sig|SIGRELSE)
	 *	sigignore(sig) :: signal(sig|SIGIGNORE)
	 *	sigpause(sig)  :: signal(sig|SIGPAUSE)
	 *	see <sys/signal.h>
	 */
#define	SYS_msgsys	49
	/*
	 * subcodes:
	 *	msgget(...) :: msgsys(0, ...)
	 *	msgctl(...) :: msgsys(1, ...)
	 *	msgrcv(...) :: msgsys(2, ...)
	 *	msgsnd(...) :: msgsys(3, ...)
	 *	see <sys/msg.h>
	 */
#define	SYS_syssun	50
#define	SYS_sysi86	50
	/*
	 * subcodes:
	 *	syssun(code, ...)
	 *	see <sys/sys3b.h>
	 */
#define	SYS_acct	51
#define	SYS_shmsys	52
	/*
	 * subcodes:
	 *	shmat (...) :: shmsys(0, ...)
	 *	shmctl(...) :: shmsys(1, ...)
	 *	shmdt (...) :: shmsys(2, ...)
	 *	shmget(...) :: shmsys(3, ...)
	 *	see <sys/shm.h>
	 */
#define	SYS_semsys	53
	/*
	 * subcodes:
	 *	semctl(...) :: semsys(0, ...)
	 *	semget(...) :: semsys(1, ...)
	 *	semop (...) :: semsys(2, ...)
	 *	see <sys/sem.h>
	 */
#define	SYS_ioctl	54
#define	SYS_uadmin	55
				/* 56 reserved for exch() */
#define	SYS_utssys	57
	/*
	 *subcodes (third argument):
	 *	uname(obuf)  (obsolete)   :: syscall(57, obuf, ign, 0)
	 *					subcode 1 unused
	 *	ustat(dev, obuf)	  :: syscall(57, obuf, dev, 2)
	 *	fusers(path, flags, obuf) :: syscall(57, path, flags, 3, obuf)
	 *	see <sys/utssys.h>
	 */
#define	SYS_fdsync	58
#define	SYS_execve	59
#define	SYS_umask	60
#define	SYS_chroot	61
#define	SYS_fcntl	62
#define	SYS_ulimit	63
				/* 64-69 reserved for UNIX PC */
#define	SYS_reserved_70	70	/* 70 reserved */
#define	SYS_reserved_71	71	/* 71 reserved */
#define	SYS_reserved_72	72	/* 72 reserved */
#define	SYS_reserved_73	73	/* 73 reserved */
#define	SYS_reserved_74	74	/* 74 reserved */
#define	SYS_reserved_75	75	/* 75 reserved */
#define	SYS_reserved_76	76	/* 76 reserved */
#define	SYS_reserved_77	77	/* 77 reserved */
#define	SYS_reserved_78	78	/* 78 reserved */
#define	SYS_rmdir	79
#define	SYS_mkdir	80
#define	SYS_getdents	81
				/* 82 not used, was libattach */
				/* 83 not used, was libdetach */
#define	SYS_sysfs	84
	/*
	 * subcodes:
	 *	sysfs(code, ...)
	 *	see <sys/fstyp.h>
	 */
#define	SYS_getmsg	85
#define	SYS_putmsg	86
#define	SYS_poll	87

#define	SYS_lstat	88
#define	SYS_symlink	89
#define	SYS_readlink	90
#define	SYS_setgroups	91
#define	SYS_getgroups	92
#define	SYS_fchmod	93
#define	SYS_fchown	94
#define	SYS_sigprocmask	95
#define	SYS_sigsuspend	96
#define	SYS_sigaltstack	97
#define	SYS_sigaction	98
#define	SYS_sigpending	99
	/*
	 * subcodes:
	 *			subcode 0 unused
	 *	sigpending(...) :: syscall(99, 1, ...)
	 *	sigfillset(...) :: syscall(99, 2, ...)
	 */
#define	SYS_context	100
	/*
	 * subcodes:
	 *	getcontext(...) :: syscall(100, 0, ...)
	 *	setcontext(...) :: syscall(100, 1, ...)
	 */
#define	SYS_evsys	101
#define	SYS_evtrapret	102
#define	SYS_statvfs	103
#define	SYS_fstatvfs	104
#define	SYS_getloadavg	105
#define	SYS_nfssys	106
#define	SYS_waitsys	107
#define	SYS_sigsendsys	108
#define	SYS_hrtsys	109
#define	SYS_acancel	110
#define	SYS_async	111
#define	SYS_priocntlsys	112
#define	SYS_pathconf	113
#define	SYS_mincore	114
#define	SYS_mmap	115
#define	SYS_mprotect	116
#define	SYS_munmap	117
#define	SYS_fpathconf	118
#define	SYS_vfork	119
#define	SYS_fchdir	120
#define	SYS_readv	121
#define	SYS_writev	122
#define	SYS_xstat	123
#define	SYS_lxstat	124
#define	SYS_fxstat	125
#define	SYS_xmknod	126
#define	SYS_clocal	127
#define	SYS_setrlimit	128
#define	SYS_getrlimit	129
#define	SYS_lchown	130
#define	SYS_memcntl	131
#define	SYS_getpmsg	132
#define	SYS_putpmsg	133
#define	SYS_rename	134
#define	SYS_uname	135
#define	SYS_setegid	136
#define	SYS_sysconfig	137
#define	SYS_adjtime	138
#define	SYS_systeminfo	139
#define	SYS_seteuid	141
#define	SYS_vtrace	142
#define	SYS_fork1	143
#define	SYS_sigtimedwait	144
#define	SYS_lwp_info	145
#define	SYS_yield	146
#define	SYS_lwp_sema_wait	147
#define	SYS_lwp_sema_post	148
#define	SYS_lwp_sema_trywait	149
#define	SYS_corectl	151
#define	SYS_modctl	152
#define	SYS_fchroot	153
#define	SYS_utimes	154
#define	SYS_vhangup	155
#define	SYS_gettimeofday	156
#define	SYS_getitimer		157
#define	SYS_setitimer		158
#define	SYS_lwp_create		159
#define	SYS_lwp_exit		160
#define	SYS_lwp_suspend		161
#define	SYS_lwp_continue	162
#define	SYS_lwp_kill		163
#define	SYS_lwp_self		164
#define	SYS_lwp_setprivate	165
#define	SYS_lwp_getprivate	166
#define	SYS_lwp_wait		167
#define	SYS_lwp_mutex_wakeup	168
#define	SYS_lwp_mutex_lock	169
#define	SYS_lwp_cond_wait	170
#define	SYS_lwp_cond_signal	171
#define	SYS_lwp_cond_broadcast	172
#define	SYS_pread		173
#define	SYS_pwrite		174
#define	SYS_llseek		175
#define	SYS_inst_sync		176
#define	SYS_srmlimitsys		177
#define	SYS_kaio		178
	/*
	 * subcodes:
	 *	aioread(...)	:: kaio(AIOREAD, ...)
	 *	aiowrite(...)	:: kaio(AIOWRITE, ...)
	 *	aiowait(...)	:: kaio(AIOWAIT, ...)
	 *	aiocancel(...)	:: kaio(AIOCANCEL, ...)
	 *	aionotify()	:: kaio(AIONOTIFY)
	 *	aioinit()	:: kaio(AIOINIT)
	 *	aiostart()	:: kaio(AIOSTART)
	 *	see <sys/aio.h>
	 */
#define	SYS_cpc			179
#define	SYS_tsolsys		184
#define	SYS_acl			185
#define	SYS_auditsys		186
#define	SYS_processor_bind	187
#define	SYS_processor_info	188
#define	SYS_p_online		189
#define	SYS_sigqueue		190
#define	SYS_clock_gettime	191
#define	SYS_clock_settime	192
#define	SYS_clock_getres	193
#define	SYS_timer_create	194
#define	SYS_timer_delete	195
#define	SYS_timer_settime	196
#define	SYS_timer_gettime	197
#define	SYS_timer_getoverrun	198
#define	SYS_nanosleep		199
#define	SYS_facl		200
#define	SYS_door		201
	/*
	 * Door Subcodes:
	 *	0	door_create
	 *	1	door_revoke
	 *	2	door_info
	 *	3	door_call
	 *	4	door_return
	 */
#define	SYS_setreuid		202
#define	SYS_setregid		203
#define	SYS_install_utrap	204
#define	SYS_signotify		205
#define	SYS_schedctl		206
#define	SYS_pset		207
#define	SYS_sparc_utrap_install	208
#define	SYS_resolvepath		209
#define	SYS_signotifywait	210
#define	SYS_lwp_sigredirect	211
#define	SYS_lwp_alarm		212
/* system calls for large file ( > 2 gigabyte) support */
#define	SYS_getdents64		213
#define	SYS_mmap64		214
#define	SYS_stat64		215
#define	SYS_lstat64		216
#define	SYS_fstat64		217
#define	SYS_statvfs64		218
#define	SYS_fstatvfs64		219
#define	SYS_setrlimit64		220
#define	SYS_getrlimit64		221
#define	SYS_pread64		222
#define	SYS_pwrite64		223
#define	SYS_creat64		224
#define	SYS_open64		225
#define	SYS_rpcsys		226
#define	SYS_so_socket		230
#define	SYS_so_socketpair	231
#define	SYS_bind		232
#define	SYS_listen		233
#define	SYS_accept		234
#define	SYS_connect		235
#define	SYS_shutdown		236
#define	SYS_recv		237
#define	SYS_recvfrom		238
#define	SYS_recvmsg		239
#define	SYS_send		240
#define	SYS_sendmsg		241
#define	SYS_sendto		242
#define	SYS_getpeername		243
#define	SYS_getsockname		244
#define	SYS_getsockopt		245
#define	SYS_setsockopt		246
#define	SYS_sockconfig		247
	/*
	 * NTP codes
	 */
#define	SYS_ntp_gettime		248
#define	SYS_ntp_adjtime		249
#define	SYS_lwp_mutex_unlock	250
#define	SYS_lwp_mutex_trylock	251
#define	SYS_lwp_mutex_init	252
#define	SYS_cladm		253
#define	SYS_lwp_sigtimedwait	254
#define	SYS_umount2		255

#ifndef	_ASM

typedef struct {		/* syscall set type */
	unsigned int	word[16];
} sysset_t;

#if defined(__STDC__)
extern int	syscall(int, ...);
#else
extern int	syscall();
#endif

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSCALL_H */
