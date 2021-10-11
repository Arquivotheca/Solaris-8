	.ident	"@(#)lwp_cond_signal.s	1.9	98/07/08 SMI"

	.file	"lwp_cond_signal.s"

	.text

	.globl	__cerror

_fwdef_(`_lwp_cond_signal'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LWP_COND_SIGNAL,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	_fw_setsize_(`_lwp_cond_signal')
