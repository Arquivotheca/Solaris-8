/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)float.s	1.42	99/04/13 SMI"

#if defined(lint)
#include <sys/types.h>
#include <sys/thread.h>
#endif

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <sys/machpcb.h>
#include <sys/machthread.h>

#if !defined(lint)
#include "assym.h"
#endif	/* lint */

/*
 * Floating point trap handling.
 *
 *	The FPU may not be in the current configuration.
 *	If an fp_disabled trap is generated and the DF bit
 *	in the psr equals 0 (floating point was enabled)
 *	then there is not a FPU in the	configuration
 *	and the global variable fp_exists is cleared.
 *
 *	When a user process is first started via exec,
 *	floating point operations will be disabled by default.
 *	Upon execution of the first floating point instruction,
 *	a fp_disabled trap will be generated; at which point
 *	a check is made to see if the FPU exists, (fp_exists > 0),
 *	if not the instruction is emulated in software, otherwise
 *	a word in the uarea is written signifying use of the
 *	floating point processor so that subsequent context
 *	switches will save and restore the floating point
 *	registers. The trapped instruction will be restarted
 *	and processing will continue as normal.
 *
 *	When a operation occurs that the hardware cannot properly
 *	handle, an unfinshed fp_op exception will be generated.
 *	Software routines in the kernel will be	executed to
 *	simulate proper handling of such conditions.
 *
 *	Exception handling will emulate all instructions
 *	in the floating point address queue.
 *
 *	At context switch time we save the fsr first, which will
 *	wait the processor until all floating point operations
 *	in FQ complete... by the time the kernel gets to the point
 *	of executing the mov %fsr instruction, most FPU operations
 *	should have had time to complete anyway.
 *
 *	NOTE: This code DOES NOT SUPPORT KERNEL (DEVICE DRIVER)
 *		USE OF THE FPU
 */

#if defined(lint)

int fpu_exists = 1;

#else	/* lint */

	.seg	".data"
	.global	fpu_exists
fpu_exists:
	.word	1			! assume FPU exists
fsrholder:
	.word	0			! dummy place to write fsr
	.global fpu_version
	! retain this so that sun4 machines can figure out that
	! they have a FAB4/FAB5 FPU (which will be disabled in
	! 5.0).
fpu_version:
	.word	-1

fpu_probing:
	.word	0

	.seg	".text"
	.align	4

#endif	/* lint */

/*
 * syncfpu(), used to synchronize cpu with the fpu
 */

#if defined(lint)

/* ARGSUSED */
void
syncfpu(void)
{}

#else	/* lint */

	ENTRY_NP(syncfpu)
	set	fpu_probing, %o0
	ld	[%o0], %o1
	tst	%o1
	bnz	1f

	mov	%psr, %o5		! is floating point enabled
	set	PSR_EF, %o4
	btst	%o4, %o5
	bnz,a	1f			! do store if FP enabled
	st	%fsr, [%sp - 4]		! take an exception, if one is pending
					! this stored before %sp, but that's OK
					! because we don't reload it
1:
	retl
	nop
	SET_SIZE(syncfpu)

#endif	/* lint */

/*
 * FPU probe - try a floating point instruction to see if there
 * really is an FPU in the current configuration. Called from
 * autoconf.
 */

#if defined(lint)

/* ARGSUSED */
void
fpu_probe(void)
{}

