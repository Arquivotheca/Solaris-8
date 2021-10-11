/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)locore.s	1.118	99/03/18 SMI"

#if defined(lint)
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#endif	/* lint */

#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/clock.h>
#include <sys/cpu.h>
#include <sys/intreg.h>
#include <sys/eeprom.h>
#include <sys/debug/debug.h>
#include <sys/mmu.h>
#include <sys/pcb.h>
#include <sys/pte.h>
#include <sys/privregs.h>
#include <sys/machpcb.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/scb.h>
#include <sys/machparam.h>
#include <sys/machthread.h>
#include <sys/machlock.h>
#include <sys/x_call.h>
#include <sys/mutex_impl.h>
#include <sys/fdreg.h>
#include <sys/vis.h>
#include <sys/cyclic.h>
#include <sys/traptrace.h>
#include <sys/panic.h>

#if defined(lint)

#include <sys/thread.h>
#include <sys/time.h>

#else	/* lint */

#include "assym.h"

!
! REGOFF must add up to allow double word access to r_tstate.
! PCB_WBUF must also be aligned.
!
#if (REGOFF & 7) != 0
#error "struct regs not aligned"
#endif

/*
 * Absolute external symbols.
 * On the sun4u we put the panic buffer in the third and fourth pages.
 * We set things up so that the first 2 pages of KERNELBASE is illegal
 * to act as a redzone during copyin/copyout type operations. One of
 * the reasons the panic buffer is allocated in low memory to
 * prevent being overwritten during booting operations (besides
 * the fact that it is small enough to share pages with others).
 */

	.seg	".data"
	.global	panicbuf, t0stack

PROM	= 0xFFE00000			! address of prom virtual area
#ifdef __sparcv9
panicbuf = SYSBASE32 + PAGESIZE		! address of panic buffer
#else
panicbuf = SYSBASE + PAGESIZE		! address of panic buffer
#endif

	.type	panicbuf, #object
	.size	panicbuf, PANICBUFSIZE

/*
 * Absolute external symbol - intr_vector.
 *
 * With new bus structures supporting a larger number of interrupt
 * numbers, the interrupt vector table, intr_vector[], has been
 * moved out of kernel nucleus and allocated after panicbuf.
 */
	.global intr_vector

#ifdef __sparcv9
intr_vector = SYSBASE32 + PAGESIZE + PANICBUFSIZE ! address of interrupt table
#else
intr_vector = SYSBASE + PAGESIZE + PANICBUFSIZE	! address of interrupt table
#endif

	.type	intr_vector, #object
	.size	intr_vector, MAXIVNUM * INTR_VECTOR_SIZE

/*
 * The thread 0 stack. This must be the first thing in the data
 * segment (other than an sccs string) so that we don't stomp
 * on anything important if the stack overflows. We get a
 * red zone below this stack for free when the kernel text is
 * write protected.
 */

	.align	16
t0stack:
	.skip	T0STKSZ			! thread 0 stack
t0stacktop:

	.global t0
	.align	PTR24_ALIGN		! alignment for mutex.
	.type	t0, #object
t0:
	.skip	THREAD_SIZE		! thread 0
	.size	t0, THREAD_SIZE

#ifdef	TRAPTRACE
	.global	trap_trace_ctl
	.global	trap_tr0
	.global trap_trace_bufsize
	.global trap_trace_msg
	.global	trap_freeze

	.align	4
trap_trace_bufsize:
	.word	TRAP_TSIZE		! default trap buffer size
trap_freeze:
	.word	0

	.align	64
trap_trace_ctl:
	.skip	NCPU * TRAPTR_SIZE	! NCPU control headers

	.align	16
trap_tr0:
	.skip	TRAP_TSIZE		! one buffer for the boot cpu

	.align	4
trap_trace_msg:
	.asciz "bad traptrace environment (pstate or ctl struct)"

	.align	4
#endif	/* TRAPTRACE */

#ifdef	TRACE

TR_intr_start:
	.asciz "interrupt_start:level %d";
	.align 4;
TR_intr_end:
	.asciz "interrupt_end";
	.align 4;
TR_intr_exit:
	.asciz "intr_thread_exit";
	.align 4;

#endif	/* TRACE */

/*
 * save area for registers in case of a ptl1_panic,
 * and stack for the tl1 panicing cpu, if user panic.
 * Let's hope at least nucleus data tte is still available.
 */
	.seg	".data"
	.global	ptl1_panic_cpu
	.global	ptl1_panic_tr
	.global	ptl1_dat
	.global	ptl1_stk
	.global	ptl1_stk_top

	.align	4
kpanictl1msg:
	.asciz  "Kernel panic at trap level %x";

upanictl1msg:
	.asciz  "User panic at trap level %x";

	.align	8
ptl1_panic_cpu:
	.word	0
ptl1_panic_tr:
	.word	0

	.align	8
ptl1_dat:
	.skip	PTL1_SIZE * PTL1_MAXTL

	.align	16
ptl1_stk:				! one stack for the panic cpu
	.skip	PTL1_SSIZE
ptl1_stk_top:

	.align 4
	.seg	".text"

#ifdef	NOPROM
	.global availmem
availmem:
	.word	0
#endif	/* NOPROM */

	.align	8
_local_p1275cis:
	.nword	0

#endif	/* lint */

#if defined(lint)

void
_start(void)
{}

#else /* lint */

	.seg	".data"

	.global nwindows, nwin_minus_one, winmask
nwindows:
	.word   8
nwin_minus_one:
	.word   7
winmask:
	.word	8

	.global	afsrbuf
afsrbuf:
	.word	0,0,0,0

