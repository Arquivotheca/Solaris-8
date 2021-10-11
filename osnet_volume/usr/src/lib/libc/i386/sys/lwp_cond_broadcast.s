	.ident	"@(#)lwp_cond_broadcast.s	1.9	98/07/08 SMI"

	.file	"lwp_cond_broadcast.s"

	.text

_fwdef_(`_lwp_cond_broadcast'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LWP_COND_BROADCAST,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	_fw_setsize_(`_lwp_cond_broadcast')
