/*	Copyright (c) 1999 by Sun Microsystems, Inc.		*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


.ident	"@(#)syscall_cancelpoint.s 1.13	99/06/16 SMI" /* SVr4.0 1.9	*/


	.file	"syscall_cancelpoint.s"

#include <sys/asm_linkage.h>

/*
 * This file lists all the libc(POSIX.1) and librt(POSIX.1b)
 * which have been declared as CANCELLATION POINTS in POSIX.1c.
 *
 * SYSCALL_CANCELPOINT_n() macro provides the required wrapper to
 * interpose any call defined in an library. It assumes followings:
 * 	1. libthread must ne linked before that library
 *	2. `newname` function should have been defined.
 * For example, if a function foo() declared in libc and its a
 * system call then define bar symbol also.
 * Then insert here:
 * 			SYSCALL_CANCELPOINT_n(foo, bar)
 *			where n is number of arguments foo takes
 *
 * Currently, n is 0 to 3. If you require n > 3, add another macro
 * in ../inc/i386/SYS_CANCEL.h for those many arguments.
 *
 * This will interpose foo symbol here and
 * wrapper after creating cancellation point will call bar.
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

/* C library -- fcntl (if cmd == F_SETLKW)			*/
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

/* POSIX.4 functions are defined as cancellation point in librt */

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


#include "i386/SYS_CANCEL.h"

	PRAGMA_WEAK(_ti_read, read)
	SYSCALL_CANCELPOINT_3(read, _read)

	PRAGMA_WEAK(_ti_close, close)
	SYSCALL_CANCELPOINT_1(close, _close)

	PRAGMA_WEAK(_ti_open, open)
	SYSCALL_CANCELPOINT_3(open, _open)

	PRAGMA_WEAK(_ti_open64, open64)
	SYSCALL_CANCELPOINT_3(open64, _open64)

	PRAGMA_WEAK(_ti_write, write)
	SYSCALL_CANCELPOINT_3(write, _write)

	SYSCALL_CANCELPOINT_3(_fcntl_cancel, _fcntl)

	PRAGMA_WEAK(_ti_pause, pause)
	SYSCALL_CANCELPOINT_0(pause, _pause)

	PRAGMA_WEAK(_ti_wait, wait)
	SYSCALL_CANCELPOINT_1(wait, _wait)

	PRAGMA_WEAK(_ti_creat, creat)
	SYSCALL_CANCELPOINT_2(creat, _creat)

	PRAGMA_WEAK(_ti_creat64, creat64)
	SYSCALL_CANCELPOINT_2(creat64, _creat64)

	PRAGMA_WEAK(_ti_fsync, fsync)
	SYSCALL_CANCELPOINT_1(fsync, _fsync)

	PRAGMA_WEAK(_ti_msync, msync)
	SYSCALL_CANCELPOINT_3(msync, _msync)

	PRAGMA_WEAK(_ti_tcdrain, tcdrain)
	SYSCALL_CANCELPOINT_1(tcdrain, _tcdrain)

	PRAGMA_WEAK(_ti_waitpid, waitpid)
	SYSCALL_CANCELPOINT_3(waitpid, _waitpid)

	PRAGMA_WEAK(_ti__nanosleep, __nanosleep)
	SYSCALL_CANCELPOINT_2(__nanosleep, _libc_nanosleep)

/* UNIX98 */
/* C library -- getmsg							*/
/* int getmsg(int , struct strbuf *, struct strbuf *, int *);		*/
	PRAGMA_WEAK(_ti_getmsg, getmsg)
	SYSCALL_CANCELPOINT_4(getmsg, _getmsg)

/* C library -- getpmsg							*/
/* int getpmsg(int , struct strbuf *, struct strbuf *, int *, int *);	*/
	PRAGMA_WEAK(_ti_getpmsg, getpmsg)
	SYSCALL_CANCELPOINT_5(getpmsg, _getpmsg)

/* C library -- putmsg							*/
/* int putmsg(int , const struct strbuf *, const struct strbuf *, int);	*/
	PRAGMA_WEAK(_ti_putmsg, putmsg)
	SYSCALL_CANCELPOINT_4(putmsg, _putmsg)

/* C library -- putpmsg							 */
/* int putpmsg(int,const struct strbuf *,const struct strbuf *,int,int); */
	PRAGMA_WEAK(_ti_putpmsg, putpmsg)
	SYSCALL_CANCELPOINT_5(putpmsg, _putpmsg)

