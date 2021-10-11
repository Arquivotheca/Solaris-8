/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wbuf.s	1.33	98/07/24 SMI"

#include <sys/asm_linkage.h>
#include <sys/machthread.h>
#include <sys/privregs.h>
#include <sys/spitasi.h>
#include <sys/trap.h>
#include <sys/mmu.h>
#include <sys/machparam.h>
#include <sys/machtrap.h>
#include <sys/traptrace.h>

#if !defined(lint)
#include "assym.h"

	/*
	 * Spill fault handlers
	 *   sn0 - spill normal tl 0
	 *   sn1 - spill normal tl >0
	 *   so0 - spill other tl 0
	 *   so1 - spill other tl >0
	 */

	ENTRY_NP(fault_32bit_sn0)
	!
	FAULT_WINTRACE(%g1, %g2, %g3, TT_F32_SN0)
	!
	! Spill normal tl0 fault.
	! This happens when a user tries to spill to an unmapped stack.
	! We handle it by simulating a pagefault at the trap pc.
	!
	! spill the window into wbuf slot 0
	! (we know wbuf is empty since we came from user mode)
	!
	! g5 = mmu trap type, g6 = tag access reg
	!
	CPU_ADDR(%g4, %g1)
	ldn	[%g4 + CPU_MPCB], %g1
	stn	%sp, [%g1 + MPCB_SPBUF]
	ldn	[%g1 + MPCB_WBUF], %g2
	SAVE_V8WINDOW(%g2)
	mov	1, %g2
	st	%g2, [%g1 + MPCB_WBCNT]
	saved
	!
	! setup user_trap args
	!
	set	sfmmu_tsb_miss, %g1
	mov	%g6, %g2			! arg2 = tagaccess
	mov	T_WIN_OVERFLOW, %g3		! arg3 = traptype
	sub	%g0, 1, %g4
	!
	! spill traps increment %cwp by 2,
	! but user_trap wants the trap %cwp
	! 
	rdpr	%tstate, %g5
	and	%g5, TSTATE_CWP, %g5
	ba,pt	%xcc, user_trap
	wrpr	%g0, %g5, %cwp	
	SET_SIZE(fault_32bit_sn0)

	ENTRY_NP(fault_32bit_sn1)
	!
	FAULT_WINTRACE(%g5, %g6, %g7, TT_F32_SN1)
	!
	! Spill normal tl1 fault.
	! This happens when sys_trap's save spills to an unmapped stack.
	! We handle it by spilling the window to the wbuf and trying
	! sys_trap again.
	!
	CPU_ADDR(%g5, %g6)
	!
	! spill the window into wbuf slot 0
	! (we know wbuf is empty since we came from user mode)
	!
	ldn	[%g5 + CPU_MPCB], %g6
	stn	%sp, [%g6 + MPCB_SPBUF]
	ldn	[%g6 + MPCB_WBUF], %g5
	SAVE_V8WINDOW(%g5)
	mov	1, %g5
	st	%g5, [%g6 + MPCB_WBCNT]
	saved
	set	sys_trap, %g5
	wrpr	%g5, %tnpc
	done
	SET_SIZE(fault_32bit_sn1)
	
	ENTRY_NP(fault_32bit_so0)
	!
	FAULT_WINTRACE(%g5, %g6, %g1, TT_F32_SO0)
	!
	! Spill other tl0 fault.
	! This happens when the kernel spills a user window and that
	! user's stack has been unmapped.
	! We handle it by spilling the window into the user's wbuf.
	!
	! find lwp & increment wbcnt
	!
	CPU_ADDR(%g5, %g6)
	ldn	[%g5 + CPU_MPCB], %g1
	ld	[%g1 + MPCB_WBCNT], %g2
	add	%g2, 1, %g3
	st	%g3, [%g1 + MPCB_WBCNT]
	!
	! use previous wbcnt to spill new spbuf & wbuf
	!
	sll	%g2, CPTRSHIFT, %g4		! spbuf size is sizeof (caddr_t)
	add	%g1, MPCB_SPBUF, %g3
	stn	%sp, [%g3 + %g4]
	sll	%g2, RWIN32SHIFT, %g4
	ldn	[%g1 + MPCB_WBUF], %g3
	add	%g3, %g4, %g3
	SAVE_V8WINDOW(%g3)
	saved
	retry
	SET_SIZE(fault_32bit_so0)

	ENTRY_NP(fault_32bit_so1)
	!
	FAULT_WINTRACE(%g5, %g6, %g7, TT_F32_SO1)
	!
	! Spill other tl1 fault.
	! This happens when priv_trap spills a user window and that
	! user's stack has been unmapped.
	! We handle it by spilling the window to the wbuf and retrying
	! the save.
	!
	CPU_ADDR(%g5, %g6)
	!
	! find lwp & increment wbcnt
	!
	ldn	[%g5 + CPU_MPCB], %g6
	ld	[%g6 + MPCB_WBCNT], %g5
	add	%g5, 1, %g7
	st	%g7, [%g6 + MPCB_WBCNT]
	!
	! use previous wbcnt to spill new spbuf & wbuf
	!
	sll	%g5, CPTRSHIFT, %g7		! spbuf size is sizeof (caddr_t)
	add	%g6, %g7, %g7
	stn	%sp, [%g7 + MPCB_SPBUF]
	sll	%g5, RWIN32SHIFT, %g7
	ldn	[%g6 + MPCB_WBUF], %g5
	add	%g5, %g7, %g7
	SAVE_V8WINDOW(%g7)
	saved
	set	sys_trap, %g5
	wrpr	%g5, %tnpc
	done
	SET_SIZE(fault_32bit_so1)

	ENTRY_NP(fault_64bit_sn0)
	!
	FAULT_WINTRACE(%g1, %g2, %g3, TT_F64_SN0)
	!
	! Spill normal tl0 fault.
	! This happens when a user tries to spill to an unmapped stack.
	! We handle it by simulating a pagefault at the trap pc.
	!
	! spill the window into wbuf slot 0
	! (we know wbuf is empty since we came from user mode)
	!
	! g5 = mmu trap type, g6 = tag access reg
	!
	CPU_ADDR(%g4, %g1)
	ldn	[%g4 + CPU_MPCB], %g1
	stn	%sp, [%g1 + MPCB_SPBUF]
	ldn	[%g1 + MPCB_WBUF], %g2
	SAVE_V9WINDOW(%g2)
	mov	1, %g2
	st	%g2, [%g1 + MPCB_WBCNT]
	saved
	!
	! setup user_trap args
	!
	set	sfmmu_tsb_miss, %g1
	mov	%g6, %g2			! arg2 = tagaccess
	mov	%g5, %g3			! arg3 = traptype
	sub	%g0, 1, %g4
	!
	! spill traps increment %cwp by 2,
	! but user_trap wants the trap %cwp
	! 
	rdpr	%tstate, %g5
	and	%g5, TSTATE_CWP, %g5
	ba,pt	%xcc, user_trap
	  wrpr	%g0, %g5, %cwp	
	SET_SIZE(fault_64bit_sn0)

	ENTRY_NP(fault_64bit_sn1)
	!
	FAULT_WINTRACE(%g5, %g6, %g7, TT_F64_SN1)
	!
	! Spill normal tl1 fault.
	! This happens when sys_trap's save spills to an unmapped stack.
	! We handle it by spilling the window to the wbuf and trying
	! sys_trap again.
	!
	CPU_ADDR(%g5, %g6)
	!
	! spill the window into wbuf slot 0
	! (we know wbuf is empty since we came from user mode)
	!
	ldn	[%g5 + CPU_MPCB], %g6
	stn	%sp, [%g6 + MPCB_SPBUF]
	ldn	[%g6 + MPCB_WBUF], %g5
	SAVE_V9WINDOW(%g5)
	mov	1, %g5
	st	%g5, [%g6 + MPCB_WBCNT]
	saved
	set	sys_trap, %g5
	wrpr	%g5, %tnpc
	done
	SET_SIZE(fault_64bit_sn1)

	ENTRY_NP(fault_64bit_so0)
	!
	FAULT_WINTRACE(%g5, %g6, %g1, TT_F64_SO0)
	!
	! Spill other tl0 fault.
	! This happens when the kernel spills a user window and that
	! user's stack has been unmapped.
	! We handle it by spilling the window into the user's wbuf.
	!
	! find lwp & increment wbcnt
	!
	CPU_ADDR(%g5, %g6)
	ldn	[%g5 + CPU_MPCB], %g1
	ld	[%g1 + MPCB_WBCNT], %g2
	add	%g2, 1, %g3
	st	%g3, [%g1 + MPCB_WBCNT]
	!
	! use previous wbcnt to spill new spbuf & wbuf
	!
	sll	%g2, CPTRSHIFT, %g4		! spbuf size is sizeof (caddr_t)
	add	%g1, MPCB_SPBUF, %g3
	stn	%sp, [%g3 + %g4]
	sll	%g2, RWIN64SHIFT, %g4
	ldn	[%g1 + MPCB_WBUF], %g3
	add	%g3, %g4, %g3
	SAVE_V9WINDOW(%g3)
	saved
	retry
	SET_SIZE(fault_64bit_so0)

	ENTRY_NP(fault_64bit_so1)
	!
	FAULT_WINTRACE(%g5, %g6, %g7, TT_F64_SO1)
	!
	! Spill other tl1 fault.
	! This happens when priv_trap spills a user window and that
	! user's stack has been unmapped.
	! We handle it by spilling the window to the wbuf and retrying
	! the save.
	!
	CPU_ADDR(%g5, %g6)
	!
	! find lwp & increment wbcnt
	!
	ldn	[%g5 + CPU_MPCB], %g6
	ld	[%g6 + MPCB_WBCNT], %g5
	add	%g5, 1, %g7
	st	%g7, [%g6 + MPCB_WBCNT]
	!
	! use previous wbcnt to spill new spbuf & wbuf
	!
	sll	%g5, CPTRSHIFT, %g7		! spbuf size is sizeof (caddr_t)
	add	%g6, %g7, %g7
	stn	%sp, [%g7 + MPCB_SPBUF]
	sll	%g5, RWIN64SHIFT, %g7
	ldn	[%g6 + MPCB_WBUF], %g5
	add	%g5, %g7, %g7
	SAVE_V9WINDOW(%g7)
	saved
	set	sys_trap, %g5
	wrpr	%g5, %tnpc
	done
	SET_SIZE(fault_64bit_so1)

	/*
	 * Fill fault handlers
	 *   fn0 - fill normal tl 0
	 *   fn1 - fill normal tl 1
	 */

	ENTRY_NP(fault_32bit_fn0)
	!
	FAULT_WINTRACE(%g1, %g2, %g3, TT_F32_FN0)
	!
