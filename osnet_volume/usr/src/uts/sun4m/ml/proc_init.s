/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_init.s	1.35	99/04/13 SMI"

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/psr.h>
#include <sys/mmu.h>
#include <sys/machthread.h>
#include <sys/devaddr.h>
#include <sys/param.h>

/*
 * Processor initialization
 *
 * The boot prom turns control over to us with the MMU
 * turned on and the context table set to whatever the
 * master asked for. How nice!
 */

#if defined(lint)

void
cpu_startup(void)
{}

#else	/* lint */

	! allocate a temporary stack to run on while we figure who and
	! what we are.
	.seg	".data"
	.align	8
etmpstk:
	.skip	NCPU*256
tmpstk:
	.word	0

	.global	cpu_start_addrs
	.align	4
cpu_start_addrs:
	.word	0, cpu1_startup, cpu2_startup, cpu3_startup

	.seg	".text"
	.align	4
cpu1_startup:
	b	cpu_startup
	set	1, %l1
cpu2_startup:
	b	cpu_startup
	set	2, %l1
cpu3_startup:
	b	cpu_startup
	set	3, %l1

	ENTRY_NP(cpu_startup)
	!
	! XXX - Do we want to enable traps, or would this
	! just be useable for debugging purposes?
	!
	set	PSR_S|PSR_PIL|PSR_ET, %g1
	mov	%g1, %psr		! setup psr, leave traps
					! enabled for monitor.
	nop; nop; nop
	mov	0x02, %wim		! setup wim

	set	V_TBR_ADDR_BASE, %g1
	sll	%l1, 12, %l3
	or	%g1, %l3, %g1

	mov	%g1, %tbr		! set trap handler

	set	tmpstk, %l2
	sll	%l1, 8, %l3		! determine location of stack
	sub	%l2, %l3, %sp		! based on cpu id.

	!
	! It is very important to have a thread pointer and a cpu struct
	! *before* calling into C routines (vik_cache_init() in this case).
	! Otherwise, overflow/underflow handlers, etc. can get very upset!
	! 
	!
	! We don't want to simply increment
	! ncpus right now because it is in the cache, and
	! we don't have the cache on yet for this CPU.
	!
	set     cpu, %l3
	sll     %l1, 2, %l2             ! offset into CPU vector.
	ld      [%l3 + %l2], %l3        ! pointer to CPU struct
	ld      [%l3 + CPU_THREAD], THREAD_REG  ! set thread pointer (%g7)

	!
	! Turn the caches on as early as possible to avoid
	! problems.
	!
	sethi	%hi(use_cache), %l3
	ld	[%l3 + %lo(use_cache)], %l3
	tst	%l3
	bz	1f
	nop

	call	cache_init
	nop
	call	turn_cache_on
	nop

1:
        mov	%psr, %g1
        or	%g1, PSR_ET, %g1	! (traps may already be enabled)
        mov	%g1, %psr		! enable traps
        !
        ! Resume the thread allocated for the CPU.
        !
        ld	[THREAD_REG + T_PC], %i7
        ld	[THREAD_REG + T_SP], %fp
        ret				! "return" into the thread
	restore				! WILL cause underflow
	SET_SIZE(cpu_startup)
#endif	/* lint */
