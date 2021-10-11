/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)interrupt.s	1.91	99/10/21 SMI"

#if defined(lint)
#include <sys/types.h>
#include <sys/thread.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/isa_defs.h>
#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/spitasi.h>
#include <sys/errno.h>
#include <sys/machlock.h>
#include <sys/machthread.h>
#include <sys/machcpuvar.h>
#include <sys/intreg.h>
#include <sys/intr.h>
#include <sys/privregs.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/ftrace.h>
#include <sys/dmv.h>
#include <sys/cyclic.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#ifdef BB_ERRATA_1 /* writes to TICK_COMPARE may fail */
/*
 * Writes to the TICK_COMPARE register sometimes fail on blackbird modules.
 * The failure occurs only when the following instruction decodes to wr or
 * wrpr.  The workaround is to immediately follow writes to TICK_COMPARE
 * with a read, thus stalling the pipe and keeping following instructions
 * from causing data corruption.  Aligning to a quadword will ensure these
 * two instructions are not split due to i$ misses.
 */
#define WR_TICKCMPR(cmpr,label)			\
	ba	.bb_errata_1.label		;\
	nop					;\
						;\
	.align	64				;\
.bb_errata_1.label:				;\
	wr	cmpr, TICK_COMPARE		;\
	rd	TICK_COMPARE, %g0
#else
#define WR_TICKCMPR(cmpr,label)			\
	wr	cmpr, TICK_COMPARE
#endif /* BB_ERRATA_1 */


#if defined(lint)

/* ARGSUSED */
void
pil14_interrupt(int level)
{}

/* ARGSUSED */
void
pil_interrupt(int level)
{}

#else	/* lint */

/*
 * Level-14 interrupt prologue.  %g2 = %tick on entry.
 */
	ENTRY_NP(pil14_interrupt)
	CPU_ADDR(%g1, %g5)
	rdpr	%pil, %g6			! %g6 = interrupted PIL
	stn	%g6, [%g1 + CPU_PROFILE_PIL]	! record interrupted PIL
	rd	TICK_COMPARE, %g3		! %g3 = when interrupt was due
	sub	%g2, %g3, %g2			! %g2 = interrupt latency
	stx	%g2, [%g1 + CPU_PROFILE_ILATE]
	rdpr	%tstate, %g6
	btst	TSTATE_PRIV, %g6		! trap from supervisor mode?
	bz,pt	%xcc, .pil_interrupt_common	! if not, no PC to record.
	rdpr	%tpc, %g6			! delay - %g6 = interrupted PC
	ba,pt	%xcc, .pil_interrupt_common
	stn	%g6, [%g1 + CPU_PROFILE_PC]	! delay - record interrupted PC
	SET_SIZE(pil14_interrupt)

/*
 * (TT 0x40..0x4F, TL>0) Interrupt Level N Handler (N == 1..15)
 * 	Register passed from LEVEL_INTERRUPT(level)
 *	%g4 - interrupt request level
 */
	ENTRY_NP(pil_interrupt)
	!
	! Register usage
	!	%g1 - cpu
	!	%g3 - intr_req
	!	%g4 - pil
	!	%g2, %g5, %g6 - temps
	!
	! grab the 1st intr_req off the list
	! if the list is empty, clear %clear_softint
	!
	CPU_ADDR(%g1, %g5)
.pil_interrupt_common:
	sll	%g4, CPTRSHIFT, %g5
	add	%g1, INTR_HEAD, %g6	! intr_head[0]
	add	%g6, %g5, %g6		! intr_head[pil]
	ldn	[%g6], %g3		! g3 = intr_req

#ifdef DEBUG
	!
	! Verify the address of intr_req; it should be within the
	! address range of intr_pool and intr_head
	! or the address range of intr_add_head and intr_add_tail.
	! The range of intr_add_head and intr_add_tail is subdivided
	! by cpu, but the subdivision is not verified here.
	!
	! Registers passed to sys_trap()
	!	%g1 - no_intr_req
	!	%g2 - intr_req
	!	%g3 - %pil
	!	%g4 - current pil
	!
	add	%g1, INTR_POOL, %g2
	cmp	%g3, %g2
	bl,pn	%xcc, 8f
	nop
	add	%g1, INTR_HEAD, %g2
	cmp	%g2, %g3
	bge,pt	%xcc, 5f
	nop
8:
	sethi	%hi(intr_add_head), %g2
	ldn	[%g2 + %lo(intr_add_head)], %g2
	cmp	%g3, %g2
	bl,pn	%xcc, 4f
	nop
	sethi	%hi(intr_add_tail), %g2
	ldn	[%g2 + %lo(intr_add_tail)], %g2
	cmp	%g2, %g3
	bge,pt	%xcc, 5f
	nop
4:
#ifdef TRAPTRACE
	TRACE_PTR(%g5, %g2)
	GET_TRACE_TICK(%g2)
	stxa	%g2, [%g5 + TRAP_ENT_TICK]%asi
	mov	0xbad, %g2
	stha	%g2, [%g5 + TRAP_ENT_TL]%asi
	mov	0xbad, %g2
	stha	%g2, [%g5 + TRAP_ENT_TT]%asi
	rdpr	%tpc, %g2
	stna	%g2, [%g5 + TRAP_ENT_TPC]%asi
	rdpr	%tstate, %g2
	stxa	%g2, [%g5 + TRAP_ENT_TSTATE]%asi
	stna	%g0, [%g5 + TRAP_ENT_SP]%asi
	stna	%g1, [%g5 + TRAP_ENT_TR]%asi
	stna	%g2, [%g5 + TRAP_ENT_F1]%asi
	stna	%g3, [%g5 + TRAP_ENT_F2]%asi
	stna	%g4, [%g5 + TRAP_ENT_F3]%asi
	stna	%g6, [%g5 + TRAP_ENT_F4]%asi
	TRACE_NEXT(%g5, %g2, %g1)
#endif /* TRAPTRACE */
	set	no_intr_req, %g1
	mov	%g3, %g2
	mov	%g4, %g3
	mov	1, %g5
	sll	%g5, %g4, %g5
	wr	%g5, CLEAR_SOFTINT
	ba,pt	%xcc, sys_trap
	sub	%g0, 1, %g4
5:	
#endif /* DEBUG */
	ldn	[%g3 + INTR_NEXT], %g2	! 2nd entry
	brnz,pn	%g2, 1f			! branch if list not empty
	stn	%g2, [%g6]
	add	%g1, INTR_TAIL, %g6	! intr_tail[0]
	stn	%g0, [%g5 + %g6]	! update intr_tail[pil]
	mov	1, %g5
	sll	%g5, %g4, %g5
	wr	%g5, CLEAR_SOFTINT
1:
	!
	! put intr_req on free list
	!	%g2 - inumber
	!
	ldn	[%g1 + INTR_HEAD], %g5	! current head of free list
	lduw	[%g3 + INTR_NUMBER], %g2
	stn	%g3, [%g1 + INTR_HEAD]
	stn	%g5, [%g3 + INTR_NEXT]
#ifdef TRAPTRACE
	TRACE_PTR(%g5, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g5 + TRAP_ENT_TICK]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g5 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	stha	%g6, [%g5 + TRAP_ENT_TT]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g5 + TRAP_ENT_TPC]%asi
	rdpr	%tstate, %g6
	stxa	%g6, [%g5 + TRAP_ENT_TSTATE]%asi
	stna	%sp, [%g5 + TRAP_ENT_SP]%asi
	stna	%g3, [%g5 + TRAP_ENT_TR]%asi
	stna	%g2, [%g5 + TRAP_ENT_F1]%asi
	sll	%g4, CPTRSHIFT, %g3
	add	%g1, INTR_HEAD, %g6
	ldn	[%g6 + %g3], %g6		! intr_head[pil]
	stna	%g6, [%g5 + TRAP_ENT_F2]%asi
	add	%g1, INTR_TAIL, %g6
	ldn	[%g6 + %g3], %g6		! intr_tail[pil]
	stna	%g4, [%g5 + TRAP_ENT_F3]%asi
	stna	%g6, [%g5 + TRAP_ENT_F4]%asi
	TRACE_NEXT(%g5, %g6, %g3)
