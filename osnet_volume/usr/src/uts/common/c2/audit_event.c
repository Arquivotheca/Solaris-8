/*
 * @(#)audit_event.c 2.29 92/03/04 SMI; SunOS CMW
 * @(#)audit_event.c 4.2.1.2 91/05/08 SMI; SunOS BSM
 *
 * This file contains the audit event table used to control the production
 * of audit records for each system call.
 */

/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audit_event.c	1.95	99/12/06 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/systeminfo.h>	/* for sysinfo auditing */
#include <sys/utsname.h>	/* for sysinfo auditing */
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/modctl.h>		/* for modctl auditing */
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/acl.h>
#include <sys/ipc.h>
#include <sys/door.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/file.h>		/* for accept */
#include <sys/pathname.h>	/* for symlink */
#include <sys/uio.h>		/* for symlink */
#include <sys/utssys.h>		/* for fuser */
#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_kevents.h>
#include <c2/audit_record.h>
#include <sys/procset.h>
#include <nfs/mount.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <sys/ddi.h>

extern token_t	*au_to_sock_inet(struct sockaddr_in *);

extern kmutex_t  au_stat_lock;
static au_event_t	aui_null(au_event_t);
static au_event_t	aui_open(au_event_t);
static au_event_t	aui_msgsys(au_event_t);
static au_event_t	aui_shmsys(au_event_t);
static au_event_t	aui_semsys(au_event_t);
static au_event_t	aui_utssys(au_event_t);
static au_event_t	aui_fcntl(au_event_t);
static au_event_t	aui_execv(au_event_t);
static au_event_t	aui_execve(au_event_t);
static au_event_t	aui_memcntl(au_event_t);
static au_event_t	aui_auditsys(au_event_t);
static au_event_t	aui_modctl(au_event_t);
static au_event_t	aui_acl(au_event_t);
static au_event_t	aui_doorfs(au_event_t);

static void	aus_null(struct t_audit_data *);
static void	aus_acl(struct t_audit_data *);
static void	aus_acct(struct t_audit_data *);
static void	aus_chown(struct t_audit_data *);
static void	aus_fchown(struct t_audit_data *);
static void	aus_lchown(struct t_audit_data *);
static void	aus_chmod(struct t_audit_data *);
static void	aus_facl(struct t_audit_data *);
static void	aus_fchmod(struct t_audit_data *);
static void	aus_fcntl(struct t_audit_data *);
static void	aus_mkdir(struct t_audit_data *);
static void	aus_mknod(struct t_audit_data *);
static void	aus_mount(struct t_audit_data *);
static void	aus_msgsys(struct t_audit_data *);
static void	aus_semsys(struct t_audit_data *);
static void	aus_close(struct t_audit_data *);
static void	aus_fstatfs(struct t_audit_data *);
static void	aus_setgid(struct t_audit_data *);
static void	aus_setuid(struct t_audit_data *);
static void	aus_shmsys(struct t_audit_data *);
static void	aus_doorfs(struct t_audit_data *);
static void	aus_symlink(struct t_audit_data *);
static void	aus_ioctl(struct t_audit_data *);
static void	aus_memcntl(struct t_audit_data *);
static void	aus_mmap(struct t_audit_data *);
static void	aus_munmap(struct t_audit_data *);
static void	aus_priocntlsys(struct t_audit_data *);
static void	aus_setegid(struct t_audit_data *);
static void	aus_setgroups(struct t_audit_data *);
static void	aus_seteuid(struct t_audit_data *);
static void	aus_putmsg(struct t_audit_data *);
static void	aus_putpmsg(struct t_audit_data *);
static void	aus_getmsg(struct t_audit_data *);
static void	aus_getpmsg(struct t_audit_data *);
static void	aus_auditsys(struct t_audit_data *);
static void	aus_sysinfo(struct t_audit_data *);
static void	aus_modctl(struct t_audit_data *);
static void	aus_kill(struct t_audit_data *);
#if 0
static void	aus_xmknod(struct t_audit_data *);
#endif
static void	aus_setregid(struct t_audit_data *);
static void	aus_setreuid(struct t_audit_data *);

static void	auf_null(struct t_audit_data *, int, rval_t *);
static void	auf_mknod(struct t_audit_data *, int, rval_t *);
static void	auf_msgsys(struct t_audit_data *, int, rval_t *);
static void	auf_semsys(struct t_audit_data *, int, rval_t *);
static void	auf_shmsys(struct t_audit_data *, int, rval_t *);
static void	auf_symlink(struct t_audit_data *, int, rval_t *);
#if 0
static void	auf_open(struct t_audit_data *, int, rval_t *);
static void	auf_xmknod(struct t_audit_data *, int, rval_t *);
#endif
static void	auf_read(struct t_audit_data *, int, rval_t *);
static void	auf_write(struct t_audit_data *, int, rval_t *);

static void	aus_sigqueue(struct t_audit_data *);
static void	aus_p_online(struct t_audit_data *);
static void	aus_processor_bind(struct t_audit_data *);
static void	aus_inst_sync(struct t_audit_data *);

static void	auf_accept(struct t_audit_data *, int, rval_t *);

static void	auf_bind(struct t_audit_data *, int, rval_t *);
static void	auf_connect(struct t_audit_data *, int, rval_t *);
static void	aus_shutdown(struct t_audit_data *);
static void	auf_setsockopt(struct t_audit_data *, int, rval_t *);
static void	aus_sockconfig(struct t_audit_data *);
static void	auf_recv(struct t_audit_data *, int, rval_t *);
static void	auf_recvmsg(struct t_audit_data *, int, rval_t *);
static void	auf_send(struct t_audit_data *, int, rval_t *);
static void	auf_sendmsg(struct t_audit_data *, int, rval_t *);
static void	auf_recvfrom(struct t_audit_data *, int, rval_t *);
static void	auf_sendto(struct t_audit_data *, int, rval_t *);
static void	aus_socket(struct t_audit_data *);
/*
 * This table contains mapping information for converting system call numbers
 * to audit event IDs. In several cases it is necessary to map a single system
 * call to several events.
 */

struct audit_s2e audit_s2e[] =
{
/*
----------	---------- 	----------	----------
INITIAL		AUDIT		START		SYSTEM
PROCESSING	EVENT		PROCESSING	CALL
----------	----------	----------	-----------
	FINISH		EVENT
	PROCESSING	CONTROL
----------------------------------------------------------
*/
aui_null,	AUE_NULL,	aus_null,	/* 0 unused (indirect) */
		auf_null,	0,
aui_null,	AUE_EXIT,	aus_null,	/* 1 exit */
		auf_null,	S2E_NPT,
aui_null,	AUE_FORK,	aus_null,	/* 2 fork */
		auf_null,	0,
aui_null,	AUE_READ,	aus_null,	/* 3 read */
		auf_read,	0,
aui_null,	AUE_WRITE,	aus_null,	/* 4 write */
		auf_write,	0,
aui_open,	AUE_OPEN,	aus_null,	/* 5 open */
		auf_null,	S2E_SP,
aui_null,	AUE_CLOSE,	aus_close,	/* 6 close */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 7 wait */
		auf_null,	0,
aui_null,	AUE_CREAT,	aus_null,	/* 8 create */
		auf_null,	S2E_SP,
aui_null,	AUE_LINK,	aus_null,	/* 9 link */
		auf_null,	0,
aui_null,	AUE_UNLINK,	aus_null,	/* 10 unlink */
		auf_null,	0,
aui_execv,	AUE_EXEC,	aus_null,	/* 11 exec */
		auf_null,	S2E_MLD,
aui_null,	AUE_CHDIR,	aus_null,	/* 12 chdir */
		auf_null,	S2E_SP,
aui_null,	AUE_NULL,	aus_null,	/* 13 time */
		auf_null,	0,
aui_null,	AUE_MKNOD,	aus_mknod,	/* 14 mknod */
		auf_mknod,	0,
aui_null,	AUE_CHMOD,	aus_chmod,	/* 15 chmod */
		auf_null,	0,
aui_null,	AUE_CHOWN,	aus_chown,	/* 16 chown */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 17 brk */
		auf_null,	0,
aui_null,	AUE_STAT,	aus_null,	/* 18 stat */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 19 lseek */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 20 getpid */
		auf_null,	0,
aui_null,	AUE_MOUNT,	aus_mount,	/* 21 mount */
		auf_null,	0,
aui_null,	AUE_UMOUNT,	aus_null,	/* 22 umount */
		auf_null,	0,
aui_null,	AUE_SETUID,	aus_setuid,	/* 23 setuid */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 24 getuid */
		auf_null,	0,
aui_null,	AUE_STIME,	aus_null,	/* 25 stime */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 26 (loadable) was ptrace */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 27 alarm */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 28 fstat */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 29 pause */
		auf_null,	0,
aui_null,	AUE_UTIME,	aus_null,	/* 30 utime */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 31 stty (TIOCSETP-audit?) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 32 gtty */
		auf_null,	0,
aui_null,	AUE_ACCESS,	aus_null,	/* 33 access */
		auf_null,	0,
aui_null,	AUE_NICE,	aus_null,	/* 34 nice */
		auf_null,	0,
aui_null,	AUE_STATFS,	aus_null,	/* 35 statfs */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 36 sync */
		auf_null,	0,
aui_null,	AUE_KILL,	aus_kill,	/* 37 kill */
		auf_null,	0,
aui_null,	AUE_FSTATFS,	aus_fstatfs,	/* 38 fstatfs */
		auf_null,	0,
aui_null,	AUE_SETPGRP,	aus_null,	/* 39 setpgrp */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 40 (loadable) was cxenix */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 41 dup */
		auf_null,	0,
aui_null,	AUE_PIPE,	aus_null,	/* 42 pipe */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 43 times */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 44 profil */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 45 (loadable) */
						/*	was proc lock */
		auf_null,	0,
aui_null,	AUE_SETGID,	aus_setgid,	/* 46 setgid */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 47 getgid */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 48 sig */
		auf_null,	0,
aui_msgsys,	AUE_MSGSYS,	aus_msgsys,	/* 49 (loadable) was msgsys */
		auf_msgsys,	0,
#ifdef i386
aui_null,	AUE_NULL,	aus_null,	/* 50 sysi86 */
		auf_null,	0,
#else
aui_null,	AUE_NULL,	aus_null,	/* 50 (loadable) was sys3b */
		auf_null,	0,
#endif
aui_null,	AUE_ACCT,	aus_acct,	/* 51 acct */
		auf_null,	0,
aui_shmsys,	AUE_SHMSYS,	aus_shmsys,	/* 52 shared memory */
		auf_shmsys,	0,
aui_semsys,	AUE_SEMSYS,	aus_semsys,	/* 53 IPC semaphores */
		auf_semsys,	0,
aui_null,	AUE_IOCTL,	aus_ioctl,	/* 54 ioctl */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 55 uadmin */
		auf_null,	0,
#ifdef MEGA
aui_null,	AUE_NULL,	aus_null,	/* 56 uexch */
		auf_null,	0,
#else
aui_null,	AUE_NULL,	aus_null,	/* 56 (loadable) uexch */
		auf_null,	0,
#endif
aui_utssys,	AUE_FUSERS,	aus_null,	/* 57 utssys */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 58 fsync */
		auf_null,	0,
aui_execve,	AUE_EXECVE,	aus_null,	/* 59 exece */
		auf_null,	S2E_MLD,
aui_null,	AUE_NULL,	aus_null,	/* 60 umask */
		auf_null,	0,
aui_null,	AUE_CHROOT,	aus_null,	/* 61 chroot */
		auf_null,	S2E_SP,
aui_fcntl,	AUE_FCNTL,	aus_fcntl,	/* 62 fcntl */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 63 ulimit */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 64 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 65 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 66 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 67 (loadable) */
						/*	file locking call */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 68 (loadable) */
						/*	local system calls */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 69 (loadable) inode open */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 70 (loadable) was advfs */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 71 (loadable) was unadvfs */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 72 (loadable) was notused */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 73 (loadable) was notused */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 74 (loadable) was rfstart */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 75 (loadable) */
						/*	was sigret (SunOS) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 76 (loadable) was rdebug */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 77 (loadable) was rfstop */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 78 (loadable) was rfssys */
		auf_null,	0,
aui_null,	AUE_RMDIR,	aus_null,	/* 79 rmdir */
		auf_null,	0,
aui_null,	AUE_MKDIR,	aus_mkdir,	/* 80 mkdir */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 81 getdents */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 82 (loadable) */
						/*	was libattach */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 83 (loadable) */
						/*	was libdetach */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 84 sysfs */
		auf_null,	0,
aui_null,	AUE_GETMSG,	aus_getmsg,	/* 85 getmsg */
		auf_null,	0,
aui_null,	AUE_PUTMSG,	aus_putmsg,	/* 86 putmsg */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 87 poll */
		auf_null,	0,
aui_null,	AUE_LSTAT,	aus_null,	/* 88 lstat */
		auf_null,	0,
aui_null,	AUE_SYMLINK,	aus_symlink,	/* 89 symlink */
		auf_symlink,	0,
aui_null,	AUE_READLINK,	aus_null,	/* 90 readlink */
		auf_null,	0,
aui_null,	AUE_SETGROUPS,	aus_setgroups,	/* 91 setgroups */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 92 getgroups */
		auf_null,	0,
aui_null,	AUE_FCHMOD,	aus_fchmod,	/* 93 fchmod */
		auf_null,	0,
aui_null,	AUE_FCHOWN,	aus_fchown,	/* 94 fchown */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 95 sigprocmask */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 96 sigsuspend */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 97 sigaltstack */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 98 sigaction */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 99 sigpending */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 100 setcontext */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 101 (loadable) was evsys */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 102 (loadable) */
						/*	was evtrapret */
		auf_null,	0,
aui_null,	AUE_STATVFS,	aus_null,	/* 103 statvfs */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 104 fstatvfs */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 105 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 106 nfssys */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 107 waitset */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 108 sigsendset */
		auf_null,	0,
#if defined(i386)
aui_null,	AUE_NULL,	aus_null,	/* 109 hrtsys */
		auf_null,	0,
#else
aui_null,	AUE_NULL,	aus_null,	/* 109 (loadable) */
		auf_null,	0,
#endif /* defined(i386) */
aui_null,	AUE_NULL,	aus_null,	/* 110 (loadable) was acancel */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 111 (loadable) was async */
		auf_null,	0,
aui_null,	AUE_PRIOCNTLSYS,	aus_priocntlsys,
		auf_null,	0,		/* 112 priocntlsys */
aui_null,	AUE_PATHCONF,	aus_null,	/* 113 pathconf */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 114 mincore */
		auf_null,	0,
aui_null,	AUE_MMAP,	aus_mmap,	/* 115 mmap */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 116 mprotect */
		auf_null,	0,
aui_null,	AUE_MUNMAP,	aus_munmap,	/* 117 munmap */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 118 fpathconf */
		auf_null,	0,
aui_null,	AUE_VFORK,	aus_null,	/* 119 vfork */
		auf_null,	0,
aui_null,	AUE_FCHDIR,	aus_null,	/* 120 fchdir */
		auf_null,	0,
aui_null,	AUE_READ,	aus_null,	/* 121 readv */
		auf_read,	0,
aui_null,	AUE_WRITE,	aus_null,	/* 122 writev */
		auf_write,	0,
aui_null,	AUE_XSTAT,	aus_null,	/* 123 xstat (expanded stat) */
		auf_null,	0,
aui_null,	AUE_LXSTAT,	aus_null,	/* 124 lxstat */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 125 fxstat */
		auf_null,	0,
aui_null,	AUE_XMKNOD,	aus_null,	/* 126 xmknod */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 127 (loadable) was clocal */
		auf_null,	0,
aui_null,	AUE_SETRLIMIT,	aus_null,	/* 128 setrlimit */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 129 getrlimit */
		auf_null,	0,
aui_null,	AUE_LCHOWN,	aus_lchown,	/* 130 lchown */
		auf_null,	0,
aui_memcntl,	AUE_MEMCNTL,	aus_memcntl,	/* 131 memcntl */
		auf_null,	0,
aui_null,	AUE_GETPMSG,	aus_getpmsg,	/* 132 getpmsg */
		auf_null,	0,
aui_null,	AUE_PUTPMSG,	aus_putpmsg,	/* 133 putpmsg */
		auf_null,	0,
aui_null,	AUE_RENAME,	aus_null,	/* 134 rename */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 135 uname */
		auf_null,	0,
aui_null,	AUE_SETEGID,	aus_setegid,	/* 136 setegid */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 137 sysconfig */
		auf_null,	0,
aui_null,	AUE_ADJTIME,	aus_null,	/* 138 adjtime */
		auf_null,	0,
aui_null,	AUE_SYSINFO,	aus_sysinfo,	/* 139 systeminfo */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 140 reserved */
		auf_null,	0,
aui_null,	AUE_SETEUID,	aus_seteuid,	/* 141 seteuid */
		auf_null,	0,
#ifdef TRACE
aui_null,	AUE_VTRACE,	aus_null,	/* 142 vtrace */
		auf_null,	0,
#else  TRACE
aui_null,	AUE_NULL,	aus_null,	/* 142 (loadable) vtrace */
		auf_null,	0,
#endif TRACE
aui_null,	AUE_FORK1,	aus_null,	/* 143 fork1 */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 144 sigwait */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 145 lwp_info */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 146 yield */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 147 lwp_sema_wait */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 148 lwp_sema_post */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 149 lwp_sema_trywait */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 150 (loadable reserved) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 151 (loadable reserved) */
		auf_null,	0,
aui_modctl,	AUE_MODCTL,	aus_modctl,	/* 152 modctl */
		auf_null,	0,
aui_null,	AUE_FCHROOT,	aus_null,	/* 153 fchroot */
		auf_null,	0,
aui_null,	AUE_UTIMES,	aus_null,	/* 154 utimes */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 155 vhangup */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 156 gettimeofday */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 157 getitimer */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 158 setitimer */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 159 lwp_create */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 160 lwp_exit */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 161 lwp_stop */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 162 lwp_continue */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 163 lwp_kill */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 164 lwp_get_id */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 165 lwp_setprivate */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 166 lwp_getprivate */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 167 lwp_wait */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 168 lwp_mutex_unlock  */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 169 lwp_mutex_lock */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 170 lwp_cond_wait */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 171 lwp_cond_signal */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 172 lwp_cond_broadcast */
		auf_null,	0,
aui_null,	AUE_READ,	aus_null,	/* 173 pread */
		auf_read,	0,
aui_null,	AUE_WRITE,	aus_null,	/* 174 pwrite */
		auf_write,	0,
aui_null,	AUE_NULL,	aus_null,	/* 175 llseek */
		auf_null,	0,
aui_null,	AUE_INST_SYNC,	aus_inst_sync,  /* 176 (loadable) */
						/* aus_inst_sync */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 177 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 178 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 179 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 180 (loadable) kaio */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 181 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 182 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 183 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 184 (loadable) tsolsys */
		auf_null,	0,
aui_acl,	AUE_ACLSET,	aus_acl,	/* 185 acl */
		auf_null,	0,
aui_auditsys,	AUE_AUDITSYS,	aus_auditsys,	/* 186 auditsys  */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_processor_bind,
		auf_null,	0,		/* 187 processor_bind */
aui_null,	AUE_NULL,	aus_null,	/* 188 processor_info */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_p_online,	/* 189 p_online */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_sigqueue,	/* 190 sigqueue */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 191 clock_gettime */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 192 clock_settime */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 193 clock_getres */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 194 timer_create */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 195 timer_delete */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 196 timer_settime */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 197 timer_gettime */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 198 timer_getoverrun */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 199 nanosleep */
		auf_null,	0,
aui_acl,	AUE_FACLSET,	aus_facl,	/* 200 facl */
		auf_null,	0,
aui_doorfs,	AUE_DOORFS,	aus_doorfs,	/* 201 (loadable) doorfs */
		auf_null,	0,
aui_null,	AUE_SETREUID,	aus_setreuid,	/* 202 setreuid */
		auf_null,	0,
aui_null,	AUE_SETREGID,	aus_setregid,	/* 203 setregid */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 204 install_utrap */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 205 signotify */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 206 schedctl */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 207 (loadable) pset */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 208 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 209 resolvepath */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 210 signotifywait */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 211 lwp_sigredirect */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 212 lwp_alarm */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 213 getdents64 (__ppc) */
		auf_null,	0,
aui_null,	AUE_MMAP,	aus_mmap,	/* 214 mmap64 */
		auf_null,	0,
aui_null,	AUE_STAT,	aus_null,	/* 215 stat64 */
		auf_null,	0,
aui_null,	AUE_LSTAT,	aus_null,	/* 216 lstat64 */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 217 fstat64 */
		auf_null,	0,
aui_null,	AUE_STATVFS,	aus_null,	/* 218 statvfs64 */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 219 fstatvfs64 */
		auf_null,	0,
aui_null,	AUE_SETRLIMIT,	aus_null,	/* 220 setrlimit64 */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 221 getrlimit64 */
		auf_null,	0,
aui_null,	AUE_READ,	aus_null,	/* 222 pread64  */
		auf_read,	0,
aui_null,	AUE_WRITE,	aus_null,	/* 223 pwrite64 */
		auf_write,	0,
aui_null,	AUE_CREAT,	aus_null,	/* 224 creat64 */
		auf_null,	S2E_SP,
aui_open,	AUE_OPEN,	aus_null,	/* 225 open64 */
		auf_null,	S2E_SP,
aui_null,	AUE_NULL,	aus_null,	/* 226 (loadable) rpcsys */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 227 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 228 (loadable) */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 229 (loadable) */
		auf_null,	0,
aui_null,	AUE_SOCKET,	aus_socket,	/* 230 so_socket */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 231 so_socketpair */
		auf_null,	0,
aui_null,	AUE_BIND,	aus_null,	/* 232 bind */
		auf_bind,	0,
aui_null,	AUE_NULL,	aus_null,	/* 233 listen */
		auf_null,	0,
aui_null,	AUE_ACCEPT,	aus_null,	/* 234 accept */
		auf_accept,	0,
aui_null,	AUE_CONNECT,	aus_null,	/* 235 connect */
		auf_connect,	0,
aui_null,	AUE_SHUTDOWN,	aus_shutdown,	/* 236 shutdown */
		auf_null,	0,
aui_null,	AUE_READ,	aus_null,	/* 237 recv */
		auf_recv,	0,
aui_null,	AUE_RECVFROM,	aus_null,	/* 238 recvfrom */
		auf_recvfrom,	0,
aui_null,	AUE_RECVMSG,	aus_null,	/* 239 recvmsg */
		auf_recvmsg,	0,
aui_null,	AUE_WRITE,	aus_null,	/* 240 send */
		auf_send,	0,
aui_null,	AUE_SENDMSG,	aus_null,	/* 241 sendmsg */
		auf_sendmsg,	0,
aui_null,	AUE_SENDTO,	aus_null,	/* 242 sendto */
		auf_sendto,	0,
aui_null,	AUE_NULL,	aus_null,	/* 243 getpeername */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 244 getsockname */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 245 getsockopt */
		auf_null,	0,
aui_null,	AUE_SETSOCKOPT,	aus_null,	/* 246 setsockopt */
		auf_setsockopt,	0,
aui_null,	AUE_SOCKCONFIG,	aus_sockconfig,	/* 247 sockconfig */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 248 ntp_gettime */
		auf_null,	0,
aui_null,	AUE_NULL,	aus_null,	/* 249 ntp_adjtime */
		auf_null,	0,
};

