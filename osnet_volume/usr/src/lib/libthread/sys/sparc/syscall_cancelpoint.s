/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


#pragma ident	"@(#)syscall_cancelpoint.s	1.14	98/08/07 SMI"


	.file	"syscall_cancelpoint.s"

#include <sys/asm_linkage.h>

/*
 * This file lists all the libc(POSIX.1) and librt(POSIX.1b)
 * which have been declared as CANCELLATION POINTS in POSIX.1c.
 *
 * SYSCALL_CANCELPOINT() macro provides the required wrapper to
 * interpose any call defined in an library. It assumes followings:
 * 	1. libthread must be linked before that library
 *	2. `newname` function should have been defined.
 * For example, if a function foo() declared in libc and its a
 * system call then define bar symbol also.
 * Then insert here:
 * 			SYSCALL_CANCELPOINT(foo, bar)
 *
 * This will interpose foo symbol here and
 * wrapper, after creating cancellation point, will call bar.
 * In many cases, bar may be _foo which is a private interface.
 */


/* C library -- read						*/
/* int read (int fildes, void *buf, unsigned nbyte);		*/

/* C library -- close						*/
/* int close (int fildes);					*/

/* C library -- open						*/
/* int open (const char *path, int oflag, [ mode_t mode ]);	*/
/* int open64 (const char *path, int oflag, [ mode_t mode ]);	*/

/* C library -- write						*/
/* int write (int fildes, void *buf, unsigned nbyte);		*/

/* C library -- fcntl (if cnd = F_SETLKW)			*/
/* int fcntl (int fildes, int cmd [, arg]);			*/

/* C library -- pause						*/
/* int pause (void);						*/

/* C library -- sigsuspend					*/
/* int sigsuspend (sigset_t *set);				*/

/* C library -- wait						*/
/* int wait (int *stat_loc);					*/

/* C library -- creat						*/
/* int creat (char *path, mode_t mode);				*/
/* int creat64 (char *path, mode_t mode);				*/

/* C library -- fsync						*/
/* int fsync (int fildes, void *buf, unsigned nbyte);		*/

/* C library -- sleep						*/
/* int sleep (unsigned sleep_tm);				*/
/* ALREADY A CANCELLATION POINT: IT CALLS siggsuspend()		*/

/* C library -- msync						*/
/* int msync (caddr_t addr, size_t  len, int flags);		*/

/* C library -- tcdrain						*/
/* int tcdrain (int fildes);					*/

/* C library -- waitpid						*/
/* int waitpid (pid_t pid, int *stat_loc, int options);		*/

/* C library -- system						*/
/* int system (const char *s);					*/
/* ALREADY A CANCELLATION POINT: IT CALLS waipid()		*/

/* POSIX.4 functions are defined as cancellation points in librt */

/* POSIX.4 library -- sigtimedwait				*/
/* int sigtimedwait (const sigset_t *set, siginfo_t *info, 	*/
/*			const struct timespec *timeout);	*/

/* POSIX.4 library -- sigtimeinfo				*/
/* int sigtwaitinfo (const sigset_t *set, siginfo_t *info); 	*/

/* POSIX.4 library -- nanosleep					*/
/* int nanosleep (const struct timespec *rqtp, struct timespec *rqtp); */

/* POSIX.4 library -- sem_wait					*/
/* int sem_wait (sem_t *sp);					*/

/* POSIX.4 library -- mq_receive				*/
/* int mq_receive ();						*/

/* POSIX.4 library -- mq_send					*/
/* int mq_send ();						*/


#ifndef __sparcv9
#include "sparc/SYS_CANCEL.h"
#else
#include "sparcv9/SYS_CANCEL.h"
#endif /* __sparcv9 */

	PRAGMA_WEAK(_ti_read, read)
	SYSCALL_CANCELPOINT(read, _read)

	PRAGMA_WEAK(_ti_close, close)
	SYSCALL_CANCELPOINT(close, _close)

	PRAGMA_WEAK(_ti_open, open)
	SYSCALL_CANCELPOINT(open, _open)

