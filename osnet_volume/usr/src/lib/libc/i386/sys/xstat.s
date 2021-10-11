	.ident	"@(#)xstat.s	1.8	98/07/08 SMI"

/ OS library -- _xstat

/ error = _xstat(version, string, statbuf)

	.file	"xstat.s"

	.text

	.globl	__cerror
	.globl	_xstat

_fgdef_(_xstat):
	MCOUNT
	movl	$XSTAT,%eax
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
	_fg_setsize_(`_xstat')
