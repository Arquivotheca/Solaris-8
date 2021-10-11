	.ident	"@(#)wait.s	1.10	98/07/08 SMI"


	.file	"wait.s"

	.text

_fwpdef_(`_wait', `_libc_wait'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$WAIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	wait
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	movl	4(%esp),%ecx
	testl	%ecx,%ecx
	jz	.return
	movl	%edx,(%ecx)
.return:
	ret
	_fwp_setsize_(`_wait', `_libc_wait')
