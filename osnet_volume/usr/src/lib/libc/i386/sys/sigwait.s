	.ident	"@(#)sigwait.s	1.7	98/07/08 SMI"

	.file	"sigwait.s"

	/ sigwait(&set) is in reality:
	/ sigtimedwait(&set, NULL, NULL)

	.text

	.globl	__cerror
	.globl	_libc_sigwait

_fgdef_(`_libc_sigwait'):
	MCOUNT			/ subroutine entry counter if profiling
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$0
	pushl	$0
	pushl   8(%ebp)		/ address of the sigset_t argument
	pushl	$0		/ dummy return address
	movl	$SIGTIMEDWAIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	addl	$16, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	addl	$16, %esp
	leave
	ret
	_fg_setsize_(`_libc_sigwait')
