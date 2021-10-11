	.file	"memcmp.s"

	.ident	"@(#)memcmp.s	1.4	98/07/08 SMI"

	.globl	memcmp
	.align	4

/ This implementation conforms to SVID but does not implement
/ the same algorithm as the portable version because it is
/ inconvenient to get the difference of the differing characters.

_fwdef_(memcmp):
	movl	%edi,%edx	/ save register variables
	pushl	%esi
	movl	8(%esp),%esi	/ %esi = address of string 1
	movl	12(%esp),%edi	/ %edi = address of string 2
	cmpl	%esi,%edi	/ The same string?
	je	.equal

	movl    16(%esp),%eax   / %eax = length in bytes
	movl    %eax,%ecx       / compute long count in %ecx
	shrl    $2,%ecx         / make a long count, if zero length set cc
	repz ;  scmpl           / compare the longs
	jne     .longnotequal   / if a mismatch branch

	movl    %eax,%ecx       / %eax = length in bytes
	andl    $3,%ecx         / remainder of bytes to do, if zero len set cc

	repz ; 	scmpb		/ compare the bytes
	jne     .notequal
.equal:
	popl	%esi
	movl	%edx,%edi
	xorl	%eax,%eax
	ret

	.align  4
.longnotequal:
	movl    $4, %ecx        / redo the last long
	subl    %ecx, %esi      / back up to do last long
	subl    %ecx, %edi      / back up to do last long
	repz ; 	scmpb		/ compare the bytes
				/ has to be not equal
.notequal:
	sbbl    %eax, %eax      / eax = 0 if no carry, eax = -1 if carry
	orl     $1, %eax        / eax = 1 if no carry, eax = -1 if carry
	popl	%esi
	movl	%edx,%edi
	ret
	_fw_setsize_(`memcmp')
