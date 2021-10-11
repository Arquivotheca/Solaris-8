/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)getw.c	1.13	97/12/06 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/
/*
 * The intent here is to provide a means to make the order of
 * bytes in an io-stream correspond to the order of the bytes
 * in the memory while doing the io a `word' at a time.
 */

#pragma weak getw = _getw

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include "shlib.h"
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

/* Read sizeof(int) characters from stream and return these in an int */
int
getw(FILE *stream)
{
	int w;
	char *s = (char *)&w;
	int i = sizeof (int);
	int ret;
	rmutex_t *lk;

	FLOCKFILE(lk, stream);
	while (--i >= 0 && !(stream->_flag & (_IOERR | _IOEOF)))
		*s++ = GETC(stream);
	ret = ((stream->_flag & (_IOERR | _IOEOF)) ? EOF : w);
	FUNLOCKFILE(lk);
	return (ret);
}
