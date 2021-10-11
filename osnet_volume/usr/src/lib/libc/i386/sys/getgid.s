.ident	"@(#)getgid.s	1.3	98/07/08 SMI"

	.file	"getgid.s"

	.text

_fwdef_(`getgid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETGID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
	_fw_setsize_(`getgid')
