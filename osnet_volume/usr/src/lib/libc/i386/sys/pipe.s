	.ident	"@(#)pipe.s	1.8	98/07/08 SMI"

	.file	"pipe.s"

	.text

	.globl	__cerror

_fwdef_(`pipe'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PIPE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	movl	4(%esp),%ecx
	movl	%eax,(%ecx)
	movl	%edx,4(%ecx)
	xorl	%eax,%eax
	ret
	_fw_setsize_(`pipe')
