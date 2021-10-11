/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)trap_table.s	1.132	99/10/21 SMI"

#if !defined(lint)
#include "assym.h"
#endif /* !lint */
#include <sys/asm_linkage.h>
#include <sys/privregs.h>
#include <sys/spitasi.h>
#include <sys/machtrap.h>
#include <sys/machthread.h>
#include <sys/pcb.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/machpcb.h>
#include <sys/async.h>
#include <sys/intreg.h>
#include <sys/scb.h>
#include <sys/psr_compat.h>
#include <sys/syscall.h>
#include <sys/machparam.h>
#include <sys/traptrace.h>
#include <vm/hat_sfmmu.h>
#include <sys/archsystm.h>
#include <sys/utrap.h>
#include <sys/clock.h>

/*
 * SPARC V9 Trap Table
 *
 * Most of the trap handlers are made from common building
 * blocks, and some are instantiated multiple times within
 * the trap table. So, I build a bunch of macros, then
 * populate the table using only the macros.
 *
 * Many macros branch to sys_trap.  Its calling convention is:
 *	%g1		kernel trap handler
 *	%g2, %g3	args for above
 *	%g4		desire %pil
 */

#ifdef	TRAPTRACE

/*
 * Tracing macro. Adds two instructions if TRAPTRACE is defined.
 */
#define	TT_TRACE(label)		\
	ba	label		;\
	rd	%pc, %g7
#define	TT_TRACE_INS	2

#define	TT_TRACE_L(label)	\
	ba	label		;\
	rd	%pc, %l4	;\
	clr	%l4
#define	TT_TRACE_L_INS	3

#else

#define	TT_TRACE(label)
#define	TT_TRACE_INS	0

#define	TT_TRACE_L(label)
#define	TT_TRACE_L_INS	0

#endif

/*
 * This macro is used to update per cpu mmu stats in perf critical
 * paths. It is only enabled in debug kernels or if SFMMU_STAT_GATHER
 * is defined.
 */
#if defined(DEBUG) || defined(SFMMU_STAT_GATHER)
#define	HAT_PERCPU_DBSTAT(stat)			\
	mov	stat, %g1		;\
	ba	stat_mmu		;\
	rd	%pc, %g7
#else
#define	HAT_PERCPU_DBSTAT(stat)
#endif /* DEBUG || SFMMU_STAT_GATHER */

/*
 * This first set are funneled to trap() with %tt as the type.
 * Trap will then either panic or send the user a signal.
 */
/*
 * NOT is used for traps that just shouldn't happen.
 * It comes in both single and quadruple flavors.
 */
#if !defined(lint)
	.global	trap
#endif /* !lint */
#define	NOT			\
	TT_TRACE(trace_gen)	;\
	set	trap, %g1	;\
	rdpr	%tt, %g3	;\
	ba,pt	%xcc, sys_trap	;\
	sub	%g0, 1, %g4	;\
	.align	32
#define	NOT4	NOT; NOT; NOT; NOT
/*
 * RED is for traps that use the red mode handler.
 * We should never see these either.
 */
#define	RED	NOT
/*
 * BAD is used for trap vectors we don't have a kernel
 * handler for.
 * It also comes in single and quadruple versions.
 */
#define	BAD	NOT
#define	BAD4	NOT4

#define	DONE			\
	done;			\
	.align	32

/*
 * TRAP vectors to the trap() function.
 * It's main use is for user errors.
 */
#if !defined(lint)
	.global	trap
#endif /* !lint */
#define	TRAP(arg)		\
	TT_TRACE(trace_gen)	;\
	set	trap, %g1	;\
	mov	arg, %g3	;\
	ba,pt	%xcc, sys_trap	;\
	sub	%g0, 1, %g4	;\
	.align	32

/*
 * SYSCALL is used for system calls on both ILP32 and LP64 kernels
 * depending on the "which" parameter (should be either syscall_trap
 * or syscall_trap32).
 */
#define	SYSCALL(which)			\
	TT_TRACE(trace_gen)		;\
	set	(which), %g1		;\
	ba,pt	%xcc, sys_trap		;\
	sub	%g0, 1, %g4		;\
	.align	32

#define	FLUSHW()			\
	set	trap, %g1		;\
	mov	T_FLUSH_PCB, %g3	;\
	sub	%g0, 1, %g4		;\
	save				;\
	flushw				;\
	restore				;\
	done				;\
	.align	32

/*
 * GOTO just jumps to a label.
 * It's used for things that can be fixed without going thru sys_trap.
 */
#define	GOTO(label)		\
	.global	label		;\
	ba,a	label		;\
	.empty			;\
	.align	32

/*
 * GOTO_TT just jumps to a label.
 * asynchronous traps at level 0 will use this macro.
 * correctable ECC error traps at  level 0 and 1 will use this macro.
 * It's used for things that can be fixed without going thru sys_trap.
 */
#define	GOTO_TT(label, ttlabel)		\
	.global	label		;\
	TT_TRACE(ttlabel)	;\
	ba,a	label		;\
	.empty			;\
	.align	32

/*
 * Privileged traps
 * Takes breakpoint if privileged, calls trap() if not.
 */
#define	PRIV(label)			\
	rdpr	%tstate, %g1		;\
	btst	TSTATE_PRIV, %g1	;\
	bnz	label			;\
	rdpr	%tt, %g3		;\
	set	trap, %g1		;\
	ba,pt	%xcc, sys_trap		;\
	sub	%g0, 1, %g4		;\
	.align	32


/*
 * Trace traps.
 */
#ifdef TRACE
#define	TRACE_TRAP(H)	GOTO(H)
#else
#define	TRACE_TRAP(H)	BAD
#endif

/*
 * REGISTER WINDOW MANAGEMENT MACROS
 */

/*
 * various convenient units of padding
 */
#define	SKIP(n)	.skip 4*(n)

/*
 * CLEAN_WINDOW is the simple handler for cleaning a register window.
 */
#define	CLEAN_WINDOW						\
	TT_TRACE_L(trace_win)					;\
	rdpr %cleanwin, %l0; inc %l0; wrpr %l0, %cleanwin	;\
	clr %l0; clr %l1; clr %l2; clr %l3			;\
	clr %l4; clr %l5; clr %l6; clr %l7			;\
	clr %o0; clr %o1; clr %o2; clr %o3			;\
	clr %o4; clr %o5; clr %o6; clr %o7			;\
	retry; .align 128

#if !defined(lint)

/*
 * If we get an unresolved tlb miss while in a window handler, the fault
 * handler will resume execution at the last instruction of the window
 * hander, instead of delivering the fault to the kernel.  Spill handlers
 * use this to spill windows into the wbuf.
 *
 * The mixed handler works by checking %sp, and branching to the correct
 * handler.  This is done by branching back to label 1: for 32b frames,
 * or label 2: for 64b frames; which implies the handler order is: 32b,
 * 64b, mixed.  The 1: and 2: labels are offset into the routines to
 * allow the branchs' delay slots to contain useful instructions.
 */

/*
 * SPILL_32bit spills a 32-bit-wide kernel register window.  It
 * assumes that the kernel context and the nucleus context are the
 * same.  The stack pointer is required to be eight-byte aligned even
 * though this code only needs it to be four-byte aligned.
 */
#define	SPILL_32bit(tail)					\
	srl	%sp, 0, %sp					;\