#else	/* lint */

	ENTRY_NP(fpu_probe)
	set	fpu_probing, %o0
	mov	1, %o1
	st	%o1, [%o0]

	set	PSR_EF, %g1		! enable floating-point
	mov	%psr, %g2		! read psr, save value in %g2
	or	%g1, %g2, %g3		! new psr with fpu enabled
	mov	%g3, %psr		! write psr
	nop;nop				! psr delay...
	sethi	%hi(.zero), %g3
	ld	[%g3 + %lo(.zero)], %fsr ! probe for fpu, maybe trap
	!
	! If there is not an FPU, we will get a fp_disabled trap when
	! we try to load the fsr. This will clear the fpu_exists flag.
	!

	! part of fix for 1041977 (fitoX fix can panic kernel)
	! snarf the FPU version, if it exists
	sethi	%hi(fpu_exists), %g3
	ld	[%g3 + %lo(fpu_exists)], %g3
	mov	7, %g1			! assume no FPU
	tst	%g3
	bz	1f
	sethi	%hi(fpu_version), %g3

	! We know the fpu exists; we are still enabled for it
	sethi	%hi(fsrholder), %g1
	st	%fsr, [%g1 + %lo(fsrholder)]
	ld	[%g1 + %lo(fsrholder)], %g1	! snarf the FSR
#ifndef	FSR_VERS
#define	FSR_VERS	0x000e0000	/* version field */
#define	FSR_VERS_SHIFT	(17)		/* amount to shift version field */
#endif
	set	FSR_VERS, %o0
	and	%g1, %o0, %g1			! get version
	srl	%g1, FSR_VERS_SHIFT, %g1	! and shift it down

1:
	set	fpu_probing, %o0
	st	%g0, [%o0]

	st	%g1, [%g3 + %lo(fpu_version)]
	mov	%g2, %psr		! restore old psr, turn off FPU
	nop				! psr delay ...
	ba	fp_kstat_init		! initialize the fpu_kstat
	nop
	SET_SIZE(fpu_probe)

.zero:	.word	0

#endif	/* lint */

/*
 * fp_enable(fp)
 *	struct fpu *fp;
 *
 * Initialization when there is a hardware fpu.
 * Clear the fsr and initialize registers to NaN (-1)
 * The caller is supposed to update lwp->lwp_regs (the return psr)
 * so when the return to usrerland is made, the fpu is enabled.
 */

#if defined(lint)

