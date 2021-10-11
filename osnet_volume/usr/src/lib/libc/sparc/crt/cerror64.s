/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

.ident	"@(#)cerror64.s	1.1	93/08/05 SMI"

/*
 * cerror() for system calls that return 64-bit values.
 */

	.file	"cerror64.s"

#include "SYS.h"
#include "PIC.h"

	.global _cerror64
	.global	errno

	ENTRY(_cerror64)
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
	mov	-1, %o1
	retl
	mov	-1, %o0

	SET_SIZE(_cerror64)
