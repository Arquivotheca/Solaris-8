/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wscanf.c	1.7	99/05/04 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/
#include "synonyms.h"
#include "file64.h"
#include <mtlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <thread.h>
#include <synch.h>
#include <wchar.h>
#include <errno.h>
#include <sys/localedef.h>
#include <stdlib.h>
#include <alloca.h>
#include "mse.h"
#include "stdiom.h"
#include "libc.h"

int
wscanf(const wchar_t *fmt, ...)
{
	rmutex_t	*lk;
	_LC_charmap_t	*lc;
	int	ret;
	va_list	ap;

	va_start(ap, fmt);

	FLOCKFILE(lk, stdin);

	if (_set_orientation_wide(stdin, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (EOF);
	}

	ret = __wdoscan_u(stdin, fmt, ap);
	FUNLOCKFILE(lk);
	return (ret);
}

int
fwscanf(FILE *iop, const wchar_t *fmt, ...)
{
	rmutex_t	*lk;
	_LC_charmap_t	*lc;
	int	ret;
	va_list	ap;

	va_start(ap, fmt);

	FLOCKFILE(lk, iop);

	if (_set_orientation_wide(iop, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (EOF);
	}


	ret = __wdoscan_u(iop, fmt, ap);
	FUNLOCKFILE(lk);
	return (ret);
}

int
swscanf(const wchar_t *wstr, const wchar_t *fmt, ...)
{
	va_list	ap;
	FILE	strbuf;
	size_t	wlen, clen;
	char	*tmp_buf;
	int	ret;

	va_start(ap, fmt);
	/*
	 * The dummy FILE * created for swscanf has the _IOWRT
	 * flag set to distinguish it from wscanf and fwscanf
	 * invocations.
	 */

	clen = wcstombs((char *)NULL, wstr, 0);
	if (clen == (size_t)-1) {
		errno = EILSEQ;
		return (EOF);
	}
	tmp_buf = (char *)alloca(sizeof (char) * (clen + 1));
	if (tmp_buf == NULL)
		return (EOF);
	wlen = wcstombs(tmp_buf, wstr, clen + 1);
	if (wlen == (size_t)-1) {
		errno = EILSEQ;
		return (EOF);
	}

	strbuf._flag = _IOREAD | _IOWRT;
	strbuf._ptr = strbuf._base = (unsigned char *)tmp_buf;
	strbuf._cnt = strlen(tmp_buf);
	strbuf._file = _NFILE;
	/* Probably the following is not required. */
	/* _setorientation(&strbuf, _WC_MODE); */

	ret = __wdoscan_u(&strbuf, fmt, ap);
	return (ret);
}
