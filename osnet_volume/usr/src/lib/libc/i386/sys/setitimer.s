	.ident	"@(#)setitimer.s	1.10	98/07/08 SMI"

	.file	"setitimer.s"

	.text

	.globl	__cerror
	.globl	_libc_setitimer

_fgdef_(`_libc_setitimer'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SETITIMER,%eax
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
	_fg_setsize_(`_libc_setitimer')
