	.ident	"@(#)uname.s	1.6	98/07/08 SMI"


	.file	"uname.s"

	.text

	.globl	errno
	.set	UNAME,0

_fwdef_(`uname'):
	MCOUNT			/ subroutine entry counter if profiling
	pushl	$UNAME		/ type
	pushl	$0		/ mv flag
	pushl	12(%esp)	/ utsname address (retaddr+$UNAME+0)
	subl	$4,%esp		/ where return address would be.
	movl	$UTSSYS,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jc	.cerror
	addl	$16,%esp
	ret

.cerror:
	_prologue_
	pushl	%eax
	call	_fref_(_set_errno)
	movl	$-1,%eax
	_epilogue_
	addl	$16,%esp
	ret
	_fw_setsize_(`uname')
