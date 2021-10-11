/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)savestate.s	1.3	99/10/04 SMI"


/*
 * State save/restore code for fusion kadb.  Kernel debugger entry starts
 * execution here.
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/debug/debug.h>
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/machtrap.h>
#include <sys/trap.h>
#include <sys/stack.h>
#include <sys/privregs.h>
#include <sys/mmu.h>
#include <sys/fsr.h>
#include <sys/machthread.h>
#include <sys/intreg.h>
#include <sys/spitasi.h>
#include <sys/spitregs.h>
#include <sys/xc_impl.h>

#include "cpusave.h"
#include "assym.s"

#define KADB_PSTATE	(PSTATE_PEF | PSTATE_PRIV)


#if !defined(lint)

	.seg	".text"

#endif	/* !defined(lint) */

/*
 *  enter_debugger -
 *    Already saved:
 *      + cpu_state in this cpu's cpusave entry
 *      + %g1-%g7
 *      + %tba, %tstate, %tpc, %tnpc, %tt
 *      + MMU primary context
 *
 *    Parameters:
 *      + %g1:  this cpu's regs entry in cpusave
 *      + %g3:  0 if we're a slave; non-zero for a master
 *
 *    Assumptions:
 *      + %tl is currently 0
 *      + %pstate is set to KADB_PSTATE
 *      + we will resume to %tl 0
 *      + if %g3 is non-zero (i.e. a master), %o6 points to a valid
 *        stack, with a little space for us
 */
#if !defined(lint)


#define	VA_WP_REG	0x38		/* VA watchpoint register */
#define	PA_WP_REG	0x40		/* PA watchpoint register */

	ENTRY(enter_debugger)

	/*
	 *  Disable our watchpoints (if any).  There's no need to
	 *  save the watchpoints; they're in the breakpoint list.
	 */
	ldxa	[%g0]ASI_LSU, %g4
	sethi	%hi(0xfffff000), %g5
	sllx	%g5, 9, %g5
	andn	%g4, %g5, %g4		! clear wp bits in LSU
	stxa	%g4, [%g0]ASI_LSU
	membar	#Sync


	/*
	 *  Save a mess of privileged registers.
	 *
	 *  Some of these need new values while we're in kadb.
	 */
	rdpr	%pil, %g4
	stx	%g4, [%g1 + R_PIL]
	wrpr	%g0, 14, %pil

	rd	%y, %g4
	stx	%g4, [%g1 + R_Y]

	rdpr	%cwp, %g4
	stx	%g4, [%g1 + R_CWP]
	rdpr	%otherwin, %g4
	stx	%g4, [%g1 + R_OTHERWIN]
	rdpr	%cleanwin, %g4
	stx	%g4, [%g1 + R_CLEANWIN]
	rdpr	%cansave, %g4
	stx	%g4, [%g1 + R_CANSAVE]
	rdpr	%canrestore, %g4
	stx	%g4, [%g1 + R_CANRESTORE]
	rdpr	%wstate, %g4
	stx	%g4, [%g1 + R_WSTATE]


	/*
	 *  Save all our register windows into the cpusave area.
	 */
	set	nwindows, %g5
	lduw	[%g5], %g4
	sub	%g4, 1, %g4
	wrpr	%g4, %cleanwin

	sub	%g4, 1, %g4
	wrpr	%g0, %otherwin
	wrpr	%g4, %cansave
	wrpr	%g0, %canrestore
	wrpr	%g0, %cwp
	add	%g1, R_WINDOW, %g5

1:
	SAVE_V9WINDOW(%g5)
	save
	saved
	rdpr	%cwp, %g4
	brnz	%g4, 1b
	add	%g5, R_WINDOW_INCR, %g5


	/*
	 *  Check to see if we need to save the floating-point
	 *  state.  Note that we enable FP regardless, since printf
	 *  uses it.
	 */
	set	CPU_FPREGS-CPU_REGS, %g5
	add	%g5, %g1, %g2		! %g2 = &cpusave[this_cpu].fpregs
	rd	%fprs, %g4
	stx	%g4, [%g2 + FPU_FPRS]
	btst	FPRS_FEF, %g4		! fp enabled?
	bz,pt	%icc, 1f		! nope, drive on...
	wr	%g0, FPRS_FEF, %fprs	! but enable fp anyway

	STORE_FPREGS(%g2)
	stx	%fsr, [%g2 + FPU_FSR]
