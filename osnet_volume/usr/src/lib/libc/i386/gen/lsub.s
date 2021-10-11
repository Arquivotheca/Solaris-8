
/ Double long subtraction routine.

	.ident	"@(#)lsub.s	1.2	98/07/08 SMI"
	.file	"lsub.s"
	.text

	.set	lop,4
	.set	rop,12
	.set	ans,0

_fwdef_(`lsub'):
	popl	%eax
	xchgl	%eax,0(%esp)

	MCOUNT

	movl	lop(%esp),%ecx
	subl	rop(%esp),%ecx
	movl	lop+4(%esp),%edx
	sbbl	rop+4(%esp),%edx
	movl	%ecx,ans(%eax)
	movl	%edx,ans+4(%eax)

	ret
	_fw_setsize_(`lsub')
