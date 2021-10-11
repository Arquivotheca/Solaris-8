/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vik_underflow.s	1.10	99/04/13 SMI"

/* From: 5.1: srmmu/ml/underflow.s 1.23 92/12/15 SMI */

/*
 * The entry points in this file are not callable from C and hence are of no
 * interest to lint.
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/thread.h>
#endif

#include <sys/asm_linkage.h>
#include <sys/machparam.h>
#include <sys/machpcb.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/machthread.h>

#if !defined(lint)

#include "assym.h"

/*
 * These should probably be somewhere else.
 */
#define FLP_TYPE(b)	(((b) << 8) & 0x00000F00)
VIK_NWINDOWS = 8

/*
 * sun4m Viking window underflow trap handler.
 *
 *     Supervisor underflow is straight forward.  For user underflow we have
 *     to worry about the possibility that the stack page may not exist, or
 *     might be swapped out.
 *
 * On entry:
 *
 *     %l1, %l2 = %pc, %npc
 *     	   %l1=-1 => return to %l2+8, no rett
 *         (the intention is to use this in the future to clean up locore.s,
 *         although more work is needed here first).
 *     %l3 = %wim
 *
 * Trap window:
 *
 *     %l0 = %psr
 *     %o6 = scratch
 *     %o7 = scratch
 *
 * Target window:
 *
 *     %l4 = result of probe
 *     %l5 = base address of page to probe
 *     %l6 = scratch
 *     %l7 = scratch
 */
	.align	32
	ENTRY_NP(vik_window_underflow)
#ifdef TRACE
	CPU_ADDR(%l7, %l4)		! get CPU struct ptr to %l7 using %l4
	!
	! See if event is enabled, using %l4 and %l5 as scratch
	!
	VT_ASM_TEST_FT(TR_FAC_TRAP, TR_KERNEL_WINDOW_UNDERFLOW, %l7, %l4, %l5)
	!
	! We now have: %l7 = cpup, %l4 = event, %l5 = event info, and
	! the condition codes are set (ZF means not enabled)
	!
	bz	9f			! event not enabled
	sll	%l4, 16, %l4		! %l4 = (event << 16), %l5 = info
	or	%l4, %l5, %l4		! %l4 = (event << 16) | info
	st	%l3, [%l7 + CPU_TRACE_SCRATCH]		! save %l3
	st	%l6, [%l7 + CPU_TRACE_SCRATCH + 4]	! save %l6
	!
	! Dump the trace record.  The args are:
	!	(event << 16) | info, data, cpup, and three scratch registers.
	! In this case, the data is the trapped PC (%l1).
	!
	TRACE_DUMP_1(%l4, %l1, %l7, %l3, %l5, %l6)
	!
	! Trace done, restore saved registers
	!
	ld	[%l7 + CPU_TRACE_SCRATCH], %l3		! restore %l3
	ld	[%l7 + CPU_TRACE_SCRATCH + 4], %l6	! restore %l6
9:
#endif  /* TRACE */
	mov	%psr, %l0
	!-
	add	%l3, %l3, %o6		! new WIM = rol(old WIM, 1, NWINDOWS)
	srl	%l3, VIK_NWINDOWS - 1, %o7
	!-
	wr	%o6, %o7, %wim		! install new wim
	!-
	mov	%l2, %o7		! (wim delay) put npc in an unobtrusive
					! place for later on
	btst	PSR_PS, %l0		! (wim delay) supervisor mode trap?
	bnz	vik_wu_super		! (wim delay)
	!-
	restore				! (delay slot)
	!-

	!
	! Handle user underflow.
	!
	! Need to check that the stack pointer is reasonable, and do a probe to
	! see if the page is present.
	!
	!     **********************************************************
	!     * THIS CODE IS MORE SUBTLE THAN IT MIGHT AT FIRST APPEAR *
	!     * DO NOT MODIFY IT WITHOUT READING THE EXPLANATION BELOW *
	!     **********************************************************
	!
	restore	%fp, 7*8, %l6		! get into window to be restored
					! address of last ldd in %l6
	!-
	andn	%l6, MMU_STD_PAGESIZE - 8, %l6	! cut to page boundary, leaving
						! lowest few bits unchanged
	andn	%sp, MMU_STD_PAGESIZE - 1, %l5	! page base address in %l5
	!-
	or	%l5, FLP_TYPE(MMU_FLP_ALL), %l7	! address to use for probe
	cmp	%l5, %l6			! do we cross a page boundary,
	be,a	0f				! or is the stack misaligned?
	!-
	lda	[%l7]ASI_FLPR, %l4		! (delay slot) probe - MP safe!
	!-
	ba,a	vik_wu_poorly_aligned	! drats
	!-
