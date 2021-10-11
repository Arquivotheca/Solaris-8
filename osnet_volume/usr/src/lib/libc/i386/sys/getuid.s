.ident	"@(#)getuid.s	1.3	98/07/08 SMI"

	.file	"getuid.s"

	.text

_fwdef_(`getuid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETUID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
	_fw_setsize_(`getuid')
