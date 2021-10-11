	.ident	"@(#)read.s	1.10	98/07/08 SMI"

	.file	"read.s"

	.text

	.globl	__cerror

_fwpdef_(`_read', `_libc_read'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$READ,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	read
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fwp_setsize_(`_read', `_libc_read')