#ifndef __sparcv9
	PRAGMA_WEAK(_ti_open64, open64)
	SYSCALL_CANCELPOINT(open64, _open64)
#else /* __sparcv9 */
	PRAGMA_WEAK(_ti_open64, open)
	SYSCALL_CANCELPOINT(open64, _open)
#endif /* __sparcv9 */

	PRAGMA_WEAK(_ti_write, write)
	SYSCALL_CANCELPOINT(write, _write)

	SYSCALL_CANCELPOINT(_fcntl_cancel, _fcntl)

	PRAGMA_WEAK(_ti_pause, pause)
	SYSCALL_CANCELPOINT(pause, _pause)

	PRAGMA_WEAK(_ti_wait, wait)
	SYSCALL_CANCELPOINT(wait, _wait)

	PRAGMA_WEAK(_ti_creat, creat)
	SYSCALL_CANCELPOINT(creat, _creat)

#ifndef __sparcv9
	PRAGMA_WEAK(_ti_creat64, creat64)
	SYSCALL_CANCELPOINT(creat64, _creat64)
#else /* __sparcv9 */
	PRAGMA_WEAK(_ti_creat64, creat)
	SYSCALL_CANCELPOINT(creat64, _creat)
#endif /* __sparcv9 */

	PRAGMA_WEAK(_ti_fsync, fsync)
	SYSCALL_CANCELPOINT(fsync, _fsync)

	PRAGMA_WEAK(_ti_msync, msync)
	SYSCALL_CANCELPOINT(msync, _msync)

	PRAGMA_WEAK(_ti_tcdrain, tcdrain)
	SYSCALL_CANCELPOINT(tcdrain, _tcdrain)

	PRAGMA_WEAK(_ti_waitpid, waitpid)
	SYSCALL_CANCELPOINT(waitpid, _waitpid)

	PRAGMA_WEAK(_ti__nanosleep, __nanosleep)
	SYSCALL_CANCELPOINT(__nanosleep, _libc_nanosleep)

/* UNIX98 */
/* C library -- getmsg							*/
/* int getmsg(int , struct strbuf *, struct strbuf *, int *);		*/
	PRAGMA_WEAK(_ti_getmsg, getmsg)
	SYSCALL_CANCELPOINT(getmsg, _getmsg)

/* C library -- getpmsg							*/
/* int getpmsg(int , struct strbuf *, struct strbuf *, int *, int *);	*/
	PRAGMA_WEAK(_ti_getpmsg, getpmsg)
	SYSCALL_CANCELPOINT(getpmsg, _getpmsg)

/* C library -- putmsg							*/
/* int putmsg(int , const struct strbuf *, const struct strbuf *, int);	*/
	PRAGMA_WEAK(_ti_putmsg, putmsg)
	SYSCALL_CANCELPOINT(putmsg, _putmsg)

/* C library -- putpmsg							 */
/* int putpmsg(int,const struct strbuf *,const struct strbuf *,int,int); */
	PRAGMA_WEAK(_ti_putpmsg, putpmsg)
	SYSCALL_CANCELPOINT(putpmsg, _putpmsg)

/* C library -- __xpg4_putmsg						*/
/* int putmsg(int , const struct strbuf *, const struct strbuf *, int);	*/
	PRAGMA_WEAK(_ti_xpg4_putmsg, __xpg4_putmsg)
	SYSCALL_CANCELPOINT(__xpg4_putmsg, ___xpg4_putmsg)

/* C library -- ___xpg4_putpmsg						 */
/* int putpmsg(int,const struct strbuf *,const struct strbuf *,int,int); */
	PRAGMA_WEAK(_ti_xpg4_putpmsg, __xpg4_putpmsg)
	SYSCALL_CANCELPOINT(__xpg4_putpmsg, ___xpg4_putpmsg)

/* C library -- lockf							*/
/* int lockf(int fildes, int function, off_t size);			*/
	PRAGMA_WEAK(_ti_lockf, lockf)
	SYSCALL_CANCELPOINT(lockf, _lockf)

