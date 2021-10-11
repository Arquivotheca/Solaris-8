	
.ident	"@(#)yield.s	1.3 SMI"

	.file	"yield.s"

	.text


_fwdef_(`yield'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$YIELD,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
	_fw_setsize_(`yield')
