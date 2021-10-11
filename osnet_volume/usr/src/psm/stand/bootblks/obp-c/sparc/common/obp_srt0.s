/*
 * Copyright (c) 1986-1994, by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)obp_srt0.s 1.2 95/07/25 SMI"

/*
 * simple standalone startup code
 */

#include <sys/asm_linkage.h>

#if defined(lint)

#include "cbootblk.h"

/*ARGSUSED*/
void
start(void *romp)
{}

#else	/* lint */

	ENTRY(start)
	.global	end
	.global	edata
	.global	main
	!
	! OBP gives us control right here ..
	!
	! On entry, %o0 contains the romp.
	!
	sethi	%hi(start), %o1		! Top of stack
	or	%o1, %lo(start), %o1
	save	%o1, -SA(MINFRAME), %sp
	!
	! zero the bss
	!
	sethi	%hi(edata), %o0		! Beginning of bss
	or	%o0, %lo(edata), %o0
	sethi	%hi(end), %o2		! End of the whole wad
	or	%o2, %lo(end), %o2	
	call	bzero
	sub	%o2, %o0, %o1		! end - edata = size of bss
	call	main
	mov	%i0, %o0		! romvec pointer
	call	exit
	mov	0, %o0
	ret				! ret to prom
	restore
	SET_SIZE(start)

#endif	/* lint */
