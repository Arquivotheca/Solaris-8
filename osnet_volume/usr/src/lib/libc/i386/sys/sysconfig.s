	.ident	"@(#)sysconfig.s	1.12	98/07/08 SMI"

	.file	"sysconfig.s"

	.text

	.globl	__cerror
	.globl	_sysconfig

_fgdef_(`_sysconfig'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SYSCONFIG,%eax
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
	_fg_setsize_(`_sysconfig')