0:
	!
	! Good, do not cross a page boundary, not misaligned.  Check result of
	! probe.
	!
	and	%l4, PTE_PERMS(4|1) | PTE_ETYPEMASK, %l7
						! ignore middle permissions bit
	cmp	%l7, PTE_PERMS(1) | PTE_ETYPE(MMU_ET_PTE)
	!-
	be,a	1f
	!-
	ldd	[%sp + 0*8], %l0		! (delay slot) start restoring
	!-
	!
	! The above handles the common cases, permissions 1 and 3 - user RW and
	! user RWX.  Also need to handle the uncommon cases, permissions 0, 2,
	! and 5 - user R, user RX, and user R.  The stack might be read only
	! due to copy on write.
	!
	and	%l4, PTE_PERMMASK | PTE_ETYPEMASK, %l7
	cmp	%l7, PTE_PERMS(MMU_STD_SRUR) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%l7, PTE_PERMS(MMU_STD_SRXURX) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%l7, PTE_PERMS(MMU_STD_SRWUR) | PTE_ETYPE(MMU_ET_PTE)
	be,a	1f
	ldd	[%sp + 0*8], %l0		! (delay slot) start restoring
	ba,a	vik_wu_fault			! no PTE, or wrong permissions
1:
	!
	! Good, page is readable.
	!
	ldd	[%sp + 1*8], %l2		! slurp the registers up
	!-
	ldd	[%sp + 2*8], %l4
	!-
	ldd	[%sp + 3*8], %l6
	!-
	ldd	[%sp + 4*8], %i0
	!-
	ldd	[%sp + 5*8], %i2
	!-
	ldd	[%sp + 6*8], %i4
	!-
	ldd	[%sp + 7*8], %i6
	!-

vik_wu_out:
	!
	! Return from user underflow.
	!
	! If the CLEAN_WINDOWS flag is set in the pcb we need to clean any
	! registers we have dirtied.  Quicker to always do this than find out
	! if we have to.
	!
	! According to the ABI the values in %l1 and %l2 need not be cleared,
	! but we can clear them without burning any cycles, so lets be nice.
	!
	save
	!-
	save	%g0, %g0, %l3		! back into trap window, clean %l3
	!-
	clr	%l2
	orcc	%l1, %g0, %o6		! put pc in an unobtrusive place
	bneg	1f			! possibly return with a rett?
	!-
	mov	%l0, %psr		! (delay slot) reinstall PSR_CC
	!-
0:
	clr	%l1			! (psr delay)
	clr	%l0			! (psr delay)
	jmp	%o6			! (psr delay) reexecute restore
	! Does jmp use a write port?  If so maybe we should recode to avoid
	! the extra cycle.  psr should be read immediately after new wim is
	! set, psr value stored in %l3, slow paths that need old wim can figure
	! it out from %o6, save can clear %l2, %l1 cleared just after orcc,
	! clear %l3 just before jmp, no longer need to clear %l0, but need to
	! replace with a nop due to psr delay.  Does nop use a write port?
	!-
	rett	%o7
	!-