#endif /* TRAPTRACE */
	!
	! clear the iv_pending flag for this inum
	! 
	set	intr_vector, %g5;
	sll	%g2, INTR_VECTOR_SHIFT, %g6;
	add	%g5, %g6, %g5;			! &intr_vector[inum]
	sth	%g0, [%g5 + IV_PENDING]

	!
	! Prepare for sys_trap()
	!
	! Registers passed to sys_trap()
	!	%g1 - interrupt handler at TL==0
	!	%g2 - inumber
	!	%g3 - pil
	!	%g4 - initial pil for handler
	!
	! figure which handler to run and which %pil it starts at
	! intr_thread starts at LOCK_LEVEL to prevent preemption
	! current_thread starts at PIL_MAX to protect cpu_on_intr
	!
	mov	%g4, %g3
	cmp	%g4, LOCK_LEVEL
	bg,a,pt	%xcc, 4f		! branch if pil > LOCK_LEVEL
	mov	PIL_MAX, %g4
	sethi	%hi(intr_thread), %g1
	mov	LOCK_LEVEL, %g4
	ba,pt	%xcc, sys_trap
	or	%g1, %lo(intr_thread), %g1
4:
	sethi	%hi(current_thread), %g1
	ba,pt	%xcc, sys_trap
	or	%g1, %lo(current_thread), %g1
	SET_SIZE(pil_interrupt)

#endif	/* lint */

#if defined(lint)

void
vec_interrupt(void)
{}

#else	/* lint */


/*
 * (TT 0x60, TL>0) Interrupt Vector Handler
 *	Globals are the Interrupt Globals.
 */
	ENTRY_NP(vec_interrupt)
	!
	! Load the interrupt receive data register 0.
	! It could be a fast trap handler address (pc > KERNELBASE) at TL>0
	! or an interrupt number.
	!
	mov	IRDR_0, %g2
	ldxa	[%g2]ASI_INTR_RECEIVE, %g5	! %g5 = PC or Interrupt Number

	! If the high bit of IRDR_0 is set, then this is a
	! data-bearing mondo vector.
	brlz,pn %g5, dmv_vector
	.empty

vec_interrupt_resume:	
	set	KERNELBASE, %g4
	cmp	%g5, %g4
	bl,a,pt	%xcc, 0f			! an interrupt number found
	nop
	!
	!  Cross-trap request case
	!
	! Load interrupt receive data registers 1 and 2 to fetch
	! the arguments for the fast trap handler.
	!
	! Register usage:
	!	g5: TL>0 handler
	!	g1: arg1
	!	g2: arg2
	!	g3: arg3
	!	g4: arg4
	!
	mov	IRDR_1, %g2
	ldxa	[%g2]ASI_INTR_RECEIVE, %g1
	mov	IRDR_2, %g2
	ldxa	[%g2]ASI_INTR_RECEIVE, %g2
#ifdef TRAPTRACE
	TRACE_PTR(%g4, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g4 + TRAP_ENT_TICK]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g4 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	stha	%g6, [%g4 + TRAP_ENT_TT]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g4 + TRAP_ENT_TPC]%asi
	rdpr	%tstate, %g6
	stxa	%g6, [%g4 + TRAP_ENT_TSTATE]%asi
	stna	%sp, [%g4 + TRAP_ENT_SP]%asi
	stna	%g5, [%g4 + TRAP_ENT_TR]%asi	! pc of the TL>0 handler
	stxa	%g1, [%g4 + TRAP_ENT_F1]%asi
	stxa	%g2, [%g4 + TRAP_ENT_F3]%asi
#ifdef __sparcv9
	stxa	%g0, [%g4 + TRAP_ENT_F2]%asi
	stxa	%g0, [%g4 + TRAP_ENT_F4]%asi
#endif
	TRACE_NEXT(%g4, %g6, %g3)
#endif /* TRAPTRACE */
	stxa	%g0, [%g0]ASI_INTR_RECEIVE_STATUS	! clear the BUSY bit
#ifdef SF_ERRATA_51
	ba,pt	%icc, 1f
	membar	#Sync
	.align 32
1:	jmp	%g5				! call the fast trap handler
	nop
#else
	jmp	%g5
	membar	#Sync
#endif /* SF_ERRATA_51 */
	/* Never Reached */

0:
#ifdef DEBUG
	!
	! Verify the inumber received (should be inum < MAXIVNUM).
	!
	set	MAXIVNUM, %g1
	cmp	%g5, %g1
	bl,pt	%xcc, 4f
	nop
	stxa	%g0, [%g0]ASI_INTR_RECEIVE_STATUS
	membar	#Sync			! need it before the ld
	ba,pt	%xcc, 5f
	nop
4:
#endif /* DEBUG */
	!
	! We have an interrupt number.
	! Put the request on the cpu's softint list,
	! and set %set_softint.
	!
	! Register usage:
	!	%g5 - inumber
	!	%g2 - requested pil
	!	%g3 - intr_req
	!	%g4 - cpu
	!	%g1, %g6 - temps
	!
	! clear BUSY bit
	! allocate an intr_req from the free list
	!
	stxa	%g0, [%g0]ASI_INTR_RECEIVE_STATUS
	membar	#Sync			! need it before the ld
	CPU_ADDR(%g4, %g1)
	ldn	[%g4 + INTR_HEAD], %g3
	!
	! if intr_req == NULL, it will cause TLB miss
	! TLB miss handler (TL>0) will call panic
	!
	! get pil from intr_vector table
	!
1:
	set	intr_vector, %g1
	sll	%g5, INTR_VECTOR_SHIFT, %g6
	add	%g1, %g6, %g1		! %g1 = &intr_vector[IN]
	lduh	[%g1 + IV_PIL], %g2
#ifdef DEBUG
	!
	! Verify the intr_vector[] entry according to the inumber.
	! The iv_pil field should not be zero.
	!
	! Registers passed to sys_trap()
	!	%g1 - no_ivintr
	!	%g2 - inumber
	!	%g3 - %pil
	!	%g4 - current pil
	!
	brnz,pt	%g2, 6f
	nop
5:
	set	no_ivintr, %g1
	sub	%g0, 1, %g4
	rdpr	%pil, %g3
	ba,pt	%xcc, sys_trap
	mov	%g5, %g2
6:	
#endif /* DEBUG */
	!
	! fixup free list
	!
	ldn	[%g3 + INTR_NEXT], %g6

#ifdef DEBUG
	!
	! Verify that the free list is not exhausted.
	! The intr_next field should not be zero.
	!
	! Registers passed to sys_trap()
	!	%g1 - no_intr_pool
	!	%g2 - inumber
	!	%g3 - %pil
	!	%g4 - current pil
	!
	brnz,pt	%g6, 7f	
	nop
	set	no_intr_pool, %g1
	sub	%g0, 1, %g4
	rdpr	%pil, %g3
	ba,pt	%xcc, sys_trap
	mov	%g5, %g2
7:
#endif /* DEBUG */

	stn	%g6, [%g4 + INTR_HEAD]
	!
	! fill up intr_req
	!
	st	%g5, [%g3 + INTR_NUMBER]
	stn	%g0, [%g3 + INTR_NEXT]
	!
	! move intr_req to appropriate list
	!
	sll	%g2, CPTRSHIFT, %g5
	add	%g4, INTR_TAIL, %g6
	ldn	[%g6 + %g5], %g1	! current tail
	brz,pt	%g1, 2f			! branch if list empty
	stn	%g3, [%g6 + %g5]	! make intr_req new tail
	!
	! there's pending intr_req already
	!
	ba,pt	%xcc, 3f
	stn	%g3, [%g1 + INTR_NEXT]	! update old tail