/*
 * System initialization
 *
 * Our contract with the boot prom specifies that the MMU is on and the
 * first 16 meg of memory is mapped with a level-1 pte.  We are called
 * with p1275cis ptr in %o0 and dvec in %o1; we start execution directly
 * from physical memory, so we need to get up into our proper addresses
 * quickly: all code before we do this must be position independent.
 *
 * NB: Above is not true for boot/stick kernel, the only thing mapped is
 * the text+data+bss. The kernel is loaded directly into KERNELBASE.
 *
 * 	entry, the romvec pointer (romp) is the first argument;
 * 	  i.e., %o0.
 * 	the debug vector (dvec) pointer is the second argument (%o1)
 * 	the bootops vector is in the third argument (%o2)
 *
 * Our tasks are:
 * 	save parameters
 * 	construct mappings for KERNELBASE (not needed for boot/stick kernel)
 * 	hop up into high memory           (not needed for boot/stick kernel)
 * 	initialize stack pointer
 * 	initialize trap base register
 * 	initialize window invalid mask
 * 	initialize psr (with traps enabled)
 * 	figure out all the module type stuff
 * 	tear down the 1-1 mappings
 * 	dive into main()
 */
	ENTRY_NP(_start)
	!
	! Stash away our arguments in memory.
	!
	sethi	%hi(_local_p1275cis), %g1
	stn	%o0, [%g1 + %lo(_local_p1275cis)]
	sethi	%hi(dvec), %g1
	stn	%o1, [%g1 + %lo(dvec)]
	sethi	%hi(bootops), %g1
	stn	%o2, [%g1 + %lo(bootops)]

	!
	! Initialize CPU state registers
	!
	wrpr	%g0, PSTATE_KERN, %pstate
	wr	%g0, %g0, %fprs
#ifdef __sparcv9
	CLEARTICKNPT(locore, %g1, %g2, %g3)	! allow user rdtick
#endif

	!
	! Get maxwin from %ver
	!
	rdpr	%ver, %g1
	and	%g1, VER_MAXWIN, %g1

	!
	! Stuff some memory cells related to numbers of windows.
	!
	sethi	%hi(nwin_minus_one), %g2
	st	%g1, [%g2 + %lo(nwin_minus_one)]
	inc	%g1
	sethi	%hi(nwindows), %g2
	st	%g1, [%g2 + %lo(nwindows)]
	dec	%g1
	mov	-2, %g2
	sll	%g2, %g1, %g2
	sethi	%hi(winmask), %g4
	st	%g2, [%g4 + %lo(winmask)]

	! If dvec is non-zero, we are running under the debugger.
	set	dvec, %g1
	ldn	[%g1], %g1
	tst	%g1
	bz	1f
	nop

	! Copy obp's 0x7e trap entry to kadb_bpt. This is used for
	! kadb's enter (L1-A) and breakpoint traps.
	rdpr	%tba, %g1
	set	T_SOFTWARE_TRAP | ST_KADB_TRAP, %g2
	sll	%g2, 5, %g2
	or	%g1, %g2, %g1
	set	kadb_bpt, %g2
	ldx	[%g1], %g3
	stx	%g3, [%g2]
	flush	%g2
	ldx	[%g1 + 8], %g3
	stx	%g3, [%g2 + 8]
	flush	%g2 + 8
	ldx	[%g1 + 16], %g3
	stx	%g3, [%g2 + 16]
	flush	%g2 + 16
	ldx	[%g1 + 24], %g3
	stx	%g3, [%g2 + 24]
	flush	%g2 + 24
1:

	!
	! copy obp's breakpoint trap entry to obp_bpt
	!
	rdpr	%tba, %g1
	set	T_SOFTWARE_TRAP | ST_MON_BREAKPOINT, %g2
	sll	%g2, 5, %g2
	or	%g1, %g2, %g1
	set	obp_bpt, %g2
	ldx	[%g1], %g3
	stx	%g3, [%g2]
	flush	%g2
	ldx	[%g1 + 8], %g3
	stx	%g3, [%g2 + 8]
	flush	%g2 + 8
	ldx	[%g1 + 16], %g3
	stx	%g3, [%g2 + 16]
	flush	%g2 + 16
	ldx	[%g1 + 24], %g3
	stx	%g3, [%g2 + 24]
	flush	%g2 + 24

	!
	! Initialize thread 0's stack.
	!
	set	t0stacktop, %g1		! setup kernel stack pointer
	sub	%g1, SA(V9FPUSIZE+GSR_SIZE), %g2
	and	%g2, 0x3f, %g3
	sub	%g2, %g3, %o2
	sub	%o2, SA(MPCBSIZE) + STACK_BIAS, %sp

	!
	! Initialize global thread register.
	!
	set	t0, THREAD_REG

	!
	! Fill in enough of the cpu structure so that
	! the wbuf management code works. Make sure the
	! boot cpu is inserted in cpu[] based on cpuid.
	!
	CPU_INDEX(%g2, %g1)
	sll	%g2, CPTRSHIFT, %g2		! convert cpuid to cpu[] offset
	set	cpu0, %o0			! &cpu0
	set	cpu, %g1			! &cpu[]
	stn	%o0, [%g1 + %g2]		! cpu[cpuid] = &cpu0

	stn	%o0, [THREAD_REG + T_CPU]	! threadp()->t_cpu = cpu[cpuid]
	stn	THREAD_REG, [%o0 + CPU_THREAD]	! cpu[cpuid]->cpu_thread = threadp()


	!  We do NOT need to bzero our BSS...boot has already done it for us.
	!  Just need to reference edata so that we don't break /dev/ksyms
	set	edata, %g0

	!
	! Call mlsetup with address of prototype user registers.
	! and p1275cis pointers.

	sethi	%hi(_local_p1275cis), %o1
	ldn	[%o1 + %lo(_local_p1275cis)], %o1
	call	mlsetup
	add	%sp, REGOFF + STACK_BIAS, %o0

#if defined(__sparcv9) && (REGOFF != MPCB_REGS)
#error "hole in struct machpcb between frame and regs?"
#endif

	!
	! Now call main.  We will return as process 1 (init).
	!
	call	main
	nop

	!
	! Main should never return.
	!
	set	.mainretmsg, %o0
	call	panic
	nop
	SET_SIZE(_start)

.mainretmsg:
	.asciz	"main returned"
	.align	4

#endif	/* lint */