1:
	cmp	%l1, -1			! return with a rett?
	bne	0b
	!-
	mov	%l0, %psr		! (delay slot) reinstall PSR_CC
	!-
	clr	%l1			! (psr delay) return without a rett
	jmp	%o7+8			! (psr delay)
	!-
	clr	%l0			! (psr delay)

	!
	! I've decided to enter a contest to see what is the longest comment
	! that can be written to explain why what at first glance appears to be
	! perfectly innocent code actually is.
	!
	! The innocence of the above code is shatered by the fact that between
	! the time we probe to see if the stack is present and the time we load
	! or store the registers another cpu might come along and steal the
	! stack page.
	!
	! This will not cause any problems on sun4m Viking systems provided
	! assumption A1 is true.
	!
	! Assumption A1:
	!     We have at least 3 non-locked down entries in the TLB.
	!
	! sun4m Viking systems do not use a hardware TLB demap.  Instead when
	! a page is invalidate the cpu doing the invalidation interrupts all
	! other cpus that might have the PTE in their TLB requesting that they
	! flush this PTE from their TLB.
	!
	! Since we are executing with traps disabled we won't be interrupted
	! with a request to perform a TLB shoot down.  Thus a probe will ensure
	! that the PTE for the user's stack is present in the TLB.  The only
	! risk is that the PTE might be discarded from the TLB before we finish
	! saving or restoring the registers.
	!
	! TLB replacement on the Viking uses a "limited history LRU algorithm".
	! Each TLB entry has a used bit that is set whenever it is used to
	! perform a translation.  If a translation is not found in the TLB the
	! PTE for the translation replaces some other entry already in the TLB
	! and the used bit for the new entry is set.  When selecting a TLB
	! entry to be replaced one of the entries whose used bit is clear is
	! chosen.  If a translation causes all entries in the TLB to have their
	! used bits set then all entries except for the entry used to perform
	! that translation have their used bit cleared.  TLB entries can be
	! locked down.  The TLB replacement algorithm does not apply to such
	! entries.
	!
	! I don't think we currently ever lock TLB entries down, but admitting
	! this as a possibility makes it easier to do so, if we so decide to at
	! some future date. 
	!
	! Lemma L1:
	!     Immediately after accessing X, a non-locked down PTE, X will
	!     be present in the TLB, and X will have its used bit set.
	! Proof:
	!     Assume A1.
	!     Case: X was already in the TLB at the time of the access
	!         Obvious.
	!     Case: X was not in the TLB at the time of the access
	!         Obvious.
	!
	! Lemma L2:
	!     Consider accessing X, a non-locked down PTE.  If X is in the TLB
	!     immediately prior to its having been accessed then immediately
	!     subsequent to its having been accessed the identity of the PTEs
	!     in the TLB will be unchaged.
	! Proof:
	!     Since X is already in the TLB no TLB entry will be replaced.
	!
	! Note that it is the identity of the PTEs in the TLB that are
	! unchanged.  We do not mention the TLB as a whole, including the used
	! bits.
	!
	! Lemma L3:
	!     A sequence of one or more consecutive accesses to X, a non-locked
	!     down PTE, will leave the TLB in the same state as a single
	!     access.
	! Proof:
	!     From L1, after the first access X will be in the TLB and have its
	!     used bit set.  Subsequent accesses will simply set the used bit,
	!     which is already set.
	!
	! Henceforth when we speak of an access to a non-locked down PTE we
	! also include the possibility of a sequence of consecutive accesses.
	!
	! We will use X Y to denote accessing PTE X immediately followed by
	! accessing PTE Y.  Since access includes the possibility of multiple
	! consecutive accesses it follows that we can make statements such as
	! X X = X, X X Y = X Y, and so on.
	!
	! Lemma L4:
	!     Immediately after the sequence of TLB accesses X Y, where X and Y
	!     are two, possibily identical, non-locked down PTEs, X will be
	!     present in the TLB.
	! Proof:
	!     From L1 prior to Y we know X will be present in the TLB and have
	!     its used bit set.  If Y replaces a TLB entry it will replace one
	!     whose used bit is clear.  Thus it will not replace X.
	!
	! We will use [ ] to indicate that the enclosed sequence of accesses
	! may or may not be performed.
	!
	! Lemma L5:
	!     Following the sequence of non-locked TLB accesses X [Y], X will
	!     be present in the TLB.
	! Proof:
	!     Case [Y] is null:
	!         By L1.
	!     Case [Y]=Y:
	!         By L4.
	!
	! If the PTE for the users stack pointer happens to be locked down we
	! obviously are not going to have any problems.  So assume that the PTE
	! for the user's stack page is not locked down.  We will use S to
	! denote accessing the PTE for the user's stack page.
	!
	! Between the probe and the saving or restoring of the registers to
	! the user's page we will be accessing at most two other pages.  This
	! will happen if the intervening code crosses a page boundary.
	! Clearly, given the size of the critical code, we will never cross
	! more than one page boundary.  We will use A and B to refer to
	! accesses to two non-locked down PTEs.
	!
	! For the sequence of accesses,
	!
	!     A S [A] [B] S,
	!
	! we are about to prove that S is present in the TLB when the second
	! access to S is performed.
	!
	! Theorem T1:
	!     Following the sequence of non-locked accesses, A S [A] [B],
	!     S will be present in the TLB.
	! Proof:
	!     Denote the following times:
	!
	!         A   S   [A]   [B]
	!           ^   ^     ^     ^
	!           |   |     |     |
	!           t1  t2    t3    t4
	!
	!     By L4 A is in the TLB at t2.
	!     Case A has used bit set at t2:
	!         By L1 S has its used bit set at t2.
	!         From the algorithm since A has the used bit set at t2 the
	!         TLB will be unchanged at t3 from its state at t2.
	!         Thus S has its used bit set at t3.
	!         Thus from the proof of L4 S will be present in the TLB at t4.
	!     Case A has used bit clear at t2:
	!         By L1 A has its used bit set at t1.
	!         Since the bit is clear at t2 this means S must have cleared
	!         it.  Thus refering to the algorithm all used bits must be
	!         clear at t2 with the exception of S.
	!         From A1 this means we have at least 2 entries with the used
	!         bit clear at t2.
	!         Therefore from the algorithm S will be present in the TLB at
	!         t4.
	!
	! Theorem T2:
	!     Following the sequence of non-locked accesses, A S [B] [A],
	!     S will be present in the TLB.
	! Proof:
	!     Denote the following times:
	!
	!         A   S   [B]   [A]
	!           ^   ^     ^     ^
	!           |   |     |     |
	!           t1  t2    t3    t4
	!
	!     By L5 S is in the TLB at t3.
	!     Case A is in the TLB at t3:
	!         By L2 S is in the TLB at t4.
	!     Case A is not in the TLB at t3:
	!         By L4 A is in the TLB at t2.
	!         Thus [B] must have displaced A from the TLB.
	!         Since we only displace entries whose used bit is clear, A's
	!         used bit must be clear at t2.
	!         By L1 A has its used bit set at t1.
	!         Since the bit is clear at t2 this means S must have cleared
	!         it.  Thus refering to the algorithm all used bits must be
	!         clear at t2 with the exception of S.
	!         From A1 this means we have at least 2 entries with the used
	!         bit clear at t2.
	!         Therefore from the algorithm S will be present in the TLB at
	!         t4.
	!
	! We do not perform a single access to S following the probe, instead
	! we perform a sequence of accesses interspersed with accesses to the
	! PTEs for the code.
	!
	! We use the symbol | to indicate selection of either of the two
	! adjacent sequences of PTE accesses.  We use the symbol ( )* to
	! indicate a finite, possibly zero, number of repititions of the
	! enclosed PTE accesses.
	!
	! Theorem T3:
	!     Following the sequence of non-locked accesses, A S (A|B|S)*,
	!     S will be present in the TLB.
	! Proof:
	!     Consider the access sequence comprising the access immediately
	!     prior to the last access to S and all subsequent accesses.
	!     Without loss of generality this can be written as A S (A|B)*.
	!     This may be written in one or more of the following three forms.
	!     Case A S A B (A|B)*:
	!         By T1 S is in the TLB immediately prior to (A|B)*.
	!         By L1, L4, A and B will be in the TLB immediately prior to
	!         (A|B)*.
	!         Thus by L2 the TLB will be unchaged following (A|B)*.
	!     Case A S B A (A|B)*:
	!         By T2 S is in the TLB immediately prior to (A|B)*.
	!         By L1, L4, A and B will be in the TLB immediately prior to
	!         (A|B)*.
	!         Thus by L2 the TLB will be unchaged following (A|B)*.
	!     Case A S [A] [B]:
	!         By T1 S is in the TLB.
	!
	! Since the sequence of PTE accesses leading up to any of the accesses
	! to S used to load or store register values is always a special
	! instance of A S (A|B|S)* it follows that S will be present in the TLB
	! for each such access.
	!
	! This completes the core of our proof that this code will work.
	!
	! Note that the cases where our code does not cross a page boundary are
	! special cases of the above in which no accesses to B occur.
	!
	! We also need to consider the possibility that one or both of the code
	! pages are locked down.  References to locked pages can be effectively
	! ignored.  Correctness in this case is established by showing that S
	! is present in the TLB following the sequence of accesses S (A|S)*.
	! This follows trivially from L5.
	!
	! So far we have been considering what might be described as an ideal
	! CPU.  Viking is actually a pipelined superscalar processor.
	! Fortunately it does not execute instructions out of order, but there
	! are still several complications.
	!
	! Instruction TLB access is performed at pipeline stage F0, while probe
	! and load/store TLB accesses are performed at stage E0.  Thus we need
	! to know that for the instruction PTE access to A which logically
	! occurs immediately prior to the probe S there is an equivalent actual
	! instruction PTE access immediately prior to the probe S.  Similarly
	! we need to be sure that a TLB fetch for transfering control to a
	! third page prior to the final load/store to S does not occur.  The
	! Viking documentation I have is insufficiently clear for me to be able
	! to argue this from the specifics of the code.  I will simply point
	! out that the immediately preceeding code is free from loads/stores,
	! and that the immediately following code is free from distant control
	! transfers.  Note that the distance between F0 and E0 is two pipeline
	! stages, and that lda and save/restore all occupy a pipeline stage by
	! themselves.  Thus the distance over which these constraints intrude
	! into the surrounding code is truly minimal, no more than a few
	! instructions.
	!
	! Viking always prefetches the instructions at the branch target
	! address, irrespective of whether the branch is actually taken.  We
	! need to ensure that this does not violate the assumption we make that
	! the code accesses consume at most two PTEs.  Observe that the
	! branches within this critical code have non-distant targets.
	!
	! Note that while the probe operation is generic to the SPARC reference
	! MMU, and the ASI to which it is bound is specified by the sun4m
	! architecture, neither of these documents provide the assurances
	! we require for this code to work correctly.  We require the probe to
	! behave just like just like a normal memory access, that is to first
	! try and find the entry we need by accessing the TLB, and if a miss
	! occurs to load the PTE found as the result of a table walk into the
	! TLB.  The Viking probe entire operation does this.
	!
	! This code will not work on the sun4d, it has a hardware demap.
	!
	! Yes, this is pretty nasty stuff.  It is also a huge performance win.
	! We avoid 6 or 7 ASI accesses which would otherwise be required while
	! we set the MMU into/out-off a special no-fault mode, and check the
	! status it reports.  This, along with Viking pipeline performance
	! conscious coding reduces the time taken for user mode faults from
	! around 117 to 58 cycles for an overflow, and from 113 to 57 cycles
	! for an underflow.
	!

