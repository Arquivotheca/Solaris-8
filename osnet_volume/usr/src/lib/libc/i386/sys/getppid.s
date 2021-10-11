.ident	"@(#)getppid.s	1.3	98/07/08 SMI"

	.file	"getppid.s"

	.text

_fwdef_(`getppid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETPID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%edx,%eax
	ret
	_fw_setsize_(`getppid')
