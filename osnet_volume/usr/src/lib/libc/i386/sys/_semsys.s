.ident	"@(#)_semsys.s	1.4	98/07/08 SMI"

	.file	"_semsys.s"

	.text


	.globl	__cerror

_fgdef_(`_semsys'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SEMSYS,%eax
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
	_fg_setsize_(`_semsys')
