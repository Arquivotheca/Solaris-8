/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)makectxt.c	1.11	96/09/16 SMI"	/* SVr4.0 1.4	*/

#pragma weak makecontext = _makecontext

#include "synonyms.h"
#include <stdarg.h>
#include <ucontext.h>
#include <sys/stack.h>
#include <sys/frame.h>

void resumecontext();

void
makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	greg_t *reg;
	long *tsp;
	char *sp;
	int argno;
	va_list ap;

	reg = ucp->uc_mcontext.gregs;
	reg[REG_PC] = (greg_t)func;
	reg[REG_nPC] = reg[REG_PC] + 0x4;

	sp = ucp->uc_stack.ss_sp;
	*(int *)sp = (int)ucp->uc_link;		/* save uc_link */

	/*
	 * reserve enough space for argc, reg save area, uc_link,
	 * and "hidden" arg;  rounding to stack alignment
	 */
	sp -= SA((argc + 16 + 1 + 1) * sizeof (int));

	va_start(ap, argc);

	/*
	 * Copy all args to the alt stack,
	 * also copy the first 6 args to .gregs
	 */
	argno = 0;
	tsp = ((struct frame *)sp)->fr_argd;

	while (argno < argc) {
		if (argno < 6)
			*tsp++ = reg[REG_O0 + argno] = va_arg(ap, int);
		else
			*tsp++ = va_arg(ap, int);
		argno++;
	}

	va_end(ap);

	ucp->uc_stack.ss_sp = sp;
	reg[REG_O6] = (greg_t)sp;			/* sp (when done) */
	reg[REG_O7] = (greg_t)resumecontext - 8;	/* return pc */
}

void
resumecontext(void)
{
	ucontext_t uc;

	getcontext(&uc);
	setcontext(uc.uc_link);
}
