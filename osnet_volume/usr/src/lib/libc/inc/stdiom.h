/*
 * Copyright (c) 1996-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 * stdiom.h - shared guts of stdio
 */

#ifndef	_STDIOM_H
#define	_STDIOM_H

#pragma ident	"@(#)stdiom.h	1.37	99/12/08 SMI"	/* SVr4.0 1.9 */

#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include <stdarg.h>
#include "file64.h"
#include <wchar.h>
#include <sys/localedef.h>
#include "mse.h"


/*
 * The following flags, and the macros that manipulate them, operate upon
 * the FILE structure used by stdio. If new flags are required, they should
 * be created in this file. The values of the flags must be differnt from
 * the currently used values. New macros should be created to use the flags
 * so that the compilation mode dependencies can be isolated here.
 */

#ifdef _LP64
#define	_BYTE_MODE_FLAG	0400
#define	_WC_MODE_FLAG	01000
#define	_IONOLOCK	02000
#define	SET_IONOLOCK(iop)	((iop)->_flag |= _IONOLOCK)
#define	CLEAR_IONOLOCK(iop)	((iop)->_flag &= ~_IONOLOCK)
#define	GET_IONOLOCK(iop)	((iop)->_flag & _IONOLOCK)
#define	SET_BYTE_MODE(iop)	((iop)->_flag |= _BYTE_MODE_FLAG)
#define	CLEAR_BYTE_MODE(iop)	((iop)->_flag &= ~_BYTE_MODE_FLAG)
#define	GET_BYTE_MODE(iop)	((iop)->_flag & _BYTE_MODE_FLAG)
#define	SET_WC_MODE(iop)	((iop)->_flag |= _WC_MODE_FLAG)
#define	CLEAR_WC_MODE(iop)	((iop)->_flag &= ~_WC_MODE_FLAG)
#define	GET_WC_MODE(iop)	((iop)->_flag & _WC_MODE_FLAG)
#define	GET_NO_MODE(iop)	(!((iop)->_flag & \
					(_BYTE_MODE_FLAG | _WC_MODE_FLAG)))
#else
#define	_BYTE_MODE_FLAG	0001
#define	_WC_MODE_FLAG	0002
#define	SET_IONOLOCK(iop)	((iop)->__ionolock = 1)
#define	CLEAR_IONOLOCK(iop)	((iop)->__ionolock = 0)
#define	GET_IONOLOCK(iop)	((iop)->__ionolock)
#define	SET_BYTE_MODE(iop)	((iop)->__orientation |= _BYTE_MODE_FLAG)
#define	CLEAR_BYTE_MODE(iop)	((iop)->__orientation &= ~_BYTE_MODE_FLAG)
#define	GET_BYTE_MODE(iop)	((iop)->__orientation & _BYTE_MODE_FLAG)
#define	SET_WC_MODE(iop)	((iop)->__orientation |= _WC_MODE_FLAG)
#define	CLEAR_WC_MODE(iop)	((iop)->__orientation &= ~_WC_MODE_FLAG)
#define	GET_WC_MODE(iop)	((iop)->__orientation & _WC_MODE_FLAG)
#define	GET_NO_MODE(iop)	(!((iop)->__orientation & \
					(_BYTE_MODE_FLAG | _WC_MODE_FLAG)))
#endif


/*
 * Cheap check to tell if library needs to lock for MT progs.
 * Referenced directly in port/stdio/flush.c and FLOCKFILE
 * and FUNLOCKFILE macros.
 *
 * Initial value is set in _init section of libc.so by _check_threaded.
 * __threaded gets set to 1 by _init section of libthread.so via call
 * to _libc_set_threaded.
 */
extern int __threaded;

typedef unsigned char	Uchar;



extern void _flockrel(rmutex_t *rl);

#define	MAXVAL	(MAXINT - (MAXINT % BUFSIZ))

/*
 * The number of actual pushback characters is the value
 * of PUSHBACK plus the first byte of the buffer. The FILE buffer must,
 * for performance reasons, start on a word aligned boundry so the value
 * of PUSHBACK should be a multiple of word.
 * At least 4 bytes of PUSHBACK are needed. If sizeof (int) = 1 this breaks.
 */
