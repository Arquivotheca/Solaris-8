	.ident	"@(#)writev.s	1.10	98/07/08 SMI"

/ OS library -- writev 

/ error = writev(fd, iovp, iovcnt)

	.file	"writev.s"

	.text

	.globl	__cerror

_fwpdef_(`_writev', `_libc_writev'):
	MCOUNT
	movl	$WRITEV,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_writev
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	ret
	_fwp_setsize_(`_writev', `_libc_writev')
