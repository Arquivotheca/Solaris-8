/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)swapctxt.c	1.5	97/05/16 SMI"

/*LINTLIBRARY*/

#pragma weak swapcontext = _swapcontext

#include "synonyms.h"

#define _getcontext __getcontext	/* to get prototype right */

#include <sys/types.h>
#include <sys/stack.h>
#include <ucontext.h>
#include <stdlib.h>
#include "libc.h"


int
swapcontext(ucontext_t *oucp, const ucontext_t *nucp)
{
	greg_t *reg;
	greg_t *sp;

	if (__getcontext(oucp))
		return (-1);

	/*
	 * Note that %o1 and %g1 are modified by the system call
	 * routine. ABI calling conventions specify that the caller
	 * can not depend upon %o0 thru %o5 nor g1, so no effort is
	 * made to maintain these registers. %o0 is forced to reflect
	 * an affirmative return code.
	 */
	reg = oucp->uc_mcontext.gregs;
	sp = (greg_t *)((caddr_t)_getsp() + STACK_BIAS);
	reg[REG_PC] = *(sp + 15) + 0x8;
	reg[REG_nPC] = reg[REG_PC] + 0x4;
	reg[REG_O0] = 0;
	reg[REG_SP] = *(sp + 14);
	reg[REG_O7] = *(sp + 15);

	return (setcontext((ucontext_t *)nucp));
}
