/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scanf.c	1.19	99/11/03 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/
#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <thread.h>
#include <synch.h>
#include "libc.h"
#include "stdiom.h"
#include "mse.h"
#include <stdio_ext.h>

/*VARARGS1*/
int
scanf(const char *fmt, ...)
{
	rmutex_t	*lk;
	int	ret;
	va_list ap;

	va_start(ap, fmt);

	FLOCKFILE(lk, stdin);

	_SET_ORIENTATION_BYTE(stdin);

	ret = _doscan(stdin, fmt, ap);
	FUNLOCKFILE(lk);
	return (ret);
}

/*VARARGS2*/
int
fscanf(FILE *iop, const char *fmt, ...)
{
	rmutex_t	*lk;
	int	ret;
	va_list ap;

	va_start(ap, fmt);

	FLOCKFILE(lk, iop);

	_SET_ORIENTATION_BYTE(iop);

	ret = _doscan(iop, fmt, ap);
	FUNLOCKFILE(lk);
	return (ret);
}

/*VARARGS2*/
int
sscanf(const char *str, const char *fmt, ...)
{
	va_list ap;
	FILE strbuf;

	va_start(ap, fmt);
	/*
	 * The dummy FILE * created for sscanf has the _IOWRT
	 * flag set to distinguish it from scanf and fscanf
	 * invocations.
	 */
	strbuf._flag = _IOREAD | _IOWRT;
	strbuf._ptr = strbuf._base = (unsigned char *)str;
	strbuf._cnt = strlen(str);
	strbuf._file = _NFILE;

	/*
	 * Mark the stream so that routines called by __doscan_u()
	 * do not do any locking. In particular this avoids a NULL
	 * lock pointer being used by getc() causing a core dump.
	 * See bugid -  1210179 program SEGV's in sscanf if linked with
	 * the libthread.
	 * This also makes sscanf() quicker since it does not need
	 * to do any locking.
	 */
	if (__fsetlocking(&strbuf, FSETLOCKING_BYCALLER) == -1) {
		return (-1);	/* this should never happen */
	}


	/* as this stream is local to this function, no locking is be done */
	return (__doscan_u(&strbuf, fmt, ap));
}
