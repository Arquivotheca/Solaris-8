/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident "@(#)intr.s	1.7	99/10/07 SMI"

/*
 *	intr.s	-- Second level boot protected mode interrupt handling 
 */

#include <sys/bootdef.h>

#ifdef	lint

extern void trap(unsigned long *);
extern struct tss386 tss_normal;

void div0trap(void);
void dbgtrap(void);
void nodbgmon(void);
void nmiint(void);
void brktrap(void);
void ovflotrap(void);
void boundstrap(void);
void invoptrap(void);
void ndptrap0(void);
void dbfault(void) { trap((unsigned long *)&tss_normal); return; }
void overrun(void);
void invtsstrap(void);
void segnptrap(void);
void stktrap(void);
void gptrap(void);
void pftrap(void);
void resvtrap(void);
void ndperr(void);
void inval17(void);
void inval18(void);
void inval19(void);
void progent(void);
void inval21(void);
void inval22(void);
void inval23(void);
void inval24(void);
void inval25(void);
void inval26(void);
void inval27(void);
void inval28(void);
void inval29(void);
void inval30(void);
void inval31(void);
void ndptrap2(void);
void inval33(void);
void inval34(void);
void inval35(void);
void inval36(void);
void inval37(void);
void inval38(void);
void inval39(void);
void inval40(void);
void inval41(void);
void inval42(void);
void inval43(void);
void inval44(void);
void inval45(void);
void inval46(void);
void inval47(void);
void inval48(void);
void inval49(void);
void inval50(void);
void inval51(void);
void inval52(void);
void inval53(void);
void inval54(void);
void inval55(void);
void inval56(void);
void inval57(void);
void inval58(void);
void inval59(void);
void inval60(void);
void inval61(void);
void inval62(void);
void inval63(void);
void ivctM0(void);
void ivctM1(void);
void ivctM2(void);
void ivctM3(void);
void ivctM4(void);
void ivctM5(void);
void ivctM6(void);
void ivctM7(void);
void ivctM0S0(void);
void ivctM0S1(void);
void ivctM0S2(void);
void ivctM0S3(void);
void ivctM0S4(void);
void ivctM0S5(void);
void ivctM0S6(void);
void ivctM0S7(void);
void ivctM1S0(void);
void ivctM1S1(void);
void ivctM1S2(void);
void ivctM1S3(void);
void ivctM1S4(void);
void ivctM1S5(void);
void ivctM1S6(void);
void ivctM1S7(void);
void ivctM2S0(void);
void ivctM2S1(void);
void ivctM2S2(void);
void ivctM2S3(void);
void ivctM2S4(void);
void ivctM2S5(void);
void ivctM2S6(void);
void ivctM2S7(void);
void ivctM3S0(void);
void ivctM3S1(void);
void ivctM3S2(void);
void ivctM3S3(void);
void ivctM3S4(void);
void ivctM3S5(void);
void ivctM3S6(void);
void ivctM3S7(void);
void ivctM4S0(void);
void ivctM4S1(void);
void ivctM4S2(void);
void ivctM4S3(void);
void ivctM4S4(void);
void ivctM4S5(void);
void ivctM4S6(void);
void ivctM4S7(void);
void ivctM5S0(void);
void ivctM5S1(void);
void ivctM5S2(void);
void ivctM5S3(void);
void ivctM5S4(void);
void ivctM5S5(void);
void ivctM5S6(void);
void ivctM5S7(void);
void ivctM6S0(void);
void ivctM6S1(void);
void ivctM6S2(void);
void ivctM6S3(void);
void ivctM6S4(void);
void ivctM6S5(void);
void ivctM6S6(void);
void ivctM6S7(void);
void ivctM7S0(void);
void ivctM7S1(void);
void ivctM7S2(void);
void ivctM7S3(void);
void ivctM7S4(void);
void ivctM7S5(void);
void ivctM7S6(void);
void ivctM7S7(void);
void invaltrap(void);
/*ARGSUSED*/ void enter_debug(unsigned long a) { return; }
/*ARGSUSED*/ void get_cregs(unsigned long *a) { return; }
/*ARGSUSED*/ void get_dregs(unsigned long *a) { return; }
/*ARGSUSED*/ void set_dregs(unsigned long *a) { return; }
/*ARGSUSED*/ int setjmp(long *a) { return (0); }
/*ARGSUSED*/ void longjmp(long *a, int b) { return; }
/*ARGSUSED*/ void invalidate(char *a) { return; }
#else
	.text

