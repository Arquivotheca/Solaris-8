/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.

.ident	"@(#)cladm.s	1.1	98/07/17 SMI"

/ int
/ _cladm(int fac, int cmd, void *arg)
/
/ System call entry point for cluster administration.

	.file	"cladm.s"

	.text

	.globl	__cerror
	.globl	_cladm

_fwdef_(`_cladm'):
	MCOUNT
	movl	$CLADM,%eax
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
