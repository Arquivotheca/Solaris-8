/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)errno.c	1.10	97/08/08 SMI"	/* SVr4.0 1.3	*/

#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include "libc.h"
#undef errno

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
