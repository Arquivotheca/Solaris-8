/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

.ident	"@(#)fpsetsticky.s	1.2	98/07/08 SMI"

	.file	"fpsetsticky.s"
	.version	"01.01"
	.set	EXCPMASK,63
	.text
	.align	4
_fwdef_(`fpsetsticky'):
	pushl	%ebp
	movl	%esp,%ebp
	subl	$32,%esp
	leal	-4(%ebp),%eax
	fstsw   (%eax)
	movl	-4(%ebp),%eax
	andl	$EXCPMASK,%eax
	movl	%eax,-24(%ebp)
	movl	8(%ebp),%eax
	andl	$EXCPMASK,%eax
	andl	$-64,-4(%ebp)
	orl	%eax,-4(%ebp)
	leal	-4(%ebp),%eax
	movl	%eax,%ecx
	leal	-18(%ebp),%eax
	data16
	fnstenv (%eax)
	movw    (%ecx),%cx
	movw    %cx,2(%eax)
	data16
	fldenv (%eax)
	movl	-24(%ebp),%eax
	leave
	ret
	_fw_setsize_(`fpsetsticky')
