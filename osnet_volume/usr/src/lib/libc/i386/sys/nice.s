	.ident	"@(#)nice.s	1.8	98/07/08 SMI"


	.file	"nice.s"

	.text

	.globl	__cerror

_fwdef_(`nice'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$NICE,%eax
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
	_fw_setsize_(`nice')
