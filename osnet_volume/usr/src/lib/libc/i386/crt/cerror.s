/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

.ident	"@(#)cerror.s	1.10	98/07/08 SMI"

	.file	"cerror.s"

/ C return sequence which sets errno, returns -1.
/ This code should only be called by system calls which have done the prologue

	.globl	__cerror
	.globl	errno

_fgdef_(__cerror):
_m4_ifdef_(`DSHLIB',
`	cmpl	$ERESTART,(%esp)
	jne	1f
	movl	$EINTR,(%esp)
1:
',
`	cmpl	$ERESTART,%eax
	jne	1f
	movl	$EINTR,%eax
1:
	pushl	%eax
')
	call	_fref_(___errno)
	popl	%ecx
	movl	%ecx,(%eax)
	movl	$-1,%eax
	_epilogue_
	ret
	_fg_setsize_(`__cerror')

	.globl	_set_errno

_fgdef_(_set_errno):
	_prologue_
	call	_fref_(___errno)
	movl	_esp_(4),%ecx
	movl	%ecx,(%eax)
	_epilogue_
	ret
	_fg_setsize_(`_set_errno')
