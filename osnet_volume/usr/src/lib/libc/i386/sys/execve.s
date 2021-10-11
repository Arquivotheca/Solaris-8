.ident	"@(#)execve.s	1.8	98/07/08 SMI"

	.file	"execve.s"

	.text

	.globl	__cerror

_fwdef_(`execve'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$EXECE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
	_fw_setsize_(`execve')