uint_t num_syscall = sizeof (audit_s2e) / sizeof (struct audit_s2e);

char *so_not_bound = "socket not bound";
char *so_not_conn  = "socket not connected";
char *so_bad_addr  = "bad socket address";
char *so_bad_peer_addr  = "bad peer address";

/* null start function */
/*ARGSUSED*/
static void
aus_null(struct t_audit_data *tad)
{
}

/* acct start function */
/*ARGSUSED*/
static void
aus_acct(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uintptr_t fname;

	struct a {
		long	fname;		/* char * */
	} *uap = (struct a *)clwp->lwp_ap;

	fname = (uintptr_t)uap->fname;

	if (fname == 0)
		au_uwrite(au_to_arg32(1, "accounting off", (uint32_t)0));
}

/* chown start function */
/*ARGSUSED*/
static void
aus_chown(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t uid, gid;

	struct a {
		long	fname;		/* char * */
		long	uid;
		long	gid;
	} *uap = (struct a *)clwp->lwp_ap;

	uid = (uint32_t)uap->uid;
	gid = (uint32_t)uap->gid;

	au_uwrite(au_to_arg32(2, "new file uid", uid));
	au_uwrite(au_to_arg32(3, "new file gid", gid));
}

/* fchown start function */
/*ARGSUSED*/
static void
aus_fchown(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t uid, gid, fd;
	struct file  *fp;
	struct vnode *vp;
	struct f_audit_data *fad;

	struct a {
		long fd;
		long uid;
		long gid;
	} *uap = (struct a *)clwp->lwp_ap;

	fd  = (uint32_t)uap->fd;
	uid = (uint32_t)uap->uid;
	gid = (uint32_t)uap->gid;

	au_uwrite(au_to_arg32(2, "new file uid", uid));
	au_uwrite(au_to_arg32(3, "new file gid", gid));

		/*
		 * convert file pointer to file descriptor
		 *   Note: fd ref count incremented here.
		 */
	if ((fp = getf(fd)) == NULL)
		return;

		/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", fd));
	}

	vp = fp->f_vnode;
	audit_attributes(vp);

	/* decrement file descriptor reference count */
	releasef(fd);
}

/*ARGSUSED*/
static void
aus_lchown(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t uid, gid;


	struct a {
		long	fname;		/* char	* */
		long	uid;
		long	gid;
	} *uap = (struct a *)clwp->lwp_ap;

	uid = (uint32_t)uap->uid;
	gid = (uint32_t)uap->gid;

	au_uwrite(au_to_arg32(2, "new file uid", uid));
	au_uwrite(au_to_arg32(3, "new file gid", gid));
}

/* chmod start function */
/*ARGSUSED*/
static void
aus_chmod(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fmode;

	struct a {
		long	fname;		/* char	* */
		long	fmode;
	} *uap = (struct a *)clwp->lwp_ap;

	fmode = (uint32_t)uap->fmode;

	au_uwrite(au_to_arg32(2, "new file mode", fmode&07777));
}

/* chmod start function */
/*ARGSUSED*/
static void
aus_fchmod(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fmode, fd;
	struct file  *fp;
	struct vnode *vp;
	struct f_audit_data *fad;

	struct a {
		long	fd;
		long	fmode;
	} *uap = (struct a *)clwp->lwp_ap;

	fd = (uint32_t)uap->fd;
	fmode = (uint32_t)uap->fmode;

	au_uwrite(au_to_arg32(2, "new file mode", fmode&07777));

		/*
		 * convert file pointer to file descriptor
		 *   Note: fd ref count incremented here.
		 */
	if ((fp = getf(fd)) == NULL)
		return;

		/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", fd));
	}

	vp = fp->f_vnode;
	audit_attributes(vp);

	/* decrement file descriptor reference count */
	releasef(fd);
}


/* null function */
/*ARGSUSED*/
static void
auf_null(struct t_audit_data *tad, int error, rval_t *rval)
{
}

/* null function */
static au_event_t
aui_null(au_event_t e)
{
	return (e);
}

/* convert open to appropriate event */
static au_event_t
aui_open(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint_t fm;

	struct a {
		long	fnamep;		/* char	* */
		long	fmode;
		long	cmode;
	} *uap = (struct a *)clwp->lwp_ap;

	fm = (uint_t)uap->fmode;

	if (fm & O_WRONLY)
		e = AUE_OPEN_W;
	else if (fm & O_RDWR)
		e = AUE_OPEN_RW;
	else
		e = AUE_OPEN_R;

	if (fm & O_CREAT)
		e += 1;
	if (fm & O_TRUNC)
		e += 2;

	return (e);
}

/* msgsys */
static au_event_t
aui_msgsys(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint_t fm;

	struct a {
		long	id;	/* function code id */
		long	ap;	/* arg pointer for recvmsg */
	} *uap = (struct a *)clwp->lwp_ap;

	struct b {
		long	msgid;
		long	cmd;
		long	buf;	/* struct msqid_ds * */
	} *uap1 = (struct b *)&clwp->lwp_ap[1];

	fm  = (uint_t)uap->id;

	switch (fm) {
	case 0:		/* msgget */
		e = AUE_MSGGET;
		break;
	case 1:		/* msgctl */
		switch ((uint_t)uap1->cmd) {
		case IPC_RMID:
		case IPC_O_RMID:
			e = AUE_MSGCTL_RMID;
			break;
		case IPC_SET:
		case IPC_O_SET:
			e = AUE_MSGCTL_SET;
			break;
		case IPC_STAT:
		case IPC_O_STAT:
			e = AUE_MSGCTL_STAT;
			break;
		default:
			e = AUE_MSGCTL;
			break;
		}
		break;
	case 2:		/* msgrcv */
		e = AUE_MSGRCV;
		break;
	case 3:		/* msgsnd */
		e = AUE_MSGSND;
		break;
	default:	/* illegal system call */
		e = AUE_NULL;
		break;
	}

	return (e);
}


/* shmsys */
static au_event_t
aui_shmsys(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	int fm;

	struct a {		/* shmsys */
		long	id;	/* function code id */
	} *uap = (struct a *)clwp->lwp_ap;

	struct b {		/* ctrl */
		long	shmid;
		long	cmd;
		long	arg;		/* struct shmid_ds * */
	} *uap1 = (struct b *)&clwp->lwp_ap[1];
	fm  = (uint_t)uap->id;

	switch (fm) {
	case 0:		/* shmat */
		e = AUE_SHMAT;
		break;
	case 1:		/* shmctl */
		switch ((uint_t)uap1->cmd) {
		case IPC_RMID:
		case IPC_O_RMID:
			e = AUE_SHMCTL_RMID;
			break;
		case IPC_SET:
		case IPC_O_SET:
			e = AUE_SHMCTL_SET;
			break;
		case IPC_STAT:
		case IPC_O_STAT:
			e = AUE_SHMCTL_STAT;
			break;
		default:
			e = AUE_SHMCTL;
			break;
		}
		break;
	case 2:		/* shmdt */
		e = AUE_SHMDT;
		break;
	case 3:		/* shmget */
		e = AUE_SHMGET;
		break;
	default:	/* illegal system call */
		e = AUE_NULL;
		break;
	}

	return (e);
}


/* semsys */
static au_event_t
aui_semsys(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint_t fm;

	struct a {		/* semsys */
		long	id;
	} *uap = (struct a *)clwp->lwp_ap;

	struct b {		/* ctrl */
		long	semid;
		long	semnum;
		long	cmd;
		long	arg;
	} *uap1 = (struct b *)&clwp->lwp_ap[1];

	fm = (uint_t)uap->id;

	switch (fm) {
	case 0:		/* semctl */
		switch ((uint_t)uap1->cmd) {
		case IPC_RMID:
		case IPC_O_RMID:
			e = AUE_SEMCTL_RMID;
			break;
		case IPC_SET:
		case IPC_O_SET:
			e = AUE_SEMCTL_SET;
			break;
		case IPC_STAT:
		case IPC_O_STAT:
			e = AUE_SEMCTL_STAT;
			break;
		case GETNCNT:
			e = AUE_SEMCTL_GETNCNT;
			break;
		case GETPID:
			e = AUE_SEMCTL_GETPID;
			break;
		case GETVAL:
			e = AUE_SEMCTL_GETVAL;
			break;
		case GETALL:
			e = AUE_SEMCTL_GETALL;
			break;
		case GETZCNT:
			e = AUE_SEMCTL_GETZCNT;
			break;
		case SETVAL:
			e = AUE_SEMCTL_SETVAL;
			break;
		case SETALL:
			e = AUE_SEMCTL_SETALL;
			break;
		default:
			e = AUE_SEMCTL;
			break;
		}
		break;
	case 1:		/* semget */
		e = AUE_SEMGET;
		break;
	case 2:		/* semop */
		e = AUE_SEMOP;
		break;
	default:	/* illegal system call */
		e = AUE_NULL;
		break;
	}

	return (e);
}

/* utssys - uname(2), ustat(2), fusers(2) */
static au_event_t
aui_utssys(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint_t type;

	struct a {
		union {
			long	cbuf;		/* char * */
			long	ubuf;		/* struct stat * */
		} ub;
		union {
			long	mv;	/* for USTAT */
			long	flags;	/* for FUSERS */
		} un;
		long	type;
		long	outbp;		/* char * for FUSERS */
	} *uap = (struct a *)clwp->lwp_ap;

	type = (uint_t)uap->type;

	if (type == UTS_FUSERS)
		return (e);
	else
		return ((au_event_t)AUE_NULL);
}

static au_event_t
aui_fcntl(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint_t cmd;

	struct a {
		long	fdes;
		long	cmd;
		long	arg;
	} *uap = (struct a *)clwp->lwp_ap;

	cmd = (uint_t)uap->cmd;

	switch (cmd) {
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		break;
	case F_SETFL:
	case F_GETFL:
	case F_GETFD:
		break;
	default:
		e = (au_event_t)AUE_NULL;
		break;
	}
	return ((au_event_t)e);
}

