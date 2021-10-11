	.ident	"@(#)pause.s	1.9	98/07/08 SMI"

	.file	"pause.s"

	.text

	.globl	__cerror

_fwpdef_(`_pause', `_libc_pause'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PAUSE,%eax
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
	_fwp_setsize_(`_pause', `_libc_pause')
