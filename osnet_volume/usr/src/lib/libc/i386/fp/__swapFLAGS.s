#ident  "@(#)__swapFLAGS.s	1.1	92/04/17 SMI"
/	.asciz	"@(#)__swapFLAGS.s 1.3 91/11/04 SMI"
/	Copyright (c) 1987 by Sun Microsystems, Inc.

/ i386 Control Word
/ bit 0 - invalid mask
/ bit 1 - denormalize mask
/ bit 2 - zero divide mask
/ bit 3 - overflow mask
/ bit 4 - underflow mask
/ bit 5 - inexact mask
	.type	__swapTE,@function
	.text
	.globl	__swapTE
	.align	4
__swapTE:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp
	fnstcw	-4(%ebp)
	movw	-4(%ebp),%cx
	movw	%cx,%ax		/ ax = cw
	orw	$0x3f,%cx	/ cx = mask off all exception
	movw	8(%ebp),%dx	/ dx = input TRAP ENABLE value te
	andw	$0x3f,%dx	/ make sure bitn>5 is zero
	xorw	%dx,%cx		/ turn off the MASK bit accordingly
	movw	%cx,-8(%ebp)
	fldcw	-8(%ebp)	/ load new cw 
	andl	$0x3f,%eax	/ old cw exception MASK info
	xorw	$0x3f,%ax	/ return exception TRAP info
	leave
	ret
	.align	4
	.size	__swapTE,.-__swapTE

/ i386 Status Word
/ bit 0 - invalid
/ bit 1 - denormalize 
/ bit 2 - zero divide
/ bit 3 - overflow
/ bit 4 - underflow
/ bit 5 - inexact
	.type	__swapEX,@function
	.text
	.globl	__swapEX
	.align	4
__swapEX:
	pushl	%ebp
	movl	%esp,%ebp
	fnstsw	%ax		/ ax = sw
	movl	8(%ebp),%ecx	/ ecx = input ex
	andl	$0x3f,%ecx
	cmpw	$0,%cx
	jne	L1
				/ input ex=0, clear all exception
	fnclex	
	andl	$0x3f,%eax
	leave
	ret
L1:
				/ input ex !=0, use fnstenv and fldenv
	subl	$0x70,%esp
	fnstenv	-0x70(%ebp)
	movw	%ax,%dx
	andw	$0xffc0,%dx
	orw	%dx,%cx
	movw	%cx,-0x6c(%ebp)	/ replace old sw by a new one (need to verify)
	fldenv	-0x70(%ebp)
	andl	$0x3f,%eax
	leave
	ret
	.align	4
	.size	__swapEX,.-__swapEX


/ 00 - 24 bits
/ 01 - reserved
/ 10 - 53 bits
/ 11 - 64 bits
	.type	__swapRP,@function
	.text
	.globl	__swapRP
	.align	4
__swapRP:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp
	fstcw	-4(%ebp)
	movw	-4(%ebp),%ax
	andw	$0xfcff,-4(%ebp)
	movl	8(%ebp),%edx
	andl	$0x3,%edx
	shlw	$8,%dx
	orw	%dx,-4(%ebp)
	fldcw	-4(%ebp)
	shrw	$8,%ax
	andl	$0x3,%eax
	leave
	ret
	.align	4
	.size	__swapRP,.-__swapRP

/ 00 - Round to nearest or even
/ 01 - Round down
/ 10 - Round up
/ 11 - Chop
	.type	__swapRD,@function
	.text
	.globl	__swapRD
	.align	4
__swapRD:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp
	fstcw	-4(%ebp)
	movw	-4(%ebp),%ax
	andw	$0xf3ff,-4(%ebp)
	movl	8(%ebp),%edx
	andl	$0x3,%edx
	shlw	$10,%dx
	orw	%dx,-4(%ebp)
	fldcw	-4(%ebp)
	shrw	$10,%ax
	andl	$0x3,%eax
	leave
	ret
	.align	4
	.size	__swapRD,.-__swapRD
