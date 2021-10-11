#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)sys.spec	1.9	99/07/12 SMI"
#
# lib/libc/spec/sys.spec

function	_lwp_cond_signal
include		<sys/lwp.h>
declaration	int _lwp_cond_signal(lwp_cond_t *cvp)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return != 0
end

function	_lwp_cond_broadcast
include		<sys/lwp.h>
declaration	int _lwp_cond_broadcast(lwp_cond_t *cvp)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return != 0
end

function	_lwp_cond_wait
include		<sys/lwp.h>
declaration	int _lwp_cond_wait(lwp_cond_t *cvp, lwp_mutex_t *mp)
version		SUNW_0.7
errno		EINVAL EFAULT EINTR ETIME
exception	$return != 0
end

function	_lwp_cond_timedwait
include		<sys/lwp.h>
declaration	int _lwp_cond_timedwait(lwp_cond_t *cvp, lwp_mutex_t *mp, \
			timestruc_t *abstime)
version		SUNW_0.7
errno		EINVAL EFAULT EINTR ETIME
exception	$return != 0
end

function	_lwp_create
include		<sys/lwp.h>
declaration	int _lwp_create(ucontext_t *contextp, uint_t flags, \
			lwpid_t *new_lwp)
version		SUNW_0.7
errno		EFAULT EAGAIN EINVAL
exception	$return != 0
end

function	_lwp_exit
include		<sys/lwp.h>
declaration	void _lwp_exit(void)
version		SUNW_0.7
end

function	_lwp_info
include		<sys/time.h>, <sys/lwp.h>
declaration	int _lwp_info(struct lwpinfo *buffer)
version		SUNW_0.7
errno		EFAULT
exception	$return != 0
end

function	_lwp_kill
include		<sys/lwp.h>, <signal.h>
declaration	int _lwp_kill(lwpid_t target_lwp, int sig)
version		SUNW_0.7
errno		EINVAL ESRCH
exception	$return != 0
end

function	_lwp_makecontext
include		<sys/types.h>, <sys/lwp.h>, <ucontext.h>
declaration	void _lwp_makecontext(ucontext_t *ucp, \
			void (*start_routine)(void *), void *arg, \
			void *private, caddr_t stack_base, size_t stack_size)
version		SUNW_0.7
end

function	_lwp_mutex_lock
include		<sys/lwp.h>
declaration	int _lwp_mutex_lock(lwp_mutex_t *mp)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return != 0
end

function	_lwp_mutex_trylock
include		<sys/lwp.h>
declaration	int _lwp_mutex_trylock(lwp_mutex_t *mp)
version		SUNW_0.7
errno		EINVAL EFAULT EBUSY
exception	$return != 0
end

function	_lwp_mutex_unlock
include		<sys/lwp.h>
declaration	int _lwp_mutex_unlock(lwp_mutex_t *mp)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return != 0
end

function	_lwp_self
include		<sys/lwp.h>
declaration	lwpid_t _lwp_self(void)
version		SUNW_0.7
end

function	_lwp_sema_wait
include		<sys/lwp.h>
declaration	int _lwp_sema_wait(lwp_sema_t *sema)
version		SUNW_0.7
errno		EINVAL EFAULT EINTR
exception	$return != 0
end

function	_lwp_sema_init
include		<sys/lwp.h>
declaration	int _lwp_sema_init(lwp_sema_t *sema, int count)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return != 0
end

function	_lwp_sema_post
include		<sys/lwp.h>
declaration	int _lwp_sema_post(lwp_sema_t *sema)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return != 0
end

function	_lwp_setprivate
include		<sys/lwp.h>
declaration	void _lwp_setprivate(void *buffer)
version		SUNW_0.7
end

function	_lwp_getprivate
include		<sys/lwp.h>
declaration	void *_lwp_getprivate(void)
version		SUNW_0.7
exception	$return == NULL
end

function	_lwp_suspend
include		<sys/lwp.h>
declaration	int _lwp_suspend(lwpid_t target_lwp)
version		SUNW_0.7
errno		ESRCH EDEADLK
exception	$return != 0
end

function	_lwp_suspend2
include		<sys/lwp.h>
declaration	int _lwp_suspend2(lwpid_t target_lwp, int *arg)
version		SUNW_1.1
end

function	_lwp_continue
include		<sys/lwp.h>
declaration	int _lwp_continue(lwpid_t target_lwp)
version		SUNW_0.7
errno		ESRCH
exception	$return != 0
end

function	_lwp_wait
include		<sys/lwp.h>
declaration	int _lwp_wait(lwpid_t wait_for, lwpid_t *departed_lwp)
version		SUNW_0.7
errno		EINTR EDEADLK ESRCH
exception	$return != 0
end

function	_signotifywait
include		<sys/lwp.h>
declaration	int _signotifywait(void)
version		SUNWprivate_1.1
end

function	_lwp_sigredirect
include		<sys/lwp.h>
declaration	int _lwp_sigredirect(lwpid_t target_lwp, int signo)
version		SUNWprivate_1.1
errno		EINVAL ESRCH
exception	$return != 0
end

