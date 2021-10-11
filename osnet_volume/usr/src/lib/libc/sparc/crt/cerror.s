/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1986, 1989 by Sun Microsystems, Inc.	*/

.ident	"@(#)cerror.s	1.17	92/09/05 SMI"	/* SVr4.0 1.6	*/

	.file	"cerror.s"

#include "SYS.h"
#include "PIC.h"

	.global _cerror
	.global	errno

	ENTRY(_cerror)
	cmp	%o0, ERESTART
	be,a	1f
	mov	EINTR, %o0
1:
#ifdef _REENTRANT
	save	%sp, -SA(MINFRAME), %sp
	call	___errno
	nop
	st	%i0, [%o0]
	restore
#else /* Not _REENTRANT */
#ifdef PIC
	PIC_SETUP(o5)
	ld	[%o5 + errno], %g1
	st	%o0, [%g1]
#else
	sethi	%hi(errno), %g1
	st	%o0, [%g1 + %lo(errno)]
#endif PIC
#endif _REENTRANT
	retl
	mov	-1, %o0

	SET_SIZE(_cerror)
