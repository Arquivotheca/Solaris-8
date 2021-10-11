/
/ Copyright (c) 1999, by Sun Microsystems, Inc.
/ All rights reserved.
/

.ident	"@(#)_rpcsys.s 1.2     99/07/28 SMI"

/ _rpcsys function


	.file	"_rpcsys.s"

	.text

	.globl  __cerror
	.globl	_rpcsys

_fgdef_(_rpcsys):
	MCOUNT
	movl	$RPCSYS,%eax
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
	_fg_setsize_(`_rpcsys')