vik_wu_poorly_aligned:
	!
	! The stack must either cross a page boundary or be misaligned.
	!
	andcc	%sp, 7, %g0
	bnz	vik_wu_misaligned
	nop

	!
	! The stack crosses a page boundary.  Do restore in two parts.  Probe
	! and load from the first page, then probe and load from the second
	! page.
	!
	! We could just use the old "no-fault" scheme, but by using the probe
	! approach we are less dependent on that particular intricacy of the
	! MMU, or at least we will be if locore.s ever gets cleaned up.
	!
	!	**********************************************************
	!	* THIS CODE IS MORE SUBTLE THAN IT MIGHT AT FIRST APPEAR *
	!	* DO NOT MODIFY IT WITHOUT READING THE EXPLANATION ABOVE *
	!	**********************************************************
	!

	!
	! Probe the first page.
	!
	lda	[%l7]ASI_FLPR, %l4		! probe first page - MP safe!
	and	%l4, PTE_PERMMASK | PTE_ETYPEMASK, %l7
	cmp	%l7, PTE_PERMS(MMU_STD_SRUR) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%l7, PTE_PERMS(MMU_STD_SRWURW) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%l7, PTE_PERMS(MMU_STD_SRXURX) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%l7, PTE_PERMS(MMU_STD_SRWXURWX) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%l7, PTE_PERMS(MMU_STD_SRWUR) | PTE_ETYPE(MMU_ET_PTE)
	bne	vik_wu_fault			! no PTE, or wrong permissions
	nop

	! Good, page is readable.

	!
	! Restore only those values stored on the first page.
	!

	orn	%sp, MMU_STD_PAGESIZE - 1, %i7	! used by the following macro