/* ARGSUSED */
void
fp_enable(struct fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(fp_enable)
	mov	%psr, %o1		! enable the fpu 
	set	PSR_EF, %o2
	or	%o1, %o2, %o3
	mov	%o3, %psr
	nop;				! psr delay
	mov	-1, %o4			! -1 is NaN
	mov	-1, %o5	
	ld	[%o0 + FPU_FSR], %fsr
	std	%o4, [%o0]		! initialize %f0
	ldd	[%o0], %f0
	ldd	[%o0], %f2
	ldd	[%o0], %f4
	ldd	[%o0], %f6
	ldd	[%o0], %f8
	ldd	[%o0], %f10
	ldd	[%o0], %f12
	ldd	[%o0], %f14
	ldd	[%o0], %f16
	ldd	[%o0], %f18
	ldd	[%o0], %f20
	ldd	[%o0], %f22
	ldd	[%o0], %f24
	ldd	[%o0], %f26
	ldd	[%o0], %f28
	retl
	ldd	[%o0], %f30
	SET_SIZE(fp_enable)

#endif	/* lint */

/*
 * fp_disable()
 *
 * Disable the floating point unit.
 * Assumes high priority so that reading and writing of psr is safe.
 */

#if defined(lint)

/* ARGSUSED */
void
fp_disable(void)
{}

#else	/* lint */

	ENTRY_NP(fp_disable)
	mov	%psr, %o1		! get current psr
	set	PSR_EF, %o2
	andn	%o1, %o2, %o3		! zero the EF bit 
	mov	%o3, %psr		! disable fpu
	nop				! psr delay
	retl
	nop
	SET_SIZE(fp_disable)

#endif	/* lint */

/*
 * fp_save(fp)
 *	struct fpu *fp;
 * Store the floating point registers and disable the floating point unit.
 */

#if defined(lint)

/* ARGSUSED */
void
fp_save(struct fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(fp_save)
	mov	%psr, %o1
	set	PSR_EF, %o2
	or	%o1, %o2, %o3
	mov	%o3, %psr
	nop; nop; nop

	STORE_FPREGS(%o0)
	st	%fsr, [%o0 + FPU_FSR]
	wr	%o3, %o2, %psr			! disable fpu
	nop					! 3 instruction psr delay
	retl
	nop
	SET_SIZE(fp_save)

#endif	/* lint */

/*
 * fp_fksave(fp)
 *	struct fpu *fp;
 *
 * This is like the above routine but leaves the floating point
 * unit enabled, used during fork of processes that use floating point.
 */

#if defined(lint)

/* ARGSUSED */
void
fp_fksave(struct fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(fp_fksave)
	mov	%psr, %o1
	set	PSR_EF, %o2
	or	%o1, %o2, %o3
	mov	%o3, %psr
	nop; nop; nop
	STORE_FPREGS(%o0)
	retl
	st	%fsr, [%o0 + FPU_FSR]
	SET_SIZE(fp_fksave)

#endif	/* lint */

/*
 * fp_restore(fp)
 *	struct fpu *fp;
 */

#if defined(lint)

/* ARGSUSED */
void
fp_restore(struct fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(fp_restore)
	set	PSR_EF, %o2
	mov	%psr, %o1			! enable fpu
	or 	%o1, %o2, %o3			! set ef bit
	mov	%o3, %psr			! no psr delay needed, next 3
						! instrs independent of new psr
	ld	[THREAD_REG + T_LWP], %g3	! lwp pointer
	ld	[THREAD_REG + T_CPU], %o4	! cpu pointer
	st	%g3, [%o4 + CPU_FPOWNER]	! store new cpu_fpowner owner
	ld	[%g3 + LWP_REGS], %o3		! pointer to struct regs
	ld	[%o3], %o4			! get saved psr (sys_rtt's)
	or	%o4, %o2, %o4			! set EF bit in ret psr
	st	%o4, [%o3]			! new psr with fpu enabled
	LOAD_FPREGS(%o0)
	retl
	ld	[%o0 + FPU_FSR], %fsr
	SET_SIZE(fp_restore)

#endif	/* lint */

/*
 * fp_load(fp)
 *	struct fpu *fp;
 */

#if defined(lint)

/* ARGSUSED */
void
fp_load(struct fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(fp_load)
	set	PSR_EF, %o2
	mov	%psr, %o1			! enable fpu
	or 	%o1, %o2, %o3			! set ef bit
	mov	%o3, %psr			! one psr delay needed, next 2
						! instrs independent of new psr
	ld	[THREAD_REG + T_LWP], %g3	! lwp pointer
	ld	[%g3 + LWP_REGS], %o3		! pointer to struct regs
	ld	[%o3], %o4			! get saved psr (sys_rtt's)
	or	%o4, %o2, %o4			! set EF bit in ret psr
	st	%o4, [%o3]			! new psr with fpu enabled
	ld	[THREAD_REG + T_CPU], %o4	! cpu pointer
	st	%g3, [%o4 + CPU_FPOWNER]	! store new cpu_fpowner owner
	LOAD_FPREGS(%o0)
	retl
	ld	[%o0 + FPU_FSR], %fsr
	SET_SIZE(fp_load)

#endif	/* lint */


/*
 * Floating Point Exceptions.
 * handled according to type:
 *	0) no_exception
 *		do nothing
 *	1) IEEE_exception
 *		re-execute the faulty instruction(s) using
 *		software emulation (must do every instruction in FQ)
 *	2) unfinished_fpop
 *		re-execute the faulty instruction(s) using
 *		software emulation (must do every instruction in FQ)
 *	3) unimplemented_fpop
 *		an unimplemented instruction, if it is legal,
 *		will cause emulation of the instruction (and all
 *		other instuctions in the FQ)
 *	4) sequence_error
 *		panic, this should not happen, and if it does it
 *		it is the result of a kernel bug
 *
 * This code assumes the trap preamble has set up the window evironment
 * for execution of kernel code.
 */

#if defined(lint)

/* ARGSUSED */
void
_fp_exception(void)
{}

#else	/* lint */

	ENTRY_NP(_fp_exception)
	ld	[THREAD_REG + T_STACK], %l3	! t_stk doubles as mpcb
	st	%fsr, [%l3 + MPCB_FPU_FSR]	! get floating point status
	ld	[%l3 + MPCB_FPU_FSR], %g1
	set	FSR_FTT, %l6
	and	%g1, %l6, %g2		! mask out trap type
	srl	%g2, FSR_FTT_SHIFT, %l6	! use ftt after we dump queue
dumpfq:
	clr	%g2			! FQ offset
	set	FSR_QNE, %g3		! the queue is not empty bit
	add	%l3, MPCB_FPU_Q, %l4
1:
	ld	[%l3 + MPCB_FPU_FSR], %g1	! test fsr
	btst	%g3, %g1		! is the queue empty?
	bz	2f			! yes, go figure out what to do
	!
	! BUGID: 1038405 [software workaround]
	! For Calvin FPU, if two instructions in FQ, and exception and
	! [%l4+%g2] not in cache, gives wrong answer. We now preload
	! cache, and the original "nop" after "bz" has been replaced by:
	ld	[%l4 + %g2], %g0
	std	%fq, [%l4 + %g2]	! store an entry from FQ
	add	%g2, 8, %g2		! increment offset in FQ
	b	1b
	st	%fsr, [%l3 + MPCB_FPU_FSR]	! get floating point status
	!
	! if we are initializing the fpu for a process via
	! the fp_disabled trap and we get an exception, we can
	! ignore it since it is a junk state from a previous
	! process that didn't get the exception (because it died)
	!
2:

	! XXX - what to do here?
	!set	fp_init, %g1		! exceptions here should be ignored
	!cmp	%g1, %l1
	!be	_sys_rtt	
	!.empty

	!
	! lower priority to the base priority for this cpu, enable traps
	! allow other interrupts while emulating floating point instructions
	!
	andn	%l0, PSR_PIL, %g1	! clear current interrupt level
	ld	[THREAD_REG + T_CPU], %l4	! get CPU's base priority level
	ld	[%l4 + CPU_BASE_SPL], %l4
	or	%g1, %l4, %l4		! merge base priority with psr
	mov	%l4, %psr		! set priority
	wr	%l4, PSR_ET, %psr	! enable traps, set new interrupt level
	nop; nop; nop			! three cycle delay required.
	cmp	%l6, FTT_SEQ		! sanity check for bogus exceptions
	blt,a	fpeok
	clr	%l7			! offset into stored queue entries
	!
	! Sequence error or unknown ftt exception.
	!
	set	.badfpexcpmsg, %o0	! panic
	call	panic
	mov	%l6, %o1		! mov ftt to o1 for stack backtrace

fpeok:
	srl	%g2, 3, %l4		! number of entries stored from fpq
	stb	%l4, [%l3 + MPCB_FPU_QCNT] ! save number of queue entries
	!
	! Clear interrupting trap type in fsr, but preserve cexc.
	! ld to fsr.ftt is ignored.  Next successful fpop clears fsr.ftt.
	!
	fmovs	%f0, %f0		! do easy harmless fpop
	ld	[%l3 + MPCB_FPU_FSR], %fsr ! restore cexc
	st	%fsr, [%l3 + MPCB_FPU_FSR] ! now minus ftt
	!
	! Update the fp trap statistics
	!
	call	fp_kstat_update		! fp_kstat_update(ftt)
	mov	%l6, %o0
	!
	! Run the floating point queue
	!
	ld	[THREAD_REG + T_STACK], %o0 ! calculate user struct regs ptr
	call	fp_runq			! fp_runq(&regs)
	add	%o0, MINFRAME, %o0

	!
	! the fsr's condition codes could have been modified
	! by emulation so reload the fsr to update them with
	! the simulator results
	!
	b	_sys_rtt
	ld	[%l3 + MPCB_FPU_FSR], %fsr ! ld new fsr to set condition codes
	SET_SIZE(_fp_exception)

#endif	/* lint */

/*
 * void _fp_read_pfreg(pf, n)
 *	FPU_REGS_TYPE	*pf;	Old freg value.
 *	unsigned	n;	Want to read register n
 *
 * {
 *	*pf = %f[n];
 * }
 *
 * void
 * _fp_write_pfreg(pf, n)
 *	FPU_REGS_TYPE	*pf;	New freg value.
 *	unsigned	n;	Want to write register n.
 *
 * {
 *	%f[n] = *pf;
 * }
 */

#if defined(lint)

/* ARGSUSED */
void
_fp_read_pfreg(FPU_REGS_TYPE *pf, unsigned n)
{}

/* ARGSUSED */
void
_fp_write_pfreg(FPU_REGS_TYPE *pf, unsigned n)
{}

#else	/* lint */

	ENTRY_NP(_fp_read_pfreg)
	sll	%o1, 3, %o1		! Table entries are 8 bytes each.
	set	.stable, %g1		! g1 gets base of table.
	jmp	%g1 + %o1		! Jump into table
	nop				! Can't follow CTI by CTI.

	ENTRY_NP(_fp_write_pfreg)
	sll	%o1, 3, %o1		! Table entries are 8 bytes each.
	set	.ltable, %g1		! g1 gets base of table.
	jmp	%g1 + %o1		! Jump into table
	nop				! Can't follow CTI by CTI.

#define STOREFP(n) jmp %o7+8 ; st %f/**/n, [%o0]

.stable:
	STOREFP(0)
	STOREFP(1)
	STOREFP(2)
	STOREFP(3)
	STOREFP(4)
	STOREFP(5)
	STOREFP(6)
	STOREFP(7)
	STOREFP(8)
	STOREFP(9)
	STOREFP(10)
	STOREFP(11)
	STOREFP(12)
	STOREFP(13)
	STOREFP(14)
	STOREFP(15)
	STOREFP(16)
	STOREFP(17)
	STOREFP(18)
	STOREFP(19)
	STOREFP(20)
	STOREFP(21)
	STOREFP(22)
	STOREFP(23)
	STOREFP(24)
	STOREFP(25)
	STOREFP(26)
	STOREFP(27)
	STOREFP(28)
	STOREFP(29)
	STOREFP(30)
	STOREFP(31)

#define LOADFP(n) jmp %o7+8 ; ld [%o0],%f/**/n

.ltable:
	LOADFP(0)
	LOADFP(1)
	LOADFP(2)
	LOADFP(3)
	LOADFP(4)
	LOADFP(5)
	LOADFP(6)
	LOADFP(7)
	LOADFP(8)
	LOADFP(9)
	LOADFP(10)
	LOADFP(11)
	LOADFP(12)
	LOADFP(13)
	LOADFP(14)
	LOADFP(15)
	LOADFP(16)
	LOADFP(17)
	LOADFP(18)
	LOADFP(19)
	LOADFP(20)
	LOADFP(21)
	LOADFP(22)
	LOADFP(23)
	LOADFP(24)
	LOADFP(25)
	LOADFP(26)
	LOADFP(27)
	LOADFP(28)
	LOADFP(29)
	LOADFP(30)
	LOADFP(31)
	SET_SIZE(_fp_read_pfreg)
	SET_SIZE(_fp_write_pfreg)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
_fp_write_pfsr(FPU_FSR_TYPE *fsr)
{}

#else	/* lint */

	ENTRY_NP(_fp_write_pfsr)
	retl
	ld	[%o0], %fsr
	SET_SIZE(_fp_write_pfsr)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
_fp_read_pfsr(FPU_FSR_TYPE *fsr)
{}

#else	/* lint */

	ENTRY_NP(_fp_read_pfsr)
	retl
	st	%fsr, [%o0]
	SET_SIZE(_fp_read_pfsr)

.badfpexcpmsg:
	.asciz	"unexpected floating point exception %x\n"

	.align 4

#endif	/* lint */
