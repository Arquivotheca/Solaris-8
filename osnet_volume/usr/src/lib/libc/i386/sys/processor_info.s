	.ident	"@(#)processor_info.s	1.2	98/07/08 SMI"


	.file	"processor_info.s"

	.text

	.globl	__cerror

_fwdef_(`processor_info'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PROCESSOR_INFO,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fw_setsize_(`processor_info')