#ifndef __sparcv9
	PRAGMA_WEAK(_ti_lockf64, lockf64)
	SYSCALL_CANCELPOINT(lockf64, _lockf64)
#else /* __sparcv9 */
	PRAGMA_WEAK(_ti_lockf64, lockf)
	SYSCALL_CANCELPOINT(lockf64, _lockf)
#endif /* __sparcv9 */

/* C library -- msgrcv							*/
/* int msgrcv(int , void *, size_t , long , int );			*/
	PRAGMA_WEAK(_ti_msgrcv, msgrcv)
	SYSCALL_CANCELPOINT(msgrcv, _msgrcv)

/* C library -- msgsnd							*/
/* int msgsnd(int , const void *, size_t , int );			*/
	PRAGMA_WEAK(_ti_msgsnd, msgsnd)
	SYSCALL_CANCELPOINT(msgsnd, _msgsnd)

/* C library -- poll							*/
/* int poll(struct pollfd fds[], nfds_t nfds, int timeout);		*/
	PRAGMA_WEAK(_ti_poll, poll)
	SYSCALL_CANCELPOINT(poll, _poll)

/* C library -- pread							*/
/* ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);	*/
	PRAGMA_WEAK(_ti_pread, pread)
	SYSCALL_CANCELPOINT(pread, _pread)

#ifndef __sparcv9
	PRAGMA_WEAK(_ti_pread64, pread64)
	SYSCALL_CANCELPOINT(pread64, _pread64)
#else /* __sparcv9 */
	PRAGMA_WEAK(_ti_pread64, pread)
	SYSCALL_CANCELPOINT(pread64, _pread)
#endif /* __sparcv9 */

/* C library -- readv							*/
/* ssize_t readv(int fildes, const struct iovec *iov, int iovcnt);	*/
	PRAGMA_WEAK(_ti_readv, readv)
	SYSCALL_CANCELPOINT(readv, _readv)

/* C library -- pwrite							*/
/* ssize_t pwrite(int ,const void  *,size_t ,off_t);			*/
	PRAGMA_WEAK(_ti_pwrite, pwrite)
	SYSCALL_CANCELPOINT(pwrite, _pwrite)

#ifndef __sparcv9
	PRAGMA_WEAK(_ti_pwrite64, pwrite64)
	SYSCALL_CANCELPOINT(pwrite64, _pwrite64)
#else /* __sparcv9 */
	PRAGMA_WEAK(_ti_pwrite64, pwrite)
	SYSCALL_CANCELPOINT(pwrite64, _pwrite)
#endif /* __sparcv9 */

/* C library -- writev							*/
/* ssize_t writev(int fildes, const struct iovec *iov, int iovcnt);	*/
	PRAGMA_WEAK(_ti_writev, writev)
	SYSCALL_CANCELPOINT(writev, _writev)

/* C library -- select							*/
/* int select(int , fd_set *, fd_set *, fd_set *, struct timeval *);	*/
	PRAGMA_WEAK(_ti_select, select)
	SYSCALL_CANCELPOINT(select, _select)

/* C library -- sigpause						*/
/* int sigpause(int sig);						*/
	PRAGMA_WEAK(_ti_sigpause, sigpause)
	SYSCALL_CANCELPOINT(sigpause, _sigpause)

/* C library -- usleep							*/
/* int usleep(useconds_t useconds);					*/
	PRAGMA_WEAK(_ti_usleep, usleep)
	SYSCALL_CANCELPOINT(usleep, _usleep)

/* C library -- wait3							*/
/* pid_t wait3(int *statusp, int options, struct rusage *rusage);	*/
	PRAGMA_WEAK(_ti_wait3, wait3)
	SYSCALL_CANCELPOINT(wait3, _wait3)

/* C library -- waitid							*/
/* int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);	*/
	PRAGMA_WEAK(_ti_waitid, waitid)
	SYSCALL_CANCELPOINT(waitid, _waitid)

/*
 * End of syscall file
 */
