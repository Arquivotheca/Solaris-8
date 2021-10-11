/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * NOTE!
 * This code was copied from usr/src/lib/libc/i386/gen/siglongjmp.c
 */

#pragma ident	"@(#)longjmp.c	1.3	97/06/18 SMI"

#include <ucontext.h>
#include <setjmp.h>

void
longjmp(jmp_buf env, int val)
{
	ucontext_t *ucp = (ucontext_t *)env;

	if (val)
		ucp->uc_mcontext.gregs[ EAX ] = val;
	setcontext(ucp);
}