/*
 * Generic system trap handler.
 *
 * Some kernel trap handlers save themselves from buying a window by
 * borrowing some of sys_trap's unused locals. %l0 thru %l3 may be used
 * for this purpose, as user_rtt and priv_rtt do not depend on them.
 * %l4 thru %l7 should NOT be used this way.
 *
 * Entry Conditions:
 * 	%pstate		am:0 priv:1 ie:0
 * 			globals are either ag or ig (not mg!)
 *
 * Register Inputs:
 * 	%g1		pc of trap handler
 * 	%g2, %g3	args for handler
 * 	%g4		desired %pil (-1 means current %pil)
 * 	%g5, %g6	destroyed
 * 	%g7		saved
 *
 * Register Usage:
 * 	%l0, %l1	temps
 * 	%l3		saved %g1
 * 	%l6		curthread for user traps, %pil for priv traps
 * 	%l7		regs
 *
 * Called function prototype variants:
 *
 *	func(struct regs *rp);
 * 	func(struct regs *rp, uintptr_t arg1 [%g2], uintptr_t arg2 [%g3])
 *	func(struct regs *rp, uintptr_t arg1 [%g2],
 *	    uint32_t arg2 [%g3.l], uint32_t arg3 [%g3.h])
 *	func(struct regs *rp, uint32_t arg1 [%g2.l],
 *	    uint32_t arg2 [%g3.l], uint32_t arg3 [%g3.h], uint32_t [%g2.h])
 */

#if defined(lint)

void
sys_trap(void)
{}

#else	/* lint */

	ENTRY_NP(sys_trap)
	!
	! force tl=1, update %cwp, branch to correct handler
	!
	wrpr	%g0, 1, %tl
	rdpr	%tstate, %g5
	btst	TSTATE_PRIV, %g5
	and	%g5, TSTATE_CWP, %g6
	bnz,pn	%xcc, priv_trap
	wrpr	%g0, %g6, %cwp
	.global	user_trap
user_trap:
	!
	! user trap
	!
	! make all windows clean for kernel
	! buy a window using the current thread's stack
	!
	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g5
	wrpr	%g0, %g5, %cleanwin
	CPU_ADDR(%g5, %g6)
	ldn	[%g5 + CPU_THREAD], %g5
	ldn	[%g5 + T_STACK], %g6
#if STACK_BIAS != 0
	sub	%g6, STACK_BIAS, %g6
#endif
	save	%g6, 0, %sp
	!
	! set window registers so that current windows are "other" windows
	!
	rdpr	%canrestore, %l0
	rdpr	%wstate, %l1
	wrpr	%g0, 0, %canrestore
	sllx	%l1, WSTATE_SHIFT, %l1
#ifdef __sparcv9
	wrpr	%l1, WSTATE_K64, %wstate
#else
	wrpr	%l1, WSTATE_K32, %wstate
#endif
	wrpr	%g0, %l0, %otherwin
	!
	! set pcontext to run kernel
	!
	mov	KCONTEXT, %l0
	mov	MMU_PCONTEXT, %l1
	sethi	%hi(FLUSH_ADDR), %l2
	stxa	%l0, [%l1]ASI_DMMU
	flush	%l2			! flush required by immu
	set	utl0, %g6		! bounce to utl0
have_win:
	SYSTRAP_TRACE(%o1, %o2, %o3)
	!
	! at this point we have a new window we can play in,
	! and %g6 is the label we want done to bounce to
	!
	! save needed current globals
	!
	mov	%g1, %l3	! pc
	mov	%g2, %o1	! arg #1
	mov	%g3, %o2	! arg #2
	srlx	%g3, 32, %o3	! pseudo arg #3
	srlx	%g2, 32, %o4	! pseudo arg #4
	mov	%g5, %l6	! curthread if user trap, %pil if priv trap
	!
	! save trap state on stack
	!
	add	%sp, REGOFF + STACK_BIAS, %l7
	rdpr	%tpc, %l0
	rdpr	%tnpc, %l1
	rdpr	%tstate, %l2
	stn	%l0, [%l7 + PC_OFF]
	stn	%l1, [%l7 + nPC_OFF]
	stx	%l2, [%l7 + TSTATE_OFF]
	!
	! setup pil
	!
	brlz,pt		%g4, 1f
	nop
#ifdef DEBUG
	!
	! ASSERT(%g4 >= %pil).
	!
	rdpr	%pil, %l0
	cmp	%g4, %l0
	bge,pt	%xcc, 0f
	nop				! yes, nop; to avoid anull
	set	bad_g4_called, %l3
	mov	1, %o1
	st	%o1, [%l3]
	set	bad_g4, %l3		! pc
	set	sys_trap_wrong_pil, %o1	! arg #1
	mov	%g4, %o2		! arg #2
	ba	1f			! stay at the current %pil
	mov	%l0, %o3		! arg #3
0:
#endif /* DEBUG */
	wrpr		%g0, %g4, %pil
1:
	!
	! set trap regs to execute in kernel at %g6
	! done resumes execution there
	!
	wrpr	%g0, %g6, %tnpc
	rdpr	%cwp, %l0
	set	TSTATE_KERN, %l1
	wrpr	%l1, %l0, %tstate
	done
	/* NOTREACHED */

	.global	prom_trap
prom_trap:
	!
	! prom trap switches the stack to 32-bit
	! if we took a trap from a 64-bit window
	! Then buys a window on the current stack.
	!
	save	%sp, -SA64(REGOFF + REGSIZE), %sp
					/* 32 bit frame, 64 bit sized */
#ifndef __sparcv9
	add	%sp, V9BIAS64, %sp	/* back to a 32-bit frame */
#endif
	set	ptl0, %g6
	ba,pt	%xcc, have_win
	nop

	.global	priv_trap
priv_trap:
	!
	! kernel trap
	! buy a window on the current stack
	!
#if !defined(__sparcv9)
	srl	%sp, 0, %sp
	btst	1, %sp
	bnz	prom_trap
	  rdpr	%pil, %g5
#else
	! is the trap PC in the range allocated to Open Firmware?
	rdpr	%tpc, %g5
	set	OFW_END_ADDR, %g6
	cmp	%g5, %g6
	bgt,a,pn %xcc, 1f
	  rdpr	%pil, %g5
	set	OFW_START_ADDR, %g6
	cmp	%g5, %g6
	bge,pn	%xcc, prom_trap
	  rdpr	%pil, %g5
