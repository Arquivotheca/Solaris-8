	.ident	"@(#)kaio.s	1.3	98/07/08 SMI"

	.file	"kaio.s"

	.text

	.globl	__cerror

_fwdef_(`kaio'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$KAIO,%eax
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
	_fw_setsize_(`kaio')