2:
	!
	! no pending intr_req; make intr_req new head
	!
	add	%g4, INTR_HEAD, %g6
	stn	%g3, [%g6 + %g5]
3:
#ifdef TRAPTRACE
	TRACE_PTR(%g1, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g1 + TRAP_ENT_TICK]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g1 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	stha	%g6, [%g1 + TRAP_ENT_TT]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g1 + TRAP_ENT_TPC]%asi
	rdpr	%tstate, %g6
	stxa	%g6, [%g1 + TRAP_ENT_TSTATE]%asi
	stna	%sp, [%g1 + TRAP_ENT_SP]%asi
	ld	[%g3 + INTR_NUMBER], %g6
	stna	%g6, [%g1 + TRAP_ENT_TR]%asi
	add	%g4, INTR_HEAD, %g6
	ld	[%g6 + %g5], %g6		! intr_head[pil]
	stna	%g6, [%g1 + TRAP_ENT_F1]%asi
	add	%g4, INTR_TAIL, %g6
	ld	[%g6 + %g5], %g6		! intr_tail[pil]
	stna	%g6, [%g1 + TRAP_ENT_F2]%asi
	stna	%g2, [%g1 + TRAP_ENT_F3]%asi
	stna	%g3, [%g1 + TRAP_ENT_F4]%asi
	TRACE_NEXT(%g1, %g6, %g5)
#endif /* TRAPTRACE */
	!
	! Write %set_softint with (1<<pil) to cause a "pil" level trap
	!
	mov	1, %g1
	sll	%g1, %g2, %g1
	wr	%g1, SET_SOFTINT
	retry
	SET_SIZE(vec_interrupt)


	!	
!   See usr/src/uts/sun4u/sys/dmv.h for the Databearing Mondo Vector
!	 interrupt format
!
! Inputs:
!	g1: value of ASI_INTR_RECEIVE_STATUS
!	g5: word 0 of the interrupt data
! Register use:
!	g2: dmv inum
!	g3: scratch
!	g4: pointer to dmv_dispatch_table
!	g6: handler pointer from dispatch table


	.seg	".data"

	.global dmv_spurious_cnt
dmv_spurious_cnt:
	.word	0


	.seg	".text"
	ENTRY_NP(dmv_vector)
	srlx	%g5, DMV_INUM_SHIFT, %g2
	set	DMV_INUM_MASK, %g3
	and	%g2, %g3, %g2		   ! %g2 = inum

	set	dmv_totalints, %g3
	ld	[%g3], %g3
	cmp	%g2, %g3
	bge,pn	%xcc, 2f		   ! inum >= dmv_totalints
	nop
	
	set	dmv_dispatch_table, %g3
	ldn	[%g3], %g4
	brz,pn	%g4, 2f
	sll	%g2, DMV_DISP_SHIFT, %g3   ! %g3 = inum*sizeof(struct dmv_disp)
		
	add	%g4, %g3, %g4		! %g4 = &dmv_dispatch_table[inum]
#ifdef __sparcv9
#if (DMV_FUNC != 0) || (DMV_ARG != 8)
#error "DMV_FUNC or DMV_SIZE has changed"
#endif
	ldda	[%g4]ASI_NQUAD_LD, %g2  ! %g2=handler %g3=argument
	mov	%g3, %g1
#else
#if (DMV_FUNC != 0) || (DMV_ARG != 4)
#error "DMV_FUNC or DMV_SIZE has changed"
#endif
	ldx	[%g4], %g2		! high 32=handler low 32=argument
	! get ready to call possible handler by putting argument in %g1
	srl	%g2, 0, %g1
	! put just the handler address in lower 32 bits of %g2
	srlx	%g2, 32, %g2
#endif
	brz,pn  %g2, 2f	
	nop
	
	! we have a handler, so call it
	! On entry to the handler, the %g registers are set as follows:
	!
	!	%g1	The argument (arg) passed to dmv_add_intr().
	!	%g2	Word 0 of the incoming mondo vector.
	!
	jmp	%g2
	mov	%g5, %g2
		
	! No handler was listed in the table, so just record it
	! as an error condition and continue.  There is a race
	! window here updating the counter, but that's ok since
	! just knowing that spurious interrupts happened is enough,
	! we probably won't need to know exactly how many.
2:
	set	dmv_spurious_cnt, %g1
	ld	[%g1], %g2
	inc	%g2
	ba,pt	%xcc,3f
	st	%g2, [%g1]
	
	!	When the handler's processing (which should be as quick as
	!	possible) is complete, the handler must exit by jumping to
	!	the label dmv_finish_intr.  The contents of %g1 at this time
	!	determine whether a software interrupt will be issued, as
	!	follows:
	!
	!		If %g1 is less than zero, no interrupt will be queued.
	!		Otherwise, %g1 will be used as the interrupt number
	!		to simulate; this means that the behavior of the
	!		interrupt system will be exactly that which would have
	!		occurred if the first word of the incoming interrupt
	!		vector had contained the contents of %g1.

	ENTRY_NP(dmv_finish_intr)
	brlz,pn %g1,3f
	nop
	!	generate an interrupt based on the contents of %g1
	ba,pt	%xcc,vec_interrupt_resume
	mov	%g1, %g5
	!	We are done
3:	
	stxa	%g0, [%g0]ASI_INTR_RECEIVE_STATUS ! clear the busy bit
	retry
	SET_SIZE(dmv_vector)

#endif	/* lint */

#if defined(lint)

void
vec_intr_spurious(void)
{}

#else	/* lint */
	.seg	".data"

	.global vec_spurious_cnt
vec_spurious_cnt:
	.word	0

	.seg	".text"
	ENTRY_NP(vec_intr_spurious)
	set	vec_spurious_cnt, %g1
	ld	[%g1], %g2
	cmp	%g2, 16
	bl,a,pt	%xcc, 1f
	inc	%g2
	!
	! prepare for sys_trap()
	!	%g1 - sys_tl1_panic
	!	%g2 - panic message
	!	%g4 - current pil
	!
	sub	%g0, 1, %g4
	set	_not_ready, %g2
	sethi	%hi(sys_tl1_panic), %g1
	ba,pt	%xcc, sys_trap
	or	%g1, %lo(sys_tl1_panic), %g1
	!
1:	st	%g2, [%g1]
	retry
	SET_SIZE(vec_intr_spurious)

_not_ready:	.asciz	"Interrupt Vector Receive Register not READY"
	.align 	4

#endif	/* lint */

/*
 * Macro to service an interrupt.
 *
 * inum		- interrupt number (may be out, not preserved)
 * cpu		- cpu pointer (may be out, preserved)
 * ls1		- local scratch reg (used as &intr_vector[inum])
 * ls2		- local scratch reg (used as driver mutex)
 * os1 - os4	- out scratch reg
 */
#ifndef	lint
_spurious:
	.asciz	"!interrupt level %d not serviced"
	.align	4
#define	SERVE_INTR(inum, cpu, ls1, ls2, os1, os2, os3, os4)		\
	set	intr_vector, ls1;					\
	sll	inum, INTR_VECTOR_SHIFT, os1;				\
	add	ls1, os1, ls1;						\
	SERVE_INTR_TRACE(inum, os1, os2, os3, os4);			\
0:	ldn	[ls1 + IV_HANDLER], os2;				\
	ldn	[ls1 + IV_ARG], %o0;					\
	call	os2;							\
	lduh	[ls1 + IV_PIL], ls1;					\
	brnz,pt	%o0, 2f;						\
	mov	CE_WARN, %o0;						\
	set	_spurious, %o1;						\
	call	cmn_err;						\
	rdpr	%pil, %o2;						\
