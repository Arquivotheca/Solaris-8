/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

.ident	"@(#)fpgetsticky.s	1.2	98/07/08 SMI"

	.file	"fpgetsticky.s"
	.version	"01.01"
	.set	EXCPMASK,63
	.text
	.align	4

_fwdef_(`fpgetsticky'):
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp
	leal	-4(%ebp),%eax
	fstsw   (%eax)
	movl	-4(%ebp),%eax
	andl	$EXCPMASK,%eax
	leave
	ret
	_fw_setsize_(`fpgetsticky')
