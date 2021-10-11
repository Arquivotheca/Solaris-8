/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)_filbuf.c	1.12	97/12/02 SMI"	/* SVr4.0 1.8 */

/*LINTLIBRARY*/

#pragma weak _filbuf = __filbuf

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "stdiom.h"

/* fill buffer, return first character or EOF */
int
_filbuf(FILE *iop)
{
	ssize_t res;
	size_t nbyte;
	Uchar *endbuf;

	if (!(iop->_flag & _IOREAD))	/* check, correct permissions */
	{
		if (iop->_flag & _IORW)
			iop->_flag |= _IOREAD; /* change direction */
						/* to read - fseek */
		else {
			errno = EBADF;
			return (EOF);
		}
	}

	if (iop->_base == 0) {
		if ((endbuf = _findbuf(iop)) == 0) /* get buffer and */
						/* end_of_buffer */
			return (EOF);
	}
	else
		endbuf = _bufend(iop);

	/*
	* Flush all line-buffered streams before we
	* read no-buffered or line-buffered input.
	*/
	if (iop->_flag & (_IONBF | _IOLBF))
		_flushlbf();
	/*
	* Fill buffer or read 1 byte for unbuffered, handling any errors.
	*/
	iop->_ptr = iop->_base;
	if (iop->_flag & _IONBF)
		nbyte = 1;
	else
		nbyte = endbuf - iop->_base;
	if ((res = read(iop->_file, (char *)iop->_base, nbyte)) > 0) {
		iop->_cnt = res - 1;
		return (*iop->_ptr++);
	}
	else
	{
		iop->_cnt = 0;
		if (res == 0)
			iop->_flag |= _IOEOF;
		else
			iop->_flag |= _IOERR;
		return (EOF);
	}
}