#define	VECT(label, num) \
	.align	4; \
	.globl	label; \
label: \
	push	$0; \
	push	$num; \
	jmp	cmntrap

	VECT(div0trap, 0)

	.align	4
	.globl	dbgtrap
dbgtrap:
nodbgmon:
	push	$0	/ dummy error code
	pushl	$1	/ trap number
	jmp	cmntrap

	VECT(nmiint, 2)
	VECT(brktrap, 3)
	VECT(ovflotrap, 4)
	VECT(boundstrap, 5)
	VECT(invoptrap, 6)
	VECT(ndptrap0, 7)

	.align	4
	.globl	dbfault
dbfault:
	/ Double faults are handled with a task gate.  Push the
	/ contents of the old TSS to match the stack from other
	/ traps so that we can treat it more-or-less the same.
	pop	%eax			/ save error code
	push	tss_normal + 0x24	/ EFLAGS
	push	tss_normal + 0x4C	/ CS
	push	tss_normal + 0x20	/ EIP
	pushl	%eax			/ repush error code
	pushl	$8			/ trap number
	push	tss_normal + 0x28	/ EAX
	push	tss_normal + 0x2C	/ ECX
	push	tss_normal + 0x30	/ EDX
	push	tss_normal + 0x34	/ EBX
	push	tss_normal + 0x38	/ ESP
	push	tss_normal + 0x3C	/ EBP
	push	tss_normal + 0x40	/ ESI
	push	tss_normal + 0x44	/ EDI
	push	tss_normal + 0x54	/ DS
	push	tss_normal + 0x48	/ ES
	push	tss_normal + 0x58	/ FS
	push	tss_normal + 0x5C	/ GS
	
	movl	%esp, %ebp
	pushl	%ebp
	call	trap
	popl	%eax
.hang:
	jmp	.hang		/ Nothing useful to do if it returns

	VECT(overrun, 9)

	.align	4
	.globl	invtsstrap
invtsstrap:
	pushl	$10	/ trap number
	jmp	cmntrap

	.align	4
	.globl	segnptrap
segnptrap:
	pushl	$11	/ trap number
	jmp	cmntrap

	.align	4
	.globl	stktrap
stktrap:
	pushl	$12	/ trap number
	jmp	cmntrap

	.align	4
	.globl	gptrap
gptrap:
	pushl	$13	/ trap number
	jmp	cmntrap

	.align	4
	.globl	pftrap
