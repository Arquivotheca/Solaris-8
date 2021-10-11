/
/ Copyright (c) 1996, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)_so_setsockopt.s	1.6	98/07/08 SMI"


	.file	"_so_setsockopt.s"

	.text

	.globl	__cerror
	.globl	_so_setsockopt

_fgdef_(_so_setsockopt):
	MCOUNT
	movl	$SETSOCKOPT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	ret
	_fg_setsize_(`_so_setsockopt')
