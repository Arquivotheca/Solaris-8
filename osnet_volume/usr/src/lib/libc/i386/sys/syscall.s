	.ident	"@(#)syscall.s	1.13	98/07/08 SMI"


	.file	"syscall.s"

	.text

	.globl	__cerror

_fwdef_(`syscall'):
	MCOUNT			/ subroutine entry counter if profiling
	pop	%edx		/ return address.
	pop	%eax		/ system call number
	pushl	%edx
	lcall   $SYSCALL_TRAPNUM,$0
	movl	0(%esp),%edx
	pushl	%edx		/ Add an extra entry to the stack
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fw_setsize_(`syscall')
