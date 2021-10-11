/*
 * Copyright (c) 1991-1994, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)exitto.s	1.2	94/11/29 SMI"

#include <sys/asm_linkage.h>

#if defined(lint)

#include "cbootblk.h"

/*ARGSUSED*/
void
exitto(void *goto_address, void *firmware_handle)
{}

#else	/* lint */

	ENTRY(exitto)
	save	%sp, -SA(MINFRAME), %sp
	mov	%i1, %o0
	call	%i0
	clr	%o1			! bootvec (0 for bootblock)
	/*NOTREACHED*/
	SET_SIZE(exitto)

#endif	/* lint */