#define	PUSHBACK	(((3 + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

/* minimum buffer size must be at least 8 or shared library will break */
#define	_SMBFSZ	(((PUSHBACK + 4) < 8) ? 8 : (PUSHBACK + 4))

#if BUFSIZ == 1024
#define	MULTIBFSZ(SZ)	((SZ) & ~0x3ff)
#elif BUFSIZ == 512
#define	MULTIBFSZ(SZ)    ((SZ) & ~0x1ff)
#else
#define	MULTIBFSZ(SZ)    ((SZ) - (SZ % BUFSIZ))
#endif

#undef _bufend
#define	_bufend(iop)	_realbufend(iop)

/*
 * Internal data
 */
extern Uchar _smbuf[][_SMBFSZ];


/*
 * Internal routines from flush.c
 */
extern void	_cleanup(void);
extern void	_flushlbf(void);
extern FILE	*_findiop(void);

/*
 * this is to be found in <stdio.h> for 32bit mode
 */
#ifdef	_LP64
extern int	__filbuf(FILE *);
extern int	__flsbuf(int, FILE *);
#endif	/*	_LP64	*/

extern Uchar 	*_realbufend(FILE *iop);
extern void	_setbufend(FILE *iop, Uchar *end);
extern rmutex_t *_flockget(FILE *iop);
extern rmutex_t	*_reallock(FILE *iop);
extern int	_xflsbuf(FILE *iop);
extern int	_wrtchk(FILE *iop);
extern void	_bufsync(FILE *iop, Uchar *bufend);
extern int	_fflush_u(FILE *iop);
extern int	close_fd(FILE *iop);
extern int	_doscan(FILE *, const char *, va_list);
#ifdef	_LP64
extern void	close_pid(void);
#endif	/*	_LP64	*/

/*
 * Internal routines from fileno.c
 */
extern int _fileno_unlocked(FILE *iop);

/*
 * Internal routines from _findbuf.c
 */
extern Uchar 	*_findbuf(FILE *iop);

/*
 * Internal routine used by fopen.c
 */
extern	FILE	*_endopen(const char *, const char *, FILE *, int);

/*
 * Internal routine from ferror.c
 */
extern int _ferror_unlocked(FILE *);

/*
 * Internal routine from ferror.c
 */
extern size_t _fwrite_unlocked(const void *, size_t, size_t, FILE *);

/*
 * Internal routine from getc.c
 */
int _getc_unlocked(FILE *);

/*
 * Internal routine from put.c
 */
int _putc_unlocked(int, FILE *);

/*
 * Internal routine from ungetc.c
 */
int _ungetc_unlocked(int, FILE *);

/*
 * Internal routine from flockf.c
 */
int _rmutex_trylock(rmutex_t *);


/*
 * The following macros improve performance of the stdio by reducing the
 * number of calls to _bufsync and _wrtchk.  _needsync checks whether
 * or not _bufsync needs to be called.  _WRTCHK has the same effect as
 * _wrtchk, but often these functions have no effect, and in those cases
 * the macros avoid the expense of calling the functions.
 */

#define	_needsync(p, bufend)	((bufend - (p)->_ptr) < \
				    ((p)->_cnt < 0 ? 0 : (p)->_cnt))

#define	_WRTCHK(iop)	((((iop->_flag & (_IOWRT | _IOEOF)) != _IOWRT) || \
			    (iop->_base == 0) ||  \
			    (iop->_ptr == iop->_base && iop->_cnt == 0 && \
			    !(iop->_flag & (_IONBF | _IOLBF)))) \
			? _wrtchk(iop) : 0)

/* definition of recursive mutex lock for flockfile and friends */

#define	DEFAULTRMUTEX	{DEFAULTMUTEX, DEFAULTCV, 0, 0, 0}

#ifdef	_LP64
#define	IOB_LCK(iop)	(&((iop)->_lock))
#else
#define	IOB_LCK(iop)	((_fileno_unlocked(iop) < _NFILE) ? \
			    &_locktab[_fileno_unlocked(iop)] : \
			    _reallock(iop))

extern rmutex_t _locktab[];

extern mbstate_t	_statetab[];

#endif	/*	_LP64	*/

#endif	/* _STDIOM_H */
