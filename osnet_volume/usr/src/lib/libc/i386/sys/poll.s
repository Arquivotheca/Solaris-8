	.ident	"@(#)poll.s	1.11	98/07/08 SMI"

	.file	"poll.s"

	.text

	.globl	__cerror

_fwpdef_(`_poll', `_libc_poll'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$POLL,%eax
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
	_fwp_setsize_(`_poll', `_libc_poll')
