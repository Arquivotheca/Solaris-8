/	.asciz	"@(#)_mul64.s 1.3 98/07/08 SMI"

	.file	"mul64.s"
/
/   function __mul64(A,B:Longint):Longint;
/	{Overflow is not checked}
/
/ We essentially do multiply by longhand, using base 2**32 digits.
/               a       b	parameter A
/	     x 	c       d	parameter B
/		---------
/               ad      bd
/       ac	bc         
/       -----------------
/       ac	ad+bc	bd
/       
/       We can ignore ac and top 32 bits of ad+bc: if <> 0, overflow happened.
/       
	.globl	__mul64
_fgdef_(__mul64):
	push	%ebp
	mov    	%esp,%ebp
	pushl	%esi
	mov	12(%ebp),%eax	/ A.hi (a)
	mull	16(%ebp)	/ Multiply A.hi by B.lo (produces ad)
	xchg	%ecx,%eax	/ ecx = bottom half of ad.
	movl    8(%ebp),%eax	/ A.Lo (b)
	movl	%eax,%esi	/ Save A.lo for later
	mull	16(%ebp)	/ Multiply A.Lo by B.LO (dx:ax = bd.)
	addl	%edx,%ecx	/ cx is ad
	xchg	%eax,%esi       / esi is bd, eax = A.lo (d)
	mull	20(%ebp)	/ Multiply A.lo * B.hi (producing bc)
	addl	%ecx,%eax	/ Produce ad+bc
	movl	%esi,%edx
	xchg	%eax,%edx
	popl	%esi
	movl	%ebp,%esp
	popl	%ebp
	ret     $16
	_fg_setsize_(`__mul64')
