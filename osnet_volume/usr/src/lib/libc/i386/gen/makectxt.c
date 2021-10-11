/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)makectxt.c	1.4	97/02/12 SMI"	/* SVr4.0 1.4	*/

#ifdef __STDC__
	#pragma weak makecontext = _makecontext
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/user.h>
#include <sys/ucontext.h>

void
#ifdef	__STDC__

makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)

#else

makecontext(ucp, func, argc)
ucontext_t *ucp; void (*func)(); int argc; 

#endif
{
	int *sp;
	int *argp;
	static void resumecontext();

	ucp->uc_mcontext.gregs[ EIP ] = (ulong)func;

	sp = ((int *)((char *)(ucp->uc_stack.ss_sp) + ucp->uc_stack.ss_size));
	*--sp = (int)(ucp->uc_link);


	argp = ((int *)&argc) + argc;
	while (argc-- > 0)
		*--sp = *argp--;
	
	*--sp = (int)resumecontext;		/* return address */

	ucp->uc_mcontext.gregs[ UESP ] = (ulong)sp;
}


static void
resumecontext()
{
	ucontext_t uc;

	uc.uc_flags = UC_ALL;
	getcontext(&uc);
	setcontext(uc.uc_link);
}