2:	ldn	[THREAD_REG + T_CPU], cpu;				\
	ld	[cpu + CPU_SYSINFO_INTR], os1;				\
	inc	os1;							\
	st	os1, [cpu + CPU_SYSINFO_INTR];				\
	sll	ls1, CPTRSHIFT, os2;					\
	add	cpu,  INTR_HEAD, os1;					\
	add	os1, os2, os1;						\
	ldn	[os1], os3;						\
	brz,pt	os3, 5f;						\
	nop;								\
	rdpr	%pstate, ls2;						\
	wrpr	ls2, PSTATE_IE, %pstate;				\
	ldn 	[os3 + INTR_NEXT], os2;					\
	brnz,pn	os2, 4f;						\
	stn	os2, [os1];						\
	add	cpu, INTR_TAIL, os1;					\
	sll	ls1, CPTRSHIFT, os2;					\
	stn	%g0, [os1 + os2];					\
	mov	1, os1;							\
	sll	os1, ls1, os1;						\
	wr	os1, CLEAR_SOFTINT;					\
4:	ldn	[cpu + INTR_HEAD], os1;					\
	ld 	[os3 + INTR_NUMBER], inum;				\
	stn	os3, [cpu + INTR_HEAD];					\
	stn	os1, [os3 + INTR_NEXT];					\
	set	intr_vector, ls1;					\
	sll	inum, INTR_VECTOR_SHIFT, os1;				\
	add	ls1, os1, ls1;						\
	sth	%g0, [ls1 + IV_PENDING];				\
	wrpr	%g0, ls2, %pstate;					\
	SERVE_INTR_TRACE2(inum, os1, os2, os3, os4);			\
	ba,pt	%xcc, 0b;						\
5:	nop;

#ifdef TRAPTRACE
#define	SERVE_INTR_TRACE(inum, os1, os2, os3, os4)			\
	rdpr	%pstate, os3;						\
	andn	os3, PSTATE_IE | PSTATE_AM, os2;			\
	wrpr	%g0, os2, %pstate;					\
	TRACE_PTR(os1, os2);						\
	ldn	[os4 + PC_OFF], os2;					\
	stna	os2, [os1 + TRAP_ENT_TPC]%asi;				\
	ldx	[os4 + TSTATE_OFF], os2;				\
	stxa	os2, [os1 + TRAP_ENT_TSTATE]%asi;			\
	mov	os3, os4;						\
	GET_TRACE_TICK(os2); 						\
	stxa	os2, [os1 + TRAP_ENT_TICK]%asi;				\
	rdpr	%tl, os2;						\
	stha	os2, [os1 + TRAP_ENT_TL]%asi;				\
	set	TT_SERVE_INTR, os2;					\
	rdpr	%pil, os3;						\
	or	os2, os3, os2;						\
	stha	os2, [os1 + TRAP_ENT_TT]%asi;				\
	stna	%sp, [os1 + TRAP_ENT_SP]%asi;				\
	stna	inum, [os1 + TRAP_ENT_TR]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F1]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F2]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F3]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F4]%asi;				\
	TRACE_NEXT(os1, os2, os3);					\
	wrpr	%g0, os4, %pstate
#else	/* TRAPTRACE */
#define SERVE_INTR_TRACE(inum, os1, os2, os3, os4)
#endif	/* TRAPTRACE */

#ifdef TRAPTRACE
#define	SERVE_INTR_TRACE2(inum, os1, os2, os3, os4)			\
	rdpr	%pstate, os3;						\
	andn	os3, PSTATE_IE | PSTATE_AM, os2;			\
	wrpr	%g0, os2, %pstate;					\
	TRACE_PTR(os1, os2);						\
	stna	%g0, [os1 + TRAP_ENT_TPC]%asi;				\
	stxa	%g0, [os1 + TRAP_ENT_TSTATE]%asi;			\
	mov	os3, os4;						\
	GET_TRACE_TICK(os2); 						\
	stxa	os2, [os1 + TRAP_ENT_TICK]%asi;				\
	rdpr	%tl, os2;						\
	stha	os2, [os1 + TRAP_ENT_TL]%asi;				\
	set	TT_SERVE_INTR, os2;					\
	rdpr	%pil, os3;						\
	or	os2, os3, os2;						\
	stha	os2, [os1 + TRAP_ENT_TT]%asi;				\
	stna	%sp, [os1 + TRAP_ENT_SP]%asi;				\
	stna	inum, [os1 + TRAP_ENT_TR]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F1]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F2]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F3]%asi;				\
	stna	%g0, [os1 + TRAP_ENT_F4]%asi;				\
	TRACE_NEXT(os1, os2, os3);					\
	wrpr	%g0, os4, %pstate
#else	/* TRAPTRACE */
#define SERVE_INTR_TRACE2(inum, os1, os2, os3, os4)
#endif	/* TRAPTRACE */

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
intr_thread(struct regs *regs, u_int inumber, u_int pil)
{}

#else	/* lint */

/*
 * Handle an interrupt in a new thread.
 *	Entry:
 *		%o0       = pointer to regs structure
 *		%o1       = inumber
 *		%o2       = pil
 *		%sp       = on current thread's kernel stack
 *		%o7       = return linkage to trap code
 *		%g7       = current thread
 *		%pstate   = normal globals, interrupts enabled, 
 *		            privileged, fp disabled
 *		%pil      = LOCK_LEVEL
 *
 *	Register Usage
 *		%l0       = return linkage
 *		%l1       = pil
 *		%l2 - %l3 = scratch
 *		%l4 - %l7 = reserved for sys_trap
 *		%o2       = cpu
 *		%o3       = intr thread
 *		%o0       = scratch
 *		%o4 - %o5 = scratch
 */
	ENTRY_NP(intr_thread)
	mov	%o7, %l0
	mov	%o2, %l1
	!
	! Get set to run interrupt thread.
	! There should always be an interrupt thread since we allocate one
	! for each level on the CPU, and if we release an interrupt, a new
	! thread gets created.
	!
	ldn	[THREAD_REG + T_CPU], %o2
	ldn	[%o2 + CPU_INTR_THREAD], %o3	! interrupt thread pool
	ldn	[%o3 + T_LINK], %o4		! unlink thread from CPU's list
	stn	%o4, [%o2 + CPU_INTR_THREAD]
	!
	! Consider the new thread part of the same LWP so that
	! window overflow code can find the PCB.
	!
	ldn	[THREAD_REG + T_LWP], %o4
	stn	%o4, [%o3 + T_LWP]
	!
	! Threads on the interrupt thread free list could have state already
	! set to TS_ONPROC, but it helps in debugging if they're TS_FREE
	! Could eliminate the next two instructions with a little work.
	!
	mov	TS_ONPROC, %o4
	st	%o4, [%o3 + T_STATE]
	!
	! Push interrupted thread onto list from new thread.
	! Set the new thread as the current one.
	! Set interrupted thread's T_SP because if it is the idle thread,
	! resume may use that stack between threads.
	!
	stn	%o7, [THREAD_REG + T_PC]	! mark pc for resume
	stn	%sp, [THREAD_REG + T_SP]	! mark stack for resume
	stn	THREAD_REG, [%o3 + T_INTR]	! push old thread
	stn	%o3, [%o2 + CPU_THREAD]		! set new thread
	mov	%o3, THREAD_REG			! set global curthread register
#if STACK_BIAS != 0
	ldn	[%o3 + T_STACK], %o4		! interrupt stack pointer
	sub	%o4, STACK_BIAS, %sp
#else
	ldn	[%o3 + T_STACK], %sp		! interrupt stack pointer
