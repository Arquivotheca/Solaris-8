/*
 *	Copyright (c) 1991, Sun Microsystems, Inc.
 */

#ident	"@(#)lddstub.s	1.3	92/09/25 SMI"

/*
 * Stub file for ldd(1).  Provides for preloading shared libraries.
 */
	.file	"lddstub.s"
	.set	EXIT,1
	.text	
	.globl	stub

stub:
	pushl	$0
	movl	$EXIT,%eax
	lcall	$7,$0