pftrap:
	pushl	$14	/ trap number
	jmp	cmntrap

	VECT(resvtrap, 15)
	VECT(ndperr, 16)
	VECT(inval17, 17)
	VECT(inval18, 18)
	VECT(inval19, 19)
	VECT(progent, 20)
	VECT(inval21, 21)
	VECT(inval22, 22)
	VECT(inval23, 23)
	VECT(inval24, 24)
	VECT(inval25, 25)
	VECT(inval26, 26)
	VECT(inval27, 27)
	VECT(inval28, 28)
	VECT(inval29, 29)
	VECT(inval30, 30)
	VECT(inval31, 31)
	VECT(ndptrap2, 32)
	VECT(inval33, 33)
	VECT(inval34, 34)
	VECT(inval35, 35)
	VECT(inval36, 36)
	VECT(inval37, 37)
	VECT(inval38, 38)
	VECT(inval39, 39)
	VECT(inval40, 40)
	VECT(inval41, 41)
	VECT(inval42, 42)
	VECT(inval43, 43)
	VECT(inval44, 44)
	VECT(inval45, 45)
	VECT(inval46, 46)
	VECT(inval47, 47)
	VECT(inval48, 48)
	VECT(inval49, 49)
	VECT(inval50, 50)
	VECT(inval51, 51)
	VECT(inval52, 52)
	VECT(inval53, 53)
	VECT(inval54, 54)
	VECT(inval55, 55)
	VECT(inval56, 56)
	VECT(inval57, 57)
	VECT(inval58, 58)
	VECT(inval59, 59)
	VECT(inval60, 60)
	VECT(inval61, 61)
	VECT(inval62, 62)
	VECT(inval63, 63)
	VECT(ivctM0, 64)
	VECT(ivctM1, 65)
	VECT(ivctM2, 66)
	VECT(ivctM3, 67)
	VECT(ivctM4, 68)
	VECT(ivctM5, 69)
	VECT(ivctM6, 70)
	VECT(ivctM7, 71)
	VECT(ivctM0S0, 72)
	VECT(ivctM0S1, 73)
	VECT(ivctM0S2, 74)
	VECT(ivctM0S3, 75)
	VECT(ivctM0S4, 76)
	VECT(ivctM0S5, 77)
	VECT(ivctM0S6, 78)
	VECT(ivctM0S7, 79)
	VECT(ivctM1S0, 80)
	VECT(ivctM1S1, 81)
	VECT(ivctM1S2, 82)
	VECT(ivctM1S3, 83)
	VECT(ivctM1S4, 84)
	VECT(ivctM1S5, 85)
	VECT(ivctM1S6, 86)
	VECT(ivctM1S7, 87)
	VECT(ivctM2S0, 88)
	VECT(ivctM2S1, 89)
	VECT(ivctM2S2, 90)
	VECT(ivctM2S3, 91)
	VECT(ivctM2S4, 92)
	VECT(ivctM2S5, 93)
	VECT(ivctM2S6, 94)
	VECT(ivctM2S7, 95)
	VECT(ivctM3S0, 96)
	VECT(ivctM3S1, 97)
	VECT(ivctM3S2, 98)
	VECT(ivctM3S3, 99)
	VECT(ivctM3S4, 100)
	VECT(ivctM3S5, 101)
	VECT(ivctM3S6, 102)
	VECT(ivctM3S7, 103)
	VECT(ivctM4S0, 104)
	VECT(ivctM4S1, 105)
	VECT(ivctM4S2, 106)
	VECT(ivctM4S3, 107)
	VECT(ivctM4S4, 108)
	VECT(ivctM4S5, 109)
	VECT(ivctM4S6, 110)
	VECT(ivctM4S7, 111)
	VECT(ivctM5S0, 112)
	VECT(ivctM5S1, 113)
	VECT(ivctM5S2, 114)
	VECT(ivctM5S3, 115)
	VECT(ivctM5S4, 116)
	VECT(ivctM5S5, 117)
	VECT(ivctM5S6, 118)
	VECT(ivctM5S7, 119)
	VECT(ivctM6S0, 120)
	VECT(ivctM6S1, 121)
	VECT(ivctM6S2, 122)
	VECT(ivctM6S3, 123)
	VECT(ivctM6S4, 124)
	VECT(ivctM6S5, 125)
	VECT(ivctM6S6, 126)
	VECT(ivctM6S7, 127)
	VECT(ivctM7S0, 128)
	VECT(ivctM7S1, 129)
	VECT(ivctM7S2, 130)
	VECT(ivctM7S3, 131)
	VECT(ivctM7S4, 132)
	VECT(ivctM7S5, 133)
	VECT(ivctM7S6, 134)
	VECT(ivctM7S7, 135)
	VECT(invaltrap, 255)

