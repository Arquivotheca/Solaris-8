/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)errno.c	1.2 SMI"	/* SVr4.0 1.3	*/

#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#undef errno
#ifdef __STDC__
/* These two pragmas should disappear */
#pragma weak _thr_errno_addr = ___errno
#pragma weak __errno_fix = ___errno
#endif __STDC__


int *
___errno()
{
	extern int errno;
	extern int *_thr_errnop();

	if (_thr_main())
		return (&errno);
	else
		return (_thr_errnop());
}