/* C library -- __xpg4_putmsg						*/
/* int putmsg(int , const struct strbuf *, const struct strbuf *, int);	*/
	PRAGMA_WEAK(_ti_xpg4_putmsg, __xpg4_putmsg)
	SYSCALL_CANCELPOINT_4(__xpg4_putmsg, ___xpg4_putmsg)

/* C library -- putpmsg							 */
/* int putpmsg(int,const struct strbuf *,const struct strbuf *,int,int); */
	PRAGMA_WEAK(_ti_xpg4_putpmsg, __xpg4_putpmsg)
	SYSCALL_CANCELPOINT_5(__xpg4_putpmsg, ___xpg4_putpmsg)

/* C library -- lockf							*/
/* int lockf(int fildes, int function, off_t size);			*/
	PRAGMA_WEAK(_ti_lockf, lockf)
	SYSCALL_CANCELPOINT_3(lockf, _lockf)

	PRAGMA_WEAK(_ti_lockf64, lockf64)
	SYSCALL_CANCELPOINT_4(lockf64, _lockf64)

/* C library -- msgrcv							*/
/* int msgrcv(int , void *, size_t , long , int);			*/
	PRAGMA_WEAK(_ti_msgrcv, msgrcv)
	SYSCALL_CANCELPOINT_5(msgrcv, _msgrcv)

/* C library -- msgsnd							*/
/* int msgsnd(int , const void *, size_t , int);			*/
	PRAGMA_WEAK(_ti_msgsnd, msgsnd)
	SYSCALL_CANCELPOINT_4(msgsnd, _msgsnd)

/* C library -- poll							*/
/* int poll(struct pollfd fds[], nfds_t nfds, int timeout);		*/
	PRAGMA_WEAK(_ti_poll, poll)
	SYSCALL_CANCELPOINT_3(poll, _poll)

/* C library -- pread							*/
/* ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);	*/
	PRAGMA_WEAK(_ti_pread, pread)
	SYSCALL_CANCELPOINT_4(pread, _pread)

	PRAGMA_WEAK(_ti_pread64, pread64)
	SYSCALL_CANCELPOINT_5(pread64, _pread64)

/* C library -- readv							*/
/* ssize_t readv(int fildes, const struct iovec *iov, int iovcnt);	*/
	PRAGMA_WEAK(_ti_readv, readv)
	SYSCALL_CANCELPOINT_3(readv, _readv)

/* C library -- pwrite							*/
/* ssize_t pwrite(int ,const void  *,size_t ,off_t);			*/
	PRAGMA_WEAK(_ti_pwrite, pwrite)
	SYSCALL_CANCELPOINT_4(pwrite, _pwrite)

	PRAGMA_WEAK(_ti_pwrite64, pwrite64)
	SYSCALL_CANCELPOINT_5(pwrite64, _pwrite64)

/* C library -- writev							*/
/* ssize_t writev(int fildes, const struct iovec *iov, int iovcnt);	*/
	PRAGMA_WEAK(_ti_writev, writev)
	SYSCALL_CANCELPOINT_3(writev, _writev)

/* C library -- select							*/
/* int select(int , fd_set *, fd_set *, fd_set *, struct timeval *);	*/
	PRAGMA_WEAK(_ti_select, select)
	SYSCALL_CANCELPOINT_5(select, _select)

/* C library -- sigpause						*/
/* int sigpause(int sig);						*/
	PRAGMA_WEAK(_ti_sigpause, sigpause)
	SYSCALL_CANCELPOINT_1(sigpause, _sigpause)

/* C library -- usleep							*/
/* int usleep(useconds_t useconds);					*/
	PRAGMA_WEAK(_ti_usleep, usleep)
	SYSCALL_CANCELPOINT_1(usleep, _usleep)

/* C library -- wait3							*/
/* pid_t wait3(int *statusp, int options, struct rusage *rusage);	*/
	PRAGMA_WEAK(_ti_wait3, wait3)
	SYSCALL_CANCELPOINT_3(wait3, _wait3)

/* C library -- waitid							*/
/* int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);	*/
	PRAGMA_WEAK(_ti_waitid, waitid)
	SYSCALL_CANCELPOINT_4(waitid, _waitid)
/*
 * End of syscall file
 */