1:
#endif
	set	ktl0, %g6
	ba,pt	%xcc, have_win
	save	%sp, -SA(REGOFF + REGSIZE), %sp
	SET_SIZE(sys_trap)

	ENTRY_NP(utl0)
	SAVE_GLOBALS(%l7)
	SAVE_OUTS(%l7)
	mov	%l6, THREAD_REG
	wrpr	%g0, PSTATE_KERN, %pstate	! enable ints
	set	user_rtt - 8, %o7
	jmp	%l3
	mov	%l7, %o0
	SET_SIZE(utl0)

	ENTRY_NP(ktl0)
	SAVE_GLOBALS(%l7)
	SAVE_OUTS(%l7)				! for the call bug workaround
	wrpr	%g0, PSTATE_KERN, %pstate	! enable ints
	set	priv_rtt - 8, %o7
	jmp	%l3
	mov	%l7, %o0
	SET_SIZE(ktl0)

	ENTRY_NP(ptl0)
	SAVE_GLOBALS(%l7)
	SAVE_OUTS(%l7)
	CPU_ADDR(%g5, %g6)
	ldn	[%g5 + CPU_THREAD], THREAD_REG
	wrpr	%g0, PSTATE_KERN, %pstate	! enable ints
	set	prom_rtt - 8, %o7
	jmp	%l3
	mov	%l7, %o0
	SET_SIZE(ptl0)

#endif	/* lint */

#ifndef lint

#ifdef DEBUG
	.seg	".data"
	.align	4

	.global bad_g4_called
bad_g4_called:
	.word	0

sys_trap_wrong_pil:
	.asciz	"sys_trap: %g4(%d) is lower than %pil(%d)"
	.align	4
	.seg	".text"

	ENTRY_NP(bad_g4)
	mov	%o1, %o0
	mov	%o2, %o1
	call	panic
	mov	%o3, %o2
	SET_SIZE(bad_g4)
#endif /* DEBUG */
#endif /* lint */

/*
 * sys_tl1_panic can be called by traps at tl1 which
 * really want to panic, but need the rearrangement of
 * the args as provided by this wrapper routine.
 */
#if defined(lint)

void
sys_tl1_panic(void)
{}

#else	/* lint */
	ENTRY_NP(sys_tl1_panic)
	mov	%o1, %o0
	mov	%o2, %o1
	call	panic
	mov	%o3, %o2
	SET_SIZE(sys_tl1_panic)
#endif /* lint */

/*
 * Return from sys_trap routines.
 *
 * user_rtt returns from traps taken in user mode
 * priv_rtt return from traps taken in privileged mode
 */

#if defined(lint)

void
_sys_rtt(void)
{}

#else	/* lint */

	ENTRY_NP(user_rtt)
	!
	! Register inputs
	!	%l7 - regs
	!
	! disable interrupts and check for ASTs and wbuf restores
	! keep cpu_base_spl in %l4 and THREAD_REG in %l6 (needed
	! in wbuf.s when globals have already been restored).
	!
	wrpr	%g0, PIL_MAX, %pil
	ldn	[THREAD_REG + T_CPU], %l0
	ld	[%l0 + CPU_BASE_SPL], %l4

	ldub	[THREAD_REG + T_ASTFLAG], %l2
	brz,pt	%l2, 1f
	ld	[%sp + STACK_BIAS + MPCB_WBCNT], %l3
	!
	! call trap to do ast processing
	!
	wrpr	%g0, %l4, %pil			! pil = cpu_base_spl
	mov	%l7, %o0
	call	trap
	  mov	T_AST, %o2
	ba,a,pt	%xcc, user_rtt
1:
	brz,pt	%l3, 2f
	mov	THREAD_REG, %l6
	!
	! call restore_wbuf to push wbuf windows to stack
	!
	wrpr	%g0, %l4, %pil			! pil = cpu_base_spl
	mov	%l7, %o0
	call	trap
	  mov	T_FLUSH_PCB, %o2
	ba,a,pt	%xcc, user_rtt
2:
#ifdef TRAPTRACE
	TRACE_RTT(TT_SYS_RTT_USER, %l0, %l1, %l2, %l3)
#endif /* TRAPTRACE */
	ld	[%sp + STACK_BIAS + MPCB_WSTATE], %l3	! get wstate

	!
	! restore user globals and outs
	! switch to alternate globals, saving THREAD_REG in %l6
	!
	rdpr	%pstate, %l1
	wrpr	%l1, PSTATE_IE, %pstate
	RESTORE_GLOBALS(%l7)
	wrpr	%l1, PSTATE_IE | PSTATE_AG, %pstate
	mov	%sp, %g6	! remember the mpcb pointer in %g6
	RESTORE_OUTS(%l7)
	!
	! set %pil from cpu_base_spl
	!
	wrpr	%g0, %l4, %pil
	!
	! raise tl (now using nucleus context)
	! set pcontext to scontext for user execution
	!
	wrpr	%g0, 1, %tl
	mov	MMU_SCONTEXT, %g1
	ldxa	[%g1]ASI_DMMU, %g2
	mov	MMU_PCONTEXT, %g1
	sethi	%hi(FLUSH_ADDR), %g3
	stxa	%g2, [%g1]ASI_DMMU
	flush	%g3				! flush required by immu
	!
	! setup trap regs
	!
	ldn	[%l7 + PC_OFF], %g1
	ldn	[%l7 + nPC_OFF], %g2
	ldx	[%l7 + TSTATE_OFF], %l0
	andn	%l0, TSTATE_CWP, %g7
	wrpr	%g1, %tpc
	wrpr	%g2, %tnpc
	!
	! switch "other" windows back to "normal" windows and
	! restore to window we originally trapped in
	!
	rdpr	%otherwin, %g1
	wrpr	%g0, 0, %otherwin
	add	%l3, WSTATE_CLEAN_OFFSET, %l3	! convert to "clean" wstate
	wrpr	%g0, %l3, %wstate
	wrpr	%g0, %g1, %canrestore
	!
	! First attempt to restore from the watchpoint saved register window
	tst	%g1
	bne,a	1f
	  clrn	[%g6 + STACK_BIAS + MPCB_RSP0]
	tst	%fp
	be,a	1f
	  clrn	[%g6 + STACK_BIAS + MPCB_RSP0]
	! test for user return window in pcb
	ldn	[%g6 + STACK_BIAS + MPCB_RSP0], %g1
	cmp	%fp, %g1
	bne	1f
	  clrn	[%g6 + STACK_BIAS + MPCB_RSP0]
	restored
	restore
	! restore from user return window
#ifdef	__sparcv9
	RESTORE_V9WINDOW(%g6 + STACK_BIAS + MPCB_RWIN0)
#else
	RESTORE_V8WINDOW(%g6 + STACK_BIAS + MPCB_RWIN0)
