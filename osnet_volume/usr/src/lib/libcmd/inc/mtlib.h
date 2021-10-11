/*
 * Copyright (c) 1992, 1997 by Sun Microsystems, Inc.
 */

#ifndef _MTLIB_H
#define	_MTLIB_H

#pragma ident	"@(#)mtlib.h 1.4	97/11/21 SMI"	/* SVr4.0 1.33  */

#ifdef _REENTRANT

#define	mutex_lock(m)			_mutex_lock(m)
#define	mutex_trylock(m)		_mutex_trylock(m)
#define	mutex_unlock(m)			_mutex_unlock(m)
#define	mutex_init(m, n, p)		_mutex_init(m, n, p)
#define	rw_wrlock(x)			_rw_wrlock(x)
#define	rw_rdlock(x)			_rw_rdlock(x)
#define	rw_unlock(x)			_rw_unlock(x)
#define	thr_self			_thr_self
#define	thr_exit(x)			_thr_exit(x)
#define thr_getspecific(x, y)		_thr_getspecific(x, y)
#define thr_setspecific(x, y)		_thr_setspecific(x, y)
#define thr_keycreate(x, y)		_thr_keycreate(x, y)
#define dgettext(x, y)			_dgettext(x, y)

#include <thread.h>
#include <synch.h>
#include <libintl.h>

#define	SIGMASK_MUTEX_LOCK(lk, m, oldmask) \
{	sigset_t newmask; \
	sigfillset(&newmask); \
	_thr_sigsetmask(SIG_SETMASK, &newmask, &(oldmask)); \
	lk = _flockget(m); \
}

#define	SIGMASK_MUTEX_UNLOCK(lk, m, oldmask) \
{	sigset_t tmpmask; \
	_flockrel(lk); \
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
#define	FLOCKFILE(iop) flockfile((iop))
#define	FUNLOCKFILE(iop)  funlockfile((iop))
#define	FLOCKRETURN(iop, ret) \
	{	int r; \
		FLOCKFILE(iop); \
		r = (ret); \
		FUNLOCKFILE(iop); \
		return (r); \
	}

#else

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
#define	SIGMASK_MUTEX_LOCK(m, old)
#define	SIGMASK_MUTEX_UNLOCK(m, old)
#define	FLOCKFILE(iop)
#define	FUNLOCKFILE(iop)
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

#endif _REENTRANT

#define	MT_ASSERT_HELD(x)

#endif /* _MTLIB_H */
