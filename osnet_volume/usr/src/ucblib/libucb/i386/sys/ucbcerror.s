	.file	"ucbcerror.s"

	.ident	"@(#)ucbcerror.s	1.1	96/03/28 SMI"

/ C return sequence which sets errno, returns -1.
/ This code should only be called by system calls which have done the prologue

	.globl	__ucbcerror

_fgdef_(__ucbcerror):
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
