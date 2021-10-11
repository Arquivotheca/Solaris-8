
/ Double long add routine.

	.ident	"@(#)ladd.s	1.2	98/07/08 SMI"
	.file	"ladd.s"
	.text

	.set	lop,4
	.set	rop,12
	.set	ans,0

_fwdef_(`ladd'):
	popl	%eax
	xchgl	%eax,0(%esp)

	MCOUNT

	movl	lop(%esp),%ecx
	addl	rop(%esp),%ecx
	movl	lop+4(%esp),%edx
	adcl	rop+4(%esp),%edx
	movl	%ecx,ans(%eax)
	movl	%edx,ans+4(%eax)

	ret
	_fw_setsize_(`ladd')
