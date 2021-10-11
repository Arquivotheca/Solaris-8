/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)memmove.s	1.3	98/12/18 SMI"

	.file	"memmove.s"
/
/ This is an optimized version for the memmove function call,
/ using a left/right direction copy based on a check if the 
/ copies overlap, meeting the requirements of ANSI C 7.11.2.2.
/ The copies will be done 4 bytes an iteration for larger sizes
/ with the source address aligned to 4. Smaller sizes simply do
/ a byte block copy to avoid the setup overhead.
/
/ Note that the code has been hand scheduled for the Pentium 
/ pipeline, which in most cases should be proper for a i486.
/ Tails were expanded to reduce jumps, but do result in bigger
/ code.
/
/	.align	8		/ based on the branching, no stalls
	.globl	memmove

	.align	8
_fwdef_(memmove):
	MCOUNT			/ profiling
	pushl	%edi		/ save off %edi, %esi and move destination
	movl	4+12(%esp),%ecx	/ get number of bytes to move
	pushl	%esi
	testl	%ecx,%ecx	/ if (n == 0)
	je	.CleanupReturn	/    return(s);
	movl	8+ 4(%esp),%edi	/ destination buffer address
	movl	8+ 8(%esp),%esi	/ source buffer address
.Common:
	movl	$3,%eax		/ heavily used constant
	cmpl	%esi,%edi	/ if (source addr > dest addr)
	leal	-1(%esi,%ecx),%edx
	jle	.CopyRight	/ 
	cmpl	%edx,%edi
	jle	.CopyLeft
.CopyRight:
	cmpl	$8,%ecx		/    if (size < 8 bytes)
	jbe	.OneByteCopy	/        goto fast short copy loop
.FourByteCopy:
	movl	%ecx,%edx	/    save count
	movl	%esi,%ecx	/    get source buffer 4 byte aligned
	andl	%eax,%ecx
	jz	.SkipAlignRight
	subl	%ecx,%edx
	rep;	smovb		/    do the byte part of copy
.SkipAlignRight:
	movl	%edx,%ecx
	shrl	$2,%ecx
	rep;	smovl		/    do the long word part 
	movl	%edx,%ecx	/    compute bytes left to move
	andl	%eax,%ecx	/    complete copy of remaining bytes
	jz	.CleanupReturn
.OneByteCopy:
	rep;	smovb		/    do the byte part of copy
.CleanupReturn:
	popl	%esi		/  }
	popl	%edi		/  restore registers
	movl	4(%esp),%eax	/  set up return value
.Return:
	ret			/  return(dba);

.CopyLeft:
	std				/ reverse direction bit (RtoL)
	cmpl	$12,%ecx		/ if (size < 12)
	ja	.BigCopyLeft		/ {
	movl	%edx,%esi		/     src = src + size - 1
	leal	-1(%ecx,%edi),%edi	/     dst = dst + size - 1
	rep;	smovb			/    do the byte copy
	cld				/    reset direction flag to LtoR
	popl	%esi			/  }
	popl	%edi			/  restore registers
	movl	4(%esp),%eax		/  set up return value
	ret				/  return(dba);
.BigCopyLeft:				/ } else {
	xchgl	%edx,%ecx
	movl	%ecx,%esi		/ align source w/byte copy
	leal	-1(%edx,%edi),%edi
	andl	%eax,%ecx
	jz	.SkipAlignLeft
	addl	$1, %ecx		/ we need to insure that future
	subl	%ecx,%edx		/ copy is done on aligned boundary
	rep;	smovb
.SkipAlignLeft:
	movl	%edx,%ecx	
	subl	%eax,%esi
	shrl	$2,%ecx			/ do 4 byte copy RtoL
	subl	%eax,%edi
	rep;	smovl
	andl	%eax,%edx		/ do 1 byte copy whats left
	jz	.CleanupReturnLeft
	movl	%edx,%ecx	
	addl	%eax,%esi		/ rep; smovl instruction will decrement
	addl	%eax,%edi		/ %edi, %esi by four after each copy
					/ adding 3 will restore pointers to byte
					/ before last double word copied
					/ which is where they are expected to
					/ be for the single byte copy code
	rep;	smovb
.CleanupReturnLeft:
	cld				/ reset direction flag to LtoR
	popl	%esi
	popl	%edi			/ restore registers
	movl	4(%esp),%eax		/ set up return value
	ret				/ return(dba);
	_fg_setsize_(`memmove')
