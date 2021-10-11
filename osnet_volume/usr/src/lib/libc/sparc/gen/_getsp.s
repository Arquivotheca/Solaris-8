
/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)_getsp.s	1.3	92/07/14 SMI"

	.file	"_getsp.s"

#include <sys/asm_linkage.h>

/*
 * Return the stack pointer
 */
ENTRY(_getsp)
	retl
	add	%g0,%sp,%o0
	SET_SIZE(_getsp)