/ Before jumping here the entry stub will have pushed a dummy error
/ code (for traps with no error codes) and the trap number.  Push the
/ rest of the registers in the same order as the Solaris kernel.
cmntrap:
	pusha
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	movl	$B_GDT, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	movl	%esp, %ebp
	pushl	%ebp
	call	trap
	popl	%eax

	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popa

	addl	$8, %esp
	iret

/ Routine for normal code to force debugger entry.  Creates a stack
/ frame so that stack backtrace from debugger looks correct.  Puts
/ argument in EAX to permit passing data to debugger.
/
	.globl	enter_debug
enter_debug:
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp), %eax
	int	$1
	leave
	ret

/ Routine to read and write the control register set.
/ Argument is a pointer to an array of 4 longs, representing
/ CR0 - CR3.  CR1 is omitted.
/
	.globl	get_cregs
get_cregs:
	pushl	%ebp
	movl	%esp,%ebp

	movl	8(%ebp), %eax
	movl	%cr0, %ecx
	movl	%ecx, (%eax)
	movl	%cr2, %ecx
	movl	%ecx, 8(%eax)
	movl	%cr3, %ecx
	movl	%ecx, 12(%eax)

	leave
	ret

/ Routines to read and write the debug register set
/ Argument is a pointer to an array of 8 longs, representing
/ DR0 - DR7.  Both routines process DR0 - DR3 and DR6 - DR7.
/
	.globl	get_dregs
get_dregs:
	pushl	%ebp
	movl	%esp,%ebp

	movl	8(%ebp), %eax
	movl	%dr0, %ecx
	movl	%ecx, (%eax)
	movl	%dr1, %ecx
	movl	%ecx, 4(%eax)
	movl	%dr2, %ecx
	movl	%ecx, 8(%eax)
	movl	%dr3, %ecx
	movl	%ecx, 12(%eax)
	movl	%dr6, %ecx
	movl	%ecx, 24(%eax)
	movl	%dr7, %ecx
	movl	%ecx, 28(%eax)

	leave
	ret

	.globl	set_dregs
set_dregs:
	pushl	%ebp
	movl	%esp,%ebp

	movl	8(%ebp), %eax
	movl	(%eax), %ecx
	movl	%ecx, %dr0
	movl	4(%eax), %ecx
	movl	%ecx, %dr1
	movl	8(%eax), %ecx
	movl	%ecx, %dr2
	movl	12(%eax), %ecx
	movl	%ecx, %dr3
	movl	24(%eax), %ecx
	movl	%ecx, %dr6
	movl	28(%eax), %ecx
	movl	%ecx, %dr7

	leave
	ret

	.globl	setjmp
setjmp:
	movl	4(%esp), %eax	/ jmpbuf address
	movl	%ebx, 0(%eax)
	movl	%esi, 4(%eax)
	movl	%edi, 8(%eax)
	movl	%ebp, 12(%eax)
	popl	%edx		/ return address
	movl	%esp, 16(%eax)
	movl	%edx, 20(%eax)
	subl	%eax, %eax	/ return 0
	jmp	*%edx

	.globl	longjmp
longjmp:
	movl	4(%esp), %edx	/ jmpbuf address (1st parameter)
	movl	8(%esp), %eax	/ return value (second parameter)
	movl	0(%edx), %ebx
	movl	4(%edx), %esi
	movl	8(%edx), %edi
	movl	12(%edx), %ebp
	movl	16(%edx), %esp
	test	%eax, %eax
	jnz	.ret
	incl	%eax		/ avoid returning 0
.ret:
	jmp	*20(%edx)	/ return to setjmp caller

/ invalidate(char *p) - flush mapping for p from the tlb.
	.globl	invalidate
invalidate:
	push	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax
	invlpg	(%eax)
	leave
	ret

#endif	/* !lint */
