/
/ Copyright (c) 1997, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)__getloadavg.s	1.2	98/07/08 SMI"

	.file	"__getloadavg.s"

	.text

	.globl  __getloadavg
	.globl	__cerror

 _fgdef_(`__getloadavg'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETLOADAVG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je 	__fcntl
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fg_setsize_(`__getloadavg')
