/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ia32.il	1.24	99/05/04 SMI"

/
/ In-line functions for i86 kernels.
/

/
/ return current thread pointer
/
/ NOTE: the "0xc" should be replaced by the computed value of the
/	offset of "cpu_thread" from the beginning of the struct cpu.
/	Including "assym.h" does not work, however, since that stuff
/	is PSM-specific and is only visible to the 'unix' build anyway.
/	Ugh -- what a disaster.
/
	.inline	threadp,0
	movl	%gs:0xc, %eax
	.end

/
/ return caller
/
	.inline caller,0
	movl	4(%ebp), %eax
	.end

/
/ return current cpu pointer
/
	.inline	curcpup,0
	movl	%fs:0, %eax
	.end

/
/ return value of cr3 register
/
	.inline	cr3,0
	movl	%cr3, %eax
	.end

/
/ reload cr3 register with its current value
/
	.inline	reload_cr3,0
	movl	%cr3, %eax
	movl	%eax, %cr3
	.end

/
/ invalidate and flush cache.
/
	.inline	cache_bug,0
	wbinvd
	.end

/
/ convert ipl to spl.  This is the identity function for i86
/
	.inline	ipltospl,0
	movl	(%esp), %eax
	.end

/
/ enable interrupts
/
	.inline	sti,0
	sti
	.end

/
/ disable interrupts and return value describing if interrupts were enabled
/
	.inline	clear_int_flag,0
	pushfl
	cli
	popl	%eax
	.end

	.inline	intr_clear,0
	pushfl
	cli
	popl	%eax
	.end

/
/ restore interrupt enable flag to value returned from 'clear_int_flag' above
/
	.inline restore_int_flag,4
	pushl	(%esp)
	popfl
	.end

	.inline intr_restore,4
	pushl	(%esp)
	popfl
	.end

/
/ in and out
/
	.inline	inb,4
	movl	(%esp), %edx
	xorl    %eax, %eax
	inb	(%dx)
	.end

	.inline	inw,4
	movl	(%esp), %edx
	xorl    %eax, %eax
	inw	(%dx)
	.end

	.inline	inl,4
	movl	(%esp), %edx
	xorl    %eax, %eax
	inl	(%dx)
	.end

	.inline	outb,8
	movl	(%esp), %edx
	movl    4(%esp), %eax
	outb	(%dx)
	.end

	.inline	outw,8
	movl	(%esp), %edx
	movl    4(%esp), %eax
	outw	(%dx)
	.end

	.inline	outl,8
	movl	(%esp), %edx
	movl    4(%esp), %eax
	outl	(%dx)
	.end

/
/ Networking byte order functions (too bad, Intel has the wrong byte order)
/

	.inline	htonl,4
	movl	(%esp), %eax
	xchgb	%ah, %al
	rorl	$16, %eax
	xchgb	%ah, %al
	.end

	.inline	ntohl,4
	movl	(%esp), %eax
	xchgb	%ah, %al
	rorl	$16, %eax
	xchgb	%ah, %al
	.end

	.inline	htons,4
	movzwl	(%esp), %eax
	xchgb	%ah, %al
	.end

	.inline	ntohs,4
	movzwl	(%esp), %eax
	xchgb	%ah, %al
	.end

/*
 * multiply two long numbers and yield a u_lonlong_t result
 * Provided to manipulate hrtime_t values.
 */
	.inline mul32, 8
	movl	4(%esp), %eax
	movl	(%esp), %ecx
	mull	%ecx
	.end

/*
 * Unlock hres_lock and increment the count value. (See clock.h)
 */
	.inline unlock_hres_lock, 0
	lock
	incl	hres_lock
	.end

/*
 * atomically and/or a mask into a memory location:
 *	atomic_orl(unsigned long *addr, unsigned long val) { *addr |= val; }
 *	atomic_andl(unsigned long *addr, unsigned long val) { *addr &= val; }
 *
 * used, for example,  in hat_i86.c to keep i86mmu_cpusrunning
 * up to date without having to put a lock around it.
 */
	.inline	atomic_orl,8
	movl	(%esp), %eax
	movl    4(%esp), %edx
	lock
	or	%edx,(%eax)
	.end

	.inline	atomic_andl,8
	movl	(%esp), %eax
	movl    4(%esp), %edx
	lock
	and	%edx,(%eax)
	.end

 
        .inline atomic_xchgl, 8
        movl    4(%esp), %eax
        movl    (%esp), %ecx
        xchgl   %eax, (%ecx)
        .end

	.inline	atomic_orb,8
	movl	(%esp), %eax
	movl    4(%esp), %edx
	lock
	orb	%dl,(%eax)
	.end

	.inline	atomic_andb,8
	movl	(%esp), %eax
	movl    4(%esp), %edx
	lock
	andb	%dl,(%eax)
	.end


/*
 * Read Time Stamp Counter
 * uint64_t tsc_read();
 *
 * usage:
 * uint64_t cycles = tsc_read();
 *
 * PPro & PII take no less than 34 cycles to execute rdtsc + stores.
 * Pentium takes about 16 cycles.
 */
	.inline	tsc_read, 0
	rdtsc				/ %edx:%eax = RDTSC
	.end

/*
 * void tsc_clear(register)
 * Clear the local Time Stamp Counter via write-MSR instruction.
 * Note that while this is a 64-bit write, the top 32-bits are
 * ignored, so it isn't massively useful to write anything other
 * than zero.
 */
	.inline  tsc_reset, 4
	movl	(%esp), %ecx
	xorl	%eax, %eax
	movl	%eax, %edx
	wrmsr
	ret
	.end