#define LDD_FIRSTPAGE(OFFSET, REGS)	\
	addcc	%i7, (OFFSET), %g0;	\
	bneg,a	.+8;			\
	ldd	[%sp + (OFFSET)], REGS

	LDD_FIRSTPAGE(0*8, %l0)
	LDD_FIRSTPAGE(1*8, %l2)
	LDD_FIRSTPAGE(2*8, %l4)
	LDD_FIRSTPAGE(3*8, %l6)
	LDD_FIRSTPAGE(4*8, %i0)
	LDD_FIRSTPAGE(5*8, %i2)
	LDD_FIRSTPAGE(6*8, %i4)
	! LDD_FIRSTPAGE(7*8, %i6)

	! The squashing of annulled load/store instructions as used above and
	! below had better occur before they perform a TLB data access.  This
	! is what the documentation seems to indicate, but it would be nice to
	! have something concrete.

	! Need to be careful not to clobber %l0-%i6, from this point on.

	! Before doing the second probe make sure the immediately preceeding
	! TLB access was for a code fetch, and not one of the above ldd's.
	! The branch, even though untaken, ensures we have done a fetch, as
	! opposed to executing out of the prefetch buffer, the save, restore
	! ensure the branch has reached D2, where the fetch is performed,
	! before the next instruction is executed.
	bn	.
	save
	restore

	!
	! Probe the second page.
	!
	andn	%sp, MMU_STD_PAGESIZE - 1, %i7	! first page base address
	sub	%i7, - MMU_STD_PAGESIZE, %i7	! second page base address
	or	%i7, FLP_TYPE(MMU_FLP_ALL), %i7	! address to use for probe
	lda	[%i7]ASI_FLPR, %i6		! probe second page - MP safe!
	and	%i6, PTE_PERMMASK | PTE_ETYPEMASK, %i7
	cmp	%i7, PTE_PERMS(MMU_STD_SRUR) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%i7, PTE_PERMS(MMU_STD_SRWURW) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%i7, PTE_PERMS(MMU_STD_SRXURX) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%i7, PTE_PERMS(MMU_STD_SRWXURWX) | PTE_ETYPE(MMU_ET_PTE)
	bne,a	.+8
	cmp	%i7, PTE_PERMS(MMU_STD_SRWUR) | PTE_ETYPE(MMU_ET_PTE)
	be	0f
	nop

	! Probe failed.
	mov	%i6, %l4			! probe result into %l4
	andn	%sp, MMU_STD_PAGESIZE - 1, %l5	! second page base address into
	sub	%l5, - MMU_STD_PAGESIZE, %l5	! %l5
	ba,a	vik_wu_fault			! no PTE, or wrong permissions

