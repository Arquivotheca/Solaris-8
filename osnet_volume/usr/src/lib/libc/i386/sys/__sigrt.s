	.ident	"@(#)__sigrt.s	1.6	98/07/08 SMI"

	.file	"__sigrt.s"

	.text

	.globl	_libc_sigtimedwait
	.globl	__sigqueue
	.globl	__cerror

_fgdef_(`_libc_sigtimedwait'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SIGTIMEDWAIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
	_fg_setsize_(`_libc_sigtimedwait')


_fgdef_(`__sigqueue'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SIGQUEUE,%eax
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
	_fg_setsize_(`__sigqueue')
