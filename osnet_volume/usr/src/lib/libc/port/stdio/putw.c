/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)putw.c	1.12	97/12/02 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/
/*
 * The intent here is to provide a means to make the order of
 * bytes in an io-stream correspond to the order of the bytes
 * in the memory while doing the io a `word' at a time.
 */

#pragma weak putw = _putw

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include "shlib.h"
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

int
putw(int w, FILE *stream)
{
	char *s = (char *)&w;
	int i = sizeof (int);
	int ret;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	FLOCKFILE(lk, stream);
	while (--i >= 0 && PUTC(*s++, stream) != EOF)
		;
	ret = stream->_flag & _IOERR;
	FUNLOCKFILE(lk);
	return (ret);
}
