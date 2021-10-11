/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)flush.c	1.43	99/12/08 SMI"	/* SVr4.0 1.22	*/

/*LINTLIBRARY*/		/* This file always part of stdio usage */

#include "synonyms.h"
#include "shlib.h"
#include "mtlib.h"
#include "file64.h"
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <unistd.h>
#include <string.h>
#include "stdiom.h"
#include <wchar.h>

#undef _cleanup
#undef end

/* CSTYLED */
#pragma fini (_cleanup)
#define	FILE_ARY_SZ	8 /* a nice size for FILE array & end_buffer_ptrs */

/*
 * initial array of end-of-buffer ptrs
 */

extern Uchar _smbuf[][_SMBFSZ];

#ifdef	_LP64
extern	mbstate_t	_stdstate[3];
#endif	/*	_LP64	*/

static Uchar *_cp_bufendtab[_NFILE + 1] = /* for alternate system - original */
	{ NULL, NULL, _smbuf[2] + _SBFSIZ, };

struct _link_	/* manages a list of streams */
{
	FILE *iobp;	/* the array of FILE's */

#ifndef	_LP64
/* the end pointer and the lock have moved into __FILE for 64bit mode */
	Uchar 	**endbuf;	/* the array of end buffer pointers */
	rmutex_t *lockbuf;
	mbstate_t	*statebuf;
#endif	/*	_LP64	*/

	int	niob;		/* length of the arrays */
	struct _link_	*next;	/* next in the list */
};

/*
 * With dynamic linking, iob may be in either the library or in the user's
 * a.out, so the run time linker fixes up the first entry in __first_link at
 * process startup time.
 */
struct _link_ __first_link =	/* first in linked list */
{
#if DSHLIB
	NULL,
#else
	&_iob[0],
#endif

#ifndef	_LP64
	&_cp_bufendtab[0],
	&_locktab[0],
	&_statetab[0],
#endif	/*	_LP64	*/
	_NFILE,
	NULL
};

static rwlock_t _first_link_lock = DEFAULTRWLOCK;

static int _fflush_u_iops(void);
#ifdef	_LP64
static FILE *getiop(FILE *, mbstate_t *);
#else
static FILE *getiop(FILE *, struct _link_ *);
#endif	/*	_LP64	*/

#ifdef	_LP64
#define	GETIOP(fp, mb)	{FILE * ret; \
	if ((ret = getiop((fp), (mb))) != NULL) { \
		if (__threaded) \
			(void) _rw_unlock(&_first_link_lock); \
		return (ret); \
	}; \
	}
#else
#define	GETIOP(fp, lp)	{FILE * ret; \
	if ((ret = getiop((fp), (lp))) != NULL) { \
		if (__threaded) \
			(void) _rw_unlock(&_first_link_lock); \
		return (ret); \
	}; \
	}
#endif	/*	_LP64	*/

/*
 * All functions that understand the linked list of iob's follow.
 */

void
_cleanup(void)		/* called at process end to flush ouput streams */
{
	(void) fflush(NULL);
}

void
_flushlbf(void)		/* fflush() all line-buffered streams */
{
	FILE *fp;
	int i;
	struct _link_ *lp;

	(void) _rw_rdlock(&_first_link_lock);
	lp = &__first_link;

	do {
		fp = lp->iobp;
		for (i = lp->niob; --i >= 0; fp++) {
			if ((fp->_flag & (_IOLBF | _IOWRT)) ==
				(_IOLBF | _IOWRT))
				(void) _fflush_u(fp);
		}
	} while ((lp = lp->next) != NULL);
	(void) _rw_unlock(&_first_link_lock);
}

