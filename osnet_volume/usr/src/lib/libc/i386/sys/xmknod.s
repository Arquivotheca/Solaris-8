	.ident	"@(#)xmknod.s	1.8	98/07/08 SMI"

/ OS library -- _xmknod

/ error = _xmknod(version, string, mode, dev)

	.file	"xmknod.s"

	.text

	.globl	__cerror
	.globl	_xmknod

_fgdef_(`_xmknod'):
	MCOUNT
	movl	$XMKNOD,%eax
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
	_fg_setsize_(`_xmknod')
