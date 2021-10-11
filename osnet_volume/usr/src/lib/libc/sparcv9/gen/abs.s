/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 */

.ident	"@(#)abs.s	1.10	97/02/09 SMI"

	.file	"abs.s"

#include <sys/asm_linkage.h>

/*
 * int abs(register int arg);
 */
	ENTRY(abs)
	cmp	%o0, 0
	bneg,a	%icc, .done
	neg %o0
.done:
	retl
	nop

	SET_SIZE(abs)