/* null function for now */
static au_event_t
aui_execv(au_event_t e)
{
	return (e);
}

/* null function for now */
static au_event_t
aui_execve(au_event_t e)
{
	return (e);
}

/*ARGSUSED*/
static void
aus_fcntl(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t cmd, fd;
	struct file  *fp;
	struct vnode *vp;
	struct f_audit_data *fad;

	struct a {
		long	fd;
		long	cmd;
		long	arg;
	} *uap = (struct a *)clwp->lwp_ap;

	cmd = (uint32_t)uap->cmd;
	fd  = (uint32_t)uap->fd;

	au_uwrite(au_to_arg32(2, "cmd", cmd));

		/*
		 * convert file pointer to file descriptor
		 *   Note: fd ref count incremented here.
		 */
	if ((fp = getf(fd)) == NULL)
		return;

	/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", fd));
	}

	vp = fp->f_vnode;
	audit_attributes(vp);

	/* decrement file descriptor reference count */
	releasef(fd);
}

/*ARGSUSED*/
static void
aus_kill(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	struct proc *p;
	uint32_t signo;
	struct p_audit_data *pad;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	struct a {
		long	pid;
		long	signo;
	} *uap = (struct a *)clwp->lwp_ap;

	pid   = (pid_t)uap->pid;
	signo = (uint32_t)uap->signo;

	au_uwrite(au_to_arg32(2, "signal", signo));
	if (pid > 0) {
		mutex_enter(&pidlock);
		if ((p = prfind(pid)) == (struct proc *)0) {
			mutex_exit(&pidlock);
			return;
		}
		mutex_enter(&p->p_lock); /* so process doesn't go away */
		mutex_exit(&pidlock);
		uid  = p->p_cred->cr_uid;
		gid  = p->p_cred->cr_gid;
		ruid = p->p_cred->cr_ruid;
		rgid = p->p_cred->cr_rgid;
		pad  = (struct p_audit_data *)P2A(p);
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		mutex_exit(&p->p_lock);
		au_uwrite(au_to_process(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));
	}
	else
		au_uwrite(au_to_arg32(1, "process", (uint32_t)pid));
}

/*ARGSUSED*/
static void
aus_mkdir(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t dmode;

	struct a {
		long	dirnamep;		/* char * */
		long	dmode;
	} *uap = (struct a *)clwp->lwp_ap;

	dmode = (uint32_t)uap->dmode;

	au_uwrite(au_to_arg32(2, "mode", dmode));
}

/*ARGSUSED*/
static void
aus_mknod(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fmode;
	dev_t dev;

	struct a {
		long	pnamep;		/* char * */
		long	fmode;
		long	dev;
	} *uap = (struct a *)clwp->lwp_ap;

	fmode = (uint32_t)uap->fmode;
	dev   = (dev_t)uap->dev;

	au_uwrite(au_to_arg32(2, "mode", fmode));
#ifdef _LP64
	au_uwrite(au_to_arg64(3, "dev", dev));
#else
	au_uwrite(au_to_arg32(3, "dev", dev));
#endif
}

/*ARGSUSED*/
static void
auf_mknod(struct t_audit_data *tad, int error, rval_t *rval)
{
	klwp_id_t clwp = ttolwp(curthread);
	char  *path;
	size_t len;
	uint_t size;
	caddr_t pnamep;
	char *src;
	char *ptr, *base;
	struct p_audit_data *pad = (struct p_audit_data *)P2A(curproc);

	struct a {
		long	pnamep;		/* char * */
		long	fmode;
		long	dev;
	} *uap = (struct a *)clwp->lwp_ap;

	pnamep = (caddr_t)uap->pnamep;

		/* no error, then already path token in audit record */
	if (error != EPERM)
		return;
		/* not auditing this event, nothing then to do */
	if (tad->tad_flag == 0)
		return;
	path = (char *)kmem_alloc(MAXPATHLEN, KM_SLEEP);
		/* get path string */
	if (copyinstr(pnamep, path, MAXPATHLEN, &len))
		goto mknod_free_path;
		/* if length 0, then do nothing */
	if (len == 0)
		goto mknod_free_path;

	/* absolute or relative paths? */
	mutex_enter(&pad->pad_lock);
	if (path[0] == '/') {
		size  = pad->pad_cwrd->cwrd_rootlen;
		src   = pad->pad_cwrd->cwrd_root;
	} else {
		size  = pad->pad_cwrd->cwrd_dirlen;
		src   = pad->pad_cwrd->cwrd_dir;
	}
		/* space for two strings (first null becomes a '/') */
	AS_INC(as_memused, (uint_t)(size + len));
	base = ptr = (char *)kmem_alloc((size_t)(size + len), KM_SLEEP);
	bcopy(src, base, size - 1);
	mutex_exit(&pad->pad_lock);
	ptr += size - 1;
	*ptr++ = '/';
	bcopy(path, ptr, len);
	au_uwrite(au_to_path(base, size + len));
	AS_DEC(as_memused, (uint_t)(size + len));
	kmem_free(base, size + len);
mknod_free_path:
	kmem_free(path, MAXPATHLEN);
}

#if 0
/*ARGSUSED*/
static void
aus_xmknod(struct t_audit_data *tad)
{
	uint32_t fmode;
	dev_t dev;

	struct a {
		int	version;
		char    *fname;
		mode_t	fmode;
		dev_t	dev;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	fmode = (uint32_t)uap->fmode;
	dev = (dev_t)uap->dev;

	au_uwrite(au_to_arg32(2, "mode", fmode));
#ifdef _LP64
	au_uwrite(au_to_arg64(3, "dev", dev));
#else
	au_uwrite(au_to_arg32(3, "dev", dev));
#endif
}
#endif

#if 0
/*ARGSUSED*/
static void
auf_xmknod(t_audit_data_t *tad, int error, rval_t *rval)
{
	struct a {
		int	version;
		char    *fname;
		mode_t   fmode;
		dev_t    dev;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	char  path[MAXPATHLEN];
	uint_t len;
	uint_t size;
	char *src;
	char *ptr, *base;
	struct p_audit_data *pad = (p_audit_data_t *)P2A(curproc);

		/* no error, then already path token in audit record */
	if (error != EPERM)
		return;
		/* not auditing this event, nothing then to do */
	if (tad->tad_flag == 0)
		return;
		/* get path string */
	if (copyinstr((caddr_t)uap->fname, path, MAXPATHLEN, &len))
		return;
		/* if length 0, then do nothing */
	if (len == 0)
		return;

		/* absolute or relative paths? */
	mutex_enter(&pad->pad_lock);
	if (path[0] == '/') {
		size  = pad->pad_cwrd->cwrd_rootlen;
		src   = pad->pad_cwrd->cwrd_root;
	} else {
		size  = pad->pad_cwrd->cwrd_dirlen;
		src   = pad->pad_cwrd->cwrd_dir;
	}
		/* space for two strings (first null becomes a '/') */
	AS_INC(as_memused, size+len);
	base = ptr = (char *)kmem_alloc((uint_t)(size+len), KM_SLEEP);
	bcopy(src, base, size-1);
	mutex_exit(&pad->pad_lock);
	ptr += size-1;
	*ptr++ = '/';
	bcopy(path, ptr, len);
	au_uwrite(au_to_path(base, size+len));
	AS_DEC(as_memused, size+len);
	kmem_free(base, size+len);
}
#endif

/*ARGSUSED*/
static void
aus_mount(struct t_audit_data *tad)
{	/* AUS_START */
	/* XXX64 */

	klwp_id_t clwp = ttolwp(curthread);
	uint32_t flags;
	uintptr_t u_fstype, dataptr;
	STRUCT_DECL(nfs_args, nfsargs);
	size_t len;
	char *fstype, *hostname;

	struct a {
		long	spec;		/* char    * */
		long	dir;		/* char    * */
		long	flags;
		long	fstype;		/* char    * */
		long	dataptr;	/* char    * */
		long	datalen;
	} *uap = (struct a *)clwp->lwp_ap;

	u_fstype = (uintptr_t)uap->fstype;
	flags    = (uint32_t)uap->flags;
	dataptr  = (uintptr_t)uap->dataptr;

	fstype = (char *)kmem_zalloc(MAXNAMELEN, KM_SLEEP);
	if (copyinstr((caddr_t)u_fstype, (caddr_t)fstype, MAXNAMELEN, &len))
		goto mount_free_fstype;

	au_uwrite(au_to_arg32(3, "flags", flags));
	au_uwrite(au_to_text(fstype));

	if (strncmp(fstype, "nfs", 4) == 0) {

		STRUCT_INIT(nfsargs, get_udatamodel());
		bzero(STRUCT_BUF(nfsargs), STRUCT_SIZE(nfsargs));

		if (copyin((caddr_t)dataptr,
				STRUCT_BUF(nfsargs),
				MIN(uap->datalen, STRUCT_SIZE(nfsargs)))) {
			debug_enter((char *)NULL);
			goto mount_free_fstype;
		}
		hostname = (char *)kmem_zalloc(MAXNAMELEN, KM_SLEEP);
		if (copyinstr(STRUCT_FGETP(nfsargs, hostname),
				(caddr_t)hostname,
				MAXNAMELEN, &len)) {
			goto mount_free_hostname;
		}
		au_uwrite(au_to_text(hostname));
		au_uwrite(au_to_arg32(3, "internal flags",
			(uint_t)STRUCT_FGET(nfsargs, flags)));

mount_free_hostname:
		kmem_free(hostname, MAXNAMELEN);
	}

mount_free_fstype:
	kmem_free(fstype, MAXNAMELEN);
}	/* AUS_MOUNT */


static void
aus_msgsys(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t msgid;

	struct b {
		long	msgid;
		long	cmd;
		long	buf;		/* struct msqid_ds * */
	} *uap1 = (struct b *)&clwp->lwp_ap[1];

	msgid = (uint32_t)uap1->msgid;


	switch (tad->tad_event) {
	case AUE_MSGGET:		/* msgget */
		au_uwrite(au_to_arg32(1, "msg key", msgid));
		break;
	case AUE_MSGCTL:		/* msgctl */
	case AUE_MSGCTL_RMID:		/* msgctl */
	case AUE_MSGCTL_STAT:		/* msgctl */
	case AUE_MSGRCV:		/* msgrcv */
	case AUE_MSGSND:		/* msgsnd */
		au_uwrite(au_to_arg32(1, "msg ID", msgid));
		break;
	case AUE_MSGCTL_SET:		/* msgctl */
		au_uwrite(au_to_arg32(1, "msg ID", msgid));
		break;
	}
}

/*ARGSUSED*/
static void
auf_msgsys(struct t_audit_data *tad, int error, rval_t *rval)
{
	int id;

	if (error != 0)
		return;
	if (tad->tad_event == AUE_MSGGET) {
		id = (int)rval->r_val1;
		au_uwrite(au_to_ipc(AT_IPC_MSG, id));
	}
}

static void
aus_semsys(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t semid;

	struct b {		/* ctrl */
		long	semid;
		long	semnum;
		long	cmd;
		long	arg;
	} *uap1 = (struct b *)&clwp->lwp_ap[1];

	semid = (uint32_t)uap1->semid;

	switch (tad->tad_event) {
	case AUE_SEMCTL_RMID:
	case AUE_SEMCTL_STAT:
	case AUE_SEMCTL_GETNCNT:
	case AUE_SEMCTL_GETPID:
	case AUE_SEMCTL_GETVAL:
	case AUE_SEMCTL_GETALL:
	case AUE_SEMCTL_GETZCNT:
	case AUE_SEMCTL_SETVAL:
	case AUE_SEMCTL_SETALL:
	case AUE_SEMCTL:
	case AUE_SEMOP:
		au_uwrite(au_to_arg32(1, "sem ID", semid));
		break;
	case AUE_SEMCTL_SET:
		au_uwrite(au_to_arg32(1, "sem ID", semid));
		break;
	case AUE_SEMGET:
		au_uwrite(au_to_arg32(1, "sem key", semid));
		break;
	}
}

/*ARGSUSED*/
static void
auf_semsys(struct t_audit_data *tad, int error, rval_t *rval)
{
	int id;

	if (error != 0)
		return;
	if (tad->tad_event == AUE_SEMGET) {
		id = (int)rval->r_val1;
		au_uwrite(au_to_ipc(AT_IPC_SEM, id));
	}
}

/*ARGSUSED*/
static void
aus_close(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fd;
	struct file *fp;
	struct f_audit_data *fad;
	struct vnode *vp;
	struct vattr attr;

	struct a {
		long	i;
	} *uap = (struct a *)clwp->lwp_ap;

	fd = (uint32_t)uap->i;

	attr.va_mask = 0;
	au_uwrite(au_to_arg32(1, "fd", fd));

		/*
		 * convert file pointer to file descriptor
		 *   Note: fd ref count incremented here.
		 */
	if ((fp = getf(fd)) == NULL)
		return;

	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
			/* decrement file descriptor reference count */
		releasef(fd);
		if ((vp = fp->f_vnode) != NULL) {
			attr.va_mask = AT_ALL;
			if (VOP_GETATTR(vp, &attr, 0, CRED()) == 0) {
				au_uwrite(au_to_attr(&attr));
			}
		}
	} else {
			/* decrement file descriptor reference count */
		releasef(fd);
	}
}

#if 0
/*ARGSUSED*/
static void
auf_open(struct t_audit_data *tad, int error, rval_t *rval)
{
	/* system will panic in kmem_alloc if tad info is not cleared */
	tad->tad_pathlen = 0;
	tad->tad_path	= NULL;
	tad->tad_vn	= NULL;

}
#endif

/*ARGSUSED*/
static void
aus_fstatfs(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fd;
	struct file  *fp;
	struct vnode *vp;
	struct f_audit_data *fad;

	struct a {
		long	fd;
		long	buf;		/* struct statfs * */
	} *uap = (struct a *)clwp->lwp_ap;

	fd = (uint_t)uap->fd;

		/*
		 * convert file pointer to file descriptor
		 *   Note: fd ref count incremented here.
		 */
	if ((fp = getf(fd)) == NULL)
		return;

		/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", fd));
	}

	vp = fp->f_vnode;
	audit_attributes(vp);

	/* decrement file descriptor reference count */
	releasef(fd);
}

#ifdef NOTYET
/*ARGSUSED*/
static void
aus_setpgrp(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t pgrp;
	struct proc *p;
	struct p_audit_data *pad;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	struct a {
		long	pid;
		long	pgrp;
	} *uap = (struct a *)clwp->lwp_ap;

	pid  = (pid_t)uap->pid;
	pgrp = (uint32_t)uap->pgrp;

		/* current process? */
	if (pid == 0)
		(return);

	mutex_enter(&pidlock);
	p = prfind(pid);
	mutex_enter(&p->p_lock);	/* so process doesn't go away */
	mutex_exit(&pidlock);
	if (p == NULL || p->p_as == &kas) {
		mutex_exit(&p->p_lock);
		return;
	}
	uid  = p->p_cred->cr_uid;
	gid  = p->p_cred->cr_gid;
	ruid = p->p_cred->cr_ruid;
	rgid = p->p_cred->cr_rgid;
	pad  = (struct p_audit_data *)P2A(p);
	auid = pad->pad_auid;
	asid = pad->pad_asid;
	atid = pad->pad_termid;
	mutex_exit(&p->p_lock);
	au_uwrite(au_to_process(uid, gid, ruid, rgid, pid,
				auid, asid, (au_termid_t *)&atid));
	au_uwrite(au_to_arg32(2, "pgrp", pgrp));
}
#endif

/*ARGSUSED*/
static void
aus_setregid(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t rgid, egid;

	struct a {
		long	 rgid;
		long	 egid;
	} *uap = (struct a *)clwp->lwp_ap;

	rgid  = (uint32_t)uap->rgid;
	egid  = (uint32_t)uap->egid;

	au_uwrite(au_to_arg32(1, "rgid", rgid));
	au_uwrite(au_to_arg32(2, "egid", egid));
}

/*ARGSUSED*/
static void
aus_setgid(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t gid;

	struct a {
		long	gid;
	} *uap = (struct a *)clwp->lwp_ap;

	gid = (uint32_t)uap->gid;

	au_uwrite(au_to_arg32(1, "gid", gid));
}


/*ARGSUSED*/
static void
aus_setreuid(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t ruid, euid;

	struct a {
		long	ruid;
		long	euid;
	} *uap = (struct a *)clwp->lwp_ap;

	ruid = (uint32_t)uap->ruid;
	euid  = (uint32_t)uap->euid;

	au_uwrite(au_to_arg32(1, "ruid", ruid));
	au_uwrite(au_to_arg32(2, "euid", euid));
}


