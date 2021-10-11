/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_init.s	1.16	99/04/13 SMI"

#if	defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/psr.h>
#include <sys/machthread.h>
#include <sys/param.h>
#include <sys/mmu.h>


/*
 * Processor initialization
 *
 * The boot prom turns control over to us with the MMU
 * turned on and the context table set to whatever the
 * master asked for. How nice!
 */
#if	defined(lint)
void
cpu_startup(void)
{}
#else	/* lint */
	.global scb
	ENTRY_NP(cpu_startup)
	set	PSR_S|PSR_PIL|PSR_ET, %g1
	mov	%g1, %psr			! setup psr, leave traps
						! enabled for monitor.
	nop; nop; nop
	mov	0x02, %wim			! setup wim

	/*
	 * Clear the MFAR/MFSR, in case the PROM didn't. This has
	 * already been done for the boot cpu during its startup.
	 */
	set	RMMU_FAV_REG, %l4
	lda	[%l4]ASI_MOD, %g0
	set	RMMU_FSR_REG, %l4
	lda	[%l4]ASI_MOD, %g0

	/*
	 * Setup %tbr -> scb ; save old %tbr in %l4
	 */
	mov	%tbr, %l4
	bclr	0xfff, %l4			! remove tt
	set	scb, %g1
	mov	%g1, %tbr			! set trap handler

	/*
	 * Initialize THREAD_REG
	 */
	CPU_INDEX_SLOW(%l3)			! setup fast cpu index lookup
	SET_FAST_CPU_INDEX(%l3)
	CPU_ADDR(%l3, %g1)			! get cpu->cpu_thread
	ld	[%l3 + CPU_THREAD], THREAD_REG	! set thread pointer (%g7)

	/*
	 * Resume "into" thread previously allocated
	 */
	ld	[THREAD_REG + T_PC], %i7
	ld	[THREAD_REG + T_SP], %fp
	ret					! "return" into thread
	restore					! underflow guaranteed
	SET_SIZE(cpu_startup)
#endif	/* lint */
