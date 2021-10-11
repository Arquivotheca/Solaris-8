	.ident	"@(#)processor_bind.s	1.2	98/07/08 SMI"


	.file	"processor_bind.s"

	.text

	.globl	__cerror

_fwdef_(`processor_bind'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PROCESSOR_BIND,%eax
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
	_fw_setsize_(`processor_bind')
