/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_flsbuf.c	1.13	97/12/02 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#pragma weak _flsbuf = __flsbuf
#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include <mtlib.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <unistd.h>
#include <sys/types.h>
#include "stdiom.h"

int
_flsbuf(int ch, FILE *iop)	/* flush (write) buffer, save ch, */
				/* return EOF on failure */
{
	Uchar uch;

	do	/* only loop if need to use _wrtchk() on non-_IOFBF */
	{
		switch (iop->_flag & (_IOFBF | _IOLBF | _IONBF |
				_IOWRT | _IOEOF))
		{
		case _IOFBF | _IOWRT:	/* okay to do full-buffered case */
			if (iop->_base != 0 && iop->_ptr > iop->_base)
				goto flush_putc;	/* skip _wrtchk() */
			break;
		case _IOLBF | _IOWRT:	/* okay to do line-buffered case */
			if (iop->_ptr >= _bufend(iop))
				/*
				 * which will recursively call
				 * __flsbuf via putc because of no room
				 * in the buffer for the character
				 */
				goto flush_putc;
			if ((*iop->_ptr++ = (unsigned char)ch) == '\n')
				(void) _xflsbuf(iop);
			iop->_cnt = 0;
			goto out;
		case _IONBF | _IOWRT:	/* okay to do no-buffered case */
			iop->_cnt = 0;
			uch = (unsigned char)ch;
			if (write(iop->_file, (char *)&uch, 1) != 1)
				iop->_flag |= _IOERR;
			goto out;
		}
		if (_wrtchk(iop) != 0)	/* check, correct permissions */
			return (EOF);
	} while (iop->_flag & (_IOLBF | _IONBF));
flush_putc:;
	(void) _xflsbuf(iop);
	(void) PUTC(ch, iop); /*  recursive call */
out:;
		/* necessary for putc() */
	return ((iop->_flag & _IOERR) ? EOF : (unsigned char)ch);
}
