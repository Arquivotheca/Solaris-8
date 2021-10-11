/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

.ident	"@(#)cerror64.s	1.2	98/07/08 SMI"

	.file	"cerror64.s"

/ C return sequence which sets errno, returns -1.
/ This code should only be called by system calls which have done the prologue

	.globl	__cerror64
	.globl	errno

_fgdef_(__cerror64):
_m4_ifdef_(`DSHLIB',
`	cmpl	$ERESTART, (%esp)
	jne	1f
	movl	$EINTR, (%esp)
1:
',
`	cmpl	$ERESTART, %eax
	jne	1f
	movl	$EINTR, %eax
1:
	pushl	%eax
')
	call	_fref_(___errno)
	popl	%ecx
	movl	%ecx, (%eax)
	movl	$-1, %eax
	movl    $-1, %edx
	_epilogue_
	ret
	_fg_setsize_(`__cerror64')
