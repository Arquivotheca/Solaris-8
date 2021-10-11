	.ident	"@(#)getmsg.s	1.10	98/07/08 SMI"

	.file	"getmsg.s"

	.text

	.globl	__cerror

_fwpdef_(`_getmsg', `_libc_getmsg'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETMSG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_getmsg
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fwp_setsize_(`_getmsg', `_libc_getmsg')
