/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ifndef	_TIMT_H
#define	_TIMT_H

#pragma ident	"@(#)timt.h	1.16	97/08/23 SMI"

/*
 * Threading and mutual exclusion declarations primitves used for MT
 *	operation of TLI/XTI code.
 *
 * Note: These primitves are designed to achieve the affect of avoiding
 *	 deadlock possibility by an interface operation being called from a
 *	 signal handler while holding a lock.
 * Note: These primitives rely on stated "sigprocmask()" semantics that
 *	 make it be a thread specific operations for MT applications (when
 *	 libthread is linked). This saves on syscall overhead.
 * Note: These primitves rely on C library magic stubs that make all lock
 *	 opreations as no-ops for non-MT applications.
 *
 *				Single threaded app.   | MT application.
 *                              --------------------   | ---------------
 *						       |
 * MUTEX_{UN}LOCK_THRMASK	Lock ops NOOPS	       | Real lock ops
 *				signal mask ops NOOP   | thread signal mask ops
 *						       |
 * MUTEX_{UN}LOCK_PROCMASK	Lock ops NOOPS	       | Real lock ops
 *				process signal mask ops| thread signal mask ops
 *						       |
 *
 */

#include <thread.h>
#include <synch.h>

#ifdef _REENTRANT

#define	MUTEX_LOCK_THRMASK(lock, oldmask) \
{	sigset_t newmask; \
	(void) _sigfillset(&newmask); \
	_thr_sigsetmask(SIG_SETMASK, &newmask, &(oldmask)); \
	_mutex_lock(lock); \
}

#define	MUTEX_UNLOCK_THRMASK(lock, oldmask) \
{	_mutex_unlock(lock); \
	_thr_sigsetmask(SIG_SETMASK, &(oldmask), 0); \
}
#define	MUTEX_LOCK_PROCMASK(lock, oldmask) \
{	sigset_t newmask; \
	(void) _sigfillset(&newmask); \
	(void) _sigprocmask(SIG_SETMASK, &newmask, &(oldmask)); \
	_mutex_lock(lock); \
}

#define	MUTEX_UNLOCK_PROCMASK(lock, oldmask) \
{	_mutex_unlock(lock); \
	(void) _sigprocmask(SIG_SETMASK, &(oldmask), 0); \
}

#else /* _REENTRANT */

#define	MUTEX_LOCK_THRMASK(lock, oldmask)
#define	MUTEX_UNLOCK_THRMASK(lock, oldmask)
#define	MUTEX_LOCK_PROCMASK(lock, oldmask) \
{	sigset_t newmask; \
	(void) _sigfillset(&newmask); \
	(void) _sigprocmask(SIG_SETMASK, &newmask, &(oldmask)); \
}

#define	MUTEX_UNLOCK_PROCMASK(lock, oldmask) \
	(void) _sigprocmask(SIG_SETMASK, &(oldmask), 0);

#endif /* _REENTRANT */

extern int	_fcntl(int, int, ...);
extern int	_ioctl(int, int, ...);
extern int	_mutex_lock(mutex_t *);
extern int	_mutex_unlock(mutex_t *);
extern int	_sigfillset(sigset_t *);
extern int	_sigprocmask(int, const sigset_t *, sigset_t *);
extern int	_thr_main(void);
extern int	_thr_sigsetmask(int, const sigset_t *, sigset_t *);

#endif	/* _TIMT_H */
