.ident	"@(#)fork1.s	1.5 SMI"

/ pid = fork1();

/ %edx == 0 in parent process, %edx = 1 in child process.
/ %eax == pid of child in parent, %eax == pid of parent in child.
/ differs from fork in that lwps are not created in child.

	.file	"fork1.s"
	
	.text

	.globl	__cerror
	.globl	_libc_fork1

_fgdef_(`_libc_fork1'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$FORK1,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	testl	%edx,%edx
	jz	.parent
	xorl	%eax,%eax
.parent:
	ret
	_fg_setsize_(`_libc_fork1')