/* allocate an unused stream; NULL if cannot */
FILE *
_findiop(void)
{

	mbstate_t	*mb;
	struct _link_ *lp, **prev;
	/* used so there only needs to be one malloc() */
	typedef	struct	{
		struct _link_	hdr;
		FILE	iob[FILE_ARY_SZ];
		Uchar	*nbuf[FILE_ARY_SZ]; /* array of end buffer pointers */
		rmutex_t nlock[FILE_ARY_SZ];
		mbstate_t	state[FILE_ARY_SZ];
	} Pkg;
	Pkg *pkgp;

	FILE *fp;

	if (__threaded)
		(void) _rw_wrlock(&_first_link_lock);

	lp = &__first_link;

#ifdef	_LP64
	if ((mb = malloc(sizeof (mbstate_t))) == NULL) {
		if (__threaded)
			(void) _rw_unlock(&_first_link_lock);
		return (NULL);
	}
#endif	/*	_LP64	*/

	/*
	 * lock to make testing of fp->_flag == 0 and acquiring the fp atomic
	 * and for allocation of new links
	 * low contention expected on _findiop(), hence coarse locking.
	 * for finer granularity, use fp->_lock for allocating an iop
	 * and make the testing of lp->next and allocation of new link atomic
	 * using lp->_lock
	 */

	do {
		int i;
		rmutex_t *lk;

		prev = &lp->next;
		fp = lp->iobp;

		for (i = lp->niob; --i >= 0; fp++) {
#ifdef	_LP64
			GETIOP(fp, mb);
#else
			GETIOP(fp, lp);
#endif	/*	_LP64	*/
		}
	} while ((lp = lp->next) != NULL);
	/*
	 * Need to allocate another and put it in the linked list.
	 */

	if ((pkgp = (Pkg *) malloc(sizeof (Pkg))) == NULL) {
#ifdef	_LP64
		free(mb);
#endif	/*	_LP64	*/
		if (__threaded)
			(void) _rw_unlock(&_first_link_lock);
		return (NULL);
	}
	(void) memset(pkgp, 0, sizeof (Pkg));

	pkgp->hdr.iobp = &pkgp->iob[0];
	pkgp->hdr.niob = sizeof (pkgp->iob) / sizeof (FILE);
	fp = &pkgp->iob[0];

#ifdef	_LP64
	fp->_state = mb;
#else
	pkgp->hdr.endbuf = &pkgp->nbuf[0];
	pkgp->hdr.lockbuf = &pkgp->nlock[0];
	mb = pkgp->hdr.statebuf = &pkgp->state[0];
#endif	/*	_LP64	*/

	*prev = &pkgp->hdr;
	fp->_ptr = 0;
	fp->_base = 0;
	fp->_flag = 0377; /* claim the fp by setting low 8 bits */
	(void) memset(mb, 0, sizeof (mbstate_t));
	if (__threaded)
		(void) _rw_unlock(&_first_link_lock);

	return (fp);
}

#ifdef	_LP64
void
_setbufend(FILE *iop, Uchar *end)	/* set the end pointer for this iop */
{
	iop->_end = end;
}

#else	/	_LP64	*/
void
_setbufend(FILE *iop, Uchar *end)	/* set the end pointer for this iop */
{

	struct _link_ *lp;

	if (__threaded)
		(void) _rw_rdlock(&_first_link_lock);
	lp = &__first_link;

	/*
	 * Old mechanism.  Retained for binary compatibility.
	 */
	if (iop->_file < _NFILE)
		_bufendtab[iop->_file] = end;

	/*
	 * New mechanism.  Allows more than _NFILE iop's.
	 */
	do {
		if ((lp->iobp <= iop) && (iop < (lp->iobp + lp->niob))) {
			lp->endbuf[iop - lp->iobp] = end;
			break;
		}
	} while ((lp = lp->next) != NULL);
	if (__threaded)
		(void) _rw_unlock(&_first_link_lock);
}

#endif	/*	_LP64	*/

#ifdef	_LP64
Uchar *
_realbufend(FILE *iop)		/* get the end pointer for this iop */
{
	return (iop->_end);
}

#else	/*	_LP64	*/

Uchar *
_realbufend(FILE *iop)		/* get the end pointer for this iop */
{
	struct _link_ *lp;
	Uchar *result = NULL;

	if (__threaded)
		(void) _rw_rdlock(&_first_link_lock);
	lp = &__first_link;

	/*
	 * Use only the new mechanism here.
	 */
	do {
		if ((lp->iobp <= iop) && (iop < (lp->iobp + lp->niob))) {
			result = lp->endbuf[iop - lp->iobp];
			break;
		}
	} while ((lp = lp->next) != NULL);
	if (__threaded)
		(void) _rw_unlock(&_first_link_lock);
	return (result);
}

#endif	/*	_LP64	*/