#endif
	!
	! Attempt to restore from the scond watchpoint saved register window
	tst	%fp
	be,a	2f
	  clrn	[%g6 + STACK_BIAS + MPCB_RSP1]
	ldn	[%g6 + STACK_BIAS + MPCB_RSP1], %g1
	cmp	%fp, %g1
	bne	2f
	  clrn	[%g6 + STACK_BIAS + MPCB_RSP1]
	restored
	restore
#ifdef	__sparcv9
	RESTORE_V9WINDOW(%g6 + STACK_BIAS + MPCB_RWIN1)
#else
	RESTORE_V8WINDOW(%g6 + STACK_BIAS + MPCB_RWIN1)
#endif
	save
	b,a	2f
1:
	restore
2:
	!
	! set %cleanwin to %canrestore
	! set %tstate to the correct %cwp
	! retry resumes user execution
	!
	rdpr	%canrestore, %g1
	wrpr	%g0, %g1, %cleanwin
	rdpr	%cwp, %g1
	wrpr	%g1, %g7, %tstate
	retry
	/* NOTREACHED */
	SET_SIZE(user_rtt)

	ENTRY_NP(prom_rtt)
#ifdef TRAPTRACE
	TRACE_RTT(TT_SYS_RTT_PROM, %l0, %l1, %l2, %l3)
#endif /* TRAPTRACE */
	ba,pt	%xcc, common_rtt
	mov	THREAD_REG, %l0
	SET_SIZE(prom_rtt)

	ENTRY_NP(priv_rtt)
#ifdef TRAPTRACE
	TRACE_RTT(TT_SYS_RTT_PRIV, %l0, %l1, %l2, %l3)
#endif /* TRAPTRACE */
	!
	! Register inputs
	!	%l7 - regs
	!	%l6 - trap %pil
	!
	! Check for a kernel preemption request
	!
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	brz,pt	%l0, 1f
	nop

	!
	! Attempt to preempt
	!
	ldstub	[THREAD_REG + T_PREEMPT_LK], %l0	! load preempt lock
	brnz,pn	%l0, 1f			! can't call kpreempt if this thread is
	nop				!   already in it...

	call	kpreempt
	mov	%l6, %o0		! pass original interrupt level

	stub	%g0, [THREAD_REG + T_PREEMPT_LK]	! nuke the lock	

	rdpr	%pil, %o0		! compare old pil level
	cmp	%l6, %o0		!   with current pil level
	movg	%xcc, %o0, %l6		! if current is lower, drop old pil
1:
	!
	! If we interrupted the mutex_exit() critical region we must reset
	! the PC and nPC back to the beginning to prevent missed wakeups.
	! See the comments in mutex_exit() for details.
	!
	ldn	[%l7 + PC_OFF], %l0
	set	mutex_exit_critical_start, %l1
	sub	%l0, %l1, %l0
	cmp	%l0, mutex_exit_critical_size
	bgeu,pt	%xcc, common_rtt
	mov	THREAD_REG, %l0
	stn	%l1, [%l7 + PC_OFF]	! restart mutex_exit()
	add	%l1, 4, %l1
	stn	%l1, [%l7 + nPC_OFF]

	ALTENTRY(common_rtt)
	!
	! switch to alternate globals
	! restore globals and outs
	!
	rdpr	%pstate, %l1
	wrpr	%l1, PSTATE_IE, %pstate
	RESTORE_GLOBALS(%l7)
	wrpr	%l1, PSTATE_IE | PSTATE_AG, %pstate
	RESTORE_OUTS(%l7)
	!
	! set %pil from max(old pil, cpu_base_spl)
	!
	ldn	[%l0 + T_CPU], %l0
	ld	[%l0 + CPU_BASE_SPL], %l0
	cmp	%l6, %l0
	movg	%xcc, %l6, %l0
	wrpr	%g0, %l0, %pil
	!
	! raise tl
	! setup trap regs
	! restore to window we originally trapped in
	!
	wrpr	%g0, 1, %tl
	ldn	[%l7 + PC_OFF], %g1
	ldn	[%l7 + nPC_OFF], %g2
	ldx	[%l7 + TSTATE_OFF], %l0
	andn	%l0, TSTATE_CWP, %g7
	wrpr	%g1, %tpc
	wrpr	%g2, %tnpc
	restore
	!
	! set %tstate to the correct %cwp
	! retry resumes prom execution
	!
	rdpr	%cwp, %g1
	wrpr	%g1, %g7, %tstate
	retry
	/* NOTREACHED */
	SET_SIZE(common_rtt)
	SET_SIZE(priv_rtt)

#endif /* lint */

/*
 * Turn on or off bits in the auxiliary i/o register.
 *
 * set_auxioreg(bit, flag)
 *	int bit;		bit mask in aux i/o reg
 *	int flag;		0 = off, otherwise on
 *
 * This is intrinsicly ugly but is used by the floppy driver.  It is also
 * used to turn on/off the led.
 */

#if defined(lint)

/* ARGSUSED */
void
set_auxioreg(int bit, int flag)
{}

#else	/* lint */

	.seg	".data"
	.align	4
auxio_panic:
	.asciz	"set_auxioreg: interrupts already disabled on entry"
	.align	4
	.seg	".text"

	ENTRY_NP(set_auxioreg)
	/*
	 * o0 = bit mask
	 * o1 = flag: 0 = off, otherwise on
	 *
	 * disable interrupts while updating auxioreg
	 */
	rdpr	%pstate, %o2
#ifdef	DEBUG
	andcc	%o2, PSTATE_IE, %g0	/* if interrupts already */
	bnz,a,pt %icc, 1f		/* disabled, panic */
	  nop
	sethi	%hi(auxio_panic), %o0
	call	panic
	  or	%o0, %lo(auxio_panic), %o0
1:
#endif /* DEBUG */
	wrpr	%o2, PSTATE_IE, %pstate		/* disable interrupts */
	sethi	%hi(v_auxio_addr), %o3
	ldn	[%o3 + %lo(v_auxio_addr)], %o4
	ldub	[%o4], %g1			/* read aux i/o register */
	tst	%o1
	bnz,a	2f
	 bset	%o0, %g1		/* on */
	bclr	%o0, %g1		/* off */
2:
	or	%g1, AUX_MBO, %g1	/* Must Be Ones */
	stb	%g1, [%o4]		/* write aux i/o register */
	retl
	 wrpr	%g0, %o2, %pstate	/* enable interrupt */
	SET_SIZE(set_auxioreg)