/*ARGSUSED*/
static void
aus_setuid(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t uid;

	struct a {
		long	uid;
	} *uap = (struct a *)clwp->lwp_ap;

	uid = (uint32_t)uap->uid;

	au_uwrite(au_to_arg32(1, "uid", uid));
}

/*ARGSUSED*/
static void
aus_shmsys(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t id, cmd;

	struct b {
		long	id;
		long	cmd;
		long	buf;		/* struct shmid_ds * */
	} *uap1 = (struct b *)&clwp->lwp_ap[1];

	id  = (uint32_t)uap1->id;
	cmd = (uint32_t)uap1->cmd;

	switch (tad->tad_event) {
	case AUE_SHMGET:			/* shmget */
		au_uwrite(au_to_arg32(1, "shm key", id));
		break;
	case AUE_SHMCTL:			/* shmctl */
	case AUE_SHMCTL_RMID:			/* shmctl */
	case AUE_SHMCTL_STAT:			/* shmctl */
	case AUE_SHMCTL_SET:			/* shmctl */
		au_uwrite(au_to_arg32(1, "shm ID", id));
		break;
	case AUE_SHMDT:				/* shmdt */
		au_uwrite(au_to_arg32(1, "shm adr", id));
		break;
	case AUE_SHMAT:				/* shmat */
		au_uwrite(au_to_arg32(1, "shm ID", id));
		au_uwrite(au_to_arg32(2, "shm adr", cmd));
		break;
	}
}

/*ARGSUSED*/
static void
auf_shmsys(struct t_audit_data *tad, int error, rval_t *rval)
{
	int id;

	if (error != 0)
		return;
	if (tad->tad_event == AUE_SHMGET) {
		id = (int)rval->r_val1;
		au_uwrite(au_to_ipc(AT_IPC_SHM, id));
	}
}

/*ARGSUSED*/
static void
aus_symlink(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	struct pathname tpn;
	caddr_t target;

	struct a {
		long	target;		/* char    * */
		long	linkname;	/* char    * */
	} *uap = (struct a *)clwp->lwp_ap;

	target = (caddr_t)uap->target;

	if (pn_get(target, UIO_USERSPACE, &tpn))
		return;
	au_uwrite(au_to_text(tpn.pn_path));
	pn_free(&tpn);
	tad->tad_ctrl |= PAD_SAVPATH;
}

/*
 * auf symlink inherits the symlink path from the initial lookuppn
 * it now must redo the lookup to get the new vnode assosiated with
 * the symlink and record the attributes
 */

/*ARGSUSED*/
static void
auf_symlink(struct t_audit_data *tad, int error, rval_t *rval)
{
	vnode_t	*vp = NULL;
	struct pathname path;
	char *link_path;
	uint_t link_len;
	int   ret;

	link_len = (uint_t)tad->tad_pathlen;
	tad->tad_ctrl |= PAD_NOPATH;	/* audit_finish uses tad_ctrl */
	if ((!error) && (link_len)) {
		link_path = kmem_zalloc(link_len, KM_SLEEP);
		bcopy(tad->tad_path, link_path, link_len);
		path.pn_buf = link_path;
		path.pn_path = link_path;
		path.pn_pathlen = (uint_t)link_len - 1;	/* don't include \0 */
		path.pn_bufsize = (uint_t)link_len;
		ret = lookuppn(&path, NULL, NO_FOLLOW, NULLVPP, &vp);
		if ((ret == 0) && (vp != NULL)) {
			audit_attributes(vp);
			VN_RELE(vp);
		}
		kmem_free(link_path, link_len);
	}

	if (link_len) {
		dprintf(2, ("auf_symlink: %x %p\n",
			tad->tad_pathlen, (void *)tad->tad_path));
		call_debug(2);
		AS_DEC(as_memused, link_len);
		kmem_free(tad->tad_path, link_len);
		tad->tad_pathlen = 0;
		tad->tad_path = NULL;
		tad->tad_vn = NULL;
	}
}


/*ARGSUSED*/
static void
aus_ioctl(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	struct file *fp;
	struct vnode *vp;
	struct f_audit_data *fad;
	uint32_t fd, cmd;
	uintptr_t cmarg;

	/* XXX64 */
	struct a {
		long	fd;
		long	cmd;
		long	cmarg;		/* caddr_t */
	} *uap = (struct a *)clwp->lwp_ap;

	fd    = (uint32_t)uap->fd;
	cmd   = (uint32_t)uap->cmd;
	cmarg = (uintptr_t)uap->cmarg;

		/*
		 * convert file pointer to file descriptor
		 *   Note: fd ref count incremented here.
		 */
	if ((fp = getf(fd)) == NULL) {
		au_uwrite(au_to_arg32(1, "fd", fd));
		au_uwrite(au_to_arg32(2, "cmd", cmd));
#ifndef _LP64
			au_uwrite(au_to_arg32(3, "arg", (uint32_t)cmarg));
#else
			au_uwrite(au_to_arg64(3, "arg", (uint64_t)cmarg));
#endif
		return;
	}

	/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", fd));
	}

	vp = fp->f_vnode;
	audit_attributes(vp);

	/* decrement file descriptor reference count */
	releasef(fd);

	au_uwrite(au_to_arg32(2, "cmd", cmd));
#ifndef _LP64
		au_uwrite(au_to_arg32(3, "arg", (uint32_t)cmarg));
#else
		au_uwrite(au_to_arg64(3, "arg", (uint64_t)cmarg));
#endif
}

/*
 * null function for memcntl for now. We might want to limit memcntl()
 * auditing to commands: MC_LOCKAS, MC_LOCK, MC_UNLOCKAS, MC_UNLOCK which
 * require superuser privileges.
 */
static au_event_t
aui_memcntl(au_event_t e)
{
	return (e);
}

/*ARGSUSED*/
static void
aus_memcntl(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);

	struct a {
		long	addr;
		long	len;
		long	cmd;
		long	arg;
		long	attr;
		long	mask;
	} *uap = (struct a *)clwp->lwp_ap;

#ifdef _LP64
	au_uwrite(au_to_arg64(1, "base", (uint64_t)uap->addr));
	au_uwrite(au_to_arg64(2, "len", (uint64_t)uap->len));
#else
	au_uwrite(au_to_arg32(1, "base", (uint32_t)uap->addr));
	au_uwrite(au_to_arg32(2, "len", (uint32_t)uap->len));
#endif
	au_uwrite(au_to_arg32(3, "cmd", (uint_t)uap->cmd));
#ifdef _LP64
	au_uwrite(au_to_arg64(4, "arg", (uint64_t)uap->arg));
#else
	au_uwrite(au_to_arg32(4, "arg", (uint32_t)uap->arg));
#endif
	au_uwrite(au_to_arg32(5, "attr", (uint_t)uap->attr));
	au_uwrite(au_to_arg32(6, "mask", (uint_t)uap->mask));
}

/*ARGSUSED*/
static void
aus_mmap(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	struct file *fp;
	struct f_audit_data *fad;
	struct vnode *vp;
	uint32_t fd;

	struct a {
		long	addr;
		long	len;
		long	prot;
		long	flags;
		long	fd;
		long	pos;
	} *uap = (struct a *)clwp->lwp_ap;

	fd = (uint32_t)uap->fd;

		/*
		 * convert file pointer to file descriptor
		 *   Note: fd ref count incremented here.
		 */
	if ((fp = getf(fd)) == NULL)
		return;

	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", fd));
	}

	vp = (struct vnode *)fp->f_vnode;
	audit_attributes(vp);

	/* decrement file descriptor reference count */
	releasef(fd);

}	/* AUS_MMAP */




/*ARGSUSED*/
static void
aus_munmap(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);

	struct a {
		long	addr;
		long	len;
	} *uap = (struct a *)clwp->lwp_ap;

#ifdef _LP64
	au_uwrite(au_to_arg64(1, "addr", (uint64_t)uap->addr));
	au_uwrite(au_to_arg64(2, "len", (uint64_t)uap->len));
#else
	au_uwrite(au_to_arg32(1, "addr", (uint32_t)uap->addr));
	au_uwrite(au_to_arg32(2, "len", (uint32_t)uap->len));
#endif

}	/* AUS_MUNMAP */







/*ARGSUSED*/
static void
aus_priocntlsys(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);

	struct a {
		long	pc_version;
		long	psp;		/* procset_t */
		long	cmd;
		long	arg;
	} *uap = (struct a *)clwp->lwp_ap;

	au_uwrite(au_to_arg32(1, "pc_version", (uint32_t)uap->pc_version));
	au_uwrite(au_to_arg32(3, "cmd", (uint32_t)uap->cmd));

}	/* AUS_PRIOCNTLSYS */


/*ARGSUSED*/
static void
aus_setegid(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t gid;

	struct a {
		long	gid;
	} *uap = (struct a *)clwp->lwp_ap;

	gid = (uint32_t)uap->gid;

	au_uwrite(au_to_arg32(1, "gid", gid));
}	/* AUS_SETEGID */




/*ARGSUSED*/
static void
aus_setgroups(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	int i;
	int gidsetsize;
	uintptr_t gidset;
	gid_t *gidlist;

	struct a {
		long	gidsetsize;
		long	gidset;
	} *uap = (struct a *)clwp->lwp_ap;

	gidsetsize = (uint_t)uap->gidsetsize;
	gidset = (uintptr_t)uap->gidset;

	if ((gidsetsize > NGROUPS_MAX_DEFAULT) || (gidsetsize < 0))
		return;
	if (gidsetsize != 0) {
		gidlist = (gid_t *)kmem_alloc(gidsetsize * sizeof (gid_t),
						KM_SLEEP);
		if (copyin((caddr_t)gidset, gidlist,
			gidsetsize * sizeof (gid_t)) == 0)
			for (i = 0; i < gidsetsize; i++)
				au_uwrite(au_to_arg32(1, "setgroups",
						(uint32_t)gidlist[i]));
		kmem_free(gidlist, gidsetsize * sizeof (gid_t));
	} else
		au_uwrite(au_to_arg32(1, "setgroups", (uint32_t)0));

}	/* AUS_SETGROUPS */





/*ARGSUSED*/
static void
aus_seteuid(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t uid;

	struct a {
		long	uid;
	} *uap = (struct a *)clwp->lwp_ap;

	uid = (uint32_t)uap->uid;

	au_uwrite(au_to_arg32(1, "euid", uid));

}	/* AUS_SETEUID */




/*ARGSUSED*/
static void
aus_putmsg(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fdes, pri;

	struct a {
		long	fdes;
		long	ctl;		/* struct strbuf * */
		long	data;		/* struct strbuf * */
		long	pri;
	} *uap = (struct a *)clwp->lwp_ap;

	fdes = (uint32_t)uap->fdes;
	pri  = (uint32_t)uap->pri;

	au_uwrite(au_to_arg32(1, "fd", fdes));
	au_uwrite(au_to_arg32(4, "pri", pri));

}	/* AUS_PUTMSG */





/*ARGSUSED*/
static void
aus_putpmsg(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fdes, pri, flags;

	struct a {
		long	fdes;
		long	ctl;		/* struct strbuf * */
		long	data;		/* struct strbuf * */
		long	pri;
		long	flags;
	} *uap = (struct a *)clwp->lwp_ap;

	fdes = (uint32_t)uap->fdes;
	pri  = (uint32_t)uap->pri;
	flags  = (uint32_t)uap->flags;

	au_uwrite(au_to_arg32(1, "fd", fdes));
	au_uwrite(au_to_arg32(4, "pri", pri));
	au_uwrite(au_to_arg32(5, "flags", flags));

}	/* AUS_PUTPMSG */






/*ARGSUSED*/
static void
aus_getmsg(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fdes, pri;

	struct a {
		long	fdes;
		long	ctl;		/* struct strbuf * */
		long	data;		/* struct strbuf * */
		long	pri;
	} *uap = (struct a *)clwp->lwp_ap;

	fdes = (uint32_t)uap->fdes;
	pri  = (uint32_t)uap->pri;

	au_uwrite(au_to_arg32(1, "fd", fdes));
	au_uwrite(au_to_arg32(4, "pri", pri));

}	/* AUS_GETMSG */





/*ARGSUSED*/
static void
aus_getpmsg(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t fdes;

	struct a {
		long	fdes;
		long	ctl;		/* struct strbuf * */
		long	data;		/* struct strbuf * */
		long	pri;
		long	flags;
	} *uap = (struct a *)clwp->lwp_ap;

	fdes = (uint32_t)uap->fdes;

	au_uwrite(au_to_arg32(1, "fd", fdes));

}	/* AUS_GETPMSG */







static au_event_t
aui_auditsys(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t code;

	struct a {
		long	code;
		long	a1;
		long	a2;
		long	a3;
		long	a4;
		long	a5;
		long	a6;
		long	a7;
	} *uap = (struct a *)clwp->lwp_ap;

	code = (uint32_t)uap->code;

	switch (code) {

	case BSM_GETAUID:
		e = AUE_GETAUID;
		break;
	case BSM_SETAUID:
		e = AUE_SETAUID;
		break;
	case BSM_GETAUDIT:
		e = AUE_GETAUDIT;
		break;
	case BSM_GETAUDIT_ADDR:
		e = AUE_GETAUDIT_ADDR;
		break;
	case BSM_SETAUDIT:
		e = AUE_SETAUDIT;
		break;
	case BSM_SETAUDIT_ADDR:
		e = AUE_SETAUDIT_ADDR;
		break;
	case BSM_AUDIT:
		e = AUE_AUDIT;
		break;
	case BSM_AUDITSVC:
		e = AUE_AUDITSVC;
		break;
	case BSM_GETPORTAUDIT:
		e = AUE_GETPORTAUDIT;
		break;
	case BSM_AUDITON:
	case BSM_AUDITCTL:

		switch ((uint_t)uap->a1) {

		case A_GETPOLICY:
			e = AUE_AUDITON_GPOLICY;
			break;
		case A_SETPOLICY:
			e = AUE_AUDITON_SPOLICY;
			break;
		case A_GETKMASK:
			e = AUE_AUDITON_GETKMASK;
			break;
		case A_SETKMASK:
			e = AUE_AUDITON_SETKMASK;
			break;
		case A_GETQCTRL:
			e = AUE_AUDITON_GQCTRL;
			break;
		case A_SETQCTRL:
			e = AUE_AUDITON_SQCTRL;
			break;
		case A_GETCWD:
			e = AUE_AUDITON_GETCWD;
			break;
		case A_GETCAR:
			e = AUE_AUDITON_GETCAR;
			break;
		case A_GETSTAT:
			e = AUE_AUDITON_GETSTAT;
			break;
		case A_SETSTAT:
			e = AUE_AUDITON_SETSTAT;
			break;
		case A_SETUMASK:
			e = AUE_AUDITON_SETUMASK;
			break;
		case A_SETSMASK:
			e = AUE_AUDITON_SETSMASK;
			break;
		case A_GETCOND:
			e = AUE_AUDITON_GETCOND;
			break;
		case A_SETCOND:
			e = AUE_AUDITON_SETCOND;
			break;
		case A_GETCLASS:
			e = AUE_AUDITON_GETCLASS;
			break;
		case A_SETCLASS:
			e = AUE_AUDITON_SETCLASS;
			break;
		default:
			e = AUE_NULL;
			break;
		}
		break;
	default:
		e = AUE_NULL;
		break;
	}

	return (e);




}	/* AUI_AUDITSYS */