0:
	! Good, page is readable.

	!
	! Restore only those values stored on the second page.
	!

	orn	%sp, MMU_STD_PAGESIZE - 1, %i7	! used by the following macro

#define LDD_SECONDPAGE(OFFSET, REGS)	\
	addcc	%i7, (OFFSET), %g0;	\
	bpos,a	.+8;			\
	ldd	[%sp + (OFFSET)], REGS

	! LDD_SECONDPAGE(0*8, %l0)
	LDD_SECONDPAGE(1*8, %l2)
	LDD_SECONDPAGE(2*8, %l4)
	LDD_SECONDPAGE(3*8, %l6)
	LDD_SECONDPAGE(4*8, %i0)
	LDD_SECONDPAGE(5*8, %i2)
	LDD_SECONDPAGE(6*8, %i4)
	LDD_SECONDPAGE(7*8, %i6)

	ba,a	vik_wu_out			! and exit

vik_wu_fault:
	!
	! Probe returned bad.  No PTE or wrong permissions.  Fake a page fault.
	!
	! Probe result in %l4, faulting page address in %l5.
	!

	! Copy fault status and address, %l4 %l5, back to the trap window.
	mov	%g6, %l6		! save %g6, %g7
	mov	%g7, %l7

	mov	%l4, %g6		! %l4, %l5 into %g6, %g7
	mov	%l5, %g7

	save				! back to trap window
	save

	mov	%g6, %l4		! copy into trap window's %l4, %l5
	mov	%g7, %l5

	restore				! restore %g6, %g7
	restore
	mov	%l6, %g6
	mov	%l7, %g7
	save
	save

	!
	! If the PTE was not found, as opposed to simply having the wrong
	! permissions, the probe will have set bits in the fault status
	! register.
	!
	! I am not sure if following such a probe we need to reset the fault
	! status register to prevent the overwrite bit being turned on by the
	! next page fault.  We will do so to be on the safe side.
	!
	set	RMMU_FAV_REG, %l7
	lda	[%l7]ASI_MOD, %l7	! read MFAR, probably not needed
	set	RMMU_FSR_REG, %l6
	lda	[%l6]ASI_MOD, %l6	! read MFSR

	!
	! Replace %l4 with reason for fault, a faked up MFSR value.
	!

	tst	%l4			! was PTE found?
	bz,a	0f
	mov	MMU_SFSR_FAV | MMU_SFSR_FT_INV, %l4	! fake MFSR - no page

	set	MMU_SFSR_FAV | MMU_SFSR_FT_PRIV, %l4	! fake MFSR - privilege