#endif	/* lint */

/*
 * Flush all windows to memory, except for the one we entered in.
 * We do this by doing NWINDOW-2 saves then the same number of restores.
 * This leaves the WIM immediately before window entered in.
 * This is used for context switching.
 */

#if defined(lint)

void
flush_windows(void)
{}

#else	/* lint */

	ENTRY_NP(flush_windows)
	retl
	flushw
	SET_SIZE(flush_windows)

#endif	/* lint */

#if defined(lint)

void
debug_flush_windows(void)
{}

#else	/* lint */

	ENTRY_NP(debug_flush_windows)
	set	nwindows, %g1
	ld	[%g1], %g1
	mov	%g1, %g2

1:
	save	%sp, -WINDOWSIZE, %sp
	brnz	%g2, 1b
	dec	%g2

	mov	%g1, %g2
2:
	restore
	brnz	%g2, 2b
	dec	%g2

	retl
	nop

	SET_SIZE(debug_flush_windows)

#endif	/* lint */

/*
 * flush user windows to memory.
 */

#if defined(lint)

void
flush_user_windows(void)
{}

#else	/* lint */

	ENTRY_NP(flush_user_windows)
	rdpr	%otherwin, %g1
	brz	%g1, 3f
	clr	%g2
1:
	save	%sp, -WINDOWSIZE, %sp
	rdpr	%otherwin, %g1
	brnz	%g1, 1b
	add	%g2, 1, %g2
2:
	sub	%g2, 1, %g2		! restore back to orig window
	brnz	%g2, 2b
	restore
3:
	retl
	nop
	SET_SIZE(flush_user_windows)

#endif	/* lint */

/*
 * Throw out any user windows in the register file.
 * Used by setregs (exec) to clean out old user.
 * Used by sigcleanup to remove extraneous windows when returning from a
 * signal.
 */

#if defined(lint)

void
trash_user_windows(void)
{}

#else	/* lint */

	ENTRY_NP(trash_user_windows)
	rdpr	%otherwin, %g1
	brz	%g1, 3f			! no user windows?
	ldn	[THREAD_REG + T_STACK], %g5

	!
	! There are old user windows in the register file. We disable ints
	! and increment cansave so that we don't overflow on these windows.
	! Also, this sets up a nice underflow when first returning to the
	! new user.
	!
	rdpr	%pstate, %g2
	wrpr	%g2, PSTATE_IE, %pstate
	rdpr	%cansave, %g3
	add	%g3, %g1, %g3
	wrpr	%g0, 0, %otherwin
	wrpr	%g0, %g3, %cansave
	wrpr	%g0, %g2, %pstate
3:
	retl
 	clr     [%g5 + MPCB_WBCNT]       ! zero window buffer cnt
	SET_SIZE(trash_user_windows)


#endif	/* lint */

/*
 * Setup g7 via the CPU data structure.
 */
#if defined(lint)

struct scb *
set_tbr(struct scb *s)
{ return (s); }

#else	/* lint */

	ENTRY_NP(set_tbr)
	retl
	ta	72		! no tbr, stop simulation
	SET_SIZE(set_tbr)

#endif	/* lint */

/*
 * void
 * reestablish_curthread(void)
 *    - reestablishes the invariant that THREAD_REG contains
 *      the same value as the cpu struct for this cpu (implicit from
 *      where we're running). This is needed for OBP callback routines.
 *	The CPU_ADDR macro figures out the cpuid by reading hardware registers.
 */

#if defined(lint)

void
reestablish_curthread(void)
{}

#else	/* lint */

	ENTRY_NP(reestablish_curthread)

	CPU_ADDR(%o0, %o1)
	retl
	ldn	[%o0 + CPU_THREAD], THREAD_REG
	SET_SIZE(reestablish_curthread)


#endif	/* lint */

/*
 * Return the current THREAD pointer.
 * This is also available as an inline function.
 */
#if defined(lint)

kthread_id_t
threadp(void)
{ return ((kthread_id_t)0); }

trapvec kadb_tcode;
trapvec trap_kadb_tcode;
trapvec trap_monbpt_tcode;

#else	/* lint */

	ENTRY_NP(threadp)
	retl
	mov	THREAD_REG, %o0
	SET_SIZE(threadp)


/*
 * Glue code for traps that should take us to the monitor/kadb if they
 * occur in kernel mode, but that the kernel should handle if they occur
 * in user mode.
 */
        .global trap_monbpt_tcode, trap_kadb_tcode
	.global	mon_breakpoint_vec
 
 /* tcode to replace trap vectors if kadb steals them away */
trap_monbpt_tcode:
	mov	%psr, %l0
	sethi	%hi(trap_mon), %l4
	jmp	%l4 + %lo(trap_mon)
	  mov	0xff, %l4
	SET_SIZE(trap_monbpt_tcode)

trap_kadb_tcode:
	mov	%psr, %l0
	sethi	%hi(trap_kadb), %l4
	jmp	%l4 + %lo(trap_kadb)
	  mov	0xfe, %l4
	SET_SIZE(trap_kadb_tcode)

/*
 * This code assumes that:
 * 1. the monitor uses trap ff to breakpoint us
 * 2. kadb steals both ff and fe when we call scbsync()
 * 3. kadb uses the same tcode for both ff and fe.
 *
 * Note: the ".align 8" is needed so that the code that copies
 * the vectors at system boot time can use ldd and std
 */
	.align	8
trap_mon:
	! btst	PSR_PS, %l0		! test pS
	bz	sys_trap		! user-mode, treat as bad trap
	  nop
	mov	%l0, %psr		! restore psr
	nop				! psr delay
	b	mon_breakpoint_vec
	  nop				! psr delay, too!

	.align	8		! MON_BREAKPOINT_VEC MUST BE DOUBLE ALIGNED.
mon_breakpoint_vec:
	nop				! gets overlaid
	sethi	%hi(sys_trap), %l3
	jmp	%l3 + %lo(sys_trap)
	  mov	0xff, %l4

trap_kadb:
	! btst	PSR_PS, %l0		! test pS
	bz	sys_trap		! user-mode, treat as bad trap
	  nop
	mov	%l0, %psr		! restore psr
	nop				! psr delay
	b	kadb_tcode
	  nop				! psr delay, too!

	.align	8		! KADB_BREAKPOINT_VEC MUST BE DOUBLE ALIGNED.
	.global kadb_tcode