.fault_fn0_common:
	!
	! Fill normal tl0 fault.
	! This happens when a user tries to fill from an unmapped stack.
	! We handle it by simulating a pagefault at the trap pc.
	!
	! setup user_trap args
	!
	! g5 = mmu trap type, g6 = tag access reg
	!
	set	sfmmu_tsb_miss, %g1
	mov	%g6, %g2			! arg2 = tagaccess
	mov	T_WIN_UNDERFLOW, %g3
	sub	%g0, 1, %g4
	!
	! sys_trap wants %cwp to be the same as when the trap occured,
	! so set it from %tstate
	!
	rdpr	%tstate, %g5
	and	%g5, TSTATE_CWP, %g5
	ba,pt	%xcc, user_trap
	wrpr	%g0, %g5, %cwp
	SET_SIZE(fault_32bit_fn0)

	ENTRY_NP(fault_32bit_fn1)
	!
	FAULT_WINTRACE(%g1, %g2, %g3, TT_F32_FN1)
	!
	srl	%sp, 0, %g7
	!
.fault_fn1_common:	
	!
	! Fill normal tl1 fault.
	! This happens when user_rtt's restore fills from an unmapped
	! stack.  We handle it by simulating a pagefault in the kernel
	! at user_rtt.
	!
	! save fault addr & fix %cwp
	!
	rdpr	%tstate, %g1
	and	%g1, TSTATE_CWP, %g1
	wrpr	%g0, %g1, %cwp
	!
	! fake tl1 traps regs so that after pagefault runs, we
	! re-execute at user_rtt.
	!
	wrpr	%g0, 1, %tl
	set	TSTATE_KERN | TSTATE_IE, %g1
	wrpr	%g0, %g1, %tstate
	set	user_rtt, %g1
	wrpr	%g0, %g1, %tpc
	add	%g1, 4, %g1
	wrpr	%g0, %g1, %tnpc
	!
	! setup sys_trap args
	!
	! g5 = mmu trap type, g6 = tag access reg
	!
	set	sfmmu_tsb_miss, %g1
	mov	%g6, %g2			! arg2 = tagaccess
	set	T_USER | T_SYS_RTT_PAGE, %g3	! arg3 = traptype
	sub	%g0, 1, %g4
	!
	! setup to run kernel again by setting THREAD_REG, %wstate
	! and the mmu to their kernel values.
	!
	rdpr	%pstate, %l1
	wrpr	%l1, PSTATE_AG, %pstate
	mov	%l6, THREAD_REG			! %l6 is user_rtt's thread
	wrpr	%g0, %l1, %pstate
	rdpr	%wstate, %l1
	sllx	%l1, WSTATE_SHIFT, %l1
#ifdef __sparcv9
	wrpr	%l1, WSTATE_K64, %wstate
#else
	wrpr	%l1, WSTATE_K32, %wstate
#endif
	mov	KCONTEXT, %g5
	mov	MMU_PCONTEXT, %g6
	stxa	%g5, [%g6]ASI_DMMU
	ba,pt	%xcc, priv_trap
	membar	#Sync
	SET_SIZE(fault_32bit_fn1)

	ENTRY_NP(fault_64bit_fn0)
	FAULT_WINTRACE(%g1, %g2, %g3, TT_F64_FN0)
	b	.fault_fn0_common
	  nop
	SET_SIZE(fault_64bit_fn0)

	ENTRY_NP(fault_64bit_fn1)
	FAULT_WINTRACE(%g1, %g2, %g3, TT_F64_FN1)
	b	.fault_fn1_common
	  nop
	SET_SIZE(fault_64bit_fn1)

	/*
	 * Kernel fault handlers
	 */
	ENTRY_NP(fault_32bit_not)
	ENTRY_NP(fault_64bit_not)
	ta	72
	SET_SIZE(fault_32bit_not)
	SET_SIZE(fault_64bit_not)
#endif /* !lint */
