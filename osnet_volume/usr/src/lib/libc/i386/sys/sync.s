.ident	"@(#)sync.s	1.3	98/07/08 SMI"


	.file	"sync.s"
	
	.text

_fwdef_(`sync'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SYNC,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
	_fw_setsize_(`sync')
