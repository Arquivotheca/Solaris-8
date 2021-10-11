/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 */

#pragma	ident "@(#)endsrt0.s	1.1	96/04/17 SMI"

/*
 * endsrt0.s - stub file to generate _endsrt0 label.
 * Multiple files can be loaded into lowtext as long
 * as this file is loaded last (see mapfile).
 * 
 */

#if defined(lint)

char _endsrt0[1];

#else
	.align	4
	.globl	_endsrt0
_endsrt0:

#endif /* !defined(lint) */
