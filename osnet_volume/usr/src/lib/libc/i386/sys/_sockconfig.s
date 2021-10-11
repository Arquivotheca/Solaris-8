/
/ Copyright (c) 1996, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)_sockconfig.s	1.6	98/07/08 SMI"


	.file	"_sockconfig.s"

	.text

	.globl	__cerror
	.globl	_sockconfig

_fgdef_(_sockconfig):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SOCKCONFIG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fg_setsize_(`_sockconfig')
