/
/ Copyright (c) 1996, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)_so_sendto.s	1.6	98/07/08 SMI"


	.file	"_so_sendto.s"

	.text

	.globl	__cerror
	.globl	_so_sendto

_fgdef_(_so_sendto):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SENDTO,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_so_sendto
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fg_setsize_(`_so_sendto')
