/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fopen.c	1.22	98/04/29 SMI"	/* SVr4.0 1.20 */

/*LINTLIBRARY*/

#include "file64.h"
#include <sys/types.h>
#include <stdio.h>
#include <mtlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"

/* Final argument to _endopen depends on build environment */
#define	LARGE_OPEN		(_FILE_OFFSET_BITS == 64)

FILE *
fopen(const char *name, const char *type) /* open name, return new stream */
{
#ifdef _REENTRANT


	FILE *iop;
	FILE  *rc;

	iop = _findiop();
	/*
	* Note that iop is not locked here, since no other thread could
	* possibly call _endopen with the same iop at this point.
	*/
	rc = _endopen(name, type, iop, LARGE_OPEN);

	if (rc == NULL && iop != NULL)
		iop->_flag = 0; /* release iop */

	return (rc);
#else
	return (_endopen(name, type, _findiop(), LARGE_OPEN));
#endif _REENTRANT
}

FILE *
freopen(const char *name, const char *type, FILE *iop)
{
#ifdef _REENTRANT

	FILE *rc;
	rmutex_t *lk;

	/*
	 * there may be concurrent calls to reopen the same stream - need
	 * to make freopen() atomic
	 */
	FLOCKFILE(lk, iop);
	/*
	 * new function to do everything that fclose() does, except
	 * to release the iop - this cannot yet be released since
	 * _endopen() is yet to be called on this iop
	 */

	(void) close_fd(iop);

	rc = _endopen(name, type, iop, LARGE_OPEN);

	if (rc == 0)
		iop->_flag = 0; /* release iop */

	FUNLOCKFILE(lk);
	return (rc);
#else
	(void) fclose(iop);
	return (_endopen(name, type, iop, LARGE_OPEN));
#endif _REENTRANT
}
