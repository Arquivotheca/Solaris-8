	.ident	"@(#)putpmsg.s	1.10	98/07/08 SMI"

/ gid = putpmsg();
/ returns effective gid

	.file	"putpmsg.s"

	.text

	.globl  __cerror

_fwpdef_(`_putpmsg', `_libc_putpmsg'):
	MCOUNT
	movl	$PUTPMSG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_putpmsg
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fwp_setsize_(`_putpmsg', `_libc_putpmsg')