kadb_tcode:
	nop				! gets overlaid
	sethi	%hi(sys_trap), %l3
	jmp	%l3 + %lo(sys_trap)
	  mov	0xfe, %l4

/*
 * Stub to get into the monitor.
 */
	.global	call_into_monitor_prom
call_into_monitor_prom:
	retl
	t	0x7f
	SET_SIZE(call_into_monitor_prom)

#endif	/* lint */


/*
 * The interface for a 32-bit client program that takes over the TBA
 * calling the 64-bit romvec OBP.
 */

#if defined(lint)

/* ARGSUSED */
int
client_handler(void *cif_handler, void *arg_array)
{ return 0; }

#else	/* lint */

	ENTRY(client_handler)
	save	%sp, -SA64(MINFRAME64), %sp	! 32 bit frame, 64 bit sized
	sethi	%hi(tba_taken_over), %l2
	ld	[%l2+%lo(tba_taken_over)], %l3
	brz	%l3, 1f				! is the tba_taken_over = 1 ?
	rdpr	%wstate, %l5			! save %wstate
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate
#ifdef __sparcv9
1:	mov	%i1, %o0
#else
1:	srl	%i0, 0, %i0			! zero extend handler addr.
	srl	%i1, 0, %o0			! zero extend first argument.
	set	1f, %o1				! zero extend pc
	jmp	%o1
	  srl	%sp, 0, %sp			! zero extend sp
#endif
1:	rdpr	%pstate, %l4			! Get the present pstate value
	andn	%l4, PSTATE_AM, %l6
	wrpr	%l6, 0, %pstate			! Set PSTATE_AM = 0
	jmpl	%i0, %o7			! Call cif handler
#ifdef __sparcv9
	  nop
#else
	  sub	%sp, V9BIAS64, %sp		! delay; Now a 64 bit frame
	add	%sp, V9BIAS64, %sp		! back to a 32-bit frame
#endif
	wrpr	%l4, 0, %pstate			! restore pstate
	brz	%l3, 1f				! is the tba_taken_over = 1
	  nop
	wrpr	%g0, %l5, %wstate		! restore wstate
1:	ret					! Return result ...
	restore	%o0, %g0, %o0			! delay; result in %o0
	SET_SIZE(client_handler)

#endif	/* lint */

/*
 * The IEEE 1275-1994 callback handler for a 64-bit SPARC V9 PROM calling
 * a 32 bit client program. The PROM calls us with a 64 bit stack and a
 * pointer to a client interface argument array in %o0.  The called code
 * returns 0 if the call succeeded (i.e. the service name exists) or -1
 * if the call failed. NOTE: All addresses are in the range 0..2^^32-1
 *
 * This code is called as trusted subroutine of the firmware, and is
 * called with %tba pointing to the boot firmware's trap table.  All of
 * the prom's window handlers are mixed mode handlers.
 */

#if defined(lint)

int
callback_handler(cell_t *arg_array)
{
	extern int vx_handler(cell_t *arg_array);

	return (vx_handler(arg_array));
}

#else	/* lint */

	ENTRY_NP(callback_handler)
	!
	! We assume we are called with a 64 bit stack with PSTATE_AM clear
	!
	save	%sp, -SA64(MINFRAME64), %sp	! 64 bit save
	rdpr	%wstate, %l5			! save %wstate
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate
#ifndef __sparcv9
	add	%sp, V9BIAS64, %sp		! switch to a 32 bit stack
#endif
	rdpr	%pstate, %l0			! save %pstate
#ifndef __sparcv9
	wrpr	%l0, PSTATE_AM, %pstate		! Set PSTATE_AM
#endif

	call	vx_handler			! vx_handler(void **arg_array)
	  mov	%i0, %o0			! delay; argument array
	sra	%o0, 0, %i0			! sign extend result

#ifndef __sparcv9
	set	1f, %l2				! zero extend %pc
	jmp	%l2
	  srl	%sp, 0, %sp			! delay; zero extend %sp
#endif
1:	wrpr	%g0, %l0, %pstate		! restore %pstate
	wrpr	%g0, %l5, %wstate		! restore %wstate

	ret					! return result in %o0
	restore					! back to a 64 bit stack
	SET_SIZE(callback_handler)

#endif	/* lint */


#if defined(lint)
/*
 * These need to be defined somewhere to lint and there is no "hicore.s"...
 */
char etext[1], end[1];
#endif	/* lint*/

#ifdef TRAPTRACE

#if defined (lint)

void
win_trace(void)
{}

#else /* lint */
	ENTRY_NP(win_trace)
	TRACE_WINTRAP
	jmp	%l0 + 8
	nop
	SET_SIZE(win_trace)

#endif /* lint */

#endif /* TRAPTRACE */

#if defined (lint)

/* ARGSUSED */
void
ptl1_panic(u_int reason)
{}

#else /* lint */

/*
 * tl1 (or greater) panic routine, %g1 has reason why
 */
	ENTRY_NP(ptl1_panic)
	/* see if we are the panic_cpu, based on (cpu index+1) */
	CPU_INDEX(%g3, %g2)
	inc	%g3
	set	ptl1_panic_cpu, %g2
	ld	[%g2], %g4
	cmp	%g3, %g4
	be,a,pn %icc, obp_bpt
	  nop
	casa	[%g2]ASI_N, %g0, %g3
	cmp	%g0, %g3
	be,a,pt	%icc, 2f
	  nop

	/* remove self from cpu_ready_set */
	set	cpu_ready_set, %g4
0:
	CPU_INDEX(%g3, %g2)

	/* make sure cpu is in the ready set */
	CPU_INDEXTOSET(%g4, %g3, %g2)
	ld	[%g4], %g2
	btst	%g3, %g2
	bz,a,pn %icc, 1f
	  nop
	xor	%g3, %g2, %g3
	casa	[%g4]ASI_N, %g2, %g3
	cmp	%g2, %g3
	bne,pn	%icc, 0b
	  nop
1:
	/* spin forever if not panic_cpu */
	ba,pt	%xcc, 1b
	  nop
2:
	/* make a trace record for each nested trap level */
	set	ptl1_panic_tr, %g3
	/* %g1 has reason we're dieing */
	st	%g1, [%g3]
	set	ptl1_dat, %g3
