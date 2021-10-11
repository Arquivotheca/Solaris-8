/
/ Copyright (c) 1999 by Sun Microsystems, Inc.
/ All rights reserved.
/

	.file	"isnanf.s"

	.ident	"@(#)isnanf.s	1.3	99/10/18 SMI"

/	int isnanf(srcF)
/	float srcF;
/
/	This routine returns 1 if the argument is a NaN
/		     returns 0 otherwise.


	.text
	.align	4
/	.def	isnanf;	.val	isnanf;	.scl	2;	.type	046;	.endef
	.globl	isnanf

_fwdef_(`isnanf'):
	MCOUNT
	movl	4(%esp),%eax
	andl	$0x7f800000,%eax	/ exponent - bits 23-30
	cmpl	$0x7f800000,%eax
	jne	.false

	movl	4(%esp),%eax
	andl	$0x007fffff,%eax	/ fraction - bits 22-0
	jz	.false			/ all fraction bits are 0
					/ its an infinity
	movl	$1,%eax
	ret
.false:
	movl	$0,%eax
	ret
/	.def	isnanf;	.val	.;	.scl	-1;	.endef
	_fw_setsize_(`isnanf')
