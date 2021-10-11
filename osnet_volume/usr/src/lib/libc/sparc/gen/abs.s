/*
 * Copyright (c) 1987 Sun Microsystems, Inc.
 */

.ident	"@(#)abs.s	1.9	92/07/14 SMI"	/* SVr4.0 1.5	*/

	.file	"abs.s"

#include <sys/asm_linkage.h>

/*
 * int abs(register int arg);
 * long labs(register long int arg);
 */
	ENTRY2(abs,labs)
	tst	%o0
	bl,a	1f
	neg	%o0
1:
	retl
	nop

	SET_SIZE(abs)
	SET_SIZE(labs)
