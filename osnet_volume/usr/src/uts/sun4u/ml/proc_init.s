/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_init.s	1.22	99/06/14 SMI"

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/machthread.h>
#include <sys/param.h>
#include <sys/vm_machparam.h>
#include <sys/privregs.h>
#include <sys/intreg.h>
#include <sys/vis.h>

/*
 * Processor initialization
 *
 * This is the kernel entry point for other cpus except the first one.
 * When the prom jumps to this location we are still executing with the
 * prom's trap table.  It expects the cpuid as its first parameter.
 */

#if defined(lint)

/* ARGSUSED */
void
cpu_startup(int cpuid)
{}

#else	/* lint */

	! allocate a temporary stack to run on while we figure who and
	! what we are.
	.seg	".data"
	.align	8
etmpstk:
	.skip	2048
tmpstk:
	.word	0

	ENTRY_NP(cpu_startup)
	!
	! Initialize CPU state registers
	!
	! The boot cpu and other cpus are different.
	! The boot cpu has gone through the boot or the kadb,
	! and its state might be affected by them. But,
	! other cpus' states are directly coming from the prom.
	!
	wrpr	%g0, PSTATE_KERN, %pstate
	wr	%g0, %g0, %fprs				! clear fprs
#ifdef __sparcv9
	CLEARTICKNPT(cpu_startup, %g1, %g2, %g3)	! allow user rdtick
#endif

	!
	! Set up temporary stack
	!
	set	tmpstk, %g1
	sub	%g1, SA(V9FPUSIZE+GSR_SIZE), %g2
	and	%g2, 0x3F, %g3
	sub	%g2, %g3, %o2
	sub	%o2, SA(MINFRAME) + STACK_BIAS, %sp

	mov	%o0, %l1		! save cpuid

	call	sfmmu_mp_startup
	sub	%g0, 1, THREAD_REG	! catch any curthread acceses

	! We are now running on the kernel's trap table.
	!
	! It is very important to have a thread pointer and a cpu struct
	! *before* calling into C routines .
	! Otherwise, overflow/underflow handlers, etc. can get very upset!
	! 
	!
	! We don't want to simply increment
	! ncpus right now because it is in the cache, and
	! we don't have the cache on yet for this CPU.
	!
	set	cpu, %l3
	sll	%l1, CPTRSHIFT, %l2	! offset into CPU vector.
	ldn	[%l3 + %l2], %l3	! pointer to CPU struct
	ldn	[%l3 + CPU_THREAD], THREAD_REG	! set thread pointer (%g7)

	!
	! Resume the thread allocated for the CPU.
	!
 	ldn	[THREAD_REG + T_PC], %i7
	ldn	[THREAD_REG + T_SP], %fp
	ret				! "return" into the thread
	restore				! WILL cause underflow
	SET_SIZE(cpu_startup)

#endif	/* lint */