function	access
include		<unistd.h>
declaration	int access(const char *path, int amode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR ELOOP EMULTIHOP ENAMETOOLONG ENOENT \
			ENOLINK ENOTDIR EROFS EINVAL ETXTBSY
exception	$return == -1
end

function	_access
weak		access
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	acct
include		<unistd.h>
declaration	int acct(const char *path)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EBUSY EFAULT ELOOP ENAMETOOLONG ENOENT ENOTDIR \
			EPERM EROFS
exception	$return == -1
end

function	_acct
weak		acct
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	acl
include		<sys/acl.h>
declaration	int acl(const char *pathp, int cmd, int nentries, \
			aclent_t *aclbufp)
version		SUNW_0.9
errno		EACCES EINVAL EIO EPERM ENOENT ENOSPC ENOTDIR ENOSYS EROFS \
			EFAULT
exception	$return == -1
end

function	facl
include		<sys/acl.h>
declaration	int facl(int fildes, int cmd, int nentries, aclent_t *aclbufp)
version		SUNW_0.9
errno		EACCES EINVAL EIO EPERM ENOENT ENOSPC ENOTDIR ENOSYS \
			EROFS EFAULT
exception	$return == -1
end

function	adjtime
include		<sys/time.h>
declaration	int adjtime(struct timeval *delta, struct timeval *olddelta)
version		SUNW_0.7
errno		EFAULT EINVAL EPERM
exception	$return == -1
end

function	alarm
include		<unistd.h>
declaration	unsigned alarm(unsigned sec)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	ualarm
include		<unistd.h>
declaration	useconds_t ualarm(useconds_t useconds, useconds_t interval)
version		SUNW_0.9
end

function	brk
include		<unistd.h>
declaration	int brk(void *endds)
version		SUNW_0.7
errno		ENOMEM EAGAIN
exception	$return == -1
end

function	sbrk
include		<unistd.h>
declaration	void *sbrk(intptr_t incr)
version		sparc=SISCD_2.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		ENOMEM EAGAIN
exception	$return == (void *) -1
end

function	bsd_signal
include		<sys/signal.h>
declaration	void (*bsd_signal(int sig, void (*func)(int)))(int)
version		SUNW_1.1
errno		EINVAL EFAULT
exception	($return == SIG_ERR)
end

function	.stret1
version		sparc=SYSVABI_1.3
end

function	.stret2
version		sparc=SYSVABI_1.3
end

function	.stret4
version		sparc=SYSVABI_1.3
end

function	.stret8
version		sparc=SYSVABI_1.3
end

function	.udiv
version		sparc=SYSVABI_1.3
end

function	.umul
version		sparc=SYSVABI_1.3
end

function	.urem
version		sparc=SYSVABI_1.3
end

function	_Q_add
version		sparc=SYSVABI_1.3
end

function	_Q_cmp
version		sparc=SYSVABI_1.3
end

function	_Q_cmpe
version		sparc=SYSVABI_1.3
end

function	_Q_div
version		sparc=SYSVABI_1.3
end

function	_Q_dtoq
version		sparc=SYSVABI_1.3
end

function	_Q_feq
version		sparc=SYSVABI_1.3
end

function	_Q_fge
version		sparc=SYSVABI_1.3
end

function	_Q_fgt
version		sparc=SYSVABI_1.3
end

function	_Q_fle
version		sparc=SYSVABI_1.3
end

function	_Q_flt
version		sparc=SYSVABI_1.3
end

function	_Q_fne
version		sparc=SYSVABI_1.3
end

function	_Q_itoq
version		sparc=SYSVABI_1.3
end

function	_Q_mul
version		sparc=SYSVABI_1.3
end

function	_Q_neg
version		sparc=SYSVABI_1.3
end

function	_Q_qtod
version		sparc=SYSVABI_1.3
end

function	_Q_qtoi
version		sparc=SYSVABI_1.3
end

function	_Q_qtos
version		sparc=SYSVABI_1.3
end

function	_Q_qtou
version		sparc=SYSVABI_1.3
end

function	_Q_sqrt
version		sparc=SYSVABI_1.3
end

function	_Q_stoq
version		sparc=SYSVABI_1.3
end

function	_Q_sub
version		sparc=SYSVABI_1.3
end

function	_Q_utoq
version		sparc=SYSVABI_1.3
end

function	chdir
include		<unistd.h>
declaration	int chdir(const char *path)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EIO ELOOP ENAMETOOLONG ENOENT ENOLINK \
			ENOTDIR EMULTIHOP EBADF
exception	$return == -1
end

function	_chdir
weak		chdir
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fchdir
include		<unistd.h>
declaration	int fchdir(int fildes)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EIO ELOOP ENAMETOOLONG ENOENT ENOLINK \
			ENOTDIR EMULTIHOP EBADF
exception	$return == -1
end

function	_fchdir
weak		fchdir
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	chmod
include		<sys/types.h>, <sys/stat.h>
declaration	int chmod(const char *path, mode_t mode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EIO ELOOP EMULTIHOP ENAMETOOLONG ENOENT \
			ENOLINK ENOTDIR EPERM EROFS EBADF
exception	$return == -1
end

function	_chmod
weak		chmod
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fchmod
include		<sys/types.h>, <sys/stat.h>
declaration	int fchmod(int fildes, mode_t mode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EIO ELOOP EMULTIHOP ENAMETOOLONG ENOENT \
			ENOLINK ENOTDIR EPERM EROFS EBADF
exception	$return == -1
end

function	_fchmod
weak		fchmod
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	chown
include		<unistd.h>, <sys/types.h>
declaration	int chown(const char *path, uid_t owner, gid_t group)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EINVAL EIO ELOOP EMULTIHOP ENAMETOOLONG \
			ENOLINK ENOENT ENOTDIR EPERM EROFS EBADF
exception	$return == -1
end

function	_chown
weak		chown
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	lchown
include		<unistd.h>, <sys/types.h>
declaration	int lchown(const char *path, uid_t owner, gid_t group)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EINVAL EIO ELOOP EMULTIHOP ENAMETOOLONG \
			ENOLINK ENOENT ENOTDIR EPERM EROFS EBADF
exception	$return == -1
end

function	_lchown
weak		lchown
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fchown
include		<unistd.h>, <sys/types.h>
declaration	int fchown(int fildes, uid_t owner, gid_t group)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EINVAL EIO ELOOP EMULTIHOP ENAMETOOLONG \
			ENOLINK ENOENT ENOTDIR EPERM EROFS EBADF
exception	$return == -1
end

function	_fchown
weak		fchown
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	chroot
include		<unistd.h>
declaration	int chroot(const char *path)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EBADF EFAULT EINVAL EINTR EIO ELOOP EMULTIHOP \
			ENAMETOOLONG ENOENT ENOLINK ENOTDIR EPERM
exception	$return == -1
end

function	_chroot
weak		chroot
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fchroot
include		<unistd.h>
declaration	int fchroot(int fildes)
version		SUNW_0.7
errno		EACCES EBADF EFAULT EINVAL EINTR EIO ELOOP EMULTIHOP \
			ENAMETOOLONG ENOENT ENOLINK ENOTDIR EPERM
exception	$return == -1
end

function	close
include		<unistd.h>
declaration	int close(int fildes)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EINTR ENOLINK ENOSPC EIO
exception	$return == -1
end

function	_close
weak		close
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	cond_broadcast
version		SUNW_0.8
end

function	cond_destroy
version		SUNW_0.8
end

function	cond_init
version		SUNW_0.8
end

function	cond_signal
version		SUNW_0.8
end

function	cond_timedwait
version		SUNW_0.8
end

function	cond_wait
version		SUNW_0.8
end

function	creat
include		<sys/types.h>, <sys/stat.h>, <fcntl.h>
declaration	int creat(const char *path, mode_t mode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EAGAIN EDQUOT EFAULT EINTR EISDIR ELOOP EMFILE \
			EMULTIHOP ENAMETOOLONG ENFILE ENOENT ENOLINK \
			ENOSPC ENOTDIR EOVERFLOW EROFS
exception	$return == -1
end

function	_creat
weak		creat
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	dup
include		<unistd.h>
declaration	int dup(int fildes)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EINTR EMFILE ENOLINK
exception	$return == -1
end

function	_dup
weak		dup
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	exportfs
version		SUNW_0.7
end

function	fstatfs
version		SUNW_0.7
end

function	ladd
version		SUNW_0.7
end

function	ldivide
version		SUNW_0.7
end

function	lexp10
version		SUNW_0.7
end

function	llog10
version		SUNW_0.7
end

function	lmul
version		SUNW_0.7
end

function	logb
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	lshiftl
version		SUNW_0.7
end

function	lsub
version		SUNW_0.7
end

function	nss_delete
version		SUNW_0.7
end

function	nss_endent
version		SUNW_0.7
end

function	nss_getent
version		SUNW_0.7
end

function	nss_search
version		SUNW_0.7
end

function	nss_setent
version		SUNW_0.7
end

function	qecvt
version		SUNW_0.7
end

function	qfcvt
version		SUNW_0.7
end

function	qgcvt
version		SUNW_0.7
end

function	makeutx
version		SUNW_0.7
end

function	modctl
version		SUNW_0.7
end

function	modutx
version		SUNW_0.7
end

function	_modutx
weak		modutx
version		SUNWprivate_1.1
end		

function	nfs_getfh
version		SUNW_0.7
end

function	ntp_adjtime
include		<sys/timex.h>
declaration	int ntp_adjtime(struct timex *tptr)
version		SUNW_1.1
exception	$return == TIME_ERROR
end

function	_ntp_adjtime
weak		ntp_adjtime
version		SUNW_1.1
end

function	ntp_gettime
include		<sys/timex.h>
declaration	int ntp_gettime(struct ntptimeval *tptr)
version		SUNW_1.1
exception	$return == -1
end

function	_ntp_gettime
weak		ntp_gettime
version		SUNW_1.1
end

function	snprintf
version		SUNW_1.1
end

function	strtows
include		<widec.h>
declaration	wchar_t *strtows(wchar_t *dst, char *src)
version		SUNW_1.1
end

function	towctrans
version		SUNW_1.1
end

function	vsnprintf
version		SUNW_1.1
end

function	wctrans
version		SUNW_1.1
end

function	wstoll
include		<widec.h>, <wctype.h>
declaration	long long wstoll(const wchar_t *str, wchar_t **ptr, int base)
version		SUNW_1.1
end

function	wstostr
include		<widec.h>
declaration	char *wstostr(char *dst, wchar_t *src)
version		SUNW_1.1
end

function	___errno
version		sparc=SISCD_2.3 sparcv9=SUNW_0.7 i386=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	__builtin_alloca
version		SUNW_0.7
end

function	__div64
version		i386=SUNW_0.7 sparc=SISCD_2.3
end

function	__dtoll
version		SISCD_2.3
end

function	__dtou
version		sparc=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	__dtoull
version		SISCD_2.3
end

function	__filbuf
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	__flt_rounds
version		i386=SYSVABI_1.3 sparc=SUNW_1.1 sparcv9=SUNW_1.1
end

function	__ftoll
version		SISCD_2.3
end

function	__ftou
version		sparc=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	__ftoull
version		SISCD_2.3
end

function	__major
version		SUNW_0.7
end

function	__makedev
version		SUNW_0.7
end

function	__minor
version		SUNW_0.7
end

function	__mul64
version		i386=SUNW_0.7 sparc=SISCD_2.3
end

function	__nsw_extended_action
version		SUNW_0.7
end

function	__nsw_freeconfig
version		SUNW_0.7
end

function	__nsw_getconfig
version		SUNW_0.7
end

function	__posix_asctime_r
version		SUNW_1.1
end

function	__posix_ctime_r
version		SUNW_1.1
end

function	__posix_getgrgid_r
version		SUNW_1.1
end

function	__posix_getgrnam_r
version		SUNW_1.1
end

function	__posix_getlogin_r
version		SUNW_1.1
end

function	__posix_getpwnam_r
version		SUNW_1.1
end

function	__posix_getpwuid_r
version		SUNW_1.1
end

function	__posix_readdir_r
version		sparc=SUNW_1.1 i386=SUNW_1.1
end

function	__posix_sigwait
version		SUNW_1.1
end

function	__posix_ttyname_r
version		SUNW_1.1
end

function	__priocntl
version		SUNW_0.7
end

function	__priocntlset
version		SUNW_0.7
end

function	__rem64
version		i386=SUNW_0.7 sparc=SISCD_2.3
end

function	__udiv64
version		i386=SUNW_0.7 sparc=SISCD_2.3
end

function	__umul64
version		SISCD_2.3
end

function	__urem64
version		i386=SUNW_0.7 sparc=SISCD_2.3
end

function	__xpg4_putmsg
version		SUNW_1.1
end

function	__xpg4_putpmsg
version		SUNW_1.1
end

function	_alarm
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_assert
version		SUNW_0.7
end

function	__assert
weak		_assert
version		i386=SYSVABI_1.3 sparc=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_cleanup
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_exithandle
version		SUNW_1.1
end

function	_fork
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_getdate_err_addr
version		SUNW_0.8
end

function	_mutex_held
version		SUNW_0.7
end

function	_mutex_lock
version		SUNW_0.7
end

function	_nsc_trydoorcall
version		SUNW_0.9
end

function	_nss_XbyY_buf_alloc
version		SUNW_0.7
end

function	_nss_XbyY_buf_free
version		SUNW_0.7
end

function	_nss_netdb_aliases
version		SUNW_0.7
end

function	_rename
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_rw_read_held
version		SUNW_0.7
end

function	_rw_write_held
version		SUNW_0.7
end

function	_rwlock_destroy
version		SUNW_1.1
end

function	_sema_destroy
version		SUNW_1.1
end

function	_sema_held
version		SUNW_0.7
end

function	_setitimer
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_sigaction
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_siglongjmp
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_sigprocmask
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_sleep
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_xftw
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_xftw64
arch		sparc i386
version		SUNW_1.1
end

function	exit
include		<stdlib.h>, <unistd.h>
declaration	void exit(int status)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_exit
include		<stdlib.h>, <unistd.h>
declaration	void _exit(int status)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fcntl
include		<sys/types.h>, <unistd.h>, <fcntl.h>
declaration	int fcntl(int fildes, int cmd, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EFAULT EINTR EINVAL EMFILE ENOLCK ENOLINK \
			EOVERFLOW EDEADLK
exception	$return >= 0 /* OVERSIMPLIFIED */
end

function	_fcntl
weak		fcntl
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fork
include		<sys/types.h>, <unistd.h>
declaration	pid_t fork(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN ENOMEM
exception	$return == -1
end

function	fork1
include		<sys/types.h>, <unistd.h>
declaration	pid_t fork1(void)
version		SUNW_0.7
errno		EAGAIN ENOMEM
exception	$return == -1
end

function	fpathconf
include		<unistd.h>
declaration	long fpathconf(int fildes, int name)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EACCES ELOOP EMULTIHOP ENAMETOOLONG ENOENT ENOLINK \
			ENOTDIR EINVAL
exception	$return == -1
end

function	_fpathconf
weak		fpathconf
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	pathconf
include		<unistd.h>
declaration	long pathconf(const char *path, int name)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EACCES ELOOP EMULTIHOP ENAMETOOLONG ENOENT ENOLINK \
			ENOTDIR EINVAL
exception	$return == -1
end

function	_pathconf
weak		pathconf
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fsync
include		<unistd.h>
declaration	int fsync(int fildes)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EINTR EIO ETIMEDOUT
exception	$return == -1
end

function	_fsync
weak		_libc_fsync
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	getdents
include		<sys/types.h>, <sys/dirent.h>
declaration	int getdents(int fildes, struct dirent *buf, size_t nbyte)
version		SUNW_0.7
errno		EBADF EFAULT EINVAL EIO ENOENT ENOLINK ENOTDIR EOVERFLOW
exception	$return == -1
end

function	getgroups
include		<unistd.h>
declaration	int getgroups(int gidsetsize, gid_t *grouplist)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EPERM EFAULT
exception	$return == -1
end

function	_getgroups
weak		getgroups
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	setgroups
include		<unistd.h>
declaration	int setgroups(int ngroups, const gid_t *grouplist)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EPERM EFAULT
exception	$return == -1
end

function	_setgroups
weak		setgroups
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getitimer
include		<sys/time.h>
declaration	int getitimer(int which, struct itimerval *value)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EACCES
exception	$return == -1
end

function	_getitimer
weak		getitimer
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	setitimer
include		<sys/time.h>
declaration	int setitimer(int which, struct itimerval *value, struct itimerval *ovalue)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7
errno		EINVAL EACCES
exception	$return == -1
end

function	getmsg
include		<stropts.h>
declaration	int getmsg(int fildes, struct strbuf *ctlptr,\
			struct strbuf	*dataptr, int *flagsp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EBADMSG EFAULT EINTR EINVAL ENOSTR
exception	$return < 0
end

function	_getmsg
weak		getmsg
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getpmsg
include		<stropts.h>
declaration	int getpmsg(int fildes, struct strbuf *ctlptr, \
			struct strbuf	*dataptr, int *bandp, int *flagsp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EBADMSG EFAULT EINTR EINVAL ENOSTR
exception	$return < 0
end

function	_getpmsg
weak		getpmsg
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getpid
include		<unistd.h>
declaration	pid_t getpid(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EPERM ESRCH EINVAL
exception	$return == -1
end

function	_getpid
weak		getpid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getpgrp
include		<unistd.h>
declaration	pid_t getpgrp(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EPERM ESRCH EINVAL
exception	$return == -1
end

function	_getpgrp
weak		getpgrp
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getppid
include		<unistd.h>
declaration	pid_t getppid(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EPERM ESRCH EINVAL
exception	$return == -1
end

function	_getppid
weak		getppid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getpgid
include		<unistd.h>
declaration	pid_t getpgid(pid_t pid)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EPERM ESRCH EINVAL
exception	$return == -1
end

function	_getpgid
weak		getpgid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getrlimit
include		<sys/resource.h>
declaration	int getrlimit(int resource, struct rlimit *rlp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT EINVAL EPERM
exception	$return == -1
end

function	_getrlimit
weak		getrlimit
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	setrlimit
include		<sys/resource.h>
declaration	int setrlimit(int resource, const struct rlimit *rlp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT EINVAL EPERM
exception	$return == -1
end

function	_setrlimit
weak		setrlimit
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getsid
include		<unistd.h>
declaration	pid_t getsid(pid_t pid)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EPERM ESRCH
exception	$return == -1
end

function	_getsid
weak		getsid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getuid
include		<sys/types.h>, <unistd.h>
declaration	uid_t getuid(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_getuid
weak		getuid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	geteuid
include		<sys/types.h>, <unistd.h>
declaration	uid_t geteuid(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_geteuid
weak		geteuid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getgid
include		<sys/types.h>, <unistd.h>
declaration	gid_t getgid(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_getgid
weak		getgid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getegid
include		<sys/types.h>, <unistd.h>
declaration	gid_t getegid(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_getegid
weak		getegid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	ioctl
include		<unistd.h>, <stropts.h>
declaration	int ioctl(int fildes, int request, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EINTR EINVAL EFAULT EIO ENOLINK ENOTTY ENXIO ENODEV
exception	$return == -1
end

function	_ioctl
weak		ioctl
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	kill
include		<sys/types.h>, <signal.h>
declaration	int kill(pid_t pid, int sig)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EPERM ESRCH
exception	$return == -1
end

function	_kill
weak		kill
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	link
include		<unistd.h>
declaration	int link(const char *existing, const char *new)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EDQUOT EEXIST EFAULT EINTR ELOOP EMLINK EMULTIHOP \
			ENAMETOOLONG ENOENT ENOLINK ENOSPC ENOTDIR EPERM \
			EROFS EXDEV
exception	$return == -1
end

function	_link
weak		link
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	llseek
include		<sys/types.h>, <unistd.h>
declaration	offset_t llseek(int fildes, offset_t offset, int whence)
version		SUNW_0.7
errno		EBADF
exception	$return == -1
end

function	lockf
include		<unistd.h>
declaration	int lockf(int fildes, int function, off_t size)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EACCES EDEADLK EINTR ECOMM EINVAL EOVERFLOW EAGAIN \
			EOPNOTSUPP
exception	$return == -1
end

function	_lockf
weak		lockf
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	lseek
include		<sys/types.h>, <unistd.h>
declaration	off_t lseek(int fildes, off_t offset, int whence)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF
exception	$return == -1
end

function	_lseek
weak		lseek
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	memcntl
include		<sys/types.h>, <sys/mman.h>
declaration	int memcntl(caddr_t addr, size_t len, int cmd, \
			caddr_t arg, int attr, int mask)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBUSY EINVAL ENOMEM EPERM
exception	$return == -1
end

function	_memcntl
weak		memcntl
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	mincore
include		<sys/types.h>
declaration	int mincore(caddr_t addr, size_t len, char *vec)
version		SUNW_0.7
errno		EFAULT EINVAL ENOMEM
exception	$return == -1
end

function	mkdir
include		<sys/types.h>, <sys/stat.h>
declaration	int mkdir(const char *path, mode_t mode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EDQUOT EEXIST EFAULT EIO ELOOP EMLINK EMULTIHOP \
			ENAMETOOLONG ENOENT ENOLINK ENOSPC ENOTDIR EROFS
exception	$return == -1
end

function	_mkdir
weak		mkdir
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	mknod
include		<sys/stat.h>
declaration	int mknod(const char *path, mode_t mode, dev_t dev)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EDQUOT EEXIST EFAULT EINTR EINVAL EIO ELOOP EMULTIHOP \
			ENOENT ENOLINK ENOSPC ENOTDIR EPERM EROFS ENAMETOOLONG
exception	$return == -1
end

function	_mknod
weak		mknod
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	mmap
include		<sys/mman.h>
declaration	caddr_t mmap(caddr_t addr, size_t len, int prot, int flags, int fildes, \
			off_t off)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EAGAIN EBADF EINVAL EMFILE ENODEV ENOMEM ENXIO EOVERFLOW
exception	$return == MAP_FAILED
end

function	_mmap
weak		mmap
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	mount
include		<sys/types.h>, <sys/mount.h>
declaration	int mount(const char *spec, const char *dir, int mflag, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBUSY EFAULT EINVAL ELOOP EMULTIHOP ENAMETOOLONG ENOENT \
			ENOTBLK ENOTDIR EPERM EREMOTE ENOLINK ENXIO EROFS \
			ENOSPC
exception	$return == -1
end

function	_mount
weak		mount
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	mprotect
include		<sys/mman.h>
declaration	int mprotect(caddr_t addr, size_t len, int prot)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EINVAL ENOMEM EAGAIN
exception	$return == -1
end

function	_mprotect
weak		mprotect
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	msgctl
include		<sys/msg.h>
declaration	int msgctl(int msqid, int cmd, struct msqid_ds *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINVAL EPERM EOVERFLOW
exception	$return == -1
end

function	_msgctl
weak		msgctl
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	msgget
include		<sys/msg.h>
declaration	int msgget(key_t key, int msgflg)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EEXIST ENOENT ENOSPC
exception	$return == -1
end

function	_msgget
weak		msgget
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	msgrcv
include		<sys/msg.h>
declaration	ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, \
			long msgtyp, int msgflg)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EIDRM EINTR EINVAL ENOMSG
exception	$return == -1
end

function	_msgrcv
weak		msgrcv
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	msgsnd
include		<sys/msg.h>
declaration	int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EAGAIN EIDRM EINTR EINVAL
exception	$return == -1
end

function	_msgsnd
weak		msgsnd
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	munmap
include		<sys/mman.h>
declaration	int munmap(caddr_t addr, size_t len)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL
exception	$return == -1
end

function	_munmap
weak		munmap
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	nice
include		<unistd.h>
declaration	int nice(int incr)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EPERM
exception	errno != 0
end

function	_nice
weak		nice
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	open
include		<sys/types.h>, <sys/stat.h>, <fcntl.h>, <sys/file.h>
declaration	int open(const char *path, int oflag, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EDQUOT EEXIST EINTR EFAULT EIO EISDIR ELOOP EMFILE \
			EMULTIHOP ENFILE ENOENT ENOLINK ENOSR ENOSPC ENOTDIR \
			ENXIO EOPNOTSUPP EOVERFLOW EROFS EAGAIN EINVAL \
			ENAMETOOLONG ENOMEM ETXTBSY
exception	$return == -1
end

function	_open
weak		open
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	p_online
include		<sys/types.h>, <sys/processor.h>
declaration	int p_online(processorid_t	processorid, int flag)
version		SUNW_0.7
errno		EPERM EINVAL EBUSY
exception	$return == -1
end

function	pause
include		<unistd.h>
declaration	int pause(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_pause
weak		pause
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	pipe
include		<unistd.h>
declaration	int pipe(int fildes[2])
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EMFILE ENFILE
exception	$return == -1
end

function	_pipe
weak		pipe
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	poll
include		<poll.h>
declaration	int poll(struct pollfd *fds, nfds_t nfds, int timeout)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EFAULT EINTR EINVAL
exception	$return == -1
end

function	_poll
weak		poll
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	processor_bind
include		<sys/types.h>, <sys/processor.h>, <sys/procset.h>
declaration	int processor_bind(idtype_t idtype, id_t id, \
			processorid_t processorid, processorid_t *obind)
version		SUNW_0.7
errno		ESRCH EINVAL EFAULT EPERM
exception	$return == -1
end

function	processor_info
include		<sys/types.h>, <sys/processor.h>
declaration	int processor_info(processorid_t processorid, \
			processor_info_t *infop)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return == -1
end

function	profil
include		<unistd.h>
declaration	void profil(unsigned short *buff, size_t bufsiz, \
			unsigned long offset, unsigned int scale)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_profil
weak		profil
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	pset_bind
include		<sys/pset.h>
declaration	int pset_bind(psetid_t pset, idtype_t idtype, \
			id_t id, psetid_t *opset)
version		SUNW_1.1
errno		ESRCH EINVAL EFAULT EBUSY EPERM
exception	$return == -1
end

function	pset_create
include		<sys/pset.h>
declaration	int pset_create(psetid_t *newpset)
version		SUNW_1.1
errno		EBUSY EINVAL EFAULT ENOMEM EPERM
exception	$return == -1
end

function	pset_destroy
include		<sys/pset.h>
declaration	int pset_destroy(psetid_t pset)
version		SUNW_1.1
errno		EBUSY EINVAL EFAULT ENOMEM EPERM
exception	$return == -1
end

function	pset_assign
include		<sys/pset.h>
declaration	int pset_assign(psetid_t pset, processorid_t cpu, \
			psetid_t *opset)
version		SUNW_1.1
errno		EBUSY EINVAL EFAULT ENOMEM EPERM
exception	$return == -1
end

function	pset_info
include		<sys/pset.h>
declaration	int pset_info(psetid_t pset, int *type, \
			u_int	*numcpus, processorid_t *cpulist)
version		SUNW_1.1
errno		EINVAL EFAULT
exception	$return == -1
end

function	read
include		<unistd.h>, <sys/uio.h>, <limits.h>
declaration	ssize_t read(int fildes, void *buf, size_t nbyte)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EBADMSG EDEADLK EFAULT EINTR EINVAL EIO EISDIR \
			ENOLCK ENOLINK ENXIO EOVERFLOW ESPIPE
exception	$return == -1
end

function	_read
weak		read
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	pread
include		<unistd.h>, <sys/uio.h>, <limits.h>
declaration	ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset)
version		SUNW_0.7
errno		EAGAIN EBADF EBADMSG EDEADLK EFAULT EINTR EINVAL EIO EISDIR \
			ENOLCK ENOLINK ENXIO EOVERFLOW ESPIPE
exception	$return == -1
end

function	readv
include		<unistd.h>, <sys/uio.h>, <limits.h>
declaration	ssize_t readv(int fildes, const struct iovec *iov, int iovcnt)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EBADMSG EDEADLK EFAULT EINTR EINVAL EIO EISDIR \
			ENOLCK ENOLINK ENXIO EOVERFLOW ESPIPE
exception	$return == -1
end

function	_readv
weak		readv
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	readlink
include		<unistd.h>
declaration	int readlink(const char *path, char *buf, size_t bufsiz)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT EINVAL EIO ENOENT ELOOP ENOTDIR ENOSYS EACCES ENAMETOOLONG
exception	$return == -1
end

function	_readlink
weak		readlink
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	rmdir
include		<unistd.h>
declaration	int rmdir(const char *path)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EBUSY EEXIST EFAULT EINVAL EIO ELOOP EMULTIHOP \
			ENAMETOOLONG ENOENT ENOLINK ENOTDIR EROFS
exception	$return == -1
end

function	_rmdir
weak		rmdir
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	semctl
include		<sys/types.h>, <sys/ipc.h>, <sys/sem.h>
declaration	int semctl(int semid, int semnum, int cmd, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EINVAL EPERM EOVERFLOW ERANGE
exception	$return == -1
end

function	_semctl
weak		semctl
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	semget
include		<sys/types.h>, <sys/ipc.h>, <sys/sem.h>
declaration	int semget(key_t key, int nsems, int semflg)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EEXIST EINVAL ENOENT ENOSPC
exception	$return == -1
end

function	_semget
weak		semget
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	semop
include		<sys/types.h>, <sys/ipc.h>, <sys/sem.h>
declaration	int semop(int semid, struct sembuf *sops, size_t nsops)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EAGAIN EFAULT EFBIG EIDRM EINTR EINVAL ENOSPC ERANGE
exception	$return == -1
end

function	_semop
weak		semop
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	setpgrp
include		<sys/types.h>, <unistd.h>
declaration	pid_t setpgrp(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_setpgrp
weak		setpgrp
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	setsid
include		<sys/types.h>, <unistd.h>
declaration	pid_t setsid(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EPERM
exception	$return == -1
end

function	_setsid
weak		setsid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	setuid
include		<sys/types.h>, <unistd.h>, <limits.h>
declaration	int setuid(uid_t uid)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EPERM
exception	$return == -1
end

function	_setuid
weak		setuid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	setegid
include		<sys/types.h>, <unistd.h>, <limits.h>
declaration	int setegid(gid_t egid)
version		SUNW_0.7
errno		EINVAL EPERM
exception	$return == -1
end

function	seteuid
include		<sys/types.h>, <unistd.h>, <limits.h>
declaration	int seteuid(uid_t euid)
version		SUNW_0.7
errno		EINVAL EPERM
exception	$return == -1
end

function	setgid
include		<sys/types.h>, <unistd.h>, <limits.h>
declaration	int setgid(gid_t gid)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EPERM
exception	$return == -1
end

function	_setgid
weak		setgid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	shmctl
include		<sys/types.h>, <sys/ipc.h>, <sys/shm.h>
declaration	int shmctl(int shmid, int cmd, struct shmid_ds *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINVAL ENOMEM EOVERFLOW EPERM
exception	$return == -1
end

function	_shmctl
weak		shmctl
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	shmget
include		<sys/types.h>, <sys/ipc.h>, <sys/shm.h>
declaration	int shmget(key_t key, size_t size, int shmflg)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EEXIST EINVAL ENOENT ENOMEM ENOSPC
exception	$return == -1
end

function	_shmget
weak		shmget
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	shmat
include		<sys/types.h>, <sys/shm.h>
declaration	void *shmat(int shmid, const void *shmaddr, int shmflg)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EINVAL EMFILE ENOMEM
exception	$return == (void *) -1
end

function	_shmat
weak		shmat
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	shmdt
include		<sys/types.h>, <sys/shm.h>
declaration	int shmdt(char *shmaddr)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EINVAL EMFILE ENOMEM
exception	$return == -1
end

function	_shmdt
weak		shmdt
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	sigaction extends libaio/spec/aio.spec sigaction
include		<signal.h>
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	sigaltstack
include		<signal.h>
declaration	int sigaltstack(const stack_t *ss, stack_t *oss)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT EINVAL ENOMEM EPERM
exception	$return == -1
end

function	_sigaltstack
weak		sigaltstack
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	sigpause
include		<signal.h>
declaration	int sigpause(int mask)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EINTR
exception	$return == -1
end

function	_sigpause
weak		sigpause
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	sigfpe
include		<floatingpoint.h>, <siginfo.h>
declaration	sigfpe_handler_type sigfpe(sigfpe_code_type sigcode, \
			sigfpe_handler_type hdl)
version		SUNW_0.8
errno		
end

function	siginterrupt
declaration	int siginterrupt (int sig, int flag)
version		SUNW_1.1
errno		EINVAL
exception	$return == -1
end

function	sigpending
include		<signal.h>
declaration	int sigpending(sigset_t *set)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT
exception	$return == -1
end

function	_sigpending
weak		sigpending
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	sigprocmask
include		<signal.h>
declaration	int sigprocmask(int how, const sigset_t *set, sigset_t *oset)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT EINVAL
exception	$return == -1
end

function	sigsuspend
include		<signal.h>
declaration	int sigsuspend(const sigset_t *set)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT EINTR
exception	$return == -1
end

function	_sigsuspend
weak		sigsuspend
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	sigwait
include		<signal.h>
declaration	int sigwait(sigset_t *set)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return == -1
end

function	stat
include		<sys/types.h>, <sys/stat.h>
declaration	int stat(const char *path, struct stat *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW EACCES EFAULT EINTR ELOOP EMULTIHOP ENAMETOOLONG \
			ENOENT ENOLINK ENOTDIR EBADF
exception	$return == -1
end

function	_stat
weak		stat
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	lstat
include		<sys/types.h>, <sys/stat.h>
declaration	int lstat(const char *path, struct stat *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW EACCES EFAULT EINTR ELOOP EMULTIHOP ENAMETOOLONG \
			ENOENT ENOLINK ENOTDIR EBADF
exception	$return == -1
end

function	_lstat
weak		lstat
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fstat
include		<sys/types.h>, <sys/stat.h>
declaration	int fstat(int fildes, struct stat *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW EACCES EFAULT EINTR ELOOP EMULTIHOP ENAMETOOLONG \
			ENOENT ENOLINK ENOTDIR EBADF
exception	$return == -1
end

function	_fstat
weak		fstat
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	statvfs
include		<sys/types.h>, <sys/statvfs.h>
declaration	int statvfs(const char *path, struct statvfs *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW EACCES EFAULT EINTR EIO ELOOP EMULTIHOP ENAMETOOLONG \
			ENOENT ENOLINK ENOTDIR EBADF
exception	$return == -1
end

function	_statvfs
weak		statvfs
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fstatvfs
include		<sys/types.h>, <sys/statvfs.h>
declaration	int fstatvfs(int fildes, struct statvfs *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW EACCES EFAULT EINTR EIO ELOOP EMULTIHOP ENAMETOOLONG \
			ENOENT ENOLINK ENOTDIR EBADF
exception	$return == -1
end

function	_fstatvfs
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	stime
include		<unistd.h>
declaration	int stime(const time_t *tp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EPERM
exception	$return == -1
end

function	_stime
weak		stime
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	symlink
include		<unistd.h>
declaration	int symlink(const char *name1, const char *name2)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EDQUOT EEXIST EFAULT EIO ELOOP ENAMETOOLONG ENOENT \
			ENOSPC ENOSYS ENOTDIR EROFS
exception	$return == -1
end

function	_symlink
weak		symlink
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	sync
include		<unistd.h>
declaration	void sync(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_sync
weak		sync
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	syscall
declaration	int syscall(int number, void *arg, ...)
version		SUNW_0.7
exception	$return == -1
end

function	_syscall
weak		syscall
version		SUNW_0.7
end

function	sysfs
include		<sys/fstyp.h>, <sys/fsid.h>
declaration	int sysfs(int opcode, ...)
version		SUNW_0.7
errno		EFAULT EINVAL
exception	$return == -1
end

function	sysinfo
include		<sys/systeminfo.h>
declaration	int sysinfo(int command, char *buf, long count)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT EINVAL EPERM
exception	$return > count
end

function	_sysinfo
weak		sysinfo
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	time
include		<sys/types.h>, <time.h>
declaration	time_t time(time_t *tloc)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	_time
weak		time
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	times
include		<sys/times.h>, <limits.h>
declaration	clock_t times(struct tms *buffer)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT
exception	$return == -1 && errno != 0
end

function	_times
weak		times
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	uadmin
include		<sys/uadmin.h>
declaration	int uadmin(int cmd, int fcn, uintptr_t mdep)
version		SUNW_0.7
errno		EPERM ENOMEM ENOSPC ENOTSUP ENXIO EBUSY
exception	$return == -1
end

function	getcontext
include		<ucontext.h>
declaration	int getcontext(ucontext_t *ucp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	_getcontext
weak		getcontext
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	setcontext
include		<ucontext.h>
declaration	int setcontext(const ucontext_t *ucp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	_setcontext
weak		setcontext
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	ulimit
include		<ulimit.h>
declaration	long ulimit(int cmd, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINVAL EPERM
exception	$return == -1 && errno != 0
end

function	_ulimit
weak		ulimit
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	umask
include		<sys/types.h>, <sys/stat.h>
declaration	mode_t umask(mode_t cmask)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_umask
weak		umask
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	umount
include		<sys/mount.h>
declaration	int umount(const char *file)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBUSY EFAULT EINVAL ENOENT ELOOP EMULTIHOP ENAMETOOLONG \
			ENOLINK ENOTBLK EPERM EREMOTE
exception	$return == -1
end

function	_umount
weak		umount
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	umount2
include		<sys/mount.h>
declaration	int umount2(const char *file, int flag)
version		SUNW_1.19	
errno		EBUSY EFAULT EINVAL ENOENT ELOOP EMULTIHOP ENAMETOOLONG \
			ENOLINK ENOTBLK EPERM EREMOTE ENOTSUP
exception	$return == -1
end

function	_umount2
weak		umount2
version		SUNW_1.19	
end

function	uname
include		<sys/utsname.h>
declaration	int uname(struct utsname *name)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFAULT
exception	$return == -1
end

function	_uname
weak		uname
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	unlink
include		<unistd.h>
declaration	int unlink(const char *path)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EBUSY EFAULT EINTR ELOOP EMULTIHOP ENOENT ENOLINK \
			ENOTDIR EPERM EROFS ENAMETOOLONG ETXTBSY
exception	$return == -1
end

function	_unlink
weak		unlink
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	ustat
include		<sys/types.h>, <ustat.h>
declaration	int ustat(dev_t dev, struct ustat *buf)
version		SUNW_0.7
errno		ECOMM EFAULT EINTR EINVAL ENOLINK
exception	$return == -1
end

function	utime
include		<sys/types.h>, <utime.h>
declaration	int utime(const char *path, const struct utimbuf *times)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EFAULT EINTR EIO ELOOP EMULTIHOP ENAMETOOLONG ENOENT \
			ENOLINK ENOTDIR EPERM EROFS
exception	$return == -1
end

function	_utime
weak		utime
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	utimes
include		<sys/time.h>
declaration	int utimes(const char *path, const struct timeval times[2])
version		SUNW_0.7
errno		EACCES EFAULT EINTR EINVAL EIO ELOOP EMULTIHOP ENOLINK ENOENT \
			ENOTDIR EPERM EROFS ENAMETOOLONG
exception	$return == -1
end

function	vfork
include		<unistd.h>
declaration	pid_t vfork(void)
version		SUNW_0.7
errno		EAGAIN ENOMEM
exception	$return == -1
end

function	vhangup
declaration	void vhangup(void)
version		SUNW_0.7
end

function	wait
include		<sys/types.h>, <sys/wait.h>
declaration	pid_t wait(int *stat_loc)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		ECHILD EINTR
exception	$return == -1
end

function	_wait
weak		wait
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	waitid
include		<wait.h>
declaration	int waitid(idtype_t idtype, id_t id, siginfo_t *infop, \
			int options)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		ECHILD EFAULT EINTR EINVAL
exception	$return == -1
end

function	_waitid
weak		waitid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	write
include		<unistd.h>, <sys/uio.h>
declaration	ssize_t write(int fildes, const void *buf, size_t nbyte)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EDEADLK EDQUOT EFAULT EFBIG EINTR EIO ENOLCK \
			ENOLINK ENOSPC ENOSR EPIPE ERANGE ESPIPE EINVAL ENXIO
exception	$return == -1
end

function	_write
weak		write
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	pwrite
include		<unistd.h>, <sys/uio.h>
declaration	ssize_t pwrite(int fildes, const void *buf, size_t nbyte, \
			off_t offset)
version		SUNW_0.7
errno		EAGAIN EBADF EDEADLK EDQUOT EFAULT EFBIG EINTR EIO ENOLCK \
			ENOLINK ENOSPC ENOSR ENXIO EPIPE ERANGE ESPIPE EINVAL
exception	$return == -1
end

function	writev
include		<unistd.h>, <sys/uio.h>
declaration	ssize_t writev(int fildes, const struct iovec *iov, int iovcnt)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EDEADLK EDQUOT EFAULT EFBIG EINTR EIO ENOLCK \
			ENOLINK ENOSPC ENOSR ENXIO EPIPE ERANGE ESPIPE EINVAL
exception	$return == -1
end

function	_writev
weak		writev
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	yield
include		<unistd.h>
declaration	void yield(void)
version		SUNW_0.7
end

function	getexecname
declaration	const char *getexecname(void)
version		SUNW_1.1
end

function	_getexecname
weak		getexecname
version		SUNW_1.1
end

function	getloadavg
declaration	int getloadavg(double loadavg[], int nelem)
version		SUNW_1.18
errno		EINVAL
exception	$return == -1
end

function	tell
include		<sys/types.h>, <unistd.h>
declaration	off_t tell(int fd)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EOVERFLOW ESPIPE
end

function	_tell
weak		tell
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	environ
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	altzone
version		SUNW_0.7
end

function	daylight
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	timezone
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	tzname
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_ctype
version		SUNW_0.7
end

function	lzero
version		SUNW_0.7
end

function	lone
version		SUNW_0.7
end

function	lten
version		SUNW_0.7
end

function	_cladm
declaration	int _cladm(int fac, int cmd, void *arg)
version		SUNWprivate_1.1
end

function	__cladm
weak		_cladm
version		SUNWprivate_1.1
end

function	core_set_options
include		<sys/corectl.h>
declaration	int core_set_options(int)
version		SUNWprivate_1.1
end

function	_core_set_options
weak		core_set_options
version		SUNWprivate_1.1
end

function	core_get_options
include		<sys/corectl.h>
declaration	int core_get_options()
version		SUNWprivate_1.1
end

function	_core_get_options
weak		core_get_options
version		SUNWprivate_1.1
end

function	core_set_global_path
include		<sys/corectl.h>
declaration	int core_set_global_path(const char *, size_t)
version		SUNWprivate_1.1
end

function	_core_set_global_path
weak		core_set_global_path
version		SUNWprivate_1.1
end

function	core_get_global_path
include		<sys/corectl.h>
declaration	int core_get_global_path(char *, size_t)
version		SUNWprivate_1.1
end

function	_core_get_global_path
weak		core_get_global_path
version		SUNWprivate_1.1
end

function	core_set_process_path
include		<sys/corectl.h>
declaration	int core_set_process_path(const char *, size_t, pid_t)
version		SUNWprivate_1.1
end

function	_core_set_process_path
weak		core_set_process_path
version		SUNWprivate_1.1
end

function	core_get_process_path
include		<sys/corectl.h>
declaration	int core_get_process_path(char *, size_t, pid_t)
version		SUNWprivate_1.1
end

function	_core_get_process_path
weak		core_get_process_path
version		SUNWprivate_1.1
end
