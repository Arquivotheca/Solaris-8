/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wprintf.c	1.7	99/05/04 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "file64.h"
#include "shlib.h"
#include <mtlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <values.h>
#include <sys/localedef.h>
#include <wchar.h>
#include "print.h"
#include "stdiom.h"
#include <sys/types.h>
#include "libc.h"
#include "mse.h"

int
wprintf(const wchar_t *format, ...)
{
	ssize_t	count;
	rmutex_t	*lk;
	_LC_charmap_t	*lc;
	va_list	ap;

	va_start(ap, format);

	FLOCKFILE(lk, stdout);

	if (_set_orientation_wide(stdout, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (EOF);
	}

	if (!(stdout->_flag & _IOWRT)) {
		/* if no write flag */
		if (stdout->_flag & _IORW) {
			/* if ok, cause read-write */
			stdout->_flag |= _IOWRT;
		} else {
			/* else error */
			errno = EBADF;
			FUNLOCKFILE(lk);
			return (EOF);
		}
	}

	count = _wdoprnt(format, ap, stdout);
	va_end(ap);
	if (FERROR(stdout) || count == EOF) {
		FUNLOCKFILE(lk);
		return (EOF);
	}
	FUNLOCKFILE(lk);
	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}

int
fwprintf(FILE *iop, const wchar_t *format, ...)
{
	ssize_t	count;
	rmutex_t	*lk;
	_LC_charmap_t	*lc;
	va_list	ap;

	va_start(ap, format);

	FLOCKFILE(lk, iop);

	if (_set_orientation_wide(iop, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (EOF);
	}

	if (!(iop->_flag & _IOWRT)) {
		/* if no write flag */
		if (iop->_flag & _IORW) {
			/* if ok, cause read-write */
			iop->_flag |= _IOWRT;
		} else {
			/* else error */
			errno = EBADF;
			FUNLOCKFILE(lk);
			return (EOF);
		}
	}

	count = _wdoprnt(format, ap, iop);
	va_end(ap);
	if (FERROR(iop) || count == EOF) {
		FUNLOCKFILE(lk);
		return (EOF);
	}
	FUNLOCKFILE(lk);
	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}

int
swprintf(wchar_t *string, size_t n, const wchar_t *format, ...)
{
	ssize_t	count;
	FILE	siop;
	wchar_t	*wp;
	va_list	ap;

	if (n == 0)
		return (EOF);

	siop._cnt = (ssize_t)n - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD;
	va_start(ap, format);
	count = _wdoprnt(format, ap, &siop);
	va_end(ap);
	wp = (wchar_t *)siop._ptr;
	*wp = L'\0';
	if (count == EOF) {
		return (EOF);
	}
	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}


int
vwprintf(const wchar_t *format, va_list ap)
{
	ssize_t	count;
	rmutex_t	*lk;
	_LC_charmap_t	*lc;

	FLOCKFILE(lk, stdout);

	if (_set_orientation_wide(stdout, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (EOF);
	}

	if (!(stdout->_flag & _IOWRT)) {
		/* if no write flag */
		if (stdout->_flag & _IORW) {
			/* if ok, cause read-write */
			stdout->_flag |= _IOWRT;
		} else {
			/* else error */
			errno = EBADF;
			FUNLOCKFILE(lk);
			return (EOF);
		}
	}

	count = _wdoprnt(format, ap, stdout);
	if (FERROR(stdout) || count == EOF) {
		FUNLOCKFILE(lk);
		return (EOF);
	}
	FUNLOCKFILE(lk);
	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}


int
vfwprintf(FILE *iop, const wchar_t *format, va_list ap)
{
	ssize_t	count;
	rmutex_t	*lk;
	_LC_charmap_t	*lc;

	FLOCKFILE(lk, iop);

	if (_set_orientation_wide(iop, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (EOF);
	}

	if (!(iop->_flag & _IOWRT)) {
		/* if no write flag */
		if (iop->_flag & _IORW) {
			/* if ok, cause read-write */
			iop->_flag |= _IOWRT;
		} else {
			/* else error */
			errno = EBADF;
			FUNLOCKFILE(lk);
			return (EOF);
		}
	}

	count = _wdoprnt(format, ap, iop);
	if (FERROR(iop) || count == EOF) {
		FUNLOCKFILE(lk);
		return (EOF);
	}
	FUNLOCKFILE(lk);
	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}

int
vswprintf(wchar_t *string, size_t n, const wchar_t *format, va_list ap)
{
	ssize_t	count;
	FILE	siop;
	wchar_t	*wp;

	if (n == 0)
		return (EOF);

	siop._cnt = (ssize_t)n - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD;

	count = _wdoprnt(format, ap, &siop);

	wp = (wchar_t *)siop._ptr;
	*wp = L'\0';
	if (count == EOF) {
		return (EOF);
	}
	/* check for overflow */
	if ((size_t)count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int)count);
}