#endif
	!
	! Initialize thread priority level from intr_pri
	!
	sethi	%hi(intr_pri), %o4
	ldsh	[%o4 + %lo(intr_pri)], %o4	! grab base interrupt priority
	add	%l1, %o4, %o4		! convert level to dispatch priority
	sth	%o4, [THREAD_REG + T_PRI]
	stub	%l1, [THREAD_REG + T_PIL]	! save pil for intr_passivate
	wrpr	%g0, %l1, %pil			! lower %pil to new level
	!
	! Fast event tracing.
	!
	ld	[%o2 + CPU_FTRACE_STATE], %o4	! %o2 = curthread->t_cpu
	btst	FTRACE_ENABLED, %o4
	be,pt	%icc, 1f			! skip if ftrace disabled
	  mov	%l1, %o5
	!
	! Tracing is enabled - write the trace entry.
	!
	save	%sp, -SA(MINFRAME), %sp
	set	ftrace_intr_thread_format_str, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	call	ftrace_3
	mov	%i5, %o3
	restore
1:
	!
	! call the handler
	!
	SERVE_INTR(%o1, %o2, %l2, %l3, %o4, %o5, %o3, %o0)
	!
	! update cpu_sysinfo.intrthread  - interrupts as threads (below clock)
	!
	ld	[%o2 + CPU_SYSINFO_INTRTHREAD], %o3
	inc	%o3
	st	%o3, [%o2 + CPU_SYSINFO_INTRTHREAD]
	!
	! If there is still an interrupted thread underneath this one,
	! then the interrupt was never blocked or released and the
	! return is fairly simple.  Otherwise jump to intr_thread_exit.
	!
	wrpr	%g0, LOCK_LEVEL, %pil
	ldn	[THREAD_REG + T_INTR], %o4	! pinned thread
	brz,pn	%o4, intr_thread_exit		! branch if none
	nop
	!
	! link the thread back onto the interrupt thread pool
	!
	ldn	[%o2 + CPU_INTR_THREAD], %o3
	stn	%o3, [THREAD_REG + T_LINK]
	stn	THREAD_REG, [%o2 + CPU_INTR_THREAD]
	!	
	! set the thread state to free so kadb doesn't see it
	!
	mov	TS_FREE, %o5
	st	%o5, [THREAD_REG + T_STATE]
	!
	! Switch back to the interrupted thread and return
	!
	stn	%o4, [%o2 + CPU_THREAD]
	mov	%o4, THREAD_REG
	ldn	[THREAD_REG + T_SP], %sp		! restore %sp

	jmp	%l0 + 8
	nop
	SET_SIZE(intr_thread)
	/* Not Reached */

	!
	! An interrupt returned on what was once (and still might be)
	! an interrupt thread stack, but the interrupted process is no longer
	! there.  This means the interrupt must've blocked or called
	! release_interrupt(). (XXX release_interrupt() is not used by anyone)
	!
	! There is no longer a thread under this one, so put this thread back 
	! on the CPU's free list and resume the idle thread which will dispatch
	! the next thread to run.
	!
	! All traps below LOCK_LEVEL are disabled here, but the mondo interrupt
	! is enabled.
	!
	ENTRY_NP(intr_thread_exit)
#ifdef TRAPTRACE
	rdpr	%pstate, %l2
	andn	%l2, PSTATE_IE | PSTATE_AM, %o4
	wrpr	%g0, %o4, %pstate			! cpu to known state
	TRACE_PTR(%o4, %o5)
	GET_TRACE_TICK(%o5)
	stxa	%o5, [%o4 + TRAP_ENT_TICK]%asi
	rdpr	%tl, %o5
	stha	%o5, [%o4 + TRAP_ENT_TL]%asi
	set	TT_INTR_EXIT, %o5
	stha	%o5, [%o4 + TRAP_ENT_TT]%asi
	stna	%g0, [%o4 + TRAP_ENT_TPC]%asi
	stxa	%g0, [%o4 + TRAP_ENT_TSTATE]%asi
	stna	%sp, [%o4 + TRAP_ENT_SP]%asi
	stna	THREAD_REG, [%o4 + TRAP_ENT_TR]%asi
	ld	[%o2 + CPU_BASE_SPL], %o5
	stna	%o5, [%o4 + TRAP_ENT_F1]%asi
	stna	%g0, [%o4 + TRAP_ENT_F2]%asi
	stna	%g0, [%o4 + TRAP_ENT_F3]%asi
	stna	%g0, [%o4 + TRAP_ENT_F4]%asi
	TRACE_NEXT(%o4, %o5, %o0)
	wrpr	%g0, %l2, %pstate
#endif /* TRAPTRACE */
        ld	[%o2 + CPU_SYSINFO_INTRBLK], %o4   ! cpu_sysinfo.intrblk++
        inc     %o4
        st	%o4, [%o2 + CPU_SYSINFO_INTRBLK]
	!
	! Put thread back on either the interrupt thread list if it is
	! still an interrupt thread, or the CPU's free thread list, if it did a
	! release interrupt.
	!
	lduh	[THREAD_REG + T_FLAGS], %o5
	btst	T_INTR_THREAD, %o5		! still an interrupt thread?
	bz,pn	%xcc, 1f			! No, so put back on free list
	mov	1, %o0				! delay
	!
	! This was an interrupt thread, so clear the pending interrupt flag
	! for this level.
	!
	ld	[%o2 + CPU_INTR_ACTV], %o5	! get mask of interrupts active
	sll	%o0, %l1, %o0			! form mask for level
	andn	%o5, %o0, %o5			! clear interrupt flag
	call	_intr_set_spl			! set CPU's base SPL level
	st	%o5, [%o2 + CPU_INTR_ACTV]	! delay - store active mask
	!
	! Set the thread state to free so kadb doesn't see it
	!
	mov	TS_FREE, %o4
	st	%o4, [THREAD_REG + T_STATE]
	!
	! Put thread on either the interrupt pool or the free pool and
	! call swtch() to resume another thread.
	!
	ldn	[%o2 + CPU_INTR_THREAD], %o5	! get list pointer
	stn	%o5, [THREAD_REG + T_LINK]
	call	swtch				! switch to best thread
	stn	THREAD_REG, [%o2 + CPU_INTR_THREAD] ! delay - put thread on list
	ba,a,pt	%xcc, .				! swtch() shouldn't return
1:
	mov	TS_ZOMB, %o4			! set zombie so swtch will free
	call	swtch				! run next process - free thread
	st	%o4, [THREAD_REG + T_STATE]	! delay - set state to zombie
	ba,a,pt	%xcc, .				! swtch() shouldn't return
	SET_SIZE(intr_thread_exit)

	.seg	".data"
	.align	4
	.global ftrace_intr_thread_format_str
ftrace_intr_thread_format_str:
	.asciz	"intr_thread(): regs=0x%lx, int=0x%lx, pil=0x%lx"
	.align	4
	.seg	".text"
#endif	/* lint */

#if defined(lint)

/*
 * Handle an interrupt in the current thread
 *	Entry:
 *		%o0       = pointer to regs structure
 *		%o1       = inumber
 *		%o2       = pil
 *		%sp       = on current thread's kernel stack
 *		%o7       = return linkage to trap code
 *		%g7       = current thread
 *		%pstate   = normal globals, interrupts enabled, 
 *		            privileged, fp disabled
 *		%pil      = PIL_MAX
 *
 *	Register Usage
 *		%l0       = return linkage
 *		%l1       = old stack
 *		%l2 - %l3 = scratch
 *		%l4 - %l7 = reserved for sys_trap
 *		%o3       = cpu
 *		%o0       = scratch
 *		%o4 - %o5 = scratch
 */
/* ARGSUSED */
void
current_thread(struct regs *regs, u_int inumber, u_int pil)
{}

#else	/* lint */

	ENTRY_NP(current_thread)
	
	mov	%o7, %l0
	ldn	[THREAD_REG + T_CPU], %o3
	ld	[%o3 + CPU_ON_INTR], %o4
	!
	! Handle high_priority nested interrupt on separate interrupt stack
	!
	tst	%o4
	inc	%o4
	bnz,pn	%xcc, 1f			! already on the stack
	st	%o4, [%o3 + CPU_ON_INTR]
	mov	%sp, %l1
