/ Copyright (c) 1996 by Sun Microsystems, Inc.
/ All rights reserved.

.ident "@(#)_pset.s	1.3	98/07/08 SMI"

/ int
/ _pset(int subcode, long arg1, long arg2, long arg3, long arg4)
/
/ System call entry point for pset_create, pset_assign, pset_destroy,
/ pset_bind, and pset_info.

	.file	"_pset.s"

	.text

	.globl	__cerror
	.globl	_pset

_fgdef_(`_pset'):
	MCOUNT
	movl	$PSET,%eax
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
	_fg_setsize_(`_pset')
