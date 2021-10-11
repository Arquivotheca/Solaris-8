/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)crti.s	1.1	94/11/09 SMI"

	.file		"crti.s"
	.section	".init"
	.global		_init
	.type		_init, #function
	.align	4

_init:
	save	%sp, -96, %sp
