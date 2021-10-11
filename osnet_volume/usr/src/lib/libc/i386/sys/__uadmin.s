	.ident	"@(#)__uadmin.s	1.11	98/07/08 SMI"


	.file	"__uadmin.s"

	.text

	.globl	__uadmin
	.globl	__cerror

_fgdef_(`__uadmin'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$UADMIN,%eax
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
	_fg_setsize_(`__uadmin')