#if STACK_BIAS != 0
	ldn	[%o3 + CPU_INTR_STACK], %l3
	sub	%l3, STACK_BIAS, %sp
#else
	ldn	[%o3 + CPU_INTR_STACK], %sp	! get on interrupt stack
#endif
1:
#ifdef DEBUG
	!
	! ASSERT(%o2 > LOCK_LEVEL)
	!
	cmp	%o2, LOCK_LEVEL
	bg,pt	%xcc, 2f
	nop
	mov	CE_PANIC, %o0
	sethi	%hi(current_thread_wrong_pil), %o1
	call	cmn_err				! %o2 has the %pil already
	or	%o1, %lo(current_thread_wrong_pil), %o1
2:
#endif
	wrpr	%g0, %o2, %pil			! enable interrupts

	!
	! call the handler
	!
	SERVE_INTR(%o1, %o3, %l2, %l3, %o4, %o5, %o2, %o0)
	!
	! get back on current thread's stack
	!
	rdpr	%pil, %o2
	wrpr	%g0, PIL_MAX, %pil		! disable interrupts (1-15)

	cmp	%o2, PIL_14
	bne,pn	%xcc, 2f
	nop

	!
	! Load TICK_COMPARE into %o5; if bit 63 is set, then TICK_COMPARE is
	! disabled.  If TICK_COMPARE is enabled, we know that we need to
	! reenqueue the interrupt request structure.  We'll then check TICKINT
	! in SOFTINT; if it's set, then we know that we were in a TICK_COMPARE
	! interrupt.  In this case, TICK_COMPARE may have been rewritten
	! recently; we'll compare %o5 to the current time to verify that it's
	! in the future.  
	!
	! Note that %o5 is live until after 1f.
	!
	rd	TICK_COMPARE, %o5
	srlx	%o5, TICKINT_DIS_SHFT, %g1
	brnz,pt	%g1, 2f
	nop

	rdpr 	%pstate, %o1
	andn	%o1, PSTATE_IE, %g1
	call	tickcmpr_enqueue_req
	wrpr	%g0, %g1, %pstate		! Disable vec interrupts

	! Check SOFTINT for TICKINT
	rd	SOFTINT, %o4
	and	%o4, TICK_INT_MASK, %o4
	brz,a,pn %o4, 2f
	wrpr	%g0, %o1, %pstate		! Enable vec interrupts

	! clear TICKINT/STICKINT 
	sethi	%hi(STICK_INT_MASK), %o0
	or	%o0, TICK_INT_MASK, %o0
	wr	%o0, CLEAR_SOFTINT

	!
	! Now that we've cleared TICKINT, we can reread %tick and confirm
	! that the value we programmed is still in the future.  If it isn't,
	! we need to reprogram TICK_COMPARE to fire as soon as possible.
	!
	rdpr	%tick, %o0
	sllx	%o0, 1, %o0			! Clear the DIS bit
	srlx	%o0, 1, %o0
	cmp	%o5, %o0			! In the future?
	bg,a,pt	%xcc, 2f			! Yes, drive on.
	wrpr	%g0, %o1, %pstate		!   delay: enable vec intr

	!
	! If we're here, then we have programmed TICK_COMPARE with a %tick
	! which is in the past; we'll now load an initial step size, and loop
	! until we've managed to program TICK_COMPARE to fire in the future.
	!
	mov	8, %o4				! 8 = arbitrary inital step
1:
	add	%o0, %o4, %o5			! Add the step
	WR_TICKCMPR(%o5,__LINE__)		! Write TICK_COMPARE
	rdpr	%tick, %o0
	sllx	%o0, 1, %o0			! Clear the DIS bit
	srlx	%o0, 1, %o0
	cmp	%o5, %o0			! In the future?
	bg,a,pt	%xcc, 2f			! Yes, drive on.
	wrpr	%g0, %o1, %pstate		!    delay: enable vec intr
	ba	1b				! No, try again.
	sllx	%o4, 1, %o4			!    delay: double step size

2:
	! get back on current thread's stack
	!
	ld	[%o3 + CPU_ON_INTR], %o4
	dec	%o4				! decrement on_intr
	tst	%o4
	st	%o4, [%o3 + CPU_ON_INTR]	! store new on_intr
	movz	%xcc, %l1, %sp
	jmp	%l0 + 8
	wrpr	%g0, %o2, %pil			! enable interrupts
	SET_SIZE(current_thread)

#ifdef DEBUG
current_thread_wrong_pil:
	.seg	".data"
	.align	4
	.asciz	"current_thread: unexpected %pil level: %d"
	.align	4
	.seg	".text"
#endif /* DEBUG */
#endif /* lint */

/*
 * Set CPU's base SPL level, based on which interrupt levels are active.
 * 	Called at spl7 or above.
 */

#if defined(lint)

void
set_base_spl(void)
{}

#else	/* lint */

	ENTRY_NP(set_base_spl)
	ldn	[THREAD_REG + T_CPU], %o2	! load CPU pointer
	ld	[%o2 + CPU_INTR_ACTV], %o5	! load active interrupts mask

/*
 * WARNING: non-standard callinq sequence; do not call from C
 *	%o2 = pointer to CPU
 *	%o5 = updated CPU_INTR_ACTV
 */
_intr_set_spl:					! intr_thread_exit enters here
	!
	! Determine highest interrupt level active.  Several could be blocked
	! at higher levels than this one, so must convert flags to a PIL
	! Normally nothing will be blocked, so test this first.
	!
	brz,pt	%o5, 1f				! nothing active
	sra	%o5, 11, %o3			! delay - set %o3 to bits 15-11
	set	_intr_flag_table, %o1
	tst	%o3				! see if any of the bits set
	ldub	[%o1 + %o3], %o3		! load bit number
	bnz,a,pn %xcc, 1f			! yes, add 10 and we're done
	add	%o3, 11-1, %o3			! delay - add bit number - 1

	sra	%o5, 6, %o3			! test bits 10-6
	tst	%o3
	ldub	[%o1 + %o3], %o3
	bnz,a,pn %xcc, 1f
	add	%o3, 6-1, %o3

	sra	%o5, 1, %o3			! test bits 5-1
	ldub	[%o1 + %o3], %o3

	!
	! highest interrupt level number active is in %l6
	!
1:
	retl
	st	%o3, [%o2 + CPU_BASE_SPL]	! delay - store base priority
	SET_SIZE(set_base_spl)

/*
 * Table that finds the most significant bit set in a five bit field.
 * Each entry is the high-order bit number + 1 of it's index in the table.
 * This read-only data is in the text segment.
 */
_intr_flag_table:
	.byte	0, 1, 2, 2,	3, 3, 3, 3,	4, 4, 4, 4,	4, 4, 4, 4
	.byte	5, 5, 5, 5,	5, 5, 5, 5,	5, 5, 5, 5,	5, 5, 5, 5
	.align	4

#endif	/* lint */

/*
 * int
 * intr_passivate(from, to)
 *	kthread_id_t	from;		interrupt thread
 *	kthread_id_t	to;		interrupted thread
 */

#if defined(lint)