3:
	rdpr	%tl, %g4
	rdpr	%tt, %g2
	stuh	%g2, [%g3 + PTL1_TT]
	stuh	%g4, [%g3 + PTL1_TL]
#ifdef	TRAPTRACE
	TRACE_PTR(%g5, %g6)
	stna	%g1, [%g5 + TRAP_ENT_TR]%asi
	stha	%g4, [%g5 + TRAP_ENT_TL]%asi
	stha	%g2, [%g5 + TRAP_ENT_TT]%asi
#endif	/* TRAPTRACE */
	rdpr	%tstate, %g1
	GET_NATIVE_TIME(%g6)
	rdpr	%tpc, %g2
	rdpr	%tnpc, %g7
	stx	%g1, [%g3 + PTL1_TSTATE]
	stx	%g6, [%g3 + PTL1_TICK]
	stx	%g2, [%g3 + PTL1_TPC]
	stx	%g7, [%g3 + PTL1_TNPC]
#ifdef	TRAPTRACE
	/* could be different than native time above */
	GET_TRACE_TICK(%g6)
	stna	%g2, [%g5 + TRAP_ENT_TPC]%asi
	stxa	%g1, [%g5 + TRAP_ENT_TSTATE]%asi
	stxa	%g6, [%g5 + TRAP_ENT_TICK]%asi
	TRACE_NEXT(%g5, %g6, %g7)
#endif	/* TRAPTRACE */
	cmp	%g4, 1
	be,a,pt	%icc, 4f
	  nop
	dec	%g4
	add	%g3, PTL1_SIZE, %g3
	ba,pt	%icc, 3b
	  wrpr	%g4, %tl
4:
	/* switch from mmu globals */
	rdpr	%pstate, %g4
	andn	%g4, PSTATE_MG, %g4
	or	%g4, PSTATE_AG, %g4
	wrpr	%g4, %g0, %pstate
#ifdef	TRAPTRACE
	set	trap_freeze, %g4
	or	%g0, 1, %g1
	st	%g1, [%g4]
#endif	/* TRAPTRACE */
	/* raise pil, dec %cwp for save, check if coming from priv */
	wrpr	%g0, PIL_MAX, %pil
	rdpr	%tstate, %g4
	and	%g4, TSTATE_CWP, %g6
	brnz,a,pt %g6, 5f
	  dec	%g6
	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g6
5:
	wrpr	%g0, %g6, %cwp
	btst	TSTATE_PRIV, %g4
	bnz,a,pn %xcc, 6f
	  nop

	/* user panic at tl1, get panic stack, go to panic */
	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g5
	wrpr	%g0, %g5, %cleanwin
	set	ptl1_stk_top, %g6
	sub	%g6, SA(MINFRAME) + STACK_BIAS, %sp
	save	%sp, -SA(MINFRAME), %sp
	/* set window registers */
	rdpr	%canrestore, %g5
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, WSTATE_KERN, %wstate
	wrpr	%g0, %g5, %otherwin
	/* Set kcontext to kernel */
	sethi	%hi(FLUSH_ADDR), %g2
	set	MMU_PCONTEXT, %g4
	stxa	%g0, [%g4]ASI_DMMU
	flush	%g2
	/* user panic at tl1 */
	set	sys_tl1_panic, %g1
	set	ptl1_dat, %g2
	lduh	[%g2 + PTL1_TL], %g3
	set	upanictl1msg, %g2
	or	%g0, -1, %g4
	CPU_ADDR(%g5, %g6)
	ldn 	[%g5 + CPU_THREAD], %g5
	set	utl0, %g6
	ba,pt	%xcc, have_win
	  nop
6:
	/* priv panic at tl1 - code below as per priv_trap */
	! is the trap PC in the range allocated to Open Firmware?
	rdpr	%tpc, %g1
	set	OFW_END_ADDR, %g6
	cmp	%g1, %g6
	bgt,pn	%xcc, 1f
	.empty
	set	OFW_START_ADDR, %g6
	cmp	%g1, %g6
	bge,pn	%xcc, obp_bpt
	  nop
1:
	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g6
	dec	%g6
	wrpr	%g6, %g0, %cansave
	wrpr	%g0, %g0, %canrestore
	wrpr	%g0, %g0, %otherwin
	set	ptl1_stk_top, %g6
	sub	%g6, SA(MINFRAME) + STACK_BIAS, %sp
	set	sys_tl1_panic, %g1
	set	ptl1_dat, %g2
	lduh	[%g2 + PTL1_TL], %g3
	set	kpanictl1msg, %g2
	or	%g0, -1, %g4
	rdpr	%pil, %g5
	set	ktl0, %g6
	ba,pt	%xcc, have_win
	  save	%sp, -SA(REGOFF + REGSIZE), %sp
	SET_SIZE(ptl1_panic)
#endif /* lint */

#if defined (lint)

void
sync_callback(void)
{
}

#else /* lint */

	ENTRY_NP(sync_callback)
	wrpr	%g0, 1, %tl		! TL = 1

	rdpr	%pstate, %g1
	andn	%g1, PSTATE_AG, %g2
	wrpr	%g2, %g0, %pstate
	CPU_ADDR(%g3, %g4)
	ldn	[%g3 + CPU_THREAD], THREAD_REG

	rdpr	%pstate, %g1
	or	%g1, PSTATE_AG, %g1
	wrpr	%g1, %g0, %pstate

	set	trap_table, %g1
	wrpr	%g1, %tba
	wrpr	%g0, WSTATE_KERN, %wstate

	sethi	%hi(nwin_minus_one), %g5
	ld	[%g5 + %lo(nwin_minus_one)], %g5
	dec	%g5
	wrpr	%g5, %g0, %cansave
	wrpr	%g0, %g0, %canrestore
	wrpr	%g0, %g0, %otherwin

	rdpr	%tstate, %g1
	or	%g1, TSTATE_PRIV, %g1
	wrpr	%g1, %tstate		! take priv_trap route

	set	sync_callback, %g1	! keep it at priv_trap route
	wrpr	%g0, %g1, %tpc

	set	ptl1_stk_top, %g6
	sub	%g6, SA(MINFRAME) + STACK_BIAS, %sp
	set	sync_handler, %g1
	ba,pt	%xcc, sys_trap
	sub	%g0, 1, %g4
	SET_SIZE(sync_callback)

#endif /* lint */