1:	st	%l0, [%sp + 0]					;\
	st	%l1, [%sp + 4]					;\
	st	%l2, [%sp + 8]					;\
	st	%l3, [%sp + 12]					;\
	st	%l4, [%sp + 16]					;\
	st	%l5, [%sp + 20]					;\
	st	%l6, [%sp + 24]					;\
	st	%l7, [%sp + 28]					;\
	st	%i0, [%sp + 32]					;\
	st	%i1, [%sp + 36]					;\
	st	%i2, [%sp + 40]					;\
	st	%i3, [%sp + 44]					;\
	st	%i4, [%sp + 48]					;\
	st	%i5, [%sp + 52]					;\
	st	%i6, [%sp + 56]					;\
	st	%i7, [%sp + 60]					;\
	TT_TRACE_L(trace_win)					;\
	saved							;\
	retry							;\
	SKIP(31-19-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_32bit_/**/tail			;\
	.empty

/*
 * SPILL_32bit_asi spills a 32-bit-wide register window into a 32-bit
 * wide address space via the designated asi.  It is used to spill
 * non-kernel windows.  The stack pointer is required to be eight-byte
 * aligned even though this code only needs it to be four-byte
 * aligned.
 */
#define	SPILL_32bit_asi(asi_num, tail)				\
	srl	%sp, 0, %sp					;\
1:	sta	%l0, [%sp + %g0]asi_num				;\
	mov	4, %g1						;\
	sta	%l1, [%sp + %g1]asi_num				;\
	mov	8, %g2						;\
	sta	%l2, [%sp + %g2]asi_num				;\
	mov	12, %g3						;\
	sta	%l3, [%sp + %g3]asi_num				;\
	add	%sp, 16, %g4					;\
	sta	%l4, [%g4 + %g0]asi_num				;\
	sta	%l5, [%g4 + %g1]asi_num				;\
	sta	%l6, [%g4 + %g2]asi_num				;\
	sta	%l7, [%g4 + %g3]asi_num				;\
	add	%g4, 16, %g4					;\
	sta	%i0, [%g4 + %g0]asi_num				;\
	sta	%i1, [%g4 + %g1]asi_num				;\
	sta	%i2, [%g4 + %g2]asi_num				;\
	sta	%i3, [%g4 + %g3]asi_num				;\
	add	%g4, 16, %g4					;\
	sta	%i4, [%g4 + %g0]asi_num				;\
	sta	%i5, [%g4 + %g1]asi_num				;\
	sta	%i6, [%g4 + %g2]asi_num				;\
	sta	%i7, [%g4 + %g3]asi_num				;\
	TT_TRACE_L(trace_win)					;\
	saved							;\
	retry							;\
	SKIP(31-25-TT_TRACE_L_INS)				;\
	ba,a,pt %xcc, fault_32bit_/**/tail			;\
	.empty

/*
 * SPILL_32bit_tt1 spills a 32-bit-wide register window into a 32-bit
 * wide address space via the designated asi.  It is used to spill
 * windows at tl>1 where performance isn't the primary concern and
 * where we don't want to use unnecessary registers.  The stack
 * pointer is required to be eight-byte aligned even though this code
 * only needs it to be four-byte aligned.
 */
#define	SPILL_32bit_tt1(asi_num, tail)				\
	mov	asi_num, %asi					;\
1:	srl	%sp, 0, %sp					;\
	sta	%l0, [%sp + 0]%asi				;\
	sta	%l1, [%sp + 4]%asi				;\
	sta	%l2, [%sp + 8]%asi				;\
	sta	%l3, [%sp + 12]%asi				;\
	sta	%l4, [%sp + 16]%asi				;\
	sta	%l5, [%sp + 20]%asi				;\
	sta	%l6, [%sp + 24]%asi				;\
	sta	%l7, [%sp + 28]%asi				;\
	sta	%i0, [%sp + 32]%asi				;\
	sta	%i1, [%sp + 36]%asi				;\
	sta	%i2, [%sp + 40]%asi				;\
	sta	%i3, [%sp + 44]%asi				;\
	sta	%i4, [%sp + 48]%asi				;\
	sta	%i5, [%sp + 52]%asi				;\
	sta	%i6, [%sp + 56]%asi				;\
	sta	%i7, [%sp + 60]%asi				;\
	TT_TRACE_L(trace_win)					;\
	saved							;\
	retry							;\
	SKIP(31-20-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_32bit_/**/tail			;\
	.empty


/*
 * FILL_32bit fills a 32-bit-wide kernel register window.  It assumes
 * that the kernel context and the nucleus context are the same.  The
 * stack pointer is required to be eight-byte aligned even though this
 * code only needs it to be four-byte aligned.
 */
#define	FILL_32bit(tail)					\
	srl	%sp, 0, %sp					;\
1:	TT_TRACE_L(trace_win)					;\
	ld	[%sp + 0], %l0					;\
	ld	[%sp + 4], %l1					;\
	ld	[%sp + 8], %l2					;\
	ld	[%sp + 12], %l3					;\
	ld	[%sp + 16], %l4					;\
	ld	[%sp + 20], %l5					;\
	ld	[%sp + 24], %l6					;\
	ld	[%sp + 28], %l7					;\
	ld	[%sp + 32], %i0					;\
	ld	[%sp + 36], %i1					;\
	ld	[%sp + 40], %i2					;\
	ld	[%sp + 44], %i3					;\
	ld	[%sp + 48], %i4					;\
	ld	[%sp + 52], %i5					;\
	ld	[%sp + 56], %i6					;\
	ld	[%sp + 60], %i7					;\
	restored						;\
	retry							;\
	SKIP(31-19-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_32bit_/**/tail			;\
	.empty

/*
 * FILL_32bit_asi fills a 32-bit-wide register window from a 32-bit
 * wide address space via the designated asi.  It is used to fill
 * non-kernel windows.  The stack pointer is required to be eight-byte
 * aligned even though this code only needs it to be four-byte
 * aligned.
 */
#define	FILL_32bit_asi(asi_num, tail)				\
	srl	%sp, 0, %sp					;\
1:	TT_TRACE_L(trace_win)					;\
	mov	4, %g1						;\
	lda	[%sp + %g0]asi_num, %l0				;\
	mov	8, %g2						;\
	lda	[%sp + %g1]asi_num, %l1				;\
	mov	12, %g3						;\
	lda	[%sp + %g2]asi_num, %l2				;\
	lda	[%sp + %g3]asi_num, %l3				;\
	add	%sp, 16, %g4					;\
	lda	[%g4 + %g0]asi_num, %l4				;\
	lda	[%g4 + %g1]asi_num, %l5				;\
	lda	[%g4 + %g2]asi_num, %l6				;\
	lda	[%g4 + %g3]asi_num, %l7				;\
	add	%g4, 16, %g4					;\
	lda	[%g4 + %g0]asi_num, %i0				;\
	lda	[%g4 + %g1]asi_num, %i1				;\
	lda	[%g4 + %g2]asi_num, %i2				;\
	lda	[%g4 + %g3]asi_num, %i3				;\
	add	%g4, 16, %g4					;\
	lda	[%g4 + %g0]asi_num, %i4				;\
	lda	[%g4 + %g1]asi_num, %i5				;\
	lda	[%g4 + %g2]asi_num, %i6				;\
	lda	[%g4 + %g3]asi_num, %i7				;\
	restored						;\
	retry							;\
	SKIP(31-25-TT_TRACE_L_INS)				;\
	ba,a,pt %xcc, fault_32bit_/**/tail			;\
	.empty

/*
 * FILL_32bit_tt1 fills a 32-bit-wide register window from a 32-bit
 * wide address space via the designated asi.  It is used to fill
 * windows at tl>1 where performance isn't the primary concern and
 * where we don't want to use unnecessary registers.  The stack
 * pointer is required to be eight-byte aligned even though this code
 * only needs it to be four-byte aligned.
 */
#define	FILL_32bit_tt1(asi_num, tail)				\
	mov	asi_num, %asi					;\
1:	srl	%sp, 0, %sp					;\
	TT_TRACE_L(trace_win)					;\
	lda	[%sp + 0]%asi, %l0				;\
	lda	[%sp + 4]%asi, %l1				;\
	lda	[%sp + 8]%asi, %l2				;\
	lda	[%sp + 12]%asi, %l3				;\
	lda	[%sp + 16]%asi, %l4				;\
	lda	[%sp + 20]%asi, %l5				;\
	lda	[%sp + 24]%asi, %l6				;\
	lda	[%sp + 28]%asi, %l7				;\
	lda	[%sp + 32]%asi, %i0				;\
	lda	[%sp + 36]%asi, %i1				;\
	lda	[%sp + 40]%asi, %i2				;\
	lda	[%sp + 44]%asi, %i3				;\
	lda	[%sp + 48]%asi, %i4				;\
	lda	[%sp + 52]%asi, %i5				;\
	lda	[%sp + 56]%asi, %i6				;\
	lda	[%sp + 60]%asi, %i7				;\
	restored						;\
	retry							;\
	SKIP(31-20-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_32bit_/**/tail			;\
	.empty


/*
 * SPILL_64bit spills a 64-bit-wide kernel register window.  It
 * assumes that the kernel context and the nucleus context are the
 * same.  The stack pointer is required to be eight-byte aligned.
 */
#define	SPILL_64bit(tail)					\
2:	stx	%l0, [%sp + V9BIAS64 + 0]			;\
	stx	%l1, [%sp + V9BIAS64 + 8]			;\
	stx	%l2, [%sp + V9BIAS64 + 16]			;\
	stx	%l3, [%sp + V9BIAS64 + 24]			;\
	stx	%l4, [%sp + V9BIAS64 + 32]			;\
	stx	%l5, [%sp + V9BIAS64 + 40]			;\
	stx	%l6, [%sp + V9BIAS64 + 48]			;\
	stx	%l7, [%sp + V9BIAS64 + 56]			;\
	stx	%i0, [%sp + V9BIAS64 + 64]			;\
	stx	%i1, [%sp + V9BIAS64 + 72]			;\
	stx	%i2, [%sp + V9BIAS64 + 80]			;\
	stx	%i3, [%sp + V9BIAS64 + 88]			;\
	stx	%i4, [%sp + V9BIAS64 + 96]			;\
	stx	%i5, [%sp + V9BIAS64 + 104]			;\
	stx	%i6, [%sp + V9BIAS64 + 112]			;\
	stx	%i7, [%sp + V9BIAS64 + 120]			;\
	TT_TRACE_L(trace_win)					;\
	saved							;\
	retry							;\
	SKIP(31-18-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_64bit_/**/tail			;\
	.empty

/*
 * SPILL_64bit_asi spills a 64-bit-wide register window into a 64-bit
 * wide address space via the designated asi.  It is used to spill
 * non-kernel windows.  The stack pointer is required to be eight-byte
 * aligned.
 */
#define	SPILL_64bit_asi(asi_num, tail)				\
	mov	0 + V9BIAS64, %g1				;\
2:	stxa	%l0, [%sp + %g1]asi_num				;\
	mov	8 + V9BIAS64, %g2				;\
	stxa	%l1, [%sp + %g2]asi_num				;\
	mov	16 + V9BIAS64, %g3				;\
	stxa	%l2, [%sp + %g3]asi_num				;\
	mov	24 + V9BIAS64, %g4				;\
	stxa	%l3, [%sp + %g4]asi_num				;\
	add	%sp, 32, %g5					;\
	stxa	%l4, [%g5 + %g1]asi_num				;\
	stxa	%l5, [%g5 + %g2]asi_num				;\
	stxa	%l6, [%g5 + %g3]asi_num				;\
	stxa	%l7, [%g5 + %g4]asi_num				;\
	add	%g5, 32, %g5					;\
	stxa	%i0, [%g5 + %g1]asi_num				;\
	stxa	%i1, [%g5 + %g2]asi_num				;\
	stxa	%i2, [%g5 + %g3]asi_num				;\
	stxa	%i3, [%g5 + %g4]asi_num				;\
	add	%g5, 32, %g5					;\
	stxa	%i4, [%g5 + %g1]asi_num				;\
	stxa	%i5, [%g5 + %g2]asi_num				;\
	stxa	%i6, [%g5 + %g3]asi_num				;\
	stxa	%i7, [%g5 + %g4]asi_num				;\
	TT_TRACE_L(trace_win)					;\
	saved							;\
	retry							;\
	SKIP(31-25-TT_TRACE_L_INS)				;\
	ba,a,pt %xcc, fault_64bit_/**/tail			;\
	.empty

/*
 * SPILL_64bit_tt1 spills a 64-bit-wide register window into a 64-bit
 * wide address space via the designated asi.  It is used to spill
 * windows at tl>1 where performance isn't the primary concern and
 * where we don't want to use unnecessary registers.  The stack
 * pointer is required to be eight-byte aligned.
 */
#define	SPILL_64bit_tt1(asi_num, tail)				\
	mov	asi_num, %asi					;\
2:	stxa	%l0, [%sp + V9BIAS64 + 0]%asi			;\
	stxa	%l1, [%sp + V9BIAS64 + 8]%asi			;\
	stxa	%l2, [%sp + V9BIAS64 + 16]%asi			;\
	stxa	%l3, [%sp + V9BIAS64 + 24]%asi			;\
	stxa	%l4, [%sp + V9BIAS64 + 32]%asi			;\
	stxa	%l5, [%sp + V9BIAS64 + 40]%asi			;\
	stxa	%l6, [%sp + V9BIAS64 + 48]%asi			;\
	stxa	%l7, [%sp + V9BIAS64 + 56]%asi			;\
	stxa	%i0, [%sp + V9BIAS64 + 64]%asi			;\
	stxa	%i1, [%sp + V9BIAS64 + 72]%asi			;\
	stxa	%i2, [%sp + V9BIAS64 + 80]%asi			;\
	stxa	%i3, [%sp + V9BIAS64 + 88]%asi			;\
	stxa	%i4, [%sp + V9BIAS64 + 96]%asi			;\
	stxa	%i5, [%sp + V9BIAS64 + 104]%asi			;\
	stxa	%i6, [%sp + V9BIAS64 + 112]%asi			;\
	stxa	%i7, [%sp + V9BIAS64 + 120]%asi			;\
	TT_TRACE_L(trace_win)					;\
	saved							;\
	retry							;\
	SKIP(31-19-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_64bit_/**/tail			;\
	.empty


/*
 * FILL_64bit fills a 64-bit-wide kernel register window.  It assumes
 * that the kernel context and the nucleus context are the same.  The
 * stack pointer is required to be eight-byte aligned.
 */
#define	FILL_64bit(tail)					\
2:	TT_TRACE_L(trace_win)					;\
	ldx	[%sp + V9BIAS64 + 0], %l0			;\
	ldx	[%sp + V9BIAS64 + 8], %l1			;\
	ldx	[%sp + V9BIAS64 + 16], %l2			;\
	ldx	[%sp + V9BIAS64 + 24], %l3			;\
	ldx	[%sp + V9BIAS64 + 32], %l4			;\
	ldx	[%sp + V9BIAS64 + 40], %l5			;\
	ldx	[%sp + V9BIAS64 + 48], %l6			;\
	ldx	[%sp + V9BIAS64 + 56], %l7			;\
	ldx	[%sp + V9BIAS64 + 64], %i0			;\
	ldx	[%sp + V9BIAS64 + 72], %i1			;\
	ldx	[%sp + V9BIAS64 + 80], %i2			;\
	ldx	[%sp + V9BIAS64 + 88], %i3			;\
	ldx	[%sp + V9BIAS64 + 96], %i4			;\
	ldx	[%sp + V9BIAS64 + 104], %i5			;\
	ldx	[%sp + V9BIAS64 + 112], %i6			;\
	ldx	[%sp + V9BIAS64 + 120], %i7			;\
	restored						;\
	retry							;\
	SKIP(31-18-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_64bit_/**/tail			;\
	.empty

/*
 * FILL_64bit_asi fills a 64-bit-wide register window from a 64-bit
 * wide address space via the designated asi.  It is used to fill
 * non-kernel windows.  The stack pointer is required to be eight-byte
 * aligned. 
 */
#define	FILL_64bit_asi(asi_num, tail)				\
	mov	V9BIAS64 + 0, %g1				;\
2:	TT_TRACE_L(trace_win)					;\
	ldxa	[%sp + %g1]asi_num, %l0				;\
	mov	V9BIAS64 + 8, %g2				;\
	ldxa	[%sp + %g2]asi_num, %l1				;\
	mov	V9BIAS64 + 16, %g3				;\
	ldxa	[%sp + %g3]asi_num, %l2				;\
	mov	V9BIAS64 + 24, %g4				;\
	ldxa	[%sp + %g4]asi_num, %l3				;\
	add	%sp, 32, %g5					;\
	ldxa	[%g5 + %g1]asi_num, %l4				;\
	ldxa	[%g5 + %g2]asi_num, %l5				;\
	ldxa	[%g5 + %g3]asi_num, %l6				;\
	ldxa	[%g5 + %g4]asi_num, %l7				;\
	add	%g5, 32, %g5					;\
	ldxa	[%g5 + %g1]asi_num, %i0				;\
	ldxa	[%g5 + %g2]asi_num, %i1				;\
	ldxa	[%g5 + %g3]asi_num, %i2				;\
	ldxa	[%g5 + %g4]asi_num, %i3				;\
	add	%g5, 32, %g5					;\
	ldxa	[%g5 + %g1]asi_num, %i4				;\
	ldxa	[%g5 + %g2]asi_num, %i5				;\
	ldxa	[%g5 + %g3]asi_num, %i6				;\
	ldxa	[%g5 + %g4]asi_num, %i7				;\
	restored						;\
	retry							;\
	SKIP(31-25-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_64bit_/**/tail			;\
	.empty

/*
 * FILL_64bit_tt1 fills a 64-bit-wide register window from a 64-bit
 * wide address space via the designated asi.  It is used to fill
 * windows at tl>1 where performance isn't the primary concern and
 * where we don't want to use unnecessary registers.  The stack
 * pointer is required to be eight-byte aligned.
 */
#define	FILL_64bit_tt1(asi_num, tail)				\
	mov	asi_num, %asi					;\
	TT_TRACE_L(trace_win)					;\
	ldxa	[%sp + V9BIAS64 + 0]%asi, %l0			;\
	ldxa	[%sp + V9BIAS64 + 8]%asi, %l1			;\
	ldxa	[%sp + V9BIAS64 + 16]%asi, %l2			;\
	ldxa	[%sp + V9BIAS64 + 24]%asi, %l3			;\
	ldxa	[%sp + V9BIAS64 + 32]%asi, %l4			;\
	ldxa	[%sp + V9BIAS64 + 40]%asi, %l5			;\
	ldxa	[%sp + V9BIAS64 + 48]%asi, %l6			;\
	ldxa	[%sp + V9BIAS64 + 56]%asi, %l7			;\
	ldxa	[%sp + V9BIAS64 + 64]%asi, %i0			;\
	ldxa	[%sp + V9BIAS64 + 72]%asi, %i1			;\
	ldxa	[%sp + V9BIAS64 + 80]%asi, %i2			;\
	ldxa	[%sp + V9BIAS64 + 88]%asi, %i3			;\
	ldxa	[%sp + V9BIAS64 + 96]%asi, %i4			;\
	ldxa	[%sp + V9BIAS64 + 104]%asi, %i5			;\
	ldxa	[%sp + V9BIAS64 + 112]%asi, %i6			;\
	ldxa	[%sp + V9BIAS64 + 120]%asi, %i7			;\
	restored						;\
	retry							;\
	SKIP(31-19-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_64bit_/**/tail			;\
	.empty

#endif /* !lint */

/*
 * SPILL_mixed spills either size window, depending on
 * whether %sp is even or odd, to a 32-bit address space.
 * This may only be used in conjunction with SPILL_32bit/
 * SPILL_64bit. New versions of SPILL_mixed_{tt1,asi} would be
 * needed for use with SPILL_{32,64}bit_{tt1,asi}.  Particular
 * attention should be paid to the instructions that belong
 * in the delay slots of the branches depending on the type
 * of spill handler being branched to.
 * Clear upper 32 bits of %sp if it is odd.
 * We won't need to clear them in 64 bit kernel.
 */
#define	SPILL_mixed						\
	btst	1, %sp						;\
	bz,a,pt	%xcc, 1b					;\
	srl	%sp, 0, %sp					;\
	ba,pt	%xcc, 2b					;\
	nop							;\
	.align	128

/*
 * FILL_mixed(ASI) fills either size window, depending on
 * whether %sp is even or odd, from a 32-bit address space.
 * This may only be used in conjunction with FILL_32bit/
 * FILL_64bit. New versions of FILL_mixed_{tt1,asi} would be
 * needed for use with FILL_{32,64}bit_{tt1,asi}. Particular
 * attention should be paid to the instructions that belong
 * in the delay slots of the branches depending on the type
 * of fill handler being branched to.
 * Clear upper 32 bits of %sp if it is odd.
 * We won't need to clear them in 64 bit kernel.
 */
#define	FILL_mixed						\
	btst	1, %sp						;\
	bz,a,pt	%xcc, 1b					;\
	srl	%sp, 0, %sp					;\
	ba,pt	%xcc, 2b					;\
	nop							;\
	.align	128


/*
 * SPILL_32clean/SPILL_64clean spill 32-bit and 64-bit register windows,
 * respectively, into the address space via the designated asi.  The
 * unbiased stack pointer is required to be eight-byte aligned (even for
 * the 32-bit case even though this code does not require such strict
 * alignment).
 *
 * With SPARC v9 the spill trap takes precedence over the cleanwin trap
 * so when cansave == 0, canrestore == 6, and cleanwin == 6 the next save
 * will cause cwp + 2 to be spilled but will not clean cwp + 1.  That
 * window may contain kernel data so in user_rtt we set wstate to call
 * these spill handlers on the first user spill trap.  These handler then
 * spill the appropriate window but also back up a window and clean the
 * window that didn't get a cleanwin trap.
 */
#define	SPILL_32clean(asi_num, tail)				\
	srl	%sp, 0, %sp					;\
	sta	%l0, [%sp + %g0]asi_num				;\
	mov	4, %g1						;\
	sta	%l1, [%sp + %g1]asi_num				;\
	mov	8, %g2						;\
	sta	%l2, [%sp + %g2]asi_num				;\
	mov	12, %g3						;\
	sta	%l3, [%sp + %g3]asi_num				;\
	add	%sp, 16, %g4					;\
	sta	%l4, [%g4 + %g0]asi_num				;\
	sta	%l5, [%g4 + %g1]asi_num				;\
	sta	%l6, [%g4 + %g2]asi_num				;\
	sta	%l7, [%g4 + %g3]asi_num				;\
	add	%g4, 16, %g4					;\
	sta	%i0, [%g4 + %g0]asi_num				;\
	sta	%i1, [%g4 + %g1]asi_num				;\
	sta	%i2, [%g4 + %g2]asi_num				;\
	sta	%i3, [%g4 + %g3]asi_num				;\
	add	%g4, 16, %g4					;\
	sta	%i4, [%g4 + %g0]asi_num				;\
	sta	%i5, [%g4 + %g1]asi_num				;\
	sta	%i6, [%g4 + %g2]asi_num				;\
	sta	%i7, [%g4 + %g3]asi_num				;\
	TT_TRACE_L(trace_win)					;\
	b	.spill_clean					;\
	  mov	WSTATE_USER32, %g7				;\
	SKIP(31-25-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_32bit_/**/tail			;\
	.empty

#define	SPILL_64clean(asi_num, tail)				\
	mov	0 + V9BIAS64, %g1				;\
	stxa	%l0, [%sp + %g1]asi_num				;\
	mov	8 + V9BIAS64, %g2				;\
	stxa	%l1, [%sp + %g2]asi_num				;\
	mov	16 + V9BIAS64, %g3				;\
	stxa	%l2, [%sp + %g3]asi_num				;\
	mov	24 + V9BIAS64, %g4				;\
	stxa	%l3, [%sp + %g4]asi_num				;\
	add	%sp, 32, %g5					;\
	stxa	%l4, [%g5 + %g1]asi_num				;\
	stxa	%l5, [%g5 + %g2]asi_num				;\
	stxa	%l6, [%g5 + %g3]asi_num				;\
	stxa	%l7, [%g5 + %g4]asi_num				;\
	add	%g5, 32, %g5					;\
	stxa	%i0, [%g5 + %g1]asi_num				;\
	stxa	%i1, [%g5 + %g2]asi_num				;\
	stxa	%i2, [%g5 + %g3]asi_num				;\
	stxa	%i3, [%g5 + %g4]asi_num				;\
	add	%g5, 32, %g5					;\
	stxa	%i4, [%g5 + %g1]asi_num				;\
	stxa	%i5, [%g5 + %g2]asi_num				;\
	stxa	%i6, [%g5 + %g3]asi_num				;\
	stxa	%i7, [%g5 + %g4]asi_num				;\
	TT_TRACE_L(trace_win)					;\
	b	.spill_clean					;\
	  mov	WSTATE_USER64, %g7				;\
	SKIP(31-25-TT_TRACE_L_INS)				;\
	ba,a,pt	%xcc, fault_64bit_/**/tail			;\
	.empty


/*
 * Floating point disabled.
 */
#define	FP_DISABLED_TRAP		\
	TT_TRACE(trace_gen)		;\
	ba,pt	%xcc,.fp_disabled	;\
	nop				;\
	.align	32

/*
 * Floating point exceptions.
 */
#define	FP_IEEE_TRAP			\
	TT_TRACE(trace_gen)		;\
	ba,pt	%xcc,.fp_ieee_exception	;\
	nop				;\
	.align	32

#define	FP_TRAP				\
	TT_TRACE(trace_gen)		;\
	ba,pt	%xcc,.fp_exception	;\
	nop				;\
	.align	32

/*
 * asynchronous traps at level 0
 */
/*
 * asynchronous traps at level 1
 */
#define	ASYNC_ITRAP_TL1			  \
	TT_TRACE(trace_gen)		  ;\
	set	async_itrap_tl1, %g1	  ;\
	sethi	%hi(dis_err_panic1), %g4  ;\
	jmp	%g4 + %lo(dis_err_panic1) ;\
	nop				  ;\
	.align	32

#define	ASYNC_DTRAP_TL1			  \
	TT_TRACE(trace_gen)		  ;\
	set	async_dtrap_tl1, %g1	  ;\
	sethi	%hi(dis_err_panic1), %g4  ;\
	jmp	%g4 + %lo(dis_err_panic1) ;\
	nop				  ;\
	.align	32

/*
 * macro for async instruction/data and ce error at tl1
 */
#define ERR_PANIC_TL1(errtype, string)					\
	ENTRY_NP(errtype/**/_tl1);					\
	/* upper 32 bits of AFSR already in o3 */			\
	mov	%o4, %o0;		/* save AFAR upper 32 bits */	\
	mov	%o2, %o4;		/* lower 32 bits of AFSR */ 	\
	mov	%o1, %o2;		/* lower 32 bits of AFAR */ 	\
	mov	%o0, %o1;		/* upper 32 bits of AFAR */ 	\
	set	string, %o0;						\
	call	panic;							\
	nop;								\
	SET_SIZE(errtype/**/_tl1)

/*
 * ECACHE_ECC error traps at level 0 and level 1
 */
#define	ECACHE_ECC(table_name)		\
	.global	table_name/**/_fecc	;\
table_name/**/_fecc:			;\
	set	trap, %g1		;\
	rdpr	%tt, %g3		;\
	ba,pt	%xcc, sys_trap		;\
	sub	%g0, 1, %g4		;\
	.align	32

#ifdef __sparcv9
/*
 * illegal instruction trap
 */
#define	ILLTRAP_INSTR			  \
	TT_TRACE(trace_gen)		  ;\
	or	%g0, P_UTRAP4, %g2	  ;\
	or	%g0, T_UNIMP_INSTR, %g3   ;\
	sethi	%hi(.check_v9utrap), %g4  ;\
	jmp	%g4 + %lo(.check_v9utrap) ;\
	nop				  ;\
	.align	32

/*
 * tag overflow trap
 */
#define	TAG_OVERFLOW			  \
	TT_TRACE(trace_gen)		  ;\
	or	%g0, P_UTRAP10, %g2	  ;\
	or	%g0, T_TAG_OVERFLOW, %g3  ;\
	sethi	%hi(.check_v9utrap), %g4  ;\
	jmp	%g4 + %lo(.check_v9utrap) ;\
	nop				  ;\
	.align	32

/*
 * divide by zero trap
 */
#define	DIV_BY_ZERO			  \
	TT_TRACE(trace_gen)		  ;\
	or	%g0, P_UTRAP11, %g2	  ;\
	or	%g0, T_IDIV0, %g3	  ;\
	sethi	%hi(.check_v9utrap), %g4  ;\
	jmp	%g4 + %lo(.check_v9utrap) ;\
	nop				  ;\
	.align	32

/*
 * trap instruction for V9 user trap handlers
 */
#define	TRAP_INSTR			  \
	TT_TRACE(trace_gen)		  ;\
	or	%g0, T_SOFTWARE_TRAP, %g3 ;\
	sethi	%hi(.check_v9utrap), %g4  ;\
	jmp	%g4 + %lo(.check_v9utrap) ;\
	nop				  ;\
	.align	32
#define	TRP4	TRAP_INSTR; TRAP_INSTR; TRAP_INSTR; TRAP_INSTR

#else

#define	ILLTRAP_INSTR			\
	TRAP(T_UNIMP_INSTR) 		;\

#define	TAG_OVERFLOW			\
	TRAP(T_TAG_OVERFLOW) 		;\

#define	DIV_BY_ZERO			\
	TRAP(T_IDIV0)			;\

#endif

/*
 * LEVEL_INTERRUPT is for level N interrupts.
 * VECTOR_INTERRUPT is for the vector trap.
 */
#define	LEVEL_INTERRUPT(level)		\
	ba,pt	%xcc, pil_interrupt	;\
	mov	level, %g4		;\
	.align	32

#define	LEVEL14_INTERRUPT(level)	\
	rdpr	%tick, %g2		;\
	ba,pt	%xcc, pil14_interrupt	;\
	mov	level, %g4		;\
	.align	32

#define	VECTOR_INTERRUPT				\
	ldxa	[%g0]ASI_INTR_RECEIVE_STATUS, %g1	;\
	btst	IRSR_BUSY, %g1				;\
	bnz,pt	%xcc, vec_interrupt			;\
	nop						;\
	ba,a,pt	%xcc, vec_intr_spurious			;\
	.empty						;\
	.align	32

/*
 * MMU Trap Handlers.
 */
#define	USE_ALTERNATE_GLOBALS						\
	rdpr	%pstate, %g5						;\
	wrpr	%g5, PSTATE_MG | PSTATE_AG, %pstate

#define	IMMU_EXCEPTION							\
	USE_ALTERNATE_GLOBALS						;\
	wr	%g0, ASI_IMMU, %asi					;\
	rdpr	%tpc, %g2						;\
	ldxa	[MMU_SFSR]%asi, %g3					;\
	sllx	%g3, 32, %g3						;\
	ba,pt	%xcc, .mmu_exception_end				;\
	  or	%g3, T_INSTR_EXCEPTION, %g3				;\
	.align	32

#define	DMMU_EXCEPTION							\
	USE_ALTERNATE_GLOBALS						;\
	wr	%g0, ASI_DMMU, %asi					;\
	ldxa	[MMU_TAG_ACCESS]%asi, %g2				;\
	ldxa	[MMU_SFSR]%asi, %g3					;\
	sllx	%g3, 32, %g3						;\
	ba,pt	%xcc, .mmu_exception_end				;\
	  or	%g3, T_DATA_EXCEPTION, %g3				;\
	.align	32

#define	DMMU_EXC_AG_PRIV						\
	wr	%g0, ASI_DMMU, %asi					;\
	ldxa	[MMU_SFAR]%asi, %g2					;\
	ldxa	[MMU_SFSR]%asi, %g3					;\
	sllx	%g3, 32, %g3						;\
	ba,pt	%xcc, .mmu_priv_exception				;\
	  or	%g3, T_PRIV_INSTR, %g3					;\
	.align	32

#define	DMMU_EXC_AG_NOT_ALIGNED						\
	wr	%g0, ASI_DMMU, %asi					;\
	ldxa	[MMU_SFAR]%asi, %g2					;\
	ba,pt	%xcc, .mmu_exception_not_aligned			;\
	ldxa	[MMU_SFSR]%asi, %g3					;\
	.align	32

#ifdef __sparcv9
/*
 * SPARC V9 IMPL. DEP. #109(1) and (2) and #110(1) and (2)
 */
#define	DMMU_EXC_LDDF_NOT_ALIGNED					\
	btst	1, %sp							;\
	bnz,pt	%xcc, .lddf_exception_not_aligned			;\
	wr	%g0, ASI_DMMU, %asi					;\
	ldxa	[MMU_SFAR]%asi, %g2					;\
	ba,pt	%xcc, .mmu_exception_not_aligned			;\
	ldxa	[MMU_SFSR]%asi, %g3					;\
	.align	32

#define	DMMU_EXC_STDF_NOT_ALIGNED					\
	btst	1, %sp							;\
	bnz,pt	%xcc, .stdf_exception_not_aligned			;\
	wr	%g0, ASI_DMMU, %asi					;\
	ldxa	[MMU_SFAR]%asi, %g2					;\
	ba,pt	%xcc, .mmu_exception_not_aligned			;\
	ldxa	[MMU_SFSR]%asi, %g3					;\
	.align	32
#else  /* __sparcv9 */
/*
 * There are existing 32-bit applications that depend on this behavior.
 */
#define	DMMU_EXC_LDDF_NOT_ALIGNED	DMMU_EXC_AG_NOT_ALIGNED
#define	DMMU_EXC_STDF_NOT_ALIGNED	DMMU_EXC_AG_NOT_ALIGNED
#endif /* __sparcv9 */

/*
 * Flush the TLB using either the primary, secondary, or nucleus flush
 * operation based on whether the ctx from the tag access register matches
 * the primary or secondary context (flush the nucleus if neither matches).
 *
 * Requires a membar #Sync before next ld/st.
 * exits with:
 * g2 = tag access register
 * g3 = ctx number
 */
#define	DTLB_DEMAP_ENTRY						\
	mov	MMU_TAG_ACCESS, %g1					;\
	mov	MMU_PCONTEXT, %g5					;\
	ldxa	[%g1]ASI_DMMU, %g2					;\
	sethi	%hi(TAGACC_CTX_MASK), %g4				;\
	or	%g4, %lo(TAGACC_CTX_MASK), %g4				;\
	and	%g2, %g4, %g3			/* g3 = ctx */		;\
	ldxa	[%g5]ASI_DMMU, %g6		/* g6 = primary ctx */	;\
	andn	%g2, %g4, %g1			/* ctx = primary */	;\
	cmp	%g3, %g6						;\
	be,a,pt	%xcc, 1f						;\
	  stxa	%g0, [%g1]ASI_DTLB_DEMAP	/* MMU_DEMAP_PAGE */	;\
	or	%g1, DEMAP_SECOND, %g1					;\
	mov	MMU_SCONTEXT, %g5					;\
	ldxa	[%g5]ASI_DMMU, %g6		/* g6 = secondary ctx */ ;\
	cmp	%g3, %g6						;\
	be,a,pt	%xcc, 1f						;\
	  stxa	%g0, [%g1]ASI_DTLB_DEMAP	/* MMU_DEMAP_PAGE */	;\
	andn	%g2, %g4, %g1						;\
	or	%g1, DEMAP_NUCLEUS, %g1					;\
	stxa	%g0, [%g1]ASI_DTLB_DEMAP	/* MMU_DEMAP_PAGE */	;\
1:

#define	TSB_MISS_HANDLER(tsbptr, ctx)					\
	cmp	ctx, %g0						;\
	bne,a,pt %icc, sfmmu_utsb_miss					;\
	  nop								;\
	ba,a,pt	%xcc, sfmmu_ktsb_miss					;\
	  nop								;\

#if defined(TRAPTRACE) || defined(DEBUG)

#define	TSB_MISS(tsbptr, ctx)						\
	ba,a,pt	%xcc, .tsb_miss						;\
	  nop								;\

#define	TSB_DMISS_LABEL(handler)	.dtsb_miss

#else

#define	TSB_MISS(tsbptr, ctx)	TSB_MISS_HANDLER(tsbptr, ctx)

#define	TSB_DMISS_LABEL(handler)	handler

#endif

/*
 * Needs to be exactly 32 instructions
 *
 * UTLB NOTE: If we don't hit on the 8k pointer then we branch
 * to a special 4M tsb handler. It would be nice if that handler
 * could live in this file but currently it seems better to allow
 * it to fall thru to sfmmu_utsb_miss. For now this means we don't
 * get any trap trace data on 4M ttes.
 */
#define	DTLB_MISS(table_name)						;\
	HAT_PERCPU_DBSTAT(TSBMISS_DTLBMISS) /* 3 instr ifdef DEBUG */	;\
	ldxa	[%g0]ASI_DMMU, %g2		/* MMU_TARGET */	;\
	ldxa	[%g0]ASI_DMMU_TSB_8K, %g1	/* g1 = tsbe ptr */	;\
	srlx	%g2, TTARGET_CTX_SHIFT, %g3				;\
	brz,pn	%g3, 1f							;\
	sll	%g3, TSB_ENTRY_SHIFT, %g5				;\
	xor	%g5, %g1, %g1						;\
	ldda	[%g1]ASI_NQUAD_LD, %g4	/* g4 = tag, %g5 data */	;\
	cmp	%g2, %g4						;\
	/* we may patch the next instruction at run time */		;\
	/* NOTE: any changes here require sfmmu_patch_utsb fixed */	;\
	.global table_name/**/_dutsb					;\
table_name/**/_dutsb:							;\
	bne,pn	%xcc, TSB_DMISS_LABEL(sfmmu_udtlb_miss)			;\
	  sethi	%hi(UTSB_MAX_BASE_MASK), %g4				;\
	stxa	%g5, [%g0]ASI_DTLB_IN					;\
	TT_TRACE(trace_tsbhit)		/* 2 instr ifdef TRAPTRACE */	;\
	retry								;\
1:	mov	MMU_TAG_ACCESS, %g6					;\
	ldxa	[%g6] ASI_DMMU, %g6	/* load tag access */		;\
	/* we patch the next 3 instruction at run time */		;\
	/* NOTE: any changes here require sfmmu_patch_ktsb fixed */	;\
	.global table_name/**/_dktsb					;\
table_name/**/_dktsb:							;\
	/* get tsb offset */						;\
	sethi	%hi(0), %g7			/* ktsb_base */		;\
	sllx	%g6, 64 - (0 + TAGACC_SHIFT), %g1			;\
	srlx	%g1, 64 - (0 + TSB_ENTRY_SHIFT), %g1			;\
	ldda	[%g7 + %g1]ASI_NQUAD_LD, %g4  /* g4 = tag, %g5 data */	;\
	cmp	%g2, %g4						;\
	bne,a,pn %xcc, TSB_DMISS_LABEL(sfmmu_ktsb_miss)			;\
	  add	%g7, %g1, %g1			/* make tsbptr */	;\
	stxa	%g5, [%g0]ASI_DTLB_IN					;\
	TT_TRACE(trace_tsbhit)		/* 2 instr ifdef TRAPTRACE */	;\
	retry								;\
	.align 128

/* this needs to exactly 32 instructions */
#define	ITLB_MISS(table_name)						 \
	HAT_PERCPU_DBSTAT(TSBMISS_ITLBMISS) /* 3 instr ifdef DEBUG */	;\
	ldxa	[%g0]ASI_IMMU, %g2		/* MMU_TARGET */	;\
	ldxa	[%g0]ASI_IMMU_TSB_8K, %g1	/* g1 = tsbe ptr */	;\
	srlx	%g2, TTARGET_CTX_SHIFT, %g3				;\
	brz,pn	%g3, 2f							;\
	sll	%g3, TSB_ENTRY_SHIFT, %g5				;\
	xor	%g5, %g1, %g1						;\
1:	ldda	[%g1]ASI_NQUAD_LD, %g4	/* g4 = tag, g5 = data */	;\
	cmp	%g2, %g4						;\
	bne,pn	%xcc, 8f		/* br if 8k ptr miss */		;\
	andcc	%g5, TTE_EXECPRM_INT, %g0 /* check execute bit */	;\
	bz,pn	%icc, .exec_fault					;\
	  nop								;\
	stxa	%g5, [%g0]ASI_ITLB_IN		/* 8k ptr hit */	;\
	TT_TRACE(trace_tsbhit)		/* 2 instr ifdef TRAPTRACE */	;\
	retry								;\
2:	/* kernel miss */						;\
	mov	MMU_TAG_ACCESS, %g6					;\
	ldxa	[%g6]ASI_IMMU, %g6	/* load tag access */		;\
	/* we patch the next 3 instruction at run time */		;\
	/* NOTE: any changes here require sfmmu_patch_kernel fixed */	;\
	.global table_name/**/_iktsb					;\
table_name/**/_iktsb:							;\
	/* get kernel tsb pointer */					;\
	sethi	%hi(0), %g4			/* ktsb_base */		;\
	sllx	%g6, 64 - (0 + TAGACC_SHIFT), %g1			;\
	srlx	%g1, 64 - (0 + TSB_ENTRY_SHIFT), %g1			;\
	ba,pt	%xcc, 1b						;\
	or	%g4, %g1, %g1			/* tsb entry ptr */	;\
8:									;\
	/*								;\
	 * We get here if we couldn't find the 8k mapping in the TSB	;\
	 * We don't look for 64K, 512K, or 4MB mappings in the TSB	;\
	 * because we currently don't support large pages in user	;\
	 * land and for the kernel I might as well go to the hash.	;\
	 * If we ever implement large pages for user adress space	;\
	 * (eg. shared memory) we need to reevaluate this.		;\
	 * g1 = tsbptr							;\
	 * g3 = ctx #							;\
	 */								;\
	TT_TRACE(trace_tsbmiss)		/* 2 instr ifdef TRAPTRACE */	;\
	TSB_MISS(%g1, %g3)						;\
	.align 128

/*
 * This macro is the first level handler for fast protection faults.
 * It first demaps the tlb entry which generated the fault and then
 * attempts to set the modify bit on the hash.  It needs to be
 * exactly 32 instructions.
 */
#define	DTLB_PROT							 \
	DTLB_DEMAP_ENTRY		/* 20 instructions */		;\
	/*								;\
	 * g2 = tag access register					;\
	 * g3 = ctx number						;\
	 */								;\
	TT_TRACE(trace_dataprot)	/* 2 instr ifdef TRAPTRACE */	;\
	brnz,pt %g3, sfmmu_uprot_trap					;\
	  membar #Sync							;\
	ba,a,pt	%xcc, sfmmu_kprot_trap					;\
	  nop								;\
	.align 128

#define	DMMU_EXCEPTION_TL1						;\
	USE_ALTERNATE_GLOBALS						;\
	ba,a,pt	%xcc, mmu_trap_tl1					;\
	  nop								;\
	.align 32

#define	MISALIGN_ADDR_TL1						;\
	ba,a,pt	%xcc, mmu_trap_tl1					;\
	  nop								;\
	.align 32

#if defined(lint)

struct scb	trap_table;
struct scb	scb;		/* trap_table/scb are the same object */

#else /* lint */

/*
 * =======================================================================
 *		SPARC V9 TRAP TABLE
 *
 * The trap table is divided into two halves: the first half is used when
 * taking traps when TL=0; the second half is used when taking traps from
 * TL>0. Note that handlers in the second half of the table might not be able
 * to make the same assumptions as handlers in the first half of the table.
 *
 * Worst case trap nesting so far:
 *
 *	at TL=0 client issues software trap requesting service
 *	at TL=1 nucleus wants a register window
 *	at TL=2 register window clean/spill/fill takes a TLB miss
 *	at TL=3 processing TLB miss
 *	at TL=4 handle asynchronous error
 *
 * Note that a trap from TL=4 to TL=5 places Spitfire in "RED mode".
 *
 * =======================================================================
 */
	.section ".text"
	.align	4
	.global trap_table, scb, trap_table0, trap_table1, etrap_table
	.type	trap_table, #function
	.type	scb, #function
trap_table:
scb:
trap_table0:
	/* hardware traps */
	NOT;				/* 000	reserved */
	RED;				/* 001	power on reset */
	RED;				/* 002	watchdog reset */
	RED;				/* 003	externally initiated reset */
	RED;				/* 004	software initiated reset */
	RED;				/* 005	red mode exception */
	NOT; NOT;			/* 006 - 007 reserved */
	IMMU_EXCEPTION;			/* 008	instruction access exception */
	NOT;				/* 009	instruction access MMU miss */
	GOTO_TT(async_err, trace_gen);	/* 00A	instruction access error */
	NOT; NOT4;			/* 00B - 00F reserved */
	ILLTRAP_INSTR;			/* 010	illegal instruction */
	TRAP(T_PRIV_INSTR);		/* 011	privileged opcode */
	NOT;				/* 012	unimplemented LDD */
	NOT;				/* 013	unimplemented STD */
	NOT4; NOT4; NOT4;		/* 014 - 01F reserved */
	FP_DISABLED_TRAP;		/* 020	fp disabled */
	FP_IEEE_TRAP;			/* 021	fp exception ieee 754 */
	FP_TRAP;			/* 022	fp exception other */
	TAG_OVERFLOW;			/* 023	tag overflow */
	CLEAN_WINDOW;			/* 024 - 027 clean window */
	DIV_BY_ZERO;			/* 028	division by zero */
	NOT;				/* 029	internal processor error */
	NOT; NOT; NOT4;			/* 02A - 02F reserved */
	DMMU_EXCEPTION;			/* 030	data access exception */
	NOT;				/* 031	data access MMU miss */
	GOTO_TT(async_err, trace_gen);	/* 032	data access error */
	NOT;				/* 033	data access protection */
	DMMU_EXC_AG_NOT_ALIGNED;	/* 034	mem address not aligned */
	DMMU_EXC_LDDF_NOT_ALIGNED;	/* 035	LDDF mem address not aligned */
	DMMU_EXC_STDF_NOT_ALIGNED;	/* 036	STDF mem address not aligned */
	DMMU_EXC_AG_PRIV;		/* 037	privileged action */
	NOT;				/* 038	LDQF mem address not aligned */
	NOT;				/* 039	STQF mem address not aligned */
	NOT; NOT; NOT4;			/* 03A - 03F reserved */
	NOT;				/* 040	async data error */
	LEVEL_INTERRUPT(1);		/* 041	interrupt level 1 */
	LEVEL_INTERRUPT(2);		/* 042	interrupt level 2 */
	LEVEL_INTERRUPT(3);		/* 043	interrupt level 3 */
	LEVEL_INTERRUPT(4);		/* 044	interrupt level 4 */
	LEVEL_INTERRUPT(5);		/* 045	interrupt level 5 */
	LEVEL_INTERRUPT(6);		/* 046	interrupt level 6 */
	LEVEL_INTERRUPT(7);		/* 047	interrupt level 7 */
	LEVEL_INTERRUPT(8);		/* 048	interrupt level 8 */
	LEVEL_INTERRUPT(9);		/* 049	interrupt level 9 */
	LEVEL_INTERRUPT(10);		/* 04A	interrupt level 10 */
	LEVEL_INTERRUPT(11);		/* 04B	interrupt level 11 */
	LEVEL_INTERRUPT(12);		/* 04C	interrupt level 12 */
	LEVEL_INTERRUPT(13);		/* 04D	interrupt level 13 */
	LEVEL14_INTERRUPT(14);		/* 04E	interrupt level 14 */
	LEVEL_INTERRUPT(15);		/* 04F	interrupt level 15 */
	NOT4; NOT4; NOT4; NOT4;		/* 050 - 05F reserved */
	VECTOR_INTERRUPT;		/* 060	interrupt vector */
	GOTO(kadb_bpt);			/* 061	PA watchpoint */
	GOTO(kadb_bpt);			/* 062	VA watchpoint */
	GOTO_TT(ce_err, trace_gen);	/* 063	corrected ECC error */
	ITLB_MISS(tt0);			/* 064	instruction access MMU miss */
	DTLB_MISS(tt0);			/* 068	data access MMU miss */
	DTLB_PROT;			/* 06C	data access protection */
	ECACHE_ECC(tt0);		/* 070  fast ecache ECC error */
	NOT; NOT; NOT;			/* 071 - 073 reserved */
	NOT4; NOT4; NOT4;		/* 074 - 07F reserved */
	NOT4;				/* 080	spill 0 normal */
	SPILL_32bit_asi(ASI_AIUP,sn0);	/* 084	spill 1 normal */
	SPILL_64bit_asi(ASI_AIUP,sn0);	/* 088	spill 2 normal */
	SPILL_32clean(ASI_AIUP,sn0);	/* 08C	spill 3 normal */
	SPILL_64clean(ASI_AIUP,sn0);	/* 090	spill 4 normal */
	SPILL_32bit(not);		/* 094	spill 5 normal */
	SPILL_64bit(not);		/* 098	spill 6 normal */
	SPILL_mixed;			/* 09C	spill 7 normal */
	NOT4;				/* 0A0	spill 0 other */
	SPILL_32bit_asi(ASI_AIUS,so0);	/* 0A4	spill 1 other */
	SPILL_64bit_asi(ASI_AIUS,so0);	/* 0A8	spill 2 other */
	SPILL_32bit_asi(ASI_AIUS,so0);	/* 0AC	spill 3 other */
	SPILL_64bit_asi(ASI_AIUS,so0);	/* 0B0	spill 4 other */
	NOT4;				/* 0B4	spill 5 other */
	NOT4;				/* 0B8	spill 6 other */
	NOT4;				/* 0BC	spill 7 other */
	NOT4;				/* 0C0	fill 0 normal */
	FILL_32bit_asi(ASI_AIUP,fn0);	/* 0C4	fill 1 normal */
	FILL_64bit_asi(ASI_AIUP,fn0);	/* 0C8	fill 2 normal */
	FILL_32bit_asi(ASI_AIUP,fn0);	/* 0CC	fill 3 normal */
	FILL_64bit_asi(ASI_AIUP,fn0);	/* 0D0	fill 4 normal */
	FILL_32bit(not);		/* 0D4	fill 5 normal */
	FILL_64bit(not);		/* 0D8	fill 6 normal */
	FILL_mixed;			/* 0DC	fill 7 normal */
	NOT4;				/* 0E0	fill 0 other */
	NOT4;				/* 0E4	fill 1 other */
	NOT4;				/* 0E8	fill 2 other */
	NOT4;				/* 0EC	fill 3 other */
	NOT4;				/* 0F0	fill 4 other */
	NOT4;				/* 0F4	fill 5 other */
	NOT4;				/* 0F8	fill 6 other */
	NOT4;				/* 0FC	fill 7 other */
	/* user traps */
	GOTO(syscall_trap_4x);		/* 100	old system call */
	TRAP(T_BREAKPOINT);		/* 101	user breakpoint */
	TRAP(T_DIV0);			/* 102	user divide by zero */
	FLUSHW();			/* 103	flush windows */
	GOTO(.clean_windows);		/* 104	clean windows */
	BAD;				/* 105	range check ?? */
	GOTO(.fix_alignment);		/* 106	do unaligned references */
	BAD;				/* 107	unused */
#ifdef __sparcv9
	SYSCALL(syscall_trap32);	/* 108	ILP32 system call on LP64 */
#else
	SYSCALL(syscall_trap);		/* 108	new system call ILP32 native */
#endif
	GOTO(set_trap0_addr);		/* 109	set trap0 address */
	BAD;				/* 10A	reserved for hot patch trap */
	BAD; BAD4;			/* 10B - 10F unused */
#ifdef __sparcv9
	TRP4; TRP4; TRP4; TRP4;		/* 110 - 11F V9 user trap handlers */
#else
	BAD4; BAD4; BAD4; BAD4;		/* 110 - 11F unused */
#endif
	GOTO(.getcc);			/* 120	get condition codes */
	GOTO(.setcc);			/* 121	set condition codes */
	GOTO(.getpsr);			/* 122	get psr */
	GOTO(.setpsr);			/* 123	set psr (some fields) */
	GOTO(get_timestamp);		/* 124	get timestamp */
	GOTO(get_virtime);		/* 125	get lwp virtual time */
	PRIV(self_xcall);		/* 126	self xcall */
	GOTO(get_hrestime);		/* 127	get hrestime */
	BAD4; BAD4;			/* 128 - 12F unused */
	TRACE_TRAP(trace_trap_0);	/* 130	trace, no data */
	TRACE_TRAP(trace_trap_1);	/* 131	trace, 1 data word */
	TRACE_TRAP(trace_trap_2);	/* 132	trace, 2 data words */
	TRACE_TRAP(trace_trap_3);	/* 133	trace, 3 data words */
	TRACE_TRAP(trace_trap_4);	/* 134	trace, 4 data words */
	TRACE_TRAP(trace_trap_5);	/* 135	trace, 5 data words */
	BAD;				/* 136	trace, unused */
	TRACE_TRAP(trace_trap_write_buffer);
					/* 137	trace, atomic buf write */
	BAD4; BAD4;			/* 138 - 13F unused */
#ifdef __sparcv9
	SYSCALL(syscall_trap)		/* 140  LP64 system call */
#else
	BAD;				/* 140  reserved */
#endif
	BAD;				/* 141  unused */
#ifdef DEBUG_USER_TRAPTRACECTL
	GOTO(.traptrace_freeze);	/* 142  freeze traptrace */
	GOTO(.traptrace_unfreeze);	/* 143  unfreeze traptrace */
#else
	BAD; BAD;			/* 142 - 143 unused */
#endif
	BAD4; BAD4; BAD4;		/* 144 - 14F unused */
	BAD4; BAD4; BAD4; BAD4;		/* 150 - 15F unused */
	BAD4; BAD4; BAD4; BAD4;		/* 160 - 16F unused */
#if defined(CHEETAH_ERRATUM_1)
	DONE;				/* 170 - cheetah workaround */
#else
	BAD;				/* 170 - unused */
#endif
	BAD; BAD; BAD;			/* 171 - 173 unused */
	BAD4; BAD4;			/* 174 - 17B unused */
#ifdef	PTL1_PANIC_DEBUG
	mov 1, %g1; GOTO(ptl1_panic);	/* 17C	test ptl1_panic */
#else
	BAD;				/* 17C  unused */
#endif	/* PTL1_PANIC_DEBUG */
	PRIV(kadb_bpt);			/* 17D	kadb enter (L1-A) */
	PRIV(kadb_bpt);			/* 17E	kadb breakpoint */
	PRIV(obp_bpt);			/* 17F	obp breakpoint */
	/* reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 180 - 18F reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 190 - 19F reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 1A0 - 1AF reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 1B0 - 1BF reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 1C0 - 1CF reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 1D0 - 1DF reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 1E0 - 1EF reserved */
	NOT4; NOT4; NOT4; NOT4;		/* 1F0 - 1FF reserved */
trap_table1:
	NOT4; NOT4; NOT; NOT;		/* 000 - 009 unused */
	ASYNC_ITRAP_TL1;		/* 00A	instruction access error */
	NOT; NOT4;			/* 00B - 00F unused */
	NOT4; NOT4; NOT4; NOT4;		/* 010 - 01F unused */
	NOT4;				/* 020 - 023 unused */
	CLEAN_WINDOW;			/* 024 - 027 clean window */
	NOT4; NOT4;			/* 028 - 02F unused */
	DMMU_EXCEPTION_TL1;		/* 030 	data access exception */
	NOT;				/* 031 unused */
	ASYNC_DTRAP_TL1;		/* 032	data access error */
	NOT;				/* 033	unused */
	MISALIGN_ADDR_TL1;		/* 034	mem address not aligned */
	NOT; NOT; NOT; NOT4; NOT4	/* 035 - 03F unused */
	NOT4; NOT4; NOT4; NOT4;		/* 040 - 04F unused */
	NOT4; NOT4; NOT4; NOT4;		/* 050 - 05F unused */
	NOT;				/* 060	unused */
	GOTO(kadb_bpt);			/* 061	PA watchpoint */
	GOTO(kadb_bpt);			/* 062	VA watchpoint */
	GOTO_TT(ce_err_tl1, trace_gen);	/* 063	corrected ECC error */
	ITLB_MISS(tt1);			/* 064	instruction access MMU miss */
	DTLB_MISS(tt1);			/* 068	data access MMU miss */
	DTLB_PROT;			/* 06C	data access protection */
	ECACHE_ECC(tt1);		/* 070  fast ecache ECC error */
	NOT; NOT; NOT;			/* 071 - 073 reserved */
	NOT4; NOT4; NOT4;		/* 074 - 07F reserved */
	NOT4;				/* 080	spill 0 normal */
	SPILL_32bit_tt1(ASI_AIUP,sn1);	/* 084	spill 1 normal */
	SPILL_64bit_tt1(ASI_AIUP,sn1);	/* 088	spill 2 normal */
	SPILL_32bit_tt1(ASI_AIUP,sn1);	/* 08C	spill 3 normal */
	SPILL_64bit_tt1(ASI_AIUP,sn1);	/* 090	spill 4 normal */
	SPILL_32bit(not);		/* 094	spill 5 normal */
	SPILL_64bit(not);		/* 098	spill 6 normal */
	SPILL_mixed;			/* 09C	spill 7 normal */
	NOT4;				/* 0A0	spill 0 other */
	SPILL_32bit_tt1(ASI_AIUS,so1);	/* 0A4	spill 1 other */
	SPILL_64bit_tt1(ASI_AIUS,so1);	/* 0A8	spill 2 other */
	SPILL_32bit_tt1(ASI_AIUS,so1);	/* 0AC	spill 3 other */
	SPILL_64bit_tt1(ASI_AIUS,so1);	/* 0B0  spill 4 other */
	NOT4;				/* 0B4  spill 5 other */
	NOT4;				/* 0B8  spill 6 other */
	NOT4;				/* 0BC  spill 7 other */
	NOT4;				/* 0C0	fill 0 normal */
	FILL_32bit_tt1(ASI_AIUP,fn1);	/* 0C4	fill 1 normal */
	FILL_64bit_tt1(ASI_AIUP,fn1);	/* 0C8	fill 2 normal */
	FILL_32bit_tt1(ASI_AIUP,fn1);	/* 0CC	fill 3 normal */
	FILL_64bit_tt1(ASI_AIUP,fn1);	/* 0D0	fill 4 normal */
	FILL_32bit(not);		/* 0D4	fill 5 normal */
	FILL_64bit(not);		/* 0D8	fill 6 normal */
	FILL_mixed;			/* 0DC	fill 7 normal */
	NOT4; NOT4; NOT4; NOT4;		/* 0E0 - 0EF unused */
	NOT4; NOT4; NOT4; NOT4;		/* 0F0 - 0FF unused */
/*
 * Code running at TL>0 does not use soft traps, so
 * we can truncate the table here.
 */
etrap_table:
	.size	trap_table, (.-trap_table)
	.size	scb, (.-scb)

/*
 * Software trap handlers run directly from the trap table
 * These are used for DEBUG or TRAPTRACE kernels.
 */
.dtsb_miss:
	/*
	 * %g1 = tsb ptr
	 * %g2 = VA[31: 22] if user, and tag target if kernel
	 * %g3 = context number
	 */
	TT_TRACE(trace_tsbmiss)
	cmp	%g3, KCONTEXT
	be,pt %xcc, sfmmu_ktsb_miss
	  nop
	set	utsb_4m_disable, %g7
	ld	[%g7], %g7
	brnz,pn	%g7, sfmmu_utsb_miss
	  nop
	ba,pt	%icc, sfmmu_udtlb_miss
	  nop
	/* not reachable */
	unimp 0

.tsb_miss:
	/*
	 * %g1 = tsb ptr
	 * %g3 = context number
	 */
	TSB_MISS_HANDLER(%g1, %g3)
	/* not reachable */
	unimp 0
/*
 * We get to exec_fault in the case of an instruction miss and tte
 * has no execute bit set.  We go to tl0 to handle it.
 * g5 = tte
 */
.exec_fault:
#ifdef TRAPTRACE
	membar	#Sync
	sethi	%hi(FLUSH_ADDR), %g6
	flush	%g6
	TRACE_PTR(%g3, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g3 + TRAP_ENT_TICK]%asi
	stxa	%g2, [%g3 + TRAP_ENT_TSTATE]%asi	! tlb tag
	stxa	%g5, [%g3 + TRAP_ENT_F1]%asi		! tsb data
#ifdef __sparcv9
	stxa	%g0, [%g3 + TRAP_ENT_F2]%asi
#endif
	stna	%g0, [%g3 + TRAP_ENT_F3]%asi
	stna	%g0, [%g3 + TRAP_ENT_F4]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g3 + TRAP_ENT_TPC]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g3 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	or	%g6, 0x200, %g6
	stha	%g6, [%g3 + TRAP_ENT_TT]%asi
	mov	MMU_TAG_ACCESS, %g4
	ldxa	[%g4] ASI_IMMU, %g1
	stxa	%g1, [%g3 + TRAP_ENT_SP]%asi		! tag access
#ifdef __sparcv9
	stxa	%g0, [%g3 + TRAP_ENT_TR]%asi
#endif
	TRACE_NEXT(%g3, %g4, %g5)
#endif /* TRAPTRACE */
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g4
	ldxa	[%g4] ASI_IMMU, %g2			! arg1 = addr
	mov	T_INSTR_MMU_MISS, %g3			! arg2 = traptype
	set	trap, %g1
	ba,pt	%xcc, sys_trap
	  mov	-1, %g4

.mmu_exception_not_aligned:
	rdpr	%tstate, %g1
	btst	TSTATE_PRIV, %g1
	bnz,a,pn %icc, 2f
	  nop
	CPU_ADDR(%g1, %g4)				! load CPU struct addr
	ldn	[%g1 + CPU_THREAD], %g1			! load thread pointer
	ldn	[%g1 + T_PROCP], %g1			! load proc pointer
	ldn	[%g1 + P_UTRAPS], %g5			! are there utraps?
	brz,a,pt %g5, 2f
	  nop
	ldn	[%g5 + P_UTRAP15], %g5			! unaligned utrap?
	brz,a,pn %g5, 2f
	  nop
#ifdef __sparcv9
	btst	1, %sp
	bz,a,pt	%xcc, 1f				! 32 bit user program
	  nop
	ba,a,pt	%xcc, .setup_v9utrap			! 64 bit user program
	  nop
1:
#endif
	ba,pt	%xcc, .setup_utrap
	  or	%g2, %g0, %g7
2:
	sllx	%g3, 32, %g3
	ba,pt	%xcc, .mmu_exception_end
	  or	%g3, T_ALIGNMENT, %g3

.mmu_priv_exception:
#ifdef __sparcv9
	rdpr	%tstate, %g1
	btst	TSTATE_PRIV, %g1
	bnz,a,pn %icc, 1f
	  nop
	CPU_ADDR(%g1, %g4)				! load CPU struct addr
	ldn	[%g1 + CPU_THREAD], %g1			! load thread pointer
	ldn	[%g1 + T_PROCP], %g1			! load proc pointer
	ldn	[%g1 + P_UTRAPS], %g5			! are there utraps?
	brz,a,pt %g5, 1f
	  nop
	ldn	[%g5 + P_UTRAP16], %g5
	brnz,a,pt %g5, .setup_v9utrap
	  nop
1:
#endif
.mmu_exception_end:
	set	trap, %g1
	ba,pt	%xcc, sys_trap
	  sub	%g0, 1, %g4

.fp_disabled:
	CPU_ADDR(%g1, %g4)				! load CPU struct addr
	ldn	[%g1 + CPU_THREAD], %g1			! load thread pointer
#ifdef SF_ERRATA_30 /* call causes fp-disabled */
	brz,a,pn %g1, 2f
	  nop
#endif
	rdpr	%tstate, %g4
	btst	TSTATE_PRIV, %g4
#ifdef SF_ERRATA_30 /* call causes fp-disabled */
	bnz,pn %icc, 2f
	  nop
#else
	bnz,a,pn %icc, ptl1_panic
	  mov	PTL1_BAD_FPTRAP, %g1
#endif
	ldn	[%g1 + T_PROCP], %g1			! load proc pointer
	ldn	[%g1 + P_UTRAPS], %g5			! are there utraps?
	brz,a,pt %g5, 2f
	  nop
	ldn	[%g5 + P_UTRAP7], %g5			! fp_disabled utrap?
	brz,a,pn %g5, 2f
	  nop
#ifdef __sparcv9
	btst	1, %sp
	bz,a,pt	%xcc, 1f				! 32 bit user program
	  nop
	ba,a,pt	%xcc, .setup_v9utrap			! 64 bit user program
	  nop
1:
#endif
	ba,pt	%xcc, .setup_utrap
	  or	%g0, %g0, %g7
2:
	set	fp_disabled, %g1
	ba,pt	%xcc, sys_trap
	  sub	%g0, 1, %g4

.fp_ieee_exception:
	rdpr	%tstate, %g1
	btst	TSTATE_PRIV, %g1
	bnz,a,pn %icc, ptl1_panic
	  mov	PTL1_BAD_FPTRAP, %g1
	CPU_ADDR(%g1, %g4)				! load CPU struct addr
	stx	%fsr, [%g1 + CPU_TMP1]
	ldx	[%g1 + CPU_TMP1], %g2
#ifdef __sparcv9
	ldn	[%g1 + CPU_THREAD], %g1			! load thread pointer
	ldn	[%g1 + T_PROCP], %g1			! load proc pointer
	ldn	[%g1 + P_UTRAPS], %g5			! are there utraps?
	brz,a,pt %g5, 1f
	  nop
	ldn	[%g5 + P_UTRAP8], %g5
	brnz,a,pt %g5, .setup_v9utrap
	  nop
1:
#endif
	set	_fp_ieee_exception, %g1
	ba,pt	%xcc, sys_trap
	  sub	%g0, 1, %g4

/*
 * Register Inputs:
 *	%g5		user trap handler
 *	%g7		misaligned addr - for alignment traps only
 */
.setup_utrap:
	set	trap, %g1			! setup in case we go
	mov	T_FLUSH_PCB, %g3		! through sys_trap on
	sub	%g0, 1, %g4			! the save instruction below

	save	%sp, -SA(MINFRAME32), %sp	! window for trap handler
	rdpr	%tpc, %l1			! arg0 == tpc
	rdpr	%tnpc, %l2			! arg1 == tnpc
	or	%g7, %g0, %l3			! arg2 == misaligned address

	rdpr	%tstate, %g1			! cwp for trap handler
	rdpr	%cwp, %g4
	bclr	TSTATE_CWP_MASK, %g1
	wrpr	%g1, %g4, %tstate
	wrpr	%g0, %g5, %tnpc			! trap handler address
	done
	/* NOTREACHED */

#ifdef __sparcv9
.check_v9utrap:
	rdpr	%tstate, %g1
	btst	TSTATE_PRIV, %g1
	bnz,a,pn %icc, 3f
	  nop
	CPU_ADDR(%g1, %g4)				! load CPU struct addr
	ldn	[%g1 + CPU_THREAD], %g1			! load thread pointer
	ldn	[%g1 + T_PROCP], %g1			! load proc pointer
	ldn	[%g1 + P_UTRAPS], %g5			! are there utraps?
	brz,a,pt %g5, 3f
	  nop
	cmp	%g3, T_SOFTWARE_TRAP
	bne,a,pt %icc, 1f
	  nop

	rdpr	%tt, %g1		! check for trap instruction
	sub	%g1, 254, %g1		! UT 16 is defined as 18
	ba,pt	%icc, 2f
	  smul	%g1, CPTRSIZE, %g2
1:
	cmp	%g3, T_UNIMP_INSTR
	bne,a,pt %icc, 2f
	  nop

	rdpr	%tpc, %l6		! check for illtrap instruction
	srl	%l6, 30, %l7		! check for zeros in [31 30]
	brnz,a,pt %l7, 3f
	  nop
	srl	%l6, 22, %l6		! check for zeros in [24 23 22]
	and	%l6, 0x7, %l6
	brnz,a,pt %l6, 3f
	  nop
2:
	ldn	[%g5 + %g2], %g5
	brnz,a,pt %g5, .setup_v9utrap
	  nop
3:
	set	trap, %g1
	ba,pt	%xcc, sys_trap
	  sub	%g0, 1, %g4
	/* NOTREACHED */

/*
 * Register Inputs:
 *	%g5		user trap handler
 */
.setup_v9utrap:
	set	trap, %g1			! setup in case we go
	mov	T_FLUSH_PCB, %g3		! through sys_trap on
	sub	%g0, 1, %g4			! the save instruction below

	save	%sp, -SA(MINFRAME64), %sp	! window for trap handler
	rdpr	%tpc, %l6			! arg0 == tpc
	rdpr	%tnpc, %l7			! arg1 == tnpc
	rdpr	%tstate, %g1			! cwp for trap handler
	rdpr	%cwp, %g4
	bclr	TSTATE_CWP_MASK, %g1
	wrpr	%g1, %g4, %tstate

	CPU_ADDR(%g1, %g4)			! load CPU struct addr
	ldn	[%g1 + CPU_THREAD], %g1		! load thread pointer
	ldn	[%g1 + T_PROCP], %g4		! load proc pointer
	ldn	[%g4 + P_AS], %g4		! load as pointer
	ldn	[%g4 + A_USERLIMIT], %g4	! load as userlimit
	cmp	%l7, %g4			! check for single-step set
	bne,pt	%xcc, 4f
	  nop
	ldn	[%g1 + T_LWP], %g1		! load klwp pointer
	ld	[%g1 + PCB_STEP], %g4		! load single-step flag
	cmp	%g4, STEP_ACTIVE		! step flags set in pcb?
	bne,pt	%icc, 4f
	  nop
	stn	%g5, [%g1 + PCB_TRACEPC]	! save trap handler addr in pcb
	mov	%l7, %g4			! on entry to precise user trap
	add	%l6, 4, %l7			! handler, %l6 == pc, %l7 == npc
						! at time of trap
	wrpr	%g0, %g4, %tnpc			! generate FLTBOUNDS,
						! %g4 == userlimit
	done
	/* NOTREACHED */
4:
	wrpr	%g0, %g5, %tnpc			! trap handler address
	done
	/* NOTREACHED */
#endif	/* __sparcv9 */

.fp_exception:
	CPU_ADDR(%g1, %g4)
	stx	%fsr, [%g1 + CPU_TMP1]
	ldx	[%g1 + CPU_TMP1], %g2
	set	_fp_exception, %g1
	ba,pt	%xcc, sys_trap
	sub	%g0, 1, %g4

.clean_windows:
	set	trap, %g1
	mov	T_FLUSH_PCB, %g2
	sub	%g0, 1, %g4
	save
	flushw
	restore
	wrpr	%g0, %g0, %cleanwin	! no clean windows

	CPU_ADDR(%g4, %g5)
	ldn	[%g4 + CPU_MPCB], %g4
	brz,a,pn %g4, 1f
	  nop
	ld	[%g4 + MPCB_WSTATE], %g5
	add	%g5, WSTATE_CLEAN_OFFSET, %g5
	wrpr	%g0, %g5, %wstate
	done

/*
 * .spill_clean: clean the previous window, restore the wstate, and
 * "done".
 *
 * Entry: %g7 contains new wstate
 */
.spill_clean:
	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g5 ! %g5 = nwin - 1
	rdpr	%cwp, %g6			! %g6 = %cwp
	deccc	%g6				! %g6--
	movneg	%xcc, %g5, %g6			! if (%g6<0) %g6 = nwin-1
	wrpr	%g6, %cwp
	TT_TRACE_L(trace_win)
	clr	%l0
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	wrpr	%g0, %g7, %wstate
	saved
	retry			! restores correct %cwp

.fix_alignment:
	CPU_ADDR(%g1, %g2)		! load CPU struct addr to %g1 using %g2
	ldn	[%g1 + CPU_THREAD], %g1	! load thread pointer
	ldn	[%g1 + T_PROCP], %g1
	mov	1, %g2
	stb	%g2, [%g1 + P_FIXALIGNMENT]
	done

#ifdef __sparcv9
#define	STDF_REG(REG, ADDR, TMP)		\
	sll	REG, 3, REG			;\
mark1:	set	start1, TMP			;\
	jmp	REG + TMP			;\
	  nop					;\
start1:	ba,pt	%xcc, done1			;\
	  std	%f0, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f32, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f2, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f34, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f4, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f36, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f6, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f38, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f8, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f40, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f10, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f42, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f12, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f44, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f14, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f46, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f16, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f48, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f18, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f50, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f20, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f52, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f22, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f54, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f24, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f56, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f26, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f58, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f28, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f60, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f30, [ADDR + CPU_TMP1]		;\
	ba,pt	%xcc, done1			;\
	  std	%f62, [ADDR + CPU_TMP1]		;\
done1:

#define	LDDF_REG(REG, ADDR, TMP)		\
	sll	REG, 3, REG			;\
mark2:	set	start2, TMP			;\
	jmp	REG + TMP			;\
	  nop					;\
start2:	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f0		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f32		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f2		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f34		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f4		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f36		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f6		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f38		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f8		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f40		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f10		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f42		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f12		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f44		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f14		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f46		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f16		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f48		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f18		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f50		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f20		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f52		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f22		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f54		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f24		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f56		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f26		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f58		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f28		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f60		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f30		;\
	ba,pt	%xcc, done2			;\
	  ldd	[ADDR + CPU_TMP1], %f62		;\
done2:

.lddf_exception_not_aligned:
#ifdef DEBUG
	sethi	%hi(fpu_exists), %g2		! check fpu_exists
	ld	[%g2 + %lo(fpu_exists)], %g5
	brz,a,pn %g5, 4f
	  nop
#endif
	CPU_ADDR(%g1, %g4)
	or	%g0, 1, %g4
	st	%g4, [%g1 + CPU_TL1_HDLR] ! set tl1_hdlr flag

	ldxa	[MMU_SFAR]%asi, %g5	! misaligned vaddr in %g5
	rdpr	%tpc, %g2
	lda	[%g2]ASI_USER, %g6	! get the user's lddf instruction
	srl	%g6, 23, %g1		! using ldda or not?
	and	%g1, 1, %g1
	brz,a,pt %g1, 2f		! check for ldda instruction
	  nop
	srl	%g6, 13, %g1		! check immflag
	and	%g1, 1, %g1
	rdpr	%tstate, %g2		! %tstate in %g2
	brnz,a,pn %g1, 1f
	  srl	%g2, 31, %g1		! get asi from %tstate
	srl	%g6, 5, %g1		! get asi from instruction
	and	%g1, 0xFF, %g1		! imm_asi field
1:
	cmp	%g1, ASI_P		! primary address space
	be,a,pt %icc, 2f
	  nop
	cmp	%g1, ASI_PNF		! primary no fault address space
	be,a,pt %icc, 2f
	  nop
	cmp	%g1, ASI_S		! secondary address space
	be,a,pt %icc, 2f
	  nop
	cmp	%g1, ASI_SNF		! secondary no fault address space
	bne,a,pn %icc, 3f
	  nop
2:
	lduwa	[%g5]ASI_USER, %g7	! get first half of misaligned data
	add	%g5, 4, %g5		! increment misaligned data address
	lduwa	[%g5]ASI_USER, %g5	! get second half of misaligned data

	sllx	%g7, 32, %g7
	or	%g5, %g7, %g5		! combine data
	CPU_ADDR(%g7, %g1)		! save data on a per-cpu basis
	stx	%g5, [%g7 + CPU_TMP1]	! save in cpu_tmp1

	srl	%g6, 25, %g3		! %g6 has the instruction
	and	%g3, 0x1F, %g3		! %g3 has rd
	LDDF_REG(%g3, %g7, %g4)

	CPU_ADDR(%g1, %g4)
	st	%g0, [%g1 + CPU_TL1_HDLR] ! clear tl1_hdlr flag
	done
3:
	CPU_ADDR(%g1, %g4)
	st	%g0, [%g1 + CPU_TL1_HDLR] ! clear tl1_hdlr flag
4:
	set	T_USER, %g3		! trap type in %g3
	or	%g3, T_LDDF_ALIGN, %g3
	ldxa	[MMU_SFAR]%asi, %g2	! misaligned vaddr in %g2
	set	fpu_trap, %g1		! goto C for the little and
	ba,pt	%xcc, sys_trap		! no fault little asi's
	  sub	%g0, 1, %g4

.stdf_exception_not_aligned:
#ifdef DEBUG
	sethi	%hi(fpu_exists), %g7		! check fpu_exists
	ld	[%g7 + %lo(fpu_exists)], %g3
	brz,a,pn %g3, 4f
	  nop
#endif
	CPU_ADDR(%g1, %g4)
	or	%g0, 1, %g4
	st	%g4, [%g1 + CPU_TL1_HDLR] ! set tl1_hdlr flag

	ldxa	[MMU_SFAR]%asi, %g5	! misaligned vaddr in %g5
	rdpr	%tpc, %g2
	lda	[%g2]ASI_USER, %g6	! get the user's stdf instruction

	srl	%g6, 23, %g1		! using stda or not?
	and	%g1, 1, %g1
	brz,a,pt %g1, 2f		! check for stda instruction
	  nop
	srl	%g6, 13, %g1		! check immflag
	and	%g1, 1, %g1
	rdpr	%tstate, %g2		! %tstate in %g2
	brnz,a,pn %g1, 1f
	  srl	%g2, 31, %g1		! get asi from %tstate
	srl	%g6, 5, %g1		! get asi from instruction
	and	%g1, 0xFF, %g1		! imm_asi field
1:
	cmp	%g1, ASI_P		! primary address space
	be,a,pt %icc, 2f
	  nop
	cmp	%g1, ASI_S		! secondary address space
	bne,a,pn %icc, 3f
	  nop
2:
	srl	%g6, 25, %g6
	and	%g6, 0x1F, %g6		! %g6 has rd
	CPU_ADDR(%g7, %g1)
	STDF_REG(%g6, %g7, %g4)		! STDF_REG(REG, ADDR, TMP)

	ldx	[%g7 + CPU_TMP1], %g6
	srlx	%g6, 32, %g7
	stuwa	%g7, [%g5]ASI_USER	! first half
	add	%g5, 4, %g5		! increment misaligned data address
	stuwa	%g6, [%g5]ASI_USER	! second half

	CPU_ADDR(%g1, %g4)
	st	%g0, [%g1 + CPU_TL1_HDLR] ! clear tl1_hdlr flag
	done
3:
	CPU_ADDR(%g1, %g4)
	st	%g0, [%g1 + CPU_TL1_HDLR] ! clear tl1_hdlr flag
4:
	set	T_USER, %g3		! trap type in %g3
	or	%g3, T_STDF_ALIGN, %g3
	ldxa	[MMU_SFAR]%asi, %g2	! misaligned vaddr in %g2
	set	fpu_trap, %g1		! goto C for the little and
	ba,pt	%xcc, sys_trap		! nofault little asi's
	  sub	%g0, 1, %g4
#endif /* __sparcv9 */

#ifdef DEBUG_USER_TRAPTRACECTL

.traptrace_freeze:
	mov	%l0, %g1 ; mov	%l1, %g2 ; mov	%l2, %g3 ; mov	%l4, %g4
	TT_TRACE_L(trace_win)
	mov	%g4, %l4 ; mov	%g3, %l2 ; mov	%g2, %l1 ; mov	%g1, %l0
	set	trap_freeze, %g1
	mov	1, %g2
	st	%g2, [%g1]
	done

.traptrace_unfreeze:
	set	trap_freeze, %g1
	st	%g0, [%g1]
	mov	%l0, %g1 ; mov	%l1, %g2 ; mov	%l2, %g3 ; mov	%l4, %g4
	TT_TRACE_L(trace_win)
	mov	%g4, %l4 ; mov	%g3, %l2 ; mov	%g2, %l1 ; mov	%g1, %l0
	done

#endif /* DEBUG_USER_TRAPTRACECTL */

.getcc:
	CPU_ADDR(%g1, %g2)
	stx	%o0, [%g1 + CPU_TMP1]		! save %o0
	stx	%o1, [%g1 + CPU_TMP2]		! save %o1
	mov	%g1, %o1
	rdpr	%tstate, %g1			! get tstate
	srlx	%g1, PSR_TSTATE_CC_SHIFT, %o0	! shift ccr to V8 psr
	set	PSR_ICC, %g2
	and	%o0, %g2, %o0			! mask out the rest
	srl	%o0, PSR_ICC_SHIFT, %o0		! right justify
	rdpr	%pstate, %g1
	wrpr	%g1, PSTATE_AG, %pstate		! get into normal globals
	mov	%o0, %g1
	ldx	[%o1 + CPU_TMP1], %o0		! restore %o0
	ldx	[%o1 + CPU_TMP2], %o1		! restore %o1
	done

.setcc:
	CPU_ADDR(%g1, %g2)
	stx	%o0, [%g1 + CPU_TMP1]		! save %o0
	stx	%o1, [%g1 + CPU_TMP2]		! save %o1
	rdpr	%pstate, %o0
	wrpr	%o0, PSTATE_AG, %pstate		! get into normal globals
	mov	%g1, %o1
	wrpr	%g0, %o0, %pstate		! back to alternates
	sll	%o1, PSR_ICC_SHIFT, %g2
	set	PSR_ICC, %g3
	and	%g2, %g3, %g2			! mask out rest
	sllx	%g2, PSR_TSTATE_CC_SHIFT, %g2
	rdpr	%tstate, %g3			! get tstate
	srl	%g3, 0, %g3			! clear upper word
	or	%g3, %g2, %g3			! or in new bits
	wrpr	%g3, %tstate
	ldx	[%g1 + CPU_TMP1], %o0		! restore %o0
	ldx	[%g1 + CPU_TMP2], %o1		! restore %o1
	done

/*
 * getpsr(void)
 * Note that the xcc part of the ccr is not provided.
 * The V8 code shows why the V9 trap is not faster:
 * #define GETPSR_TRAP() \
 *      mov %psr, %i0; jmp %l2; rett %l2+4; nop;
 */

	.type	.getpsr, #function
.getpsr:
	rdpr	%tstate, %g1			! get tstate
	srlx	%g1, PSR_TSTATE_CC_SHIFT, %o0	! shift ccr to V8 psr
	set	PSR_ICC, %g2
	and	%o0, %g2, %o0			! mask out the rest

	rd	%fprs, %g1			! get fprs
	and	%g1, FPRS_FEF, %g2		! mask out dirty upper/lower
	sllx	%g2, PSR_FPRS_FEF_SHIFT, %g2	! shift fef to V8 psr.ef
	or	%o0, %g2, %o0			! or result into psr.ef

	set	V9_PSR_IMPLVER, %g2		! SI assigned impl/ver: 0xef
	or	%o0, %g2, %o0			! or psr.impl/ver
	done
	SET_SIZE(.getpsr)

/*
 * setpsr(newpsr)
 * Note that there is no support for ccr.xcc in the V9 code.
 */

	.type	.setpsr, #function
.setpsr:
	rdpr	%tstate, %g1			! get tstate
!	setx	TSTATE_V8_UBITS, %g2
	or 	%g0, CCR_ICC, %g3
	sllx	%g3, TSTATE_CCR_SHIFT, %g2

	andn	%g1, %g2, %g1			! zero current user bits
	set	PSR_ICC, %g2
	and	%g2, %o0, %g2			! clear all but psr.icc bits
	sllx	%g2, PSR_TSTATE_CC_SHIFT, %g3	! shift to tstate.ccr.icc
	wrpr	%g1, %g3, %tstate		! write tstate

	set	PSR_EF, %g2
	and	%g2, %o0, %g2			! clear all but fp enable bit
	srlx	%g2, PSR_FPRS_FEF_SHIFT, %g4	! shift ef to V9 fprs.fef
	wr	%g0, %g4, %fprs			! write fprs

	CPU_ADDR(%g1, %g2)			! load CPU struct addr to %g1
	ldn	[%g1 + CPU_THREAD], %g2		! load thread pointer
	ldn	[%g2 + T_LWP], %g3		! load klwp pointer
	ldn	[%g3 + LWP_FPU], %g2		! get lwp_fpu pointer
	stuw	%g4, [%g2 + FPU_FPRS]		! write fef value to fpu_fprs
	srlx	%g4, 2, %g4			! shift fef value to bit 0
	stub	%g4, [%g2 + FPU_EN]		! write fef value to fpu_en
	done
	SET_SIZE(.setpsr)

/*
 * Entry for old 4.x trap (trap 0).
 */
	ENTRY_NP(syscall_trap_4x)
	CPU_ADDR(%g1, %g2)		! load CPU struct addr to %g1 using %g2
	ldn	[%g1 + CPU_THREAD], %g2	! load thread pointer
	ldn	[%g2 + T_LWP], %g2	! load klwp pointer
	ld	[%g2 + PCB_TRAP0], %g2	! lwp->lwp_pcb.pcb_trap0addr
	brz,pn	%g2, 1f			! has it been set?
	st	%l0, [%g1 + CPU_TMP1]	! delay - save some locals
	mov	%g1, %l0
	st	%l1, [%g1 + CPU_TMP2]
	rdpr	%tnpc, %l1		! save old tnpc
	wrpr	%g0, %g2, %tpc		! setup tpc
	add	%g2, 4, %g2
	wrpr	%g0, %g2, %tnpc		! setup tnpc
	rdpr	%pstate, %g1
	wrpr	%g1, PSTATE_AG, %pstate	! switch to normal globals
	mov	%l1, %g7		! pass tnpc to user code in %g7
	ld	[%l0 + CPU_TMP2], %l1	! restore locals
	ld	[%l0 + CPU_TMP1], %l0
	retry
1:
	mov	%g1, %l0
	st	%l1, [%g1 + CPU_TMP2]
	rdpr	%pstate, %l1
	wrpr	%l1, PSTATE_AG, %pstate
	!
	! check for old syscall mmap which is the only different one which
	! must be the same.  Others are handled in the compatibility library.
	!
	cmp	%g1, OSYS_mmap	! compare to old 4.x mmap
	movz	%icc, SYS_mmap, %g1
	wrpr	%g0, %l1, %pstate
	ld	[%l0 + CPU_TMP2], %l1	! restore locals
	ld	[%l0 + CPU_TMP1], %l0
#ifdef __sparcv9
	SYSCALL(syscall_trap32)
#else
	SYSCALL(syscall_trap);
#endif
	SET_SIZE(syscall_trap_4x)

/*
 * Handler for software trap 9.
 * Set trap0 emulation address for old 4.x system call trap.
 * XXX - this should be a system call.
 */
	ENTRY_NP(set_trap0_addr)
	CPU_ADDR(%g1, %g2)		! load CPU struct addr to %g1 using %g2
	ldn	[%g1 + CPU_THREAD], %g2	! load thread pointer
	ldn	[%g2 + T_LWP], %g2	! load klwp pointer
	st	%l0, [%g1 + CPU_TMP1]	! save some locals
	st	%l1, [%g1 + CPU_TMP2]
	rdpr	%pstate, %l0
	wrpr	%l0, PSTATE_AG, %pstate
	mov	%g1, %l1
	wrpr	%g0, %l0, %pstate
	andn	%l1, 3, %l1		! force alignment
	st	%l1, [%g2 + PCB_TRAP0]	! lwp->lwp_pcb.pcb_trap0addr
	ld	[%g1 + CPU_TMP1], %l0	! restore locals
	ld	[%g1 + CPU_TMP2], %l1
	done
	SET_SIZE(set_trap0_addr)

/*
 * mmu_trap_tl1
 * trap handler for unexpected mmu traps.
 * simply checks if the trap was a user lddf/stdf alignment trap, in which
 * case we go to fpu_trap or a user trap from the window handler, in which
 * case we go save the state on the pcb.  Otherwise, we go to ptl1_panic.
 */
	.type	mmu_trap_tl1, #function
mmu_trap_tl1:
#ifdef	TRAPTRACE
	TRACE_PTR(%g5, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g5 + TRAP_ENT_TICK]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g5 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	stha	%g6, [%g5 + TRAP_ENT_TT]%asi
	rdpr	%tstate, %g6
	stxa	%g6, [%g5 + TRAP_ENT_TSTATE]%asi
	stna	%sp, [%g5 + TRAP_ENT_SP]%asi
	stna	%g0, [%g5 + TRAP_ENT_TR]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g5 + TRAP_ENT_TPC]%asi
	set	MMU_SFAR, %g6
	ldxa	[%g6]ASI_DMMU, %g6
	stxa	%g6, [%g5 + TRAP_ENT_F1]%asi
#ifdef __sparcv9
	CPU_ADDR(%g7, %g6)
	ld	[%g7 + CPU_TL1_HDLR], %g6
	stxa	%g6, [%g5 + TRAP_ENT_F2]%asi
#endif
	set	0xdeadbeef, %g6
	stna	%g6, [%g5 + TRAP_ENT_F3]%asi
	stna	%g6, [%g5 + TRAP_ENT_F4]%asi
	TRACE_NEXT(%g5, %g6, %g7)
#endif /* TRAPTRACE */
	CPU_ADDR(%g7, %g6)
	ld	[%g7 + CPU_TL1_HDLR], %g6
	brz,a,pt %g6, 1f
	  nop
	st	%g0, [%g7 + CPU_TL1_HDLR]
	set	T_USER, %g3		! trap type in %g3
	or	%g3, T_LDDF_ALIGN, %g3
	set	fpu_trap, %g1		! goto C for any unexpected
	ba,pt	%xcc, sys_trap		! data access exception traps
	  sub	%g0, 1, %g4
1:
	rdpr	%tl, %g7
	sub	%g7, 1, %g6
	wrpr	%g6, %tl
	rdpr	%tt, %g5
	wrpr	%g7, %tl
	and	%g5, WTRAP_TTMASK, %g6
	cmp	%g6, WTRAP_TYPE
	bne,a,pn %xcc, ptl1_panic
	mov	PTL1_BAD_MMUTRAP, %g1
	mov	MMU_SFAR, %g7
	ldxa	[%g7]ASI_DMMU, %g6
	rdpr	%tpc, %g7
	/* tpc should be in the trap table */
	set	trap_table, %g5
	cmp	%g7, %g5
	blt,a,pn %xcc, ptl1_panic
	  mov	PTL1_BAD_MMUTRAP, %g1
	set	etrap_table, %g5
	cmp	%g7, %g5
	bge,a,pn %xcc, ptl1_panic
	  mov	PTL1_BAD_MMUTRAP, %g1
	mov	T_DATA_MMU_MISS, %g5	/* hardwire trap type to dmmu miss */
	andn	%g7, WTRAP_ALIGN, %g7	/* 128 byte aligned */
	add	%g7, WTRAP_FAULTOFF, %g7
	wrpr	%g0, %g7, %tnpc
	done
	SET_SIZE(mmu_trap_tl1)

#if !defined(lint)
.asyncilevel1msg:
	.asciz "Async instruction error at tl1: AFAR 0x%08x.%08x AFSR 0x%08x.%08x"
.asyncdlevel1msg:
	.asciz "Async data error at tl1: AFAR 0x%08x.%08x AFSR 0x%08x.%08x"
	.align	4
#endif
	ERR_PANIC_TL1(async_itrap, .asyncilevel1msg)

	ERR_PANIC_TL1(async_dtrap, .asyncdlevel1msg)

/*
 * These two get overlaid with the associated debugger's
 * breakpoint entry.
 */
	.global	kadb_bpt
	.global	obp_bpt
	.align	8
kadb_bpt:
	NOT
obp_bpt:
	NOT



#ifdef	TRAPTRACE
/*
 * TRAPTRACE support.
 * labels here are branched to with "rd %pc, %g7" in the delay slot.
 * Return is done by "jmp %g7 + 4".
 */

trace_gen:
	TRACE_PTR(%g3, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g3 + TRAP_ENT_TICK]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g3 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	stha	%g6, [%g3 + TRAP_ENT_TT]%asi
	rdpr	%tstate, %g6
	stxa	%g6, [%g3 + TRAP_ENT_TSTATE]%asi
	stna	%sp, [%g3 + TRAP_ENT_SP]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g3 + TRAP_ENT_TPC]%asi
	TRACE_NEXT(%g3, %g4, %g5)
	jmp	%g7 + 4
	nop

trace_win:
	TRACE_WIN_INFO(0, %l0, %l1, %l2)
	! Keep the locals as clean as possible, caller cleans %l4
	clr	%l2
	clr	%l1
	jmp	%l4 + 4
	  clr	%l0

trace_tsbhit:
	membar	#Sync
	sethi	%hi(FLUSH_ADDR), %g6
	flush	%g6
	TRACE_PTR(%g3, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g3 + TRAP_ENT_TICK]%asi
	stxa	%g2, [%g3 + TRAP_ENT_TSTATE]%asi	! tlb tag
	stxa	%g5, [%g3 + TRAP_ENT_F1]%asi		! tsb data
#ifdef __sparcv9
	rdpr	%tnpc, %g6
	stxa	%g6, [%g3 + TRAP_ENT_F2]%asi
#endif
	stxa	%g1, [%g3 + TRAP_ENT_F3]%asi		! tsb pointer
#ifdef __sparcv9
	stxa	%g0, [%g3 + TRAP_ENT_F4]%asi
#endif
	rdpr	%tpc, %g6
	stna	%g6, [%g3 + TRAP_ENT_TPC]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g3 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	stha	%g6, [%g3 + TRAP_ENT_TT]%asi
	mov	MMU_TAG_ACCESS, %g4
	ldxa	[%g4] ASI_IMMU, %g1
	ldxa	[%g4] ASI_DMMU, %g4
	cmp	%g6, FAST_IMMU_MISS_TT
	movne	%icc, %g4, %g1
	stxa	%g1, [%g3 + TRAP_ENT_SP]%asi		! tag access
#ifdef __sparcv9
	stxa	%g0, [%g3 + TRAP_ENT_TR]%asi
#endif
	TRACE_NEXT(%g3, %g4, %g5)
	jmp	%g7 + 4
	nop

trace_tsbmiss:
	membar	#Sync
	sethi	%hi(FLUSH_ADDR), %g6
	flush	%g6
	TRACE_PTR(%g5, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g5 + TRAP_ENT_TICK]%asi
	stxa	%g2, [%g5 + TRAP_ENT_TSTATE]%asi	! tlb tag
	stxa	%g4, [%g5 + TRAP_ENT_F1]%asi		! tsb tag
#ifdef __sparcv9
	rdpr	%tnpc, %g6
	stxa	%g6, [%g5 + TRAP_ENT_F2]%asi
#endif
	stna	%g0, [%g5 + TRAP_ENT_F3]%asi
	stna	%g0, [%g5 + TRAP_ENT_F4]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g5 + TRAP_ENT_TPC]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g5 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	or	%g6, 0x100, %g4
	stha	%g4, [%g5 + TRAP_ENT_TT]%asi
	mov	MMU_TAG_ACCESS, %g4
	cmp	%g6, FAST_IMMU_MISS_TT
	be,a	%icc, 1f
	  ldxa	[%g4] ASI_IMMU, %g6
#ifdef __sparcv9
	stxa	%g0, [%g5 + TRAP_ENT_TR]%asi
#endif
	ldxa	[%g4] ASI_DMMU, %g6
1:
	stxa	%g6, [%g5 + TRAP_ENT_SP]%asi		! tag access
	TRACE_NEXT(%g5, %g4, %g6)
	jmp	%g7 + 4
	nop

trace_dataprot:
	membar	#Sync
	sethi	%hi(FLUSH_ADDR), %g6
	flush	%g6
	TRACE_PTR(%g1, %g6)
	GET_TRACE_TICK(%g6)
	stxa	%g6, [%g1 + TRAP_ENT_TICK]%asi
	rdpr	%tpc, %g6
	stna	%g6, [%g1 + TRAP_ENT_TPC]%asi
	rdpr	%tstate, %g6
	stxa	%g6, [%g1 + TRAP_ENT_TSTATE]%asi
	stxa	%g2, [%g1 + TRAP_ENT_SP]%asi		! tag access reg
#ifdef __sparcv9
	stxa	%g0, [%g1 + TRAP_ENT_TR]%asi
#endif
	stna	%g0, [%g1 + TRAP_ENT_F1]%asi
	stna	%g0, [%g1 + TRAP_ENT_F2]%asi
	stna	%g0, [%g1 + TRAP_ENT_F3]%asi
	stna	%g0, [%g1 + TRAP_ENT_F4]%asi
	rdpr	%tl, %g6
	stha	%g6, [%g1 + TRAP_ENT_TL]%asi
	rdpr	%tt, %g6
	stha	%g6, [%g1 + TRAP_ENT_TT]%asi
	TRACE_NEXT(%g1, %g4, %g5)
	jmp	%g7 + 4
	nop

#endif /* TRAPTRACE */

/*
 * expects offset into tsbmiss area in %g1 and return pc in %g7
 */
stat_mmu:
	CPU_INDEX(%g5, %g6)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g5, TSBMISS_SHIFT, %g5
	or	%g6, %lo(tsbmiss_area), %g6
	add	%g6, %g5, %g6		/* g6 = tsbmiss area */
	ld	[%g6 + %g1], %g5
	add	%g5, 1, %g5
	jmp	%g7 + 4
	st	%g5, [%g6 + %g1]

#endif	/* lint */
