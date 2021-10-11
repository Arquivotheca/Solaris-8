/
/ Copyright (c) 1996, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)_so_connect.s	1.7	98/07/27 SMI"


	.file	"_so_connect.s"

	.text

	.globl	__cerror
	.globl	_so_connect

_fgdef_(_so_connect):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$CONNECT,%eax
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
	_fg_setsize_(`_so_connect')
