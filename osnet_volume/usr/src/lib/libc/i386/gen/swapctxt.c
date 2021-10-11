/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)swapctxt.c	1.6	95/03/15 SMI"	/* SVr4.0 1.3	*/

#ifdef __STDC__
	#pragma weak swapcontext = _swapcontext
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/user.h>
#include <sys/ucontext.h>
/* #include <stdlib.h> */

#ifdef notdef
asm int *
_getfp()
{
	leal	0(%ebp),%eax
}

asm int *
_getap()
{
	leal	8(%ebp),%eax
}
#else
int *_getfp(), *_getap();
#endif

int
swapcontext(ucontext_t *oucp, const ucontext_t *nucp)
{
	register greg_t *cpup;
	int rval;
#if defined(PIC)
	greg_t oldebx = (greg_t) _getbx();
#endif /* defined(PIC) */

	if (rval = __getcontext(oucp))
		return rval;

	cpup = (greg_t *)&oucp->uc_mcontext.gregs;
	cpup[ EBP ] = *((greg_t *)_getfp()); /* get old ebp off stack */
	cpup[ EIP ] = *((greg_t *)_getfp()+1); /* get old eip off stack */
	cpup[ UESP ] = (greg_t)_getap();	/* get old esp off stack */
#if defined(PIC)
	cpup[ EBX ] = oldebx;			/* get old ebx off stack */
#endif /* defined(PIC) */
	cpup[ EAX ] = 0;			/* so that swapcontext
						 * returns 0 on success */

	return setcontext((ucontext_t *)nucp);		/* setcontext only returns
						 * on failure */
}