static void
aus_auditsys(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uintptr_t a1, a2;
	struct file *fp;
	struct f_audit_data *fad;
	struct vnode *vp;
	STRUCT_DECL(auditinfo, ainfo);
	STRUCT_DECL(auditinfo_addr, ainfo_addr);
	au_evclass_map_t event;
	au_mask_t mask;
	int auditstate, policy;


	struct a {
		long	code;
		long	a1;
		long	a2;
		long	a3;
		long	a4;
		long	a5;
		long	a6;
		long	a7;
	} *uap = (struct a *)clwp->lwp_ap;

	a1   = (uintptr_t)uap->a1;
	a2   = (uintptr_t)uap->a2;

	switch (tad->tad_event) {
	case AUE_SETAUID:
		au_uwrite(au_to_arg32(2, "setauid", (uint32_t)a1));
		break;
	case AUE_SETAUDIT:
		STRUCT_INIT(ainfo, get_udatamodel());
		if (copyin((caddr_t)a1, STRUCT_BUF(ainfo),
				STRUCT_SIZE(ainfo))) {
				return;
		}
		au_uwrite(au_to_arg32((char)1, "setaudit:auid",
			(uint32_t)STRUCT_FGET(ainfo, ai_auid)));
#ifdef _LP64
		au_uwrite(au_to_arg64((char)1, "setaudit:port",
			(uint64_t)STRUCT_FGET(ainfo, ai_termid.port)));
#else
		au_uwrite(au_to_arg32((char)1, "setaudit:port",
			(uint32_t)STRUCT_FGET(ainfo, ai_termid.port)));
#endif
		au_uwrite(au_to_arg32((char)1, "setaudit:machine",
			(uint32_t)STRUCT_FGET(ainfo, ai_termid.machine)));
		au_uwrite(au_to_arg32((char)1, "setaudit:as_success",
			(uint32_t)STRUCT_FGET(ainfo, ai_mask.as_success)));
		au_uwrite(au_to_arg32((char)1, "setaudit:as_failure",
			(uint32_t)STRUCT_FGET(ainfo, ai_mask.as_failure)));
		au_uwrite(au_to_arg32((char)1, "setaudit:asid",
			(uint32_t)STRUCT_FGET(ainfo, ai_asid)));
		break;
	case AUE_SETAUDIT_ADDR:
		STRUCT_INIT(ainfo_addr, get_udatamodel());
		if (copyin((caddr_t)a1, STRUCT_BUF(ainfo_addr),
				STRUCT_SIZE(ainfo_addr))) {
				return;
		}
		au_uwrite(au_to_arg32((char)1, "auid",
			(uint32_t)STRUCT_FGET(ainfo_addr, ai_auid)));
#ifdef _LP64
		au_uwrite(au_to_arg64((char)1, "port",
			(uint64_t)STRUCT_FGET(ainfo_addr, ai_termid.at_port)));
#else
		au_uwrite(au_to_arg32((char)1, "port",
			(uint32_t)STRUCT_FGET(ainfo_addr, ai_termid.at_port)));
#endif
		au_uwrite(au_to_arg32((char)1, "type",
			(uint32_t)STRUCT_FGET(ainfo_addr, ai_termid.at_type)));
		if ((uint32_t)STRUCT_FGET(ainfo_addr, ai_termid.at_type) ==
		    AU_IPv4) {
			au_uwrite(au_to_in_addr(
				(struct in_addr *)STRUCT_FGETP(ainfo_addr,
					ai_termid.at_addr)));
		} else {
			au_uwrite(au_to_in_addr_ex(
				(int32_t *)STRUCT_FGETP(ainfo_addr,
					ai_termid.at_addr)));
		}
		au_uwrite(au_to_arg32((char)1, "as_success",
			(uint32_t)STRUCT_FGET(ainfo_addr, ai_mask.as_success)));
		au_uwrite(au_to_arg32((char)1, "as_failure",
			(uint32_t)STRUCT_FGET(ainfo_addr, ai_mask.as_failure)));
		au_uwrite(au_to_arg32((char)1, "asid",
			(uint32_t)STRUCT_FGET(ainfo_addr, ai_asid)));
		break;
	case AUE_AUDITSVC:
		/*
		 * convert file pointer to file descriptor
		 * Note: fd ref count incremented here
		 */
		if ((fp = getf((uint_t)a1)) == NULL)
			return;
		fad = (struct f_audit_data *)F2A(fp);
		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(2, "no path: fd", (uint32_t)a1));
		}

		vp = fp->f_vnode;	/* include vnode attributes */
		audit_attributes(vp);

		/* decrement file descriptor ref count */
		releasef((uint_t)a1);

		au_uwrite(au_to_arg32(3, "limit", (uint32_t)a2));
		break;
	case AUE_AUDITON_SETKMASK:
		if (copyin((caddr_t)a2, &mask, sizeof (au_mask_t)))
				return;
		au_uwrite(au_to_arg32(
			2, "setkmask:as_success", (uint32_t)mask.as_success));
		au_uwrite(au_to_arg32(
			2, "setkmask:as_failure", (uint32_t)mask.as_failure));
		break;
	case AUE_AUDITON_SPOLICY:
		if (copyin((caddr_t)a2, &policy, sizeof (int)))
			return;
		au_uwrite(au_to_arg32(3, "setpolicy", (uint32_t)policy));
		break;
	case AUE_AUDITON_SQCTRL: {
		STRUCT_DECL(au_qctrl, qctrl);
		model_t model;

		model = get_udatamodel();
		STRUCT_INIT(qctrl, model);
		if (copyin((caddr_t)a2, STRUCT_BUF(qctrl), STRUCT_SIZE(qctrl)))
				return;
		if (model == DATAMODEL_ILP32) {
			au_uwrite(au_to_arg32(
				3, "setqctrl:aq_hiwater",
				(uint32_t)STRUCT_FGET(qctrl, aq_hiwater)));
			au_uwrite(au_to_arg32(
				3, "setqctrl:aq_lowater",
				(uint32_t)STRUCT_FGET(qctrl, aq_lowater)));
			au_uwrite(au_to_arg32(
				3, "setqctrl:aq_bufsz",
				(uint32_t)STRUCT_FGET(qctrl, aq_bufsz)));
			au_uwrite(au_to_arg32(
				3, "setqctrl:aq_delay",
				(uint32_t)STRUCT_FGET(qctrl, aq_delay)));
		} else {
			au_uwrite(au_to_arg64(
				3, "setqctrl:aq_hiwater",
				(uint64_t)STRUCT_FGET(qctrl, aq_hiwater)));
			au_uwrite(au_to_arg64(
				3, "setqctrl:aq_lowater",
				(uint64_t)STRUCT_FGET(qctrl, aq_lowater)));
			au_uwrite(au_to_arg64(
				3, "setqctrl:aq_bufsz",
				(uint64_t)STRUCT_FGET(qctrl, aq_bufsz)));
			au_uwrite(au_to_arg64(
				3, "setqctrl:aq_delay",
				(uint64_t)STRUCT_FGET(qctrl, aq_delay)));
		}
		break;
	}
	case AUE_AUDITON_SETUMASK:
		STRUCT_INIT(ainfo, get_udatamodel());
		if (copyin((caddr_t)uap->a2, STRUCT_BUF(ainfo),
				STRUCT_SIZE(ainfo))) {
				return;
		}
		au_uwrite(au_to_arg32(3, "setumask:as_success",
			(uint32_t)STRUCT_FGET(ainfo, ai_mask.as_success)));
		au_uwrite(au_to_arg32(3, "setumask:as_failure",
			(uint32_t)STRUCT_FGET(ainfo, ai_mask.as_failure)));
		break;
	case AUE_AUDITON_SETSMASK:
		STRUCT_INIT(ainfo, get_udatamodel());
		if (copyin((caddr_t)uap->a2, STRUCT_BUF(ainfo),
				STRUCT_SIZE(ainfo))) {
				return;
		}
		au_uwrite(au_to_arg32(3, "setsmask:as_success",
			(uint32_t)STRUCT_FGET(ainfo, ai_mask.as_success)));
		au_uwrite(au_to_arg32(3, "setsmask:as_failure",
			(uint32_t)STRUCT_FGET(ainfo, ai_mask.as_failure)));
		break;
	case AUE_AUDITON_SETCOND:
		if (copyin((caddr_t)a2, &auditstate, sizeof (int)))
			return;
		au_uwrite(au_to_arg32(3, "setcond", (uint32_t)auditstate));
		break;
	case AUE_AUDITON_SETCLASS:
		if (copyin((caddr_t)a2, &event, sizeof (au_evclass_map_t)))
			return;
		au_uwrite(au_to_arg32(
			2, "setclass:ec_event", (uint32_t)event.ec_number));
		au_uwrite(au_to_arg32(
			3, "setclass:ec_class", (uint32_t)event.ec_class));
		break;
	case AUE_GETAUID:
	case AUE_GETAUDIT:
	case AUE_GETAUDIT_ADDR:
	case AUE_AUDIT:
	case AUE_GETPORTAUDIT:
	case AUE_AUDITON_GPOLICY:
	case AUE_AUDITON_GQCTRL:
	case AUE_AUDITON_GETKMASK:
	case AUE_AUDITON_GETCWD:
	case AUE_AUDITON_GETCAR:
	case AUE_AUDITON_GETSTAT:
	case AUE_AUDITON_SETSTAT:
	case AUE_AUDITON_GETCOND:
	case AUE_AUDITON_GETCLASS:
		break;
	default:
		break;
	}

}	/* AUS_AUDITSYS */


#if 0
/* only audit privileged operations for systeminfo(2) system call */
static au_event_t
aui_sysinfo(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t command;

	struct a {
		long	command;
		long	buf;		/* char * */
		long	count;
	} *uap = (struct a *)clwp->lwp_ap;

	command = (uint32_t)uap->command;

	switch (command) {
	case SI_SET_HOSTNAME:
	case SI_SET_SRPC_DOMAIN:
		e = AUE_SYSINFO;
		break;
	default:
		e = AUE_NULL;
		break;
	}
	return (e);
}
#endif

/*ARGSUSED*/
static void
aus_sysinfo(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint32_t command;
	size_t len, maxlen;
	char *name;
	uintptr_t buf;

	struct a {
		long	command;
		long	buf;		/* char * */
		long	count;
	} *uap = (struct a *)clwp->lwp_ap;

	command = (uint32_t)uap->command;
	buf = (uintptr_t)uap->buf;

	au_uwrite(au_to_arg32(1, "cmd", command));

	switch (command) {
	case SI_SET_HOSTNAME:
	{
		if (!suser(CRED()))
			return;

		maxlen = SYS_NMLN;
		name = kmem_zalloc(maxlen, KM_SLEEP);
		if (copyinstr((caddr_t)buf, name, SYS_NMLN, &len))
			break;

		/*
		 * Must be non-NULL string and string
		 * must be less than SYS_NMLN chars.
		 */
		if (len < 2 || (len == SYS_NMLN && name[SYS_NMLN - 1] != '\0'))
			break;

		au_uwrite(au_to_text(name));
		break;
	}

	case SI_SET_SRPC_DOMAIN:
	{
		if (!suser(CRED()))
			return;

		maxlen = SYS_NMLN;
		name = (char *)kmem_zalloc(maxlen, KM_SLEEP);
		if (copyinstr((caddr_t)buf, name, SYS_NMLN, &len))
			break;

		/*
		 * If string passed in is longer than length
		 * allowed for domain name, fail.
		 */
		if (len == SYS_NMLN && name[SYS_NMLN - 1] != '\0')
			break;

		au_uwrite(au_to_text(name));
		break;
	}

	default:
		return;
	}

	kmem_free(name, maxlen);
}

static au_event_t
aui_modctl(au_event_t e)
{
	klwp_id_t clwp = ttolwp(curthread);
	uint_t cmd;

	struct a {
		long	cmd;
	} *uap = (struct a *)clwp->lwp_ap;

	cmd = (uint_t)uap->cmd;

	switch (cmd) {
	case MODLOAD:
		e = AUE_MODLOAD;
		break;
	case MODUNLOAD:
		e = AUE_MODUNLOAD;
		break;
	case MODCONFIG:
		e = AUE_MODCONFIG;
		break;
	case MODADDMAJBIND:
		e = AUE_MODADDMAJ;
		break;
	default:
		e = AUE_NULL;
		break;
	}
	return (e);
}


/*ARGSUSED*/
static void
aus_modctl(struct t_audit_data *tad)
{
	klwp_id_t clwp = ttolwp(curthread);
	void *a	= clwp->lwp_ap;
	uint_t use_path;

	switch (tad->tad_event) {
	case AUE_MODLOAD: {
		typedef struct {
			long	cmd;
			long	use_path;
			long	filename;		/* char * */
		} modloada_t;

		char *filenamep;
		uintptr_t fname;
		extern char *default_path;

		fname = (uintptr_t)((modloada_t *)a)->filename;
		use_path = (uint_t)((modloada_t *)a)->use_path;

			/* space to hold path */
		filenamep = kmem_zalloc(MOD_MAXPATH, KM_SLEEP);
			/* get string */
		if (copyinstr((caddr_t)fname, filenamep, MOD_MAXPATH, 0)) {
				/* free allocated path */
			kmem_free(filenamep, MOD_MAXPATH);
			return;
		}
			/* ensure it's null terminated */
		filenamep[MOD_MAXPATH - 1] = 0;

		if (use_path)
			au_uwrite(au_to_text(default_path));
		au_uwrite(au_to_text(filenamep));

			/* release temporary memory */
		kmem_free(filenamep, MOD_MAXPATH);
		break;
	}
	case AUE_MODUNLOAD: {
		typedef struct {
			long	cmd;
			long	id;
		} modunloada_t;

		uint32_t id = (uint32_t)((modunloada_t *)a)->id;

		au_uwrite(au_to_arg32(1, "id", id));
		break;
	}
	case AUE_MODCONFIG: {
		STRUCT_DECL(modconfig, mc);
		typedef struct {
			long	cmd;
			long	subcmd;
			long	data;		/* int * */
		} modconfiga_t;
		char	*drvname;

		uintptr_t data = (uintptr_t)((modconfiga_t *)a)->data;

		STRUCT_INIT(mc, get_udatamodel());
			/* sanitize buffer */
		bzero((caddr_t)STRUCT_BUF(mc), STRUCT_SIZE(mc));
			/* get user arguments */
		if (copyin((caddr_t)data, (caddr_t)STRUCT_BUF(mc),
				STRUCT_SIZE(mc)) != 0)
			return;

		drvname = STRUCT_FGET(mc, drvname);
		if (drvname[0] != NULL) {
				/* safety */
			drvname[255] = '\0';
			au_uwrite(au_to_text(drvname));
		} else
			au_uwrite(au_to_text("no drvname"));
		break;
	}
	case AUE_MODADDMAJ: {
		STRUCT_DECL(modconfig, mc);
		typedef struct {
			long	cmd;
			long	subcmd;
			long	data;		/* int * */
		} modconfiga_t;

		STRUCT_DECL(aliases, alias);
		caddr_t ap;
		int i, num_aliases;
		char *drvname, *mc_drvname;
		char *name;
		extern char *ddi_major_to_name(major_t);
		model_t model;

		uintptr_t data = (uintptr_t)((modconfiga_t *)a)->data;

		model = get_udatamodel();
		STRUCT_INIT(mc, model);
			/* sanitize buffer */
		bzero((caddr_t)STRUCT_BUF(mc), STRUCT_SIZE(mc));
			/* get user arguments */
		if (copyin((caddr_t)data, (caddr_t)STRUCT_BUF(mc),
				STRUCT_SIZE(mc)) != 0)
			return;

		mc_drvname = STRUCT_FGET(mc, drvname);
		if ((drvname = ddi_major_to_name(
			(major_t)STRUCT_FGET(mc, major))) != NULL &&
			strncmp(drvname, mc_drvname, 256) != 0) {
				/* safety */
			if (mc_drvname[0] != NULL) {
				mc_drvname[255] = '\0';
				au_uwrite(au_to_text(mc_drvname));
			}
				/* drvname != NULL from test above */
			au_uwrite(au_to_text(drvname));
			return;
		}

		if (mc_drvname[0] != NULL) {
				/* safety */
			mc_drvname[255] = '\0';
			au_uwrite(au_to_text(mc_drvname));
		} else
			au_uwrite(au_to_text("no drvname"));

		num_aliases = STRUCT_FGET(mc, num_aliases);
		au_uwrite(au_to_arg32(5, "", (uint32_t)num_aliases));
		ap = (caddr_t)STRUCT_FGETP(mc, ap);
		name = (char *)kmem_alloc(256, KM_SLEEP);
		STRUCT_INIT(alias, model);
		for (i = 0; i < num_aliases; i++) {
			bzero((caddr_t)STRUCT_BUF(alias),
					STRUCT_SIZE(alias));
			if (copyin((caddr_t)ap, (caddr_t)STRUCT_BUF(alias),
					STRUCT_SIZE(alias)) != 0)
				break;
			if (copyin(STRUCT_FGETP(alias, a_name), (caddr_t)name,
				STRUCT_FGET(alias, a_len)) != 0)
				break;
			name[255] = '\0';
			au_uwrite(au_to_text(name));
			ap = (caddr_t)STRUCT_FGETP(alias, a_next);
		}
		kmem_free(name, 256);
		break;
	}
	default:
		break;
	}
}

/* copy from common/fs/sockfs/socksubr.c */
#ifdef MY_SOCK
static int
au_so_lock_single(struct sonode *so, int flag, int fmode)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	ASSERT(flag & (SOLOCKED|SOREADLOCKED));
	ASSERT((flag & ~(SOLOCKED|SOREADLOCKED)) == 0);

	while (so->so_flag & flag) {
		if (fmode & (FNDELAY|FNONBLOCK))
			return (EWOULDBLOCK);
		so->so_flag |= SOWANT;
		cv_wait(&so->so_want_cv, &so->so_lock);
	}
	so->so_flag |= flag;
	return (0);
}

