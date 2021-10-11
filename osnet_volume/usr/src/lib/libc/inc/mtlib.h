/*
 * Copyright (c) 1992,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)mtlib.h 1.26	99/10/25 SMI"	/* SVr4.0 1.33  */

#ifndef	_MTLIB_H
#define	_MTLIB_H

#ifdef	_REENTRANT


#define	lock_try			_lock_try
#define	lock_clear			_lock_clear
#define	mutex_lock(m)			_mutex_lock(m)
#define	mutex_trylock(m)		_mutex_trylock(m)
#define	mutex_unlock(m)			_mutex_unlock(m)
#define	mutex_init(m, n, p)		_mutex_init(m, n, p)
#define	cond_wait(c, m)			_cond_wait(c, m)
#define	rw_wrlock(x)			_rw_wrlock(x)
#define	rw_rdlock(x)			_rw_rdlock(x)
#define	rw_unlock(x)			_rw_unlock(x)
#define	thr_main			_thr_main
#define	thr_self			_thr_self
#define	thr_exit(x)			_thr_exit(x)
#define	thr_min_stack			_thr_min_stack
#define	thr_kill			_thr_kill
#define	thr_keycreate			_thr_keycreate
#define	thr_setspecific			_thr_setspecific
#define	thr_getspecific			_thr_getspecific

#define	SIGMASK_MUTEX_LOCK(lk, m, oldmask) \
{	sigset_t newmask; \
	sigfillset(&newmask); \
	_thr_sigsetmask(SIG_SETMASK, &newmask, &(oldmask)); \
	FLOCKFILE(lk, m); \
}

#define	SIGMASK_MUTEX_UNLOCK(lk, m, oldmask) \
{	sigset_t tmpmask; \
	FUNLOCKFILE(lk); \
	_thr_sigsetmask(SIG_SETMASK, &(oldmask), &tmpmask); \
}

#define	_FWRITE _fwrite_unlocked
#define	FILENO(s) _fileno_unlocked(s)
#define	FEOF(s) _feof_unlocked(s)
#define	FERROR(s) _ferror_unlocked(s)
#define	CLEARERR(s) _clearerr_unlocked(s)
#define	GETC(s) _getc_unlocked(s)
#define	UNGETC(c, s) _ungetc_unlocked(c, s)
#define	PUTC(c, s) _putc_unlocked(c, s)
#define	GETWC(s) getwc(s)
#define	PUTWC(c, s) putwc(c, s)

#define	FILELOCKING(iop)	(GET_IONOLOCK(iop) == 0)

#define	FLOCKFILE(lk, iop) \
	{ \
		if (__threaded && FILELOCKING(iop)) \
			lk = _flockget((iop)); \
		else \
			lk = NULL; \
	}

#define	FUNLOCKFILE(lk) \
	{ \
		if (__threaded && (lk != NULL)) \
			_flockrel(lk); \
	}

#define	FLOCKRETURN(iop, ret) \
	{	int r; \
		rmutex_t *lk; \
		FLOCKFILE(lk, iop); \
		r = (ret); \
		FUNLOCKFILE(lk); \
		return (r); \
	}

#else

#define	lock_try
#define	_lock_try
#define	lock_clear
#define	_lock_clear
#define	rw_wrlock(x)
#define	rw_rdlock(x)
#define	rw_unlock(x)
#define	_rw_wrlock(x)
#define	_rw_rdlock(x)
#define	_rw_unlock(x)
#define	mutex_lock(m)
#define	mutex_unlock(m)
#define	mutex_init(m)
#define	_mutex_lock(m)
#define	_mutex_unlock(m)
#define	_mutex_init(m)
#define	thr_main
#define	_thr_main
#define	thr_min_stack
#define	_thr_min_stack
#define	thr_kill
#define	_thr_kill
#define	thr_keycreate
#define	_thr_keycreate
#define	thr_setspecific
#define	_thr_setspecific
#define	thr_getspecific
#define	_thr_getspecific

#define	SIGMASK_MUTEX_LOCK(m, old)
#define	SIGMASK_MUTEX_UNLOCK(m, old)
#define	FLOCKFILE(lk, iop)
#define	FUNLOCKFILE(lk)
#define	FLOCKRETURN(iop, ret)	return (ret)

#define	_FWRITE fwrite
#define	REWIND rewind

#define	FILENO(s) fileno(s)
#define	FEOF(s) feof(s)
#define	FERROR(s) ferror(s)
#define	CLEARERR(s) clearerr(s)
#define	GETC(s) getc(s)
#define	UNGETC(c, s) ungetc(c, s)
#define	PUTC(c, s) putc(c, s)
#define	GETWC(s) getwc(s)
#define	PUTWC(c, s) putwc(c, s)

#endif	/*	_REENTRANT	*/


#define	MT_ASSERT_HELD(x)

#endif	/*	_MTLIB_H_	*/
