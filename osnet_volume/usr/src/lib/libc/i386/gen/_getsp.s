	.file	"_getsp.s"

	.ident	"@(#)_getsp.s	1.4	98/07/08 SMI"

	.text
	.globl	_getsp
	.globl	_getfp
	.globl	_getap
	.globl	_getbx
	.align	4

_fgdef_(_getsp):
	MCOUNT
	movl	%esp,%eax
	ret
	_fg_setsize_(`_getsp')

_fgdef_(_getfp):
	MCOUNT
	movl	%ebp,%eax
	ret
	_fg_setsize_(`_getfp')

_fgdef_(_getap):
	MCOUNT
	leal	8(%ebp),%eax
	ret
	_fg_setsize_(`_getap')

_fgdef_(_getbx):
	MCOUNT
	movl	4(%esp),%eax
	ret
	_fg_setsize_(`_getbx')
