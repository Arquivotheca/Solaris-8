	.ident	"@(#)lxstat.s	1.8	98/07/08 SMI"

/ gid = _lxstat();
/ returns effective gid

	.file	"lxstat.s"

	.text

	.globl  __cerror
	.globl  _lxstat

_fgdef_(`_lxstat'):
	MCOUNT
	movl	$LXSTAT,%eax
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
	_fg_setsize_(`_lxstat')
