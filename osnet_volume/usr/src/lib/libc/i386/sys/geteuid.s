.ident	"@(#)geteuid.s	1.3	98/07/08 SMI"

	.file	"geteuid.s"

	.text

_fwdef_(`geteuid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETUID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%edx,%eax
	ret
	_fw_setsize_(`geteuid')