/* ARGSUSED */
int
intr_passivate(kthread_id_t from, kthread_id_t to)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_passivate)
	save	%sp, -SA(MINFRAME), %sp	! get a new window 

	flushw				! force register windows to stack
	!
	! restore registers from the base of the stack of the interrupt thread.
	!
	ldn	[%i0 + T_STACK], %i2	! get stack save area pointer
	ldn	[%i2 + (0*GREGSIZE)], %l0	! load locals
	ldn	[%i2 + (1*GREGSIZE)], %l1
	ldn	[%i2 + (2*GREGSIZE)], %l2
	ldn	[%i2 + (3*GREGSIZE)], %l3
	ldn	[%i2 + (4*GREGSIZE)], %l4
	ldn	[%i2 + (5*GREGSIZE)], %l5
	ldn	[%i2 + (6*GREGSIZE)], %l6
	ldn	[%i2 + (7*GREGSIZE)], %l7
	ldn	[%i2 + (8*GREGSIZE)], %o0	! put ins from stack in outs
	ldn	[%i2 + (9*GREGSIZE)], %o1
	ldn	[%i2 + (10*GREGSIZE)], %o2
	ldn	[%i2 + (11*GREGSIZE)], %o3
	ldn	[%i2 + (12*GREGSIZE)], %o4
	ldn	[%i2 + (13*GREGSIZE)], %o5
	ldn	[%i2 + (14*GREGSIZE)], %i4
					! copy stack/pointer without using %sp
	ldn	[%i2 + (15*GREGSIZE)], %i5
	!
	! put registers into the save area at the top of the interrupted
	! thread's stack, pointed to by %l7 in the save area just loaded.
	!
	ldn	[%i1 + T_SP], %i3	! get stack save area pointer
	stn	%l0, [%i3 + STACK_BIAS + (0*GREGSIZE)]	! save locals
	stn	%l1, [%i3 + STACK_BIAS + (1*GREGSIZE)]
	stn	%l2, [%i3 + STACK_BIAS + (2*GREGSIZE)]
	stn	%l3, [%i3 + STACK_BIAS + (3*GREGSIZE)]
	stn	%l4, [%i3 + STACK_BIAS + (4*GREGSIZE)]
	stn	%l5, [%i3 + STACK_BIAS + (5*GREGSIZE)]
	stn	%l6, [%i3 + STACK_BIAS + (6*GREGSIZE)]
	stn	%l7, [%i3 + STACK_BIAS + (7*GREGSIZE)]
	stn	%o0, [%i3 + STACK_BIAS + (8*GREGSIZE)]	! save ins using outs
	stn	%o1, [%i3 + STACK_BIAS + (9*GREGSIZE)]
	stn	%o2, [%i3 + STACK_BIAS + (10*GREGSIZE)]
	stn	%o3, [%i3 + STACK_BIAS + (11*GREGSIZE)]
	stn	%o4, [%i3 + STACK_BIAS + (12*GREGSIZE)]
	stn	%o5, [%i3 + STACK_BIAS + (13*GREGSIZE)]
	stn	%i4, [%i3 + STACK_BIAS + (14*GREGSIZE)]
						! fp, %i7 copied using %i4
	stn	%i5, [%i3 + STACK_BIAS + (15*GREGSIZE)]
	stn	%g0, [%i2 + ((8+6)*GREGSIZE)]
						! clear fp in save area
	
	! load saved pil for return
	ldub	[%i0 + T_PIL], %i0
	ret
	restore
	SET_SIZE(intr_passivate)

#endif	/* lint */

/*
 * Return a thread's interrupt level.
 * Since this isn't saved anywhere but in %l4 on interrupt entry, we
 * must dig it out of the save area.
 *
 * Caller 'swears' that this really is an interrupt thread.
 *
 * int
 * intr_level(t)
 *	kthread_id_t	t;
 */

#if defined(lint)

/* ARGSUSED */
int
intr_level(kthread_id_t t)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_level)
	retl
	ldub	[%o0 + T_PIL], %o0		! return saved pil
	SET_SIZE(intr_level)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
disable_pil_intr()
{ return (0); }

#else	/* lint */

	ENTRY_NP(disable_pil_intr)
	rdpr	%pil, %o0
	retl
	wrpr	%g0, PIL_MAX, %pil		! disable interrupts (1-15)
	SET_SIZE(disable_pil_intr)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
enable_pil_intr(int pil_save)
{}

#else	/* lint */

	ENTRY_NP(enable_pil_intr)
	retl
	wrpr	%o0, %pil
	SET_SIZE(enable_pil_intr)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