static void
au_so_unlock_single(struct sonode *so, int flag)
{
	ASSERT(MUTEX_HELD(&so->so_lock));
	ASSERT(flag & (SOLOCKED|SOREADLOCKED));
	ASSERT((flag & ~(SOLOCKED|SOREADLOCKED)) == 0);
	ASSERT(so->so_flag & flag);

	if (so->so_flag & SOWANT)
		cv_broadcast(&so->so_want_cv);
	so->so_flag &= ~(SOWANT|flag);
}


#else /* MY_SOCK */

char _depends_on[] = "fs/sockfs";
extern int 	so_lock_single(struct sonode *, int, int);
extern void	so_unlock_single(struct sonode *, int);

#define	au_so_unlock_single	so_unlock_single
#define	au_so_lock_single	so_lock_single

#endif

/*
 * private version of getsonode that does not do eprintsoline()
 */
static struct sonode *
au_getsonode(int sock, int *errorp, file_t **fpp)
{
	file_t *fp;
	register vnode_t *vp;
	struct sonode *so;

	if ((fp = getf(sock)) == NULL) {
		*errorp = EBADF;
		return (NULL);
	}
	vp = fp->f_vnode;
	/* Check if it is a socket */
	if (vp->v_type != VSOCK) {
		releasef(sock);
		*errorp = ENOTSOCK;
		return (NULL);
	}
	/*
	 * Use the stream head to find the real socket vnode.
	 * This is needed when namefs sits above sockfs.
	 */
	ASSERT(vp->v_stream);
	ASSERT(vp->v_stream->sd_vnode);
	vp = vp->v_stream->sd_vnode;
	so = VTOSO(vp);
	if (so->so_version == SOV_STREAM) {
		releasef(sock);
		*errorp = ENOTSOCK;
		return (NULL);
	}
	if (fpp)
		*fpp = fp;
	return (so);
}

/*ARGSUSED*/
static void
auf_accept(
	struct t_audit_data *tad,
	int	error,
	rval_t	*rval)
{
	int64_t	fd;
	struct sonode *so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	int err;
	int len;
	short so_family, so_type;
	int add_sock_token = 0;

	/* hope this is the right union element */
	fd = rval->r_vals;

	if (error) {
		/* can't trust socket contents. Just return */
		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		return;
	}

	if ((so = au_getsonode((int)fd, &err, NULL)) == NULL) {
		/*
		 * not security relevant if doing a accept from non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:
		/*
		 * XXX - what about other socket types for AF_INET (e.g. DGRAM)
		 */
		if (so->so_type == SOCK_STREAM) {

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			/*
			 * no local address then need to get it from lower
			 * levels. only put out record on first read ala
			 * AUE_WRITE.
			 */
			if (so->so_state & SS_ISBOUND) {
				/* only done once on a connection */
				(void) sogetsockname(so);
				(void) sogetpeername(so);

				/* get local and foreign addresses */
				mutex_enter(&so->so_lock);
				len = min(so->so_laddr_len, sizeof (so_laddr));
				bcopy(so->so_laddr_sa, so_laddr, len);
				len = min(so->so_faddr_len, sizeof (so_faddr));
				bcopy(so->so_faddr_sa, so_faddr, len);
				mutex_exit(&so->so_lock);
			}

			add_sock_token = 1;
		}
		break;

	default:
		/* AF_UNIX, AF_ROUTE, AF_KEY do not support accept */
		break;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(0, "family", (uint32_t)(so_family)));
		au_uwrite(au_to_arg32(0, "type", (uint32_t)(so_type)));
		return;
	}

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*ARGSUSED*/
static void
auf_bind(struct t_audit_data *tad, int error, rval_t *rvp)
{
	struct a {
		long	fd;
		long	addr;
		long	len;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct sonode *so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	int err, fd;
	int len;
	short so_family, so_type;
	int add_sock_token = 0;

	fd = (int)uap->fd;

	/*
	 * bind failed, then nothing extra to add to audit record.
	 */
	if (error) {
		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		/* XXX may want to add failed address some day */
		return;
	}

	if ((so = au_getsonode(fd, &err, NULL)) == NULL) {
		/*
		 * not security relevant if doing a bind from non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:

		bzero(so_faddr, sizeof (so_faddr));

		if (so->so_state & SS_ISBOUND) {
			/* only done once on a connection */
			(void) sogetsockname(so);
		}

		mutex_enter(&so->so_lock);
		len = min(so->so_laddr_len, sizeof (so_laddr));
		bcopy(so->so_laddr_sa, so_laddr, len);
		mutex_exit(&so->so_lock);

		add_sock_token = 1;

		break;

	case AF_UNIX:
		/* token added by lookup */
		break;
	default:
		/* AF_ROUTE, AF_KEY do not support accept */
		break;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)(so_family)));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)(so_type)));
		return;
	}

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*ARGSUSED*/
static void
auf_connect(struct t_audit_data *tad, int error, rval_t *rval)
{
	struct a {
		long	fd;
		long	addr;
		long	len;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct sonode *so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	int err, fd;
	int len;
	short so_family, so_type;
	int add_sock_token = 0;

	fd = (int)uap->fd;


	if ((so = au_getsonode(fd, &err, NULL)) == NULL) {
		/*
		 * not security relevant if doing a connect from non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:
		/*
		 * no local address then need to get it from lower
		 * levels.
		 */
		if (so->so_state & SS_ISBOUND) {
			/* only done once on a connection */
			(void) sogetsockname(so);
			(void) sogetpeername(so);
		}

		bzero(so_laddr, sizeof (so_laddr));
		bzero(so_faddr, sizeof (so_faddr));

		mutex_enter(&so->so_lock);
		len = min(so->so_laddr_len, sizeof (so_laddr));
		bcopy(so->so_laddr_sa, so_laddr, len);
		if (error) {
			mutex_exit(&so->so_lock);
			if (uap->addr == NULL)
				break;
			if (uap->len <= 0)
				break;
			len = min(uap->len, sizeof (so_faddr));
			if (copyin((caddr_t)(uap->addr), so_faddr, len) != 0)
				break;
#ifdef NOTYET
			au_uwrite(au_to_data(AUP_HEX, AUR_CHAR, len, so_faddr));
#endif
		} else {
			/* sanity check on length */
			len = min(so->so_faddr_len, sizeof (so_faddr));
			bcopy(so->so_faddr_sa, so_faddr, len);
			mutex_exit(&so->so_lock);
		}

		add_sock_token = 1;

		break;

	case AF_UNIX:
		/* does a lookup on name */
		break;

	default:
		/* AF_ROUTE, AF_KEY do not support accept */
		break;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)(so_family)));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)(so_type)));
		return;
	}

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*ARGSUSED*/
static void
aus_shutdown(struct t_audit_data *tad)
{
	struct a {
		long	fd;
		long	how;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct sonode *so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	int err, fd;
	int len;
	short so_family, so_type;
	int add_sock_token = 0;
	file_t *fp;				/* unix domain sockets */
	struct f_audit_data *fad;		/* unix domain sockets */

	fd = (int)uap->fd;

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/*
		 * not security relevant if doing a shutdown using non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:

		bzero(so_laddr, sizeof (so_laddr));
		bzero(so_faddr, sizeof (so_faddr));

		if (so->so_state & SS_ISBOUND) {
			/*
			 * no local address then need to get it from lower
			 * levels.
			 */
			if (so->so_laddr_len == 0)
				(void) sogetsockname(so);
			if (so->so_faddr_len == 0)
				(void) sogetpeername(so);
		}

		mutex_enter(&so->so_lock);
		len = min(so->so_laddr_len, sizeof (so_laddr));
		bcopy(so->so_laddr_sa, so_laddr, len);
		len = min(so->so_faddr_len, sizeof (so_faddr));
		bcopy(so->so_faddr_sa, so_faddr, len);
		mutex_exit(&so->so_lock);

		add_sock_token = 1;

		break;

	case AF_UNIX:

		/* get path from file struct here */
		fad = (struct f_audit_data *)F2A(fp);
		ASSERT(fad);

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		break;

	default:
		/*
		 * AF_KEY and AF_ROUTE support shutdown. No socket token
		 * added.
		 */
		break;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)(so_family)));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)(so_type)));
		au_uwrite(au_to_arg32(2, "how", (uint32_t)(uap->how)));
		return;
	}

	au_uwrite(au_to_arg32(2, "how", (uint32_t)(uap->how)));

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*ARGSUSED*/
static void
auf_setsockopt(struct t_audit_data *tad, int error, rval_t *rval)
{
	struct a {
		long	fd;
		long	level;
		long	optname;
		long	*optval;
		long	optlen;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct sonode	*so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	char		val[AU_BUFSIZE];
	int		err, fd;
	int		len;
	short so_family, so_type;
	int		add_sock_token = 0;
	file_t *fp;				/* unix domain sockets */
	struct f_audit_data *fad;		/* unix domain sockets */

	fd = (int)uap->fd;

	if (error) {
		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		au_uwrite(au_to_arg32(2, "level", (uint32_t)uap->level));
		/* XXX may want to include other arguments */
		return;
	}

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/*
		 * not security relevant if doing a setsockopt from non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:

		bzero((void *)so_laddr, sizeof (so_laddr));
		bzero((void *)so_faddr, sizeof (so_faddr));

		if (so->so_state & SS_ISBOUND) {
			if (so->so_laddr_len == 0)
				(void) sogetsockname(so);
			if (so->so_faddr_len == 0)
				(void) sogetpeername(so);
		}

		/* get local and foreign addresses */
		mutex_enter(&so->so_lock);
		len = min(so->so_laddr_len, sizeof (so_laddr));
		bcopy(so->so_laddr_sa, so_laddr, len);
		len = min(so->so_faddr_len, sizeof (so_faddr));
		bcopy(so->so_faddr_sa, so_faddr, len);
		mutex_exit(&so->so_lock);

		add_sock_token = 1;

		break;

	case AF_UNIX:

		/* get path from file struct here */
		fad = (struct f_audit_data *)F2A(fp);
		ASSERT(fad);

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		break;

	default:
		/*
		 * AF_KEY and AF_ROUTE support setsockopt. No socket token
		 * added.
		 */
		break;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)(so_family)));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)(so_type)));
	}
	au_uwrite(au_to_arg32(2, "level", (uint32_t)(uap->level)));
	au_uwrite(au_to_arg32(3, "optname", (uint32_t)(uap->optname)));

	bzero(val, sizeof (val));
	len = min(uap->optlen, sizeof (val));
	if ((len > 0) &&
	    (copyin((caddr_t)(uap->optval), (caddr_t)&val, len) == 0)) {
		au_uwrite(au_to_arg32(5, "optlen", (uint32_t)(uap->optlen)));
		au_uwrite(au_to_data(AUP_HEX, AUR_BYTE, len, val));
	}

	if (add_sock_token == 0)
		return;

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*ARGSUSED*/
static void
aus_sockconfig(tad)
	struct t_audit_data *tad;
{
	struct a {
		long	domain;
		long	type;
		long	protocol;
		long	devpath;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	char	*kdevpath;
	int	kdevpathlen = MAXPATHLEN + 1;
	size_t	size;

	au_uwrite(au_to_arg32(1, "domain", (uint32_t)uap->domain));
	au_uwrite(au_to_arg32(2, "type", (uint32_t)uap->type));
	au_uwrite(au_to_arg32(3, "protocol", (uint32_t)uap->protocol));

	if (uap->devpath == 0) {
		au_uwrite(au_to_arg32(3, "devpath", (uint32_t)0));
	} else {
		kdevpath = kmem_zalloc(kdevpathlen, KM_SLEEP);

		if (copyinstr((caddr_t)uap->devpath, kdevpath, kdevpathlen,
			&size)) {
			kmem_free(kdevpath, kdevpathlen);
			return;
		}

		if (size > MAXPATHLEN) {
			kmem_free(kdevpath, kdevpathlen);
			return;
		}

		au_uwrite(au_to_text(kdevpath));
		kmem_free(kdevpath, kdevpathlen);
	}
}

/*
 * only audit recvmsg when the system call represents the creation of a new
 * circuit. This effectively occurs for all UDP packets and may occur for
 * special TCP situations where the local host has not set a local address
 * in the socket structure.
 */
/*ARGSUSED*/
static void
auf_recvmsg(
	struct t_audit_data *tad,
	int error,
	rval_t *rvp)
{
	struct a {
		long	fd;
		long	msg;	/* struct msghdr */
		long	flags;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct sonode	*so;
	STRUCT_DECL(msghdr, msg);
	caddr_t msg_name;
	socklen_t msg_namelen;
	int fd;
	int err;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	int len;
	file_t *fp;				/* unix domain sockets */
	struct f_audit_data *fad;		/* unix domain sockets */
	short so_family, so_type;
	int add_sock_token = 0;

	fd = (int)uap->fd;

	/* bail if an error */
	if (error) {
		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		return;
	}

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/*
		 * not security relevant if doing a recvmsg from non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	/*
	 * only putout SOCKET_EX token if INET/INET6 family.
	 * XXX - what do we do about other families?
	 */

	switch (so_family) {
	case AF_INET:
	case AF_INET6:

		/*
		 * if datagram type socket, then just use what is in
		 * socket structure for local address.
		 * XXX - what do we do for other types?
		 */
		if ((so->so_type == SOCK_DGRAM) ||
		    (so->so_type == SOCK_RAW)) {
			add_sock_token = 1;

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			/* get local address */
			mutex_enter(&so->so_lock);
			len = min(so->so_laddr_len, sizeof (so_laddr));
			bcopy(so->so_laddr_sa, so_laddr, len);
			mutex_exit(&so->so_lock);

			/* get peer address */
			STRUCT_INIT(msg, get_udatamodel());

			if (copyin((caddr_t)(uap->msg),
			    (caddr_t)STRUCT_BUF(msg), STRUCT_SIZE(msg)) != 0) {
				break;
			}
			msg_name = (caddr_t)STRUCT_FGETP(msg, msg_name);
			if (msg_name == NULL) {
				break;
			}

			/* length is value from recvmsg - sanity check */
			msg_namelen = (socklen_t)STRUCT_FGET(msg, msg_namelen);
			if (msg_namelen == 0) {
				break;
			}
			if (copyin(msg_name, so_faddr,
			    sizeof (so_faddr)) != 0) {
				break;
			}

		} else if (so->so_type == SOCK_STREAM) {

			/* get path from file struct here */
			fad = (struct f_audit_data *)F2A(fp);
			ASSERT(fad);

			/*
			 * already processed this file for read attempt
			 * XXX do we need locks here ? Two threads reading
			 * from same file at same time is probably broken
			 * applications.
			 */
			if (fad->fad_flags & FAD_READ) {
				/* don't want to audit every recvmsg attempt */
				tad->tad_flag = 0;
				/* free any residual audit data */
				au_close(&(u_ad), 0, 0, 0);
				releasef(fd);
				return;
			}
			/*
			 * mark things so we know what happened and don't
			 * repeat things
			 */
			/* XXX do we need locks here ? */
			fad->fad_flags |= FAD_READ;

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			if (so->so_state & SS_ISBOUND) {

				if (so->so_laddr_len == 0)
					(void) sogetsockname(so);
				if (so->so_faddr_len == 0)
					(void) sogetpeername(so);

				/* get local and foreign addresses */
				mutex_enter(&so->so_lock);
				len = min(so->so_laddr_len, sizeof (so_laddr));
				bcopy(so->so_laddr_sa, so_laddr, len);
				len = min(so->so_faddr_len, sizeof (so_faddr));
				bcopy(so->so_faddr_sa, so_faddr, len);
				mutex_exit(&so->so_lock);
			}

			add_sock_token = 1;
		}

		/* XXX - what about SOCK_RDM/SOCK_SEQPACKET ??? */

		break;

	case AF_UNIX:
		/*
		 * first check if this is first time through. Too much
		 * duplicate code to put this in an aui_ routine.
		 */

		/* get path from file struct here */
		fad = (struct f_audit_data *)F2A(fp);
		ASSERT(fad);

		/*
		 * already processed this file for read attempt
		 * XXX do we need locks here ? Two threads reading
		 * from same file at same time is probably broken
		 * applications.
		 */
		if (fad->fad_flags & FAD_READ) {
			releasef(fd);
			/* don't want to audit every recvmsg attempt */
			tad->tad_flag = 0;
			/* free any residual audit data */
			au_close(&(u_ad), 0, 0, 0);
			return;
		}
		/*
		 * mark things so we know what happened and don't
		 * repeat things
		 */
		/* XXX do we need locks here ? */
		fad->fad_flags |= FAD_READ;

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		releasef(fd);

		return;

	default:
		break;

	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)so_family));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)so_type));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		return;
	}

	au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*ARGSUSED*/
