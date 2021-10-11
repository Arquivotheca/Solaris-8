	.ident	"@(#)__xgetRD.s	1.3	93/07/30 SMI"

/	Copyright (c) 1987 by Sun Microsystems, Inc.
/ 00 - Round to nearest or even
/ 01 - Round down
/ 10 - Round up
/ 11 - Chop
	.type	__xgetRD,@function
	.text
	.globl	__xgetRD
	.align	4
__xgetRD:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$4,%esp
	fstcw	-4(%ebp)
	movw	-4(%ebp),%ax
	shrw	$10,%ax
	andl	$0x3,%eax
	leave
	ret
	.align	4
	.size	__xgetRD,.-__xgetRD
