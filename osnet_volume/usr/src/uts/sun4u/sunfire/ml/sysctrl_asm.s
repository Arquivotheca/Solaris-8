/*
 *	Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved
 */

#pragma	ident	"@(#)sysctrl_asm.s	1.3	98/01/23	SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/vtrace.h>
#include <sys/machthread.h>
#include <sys/clock.h>
#include <sys/asi.h>
#include <sys/fsr.h>
#include <sys/privregs.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/spitregs.h>
#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif

#if defined(lint)

#else	/* lint */
#include "assym.h"
#endif	/* lint */

#define	TT_HSM	0x99

#if defined(lint)
void
sysctrl_freeze(void)
{}
#else /* lint */
/*
 * This routine quiets a cpu and has it spin on a barrier.
 * It is used during memory sparing so that no memory operation
 * occurs during the memory copy.
 *
 *	Entry:
 *		%g1    - gate array base address
 *		%g2    - barrier base address
 *		%g3    - arg2
 *		%g4    - arg3
 *
 * 	Register Usage:
 *		%g3    - saved pstate
 *		%g4    - temporary
 *		%g5    - check for panicstr
 */
	ENTRY_NP(sysctrl_freeze)
	CPU_INDEX(%g4, %g5)
	sll	%g4, 2, %g4
	add	%g4, %g1, %g4			! compute address of gate id

	st	%g4, [%g4]			! indicate we are ready
	membar	#Sync
1:
	sethi	%hi(panicstr), %g5
	ld	[%g5 + %lo(panicstr)], %g5
	brnz	%g5, 2f				! exit if in panic
	 nop
	ld	[%g2], %g4
	brz,pt	%g4, 1b				! spin until barrier true
	 nop

2:
#ifdef  TRAPTRACE
	TRACE_PTR(%g3, %g4)
	rdpr	%tick, %g4
	stxa	%g4, [%g3 + TRAP_ENT_TICK]%asi
	stha	%g0, [%g3 + TRAP_ENT_TL]%asi
	set	TT_HSM, %g2
	or	%g2, 0xee, %g4
	stha	%g4, [%g3 + TRAP_ENT_TT]%asi
	sta	%o7, [%g3 + TRAP_ENT_TPC]%asi
	sta	%g0, [%g3 + TRAP_ENT_SP]%asi
	sta	%g0, [%g3 + TRAP_ENT_TR]%asi
	sta	%g0, [%g3 + TRAP_ENT_F1]%asi
	sta	%g0, [%g3 + TRAP_ENT_F2]%asi
	sta	%g0, [%g3 + TRAP_ENT_F3]%asi
	sta	%g0, [%g3 + TRAP_ENT_F4]%asi
	stxa	%g0, [%g3 + TRAP_ENT_TSTATE]%asi	/* tstate = pstate */
	TRACE_NEXT(%g2, %g3, %g4)
#endif	TRAPTRACE

	retry
	membar	#Sync
	SET_SIZE(sysctrl_freeze)

#endif	/* lint */