static void
auf_recvfrom(
	struct t_audit_data *tad,
	int error,
	rval_t *rvp)
{

	struct a {
		long	fd;
		long	msg;	/* char */
		long	len;
		long	flags;
		long	from;	/* struct sockaddr */
		long	fromlen;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	socklen_t	fromlen;
	struct sonode	*so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	int		fd;
	short so_family, so_type;
	int add_sock_token = 0;
	int len;
	int err;
	struct file *fp;
	struct f_audit_data *fad;		/* unix domain sockets */

	fd = (int)uap->fd;

	/* bail if an error */
	if (error) {
		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		return;
	}

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/*
		 * not security relevant if doing a recvmsg from non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	/*
	 * only putout SOCKET_EX token if INET/INET6 family.
	 * XXX - what do we do about other families?
	 */

	switch (so_family) {
	case AF_INET:
	case AF_INET6:

		/*
		 * if datagram type socket, then just use what is in
		 * socket structure for local address.
		 * XXX - what do we do for other types?
		 */
		if ((so->so_type == SOCK_DGRAM) ||
		    (so->so_type == SOCK_RAW)) {
			add_sock_token = 1;

			/* get local address */
			mutex_enter(&so->so_lock);
			len = min(so->so_laddr_len, sizeof (so_laddr));
			bcopy(so->so_laddr_sa, so_laddr, len);
			mutex_exit(&so->so_lock);

			/* get peer address */
			bzero((void *)so_faddr, sizeof (so_faddr));

			/* sanity check */
			if (uap->from == NULL)
				break;

			/* sanity checks */
			if (uap->fromlen == 0)
				break;

			if (copyin((caddr_t)(uap->fromlen), (caddr_t)&fromlen,
			    sizeof (fromlen)) != 0)
				break;

			if (fromlen == 0)
				break;

			/* enforce maximum size */
			if (fromlen > sizeof (so_faddr))
				fromlen = sizeof (so_faddr);

			if (copyin((caddr_t)(uap->from), so_faddr,
			    fromlen) != 0)
				break;

		} else if (so->so_type == SOCK_STREAM) {

			/* get path from file struct here */
			fad = (struct f_audit_data *)F2A(fp);
			ASSERT(fad);

			/*
			 * already processed this file for read attempt
			 * XXX do we need locks here ? Two threads reading
			 * from same file at same time is probably broken
			 * applications.
			 */
			if (fad->fad_flags & FAD_READ) {
				/* don't want to audit every recvfrom attempt */
				tad->tad_flag = 0;
				/* free any residual audit data */
				au_close(&(u_ad), 0, 0, 0);
				releasef(fd);
				return;
			}
			/*
			 * mark things so we know what happened and don't
			 * repeat things
			 */
			/* XXX do we need locks here ? */
			fad->fad_flags |= FAD_READ;

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			if (so->so_state & SS_ISBOUND) {

				if (so->so_laddr_len == 0)
					(void) sogetsockname(so);
				if (so->so_faddr_len == 0)
					(void) sogetpeername(so);

				/* get local and foreign addresses */
				mutex_enter(&so->so_lock);
				len = min(so->so_laddr_len, sizeof (so_laddr));
				bcopy(so->so_laddr_sa, so_laddr, len);
				len = min(so->so_faddr_len, sizeof (so_faddr));
				bcopy(so->so_faddr_sa, so_faddr, len);
				mutex_exit(&so->so_lock);
			}

			add_sock_token = 1;
		}

		/* XXX - what about SOCK_RDM/SOCK_SEQPACKET ??? */

		break;

	case AF_UNIX:
		/*
		 * first check if this is first time through. Too much
		 * duplicate code to put this in an aui_ routine.
		 */

		/* get path from file struct here */
		fad = (struct f_audit_data *)F2A(fp);
		ASSERT(fad);

		/*
		 * already processed this file for read attempt
		 * XXX do we need locks here ? Two threads reading
		 * from same file at same time is probably broken
		 * applications.
		 */
		if (fad->fad_flags & FAD_READ) {
			/* don't want to audit every recvfrom attempt */
			tad->tad_flag = 0;
			/* free any residual audit data */
			au_close(&(u_ad), 0, 0, 0);
			releasef(fd);
			return;
		}
		/*
		 * mark things so we know what happened and don't
		 * repeat things
		 */
		/* XXX do we need locks here ? */
		fad->fad_flags |= FAD_READ;

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		releasef(fd);

		return;

	default:
		break;

	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)so_family));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)so_type));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		return;
	}

	au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));
}

/*ARGSUSED*/
static void
auf_sendmsg(struct t_audit_data *tad, int error, rval_t *rval)
{
	struct a {
		long	fd;
		long	msg;	/* struct msghdr */
		long	flags;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct sonode	*so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	int		err;
	int		fd;
	short so_family, so_type;
	int		add_sock_token = 0;
	int		len;
	struct file	*fp;
	struct f_audit_data *fad;
	caddr_t		msg_name;
	socklen_t	msg_namelen;
	STRUCT_DECL(msghdr, msg);

	fd = (int)uap->fd;

	/* bail if an error */
	if (error) {
		/* XXX include destination address from system call arguments */
		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		return;
	}

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/*
		 * not security relevant if doing a sendmsg from non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:
		/*
		 * if datagram type socket, then just use what is in
		 * socket structure for local address.
		 * XXX - what do we do for other types?
		 */
		if ((so->so_type == SOCK_DGRAM) ||
		    (so->so_type == SOCK_RAW)) {

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			/* get local address */
			mutex_enter(&so->so_lock);
			len = min(so->so_laddr_len, sizeof (so_laddr));
			bcopy(so->so_laddr_sa, so_laddr, len);
			mutex_exit(&so->so_lock);

			/* get peer address */
			STRUCT_INIT(msg, get_udatamodel());

			if (copyin((caddr_t)(uap->msg),
			    (caddr_t)STRUCT_BUF(msg), STRUCT_SIZE(msg)) != 0) {
				break;
			}
			msg_name = (caddr_t)STRUCT_FGETP(msg, msg_name);
			if (msg_name == NULL)
				break;

			msg_namelen = (socklen_t)STRUCT_FGET(msg, msg_namelen);
			/* length is value from recvmsg - sanity check */
			if (msg_namelen == 0)
				break;

			if (copyin(msg_name, so_faddr,
			    sizeof (so_faddr)) != 0)
				break;

			add_sock_token = 1;

		} else if (so->so_type == SOCK_STREAM) {

			/* get path from file struct here */
			fad = (struct f_audit_data *)F2A(fp);
			ASSERT(fad);

			/*
			 * already processed this file for write attempt
			 * XXX do we need locks here ? Two threads reading
			 * from same file at same time is probably broken
			 * applications.
			 */
			if (fad->fad_flags & FAD_WRITE) {
				releasef(fd);
				/* don't want to audit every sendmsg attempt */
				tad->tad_flag = 0;
				/* free any residual audit data */
				au_close(&(u_ad), 0, 0, 0);
				return;
			}

			/*
			 * mark things so we know what happened and don't
			 * repeat things
			 */
			/* XXX do we need locks here ? */
			fad->fad_flags |= FAD_WRITE;

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			if (so->so_state & SS_ISBOUND) {

				if (so->so_laddr_len == 0)
					(void) sogetsockname(so);
				if (so->so_faddr_len == 0)
					(void) sogetpeername(so);

				/* get local and foreign addresses */
				mutex_enter(&so->so_lock);
				len = min(so->so_laddr_len, sizeof (so_laddr));
				bcopy(so->so_laddr_sa, so_laddr, len);
				len = min(so->so_faddr_len, sizeof (so_faddr));
				bcopy(so->so_faddr_sa, so_faddr, len);
				mutex_exit(&so->so_lock);
			}

			add_sock_token = 1;
		}

		/* XXX - what about SOCK_RAW/SOCK_RDM/SOCK_SEQPACKET ??? */

		break;

	case AF_UNIX:
		/*
		 * first check if this is first time through. Too much
		 * duplicate code to put this in an aui_ routine.
		 */

		/* get path from file struct here */
		fad = (struct f_audit_data *)F2A(fp);
		ASSERT(fad);

		/*
		 * already processed this file for write attempt
		 * XXX do we need locks here ? Two threads reading
		 * from same file at same time is probably broken
		 * applications.
		 */
		if (fad->fad_flags & FAD_WRITE) {
			releasef(fd);
			/* don't want to audit every sendmsg attempt */
			tad->tad_flag = 0;
			/* free any residual audit data */
			au_close(&(u_ad), 0, 0, 0);
			return;
		}
		/*
		 * mark things so we know what happened and don't
		 * repeat things
		 */
		/* XXX do we need locks here ? */
		fad->fad_flags |= FAD_WRITE;

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		releasef(fd);

		return;

	default:
		break;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)so_family));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)so_type));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		return;
	}

	au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));
}

/*ARGSUSED*/
static void
auf_sendto(struct t_audit_data *tad, int error, rval_t *rval)
{
	struct a {
		long	fd;
		long	msg;	/* char */
		long	len;
		long	flags;
		long	to;	/* struct sockaddr */
		long	tolen;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct sonode	*so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	socklen_t	tolen;
	int		err;
	int		fd;
	int		len;
	short so_family, so_type;
	int		add_sock_token = 0;
	struct file	*fp;
	struct f_audit_data *fad;

	fd = (int)uap->fd;

	/* bail if an error */
	if (error) {
		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		/* XXX include destination address from system call arguments */
		return;
	}

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/*
		 * not security relevant if doing a sendto using non socket
		 * so no extra tokens. Should probably turn off audit record
		 * generation here.
		 */
		return;
	}

	so_family = so->so_family;
	so_type   = so->so_type;

	/*
	 * only putout SOCKET_EX token if INET/INET6 family.
	 * XXX - what do we do about other families?
	 */

	switch (so_family) {
	case AF_INET:
	case AF_INET6:

		/*
		 * if datagram type socket, then just use what is in
		 * socket structure for local address.
		 * XXX - what do we do for other types?
		 */
		if ((so->so_type == SOCK_DGRAM) ||
		    (so->so_type == SOCK_RAW)) {

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			/* get local address */
			mutex_enter(&so->so_lock);
			len = min(so->so_laddr_len, sizeof (so_laddr));
			bcopy(so->so_laddr_sa, so_laddr, len);
			mutex_exit(&so->so_lock);

			/* get peer address */

			/* sanity check */
			if (uap->to == NULL)
				break;

			/* sanity checks */
			if (uap->tolen == 0)
				break;

			tolen = (socklen_t)uap->tolen;

			/* enforce maximum size */
			if (tolen > sizeof (so_faddr))
				tolen = sizeof (so_faddr);

			if (copyin((caddr_t)(uap->to), so_faddr, tolen) != 0)
				break;

			add_sock_token = 1;
		} else {
			/*
			 * check if this is first time through.
			 */

			/* get path from file struct here */
			fad = (struct f_audit_data *)F2A(fp);
			ASSERT(fad);

			/*
			 * already processed this file for write attempt
			 * XXX do we need locks here ? Two threads reading
			 * from same file at same time is probably broken
			 * applications.
			 */
			if (fad->fad_flags & FAD_WRITE) {
				/* don't want to audit every sendto attempt */
				tad->tad_flag = 0;
				/* free any residual audit data */
				au_close(&(u_ad), 0, 0, 0);
				releasef(fd);
				return;
			}
			/*
			 * mark things so we know what happened and don't
			 * repeat things
			 */
			/* XXX do we need locks here ? */
			fad->fad_flags |= FAD_WRITE;

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			if (so->so_state & SS_ISBOUND) {

				if (so->so_laddr_len == 0)
					(void) sogetsockname(so);
				if (so->so_faddr_len == 0)
					(void) sogetpeername(so);

				/* get local and foreign addresses */
				mutex_enter(&so->so_lock);
				len = min(so->so_laddr_len, sizeof (so_laddr));
				bcopy(so->so_laddr_sa, so_laddr, len);
				len = min(so->so_faddr_len, sizeof (so_faddr));
				bcopy(so->so_faddr_sa, so_faddr, len);
				mutex_exit(&so->so_lock);
			}

			add_sock_token = 1;
		}

		/* XXX - what about SOCK_RDM/SOCK_SEQPACKET ??? */

		break;

	case AF_UNIX:
		/*
		 * first check if this is first time through. Too much
		 * duplicate code to put this in an aui_ routine.
		 */

		/* get path from file struct here */
		fad = (struct f_audit_data *)F2A(fp);
		ASSERT(fad);

		/*
		 * already processed this file for write attempt
		 * XXX do we need locks here ? Two threads reading
		 * from same file at same time is probably broken
		 * applications.
		 */
		if (fad->fad_flags & FAD_WRITE) {
			/* don't want to audit every sendto attempt */
			tad->tad_flag = 0;
			/* free any residual audit data */
			au_close(&(u_ad), 0, 0, 0);
			releasef(fd);
			return;
		}
		/*
		 * mark things so we know what happened and don't
		 * repeat things
		 */
		/* XXX do we need locks here ? */
		fad->fad_flags |= FAD_WRITE;

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		releasef(fd);

		return;

	default:
		break;

	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	if (add_sock_token == 0) {
		au_uwrite(au_to_arg32(1, "family", (uint32_t)so_family));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)so_type));
		au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));
		return;
	}

	au_uwrite(au_to_arg32(3, "flags", (uint32_t)(uap->flags)));

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*
 * XXX socket(2) may be equivilent to open(2) on a unix domain
 * socket. This needs investigation.
 */