1:

	/*
	 *  Our state is now fully saved.  Decide whether we're a
	 *  master or a slave.
	 */
	brz	%g3, slaveloop
	nop

master:
	/*
	 *  Get ourselves a stack, and go get commands from the
	 *  user.
	 *
	 *  %sp points to the stack we had on entry.  We assume
	 *  all memory from %sp up (adjusted by the bias, if any) is
	 *  sacrosanct, and don't touch it (this is conservative;
	 *  %sp should point to a register save area which we can
	 *  reuse).  We also assume there's a little extra space for
	 *  us below the line, and proceed to use it.
	 */
	ldx	[%g1 + R_CWP], %g4
	wrpr	%g4, %cwp		! go back to our starting window

	btst	1, %sp				! 64-bit stack?
	bnz	%xcc, 1f			! no, skip stack fudging
	mov	0, %fp				! stack trace stops here

	and	%sp, -STACK_ALIGN64, %sp	! align stack to 16 bytes
	sub	%sp, V9BIAS64, %sp		! adjust for 64-bit bias
1:
	sub	%sp, SA(MINFRAME64), %sp	! get our stack space

	CPU_INDEX(%o0, %g4)		! pass our cpuid to kadb_master_entry

	mov	%g1, %l1
	call	kadb_master_entry
	mov	%g2, %l2

	mov	%l1, %g1
	ba	resume
	mov	%l2, %g2

slaveloop:
	/*
	 *  We're a slave.  We may have entered from userland, in
	 *  which case our stack is no good.  In any event, there's
	 *  no point in us trying to become master, so we just wait
	 *  here.
	 */
	wrpr	%g0, KADB_PSTATE|PSTATE_IE, %pstate

	set	kadblock, %g5
	ldub	[%g5], %g4
1:
	brnz,a	%g4, 1b
	ldub	[%g5], %g4

	wrpr	%g0, KADB_PSTATE, %pstate

resume:
	/*
	 *  We're back.  Let's retrace our steps back home.
	 *
	 *  Start off with the FPU.
	 */
	ldx	[%g2 + FPU_FPRS], %g4
	btst	FPRS_FEF, %g4		! was fp enabled?
	bz,pt	%icc, 1f		! nope, drive on...
	wr	%g4, %fprs		! restore %fprs regardless

	LOAD_FPREGS(%g2)
	ldx	[%g2 + FPU_FSR], %fsr
1:

	/*
	 *  Now restore all our register windows
	 */
	set	nwindows, %g5
	lduw	[%g5], %g4
	sub	%g4, 1, %g4
	wrpr	%g4, %cleanwin

	sub	%g4, 1, %g4
	wrpr	%g0, %otherwin
	wrpr	%g4, %cansave
	wrpr	%g0, %canrestore
	wrpr	%g0, %cwp
	add	%g1, R_WINDOW, %g5

