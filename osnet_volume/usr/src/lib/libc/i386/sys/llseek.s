	.ident	"@(#)llseek.s	1.9	98/07/08 SMI"

	.file	"llseek.s"

	.text

	.globl	__cerror64

_fwdef_(`llseek'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LLSEEK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror64)
noerror64:
	ret
	_fw_setsize_(`llseek')