0:
	!
	! Set up state to handle the fault.
	!
	mov	%l3, %wim			! reinstall old wim
	nop; nop; nop

	CPU_ADDR(%l7, %l6)			! get cpu struct pointer
	ld	[%l7 + CPU_THREAD], %l6		! get thread pointer
	ld	[%l7 + CPU_MPCB], %sp		! setup kernel stack
						! mpcb doubles as stack
	SAVE_GLOBALS(%sp + MINFRAME)		! create fault frame
	SAVE_OUTS(%sp + MINFRAME)
	st	%l0, [%sp + MINFRAME + PSR*4]	! psr
	st	%l1, [%sp + MINFRAME + PC*4]	! pc
	st	%l2, [%sp + MINFRAME + nPC*4]	! npc

	mov	%l6, THREAD_REG			! set global thread pointer

	restore					! back to last user window
	mov	%psr, %g1			! get CWP
	save					! back to trap window
	mov	1, %l7				! UWM = 0x01 << CWP
	sll	%l7, %g1, %l7
	st	%l7, [%sp + MPCB_UWM]		! setup mpcb->mpcb_uwm
	clr	[%sp + MPCB_WBCNT]

	wr	%l0, PSR_ET, %psr		! enable traps
	nop; nop; nop

	!
	! Call fault handler.
	!
	mov	T_WIN_UNDERFLOW, %o0		! fault type
	add	%sp, MINFRAME, %o1		! pointer to fault frame
	mov	%l5, %o2			! fault address, fake MFAR
	mov	%l4, %o3			! fault status, fake MFSR
	mov	S_READ, %o4			! access type
	call	trap				! trap(T_WIN_UNDERFLOW,
						!      rp, addr, be, S_READ)
	nop

	ba,a	_sys_rtt	! use the standard trap handler code to return
				! to the user

vik_wu_misaligned:
	!
	! A user underflow trap has happened with a misaligned %sp.
	!
	! Fake low level memory alignment trap.
	!
	save				! get back to original window
	save
	mov	%l3, %wim		! restore old wim
	nop; nop; nop
	! Have already set up %l0, %l1, %l2 = %psr, %pc, %npc.
	mov	T_ALIGNMENT, %l4	! fake alignment trap
	ba,a	sys_trap

	.align	32
vik_wu_super:
	!
	! Handle supervisor underflow.
	!
	restore
	!-
	RESTORE_WINDOW(%sp)
	!-
	save
	!-
	save
	!-
	cmp	%l1, -1			! return with a rett?
	be	0f
	!-
	mov	%l0, %psr		! (delay slot) reinstall PSR_CC
	!-
	nop				! (psr delay)
	nop				! (psr delay)
	jmp	%l1			! (psr delay) reexecute restore
	!-
	rett	%l2
	!-
0:
	nop				! (psr delay) return without a rett
	jmp	%l2+8			! (psr delay)
	!-
	nop				! (psr delay)
	!-

	SET_SIZE(vik_window_underflow)

#endif	/* lint */
