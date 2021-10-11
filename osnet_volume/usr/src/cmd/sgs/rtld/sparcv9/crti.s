/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)crti.s	1.1	97/05/12 SMI"

	.file		"crti.s"
	.section	".init"
	.global		_init
	.type		_init, #function
	.align	4

#include <sys/asm_linkage.h>

_init:
	save	%sp, -SA(MINFRAME), %sp
