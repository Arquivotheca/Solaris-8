	.file	"strlen.s"

	.ident	"@(#)strlen.s	1.2	98/07/08 SMI"

	.globl	strlen
	.align	4

_fgdef_(strlen):
	MCOUNT
	movl	%edi,%edx	/ save register variables

	movl	4(%esp),%edi	/ string address
	xorl	%eax,%eax	/ %al = 0
	movl	$-1,%ecx	/ Start count backward from -1.
	repnz ; scab
	incl	%ecx		/ Chip pre-decrements.
	movl	%ecx,%eax	/ %eax = return values
	notl	%eax		/ Twos complement arith. rule.

	movl	%edx,%edi	/ restore register variables
	ret
	_fg_setsize_(`strlen')
