/*
 * Copyright (c) 1995, 1998 Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)apic_s.s	1.8	99/01/19 SMI"

#include <sys/asm_linkage.h>
#include "apic_assym.s"

#if defined(lint) || defined(__lint)
#else	/* lint */
/
/ apic_calibrate() is used to initialise the timer on the local APIC
/ of CPU 0 to the desired frequency.  This routine returns the number
/ of APIC ticks decremented in the time period it takes the 8254 to
/ decrement APIC_TIME_COUNT ticks.
/
	.globl	pit_ticks_adj

	ENTRY(apic_calibrate)
	pushl	%ebx
	pushl	%edi

	xorl	%ebx, %ebx	/ zero out these registers
	xorl	%ecx, %ecx
	xorl	%edx, %edx

	/ save the address of apic counter in %edi
	movl	12(%esp), %eax
	addl	$APIC_CURR_COUNT_OFFSET, %eax
	movl	%eax, %edi

	pushfl			/ don't allow interrupts
	cli

	/ Read the value on the 8254 and ensure its > 0x5000
ready:
	inb	$PITCTR0_PORT
	movb	%al, %bl
	inb	$PITCTR0_PORT
	movb	%al, %bh
	cmpw	$APIC_TIME_MIN, %bx
	jl	ready
	/ start with the low byte in range 0x60 and 0xe0
	cmpb	$0x60, %bl
	jbe	ready
	cmpb	$0xe0, %bl
	jae	ready

	/ Wait for the 8254 to decrement by 5 ticks
	/ so that we know we did not start in the middle of a tick.
	subl	$0x5, %ebx
waiting0:
	inb	$PITCTR0_PORT
	movb	%al, %dl
	inb	$PITCTR0_PORT
	movb	%al, %dh
	cmpl	%edx, %ebx
	jl	waiting0
	/ this case won't happen, but just to kill time to make
	/ it the same before fetching the apic counter 
	cmpb	$0x10, %dl	
	jb	waiting0

	/ Read the value of the APIC current counter of the local apic
	movl	(%edi), %ecx

	/ %edx is the starting value
	movl	%edx, %ebx
	subl	$APIC_TIME_COUNT, %ebx

	/ Wait for the 8254 to decrement by APIC_TIME_COUNT ticks
waiting:
	inb	$PITCTR0_PORT
	movb	%al, %dl
	inb	$PITCTR0_PORT
	movb	%al, %dh
	cmpl	%edx, %ebx
	jl	waiting
	/ test for wrap around case
	cmpb	$0x10, %dl	
	jb	waiting

	/ Re-read the value of the APIC current counter of the local apic
	movl	(%edi), %eax

	/ %edx has the ending value which is smaller or equal to %ebx
	/ the difference is stored in pit_ticks_adj
	subl	%edx, %ebx	
	movl	%ebx, pit_ticks_adj

	/ Return the machin to its initial state
	popfl

	/ Return the number of APIC clock ticks elapsed in the period
	/ it took for the 8254 to decrement APIC_TIME_COUNT ticks
	subl	%eax, %ecx
	movl	%ecx, %eax

	popl	%edi
	popl	%ebx
	ret
	SET_SIZE(apic_calibrate)

	/ lock_add_hrt() is used to lock the high resolution base and
	/ to take care of counter overflows
	.globl	apic_hrtime_stamp
	.globl	apic_nsec_since_boot
	.globl	apic_nsec_per_intr
	.globl	lock_add_hrt

	ENTRY(lock_add_hrt)
	incl	apic_hrtime_stamp	
	/	apic_nsec_since_boot += (hrtime_t)apic_nsec_per_intr;
	movl	apic_nsec_per_intr, %ecx
	addl	%ecx, apic_nsec_since_boot
	adcl	$0, apic_nsec_since_boot+4
	incl	apic_hrtime_stamp
	ret
	SET_SIZE(lock_add_hrt)
#endif	/* lint */
