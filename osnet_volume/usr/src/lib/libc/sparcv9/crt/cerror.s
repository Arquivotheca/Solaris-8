/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1986, 1989 by Sun Microsystems, Inc.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)cerror.s	1.5	97/04/25 SMI"

	.file	"cerror.s"

#include "SYS.h"
#include "PIC.h"

	.global	errno

	ENTRY(_cerror)
	cmp	%o0, ERESTART
	be,a,pn	%icc, 1f
	mov	EINTR, %o0
1:
#ifdef _REENTRANT
	save	%sp, -SA(MINFRAME), %sp
	call	___errno
	nop
	st	%i0, [%o0]
	restore
#else	/* Not _REENTRANT */
#ifdef PIC
	PIC_SETUP(o5)
	sethi	%hi(errno), %g5
	or	%g5, %lo(errno), %g5
	ldn	[%o5 + %g5], %g1	/* Calculate address of errno */
	st	%o0, [%g1]		/* errno = (int)trap_return_value */
#else
	setnhi	errno, %g1, %o5
	st	%o0, [%o5 + %lo(errno)]	/* errno = (int)trap_return_value */
#endif	/* PIC */
#endif	/* _REENTRANT */
	retl				/* return (-1l) */
	sub	%g0, 1, %o0

	SET_SIZE(_cerror)
