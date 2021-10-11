/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma	ident	"@(#)data.c	1.27	99/06/10 SMI"	/* SVr4.0 2.14	*/

/*LINTLIBRARY*/

#pragma weak _iob = __iob

#include "synonyms.h"
#include "mbstatet.h"
#include "mtlib.h"
#include "file64.h"
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"
#include <wchar.h>

/*
 * Ptrs to start of preallocated buffers for stdin, stdout.
 * Some slop is allowed at the end of the buffers in case an upset in
 * the synchronization of _cnt and _ptr (caused by an interrupt or other
 * signal) is not immediately detected.
 */

Uchar _sibuf[BUFSIZ + _SMBFSZ], _sobuf[BUFSIZ + _SMBFSZ];
Uchar _smbuf[_NFILE + 1][_SMBFSZ] = {0};  /* shared library compatibility */


#ifdef	_LP64

mbstate_t	_stdstate[3];

FILE _iob[_NFILE] = {
	{ NULL, NULL, NULL, 0, 0, _IOREAD, DEFAULTRMUTEX, &_stdstate[0] },
	{ NULL, NULL, NULL, 0, 1, _IOWRT, DEFAULTRMUTEX, &_stdstate[1] },
	{ NULL, NULL, NULL, 0, 2, _IOWRT|_IONBF, DEFAULTRMUTEX, &_stdstate[2] },
};

#else

rmutex_t _locktab[_NFILE + 1] = {
	DEFAULTRMUTEX, DEFAULTRMUTEX, DEFAULTRMUTEX,
	DEFAULTRMUTEX, DEFAULTRMUTEX, DEFAULTRMUTEX,
	DEFAULTRMUTEX, DEFAULTRMUTEX, DEFAULTRMUTEX,
	DEFAULTRMUTEX, DEFAULTRMUTEX, DEFAULTRMUTEX,
	DEFAULTRMUTEX, DEFAULTRMUTEX, DEFAULTRMUTEX,
	DEFAULTRMUTEX, DEFAULTRMUTEX, DEFAULTRMUTEX,
	DEFAULTRMUTEX, DEFAULTRMUTEX, DEFAULTRMUTEX};

#define	DEFAULTMBSTATE \
	{ NULL, NULL, {0, 0, 0, 0, 0, 0, 0, 0}, 0, {0, 0}}

mbstate_t	_statetab[_NFILE + 1] = {
	DEFAULTMBSTATE, DEFAULTMBSTATE, DEFAULTMBSTATE,
	DEFAULTMBSTATE, DEFAULTMBSTATE, DEFAULTMBSTATE,
	DEFAULTMBSTATE, DEFAULTMBSTATE, DEFAULTMBSTATE,
	DEFAULTMBSTATE, DEFAULTMBSTATE, DEFAULTMBSTATE,
	DEFAULTMBSTATE, DEFAULTMBSTATE, DEFAULTMBSTATE,
	DEFAULTMBSTATE, DEFAULTMBSTATE, DEFAULTMBSTATE,
	DEFAULTMBSTATE, DEFAULTMBSTATE, DEFAULTMBSTATE};

/*
 * Ptrs to end of read/write buffers for first _NFILE devices.
 * There is an extra bufend pointer which corresponds to the dummy
 * file number _NFILE, which is used by sscanf and sprintf.
 */
Uchar *_bufendtab[_NFILE+1] = { NULL, NULL, _smbuf[2] + _SBFSIZ, };

FILE _iob[_NFILE] = {
	{ 0, NULL, NULL, _IOREAD, 0 },
	{ 0, NULL, NULL, _IOWRT, 1 },
	{ 0, NULL, NULL, _IOWRT|_IONBF, 2 },
};

/*
 * Ptr to end of io control blocks
 */
FILE *_lastbuf = &_iob[_NFILE];

#endif	/*	_LP64	*/
