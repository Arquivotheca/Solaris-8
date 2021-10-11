/*
 * Copyright (c) 1997, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)setbuffer.c	1.4	98/07/29 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include "shlib.h"
#include <sys/types.h>
#include "file64.h"
#include <stdio.h>
#include "stdiom.h"
#include <stdlib.h>

extern Uchar _smbuf[][_NFILE];	/* shlib.h has macro for this! */

void
setbuffer(FILE *iop, char *abuf, int asize)
{
	Uchar *buf = (Uchar *)abuf;
	int fno = iop->_file;  /* file number */
	int size = asize - _SMBFSZ;
	Uchar *temp;

	if (iop->_base != 0 && iop->_flag & _IOMYBUF)
		free((char *)iop->_base - PUSHBACK);
	iop->_flag &= ~(_IOMYBUF | _IONBF | _IOLBF);
	if (buf == 0) {
		iop->_flag |= _IONBF;
#ifndef _STDIO_ALLOCATE
		if (fno < 2) {
			/* use special buffer for std{in,out} */
			buf = (fno == 0) ? _sibuf : _sobuf;
			size = BUFSIZ - _SMBFSZ;
		} else /* needed for ifdef */
#endif
		if (fno < _NFILE) {
			buf = _smbuf[fno];
			size = _SMBFSZ - PUSHBACK;
		} else if ((buf = (Uchar *)malloc(_SMBFSZ * sizeof (Uchar))) !=
		    0) {
			iop->_flag |= _IOMYBUF;
			size = _SMBFSZ - PUSHBACK;
		}
	} else /* regular buffered I/O, specified buffer size */ {
		if (size <= 0)
			return;
	}
	if (buf == 0)
		return; /* malloc() failed */
	temp = buf + PUSHBACK;
	iop->_base = temp;
	_setbufend(iop, temp + size);
	iop->_ptr = temp;
	iop->_cnt = 0;
}

/*
 * set line buffering
 */

int
setlinebuf(FILE *iop)
{
	char *buf;

	(void) fflush(iop);
	setbuffer(iop, (char *)NULL, 0);
	buf = (char *)malloc(128);
	if (buf != NULL) {
		setbuffer(iop, buf, 128);
		iop->_flag |= _IOLBF|_IOMYBUF;
	}
	return (0);	/* returns no useful value, keep the same prototype */
}
