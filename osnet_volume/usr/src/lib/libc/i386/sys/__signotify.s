	.ident	"@(#)__signotify.s	1.3	98/07/08 SMI"

	.file	"__signotify.s"

	.text

	.globl	__signotify
	.globl	__cerror

_fgdef_(`__signotify'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SIGNOTIFY,%eax
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
	_fg_setsize_(`__signotify')
