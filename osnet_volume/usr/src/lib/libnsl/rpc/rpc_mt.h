/*
 * Copyright (c) 1986-1991,1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */


/*
 *	Contains the mt libraries include definitions
 */

#ifndef	_RPC_MT_H
#define	_RPC_MT_H

#pragma ident	"@(#)rpc_mt.h	1.19	99/11/18 SMI"

#ifdef _REENTRANT

#define	mutex_lock(m)			_mutex_lock(m)
#define	mutex_trylock(m)		_mutex_trylock(m)
#define	mutex_unlock(m)			_mutex_unlock(m)
#define	mutex_init(m, n, p)		_mutex_init(m, n, p)
#define	cond_signal(m)			_cond_signal(m)
#define	cond_wait(c, m)			_cond_wait(c, m)
#define	cond_init(m, n, p)		_cond_init(m, n, p)
#define	cond_broadcast(c)		_cond_broadcast(c)
#define	rwlock_init(m, n, p)		_rwlock_init(m, n, p)
#define	rw_wrlock(x)			_rw_wrlock(x)
#define	rw_rdlock(x)			_rw_rdlock(x)
#define	rw_unlock(x)			_rw_unlock(x)
#define	thr_self(void)			_thr_self(void)
#define	thr_exit(x)			_thr_exit(x)
#define	thr_keycreate(m, n)		_thr_keycreate(m, n)
#define	thr_setspecific(k, p)		_thr_setspecific(k, p)
#define	thr_getspecific(k, p)		_thr_getspecific(k, p)
#define	thr_sigsetmask(f, n, o)		_thr_sigsetmask(f, n, o)
#define	thr_create(s, t, r, a, f, n)	_thr_create(s, t, r, a, f, n)

#else

#define	rwlock_init(m)
#define	rw_wrlock(x)
#define	rw_rdlock(x)
#define	rw_unlock(x)
#define	_rwlock_init(m, n, p)
#define	_rw_wrlock(x)
#define	_rw_rdlock(x)
#define	_rw_unlock(x)
#define	mutex_lock(m)
#define	mutex_unlock(m)
#define	mutex_init(m, n, p)
#define	_mutex_lock(m)
#define	_mutex_unlock(m)
#define	_mutex_init(m, n, p)
#define	cond_signal(m)
#define	cond_wait(m)
#define	cond_init(m, n, p)
#define	_cond_signal(m)
#define	_cond_wait(m)
#define	_cond_init(m, n, p)
#define	cond_broadcast(c)
#define	_cond_broadcast(c)
#define	thr_self(void)
#define	_thr_self(void)
#define	thr_exit(x)
#define	_thr_exit(x)
#define	thr_keycreate(m, n)
#define	_thr_keycreate(m, n)
#define	thr_setspecific(k, p)
#define	_thr_setspecific(k, p)
#define	thr_getspecific(k, p)
#define	_thr_getspecific(k, p)
#define	thr_setsigmask(f, n, o)
#define	_thr_setsigmask(f, n, o)
#define	thr_create(s, t, r, a, f, n)
#define	_thr_create(s, t, r, a, f, n)

#endif /* _REENTRANT */

#define	MT_ASSERT_HELD(x)

#include <thread.h>
#include <synch.h>
#include <sys/types.h>
#include <rpc/rpc.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int _thr_main();

/*
 * declaration to private interfaces in rpc library
 */

extern int svc_npollfds;
extern int svc_npollfds_set;
extern int svc_pollfd_allocd;
extern rwlock_t svc_fd_lock;

/*
 * macros to handle pollfd array; ***** Note that the macro takes
 * address of the array ( &array[0] ) always not the address of an
 * element *****.
 */

#define	MASKVAL	(POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND)
#define	POLLFD_EXTEND		64
#define	POLLFD_SHRINK		(2 * POLLFD_EXTEND)
#define	POLLFD_SET(x, y)	{ \
					(y)[(x)].fd = (x); \
					(y)[(x)].events = MASKVAL; \
				}
#define	POLLFD_CLR(x, y)	{ \
					(y)[(x)].fd = -1; \
					(y)[(x)].events = 0; \
					(y)[(x)].revents = 0; \
				}
#define	POLLFD_ISSET(x, y)	((y)[(x)].fd >= 0)


extern int __rpc_use_pollfd_done;
extern int __rpc_rlim_max(void);

/* Following functions create and manipulates the dgfd lock object */

extern void *rpc_fd_init(void);
extern int rpc_fd_lock(const void *handle, int fd);
extern int rpc_fd_unlock(const void *handle, int fd);
extern int rpc_fd_destroy(const void *handle, int fd);
extern int rpc_fd_trylock(const void *handle, int fd);
#define	fd_releaselock(fd, mask, handle) {	\
	rpc_fd_unlock((handle), (fd)); 	\
	thr_sigsetmask(SIG_SETMASK, &(mask), (sigset_t *)NULL);	\
}

#define	RPC_FD_NOTIN_FDSET(x)	(!__rpc_use_pollfd_done && (x) >= FD_SETSIZE)
#define	FD_INCREMENT FD_SETSIZE

extern int _sigdelset();
/*
 * synchronouly generated signals like SIGBUS, SIGSEGV can not be masked
 */
#define	DELETE_UNMASKABLE_SIGNAL_FROM_SET(s)	{ \
				(void) _sigdelset((s), SIGSEGV); \
				(void) _sigdelset((s), SIGBUS); \
				(void) _sigdelset((s), SIGFPE); \
				(void) _sigdelset((s), SIGILL); \
				}
/*
 * External functions without prototypes.  This is somewhat crufty, but
 * there is no other header file for this directory.  One should probably
 * be created and this stuff moved there if there turns out to be no better
 * way to avoid the warnings.
 */
struct stat;

extern int	_rw_wrlock(rwlock_t *);
extern unsigned	_sleep(unsigned);
extern int	_mutex_lock(mutex_t *);
extern int	_mutex_unlock(mutex_t *);
extern int	_fstat(int, struct stat *);
extern int	_stat(const char *, struct stat *);
extern int	_fcntl(int, int, ...);
extern int	__getpublickey_cached(char *, char *, int *);
extern void	__getpublickey_flush(const char *);
extern int	__can_use_af(sa_family_t);
extern int	__rpc_raise_fd(int);

#ifdef	__cplusplus
}
#endif

#endif	/* !_RPC_MT_H */