1:
	RESTORE_V9WINDOW(%g5)
	save
	saved
	rdpr	%cwp, %g4
	brnz	%g4, 1b
	add	%g5, R_WINDOW_INCR, %g5


	/*
	 *  Restore various privileged registers
	 */
	ldx	[%g1 + R_CWP], %g4
	wrpr	%g4, %cwp
	ldx	[%g1 + R_OTHERWIN], %g4
	wrpr	%g4, %otherwin
	ldx	[%g1 + R_CLEANWIN], %g4
	wrpr	%g4, %cleanwin
	ldx	[%g1 + R_CANSAVE], %g4
	wrpr	%g4, %cansave
	ldx	[%g1 + R_CANRESTORE], %g4
	wrpr	%g4, %canrestore
	ldx	[%g1 + R_WSTATE], %g4
	wrpr	%g4, %wstate

	ldx	[%g1 + R_Y], %g4
	wr	%g4, %y

	ldx	[%g1 + R_PIL], %g4
	wrpr	%g4, %pil


	/*
	 *  Restore the registers our caller saved
	 *
	 *  After %tba, the register restores require TL > 0 to
	 *  execute properly.
	 */
	ldx	[%g1 + R_TBA], %g4
	wrpr	%g4, %tba

	wrpr	%g0, 1, %tl

	ldx	[%g1 + R_TSTATE], %g4
	wrpr	%g4, %tstate
	ldx	[%g1 + R_PC], %g4
	wrpr	%g4, %tpc
	ldx	[%g1 + R_NPC], %g4
	wrpr	%g4, %tnpc

	/*
	 *  We need to know whether we got here because of the trap
	 *  at .enterkadb, or because of some sort of breakpoint or
	 *  interrupt.  If our trap type corresponds to ST_KADB_TRAP,
	 *  we assume that the instruction at %tpc is a "ta" that we
	 *  don't want to re-execute.
	 *
	 *  Note that we test for the trap here, and then hold %xcc
	 *  for testing until the very end.
	 */
	ldx	[%g1 + R_TT], %g4
	wrpr	%g4, %tt
	cmp	%g4, ST_KADB_TRAP|T_SOFTWARE_TRAP


	/*
	 *  Restore the MMU primary context.  Keep %asi set to
	 *  ASI_DMMU for later.
	 */
	wr	%g0, ASI_DMMU, %asi
	ldx	[%g1 + R_MMU_PCONTEXT], %g4
	stxa	%g4, [MMU_PCONTEXT]%asi
	membar	#Sync

	/*
	 *  Enable watchpoints, if any are set.
	 */
	set	wp_lsucr, %g5
	ldx	[%g5], %g6		! hold this for later
	brz	%g6, 1f			! if clear, no watchpoints
	nop

	set	wp_vaddress, %g5
	ldx	[%g5], %g4
	sllx	%g4, 20, %g4		! sign extend lower 44 bits
	srax	%g4, 20, %g4
	stxa	%g4, [VA_WP_REG]%asi	! asi is still ASI_DMMU

	set	wp_paddress, %g5
	ldx	[%g5], %g4
	sllx	%g4, 23, %g4		! zero extend lower 41 bits
	srlx	%g4, 23, %g4
	stxa	%g4, [PA_WP_REG]%asi	! asi is still ASI_DMMU

	sethi	%hi(0xfffff000), %g5
	sllx	%g5, 9, %g5
	ldxa	[%g0]ASI_LSU, %g4
	andn	%g4, %g5, %g4		! twiddle LSU
	or	%g6, %g4, %g4		! mix in new value
	stxa	%g4, [%g0]ASI_LSU
	membar	#Sync

	/*
	 *  Restore our globals last.
	 */
1:
					! %g1 is later (duh!)
	ldx	[%g1 + R_G2], %g2
	ldx	[%g1 + R_G3], %g3
	ldx	[%g1 + R_G4], %g4
	ldx	[%g1 + R_G5], %g5
	ldx	[%g1 + R_G6], %g6
	ldx	[%g1 + R_G7], %g7
	be	%xcc, 1f
	ldx	[%g1 + R_G1], %g1

	retry
1:
	done

	SET_SIZE(enter_debugger)

#endif


/*
 *  Kadb's breakpoint/L1-A trap handler.  The mechanism differs from
 *  that used on other architectures, since the PROM owns the trap
 *  table while the debugger is running.
 *
 *  The kernel's trap handler is patched with the PROM's 0x17d and
 *  0x17e trap handlers at kernel startup time. Breakpoints and L1-A
 *  traps therefore go into the PROM, which bounces to here (because
 *  of the defer word we downloaded at kadb startup time).  On
 *  entry, all registers (except %pc and %npc) match their values at
 *  breakpoint time.  We are called at TL=0, and must only use the
 *  normal globals until everything is saved (we can use the globals
 *  because they were saved in the defer word).
 *
 *  Outline:
 *	- calculate our cpusave and mpfpregs entry addresses
 *	- use the PROM's %tba
 *	- copy state saved by the defer word
 *	- goto enter_debugger (as a master) to do the rest of the work
 */
#ifdef lint
int
trap(void) { return (0); }
#else

	ENTRY(trap)

	wrpr	%g0, KADB_PSTATE, %pstate

	/*
	 *  If nofault is set, we were already in the debugger when
	 *  the trap occurred.  Call fault() to handle it.  Can't
	 *  save our state, because we'd overwrite existing state in
	 *  cpusave which we usually will need.
	 */
	set	nofault, %g5
	ldx	[%g5], %g5
	brz	%g5, 1f			! fault entry?
	nop

	call	reload_prom_callback
	nop

	set	saved_tt, %g5
	lduw	[%g5], %o0
	set	saved_pc, %g5
	ldx	[%g5], %o1
	set	saved_npc, %g5
	call	fault			! fault(tt, pc, npc)
	ldx	[%g5], %o2		! we ain't supposed to return
	/*NOTREACHED*/

	/*
	 *  The kadb_callback forth word saved a mess of registers
	 *  for us.  Copy them into our save area.
	 */