#ifdef	_LP64
rmutex_t *
_reallock(FILE *iop)
{
	return (&iop->_lock);
}
#else
rmutex_t *
_reallock(FILE *iop)
{

	struct _link_ *lp;
	rmutex_t *result = NULL;

	(void) _rw_rdlock(&_first_link_lock);
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop) && (iop < (lp->iobp + lp->niob))) {
			result = &lp->lockbuf[iop - lp->iobp];
			break;
		}
	} while ((lp = lp->next) != NULL);
	(void) _rw_unlock(&_first_link_lock);
	return (result);
}

#endif	/*	_LP64	*/

/* make sure _cnt, _ptr are correct */
void
_bufsync(FILE *iop, Uchar *bufend)
{
	ssize_t spaceleft;

	spaceleft = bufend - iop->_ptr;
	if (bufend < iop->_ptr) {
		iop->_ptr = bufend;
		iop->_cnt = 0;
	} else if (spaceleft < iop->_cnt)
		iop->_cnt = spaceleft;
}

/* really write out current buffer contents */
int
_xflsbuf(FILE *iop)
{
	ssize_t n;
	Uchar *base = iop->_base;
	Uchar *bufend;
	ssize_t num_wrote;

	/*
	 * Hopefully, be stable with respect to interrupts...
	 */
	n = iop->_ptr - base;
	iop->_ptr = base;
	bufend = _bufend(iop);
	if (iop->_flag & (_IOLBF | _IONBF))
		iop->_cnt = 0;		/* always go to a flush */
	else
		iop->_cnt = bufend - base;

	if (_needsync(iop, bufend))	/* recover from interrupts */
		_bufsync(iop, bufend);

	if (n > 0) {
		while ((num_wrote =
			write(iop->_file, base, (size_t)n)) != n) {
			if (num_wrote <= 0) {
				iop->_flag |= _IOERR;
				return (EOF);
			}
			n -= num_wrote;
			base += num_wrote;
		}
	}
	return (0);
}

/* flush (write) buffer */
int
fflush(FILE *iop)
{
	int res;
	rmutex_t *lk;

	if (iop) {
		FLOCKFILE(lk, iop);
		res = _fflush_u(iop);
		FUNLOCKFILE(lk);
	} else {
		res = _fflush_u_iops();		/* flush all iops */
	}
	return (res);
}

static int
_fflush_u_iops(void)		/* flush (write) all buffers */
{
	FILE *iop;

	int i;
	struct _link_ *lp;
	int res = 0;

	if (__threaded)
		(void) _rw_rdlock(&_first_link_lock);

	lp = &__first_link;

	do {
		/*
		 * Don't grab the locks for these file pointers
		 * since they are supposed to be flushed anyway
		 * It could also be the case in which the 2nd
		 * portion (base and lock) are not initialized
		 */
		iop = lp->iobp;
		for (i = lp->niob; --i >= 0; iop++) {
			if (!(iop->_flag & _IONBF) &&
			    (iop->_flag & (_IOWRT | _IOREAD | _IORW)))
				res |= _fflush_u(iop);
		}
	} while ((lp = lp->next) != NULL);
	if (__threaded)
		(void) _rw_unlock(&_first_link_lock);
	return (res);
}

/* flush (write) buffer */
int
_fflush_u(FILE *iop)
{
	int res = 0;

	/* this portion is always assumed locked */
	if (!(iop->_flag & _IOWRT)) {
		(void) lseek64(iop->_file, -iop->_cnt, SEEK_CUR);
		iop->_cnt = 0;
		/* needed for ungetc & mulitbyte pushbacks */
		iop->_ptr = iop->_base;
		if (iop->_flag & _IORW) {
			iop->_flag &= ~_IOREAD;
		}
		return (0);
	}
	if (iop->_base != NULL && iop->_ptr > iop->_base) {
		res = _xflsbuf(iop);
	}
	if (iop->_flag & _IORW) {
		iop->_flag &= ~_IOWRT;
		iop->_cnt = 0;
	}
	return (res);
}

