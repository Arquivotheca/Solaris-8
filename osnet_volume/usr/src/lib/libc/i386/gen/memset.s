/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/
	.ident	"@(#)memset.s	1.5	98/12/18 SMI"

	.file	"memset.s"

	.globl	memset
	.align	4

_fwdef_(memset):
	MCOUNT			/ profiling
	pushl	%edi		/ save register variable
	movl	8(%esp),%edi	/ %edi = string address
	movl	12(%esp),%eax	/ %al = byte to duplicate
	movl	16(%esp),%ecx	/ %ecx = number of copies
	cmpl	$20,%ecx	/ strings with 20 or more chars should
	jbe	.byteset	/ byteset one word at a time
.wordset:
	andl	$0xff,%eax	/ Duplicate fill const 4 times in %eax
	shrl	$2,%ecx		/ %ecx = number of words to set
	movl	%eax,%edx
	shll	$8,%eax		/ This is ugly, but no P6 partial stalls
	orl	%edx,%eax	/ get introduced as before
	shll	$8,%eax
	orl	%edx,%eax
	shll	$8,%eax
	orl	%edx,%eax
	rep; sstol
	movl	16(%esp),%ecx
	andl	$3,%ecx		/ %ecx = number of bytes left
.byteset:
	rep; sstob
	movl	8(%esp),%eax	/ return string address
	popl	%edi		/ restore register variable
	ret
	_fg_setsize_(`memset')