1:
	CPU_INDEX(%g4, %g1)		! get CPU id

	set	CPUSAVESIZE, %g5
	mulx	%g4, %g5, %g5
	set	cpusave, %g1
	add	%g1, %g5, %g1		! %g1 = &cpusave[this_cpuid]

	set	CPU_STATUS_MASTER, %g4
	st	%g4, [%g1 + CPU_STATUS]	! announce our arrival


	add	%g1, CPU_REGS, %g1	! %g1 = &cpusave[this_cpuid].regs

	rdpr	%tba, %g4
	stx	%g4, [%g1 + R_TBA]
	set	mon_tba, %g5
	ldx	[%g5], %g5
	wrpr	%g5, %tba

	set	saved_tstate, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_TSTATE]
	set	saved_pc, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_PC]
	set	saved_npc, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_NPC]
	set	saved_tt, %g5
	lduw	[%g5], %g4
	stx	%g4, [%g1 + R_TT]

	/*
	 *  Copy the saved global registers.
	 */
	set	saved_g1, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_G1]
	set	saved_g2, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_G2]
	set	saved_g3, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_G3]
	set	saved_g4, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_G4]
	set	saved_g5, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_G5]
	set	saved_g6, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_G6]
	set	saved_g7, %g5
	ldx	[%g5], %g4
	stx	%g4, [%g1 + R_G7]

	set	MMU_PCONTEXT, %g5
	ldxa	[%g5]ASI_DMMU, %g4
	stx	%g4, [%g1 + R_MMU_PCONTEXT]
	stxa	%g0, [%g5]ASI_DMMU	! guarantee kernel address space
	membar	#Sync

	ba	enter_debugger
	mov	1, %g3			! tell enter_debugger we're a master

	SET_SIZE(trap)

#endif	/* lint */


/*
 *  We get here via the kernel after getting a cross call to idle
 *  from kadb_send_mondo().  The kernel takes care of setting our
 *  parameters.  Note that we enter straight from the trap handler;
 *  we're at TL>0, and we're still using the interrupt vector global
 *  registers.
 *
 *  Outline:
 *	- save state needed by enter_debugger
 *	- switch from the I globals to the regular globals
 *	- set TL to 0
 */

#ifdef lint
/*ARGSUSED*/
void
save_cpu_state(void *savereg) {}
#else	/* lint */

	ENTRY(save_cpu_state)

	! Register Usage:
	!	g1: save area ptr

	set	CPU_STATUS_SLAVE, %g4
	st	%g4, [%g1 + CPU_STATUS]	! announce our arrival

	add	%g1, CPU_REGS, %g1	! get address of our regs ptr

	/*
	 *  Save privileged trap state registers indexed by %tl
	 */
	rdpr	%tba, %g4
	stx	%g4, [%g1 + R_TBA]

	rdpr	%tstate, %g4
	stx	%g4, [%g1 + R_TSTATE]
	rdpr	%tpc, %g4
	stx	%g4, [%g1 + R_PC]
	rdpr	%tnpc, %g4
	stx	%g4, [%g1 + R_NPC]
	rdpr	%tt, %g4
	stx	%g4, [%g1 + R_TT]

	/*
	 *  We may have come from userland, so make sure that our
	 *  MMU primary context is saved before we switch to TL 0.
	 */
	set	MMU_PCONTEXT, %g5
	ldxa	[%g5]ASI_DMMU, %g4
	stx	%g4, [%g1 + R_MMU_PCONTEXT]
	stxa	%g0, [%g5]ASI_DMMU	! guarantee kernel address space
	membar	#Sync

	/*
	 *  Save the global registers.  We have to do a little
	 *  juggling in order to hang on to the %g1 and %g2 in the
	 *  interrupt globals set.
	 */
	mov	%o1, %g5				! save this
	mov	%o2, %g6				! and this
	mov	%g1, %o1
	mov	%g2, %o2

	wrpr	%g0, KADB_PSTATE, %pstate		! go to reg. globals
	stx	%g1, [%o1 + R_G1]			! and save 'em all
	stx	%g2, [%o1 + R_G2]
	stx	%g3, [%o1 + R_G3]
	stx	%g4, [%o1 + R_G4]
	stx	%g5, [%o1 + R_G5]
	stx	%g6, [%o1 + R_G6]
	stx	%g7, [%o1 + R_G7]

	mov	%o1, %g1				! copy from I globals
	mov	%o2, %g2				! copy from I globals

	wrpr	%g0, KADB_PSTATE|PSTATE_IG, %pstate	! back to I globals
	mov	%g5, %o1				! restore
	mov	%g6, %o2				! restore

	/*
	 *  Ready to save the rest of our state.  Go to
	 *  enter_debugger to handle the rest of the work.
	 */
	wrpr	%g0, KADB_PSTATE, %pstate		! back to reg. globals
	wrpr	%g0, 0, %tl

	ba	enter_debugger
	mov	0, %g3			! tell enter_debugger we're a slave

	SET_SIZE(save_cpu_state)

