/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setvbuf.c	1.16	97/12/02 SMI"	/* SVr4.0 1.18 */

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

int
setvbuf(FILE *iop, char *abuf, int type, size_t size)
{

	Uchar	*buf = (Uchar *)abuf;
	Uchar *temp;
	int	sflag = iop->_flag & _IOMYBUF;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	FLOCKFILE(lk, iop);
	iop->_flag &= ~(_IOMYBUF | _IONBF | _IOLBF);
	switch (type) {
	/* note that the flags are the same as the possible values for type */
	case _IONBF:
		iop->_flag |= _IONBF;	 /* file is unbuffered */
#ifndef _STDIO_ALLOCATE
		if (iop->_file < 2) {
			/* use special buffer for std{in,out} */
			buf = (iop->_file == 0) ? _sibuf : _sobuf;
			size = BUFSIZ;
		} else /* needed for ifdef */
#endif
		if (iop->_file < _NFILE) {
			buf = _smbuf[iop->_file];
			size = _SMBFSZ - PUSHBACK;
		} else
			if ((buf = malloc(_SMBFSZ * sizeof (Uchar))) != NULL) {
				iop->_flag |= _IOMYBUF;
				size = _SMBFSZ - PUSHBACK;
			} else {
				FUNLOCKFILE(lk);
				return (EOF);
			}
		break;
	case _IOLBF:
	case _IOFBF:
		iop->_flag |= type;	/* buffer file */
		/*
		 * need at least an 8 character buffer for
		 * out_of_sync concerns.
		 */
		if (size <= _SMBFSZ) {
			size = BUFSIZ;
			buf = NULL;
		}
		if (buf == NULL) {
			if ((buf = malloc(sizeof (Uchar) *
			    (size + _SMBFSZ))) != NULL)
				iop->_flag |= _IOMYBUF;
			else {
				FUNLOCKFILE(lk);
				return (EOF);
			}
		}
		else
			size -= _SMBFSZ;
		break;
	default:
		FUNLOCKFILE(lk);
		return (EOF);
	}
	if (iop->_base != NULL && sflag)
		free((char *)iop->_base - PUSHBACK);
	temp = buf + PUSHBACK;
	iop->_base = temp;
	_setbufend(iop, temp + size);
	iop->_ptr = temp;
	iop->_cnt = 0;
	FUNLOCKFILE(lk);
	return (0);
}
