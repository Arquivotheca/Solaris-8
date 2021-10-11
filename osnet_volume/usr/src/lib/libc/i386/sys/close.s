.ident	"@(#)close.s	1.9	98/07/08 SMI"
	
	.file	"close.s"

	.text

	.globl	__cerror

_fwpdef_(`_close', `_libc_close'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$CLOSE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
	_fwp_setsize_(`_close', `_libc_close')