#endif	/* lint */


/*
 *  kadb lock management functions.  Simple mutex entry, test, and
 *  exit.  Note that this is not quite a spin lock.  These functions
 *  are used by kadb_master_entry() to decide which CPU becomes the
 *  current master.
 *
 *  obtain_lock -
 *    Grab the lock, if available.  Return true if we get it, false
 *    if we don't.
 *
 *  lock_held -
 *    Return true if the lock is held, false otherwise.
 *
 *  release_lock -
 *    Unconditionally release the lock.
 */

#if defined(lint)
int
obtain_lock(lock_t *l)
{ return *l = 0xff; }

int
lock_held(lock_t *l)
{ return *l; }

void
release_lock(lock_t *l)
{ *l = 0; }
#else

	ENTRY(obtain_lock)

	ldstub	[%o0], %g1
	mov	1, %o0
	retl
	movrnz	%g1, 0, %o0

	SET_SIZE(obtain_lock)


	ENTRY(lock_held)

	retl
	ldub	[%o0], %o0

	SET_SIZE(lock_held)


	ENTRY(release_lock)

	retl
	stb	%g0, [%o0]

	SET_SIZE(release_lock)

#endif


/*
 * Send a mondo interrupt to the specified CPU. 
 *
 *	%o0: CPU number (UPA port id)
 *	%o1: PC of mondo handler
 *	%o2-%o3: arguments
 */

#if defined(lint)
/* ARGSUSED */
int
kadb_send_mondo(u_int cpuid, func_t func, u_int arg1, u_int arg2)
{ return 0; }
#else

	ENTRY(kadb_send_mondo)

	! IDSR should not be busy at the moment

	ldxa	[%g0]ASI_INTR_DISPATCH_STATUS, %g1
	btst	IDSR_BUSY, %g1
	bnz,pn	%xcc, 1f
	mov	ASI_INTR_DISPATCH, %asi

	stxa	%o1, [IDDR_0]%asi		! pass func to target
	stxa	%o2, [IDDR_1]%asi		! pass arg1 to target
	stxa	%o3, [IDDR_2]%asi		! pass arg2 to target
	membar	#Sync


	! construct the interrupt dispatch command register in %g1

	sll	%o0, IDCR_PID_SHIFT, %g1	! IDCR<18:14> = upa port id
	or	%g1, IDCR_OFFSET, %g1		! IDCR<13:0> = 0x70
	stxa	%g0, [%g1]ASI_INTR_DISPATCH	! dispatch the command

						! Spitfire SF_ERRATA_54
	membar	#Sync				!  store must occur before load
	mov	0x20, %g3			!  UDBH Control Register Read
	ldxa	[%g3]ASI_SDB_INTR_R, %g0	! Spitfire SF_ERRATA_54 end

	membar	#Sync

	set	XC_BUSY_COUNT-1, %o2		! set BUSY debugging counter

2:
	ldxa	[%g0]ASI_INTR_DISPATCH_STATUS, %g5

	brnz,a,pt   %g5, 3f
	btst	    IDSR_NACK, %g5		! test for NACK

	retl
	set	    1, %o0			! successful return

3:
	bnz,a,pn    %xcc, 4f
	sethi	    %hi(Cpudelay), %g2

	brnz,a,pt   %o2, 2b			! retry if not past limit
	dec	    %o2				! decrement BUSY counter

1:
	sethi	%hi(_send_mondo_busy), %o0
	call	prom_panic
	or	%o0, %lo(_send_mondo_busy), %o0
	/*NOTREACHED*/

4:
	! nacked so pause for 1 usec_delay
	ld	[%g2 + %lo(Cpudelay)], %g2	! microsecond countdown counter

6:
	brnz	%g2, 6b				! microsecond countdown loop
	dec	%g2				! 2 instructions in loop

	retl
	set	0, %o0				! failure return


	SET_SIZE(kadb_send_mondo)

_send_mondo_busy:
	.asciz	"kadb_send_mondo: BUSY"
	.align	4

#endif	/* lint */