/* flush buffer and close stream */
int
fclose(FILE *iop)
{
	int res = 0;
	rmutex_t *lk;

	if (iop == NULL) {
		return (EOF);		/* avoid passing zero to FLOCKFILE */
	}

	FLOCKFILE(lk, iop);
	if (iop->_flag == 0) {
		FUNLOCKFILE(lk);
		return (EOF);
	}
	/* Is not unbuffered and opened for read and/or write ? */
	if (!(iop->_flag & _IONBF) && (iop->_flag & (_IOWRT | _IOREAD | _IORW)))
		res = _fflush_u(iop);
	if (close(iop->_file) < 0)
		res = EOF;
	if (iop->_flag & _IOMYBUF) {
		(void) free((char *)iop->_base - PUSHBACK);
	}
	iop->_base = NULL;
	iop->_ptr = NULL;
#ifdef  _LP64
	/*
	 * clear instead of free the state associated
	 * with stdin, stdout or stderr
	 */
	if (iop->_state == &_stdstate[0] || \
	    iop->_state == &_stdstate[1] || \
	    iop->_state == &_stdstate[2]) {
		(void) memset(iop->_state, 0, sizeof (mbstate_t));
	} else {
		free(iop->_state);
		iop->_state = NULL;
	}
#endif	/*	_LP64	*/
	iop->_cnt = 0;
	iop->_flag = 0;			/* marks it as available */
	FUNLOCKFILE(lk);
	return (res);
}

/* flush buffer, close fd but keep the stream used by freopen() */
int
close_fd(FILE *iop)
{
	int res = 0;

	if (iop == NULL || iop->_flag == 0)
		return (EOF);
	/* Is not unbuffered and opened for read and/or write ? */
	if (!(iop->_flag & _IONBF) && (iop->_flag & (_IOWRT | _IOREAD | _IORW)))
		res = _fflush_u(iop);
	if (close(iop->_file) < 0)
		res = EOF;
	if (iop->_flag & _IOMYBUF) {
		(void) free((char *)iop->_base - PUSHBACK);
	}
	iop->_base = NULL;
	iop->_ptr = NULL;
#ifdef	_LP64
	/*
	 * we deliberately retain the mbstate but clean it out, since
	 * the iop is not being released when this function is called
	 *
	 */
	(void) memset(iop->_state, 0, sizeof (mbstate_t));
#endif	/*	_LP64	*/
	iop->_cnt = 0;
	_setorientation(iop, _NO_MODE);
	return (res);
}

#ifdef	_LP64
static FILE *
getiop(FILE *fp, mbstate_t *mb)
{
#else
static FILE *
getiop(FILE *fp, struct _link_ *lp)
{

	mbstate_t	*mb;

#endif	/*	_LP64	*/

	rmutex_t *lk;

#ifdef	_LP64
	lk = &fp->_lock;
#else
	lk = &lp->lockbuf[fp - lp->iobp];
#endif	/*	_LP64	*/
	if (__threaded && _rmutex_trylock(lk))
		return (NULL); /* being locked: fp in use */

	if (fp->_flag == 0) {	/* unused */
#ifdef	_LP64
		fp->_state = mb;
#else  /* _LP64 */
		fp->__orientation = 0;
		mb = &lp->statebuf[fp - lp->iobp];
#endif /* _LP64 */
		fp->_cnt = 0;
		fp->_ptr = NULL;
		fp->_base = NULL;
		fp->_flag = 0377;	/* claim the fp by setting low 8 bits */
		(void) memset(mb, 0, sizeof (mbstate_t));
		FUNLOCKFILE(lk);
		return (fp);
	}
	FUNLOCKFILE(lk);
	return (NULL);
}

/*
 * DESCRIPTION:
 * This function gets the pointer to the mbstate_t structure associated
 * with the specified iop.
 *
 * RETURNS:
 * If the associated mbstate_t found, the pointer to the mbstate_t is
 * returned.  Otherwise, NULL is returned.
 */
mbstate_t *
_getmbstate(FILE *iop)
{
#ifdef	_LP64
	return (iop->_state);
#else  /* _LP64 */
	struct _link_	*lp;
	mbstate_t	*result = NULL;

	if (__threaded)
		(void) _rw_rdlock(&_first_link_lock);

	lp = &__first_link;

	do {
		if ((lp->iobp <= iop) && (iop < (lp->iobp + lp->niob))) {
			result = &lp->statebuf[iop - lp->iobp];
			break;
		}
	} while ((lp = lp->next) != NULL);

	if (__threaded)
		(void) _rw_unlock(&_first_link_lock);

	return (result);
#endif /* _LP64 */
}
