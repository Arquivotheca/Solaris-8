/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)gets.c	1.15	99/11/03 SMI"	/* SVr4.0 3.13 */

/*LINTLIBRARY*/
#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <memory.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include "stdiom.h"
#include "mse.h"

/* read a single line from stdin, replace the '\n' with '\0' */
char *
gets(char *buf)
{
	char *ptr = buf;
	ssize_t n;
	char *p;
	Uchar *bufend;
	rmutex_t *lk;

	FLOCKFILE(lk, stdin);

	_SET_ORIENTATION_BYTE(stdin);

	if (!(stdin->_flag & (_IOREAD | _IORW))) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (0);
	}

	if (stdin->_base == NULL) {
		if ((bufend = _findbuf(stdin)) == 0) {
			FUNLOCKFILE(lk);
			return (0);
		}
	}
	else
		bufend = _bufend(stdin);

	for (;;)	/* until get a '\n' */
	{
		if (stdin->_cnt <= 0)	/* empty buffer */
		{
			if (_filbuf(stdin) != EOF) {
				stdin->_ptr--;	/* put back the character */
				stdin->_cnt++;
			} else if (ptr == buf) {  /* never read anything */
				FUNLOCKFILE(lk);
				return (0);
			} else
				break;		/* nothing left to read */
		}
		n = stdin->_cnt;
		if ((p = (char *)memccpy(ptr, (char *)stdin->_ptr, '\n',
		    (size_t)n)) != 0)
			n = p - ptr;
		ptr += n;
		stdin->_cnt -= n;
		stdin->_ptr += n;
		if (_needsync(stdin, bufend))
			_bufsync(stdin, bufend);
		if (p != 0) /* found a '\n' */
		{
			ptr--;	/* step back over the '\n' */
			break;
		}
	}
	*ptr = '\0';
	FUNLOCKFILE(lk);
	return (buf);
}
