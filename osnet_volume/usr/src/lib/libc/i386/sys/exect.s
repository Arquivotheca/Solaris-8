	.file	"exect.s"

	.ident	"@(#)exect.s	1.8	98/07/08 SMI"

/ this is the same as execve described below.
/ It sets single step prior to exec call,
/ this will stop the user on the first instruction executed
/ and allow the parent to set break points as appropriate.
/ This is used by tracing mechanisms,such as sdb.
/ execve(path, argv, envp);
/ char	*path, *argv[], *envp[];

_m4_define_(`PS_T', 0x0100)

	.globl	__cerror

_fwdef_(`exect'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$EXECE,%eax

/ set the single step flag bit (trap flag)
	pushf
	popl	%edx
	orl	$PS_T,%edx
	pushl	%edx
	popf
/ this has now set single step which should be preserved by the system
	lcall   $SYSCALL_TRAPNUM,$0 / call gate into OS
	_prologue_
	jmp	_fref_(__cerror)	/ came back
	_fw_setsize_(`exect')
