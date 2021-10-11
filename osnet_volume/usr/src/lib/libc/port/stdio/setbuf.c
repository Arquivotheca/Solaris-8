/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)setbuf.c	1.13	97/12/02 SMI"	/* SVr4.0 2.11	*/

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


void
setbuf(FILE *iop, char *abuf)
{
	Uchar *buf = (Uchar *)abuf;
	int fno = iop->_file;  /* file number */
	int size = BUFSIZ - _SMBFSZ;
	Uchar *temp;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	FLOCKFILE(lk, iop);
	if ((iop->_base != 0) && (iop->_flag & _IOMYBUF))
		free((char *)iop->_base - PUSHBACK);
	iop->_flag &= ~(_IOMYBUF | _IONBF | _IOLBF);
	if (buf == 0) {
		iop->_flag |= _IONBF;
#ifndef _STDIO_ALLOCATE
		if (fno < 2) {
			/* use special buffer for std{in,out} */
			buf = (fno == 0) ? _sibuf : _sobuf;
		} else	/* needed for ifndef */
#endif
		if (fno < _NFILE) {
			buf = _smbuf[fno];
			size = _SMBFSZ - PUSHBACK;
		} else
		if ((buf = (Uchar *)malloc(_SMBFSZ * sizeof (Uchar))) != 0) {
			iop->_flag |= _IOMYBUF;
			size = _SMBFSZ - PUSHBACK;
		}
	} else {	/* regular buffered I/O, standard buffer size */
		if (isatty(fno))
			iop->_flag |= _IOLBF;
	}
	if (buf == 0) {
		FUNLOCKFILE(lk);
		return;		/* malloc() failed */
	}
	temp = buf + PUSHBACK;
	iop->_base = temp;
	_setbufend(iop, temp + size);
	iop->_ptr = temp;
	iop->_cnt = 0;
	FUNLOCKFILE(lk);
}
