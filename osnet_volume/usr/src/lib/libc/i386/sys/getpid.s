	
.ident	"@(#)getpid.s	1.3	98/07/08 SMI"

	.file	"getpid.s"

	.text


_fwdef_(`getpid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETPID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
	_fw_setsize_(`getpid')
