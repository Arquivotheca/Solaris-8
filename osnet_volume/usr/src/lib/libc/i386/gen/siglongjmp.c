/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)siglongjmp.c	1.3	96/03/08 SMI"

#include "synonyms.h"
#include <sys/types.h>
#include <sys/ucontext.h>

#include <setjmp.h>

void
_libc_siglongjmp(env, val)
sigjmp_buf env;
int val;
{
	register ucontext_t *ucp = (ucontext_t *)env;
	if (val)
		ucp->uc_mcontext.gregs[ EAX ] = val;
	setcontext(ucp);
}
