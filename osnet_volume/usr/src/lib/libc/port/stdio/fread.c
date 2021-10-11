/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident  "@(#)fread.c 1.20	99/11/03 SMI"		/* SVr4.0 3.29 */

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <stddef.h>
#include <values.h>
#include <memory.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <sys/types.h>
#include "stdiom.h"
#include "mse.h"

size_t
fread(void *ptr, size_t size, size_t count, FILE *iop)
{
	ssize_t s;
	int c;
	char * dptr = (char *)ptr;
	rmutex_t *lk;

	FLOCKFILE(lk, iop);

	_SET_ORIENTATION_BYTE(iop);

	/* is it a readable stream */
	if (!(iop->_flag & (_IOREAD | _IORW))) {
		iop->_flag |= _IOERR;
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (0);
	}

	if (iop->_flag & _IOEOF) {
		FUNLOCKFILE(lk);
		return (0);
	}

	/* These checks are here to avoid the multiply */
	if (count == 1)
		s = size;
	else if (size == 1)
		s = count;
	else
		s = size * count;

	while (s > 0) {
		if (iop->_cnt < s) {
			if (iop->_cnt > 0) {
				(void) memcpy((void*)dptr, iop->_ptr,
				    iop->_cnt);
				dptr += iop->_cnt;
				s -= iop->_cnt;
			}
			/*
			 * filbuf clobbers _cnt & _ptr,
			 * so don't waste time setting them.
			 */
			if ((c = _filbuf(iop)) == EOF)
				break;
			*dptr++ = (char)c;
			s--;
		}
		if (iop->_cnt >= s) {
			char * tmp = (char *)iop->_ptr;
			switch (s) {
			case 8:
				*dptr++ = *tmp++;
				/*FALLTHRU*/
			case 7:
				*dptr++ = *tmp++;
				/*FALLTHRU*/
			case 6:
				*dptr++ = *tmp++;
				/*FALLTHRU*/
			case 5:
				*dptr++ = *tmp++;
				/*FALLTHRU*/
			case 4:
				*dptr++ = *tmp++;
				/*FALLTHRU*/
			case 3:
				*dptr++ = *tmp++;
				/*FALLTHRU*/
			case 2:
				*dptr++ = *tmp++;
				/*FALLTHRU*/
			case 1:
				*dptr++ = *tmp++;
				break;
			default:
				(void) memcpy((void*)dptr, iop->_ptr,
				    (size_t)s);
			}
			iop->_ptr += s;
			iop->_cnt -= s;
			FUNLOCKFILE(lk);
			return (count);
		}
	}
	FUNLOCKFILE(lk);
	return (size != 0 ? count - ((s + size - 1) / size) : 0);
}
