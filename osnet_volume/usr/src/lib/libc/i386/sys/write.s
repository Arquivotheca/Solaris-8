	.ident	"@(#)write.s	1.10	98/07/08 SMI"


	.file	"write.s"

	.text

_fwpdef_(`_write', `_libc_write'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$WRITE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	write
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fwp_setsize_(`_write', `_libc_write')
