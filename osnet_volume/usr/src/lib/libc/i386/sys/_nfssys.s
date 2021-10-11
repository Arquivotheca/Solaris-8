.ident	"@(#)_nfssys.s	1.8	98/07/08 SMI"

/ _nfssys function


	.file	"_nfssys.s"

	.text

	.globl  __cerror
	.globl	_nfssys

_fgdef_(_nfssys):
	MCOUNT
	movl	$NFSSYS,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
	_fg_setsize_(`_nfssys')
