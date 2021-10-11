.ident	"@(#)alarm.s	1.7	98/07/08 SMI"

/ alarm 

	.file	"alarm.s"

	.text

	.globl _libc_alarm

_fgdef_(`_libc_alarm'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$ALARM,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
	_fg_setsize_(`_libc_alarm')