/*ARGSUSED*/
static void
aus_socket(struct t_audit_data *tad)
{
	struct a {
		long	domain;
		long	type;
		long	protocol;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	au_uwrite(au_to_arg32(1, "domain", (uint32_t)uap->domain));
	au_uwrite(au_to_arg32(2, "type", (uint32_t)uap->type));
	au_uwrite(au_to_arg32(3, "protocol", (uint32_t)uap->protocol));
}

/*ARGSUSED*/
static void
aus_sigqueue(struct t_audit_data *tad)
{
	struct a {
		long	pid;
		long	signo;
		long	*val;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;
	struct proc *p;
	struct p_audit_data *pad;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	pid = (pid_t)uap->pid;

	au_uwrite(au_to_arg32(2, "signal", (uint32_t)uap->signo));
	if (pid > 0) {
		mutex_enter(&pidlock);
		if ((p = prfind(pid)) == (struct proc *)0) {
			mutex_exit(&pidlock);
			return;
		}
		mutex_enter(&p->p_lock); /* so process doesn't go away */
		mutex_exit(&pidlock);
		uid  = p->p_cred->cr_uid;
		gid  = p->p_cred->cr_gid;
		ruid = p->p_cred->cr_ruid;
		rgid = p->p_cred->cr_rgid;
		pad  = (struct p_audit_data *)P2A(p);
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		mutex_exit(&p->p_lock);
		au_uwrite(au_to_process(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));
	}
	else
		au_uwrite(au_to_arg32(1, "process ID", (uint32_t)pid));
}

/*ARGSUSED*/
static void
aus_inst_sync(struct t_audit_data *tad)
{
	struct a {
		long	name;	/* char */
		long	flags;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	au_uwrite(au_to_arg32(2, "flags", (uint32_t)uap->flags));
}

/*ARGSUSED*/
static void
aus_p_online(struct t_audit_data *tad)
{
	struct a {
		long	processor_id;
		long	flag;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct flags {
			int	flag;
			char	*cflag;
	} aflags[3] = {
			{ P_ONLINE, "P_ONLINE"},
			{ P_OFFLINE, "P_OFFLINE"},
			{ P_STATUS, "P_STATUS"}
	};
	int i;
	char *cflag;

	au_uwrite(au_to_arg32(1, "processor ID", (uint32_t)uap->processor_id));
	au_uwrite(au_to_arg32(2, "flag", (uint32_t)uap->flag));

	for (i = 0; i < 3; i++) {
		if (aflags[i].flag == uap->flag)
			break;
	}
	cflag = (i == 3) ? "bad flag":aflags[i].cflag;

	au_uwrite(au_to_text(cflag));
}

/*ARGSUSED*/
static void
aus_processor_bind(struct t_audit_data *tad)
{
	struct a {
		long	id_type;
		long	id;
		long	processor_id;
		long	obind;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	struct proc *p;
	int lwpcnt;
	struct p_audit_data *pad;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	au_uwrite(au_to_arg32(1, "ID type", (uint32_t)uap->id_type));
	au_uwrite(au_to_arg32(2, "ID", (uint32_t)uap->id));
	if (uap->processor_id == PBIND_NONE)
		au_uwrite(au_to_text("PBIND_NONE"));
	else
		au_uwrite(au_to_arg32(3, "processor_id",
					(uint32_t)uap->processor_id));


	switch (uap->id_type) {
	case P_MYID:
	case P_LWPID:
		mutex_enter(&pidlock);
		p = ttoproc(curthread);
		mutex_enter(&p->p_lock);
		mutex_exit(&pidlock);
		if (p == NULL || p->p_as == &kas) {
			mutex_exit(&p->p_lock);
			return;
		}
		lwpcnt = p->p_lwpcnt;
		uid  = p->p_cred->cr_uid;
		gid  = p->p_cred->cr_gid;
		ruid = p->p_cred->cr_ruid;
		rgid = p->p_cred->cr_rgid;
		pid  = p->p_pid;
		pad  = (struct p_audit_data *)P2A(p);
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		mutex_exit(&p->p_lock);
		au_uwrite(au_to_process(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));
		break;
	case P_PID:
		mutex_enter(&pidlock);
		p = prfind(uap->id);
		mutex_enter(&p->p_lock);
		mutex_exit(&pidlock);
		if (p == NULL || p->p_as == &kas) {
			mutex_exit(&p->p_lock);
			return;
		}
		lwpcnt = p->p_lwpcnt;
		uid  = p->p_cred->cr_uid;
		gid  = p->p_cred->cr_gid;
		ruid = p->p_cred->cr_ruid;
		rgid = p->p_cred->cr_rgid;
		pid  = p->p_pid;
		pad  = (struct p_audit_data *)P2A(p);
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		mutex_exit(&p->p_lock);
		au_uwrite(au_to_process(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));
		break;
	default:
		return;
	}

	if (uap->processor_id == PBIND_NONE &&
	    (!(uap->id_type == P_LWPID && lwpcnt > 1)))
		au_uwrite(au_to_text("PBIND_NONE for process"));
	else
		au_uwrite(au_to_arg32(3, "processor_id",
					(uint32_t)uap->processor_id));
}

/*ARGSUSED*/
static au_event_t
aui_doorfs(au_event_t e)
{
	unsigned code;

	struct a {		/* doorfs */
		long	a1;
		long	a2;
		long	a3;
		long	a4;
		long	a5;
		long	code;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	/*
	 *	audit formats for several of the
	 *	door calls have not yet been determined
	 */
	code = (unsigned)uap->code;
	switch (code) {
	case DOOR_CALL:
		e = AUE_DOORFS_DOOR_CALL;
		break;
	case DOOR_RETURN:
		e = AUE_NULL;
		break;
	case DOOR_CREATE:
		e = AUE_DOORFS_DOOR_CREATE;
		break;
	case DOOR_REVOKE:
		e = AUE_DOORFS_DOOR_REVOKE;
		break;
	case DOOR_INFO:
		e = AUE_NULL;
		break;
	case DOOR_CRED:
		e = AUE_NULL;
		break;
	case DOOR_BIND:
		e = AUE_NULL;
		break;
	case DOOR_UNBIND:
		e = AUE_NULL;
		break;
	default:	/* illegal system call */
		e = AUE_NULL;
		break;
	}

	return (e);
}

static door_node_t *
au_door_lookup(int did)
{
	vnode_t	*vp;
	file_t *fp;

	if ((fp = getf(did)) == NULL)
		return (NULL);
	/*
	 * Use the underlying vnode (we may be namefs mounted)
	 */
	if (VOP_REALVP(fp->f_vnode, &vp))
		vp = fp->f_vnode;

	if (vp == NULL || vp->v_type != VDOOR) {
		releasef(did);
		return (NULL);
	}

	return (VTOD(vp));
}

/*ARGSUSED*/
static void
aus_doorfs(struct t_audit_data *tad)
{

	struct a {		/* doorfs */
		long	a1;
		long	a2;
		long	a3;
		long	a4;
		long	a5;
		long	code;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	door_node_t	*dp;
	struct proc	*p;
	unsigned	did;
	struct p_audit_data *pad;
	uid_t uid, ruid;
	gid_t gid, rgid;
	pid_t pid;
	au_id_t auid;
	au_asid_t asid;
	au_termid_t atid;

	did = (unsigned)uap->a1;

	switch (tad->tad_event) {
	case AUE_DOORFS_DOOR_CALL:
		au_uwrite(au_to_arg32(1, "door ID", (uint32_t)did));
		if ((dp = au_door_lookup(did)) == NULL)
			break;

		if (DOOR_INVALID(dp)) {
			releasef(did);
			break;
		}

		if ((p = dp->door_target) == NULL) {
			releasef(did);
			break;
		}
		mutex_enter(&p->p_lock);
		releasef(did);
		uid  = p->p_cred->cr_uid;
		gid  = p->p_cred->cr_gid;
		ruid = p->p_cred->cr_ruid;
		rgid = p->p_cred->cr_rgid;
		pid  = p->p_pid;
		pad  = (struct p_audit_data *)P2A(p);
		auid = pad->pad_auid;
		asid = pad->pad_asid;
		atid = pad->pad_termid;
		mutex_exit(&p->p_lock);
		au_uwrite(au_to_process(uid, gid, ruid, rgid, pid,
					auid, asid, (au_termid_t *)&atid));
		break;
	case AUE_DOORFS_DOOR_RETURN:
		/*
		 * We may want to write information about
		 * all doors (if any) which will be copied
		 * by this call to the user space
		*/
		break;
	case AUE_DOORFS_DOOR_CREATE:
		au_uwrite(au_to_arg32(3, "door attr", (uint32_t)uap->a3));
		break;
	case AUE_DOORFS_DOOR_REVOKE:
		au_uwrite(au_to_arg32(1, "door ID", (uint32_t)did));
		break;
	case AUE_DOORFS_DOOR_INFO:
		break;
	case AUE_DOORFS_DOOR_CRED:
		break;
	case AUE_DOORFS_DOOR_BIND:
		break;
	case AUE_DOORFS_DOOR_UNBIND: {
		break;
	}
	default:	/* illegal system call */
		break;
	}
}

/*ARGSUSED*/
static au_event_t
aui_acl(au_event_t e)
{
	struct a {
		union {
			long	name;	/* char */
			long	fd;
		}		obj;

		long		cmd;
		long		nentries;
		long		arg;	/* aclent_t */
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	switch (uap->cmd) {
	case SETACL:
		/* ok, acl(SETACL, ...) and facl(SETACL, ...)  are expected. */
		break;
	case GETACL:
	case GETACLCNT:
		/* do nothing for these two values. */
		e = AUE_NULL;
		break;
	default:
		/* illegal system call */
		break;
	}

	return (e);
}


/*ARGSUSED*/
static void
aus_acl(struct t_audit_data *tad)
{
	struct a {
		long	fname;
		long	cmd;
		long	nentries;
		long	aclbufp;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;
	struct acl *aclbufp;

	au_uwrite(au_to_arg32(2, "cmd", (uint32_t)uap->cmd));
	au_uwrite(au_to_arg32(3, "nentries", (uint32_t)uap->nentries));

	switch (uap->cmd) {
	case GETACL:
	case GETACLCNT:
		break;
	case SETACL:
	    if (uap->nentries < 3)
		break;
	    else {
		size_t a_size = uap->nentries * sizeof (struct acl);
		int i;

		aclbufp = (struct acl *)kmem_alloc(a_size, KM_SLEEP);
		if (copyin((caddr_t)(uap->aclbufp), aclbufp, a_size)) {
			kmem_free(aclbufp, a_size);
			break;
		}
		for (i = 0; i < uap->nentries; i++) {
			au_uwrite(au_to_acl(aclbufp + i));
		}
		kmem_free(aclbufp, a_size);
		break;
	}
	default:
		break;
	}
}

/*ARGSUSED*/
static void
aus_facl(struct t_audit_data *tad)
{
	struct a {
		long	fd;
		long	cmd;
		long	nentries;
		long	aclbufp;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;
	struct file  *fp;
	struct vnode *vp;
	struct f_audit_data *fad;
	struct acl *aclbufp;
	int fd;

	au_uwrite(au_to_arg32(2, "cmd", (uint32_t)uap->cmd));
	au_uwrite(au_to_arg32(3, "nentries", (uint32_t)uap->nentries));

	fd = (int)uap->fd;

	if ((fp = getf(fd)) == NULL)
		return;

	/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", (uint32_t)fd));
	}

	vp = fp->f_vnode;
	audit_attributes(vp);

	/* decrement file descriptor reference count */
	releasef(fd);

	switch (uap->cmd) {
	case GETACL:
	case GETACLCNT:
		break;
	case SETACL:
	    if (uap->nentries < 3)
		break;
	    else {
		size_t a_size = uap->nentries * sizeof (struct acl);
		int i;

		aclbufp = (struct acl *)kmem_alloc(a_size, KM_SLEEP);
		if (copyin((caddr_t)(uap->aclbufp), aclbufp, a_size)) {
			kmem_free(aclbufp, a_size);
			break;
		}
		for (i = 0; i < uap->nentries; i++) {
			au_uwrite(au_to_acl(aclbufp + i));
		}
		kmem_free(aclbufp, a_size);
		break;
	}
	default:
		break;
	}
}

/*ARGSUSED*/
static void
auf_read(tad, error, rval)
	struct t_audit_data *tad;
	int error;
	rval_t *rval;
{
	struct file *fp;
	struct f_audit_data *fad;
	int fd;
	register struct a {
		long	fd;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	fd = (int)uap->fd;

	/*
	 * convert file pointer to file descriptor
	 *   Note: fd ref count incremented here.
	 */
	if ((fp = getf(fd)) == NULL)
		return;

	/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	ASSERT(fad);

	/*
	 * already processed this file for read attempt
	 * XXX do we need locks here ? Two threads reading from same file
	 * at same time is probably broken applications.
	 *
	 * might be better to turn off auditing in a aui_read() routine.
	 */
	if (fad->fad_flags & FAD_READ) {
		/* don't really want to audit every read attempt */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		releasef(fd);
		return;
	}
	/* mark things so we know what happened and don't repeat things */
	/* XXX do we need locks here ? */
	fad->fad_flags |= FAD_READ;

	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", (uint32_t)fd));
	}

	/* include attributes */
	audit_attributes(fp->f_vnode);

	/* decrement file descriptor reference count */
	releasef(fd);
}

/*ARGSUSED*/
static void
auf_write(tad, error, rval)
	struct t_audit_data *tad;
	int error;
	rval_t *rval;
{
	struct file *fp;
	struct f_audit_data *fad;
	int fd;
	register struct a {
		long	fd;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	fd = (int)uap->fd;

	/*
	 * convert file pointer to file descriptor
	 *   Note: fd ref count incremented here.
	 */
	if ((fp = getf(fd)) == NULL)
		return;

	/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	ASSERT(fad);

	/*
	 * already processed this file for write attempt
	 * XXX do we need locks here ? Two threads writing to same file
	 * at same time is probably broken applications.
	 *
	 * might be better to turn off auditing in a aus_write() routine.
	 */
	if (fad->fad_flags & FAD_WRITE) {
		/* don't really want to audit every write attempt */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		releasef(fd);
		return;
	}
	/* mark things so we know what happened and don't repeat things */
	/* XXX do we need locks here ? */
	fad->fad_flags |= FAD_WRITE;

	if (fad->fad_lpbuf) {
		au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
	} else {
		au_uwrite(au_to_arg32(1, "no path: fd", (uint32_t)fd));
	}

	/* include attributes */
	audit_attributes(fp->f_vnode);

	/* decrement file descriptor reference count */
	releasef(fd);
}

/*ARGSUSED*/
static void
auf_recv(tad, error, rval)
	struct t_audit_data *tad;
	int error;
	rval_t *rval;
{
	struct sonode *so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	struct file *fp;
	struct f_audit_data *fad;
	int fd;
	int err;
	int len;
	short so_family, so_type;
	register struct a {
		long	fd;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	/*
	 * If there was an error, then nothing to do. Only generate
	 * audit record on first successful recv.
	 */
	if (error) {
		/* Turn off audit record generation here. */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		return;
	}

	fd = (int)uap->fd;

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/* Turn off audit record generation here. */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		return;
	}

	/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	ASSERT(fad);

	/*
	 * already processed this file for read attempt
	 * XXX do we need locks here ? Two threads reading from same file
	 * at same time is probably broken applications.
	 */
	if (fad->fad_flags & FAD_READ) {
		releasef(fd);
		/* don't really want to audit every recv call */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		return;
	}

	/* mark things so we know what happened and don't repeat things */
	/* XXX do we need locks here ? */
	fad->fad_flags |= FAD_READ;

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:
		/*
		 * Only for connections.
		 * XXX - do we need to worry about SOCK_DGRAM or other types???
		 */
		if (so->so_state & SS_ISBOUND) {

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			/* only done once on a connection */
			(void) sogetsockname(so);
			(void) sogetpeername(so);

			/* get local and foreign addresses */
			mutex_enter(&so->so_lock);
			len = min(so->so_laddr_len, sizeof (so_laddr));
			bcopy(so->so_laddr_sa, so_laddr, len);
			len = min(so->so_faddr_len, sizeof (so_faddr));
			bcopy(so->so_faddr_sa, so_faddr, len);
			mutex_exit(&so->so_lock);

			/*
			 * only way to drop out of switch. Note that we
			 * we release fd below.
			 */

			break;
		}

		releasef(fd);

		/* don't really want to audit every recv call */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);

		return;

	case AF_UNIX:

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		releasef(fd);

		return;

	default:
		releasef(fd);

		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		au_uwrite(au_to_arg32(1, "family", (uint32_t)so_family));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)so_type));

		return;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));

}

/*ARGSUSED*/
static void
auf_send(tad, error, rval)
	struct t_audit_data *tad;
	int error;
	rval_t *rval;
{
	struct sonode *so;
	char so_laddr[sizeof (struct sockaddr_in6)];
	char so_faddr[sizeof (struct sockaddr_in6)];
	struct file *fp;
	struct f_audit_data *fad;
	int fd;
	int err;
	int len;
	short so_family, so_type;
	register struct a {
		long	fd;
	} *uap = (struct a *)ttolwp(curthread)->lwp_ap;

	fd = (int)uap->fd;

	/*
	 * If there was an error, then nothing to do. Only generate
	 * audit record on first successful send.
	 */
	if (error != 0) {
		/* Turn off audit record generation here. */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		return;
	}

	fd = (int)uap->fd;

	if ((so = au_getsonode(fd, &err, &fp)) == NULL) {
		/* Turn off audit record generation here. */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		return;
	}

	/* get path from file struct here */
	fad = (struct f_audit_data *)F2A(fp);
	ASSERT(fad);

	/*
	 * already processed this file for write attempt
	 * XXX do we need locks here ? Two threads reading from same file
	 * at same time is probably broken applications.
	 */
	if (fad->fad_flags & FAD_WRITE) {
		releasef(fd);
		/* don't really want to audit every send call */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);
		return;
	}

	/* mark things so we know what happened and don't repeat things */
	/* XXX do we need locks here ? */
	fad->fad_flags |= FAD_WRITE;

	so_family = so->so_family;
	so_type   = so->so_type;

	switch (so_family) {
	case AF_INET:
	case AF_INET6:
		/*
		 * Only for connections.
		 * XXX - do we need to worry about SOCK_DGRAM or other types???
		 */
		if (so->so_state & SS_ISBOUND) {

			bzero((void *)so_laddr, sizeof (so_laddr));
			bzero((void *)so_faddr, sizeof (so_faddr));

			/* only done once on a connection */
			(void) sogetsockname(so);
			(void) sogetpeername(so);

			/* get local and foreign addresses */
			mutex_enter(&so->so_lock);
			len = min(so->so_laddr_len, sizeof (so_laddr));
			bcopy(so->so_laddr_sa, so_laddr, len);
			len = min(so->so_faddr_len, sizeof (so_faddr));
			bcopy(so->so_faddr_sa, so_faddr, len);
			mutex_exit(&so->so_lock);

			/*
			 * only way to drop out of switch. Note that we
			 * we release fd below.
			 */

			break;
		}

		releasef(fd);
		/* don't really want to audit every send call */
		tad->tad_flag = 0;
		/* free any residual audit data */
		au_close(&(u_ad), 0, 0, 0);

		return;

	case AF_UNIX:

		if (fad->fad_lpbuf) {
			au_uwrite(au_to_path(fad->fad_path, fad->fad_pathlen));
		} else {
			au_uwrite(au_to_arg32(1, "no path: fd", fd));
		}

		audit_attributes(fp->f_vnode);

		releasef(fd);

		return;

	default:
		releasef(fd);

		au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));
		au_uwrite(au_to_arg32(1, "family", (uint32_t)so_family));
		au_uwrite(au_to_arg32(1, "type", (uint32_t)so_type));

		return;
	}

	releasef(fd);

	au_uwrite(au_to_arg32(1, "so", (uint32_t)fd));

	au_uwrite(au_to_socket_ex(so_family, so_type, so_laddr, so_faddr));
}

au_state_t audit_ets[MAX_KEVENTS];
int naevent = sizeof (audit_ets) / sizeof (au_state_t);
