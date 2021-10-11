	.ident	"@(#)stime.s	1.8	98/07/08 SMI"


	.file	"stime.s"

	.text

	.globl	__cerror

_fwdef_(`stime'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	4(%esp),%eax	/ Move it to a safe location before
	movl	(%eax),%eax	/ getting privileged.
	movl	%eax,4(%esp)
	movl	$STIME,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
	_fw_setsize_(`stime')