u_int
disable_vec_intr(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(disable_vec_intr)
	rdpr	%pstate, %o0
	andn	%o0, PSTATE_IE, %g1
	retl
	wrpr	%g0, %g1, %pstate		! disable interrupt
	SET_SIZE(disable_vec_intr)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
enable_vec_intr(u_int pstate_save)
{}

#else	/* lint */

	ENTRY_NP(enable_vec_intr)
	retl
	wrpr	%g0, %o0, %pstate
	SET_SIZE(enable_vec_intr)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
setsoftint(u_int inum)
{}

#else	/* lint */

	ENTRY_NP(setsoftint)
	save	%sp, -SA(MINFRAME), %sp	! get a new window 
	rdpr	%pstate, %l5
	wrpr	%l5, PSTATE_IE, %pstate		! disable interrupt
	!
	! Fetch data from intr_vector[] table according to the inum.
	!
	! We have an interrupt number.
	! Put the request on the cpu's softint list,
	! and set %set_softint.
	!
	! Register usage
	!	%i0 - inumber
	!	%l2 - requested pil
	!	%l3 - intr_req
	!	%l4 - *cpu
	!	%l1, %l6 - temps
	!
	! check if a softint is pending for this inum already
	! if one is pending, don't bother queuing another
	!
	set	intr_vector, %l1
	sll	%i0, INTR_VECTOR_SHIFT, %l6
	add	%l1, %l6, %l1			! %l1 = &intr_vector[inum]
	lduh	[%l1 + IV_PENDING], %l6
	brnz,pn	%l6, 4f				! branch, if pending
	or	%g0, 1, %l2
	sth	%l2, [%l1 + IV_PENDING]		! intr_vector[inum].pend = 1
	!
	! allocate an intr_req from the free list
	!
	CPU_ADDR(%l4, %l2)
	ldn	[%l4 + INTR_HEAD], %l3
	lduh	[%l1 + IV_PIL], %l2
	!
	! fixup free list
	!
	ldn	[%l3 + INTR_NEXT], %l6
	stn	%l6, [%l4 + INTR_HEAD]
	!
	! fill up intr_req
	!
	st	%i0, [%l3 + INTR_NUMBER]
	stn	%g0, [%l3 + INTR_NEXT]
	!
	! move intr_req to appropriate list
	!
	sll	%l2, CPTRSHIFT, %l0
	add	%l4, INTR_TAIL, %l6
	ldn	[%l6 + %l0], %l1	! current tail
	brz,pt	%l1, 2f			! branch if list empty
	stn	%l3, [%l6 + %l0]	! make intr_req new tail
	!
	! there's pending intr_req already
	!
	ba,pt	%xcc, 3f
	stn	%l3, [%l1 + INTR_NEXT]	! update old tail
2:
	!
	! no pending intr_req; make intr_req new head
	!
	add	%l4, INTR_HEAD, %l6
	stn	%l3, [%l6 + %l0]
3:
	!
	! Write %set_softint with (1<<pil) to cause a "pil" level trap
	!
	mov	1, %l1
	sll	%l1, %l2, %l1
	wr	%l1, SET_SOFTINT
4:
	wrpr	%g0, %l5, %pstate
	ret
	restore
	SET_SIZE(setsoftint)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
clear_soft_intr(u_int pil)
{}

#else	/* lint */

	ENTRY_NP(clear_soft_intr)
	mov	1, %o1
	sll	%o1, %o0, %o1
	retl
	wr	%o1, CLEAR_SOFTINT
	SET_SIZE(clear_soft_intr)

#endif	/* lint */


#if defined(lint)
 
void
cbe_level14(void)
{}

#else   /* lint */

        ENTRY_NP(cbe_level14)
        save    %sp, -SA(MINFRAME), %sp ! get a new window
 
 	!
        ! Make sure that this is from TICK_COMPARE; if not just return
	!
        rd      SOFTINT, %l1
	sethi	%hi(STICK_INT_MASK), %o2
	or	%o2, TICK_INT_MASK, %o2
        and     %l1, %o2, %l1
        brz,pn  %l1, 2f
        nop

        CPU_ADDR(%o1, %o2)
        call    cyclic_fire
	mov	%o1, %o0
2:
        ret
        restore %g0, 1, %o0
        SET_SIZE(cbe_level14)

#endif  /* lint */

#if defined(lint)
/*
 * Softint generated when counter field of tick reg matches value field 
 * of tick_cmpr reg
 */

/* ARGSUSED */
void
tickcmpr_set(uint64_t clock_cycles)
{}

#else   /* lint */

	ENTRY_NP(tickcmpr_set)

	! get 64-bit clock_cycles interval
#ifdef __sparcv9
	mov	%o0, %o2
#else
	sllx	%o0, 0x20, %o2
	or	%o2, %o1, %o2
#endif
	mov	8, %o3			! A reasonable initial step size
1:
	WR_TICKCMPR(%o2,__LINE__)	! Write to TICK_CMPR

	rdpr	%tick, %o0		! Read %tick to confirm that the
	sllx	%o0, 1, %o0		!   value we wrote was in the future.
	srlx	%o0, 1, %o0

	cmp	%o2, %o0		! If the value we wrote was in the
	bg,pt	%xcc, 2f		!   future, then blow out of here.
	sllx	%o3, 1, %o3		! If not, then double our step size,
	ba,pt	%xcc, 1b		!   and take another lap.
	add	%o0, %o3, %o2		!
2:
	retl
	nop
	SET_SIZE(tickcmpr_set)

#endif  /* lint */

#if defined(lint)

void
tickcmpr_disable()
{}

#else

	ENTRY_NP(tickcmpr_disable)
	mov	1, %g1
	sllx	%g1, TICKINT_DIS_SHFT, %o0
	WR_TICKCMPR(%o0,__LINE__)	! Write to TICK_CMPR
	retl
	nop
	SET_SIZE(tickcmpr_disable)

#endif

#if defined(lint)

/*
 * tick_write_delta() increments %tick by the specified delta.  This should
 * only be called after a CPR event to assure that gethrtime() continues to
 * increase monotonically.  Obviously, writing %tick needs to de done very
 * carefully to avoid introducing unnecessary %tick skew across CPUs.  For
 * this reason, we make sure we're i-cache hot before actually writing to
 * %tick.
 */
/*ARGSUSED*/
void
tick_write_delta(uint64_t delta)
{}

#else	/* lint */

#ifdef DEBUG
	.seg	".data"
	.align	4
tick_write_panic:
	.asciz	"tick_write_delta: interrupts already disabled on entry"
#endif 

	ENTRY_NP(tick_write_delta)

	rdpr	%pstate, %g1

#ifdef DEBUG
	andcc	%g1, PSTATE_IE, %g0	! If DEBUG, check that interrupts
	bnz	0f			! aren't already disabled.
	sethi	%hi(tick_write_panic), %o1
        save    %sp, -SA(MINFRAME), %sp ! get a new window to preserve caller
	call	panic
	or	%i1, %lo(tick_write_panic), %o0
#endif
0:
	wrpr	%g1, PSTATE_IE, %pstate	! Disable interrupts

#ifdef __sparcv9
	mov	%o0, %o2
#else
	sllx	%o0, 32, %o2		! Slurp 64-bit value.
	or	%o2, %o1, %o2
#endif
	ba	0f			! Branch to cache line-aligned instr.
	nop

	.align	16
0:
	nop				! The next 3 instructions are now hot.
	rdpr	%tick, %o0		! Read %tick
	add	%o0, %o2, %o0		! Add the delta
	wrpr	%o0, %tick		! Write the tick register

	retl				! Return
	wrpr	%g0, %g1, %pstate	!     delay: Re-enable interrupts
#endif

#if defined(lint)
/*
 *  return 1 if disabled
 */

int
tickcmpr_disabled(void)
{ return (0); }

#else   /* lint */

	ENTRY_NP(tickcmpr_disabled)
	rd	TICK_COMPARE, %g1
	retl
	srlx	%g1, TICKINT_DIS_SHFT, %o0
	SET_SIZE(tickcmpr_disabled)

#endif  /* lint */

#if defined(lint)

void
tickcmpr_enqueue_req()
{}

#else   /* lint */

	ENTRY_NP(tickcmpr_enqueue_req)
	sethi	%hi(cbe_level14_inum), %o0
	ld	[%o0 + %lo(cbe_level14_inum)], %g5
	mov	PIL_14, %g2

	! get intr_req free list
	CPU_ADDR(%g4, %g1)
	ldn	[%g4 + INTR_HEAD], %g3  

	! take intr_req from free list
	ldn	[%g3 + INTR_NEXT], %g6
	stn	%g6, [%g4 + INTR_HEAD]

	! fill up intr_req
	st	%g5, [%g3 + INTR_NUMBER]
	stn	%g0, [%g3 + INTR_NEXT]

	! add intr_req to proper pil list
	sll	%g2, CPTRSHIFT, %g5
	add	%g4, INTR_TAIL, %g6
	ldn	[%g5 + %g6], %g1	! current tail
	brz,pt	%g1, 2f			! branch if list is empty
	stn	%g3, [%g6 + %g5]	! make intr_req the new tail

	! an intr_req was already queued so update old tail
	ba,pt	%xcc, 3f
	stn	%g3, [%g1 + INTR_NEXT]
2:
	! no intr_req's queued so make intr_req the new head
	add	%g4, INTR_HEAD, %g6
	stn	%g3, [%g6 + %g5]
3:
	retl
	nop
	SET_SIZE(tickcmpr_enqueue_req)

#endif  /* lint */

#if defined(lint)

void
tickcmpr_dequeue_req()
{}

#else   /* lint */

	ENTRY_NP(tickcmpr_dequeue_req)
	sethi	%hi(cbe_level14_inum), %o0
	ld	[%o0 + %lo(cbe_level14_inum)], %g5

	mov	PIL_14, %g2
	sll	%g2, CPTRSHIFT, %g2

	! begin search with head of pil list
	CPU_ADDR(%g4, %g1)
	add	%g4, INTR_HEAD, %g6
	add	%g6, %g2, %g6		! pil queue head
	ldn	[%g6], %g1  		! first entry
	mov	%g6, %g3		! use %g3 as intr_req prev
1:
	brz,pn	%g1, 5f			! branch if list empty 
	nop
	ld	[%g1 + INTR_NUMBER], %o0		 
	cmp	%o0, %g5
	be,pt  %xcc, 2f
	nop
	or	%g0, %g1, %g3		
	add	%g3, INTR_NEXT, %g3	! %g3 is next of prev
	ba,pt	%xcc, 1b
	ldn	[%g1 + INTR_NEXT], %g1
2:
	! dequeue the found entry
	ldn	[%g1 + INTR_NEXT], %g5
	stn	%g5, [%g3]		! prev.next <- current.next
	brnz,pn  %g5, 4f		! branch if tail not reached	
	nop
	add	%g4, INTR_TAIL, %g5
	cmp	%g3, %g6			
	bne,pn  %xcc, 3f
	stn	%g0, [%g5 + %g2]
	mov	TICK_INT_MASK, %g3	! if queue now empty insure
	sll	%g3, PIL_14, %g2	! interrupt is clear
	or	%g2, %g3, %g3
	wr	%g3, CLEAR_SOFTINT
	ba,pt	%xcc, 4f
3:
	sub	%g3, INTR_NEXT, %g3
	stn	%g3, [%g5 + %g2]
4:
	! move the found entry to the free list 
	ldn	[%g4 + INTR_HEAD], %g5
	stn	%g1, [%g4 + INTR_HEAD]
	stn	%g5, [%g1 + INTR_NEXT]
5:
	retl
	nop
	SET_SIZE(tickcmpr_dequeue_req)

/*
 * Check shift value used for computing array offsets
 */ 
#if INTR_VECTOR_SIZE != (1 << INTR_VECTOR_SHIFT)
#error "INTR_VECTOR_SIZE has changed"
#endif

#endif  /* lint */
