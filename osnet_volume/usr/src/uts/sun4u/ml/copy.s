/*
 * Copyright (c) 1990-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)copy.s	1.44	00/01/03 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/vtrace.h>
#include <sys/machthread.h>
#include <sys/clock.h>
#include <sys/asi.h>
#include <sys/fsr.h>
#include <sys/privregs.h>

#if !defined(lint)
#include "assym.h"
#endif	/* lint */

/*
 * Pseudo-code to aid in understanding the control flow of the
 * bcopy/copyin/copyout routines.
 *
 * On entry:
 *
 * 	%l6 = curthread->t_lofault;
 * 	used_block_copy = FALSE;			! %l6 |= 1
 * 	if (%l6 != NULL) {
 * 		curthread->t_lofault = .copyerr;
 * 		caller_error_handler = TRUE             ! %l6 |= 2
 * 	}
 *
 * 	if (length < HW_THRESHOLD)
 * 		goto regular_copy;
 *
 * 	if (!use_vis)
 * 		goto_regular_copy;
 *
 * 	if (curthread->t_lwp == NULL) {
 *		! Kernel threads do not have pcb's in which to store
 *		! the floating point state, disallow preemption during
 *		! the copy.
 * 		kpreempt_disable(curthread);
 *	}
 *
 * 	old_fprs = %fprs;
 * 	old_gsr = %gsr;
 * 	if (%fprs.fef) {
 *              ! If we need to save 4 blocks of fpregs then make sure
 *		! the length is still appropriate for that extra overhead.
 * 		if (length < (large_length + (64 * 4))) {
 * 			if (curthread->t_lwp == NULL)
 * 				kpreempt_enable(curthread);
 * 			goto regular_copy;
 * 		}
 * 		%fprs.fef = 1;
 * 		save current fpregs on stack using blockstore
 * 	} else {
 * 		%fprs.fef = 1;
 * 	}
 *
 * 	used_block_copy = 1;				! %l6 |= 1
 * 	do_blockcopy_here;
 *
 * On exit (in lofault handler as well):
 *
 * 	if (used_block_copy) {				! %l6 & 1
 * 		%gsr = old_gsr;
 * 		if (old_fprs & (FPRS_DU | FPRS_DL | FPRS_FEF))
 * 			restore fpregs from stack using blockload
 *		else
 *			zero fpregs
 * 		%fprs = old_fprs;
 * 		if (curthread->t_lwp == NULL)
 *			kpreempt_enable(curthread);
 * 	}
 * 	curthread->t_lofault = (%l6 & ~3);
 * 	return (0)
 */

/*
 * Number of bytes needed to "break even" using VIS-accelerated
 * memory operations.
 */
#define	HW_THRESHOLD	900

/*
 * Size of stack frame in order to accomodate a 64-byte aligned
 * floating-point register save area and 2 32-bit temp locations.
 */
#define	HWCOPYFRAMESIZE	((64 * 5) + (2 * 4))

#define SAVED_FPREGS_OFFSET	(64 * 5)
#define	SAVED_FPRS_OFFSET	(SAVED_FPREGS_OFFSET + 4)
#define	SAVED_GSR_OFFSET	(SAVED_FPRS_OFFSET + 4)


#ifdef TRACE

	.section	".text"
TR_bcopy_start:
	.asciz "bcopy_start:caller %K src %x dest %x size %d"
	.align 4
TR_kcopy_start:
	.asciz "kcopy_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyout_noerr_start:
	.asciz "copyout_noerr_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyin_noerr_start:
	.asciz "copyin_noerr_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyout_start:
	.asciz "copyout_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyin_start:
	.asciz "copyin_start:caller %K src %x dest %x size %d"
	.align 4

TR_copy_end:
	.asciz "copy_end"
	.align 4

TR_copy_fault:
	.asciz "copy_fault"
	.align 4
TR_copyio_fault:
	.asciz "copyio_fault"
	.align 4

#define	TRACE_IN(a, b)		\
	save	%sp, -SA(MINFRAME), %sp;	\
	mov	%i0, %l0;	\
	mov	%i1, %l1;	\
	mov	%i2, %l2;	\
	TRACE_ASM_4(%o5, TR_FAC_BCOPY, a, b, %i7, %l0, %l1, %l2);	\
	mov	%l0, %i0;	\
	mov	%l1, %i1;	\
	mov	%l2, %i2;	\
	restore %g0, 0, %g0;

#else	/* TRACE */

#define	TRACE_IN(a,b)

#endif	/* TRACE */

/*
 * Common macros used by the various versions of the block copy
 * routines in this file.
 */

#define	FZERO				\
	fzero	%f0			;\
	fzero	%f2			;\
	faddd	%f0, %f2, %f4		;\
	fmuld	%f0, %f2, %f6		;\
	faddd	%f0, %f2, %f8		;\
	fmuld	%f0, %f2, %f10		;\
	faddd	%f0, %f2, %f12		;\
	fmuld	%f0, %f2, %f14		;\
	faddd	%f0, %f2, %f16		;\
	fmuld	%f0, %f2, %f18		;\
	faddd	%f0, %f2, %f20		;\
	fmuld	%f0, %f2, %f22		;\
	faddd	%f0, %f2, %f24		;\
	fmuld	%f0, %f2, %f26		;\
	faddd	%f0, %f2, %f28		;\
	fmuld	%f0, %f2, %f30		;\
	faddd	%f0, %f2, %f32		;\
	fmuld	%f0, %f2, %f34		;\
	faddd	%f0, %f2, %f36		;\
	fmuld	%f0, %f2, %f38		;\
	faddd	%f0, %f2, %f40		;\
	fmuld	%f0, %f2, %f42		;\
	faddd	%f0, %f2, %f44		;\
	fmuld	%f0, %f2, %f46		;\
	faddd	%f0, %f2, %f48		;\
	fmuld	%f0, %f2, %f50		;\
	faddd	%f0, %f2, %f52		;\
	fmuld	%f0, %f2, %f54		;\
	faddd	%f0, %f2, %f56		;\
	fmuld	%f0, %f2, %f58		;\
	faddd	%f0, %f2, %f60		;\
	fmuld	%f0, %f2, %f62


#define	FALIGN_D0			\
	faligndata %d0, %d2, %d48	;\
	faligndata %d2, %d4, %d50	;\
	faligndata %d4, %d6, %d52	;\
	faligndata %d6, %d8, %d54	;\
	faligndata %d8, %d10, %d56	;\
	faligndata %d10, %d12, %d58	;\
	faligndata %d12, %d14, %d60	;\
	faligndata %d14, %d16, %d62

#define	FALIGN_D16			\
	faligndata %d16, %d18, %d48	;\
	faligndata %d18, %d20, %d50	;\
	faligndata %d20, %d22, %d52	;\
	faligndata %d22, %d24, %d54	;\
	faligndata %d24, %d26, %d56	;\
	faligndata %d26, %d28, %d58	;\
	faligndata %d28, %d30, %d60	;\
	faligndata %d30, %d32, %d62

#define	FALIGN_D32			\
	faligndata %d32, %d34, %d48	;\
	faligndata %d34, %d36, %d50	;\
	faligndata %d36, %d38, %d52	;\
	faligndata %d38, %d40, %d54	;\
	faligndata %d40, %d42, %d56	;\
	faligndata %d42, %d44, %d58	;\
	faligndata %d44, %d46, %d60	;\
	faligndata %d46, %d0, %d62

#define	FALIGN_D2			\
	faligndata %d2, %d4, %d48	;\
	faligndata %d4, %d6, %d50	;\
	faligndata %d6, %d8, %d52	;\
	faligndata %d8, %d10, %d54	;\
	faligndata %d10, %d12, %d56	;\
	faligndata %d12, %d14, %d58	;\
	faligndata %d14, %d16, %d60	;\
	faligndata %d16, %d18, %d62

#define	FALIGN_D18			\
	faligndata %d18, %d20, %d48	;\
	faligndata %d20, %d22, %d50	;\
	faligndata %d22, %d24, %d52	;\
	faligndata %d24, %d26, %d54	;\
	faligndata %d26, %d28, %d56	;\
	faligndata %d28, %d30, %d58	;\
	faligndata %d30, %d32, %d60	;\
	faligndata %d32, %d34, %d62

#define	FALIGN_D34			\
	faligndata %d34, %d36, %d48	;\
	faligndata %d36, %d38, %d50	;\
	faligndata %d38, %d40, %d52	;\
	faligndata %d40, %d42, %d54	;\
	faligndata %d42, %d44, %d56	;\
	faligndata %d44, %d46, %d58	;\
	faligndata %d46, %d0, %d60	;\
	faligndata %d0, %d2, %d62

#define	FALIGN_D4			\
	faligndata %d4, %d6, %d48	;\
	faligndata %d6, %d8, %d50	;\
	faligndata %d8, %d10, %d52	;\
	faligndata %d10, %d12, %d54	;\
	faligndata %d12, %d14, %d56	;\
	faligndata %d14, %d16, %d58	;\
	faligndata %d16, %d18, %d60	;\
	faligndata %d18, %d20, %d62

#define	FALIGN_D20			\
	faligndata %d20, %d22, %d48	;\
	faligndata %d22, %d24, %d50	;\
	faligndata %d24, %d26, %d52	;\
	faligndata %d26, %d28, %d54	;\
	faligndata %d28, %d30, %d56	;\
	faligndata %d30, %d32, %d58	;\
	faligndata %d32, %d34, %d60	;\
	faligndata %d34, %d36, %d62

#define	FALIGN_D36			\
	faligndata %d36, %d38, %d48	;\
	faligndata %d38, %d40, %d50	;\
	faligndata %d40, %d42, %d52	;\
	faligndata %d42, %d44, %d54	;\
	faligndata %d44, %d46, %d56	;\
	faligndata %d46, %d0, %d58	;\
	faligndata %d0, %d2, %d60	;\
	faligndata %d2, %d4, %d62

#define	FALIGN_D6			\
	faligndata %d6, %d8, %d48	;\
	faligndata %d8, %d10, %d50	;\
	faligndata %d10, %d12, %d52	;\
	faligndata %d12, %d14, %d54	;\
	faligndata %d14, %d16, %d56	;\
	faligndata %d16, %d18, %d58	;\
	faligndata %d18, %d20, %d60	;\
	faligndata %d20, %d22, %d62

#define	FALIGN_D22			\
	faligndata %d22, %d24, %d48	;\
	faligndata %d24, %d26, %d50	;\
	faligndata %d26, %d28, %d52	;\
	faligndata %d28, %d30, %d54	;\
	faligndata %d30, %d32, %d56	;\
	faligndata %d32, %d34, %d58	;\
	faligndata %d34, %d36, %d60	;\
	faligndata %d36, %d38, %d62

#define	FALIGN_D38			\
	faligndata %d38, %d40, %d48	;\
	faligndata %d40, %d42, %d50	;\
	faligndata %d42, %d44, %d52	;\
	faligndata %d44, %d46, %d54	;\
	faligndata %d46, %d0, %d56	;\
	faligndata %d0, %d2, %d58	;\
	faligndata %d2, %d4, %d60	;\
	faligndata %d4, %d6, %d62

#define	FALIGN_D8			\
	faligndata %d8, %d10, %d48	;\
	faligndata %d10, %d12, %d50	;\
	faligndata %d12, %d14, %d52	;\
	faligndata %d14, %d16, %d54	;\
	faligndata %d16, %d18, %d56	;\
	faligndata %d18, %d20, %d58	;\
	faligndata %d20, %d22, %d60	;\
	faligndata %d22, %d24, %d62

#define	FALIGN_D24			\
	faligndata %d24, %d26, %d48	;\
	faligndata %d26, %d28, %d50	;\
	faligndata %d28, %d30, %d52	;\
	faligndata %d30, %d32, %d54	;\
	faligndata %d32, %d34, %d56	;\
	faligndata %d34, %d36, %d58	;\
	faligndata %d36, %d38, %d60	;\
	faligndata %d38, %d40, %d62

#define	FALIGN_D40			\
	faligndata %d40, %d42, %d48	;\
	faligndata %d42, %d44, %d50	;\
	faligndata %d44, %d46, %d52	;\
	faligndata %d46, %d0, %d54	;\
	faligndata %d0, %d2, %d56	;\
	faligndata %d2, %d4, %d58	;\
	faligndata %d4, %d6, %d60	;\
	faligndata %d6, %d8, %d62

#define	FALIGN_D10			\
	faligndata %d10, %d12, %d48	;\
	faligndata %d12, %d14, %d50	;\
	faligndata %d14, %d16, %d52	;\
	faligndata %d16, %d18, %d54	;\
	faligndata %d18, %d20, %d56	;\
	faligndata %d20, %d22, %d58	;\
	faligndata %d22, %d24, %d60	;\
	faligndata %d24, %d26, %d62

#define	FALIGN_D26			\
	faligndata %d26, %d28, %d48	;\
	faligndata %d28, %d30, %d50	;\
	faligndata %d30, %d32, %d52	;\
	faligndata %d32, %d34, %d54	;\
	faligndata %d34, %d36, %d56	;\
	faligndata %d36, %d38, %d58	;\
	faligndata %d38, %d40, %d60	;\
	faligndata %d40, %d42, %d62

#define	FALIGN_D42			\
	faligndata %d42, %d44, %d48	;\
	faligndata %d44, %d46, %d50	;\
	faligndata %d46, %d0, %d52	;\
	faligndata %d0, %d2, %d54	;\
	faligndata %d2, %d4, %d56	;\
	faligndata %d4, %d6, %d58	;\
	faligndata %d6, %d8, %d60	;\
	faligndata %d8, %d10, %d62

#define	FALIGN_D12			\
	faligndata %d12, %d14, %d48	;\
	faligndata %d14, %d16, %d50	;\
	faligndata %d16, %d18, %d52	;\
	faligndata %d18, %d20, %d54	;\
	faligndata %d20, %d22, %d56	;\
	faligndata %d22, %d24, %d58	;\
	faligndata %d24, %d26, %d60	;\
	faligndata %d26, %d28, %d62

#define	FALIGN_D28			\
	faligndata %d28, %d30, %d48	;\
	faligndata %d30, %d32, %d50	;\
	faligndata %d32, %d34, %d52	;\
	faligndata %d34, %d36, %d54	;\
	faligndata %d36, %d38, %d56	;\
	faligndata %d38, %d40, %d58	;\
	faligndata %d40, %d42, %d60	;\
	faligndata %d42, %d44, %d62

#define	FALIGN_D44			\
	faligndata %d44, %d46, %d48	;\
	faligndata %d46, %d0, %d50	;\
	faligndata %d0, %d2, %d52	;\
	faligndata %d2, %d4, %d54	;\
	faligndata %d4, %d6, %d56	;\
	faligndata %d6, %d8, %d58	;\
	faligndata %d8, %d10, %d60	;\
	faligndata %d10, %d12, %d62

#define	FALIGN_D14			\
	faligndata %d14, %d16, %d48	;\
	faligndata %d16, %d18, %d50	;\
	faligndata %d18, %d20, %d52	;\
	faligndata %d20, %d22, %d54	;\
	faligndata %d22, %d24, %d56	;\
	faligndata %d24, %d26, %d58	;\
	faligndata %d26, %d28, %d60	;\
	faligndata %d28, %d30, %d62

#define	FALIGN_D30			\
	faligndata %d30, %d32, %d48	;\
	faligndata %d32, %d34, %d50	;\
	faligndata %d34, %d36, %d52	;\
	faligndata %d36, %d38, %d54	;\
	faligndata %d38, %d40, %d56	;\
	faligndata %d40, %d42, %d58	;\
	faligndata %d42, %d44, %d60	;\
	faligndata %d44, %d46, %d62

#define	FALIGN_D46			\
	faligndata %d46, %d0, %d48	;\
	faligndata %d0, %d2, %d50	;\
	faligndata %d2, %d4, %d52	;\
	faligndata %d4, %d6, %d54	;\
	faligndata %d6, %d8, %d56	;\
	faligndata %d8, %d10, %d58	;\
	faligndata %d10, %d12, %d60	;\
	faligndata %d12, %d14, %d62


/*
 * Copy a block of storage, returning an error code if `from' or
 * `to' takes a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok
 */

#if defined(lint)

/* ARGSUSED */
int
kcopy(const void *from, void *to, size_t count)
{ return(0); }

#else	/* lint */

	.seg	".text"
	.align	4

	ENTRY(kcopy)

	TRACE_IN(TR_KCOPY_START, TR_kcopy_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	set	.copyerr, %l6		! copyerr is lofault value
	ldn	[THREAD_REG + T_LOFAULT], %l7	! save existing handler
	stn	%l6, [THREAD_REG + T_LOFAULT]	! set t_lofault
	b	.do_copy		! common code
	  mov	%l7, %l6

/*
 * We got here because of a fault during kcopy.
 * Errno value is in %g1.
 */
.copyerr:
	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPY_FAULT, TR_copy_fault);
	btst	1, %l6
	bz	1f
	  andn	%l6, 1, %l6

	membar	#Sync

	ld	[%fp + STACK_BIAS - SAVED_GSR_OFFSET], %o2	! restore gsr
	wr	%o2, 0, %gsr

	ld	[%fp + STACK_BIAS - SAVED_FPRS_OFFSET], %o3
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o3
	bz	4f
	  nop

	! restore fpregs from stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d16
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d32
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d48
	membar	#Sync

	ba	2f
	  wr	%o3, 0, %fprs		! restore fprs

4:
	FZERO				! zero all of the fpregs
	wr	%o3, 0, %fprs		! restore fprs

2:	ldn	[THREAD_REG + T_LWP], %o2
	tst	%o2
	bnz,pt	%ncc, 1f
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%ncc, 1f
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%ncc, 1f
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

1:
	btst	2, %l6
	andn	%l6, 2, %l6
	bnz,pn	%ncc, 3f
	  stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g1, 0, %o0

3:
	jmp	%l6				! goto real handler
	restore	%g0, 0, %o0			! dispose of copy window

	SET_SIZE(kcopy)
#endif	/* lint */

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * Registers: l6 - saved t_lofault
 *
 * Copy a page of memory.
 * Assumes double word alignment and a count >= 256.
 */
#if defined(lint)

/* ARGSUSED */
void
bcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(bcopy)

	TRACE_IN(TR_BCOPY_START, TR_bcopy_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	ldn	[THREAD_REG + T_LOFAULT], %l6	! save t_lofault
	brz,pt	%l6, .do_copy
	  nop
	set	.copyerr, %o2
	stn	%o2, [THREAD_REG + T_LOFAULT]	! install new vector
	or	%l6, 2, %l6		! error should trampoline

.do_copy:
	cmp	%i2, 12			! for small counts
	blu	%ncc, .bytecp		! just copy bytes
	  .empty

	cmp	%i2, HW_THRESHOLD	! for large counts
	blu	%ncc, .bcb_punt
	  .empty

	set	use_hw_bcopy, %o2
	ld	[%o2], %o2
	tst	%o2
	bz	.bcb_punt
	  nop

	subcc	%i1, %i0, %i3
	bneg,a,pn %ncc, 1f
	neg	%i3
1:
	/*
	 * Compare against 256 since we should be checking block addresses
	 * and (dest & ~63) - (src & ~63) can be 3 blocks even if
	 * src = dest + (64 * 3) + 63.
	 */
	cmp	%i3, 256
	blu,pn	%ncc, .bcb_punt
	  nop

	ldn	[THREAD_REG + T_LWP], %o3
	tst	%o3
	bnz,pt	%ncc, 1f
	  nop

	! kpreempt_disable();
	ldsb	[THREAD_REG + T_PREEMPT], %o2
	inc	%o2
	stb	%o2, [THREAD_REG + T_PREEMPT]

1:
	rd	%fprs, %o2		! check for unused fp
	st	%o2, [%fp + STACK_BIAS - SAVED_FPRS_OFFSET] ! save orig %fprs
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o2
	bz,a	.do_blockcopy
	  wr	%g0, FPRS_FEF, %fprs

.bcb_fpregs_inuse:
	cmp	%i2, HW_THRESHOLD+(64*4) ! for large counts (larger
	bgeu	%ncc, 1f		!  if we have to save the fpregs)
	  nop

	tst	%o3
	bnz,pt	%ncc, .bcb_punt
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%xcc, .bcb_punt
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%icc, .bcb_punt
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

	ba,a	.bcb_punt
	  nop

1:
	wr	%g0, FPRS_FEF, %fprs

	! save in-use fpregs on stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	stda	%d0, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d16, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d32, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d48, [%o2]ASI_BLK_P
	membar	#Sync

.do_blockcopy:
	membar	#StoreStore|#StoreLoad|#LoadStore

	rd	%gsr, %o2
	st	%o2, [%fp + STACK_BIAS - SAVED_GSR_OFFSET]	! save gsr

	! Set the lower bit in the saved t_lofault to indicate
	! that we need to clear the %fprs register on the way
	! out
	or	%l6, 1, %l6

	! Swap src/dst since the code below is memcpy code
	! and memcpy/bcopy have different calling sequences
	mov	%i1, %i5
	mov	%i0, %i1
	mov	%i5, %i0

!!! This code is nearly identical to the version in the sun4u
!!! libc_psr.  Most bugfixes made to that file should be
!!! merged into this routine.

	andcc	%i0, 7, %o3
	bz,pt	%ncc, blkcpy
	sub	%o3, 8, %o3
	neg	%o3
	sub	%i2, %o3, %i2

	! Align Destination on double-word boundary

2:	ldub	[%i1], %o4
	inc	%i1
	inc	%i0
	deccc	%o3
	bgu	%ncc, 2b
	stb	%o4, [%i0 - 1]
blkcpy:	
	andcc	%i0, 63, %i3
	bz,pn	%ncc, blalign		! now block aligned
	sub	%i3, 64, %i3
	neg	%i3			! bytes till block aligned
	sub	%i2, %i3, %i2		! update %i2 with new count

	! Copy %i3 bytes till dst is block (64 byte) aligned. use
	! double word copies.

	alignaddr %i1, %g0, %g1
	ldd	[%g1], %d0
	add	%g1, 8, %g1
6:
	ldd	[%g1], %d2
	add	%g1, 8, %g1
	subcc	%i3, 8, %i3
	faligndata %d0, %d2, %d8
	std	%d8, [%i0]
	add	%i1, 8, %i1
	bz,pn	%ncc, blalign
	add	%i0, 8, %i0
	ldd	[%g1], %d0
	add	%g1, 8, %g1
	subcc	%i3, 8, %i3
	faligndata %d2, %d0, %d8
	std	%d8, [%i0]
	add	%i1, 8, %i1
	bgu,pn	%ncc, 6b
	add	%i0, 8, %i0
 
blalign:
	membar	#StoreLoad
	! %i2 = total length
	! %i3 = blocks	(length - 64) / 64
	! %i4 = doubles remaining  (length - blocks)
	sub	%i2, 64, %i3
	andn	%i3, 63, %i3
	sub	%i2, %i3, %i4
	andn	%i4, 7, %i4
	sub	%i4, 16, %i4
	sub	%i2, %i4, %i2
	sub	%i2, %i3, %i2

	andn	%i1, 0x3f, %l7		! blk aligned address
	alignaddr %i1, %g0, %g0		! gen %gsr

	srl	%i1, 3, %l5		! bits 3,4,5 are now least sig in  %l5
	andcc	%l5, 7, %i5		! mask everything except bits 1,2 3
	add	%i1, %i4, %i1
	add	%i1, %i3, %i1

	ldda	[%l7]ASI_BLK_P, %d0
	add	%l7, 64, %l7
	ldda	[%l7]ASI_BLK_P, %d16
	add	%l7, 64, %l7
	ldda	[%l7]ASI_BLK_P, %d32
	add	%l7, 64, %l7
	sub	%i3, 128, %i3

	! switch statement to get us to the right 8 byte blk within a
	! 64 byte block
	cmp	 %i5, 4
	bgeu,a	 hlf
	cmp	 %i5, 6
	cmp	 %i5, 2
	bgeu,a	 sqtr
	nop
	cmp	 %i5, 1
	be,a	 seg1
	nop
	ba	 seg0
	nop
sqtr:
	be,a	 seg2
	nop
	ba,a	 seg3
	nop

hlf:
	bgeu,a	 fqtr
	nop	 
	cmp	 %i5, 5
	be,a	 seg5
	nop
	ba	 seg4
	nop
fqtr:
	be,a	 seg6
	nop
	ba	 seg7
	nop
	

seg0:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D0
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D16
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D32
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg0

0:
	FALIGN_D16
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D32
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd0
	add	%i0, 64, %i0

1:
	FALIGN_D32
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D0
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd16
	add	%i0, 64, %i0

2:
	FALIGN_D0
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D16
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd32
	add	%i0, 64, %i0

seg1:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D2
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D18
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D34
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg1
0:
	FALIGN_D18
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D34
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd2
	add	%i0, 64, %i0

1:
	FALIGN_D34
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D2
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd18
	add	%i0, 64, %i0

2:
	FALIGN_D2
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D18
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd34
	add	%i0, 64, %i0

seg2:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D4
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D20
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D36
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg2

0:
	FALIGN_D20
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D36
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd4
	add	%i0, 64, %i0

1:
	FALIGN_D36
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D4
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd20
	add	%i0, 64, %i0

2:
	FALIGN_D4
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D20
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd36
	add	%i0, 64, %i0

seg3:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D6
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D22
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D38
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg3

0:
	FALIGN_D22
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D38
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd6
	add	%i0, 64, %i0

1:
	FALIGN_D38
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D6
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd22
	add	%i0, 64, %i0

2:
	FALIGN_D6
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D22
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd38
	add	%i0, 64, %i0

seg4:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D8
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D24
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D40
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg4

0:
	FALIGN_D24
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D40
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd8
	add	%i0, 64, %i0

1:
	FALIGN_D40
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D8
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd24
	add	%i0, 64, %i0

2:
	FALIGN_D8
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D24
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd40
	add	%i0, 64, %i0

seg5:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D10
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D26
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D42
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg5

0:
	FALIGN_D26
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D42
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd10
	add	%i0, 64, %i0

1:
	FALIGN_D42
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D10
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd26
	add	%i0, 64, %i0

2:
	FALIGN_D10
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D26
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd42
	add	%i0, 64, %i0

seg6:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D12
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D28
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D44
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg6

0:
	FALIGN_D28
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D44
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd12
	add	%i0, 64, %i0

1:
	FALIGN_D44
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D12
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd28
	add	%i0, 64, %i0

2:
	FALIGN_D12
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D28
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd44
	add	%i0, 64, %i0

seg7:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D14
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D30
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D46
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, seg7

0:
	FALIGN_D30
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D46
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd14
	add	%i0, 64, %i0

1:
	FALIGN_D46
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D14
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd30
	add	%i0, 64, %i0

2:
	FALIGN_D14
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D30
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, blkd46
	add	%i0, 64, %i0


	!
	! dribble out the last partial block
	!
blkd0:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d0, %d2, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd2:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d2, %d4, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd4:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d4, %d6, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd6:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d6, %d8, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd8:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d8, %d10, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd10:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d10, %d12, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd12:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d12, %d14, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd14:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	fsrc1	%d14, %d0
	ba,a,pt	%ncc, blkleft

blkd16:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d16, %d18, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd18:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d18, %d20, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd20:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d20, %d22, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd22:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d22, %d24, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd24:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d24, %d26, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd26:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d26, %d28, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd28:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d28, %d30, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd30:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	fsrc1	%d30, %d0
	ba,a,pt	%ncc, blkleft
blkd32:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d32, %d34, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd34:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d34, %d36, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd36:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d36, %d38, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd38:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d38, %d40, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd40:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d40, %d42, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd42:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d42, %d44, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd44:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	faligndata %d44, %d46, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
blkd46:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, blkdone
	fsrc1	%d46, %d0

blkleft:
1:
	ldd	[%l7], %d2
	add	%l7, 8, %l7
	subcc	%i4, 8, %i4
	faligndata %d0, %d2, %d8
	std	%d8, [%i0]
	blu,pn	%ncc, blkdone
	add	%i0, 8, %i0
	ldd	[%l7], %d0
	add	%l7, 8, %l7
	subcc	%i4, 8, %i4
	faligndata %d2, %d0, %d8
	std	%d8, [%i0]
	bgeu,pt	%ncc, 1b
	add	%i0, 8, %i0

blkdone:
	tst	%i2
	bz,pt	%ncc, .bcb_exit
	and	%l3, 0x4, %l3		! fprs.du = fprs.dl = 0

7:	ldub	[%i1], %i4
	inc	%i1
	inc	%i0
	deccc	%i2
	bgu,pt	%ncc, 7b
	  stb	  %i4, [%i0 - 1]

.bcb_exit:
	membar	#StoreLoad|#StoreStore
	btst	1, %l6
	bz	1f
	  andn	%l6, 3, %l6

	ld	[%fp + STACK_BIAS - SAVED_GSR_OFFSET], %o2	! restore gsr
	wr	%o2, 0, %gsr

	ld	[%fp + STACK_BIAS - SAVED_FPRS_OFFSET], %o3
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o3
	bz	4f
	  nop

	! restore fpregs from stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d16
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d32
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d48
	membar	#Sync

	ba	2f	
	  wr	%o3, 0, %fprs		! restore fprs

4:
	FZERO				! zero all of the fpregs
	wr	%o3, 0, %fprs		! restore fprs

2:	ldn	[THREAD_REG + T_LWP], %o2
	tst	%o2
	bnz,pt	%ncc, 1f
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%ncc, 1f
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%ncc, 1f
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

1:
	stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g0, 0, %o0

.bcb_punt:
	!
	! use aligned transfers where possible
	!
	xor	%i0, %i1, %o4		! xor from and to address
	btst	7, %o4			! if lower three bits zero
	bz	.aldoubcp		! can align on double boundary
	.empty	! assembler complaints about label

	xor	%i0, %i1, %o4		! xor from and to address
	btst	3, %o4			! if lower two bits zero
	bz	.alwordcp		! can align on word boundary
	btst	3, %i0			! delay slot, from address unaligned?
	!
	! use aligned reads and writes where possible
	! this differs from wordcp in that it copes
	! with odd alignment between source and destnation
	! using word reads and writes with the proper shifts
	! in between to align transfers to and from memory
	! i0 - src address, i1 - dest address, i2 - count
	! i3, i4 - tmps for used generating complete word
	! i5 (word to write)
	! l0 size in bits of upper part of source word (US)
	! l1 size in bits of lower part of source word (LS = 32 - US)
	! l2 size in bits of upper part of destination word (UD)
	! l3 size in bits of lower part of destination word (LD = 32 - UD)
	! l4 number of bytes leftover after aligned transfers complete
	! l5 the number 32
	!
	mov	32, %l5			! load an oft-needed constant
	bz	.align_dst_only
	btst	3, %i1			! is destnation address aligned?
	clr	%i4			! clear registers used in either case
	bz	.align_src_only
	clr	%l0
	!
	! both source and destination addresses are unaligned
	!
1:					! align source
	ldub	[%i0], %i3		! read a byte from source address
	add	%i0, 1, %i0		! increment source address
	or	%i4, %i3, %i4		! or in with previous bytes (if any)
	btst	3, %i0			! is source aligned?
	add	%l0, 8, %l0		! increment size of upper source (US)
	bnz,a	1b
	sll	%i4, 8, %i4		! make room for next byte

	sub	%l5, %l0, %l1		! generate shift left count (LS)
	sll	%i4, %l1, %i4		! prepare to get rest
	ld	[%i0], %i3		! read a word
	add	%i0, 4, %i0		! increment source address
	srl	%i3, %l0, %i5		! upper src bits into lower dst bits
	or	%i4, %i5, %i5		! merge
	mov	24, %l3			! align destination
1:
	srl	%i5, %l3, %i4		! prepare to write a single byte
	stb	%i4, [%i1]		! write a byte
	add	%i1, 1, %i1		! increment destination address
	sub	%i2, 1, %i2		! decrement count
	btst	3, %i1			! is destination aligned?
	bnz,a	1b
	sub	%l3, 8, %l3		! delay slot, decrement shift count (LD)
	sub	%l5, %l3, %l2		! generate shift left count (UD)
	sll	%i5, %l2, %i5		! move leftover into upper bytes
	cmp	%l2, %l0		! cmp # reqd to fill dst w old src left
	bgu	%ncc, .more_needed	! need more to fill than we have
	nop

	sll	%i3, %l1, %i3		! clear upper used byte(s)
	srl	%i3, %l1, %i3
	! get the odd bytes between alignments
	sub	%l0, %l2, %l0		! regenerate shift count
	sub	%l5, %l0, %l1		! generate new shift left count (LS)
	and	%i2, 3, %l4		! must do remaining bytes if count%4 > 0
	andn	%i2, 3, %i2		! # of aligned bytes that can be moved
	srl	%i3, %l0, %i4
	or	%i5, %i4, %i5
	st	%i5, [%i1]		! write a word
	subcc	%i2, 4, %i2		! decrement count
	bz	%ncc, .unalign_out
	add	%i1, 4, %i1		! increment destination address

	b	2f
	sll	%i3, %l1, %i5		! get leftover into upper bits
.more_needed:
	sll	%i3, %l0, %i3		! save remaining byte(s)
	srl	%i3, %l0, %i3
	sub	%l2, %l0, %l1		! regenerate shift count
	sub	%l5, %l1, %l0		! generate new shift left count
	sll	%i3, %l1, %i4		! move to fill empty space
	b	3f
	or	%i5, %i4, %i5		! merge to complete word
	!
	! the source address is aligned and destination is not
	!
.align_dst_only:
	ld	[%i0], %i4		! read a word
	add	%i0, 4, %i0		! increment source address
	mov	24, %l0			! initial shift alignment count
1:
	srl	%i4, %l0, %i3		! prepare to write a single byte
	stb	%i3, [%i1]		! write a byte
	add	%i1, 1, %i1		! increment destination address
	sub	%i2, 1, %i2		! decrement count
	btst	3, %i1			! is destination aligned?
	bnz,a	1b
	sub	%l0, 8, %l0		! delay slot, decrement shift count
.xfer:
	sub	%l5, %l0, %l1		! generate shift left count
	sll	%i4, %l1, %i5		! get leftover
3:
	and	%i2, 3, %l4		! must do remaining bytes if count%4 > 0
	andn	%i2, 3, %i2		! # of aligned bytes that can be moved
2:
	ld	[%i0], %i3		! read a source word
	add	%i0, 4, %i0		! increment source address
	srl	%i3, %l0, %i4		! upper src bits into lower dst bits
	or	%i5, %i4, %i5		! merge with upper dest bits (leftover)
	st	%i5, [%i1]		! write a destination word
	subcc	%i2, 4, %i2		! decrement count
	bz	%ncc, .unalign_out	! check if done
	add	%i1, 4, %i1		! increment destination address
	b	2b			! loop
	sll	%i3, %l1, %i5		! get leftover
.unalign_out:
	tst	%l4			! any bytes leftover?
	bz	%ncc, .cpdone
	.empty				! allow next instruction in delay slot
1:
	sub	%l0, 8, %l0		! decrement shift
	srl	%i3, %l0, %i4		! upper src byte into lower dst byte
	stb	%i4, [%i1]		! write a byte
	subcc	%l4, 1, %l4		! decrement count
	bz	%ncc, .cpdone		! done?
	add	%i1, 1, %i1		! increment destination
	tst	%l0			! any more previously read bytes
	bnz	%ncc, 1b		! we have leftover bytes
	mov	%l4, %i2		! delay slot, mv cnt where dbytecp wants
	b	.dbytecp		! let dbytecp do the rest
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst
	!
	! the destination address is aligned and the source is not
	!
.align_src_only:
	ldub	[%i0], %i3		! read a byte from source address
	add	%i0, 1, %i0		! increment source address
	or	%i4, %i3, %i4		! or in with previous bytes (if any)
	btst	3, %i0			! is source aligned?
	add	%l0, 8, %l0		! increment shift count (US)
	bnz,a	.align_src_only
	sll	%i4, 8, %i4		! make room for next byte
	b,a	.xfer
	!
	! if from address unaligned for double-word moves,
	! move bytes till it is, if count is < 56 it could take
	! longer to align the thing than to do the transfer
	! in word size chunks right away
	!
.aldoubcp:
	cmp	%i2, 56			! if count < 56, use wordcp, it takes
	blu,a	%ncc, .alwordcp		! longer to align doubles than words
	mov	3, %o0			! mask for word alignment
	call	.alignit		! copy bytes until aligned
	mov	7, %o0			! mask for double alignment
	!
	! source and destination are now double-word aligned
	! see if transfer is large enough to gain by loop unrolling
	!
!!	cmp	%i2, 512		! if less than 512 bytes
!!	bgeu,a	.blkcopy		! just copy double-words (overwrite i3)
!!	mov	0x100, %i3		! blk copy chunk size for unrolled loop
	!
	! i3 has aligned count returned by alignit
	!
	and	%i2, 7, %i2		! unaligned leftover count
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst
5:
	ldd	[%i0+%i1], %o4		! read from address
	std	%o4, [%i1]		! write at destination address
	subcc	%i3, 8, %i3		! dec count
	bgu	%ncc, 5b
	add	%i1, 8, %i1		! delay slot, inc to address
	cmp	%i2, 4			! see if we can copy a word
	blu	%ncc, .dbytecp		! if 3 or less bytes use bytecp
	.empty
	!
	! for leftover bytes we fall into wordcp, if needed
	!
.wordcp:
	and	%i2, 3, %i2		! unaligned leftover count
5:
	ld	[%i0+%i1], %o4		! read from address
	st	%o4, [%i1]		! write at destination address
	subcc	%i3, 4, %i3		! dec count
	bgu	%ncc, 5b
	add	%i1, 4, %i1		! delay slot, inc to address
	b,a	.dbytecp

	! we come here to align copies on word boundaries
.alwordcp:
	call	.alignit		! go word-align it
	mov	3, %o0			! bits that must be zero to be aligned
	b	.wordcp
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst

	!
	! byte copy, works with any alignment
	!
.bytecp:
	b	.dbytecp
	sub	%i0, %i1, %i0		! i0 gets difference of src and dst

	!
	! differenced byte copy, works with any alignment
	! assumes dest in %i1 and (source - dest) in %i0
	!
1:
	stb	%o4, [%i1]		! write to address
	inc	%i1			! inc to address
.dbytecp:
	deccc	%i2			! dec count
	bgeu,a	%ncc, 1b		! loop till done
	ldub	[%i0+%i1], %o4		! read from address
.cpdone:
	andn	%l6, 3, %l6
	stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPY_END, TR_copy_end)
	ret
	restore %g0, 0, %o0		! return (0)

/*
 * Common code used to align transfers on word and doubleword
 * boudaries.  Aligns source and destination and returns a count
 * of aligned bytes to transfer in %i3
 */
1:
	inc	%i0			! inc from
	stb	%o4, [%i1]		! write a byte
	inc	%i1			! inc to
	dec	%i2			! dec count
.alignit:
	btst	%o0, %i0		! %o0 is bit mask to check for alignment
	bnz,a	1b
	ldub	[%i0], %o4		! read next byte

	retl
	andn	%i2, %o0, %i3		! return size of aligned bytes
	SET_SIZE(bcopy)

#endif	/* lint */

/*
 * Block copy with possibly overlapped operands.
 */

#if defined(lint)

/* ARGSUSED */
void
ovbcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(ovbcopy)
	tst	%o2			! check count
	bgu,a	%ncc, 1f		! nothing to do or bad arguments
	subcc	%o0, %o1, %o3		! difference of from and to address

	retl				! return
	nop
1:
	bneg,a	%ncc, 2f
	neg	%o3			! if < 0, make it positive
2:	cmp	%o2, %o3		! cmp size and abs(from - to)
	bleu	%ncc, bcopy		! if size <= abs(diff): use bcopy,
	.empty				!   no overlap
	cmp	%o0, %o1		! compare from and to addresses
	blu	%ncc, .ov_bkwd		! if from < to, copy backwards
	nop
	!
	! Copy forwards.
	!
.ov_fwd:
	ldub	[%o0], %o3		! read from address
	inc	%o0			! inc from address
	stb	%o3, [%o1]		! write to address
	deccc	%o2			! dec count
	bgu	%ncc, .ov_fwd		! loop till done
	inc	%o1			! inc to address

	retl				! return
	nop
	!
	! Copy backwards.
	!
.ov_bkwd:
	deccc	%o2			! dec count
	ldub	[%o0 + %o2], %o3	! get byte at end of src
	bgu	%ncc, .ov_bkwd		! loop till done
	stb	%o3, [%o1 + %o2]	! delay slot, store at end of dst

	retl				! return
	nop
	SET_SIZE(ovbcopy)

#endif	/* lint */

/*
 * Zero a block of storage.
 *
 * uzero is used by the kernel to zero a block in user address space.
 */

#if defined(lint)

/* ARGSUSED */
int
kzero(void *addr, size_t count)
{ return(0); }

/* ARGSUSED */
void
uzero(void *addr, size_t count)
{}

#else	/* lint */

	ENTRY(uzero)
	wr	%g0, ASI_USER, %asi
	ldn	[THREAD_REG + T_LOFAULT], %o5
	brz,pt	%o5, .do_zero
	  nop
	set	.zeroerr, %o2
	stn	%o2, [THREAD_REG + T_LOFAULT]	! install new vector
	b	.do_zero
	  or	%o5, 2, %o5

	ENTRY(kzero)
	set	.zeroerr, %o5
	wr	%g0, ASI_P, %asi
	
	ldn	[THREAD_REG + T_LOFAULT], %o4	! install new vector
	stn	%o5, [THREAD_REG + T_LOFAULT]
	b	.do_zero
	   mov	%o4, %o5

/*
 * We got here because of a fault during kzero.
 * Errno value is in %g1.
 */
.zeroerr:
	! if saved t_lofault is odd then clear the %fprs register
	btst	1, %o5
	bz	1f
	  membar #Sync
	wr	%g0, %g0, %fprs		! clear fprs
	andn	%o5, 1, %o5
1:
	btst	2, %o5
	andn	%o5, 2, %o5
	bnz,pn	%ncc, 2f
	  stn	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl				! return
	mov	%g1, %o0		! error code from %g1

2:
	jmp	%o5			! goto real handler
	  nop

	SET_SIZE(kzero)
	SET_SIZE(uzero)

#endif	/* lint */

/*
 * Zero a block of storage.
 */

#if defined(lint)

/* ARGSUSED */
void
bzero(void *addr, size_t count)
{}

#else	/* lint */

	ENTRY(bzero)
	wr	%g0, ASI_P, %asi

	ldn	[THREAD_REG + T_LOFAULT], %o5	! save t_lofault
	brz,pt	%o5, .do_zero
	  nop
	set	.zeroerr, %o2
	stn	%o2, [THREAD_REG + T_LOFAULT]	! install new vector
	or	%o5, 2, %o5		! error should trampoline
	
.do_zero:
	cmp	%o1, 15			! check for small counts
	blu	%ncc, .byteclr		! just clear bytes
	nop

	cmp	%o1, 192		! check for large counts
	blu	%ncc, .bzero_small
	nop

	set	use_hw_bzero, %o2
	ld	[%o2], %o2
	tst	%o2
	bz	.bzero_small
	nop

	rd	%fprs, %o2		! check for unused fp
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o2
	bnz	.bzero_small
	nop

	ldn	[THREAD_REG + T_LWP], %o2
	tst	%o2
	bz	%ncc, .bzero_small
	nop

	! Check for block alignment
	btst	(64-1), %o0
	bz	.bzl_block
	nop

	! Check for double-word alignment
	btst	(8-1), %o0
	bz	.bzl_dword
	nop

	! Check for word alignment
	btst	(4-1), %o0
	bz	.bzl_word
	nop

	! Clear bytes until word aligned
.bzl_byte:
	stba	%g0, [%o0]%asi
	add	%o0, 1, %o0
	btst	(4-1), %o0
	bnz	.bzl_byte
	sub	%o1, 1, %o1

	! Check for dword-aligned
	btst	(8-1), %o0
	bz	.bzl_dword
	nop
	
	! Clear words until double-word aligned
.bzl_word:
	sta	%g0, [%o0]%asi
	add	%o0, 4, %o0
	btst	(8-1), %o0
	bnz	.bzl_word
	sub	%o1, 4, %o1

.bzl_dword:
	! Clear dwords until block aligned
	stxa	%g0, [%o0]%asi
	add	%o0, 8, %o0
	btst	(64-1), %o0
	bnz	.bzl_dword
	sub	%o1, 8, %o1

.bzl_block:
	membar	#StoreStore|#StoreLoad|#LoadStore
	wr	%g0, FPRS_FEF, %fprs

	! Set the lower bit in the saved t_lofault to indicate
	! that we need to clear the %fprs register on the way
	! out
	or	%o5, 1, %o5

	! Clear block
	fzero	%d0
	fzero	%d2
	fzero	%d4
	fzero	%d6
	fzero	%d8
	fzero	%d10
	fzero	%d12
	fzero	%d14
	rd	%asi, %o3
	wr	%g0, ASI_BLK_P, %asi
	cmp	%o3, ASI_P
	bne,a	1f
	wr	%g0, ASI_BLK_AIUS, %asi
1:	
	mov	256, %o3
	ba	.bzl_doblock
	nop

.bzl_blkstart:	
      ! stda	%d0, [%o0+192]%asi  ! in dly slot of branch that got us here
	stda	%d0, [%o0+128]%asi
	stda	%d0, [%o0+64]%asi
	stda	%d0, [%o0]%asi
.bzl_zinst:
	add	%o0, %o3, %o0
	sub	%o1, %o3, %o1
.bzl_doblock:
	cmp	%o1, 256
	bgeu,a	%ncc, .bzl_blkstart
	stda	%d0, [%o0+192]%asi

	cmp	%o1, 64
	blu	%ncc, .bzl_finish
	
	andn	%o1, (64-1), %o3
	srl	%o3, 4, %o2		! using blocks, 1 instr / 16 words
	set	.bzl_zinst, %o4
	sub	%o4, %o2, %o4
	jmp	%o4
	nop

.bzl_finish:
	membar	#StoreLoad|#StoreStore
	wr	%g0, %g0, %fprs
	andn	%o5, 1, %o5

	rd	%asi, %o4
	wr	%g0, ASI_P, %asi
	cmp	%o4, ASI_BLK_P
	bne,a	1f
	wr	%g0, ASI_USER, %asi
1:

.bzlf_dword:
	! double words
	cmp	%o1, 8
	blu	%ncc, .bzlf_word
	nop
	stxa	%g0, [%o0]%asi
	add	%o0, 8, %o0
	sub	%o1, 8, %o1
	ba	.bzlf_dword
	nop

.bzlf_word:
	! words
	cmp	%o1, 4
	blu	%ncc, .bzlf_byte
	nop
	sta	%g0, [%o0]%asi
	add	%o0, 4, %o0
	sub	%o1, 4, %o1
	ba	.bzlf_word
	nop

1:
	add	%o0, 1, %o0		! increment address
.bzlf_byte:
	subcc	%o1, 1, %o1		! decrement count
	bgeu,a	%ncc, 1b
	stba	%g0, [%o0]%asi		! zero a byte

.bzlf_finished:
	andn	%o5, 3, %o5
	stn	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl
	clr	%o0			! return (0)

.bzero_small:

	!
	! Check for word alignment.
	!
	btst	3, %o0
	bz	.bzero_probe
	mov	0x100, %o3		! constant size of main loop
	!
	!
	! clear bytes until word aligned
	!
1:	stba	%g0,[%o0]%asi
	add	%o0, 1, %o0
	btst	3, %o0
	bnz	1b
	sub	%o1, 1, %o1
.bzero_probe:

	!
	! obmem, if needed move a word to become double-word aligned.
	!
.bzero_obmem:
	btst	7, %o0			! is double aligned?
	bz	.bzero_nobuf
	sta	%g0, [%o0]%asi		! clr to double boundry
	sub	%o1, 4, %o1
	b	.bzero_nobuf
	add	%o0, 4, %o0

	!stxa	%g0, [%o0+0xf8]
.bzero_blk:
	stxa	%g0, [%o0+0xf0]%asi
	stxa	%g0, [%o0+0xe8]%asi
	stxa	%g0, [%o0+0xe0]%asi
	stxa	%g0, [%o0+0xd8]%asi
	stxa	%g0, [%o0+0xd0]%asi
	stxa	%g0, [%o0+0xc8]%asi
	stxa	%g0, [%o0+0xc0]%asi
	stxa	%g0, [%o0+0xb8]%asi
	stxa	%g0, [%o0+0xb0]%asi
	stxa	%g0, [%o0+0xa8]%asi
	stxa	%g0, [%o0+0xa0]%asi
	stxa	%g0, [%o0+0x98]%asi
	stxa	%g0, [%o0+0x90]%asi
	stxa	%g0, [%o0+0x88]%asi
	stxa	%g0, [%o0+0x80]%asi
	stxa	%g0, [%o0+0x78]%asi
	stxa	%g0, [%o0+0x70]%asi
	stxa	%g0, [%o0+0x68]%asi
	stxa	%g0, [%o0+0x60]%asi
	stxa	%g0, [%o0+0x58]%asi
	stxa	%g0, [%o0+0x50]%asi
	stxa	%g0, [%o0+0x48]%asi
	stxa	%g0, [%o0+0x40]%asi
	stxa	%g0, [%o0+0x38]%asi
	stxa	%g0, [%o0+0x30]%asi
	stxa	%g0, [%o0+0x28]%asi
	stxa	%g0, [%o0+0x20]%asi
	stxa	%g0, [%o0+0x18]%asi
	stxa	%g0, [%o0+0x10]%asi
	stxa	%g0, [%o0+0x08]%asi
	stxa	%g0, [%o0]%asi
.zinst:
	add	%o0, %o3, %o0		! increment source address
	sub	%o1, %o3, %o1		! decrement count
.bzero_nobuf:
	cmp	%o1, 0x100		! can we do whole chunk?
	bgeu,a	%ncc, .bzero_blk
	stxa	%g0, [%o0+0xf8]%asi	! do first double of chunk

	cmp	%o1, 7			! can we zero any more double words
	bleu	%ncc, .byteclr		! too small go zero bytes

	andn	%o1, 7, %o3		! %o3 bytes left, double-word aligned
	srl	%o3, 1, %o2		! using doubles, need 1 instr / 2 words
	set	.zinst, %o4		! address of clr instructions
	sub	%o4, %o2, %o4		! jmp address relative to instr
	jmp	%o4
	nop
	!
	! do leftover bytes
	!
3:
	add	%o0, 1, %o0		! increment address
.byteclr:
	subcc	%o1, 1, %o1		! decrement count
	bgeu,a	%ncc, 3b
	stba	%g0, [%o0]%asi		! zero a byte

.bzero_finished:
	andn	%o5, 3, %o5
	stn	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl
	clr	%o0			! return (0)

	SET_SIZE(bzero)
#endif	/* lint */


/*
 * hwblkclr - clears block-aligned, block-multiple-sized regions that are
 * longer than 256 bytes in length using spitfire's block stores.  If
 * the criteria for using this routine are not met then it calls bzero
 * and returns 1.  Otherwise 0 is returned indicating success.
 * Caller is responsible for ensuring use_hw_bzero is true and that
 * kpreempt_disable() has been called.
 */
#ifdef lint
/*ARGSUSED*/
int
hwblkclr(void *addr, size_t len)
{ 
	return(0);
}
#else /* lint */
	! %i0 - start address
	! %i1 - length of region (multiple of 64)
	! %l0 - saved fprs
	! %l1 - pointer to saved %d0 block
	! %l2 - saved curthread->t_lwp

	ENTRY(hwblkclr)
	! get another window w/space for one aligned block of saved fpregs
	save	%sp, -SA(MINFRAME + 2*64), %sp

	! Must be block-aligned
	andcc	%i0, (64-1), %g0
	bnz,pn	%ncc, 1f
	  nop

	! ... and must be 256 bytes or more
	cmp	%i1, 256
	blu,pn	%ncc, 1f
	  nop

	! ... and length must be a multiple of 64
	andcc	%i1, (64-1), %g0
	bz,pn	%ncc, 2f
	  nop

1:	! punt, call bzero but notify the caller that bzero was used
	mov	%i0, %o0
	call	bzero
	  mov	%i1, %o1
	ret
	restore	%g0, 1, %o0	! return (1) - did not use block operations

2:	rd	%fprs, %l0		! check for unused fp
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %l0
	bz	1f
	  nop

	! save in-use fpregs on stack
	membar	#Sync
	add	%fp, STACK_BIAS - 65, %l1
	and	%l1, -64, %l1
	stda	%d0, [%l1]ASI_BLK_P

1:	membar	#StoreStore|#StoreLoad|#LoadStore
	wr	%g0, FPRS_FEF, %fprs
	wr	%g0, ASI_BLK_P, %asi

	! Clear block
	fzero	%d0
	fzero	%d2
	fzero	%d4
	fzero	%d6
	fzero	%d8
	fzero	%d10
	fzero	%d12
	fzero	%d14

	mov	256, %i3
	ba	.pz_doblock
	  nop

.pz_blkstart:	
      ! stda	%d0, [%i0+192]%asi  ! in dly slot of branch that got us here
	stda	%d0, [%i0+128]%asi
	stda	%d0, [%i0+64]%asi
	stda	%d0, [%i0]%asi
.pz_zinst:
	add	%i0, %i3, %i0
	sub	%i1, %i3, %i1
.pz_doblock:
	cmp	%i1, 256
	bgeu,a	%ncc, .pz_blkstart
	  stda	%d0, [%i0+192]%asi

	cmp	%i1, 64
	blu	%ncc, .pz_finish
	
	andn	%i1, (64-1), %i3
	srl	%i3, 4, %i2		! using blocks, 1 instr / 16 words
	set	.pz_zinst, %i4
	sub	%i4, %i2, %i4
	jmp	%i4
	  nop

.pz_finish:
	membar	#Sync
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %l0
	bz,a	.pz_finished
	  wr	%l0, 0, %fprs		! restore fprs

	! restore fpregs from stack
	ldda	[%l1]ASI_BLK_P, %d0
	membar	#Sync
	wr	%l0, 0, %fprs		! restore fprs

.pz_finished:
	ret
	restore	%g0, 0, %o0		! return (bzero or not)
	SET_SIZE(hwblkclr)
#endif	/* lint */

/*
 * hwblkpagecopy()
 *
 * Copies exactly one page.  This routine assumes the caller (ppcopy)
 * has already disabled kernel preemption and has checked
 * use_hw_bcopy.
 */
#ifdef lint
/*ARGSUSED*/
void
hwblkpagecopy(const void *src, void *dst)
{ }
#else /* lint */
	ENTRY(hwblkpagecopy)
	! get another window w/space for three aligned blocks of saved fpregs
	save	%sp, -SA(MINFRAME + 4*64), %sp

	! %i0 - source address (arg)
	! %i1 - destination address (arg)
	! %i2 - length of region (not arg)
	! %l0 - saved fprs
	! %l1 - pointer to saved fpregs

	rd	%fprs, %l0		! check for unused fp
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %l0
	bz	1f
	membar #Sync

	! save in-use fpregs on stack
	add	%fp, STACK_BIAS - 193, %l1
	and	%l1, -64, %l1
	stda	%d0, [%l1]ASI_BLK_P
	add	%l1, 64, %l3
	stda	%d16, [%l3]ASI_BLK_P
	add	%l3, 64, %l3
	stda	%d32, [%l3]ASI_BLK_P
	membar #Sync

1:	wr	%g0, FPRS_FEF, %fprs
	ldda	[%i0]ASI_BLK_P, %d0
	add	%i0, 64, %i0
	set	PAGESIZE - 64, %i2

2:	ldda	[%i0]ASI_BLK_P, %d16
	fsrc1	%d0, %d32
	fsrc1	%d2, %d34
	fsrc1	%d4, %d36
	fsrc1	%d6, %d38
	fsrc1	%d8, %d40
	fsrc1	%d10, %d42
	fsrc1	%d12, %d44
	fsrc1	%d14, %d46
	stda	%d32, [%i1]ASI_BLK_P
	add	%i0, 64, %i0
	subcc	%i2, 64, %i2
	bz,pn	%ncc, 3f
	add	%i1, 64, %i1
	ldda	[%i0]ASI_BLK_P, %d0
	fsrc1	%d16, %d32
	fsrc1	%d18, %d34
	fsrc1	%d20, %d36
	fsrc1	%d22, %d38
	fsrc1	%d24, %d40
	fsrc1	%d26, %d42
	fsrc1	%d28, %d44
	fsrc1	%d30, %d46
	stda	%d32, [%i1]ASI_BLK_P
	add	%i0, 64, %i0
	sub	%i2, 64, %i2
	ba,pt	%ncc, 2b
	add	%i1, 64, %i1

3:	membar	#Sync
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %l0
	bz	4f
	stda	%d16, [%i1]ASI_BLK_P

	! restore fpregs from stack
	membar	#Sync
	ldda	[%l1]ASI_BLK_P, %d0
	add	%l1, 64, %l3
	ldda	[%l3]ASI_BLK_P, %d16
	add	%l3, 64, %l3
	ldda	[%l3]ASI_BLK_P, %d32

4:
	wr	%l0, 0, %fprs		! restore fprs
	membar #Sync
	ret
	restore	%g0, 0, %o0
	SET_SIZE(hwblkpagecopy)
#endif	/* lint */

	
/*
 * Copy a null terminated string from one point to another in
 * the kernel address space.
 * NOTE - don't use %o5 in this routine as copy{in,out}str uses it.
 *
 * copystr(from, to, maxlength, lencopied)
 *	caddr_t from, to;
 *	u_int maxlength, *lencopied;
 */

#if defined(lint)

/* ARGSUSED */
int
copystr(const char *from, char *to, size_t maxlength, size_t *lencopied)
{ return(0); }

#else	/* lint */

	ENTRY(copystr)
	ldn	[THREAD_REG + T_LOFAULT], %o5	! set up for .cs_out
	orcc	%o2, %g0, %o4		! save original count
	bg,a	%ncc, 1f
	  sub	%o0, %o1, %o0		! o0 gets the difference of src and dst

	!
	! maxlength <= 0
	!
	bz	%ncc, .cs_out		! maxlength = 0
	mov	ENAMETOOLONG, %o0

	retl				! maxlength < 0
	mov	EFAULT, %o0		! return failure

	!
	! Do a byte by byte loop.
	! We do this instead of a word by word copy because most strings
	! are small and this takes a small number of cache lines.
	!
0:
	stb	%g1, [%o1]		! store byte
	tst	%g1			! null byte?
	bnz	1f
	add	%o1, 1, %o1		! incr dst addr

	b	.cs_out			! last byte in string
	mov	0, %o0			! ret code = 0
1:
	subcc	%o2, 1, %o2		! test count
	bgeu,a	%ncc, 0b
	ldub	[%o0+%o1], %g1		! delay slot, get source byte

	mov	0, %o2			! max number of bytes moved
	mov	ENAMETOOLONG, %o0	! ret code = ENAMETOOLONG
.cs_out:
	tst	%o3			! want length?
	bz	%ncc, 2f
	sub	%o4, %o2, %o4		! compute length and store it
	stn	%o4, [%o3]
2:
	retl
	stn	%o5, [THREAD_REG + T_LOFAULT]	! stop catching faults
	SET_SIZE(copystr)

#endif	/* lint */

/*
 * Transfer data to and from user space -
 * Note that these routines can cause faults
 * It is assumed that the kernel has nothing at
 * less than KERNELBASE in the virtual address space.
 *
 * Note that copyin(9F) and copyout(9F) are part of the
 * DDI/DKI which specifies that they return '-1' on "errors."
 *
 * Sigh.
 *
 * So there's two extremely similar routines - xcopyin() and xcopyout()
 * which return the errno that we've faithfully computed.  This
 * allows other callers (e.g. uiomove(9F)) to work correctly.
 * Given that these are used pretty heavily, we expand the calling
 * sequences inline for all flavours (rather than making wrappers).
 *
 * There are also stub routines for xcopyout_little and xcopyin_little,
 * which currently are intended to handle requests of <= 16 bytes from
 * do_unaligned. Future enhancement to make them handle 8k pages efficiently
 * is left as an exercise...
 */

/*
 * Copy kernel data to user space (copyout/xcopyout/xcopyout_little).
 */

#if defined(lint)

/*ARGSUSED*/
int
default_copyout(const void *kaddr, void *uaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyout)

	TRACE_IN(TR_COPYOUT_START, TR_copyout_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	set	.copyioerr, %l6		! .copyioerr is lofault value
	ldn	[THREAD_REG + T_LOFAULT], %l7	! save existing handler
	stn	%l6, [THREAD_REG + T_LOFAULT]	! set t_lofault
	mov	%l7, %l6

.do_copyout:
	cmp	%i2, HW_THRESHOLD	! for large counts
	blu	%ncc, .small_copyout
	  .empty

	set	use_hw_copyio, %o2
	ld	[%o2], %o2
	tst	%o2
	bz	.small_copyout
	  nop

	ldn	[THREAD_REG + T_LWP], %o3
	tst	%o3
	bnz,pt	%ncc, 1f
	  nop

	! kpreempt_disable();
	ldsb	[THREAD_REG + T_PREEMPT], %o2
	inc	%o2
	stb	%o2, [THREAD_REG + T_PREEMPT]

1:
	rd	%fprs, %o2		! check for unused fp
	st	%o2, [%fp + STACK_BIAS - SAVED_FPRS_OFFSET]	! save orig %fprs
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o2
	bz,a	.do_blockcopyout
	  wr	%g0, FPRS_FEF, %fprs

.copyout_fpregs_inuse:
	cmp	%i2, HW_THRESHOLD+(64*4) ! for large counts (larger
	bgeu	%ncc, 1f		!  if we have to save the fpregs)
	  nop

	tst	%o3
	bnz,pt	%ncc, .small_copyout
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%xcc, .small_copyout
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%icc, .small_copyout
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

	ba,a	.small_copyout
	  nop
1:
	wr	%g0, FPRS_FEF, %fprs

	! save in-use fpregs on stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	stda	%d0, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d16, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d32, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d48, [%o2]ASI_BLK_P
	membar	#Sync

.do_blockcopyout:
	membar	#StoreStore|#StoreLoad|#LoadStore

	rd	%gsr, %o2
	st	%o2, [%fp + STACK_BIAS - SAVED_GSR_OFFSET]	! save gsr

	! Set the lower bit in the saved t_lofault to indicate
	! that we need to clear the %fprs register on the way
	! out
	or	%l6, 1, %l6

	! Swap src/dst since the code below is memcpy code
	! and memcpy/bcopy have different calling sequences
	mov	%i1, %i5
	mov	%i0, %i1
	mov	%i5, %i0

!!! This code is nearly identical to the version in the sun4u
!!! libc_psr.  Most bugfixes made to that file should be
!!! merged into this routine.

	andcc	%i0, 7, %o3
	bz	%ncc, copyout_blkcpy
	sub	%o3, 8, %o3
	neg	%o3
	sub	%i2, %o3, %i2

	! Align Destination on double-word boundary

2:	ldub	[%i1], %o4
	inc	%i1
	stba	%o4, [%i0]ASI_USER
	deccc	%o3
	bgu	%ncc, 2b
	  inc	%i0
copyout_blkcpy:
	andcc	%i0, 63, %i3
	bz,pn	%ncc, copyout_blalign	! now block aligned
	sub	%i3, 64, %i3
	neg	%i3			! bytes till block aligned
	sub	%i2, %i3, %i2		! update %i2 with new count

	! Copy %i3 bytes till dst is block (64 byte) aligned. use
	! double word copies.

	alignaddr %i1, %g0, %g1
	ldd	[%g1], %d0
	add	%g1, 8, %g1
6:
	ldd	[%g1], %d2
	add	%g1, 8, %g1
	subcc	%i3, 8, %i3
	faligndata %d0, %d2, %d8
	stda	 %d8, [%i0]ASI_USER
	add	%i1, 8, %i1
	bz,pn	%ncc, copyout_blalign
	add	%i0, 8, %i0
	ldd	[%g1], %d0
	add	%g1, 8, %g1
	subcc	%i3, 8, %i3
	faligndata %d2, %d0, %d8
	stda	 %d8, [%i0]ASI_USER
	add	%i1, 8, %i1
	bgu,pn	%ncc, 6b
	add	%i0, 8, %i0
 
copyout_blalign:
	membar	#StoreLoad
	! %i2 = total length
	! %i3 = blocks	(length - 64) / 64
	! %i4 = doubles remaining  (length - blocks)
	sub	%i2, 64, %i3
	andn	%i3, 63, %i3
	sub	%i2, %i3, %i4
	andn	%i4, 7, %i4
	sub	%i4, 16, %i4
	sub	%i2, %i4, %i2
	sub	%i2, %i3, %i2

	andn	%i1, 0x3f, %l7		! blk aligned address
	alignaddr %i1, %g0, %g0		! gen %gsr

	srl	%i1, 3, %l5		! bits 3,4,5 are now least sig in  %l5
	andcc	%l5, 7, %i5		! mask everything except bits 1,2 3
	add	%i1, %i4, %i1
	add	%i1, %i3, %i1

	ldda	[%l7]ASI_BLK_P, %d0
	add	%l7, 64, %l7
	ldda	[%l7]ASI_BLK_P, %d16
	add	%l7, 64, %l7
	ldda	[%l7]ASI_BLK_P, %d32
	add	%l7, 64, %l7
	sub	%i3, 128, %i3

	! switch statement to get us to the right 8 byte blk within a
	! 64 byte block

	cmp	 %i5, 4
	bgeu,a	 copyout_hlf
	cmp	 %i5, 6
	cmp	 %i5, 2
	bgeu,a	 copyout_sqtr
	nop
	cmp	 %i5, 1
	be,a	 copyout_seg1
	nop
	ba	 copyout_seg0
	nop
copyout_sqtr:
	be,a	 copyout_seg2
	nop
	ba,a	 copyout_seg3
	nop

copyout_hlf:
	bgeu,a	 copyout_fqtr
	nop	 
	cmp	 %i5, 5
	be,a	 copyout_seg5
	nop
	ba	 copyout_seg4
	nop
copyout_fqtr:
	be,a	 copyout_seg6
	nop
	ba	 copyout_seg7
	nop
	
copyout_seg0:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D0
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D16
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D32
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg0

0:
	FALIGN_D16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D32
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd0
	add	%i0, 64, %i0

1:
	FALIGN_D32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D0
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd16
	add	%i0, 64, %i0

2:
	FALIGN_D0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D16
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd32
	add	%i0, 64, %i0

copyout_seg1:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D2
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D18
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D34
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg1
0:
	FALIGN_D18
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D34
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd2
	add	%i0, 64, %i0

1:
	FALIGN_D34
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D2
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd18
	add	%i0, 64, %i0

2:
	FALIGN_D2
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D18
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd34
	add	%i0, 64, %i0

copyout_seg2:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D4
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D20
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D36
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg2

0:
	FALIGN_D20
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D36
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd4
	add	%i0, 64, %i0

1:
	FALIGN_D36
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D4
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd20
	add	%i0, 64, %i0

2:
	FALIGN_D4
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D20
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd36
	add	%i0, 64, %i0

copyout_seg3:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D6
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D22
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D38
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg3

0:
	FALIGN_D22
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D38
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd6
	add	%i0, 64, %i0

1:
	FALIGN_D38
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D6
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd22
	add	%i0, 64, %i0

2:
	FALIGN_D6
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D22
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd38
	add	%i0, 64, %i0

copyout_seg4:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D8
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D24
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D40
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg4

0:
	FALIGN_D24
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D40
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd8
	add	%i0, 64, %i0

1:
	FALIGN_D40
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D8
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd24
	add	%i0, 64, %i0

2:
	FALIGN_D8
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D24
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd40
	add	%i0, 64, %i0

copyout_seg5:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D10
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D26
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D42
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg5

0:
	FALIGN_D26
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D42
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd10
	add	%i0, 64, %i0

1:
	FALIGN_D42
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D10
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd26
	add	%i0, 64, %i0

2:
	FALIGN_D10
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D26
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd42
	add	%i0, 64, %i0

copyout_seg6:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D12
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D28
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D44
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg6

0:
	FALIGN_D28
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D44
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd12
	add	%i0, 64, %i0

1:
	FALIGN_D44
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D12
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd28
	add	%i0, 64, %i0

2:
	FALIGN_D12
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D28
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd44
	add	%i0, 64, %i0

copyout_seg7:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D14
	ldda	[%l7]ASI_BLK_P, %d0
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D30
	ldda	[%l7]ASI_BLK_P, %d16
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D46
	ldda	[%l7]ASI_BLK_P, %d32
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyout_seg7

0:
	FALIGN_D30
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D46
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd14
	add	%i0, 64, %i0

1:
	FALIGN_D46
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D14
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd30
	add	%i0, 64, %i0

2:
	FALIGN_D14
	stda	%d48, [%i0]ASI_BLK_AIUS
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D30
	stda	%d48, [%i0]ASI_BLK_AIUS
	ba,pt	%ncc, copyout_blkd46
	add	%i0, 64, %i0


	!
	! dribble out the last partial block
	!
copyout_blkd0:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d0, %d2, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd2:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d2, %d4, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd4:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d4, %d6, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd6:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d6, %d8, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd8:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d8, %d10, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd10:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d10, %d12, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd12:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d12, %d14, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd14:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	fsrc1	%d14, %d0
	ba,a,pt	%ncc, copyout_blkleft

copyout_blkd16:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d16, %d18, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd18:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d18, %d20, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd20:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d20, %d22, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd22:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d22, %d24, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd24:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d24, %d26, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd26:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d26, %d28, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd28:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d28, %d30, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd30:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	fsrc1	%d30, %d0
	ba,a,pt	%ncc, copyout_blkleft
copyout_blkd32:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d32, %d34, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd34:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d34, %d36, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd36:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d36, %d38, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd38:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d38, %d40, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd40:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d40, %d42, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd42:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d42, %d44, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd44:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	faligndata %d44, %d46, %d48
	stda	%d48, [%i0]ASI_USER
	add	%i0, 8, %i0
copyout_blkd46:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyout_blkdone
	fsrc1	%d46, %d0

copyout_blkleft:
1:
	ldd	[%l7], %d2
	add	%l7, 8, %l7
	subcc	%i4, 8, %i4
	faligndata %d0, %d2, %d8
	stda	%d8, [%i0]ASI_USER
	blu,pn	%ncc, copyout_blkdone
	add	%i0, 8, %i0
	ldd	[%l7], %d0
	add	%l7, 8, %l7
	subcc	%i4, 8, %i4
	faligndata %d2, %d0, %d8
	stda	%d8, [%i0]ASI_USER
	bgeu,pt	%ncc, 1b
	add	%i0, 8, %i0

copyout_blkdone:
	tst	%i2
	bz,pt	%ncc, .copyout_exit
	and	%l3, 0x4, %l3		! fprs.du = fprs.dl = 0

7:	ldub	[%i1], %i4
	inc	%i1
	stba	%i4, [%i0]ASI_USER
	inc	%i0
	deccc	%i2
	bgu	%ncc, 7b
	  nop

.copyout_exit:
	membar	#StoreLoad|#StoreStore
	btst	1, %l6
	bz	1f
	  andn	%l6, 3, %l6

	ld	[%fp + STACK_BIAS - SAVED_GSR_OFFSET], %o2
	wr	%o2, 0, %gsr		! restore gsr

	ld	[%fp + STACK_BIAS - SAVED_FPRS_OFFSET], %o3
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o3
	bz	4f
	  nop

	! restore fpregs from stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d16
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d32
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d48
	membar	#Sync

	ba	2f
	  wr	%o3, 0, %fprs		! restore fprs

4:
	FZERO				! zero all of the fpregs
	wr	%o3, 0, %fprs		! restore fprs

2:	ldn	[THREAD_REG + T_LWP], %o2
	tst	%o2
	bnz,pt	%ncc, 1f
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%ncc, 1f
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%ncc, 1f
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

1:
	stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g0, 0, %o0

.small_copyout:
	subcc	%g0, %i2, %i3
	add	%i0, %i2, %i0
	bz	%ncc, 2f
	add	%i1, %i2, %i1
	ldub	[%i0+%i3], %i4
1:	stba	%i4, [%i1+%i3]ASI_USER
	inccc	%i3
	bcc,a,pt %ncc, 1b
	  ldub	[%i0+%i3], %i4
2:
	andn	%l6, 3, %l6
	stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore %g0, 0, %o0		! return (0)

/*
 * We got here because of a fault during copy{in,out}.
 * Errno value is in %g1, but DDI/DKI says return -1 (sigh).
 */
.copyioerr:
	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPY_FAULT, TR_copyio_fault);
	btst	1, %l6
	bz	1f
	  andn	%l6, 1, %l6

	membar	#Sync

	ld	[%fp + STACK_BIAS - SAVED_GSR_OFFSET], %o2
	wr	%o2, 0, %gsr		! restore gsr

	ld	[%fp + STACK_BIAS - SAVED_FPRS_OFFSET], %o3
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o3
	bz	4f
	  nop

	! restore fpregs from stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d16
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d32
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d48
	membar	#Sync

	ba	2f
	  wr	%o3, 0, %fprs		! restore fprs

4:
	FZERO				! zero all of the fpregs
	wr	%o3, 0, %fprs		! restore fprs

2:	ldn	[THREAD_REG + T_LWP], %o2
	tst	%o2
	bnz,pt	%ncc, 1f
	  nop
				   
	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%ncc, 1f
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%ncc, 1f
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		! pass %pil

1:
	btst	2, %l6			! trampoline?
	andn	%l6, 2, %l6
	bnz,pn	%ncc, 3f
	  stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g0, -1, %o0		! return DDI_FAILURE

3:
	jmp	%l6				! goto real handler
	  restore	%g0, 0, %o0		! dispose of copy window
	SET_SIZE(default_copyout)

#endif	/* lint */


#ifdef	lint

/*ARGSUSED*/
int
default_xcopyout(const void *kaddr, void *uaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_xcopyout)

	TRACE_IN(TR_COPYOUT_START, TR_copyout_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	set	.xcopyioerr, %l6	! .xcopyioerr is lofault value
	ldn	[THREAD_REG + T_LOFAULT], %l7	! save existing handler
	stn	%l6, [THREAD_REG + T_LOFAULT]	! set t_lofault
	b	.do_copyout		! common code
	   mov	%l7, %l6

	SET_SIZE(default_xcopyout)

#endif	/* lint */
	
#ifdef	lint

/*ARGSUSED*/
int
xcopyout_little(const void *kaddr, void *uaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(xcopyout_little)
	set	.xcopyio_err, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o4
	stn	%o5, [THREAD_REG + T_LOFAULT]
	mov	%o4, %o5

	subcc	%g0, %o2, %o3
	add	%o0, %o2, %o0
	bz,pn	%ncc, 2f		! check for zero bytes
	sub	%o2, 1, %o4
	add	%o0, %o4, %o0		! start w/last byte
	add	%o1, %o2, %o1
	ldub	[%o0+%o3], %o4

1:	stba	%o4, [%o1+%o3]ASI_AIUSL
	inccc	%o3
	sub	%o0, 2, %o0		! get next byte
	bcc,a,pt %ncc, 1b
	  ldub	[%o0+%o3], %o4

2:	stn	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl
	mov	%g0, %o0		! return (0)
	SET_SIZE(xcopyout_little)

#endif	/* lint */

/*
 * Copy user data to kernel space (copyin/xcopyin/xcopyin_little)
 */

#if defined(lint)

/*ARGSUSED*/
int
default_copyin(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyin)

	TRACE_IN(TR_COPYIN_START, TR_copyin_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	set	.copyioerr, %l6		! .copyioerr is lofault value
	ldn	[THREAD_REG + T_LOFAULT], %l7	! set/save t_lofault, no tramp
	stn	%l6, [THREAD_REG + T_LOFAULT]
	mov	%l7, %l6

.do_copyin:
	cmp	%i2, HW_THRESHOLD	! for large counts
	blu	%ncc, .small_copyin
	  .empty

	set	use_hw_copyio, %o2
	ld	[%o2], %o2
	tst	%o2
	bz	.small_copyin
	  nop

	ldn	[THREAD_REG + T_LWP], %o3
	tst	%o3
	bnz,pt	%ncc, 1f
	  nop

	! kpreempt_disable();
	ldsb	[THREAD_REG + T_PREEMPT], %o2
	inc	%o2
	stb	%o2, [THREAD_REG + T_PREEMPT]

1:
	rd	%fprs, %o2		! check for unused fp
	st	%o2, [%fp + STACK_BIAS - SAVED_FPRS_OFFSET]	! save orig %fprs
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o2
	bz,a	.do_blockcopyin
	  wr	%g0, FPRS_FEF, %fprs

.copyin_fpregs_inuse:
	cmp	%i2, HW_THRESHOLD+(64*4) ! for large counts (larger
	bgeu	%ncc, 1f		!  if we have to save the fpregs)
	  nop

	tst	%o3
	bnz,pt	%ncc, .small_copyin
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%xcc, .small_copyin
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%icc, .small_copyin
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

	ba,a	.small_copyin
	  nop
1:
	wr	%g0, FPRS_FEF, %fprs

	! save in-use fpregs on stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	stda	%d0, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d16, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d32, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d48, [%o2]ASI_BLK_P
	membar	#Sync

.do_blockcopyin:
	membar	#StoreStore|#StoreLoad|#LoadStore

	rd	%gsr, %o2
	st	%o2, [%fp + STACK_BIAS - SAVED_GSR_OFFSET]	! save gsr

	! Set the lower bit in the saved t_lofault to indicate
	! that we need to clear the %fprs register on the way
	! out
	or	%l6, 1, %l6

	! Swap src/dst since the code below is memcpy code
	! and memcpy/bcopy have different calling sequences
	mov	%i1, %i5
	mov	%i0, %i1
	mov	%i5, %i0

!!! This code is nearly identical to the version in the sun4u
!!! libc_psr.  Most bugfixes made to that file should be
!!! merged into this routine.

	andcc	%i0, 7, %o3
	bz	copyin_blkcpy
	sub	%o3, 8, %o3
	neg	%o3
	sub	%i2, %o3, %i2

	! Align Destination on double-word boundary

2:	lduba	[%i1]ASI_USER, %o4
	inc	%i1
	inc	%i0
	deccc	%o3
	bgu	%ncc, 2b
	stb	%o4, [%i0-1]
copyin_blkcpy:
	andcc	%i0, 63, %i3
	bz,pn	%ncc, copyin_blalign	! now block aligned
	sub	%i3, 64, %i3
	neg	%i3			! bytes till block aligned
	sub	%i2, %i3, %i2		! update %i2 with new count

	! Copy %i3 bytes till dst is block (64 byte) aligned. use
	! double word copies.

	alignaddr %i1, %g0, %g1
	ldda	[%g1]ASI_USER, %d0
	add	%g1, 8, %g1
6:
	ldda	[%g1]ASI_USER, %d2
	add	%g1, 8, %g1
	subcc	%i3, 8, %i3
	faligndata %d0, %d2, %d8
	std	%d8, [%i0]
	add	%i1, 8, %i1
	bz,pn	%ncc, copyin_blalign
	add	%i0, 8, %i0
	ldda	[%g1]ASI_USER, %d0
	add	%g1, 8, %g1
	subcc	%i3, 8, %i3
	faligndata %d2, %d0, %d8
	std	%d8, [%i0]
	add	%i1, 8, %i1
	bgu,pn	%ncc, 6b
	add	%i0, 8, %i0
 
copyin_blalign:
	membar	#StoreLoad
	! %i2 = total length
	! %i3 = blocks	(length - 64) / 64
	! %i4 = doubles remaining  (length - blocks)
	sub	%i2, 64, %i3
	andn	%i3, 63, %i3
	sub	%i2, %i3, %i4
	andn	%i4, 7, %i4
	sub	%i4, 16, %i4
	sub	%i2, %i4, %i2
	sub	%i2, %i3, %i2

	andn	%i1, 0x3f, %l7		! blk aligned address
	alignaddr %i1, %g0, %g0		! gen %gsr

	srl	%i1, 3, %l5		! bits 3,4,5 are now least sig in  %l5
	andcc	%l5, 7, %i5		! mask everything except bits 1,2 3
	add	%i1, %i4, %i1
	add	%i1, %i3, %i1

	ldda	[%l7]ASI_BLK_AIUS, %d0
	add	%l7, 64, %l7
	ldda	[%l7]ASI_BLK_AIUS, %d16
	add	%l7, 64, %l7
	ldda	[%l7]ASI_BLK_AIUS, %d32
	add	%l7, 64, %l7
	sub	%i3, 128, %i3

	! switch statement to get us to the right 8 byte blk within a
	! 64 byte block

	cmp	 %i5, 4
	bgeu,a	 copyin_hlf
	cmp	 %i5, 6
	cmp	 %i5, 2
	bgeu,a	 copyin_sqtr
	nop
	cmp	 %i5, 1
	be,a	 copyin_seg1
	nop
	ba	 copyin_seg0
	nop
copyin_sqtr:
	be,a	 copyin_seg2
	nop
	ba,a	 copyin_seg3
	nop

copyin_hlf:
	bgeu,a	 copyin_fqtr
	nop	 
	cmp	 %i5, 5
	be,a	 copyin_seg5
	nop
	ba	 copyin_seg4
	nop
copyin_fqtr:
	be,a	 copyin_seg6
	nop
	ba	 copyin_seg7
	nop
	
copyin_seg0:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D0
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D16
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D32
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg0

0:
	FALIGN_D16
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D32
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd0
	add	%i0, 64, %i0

1:
	FALIGN_D32
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D0
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd16
	add	%i0, 64, %i0

2:
	FALIGN_D0
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D16
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd32
	add	%i0, 64, %i0

copyin_seg1:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D2
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D18
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D34
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg1
0:
	FALIGN_D18
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D34
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd2
	add	%i0, 64, %i0

1:
	FALIGN_D34
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D2
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd18
	add	%i0, 64, %i0

2:
	FALIGN_D2
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D18
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd34
	add	%i0, 64, %i0
copyin_seg2:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D4
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D20
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D36
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg2

0:
	FALIGN_D20
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D36
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd4
	add	%i0, 64, %i0

1:
	FALIGN_D36
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D4
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd20
	add	%i0, 64, %i0

2:
	FALIGN_D4
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D20
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd36
	add	%i0, 64, %i0

copyin_seg3:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D6
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D22
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D38
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg3

0:
	FALIGN_D22
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D38
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd6
	add	%i0, 64, %i0

1:
	FALIGN_D38
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D6
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd22
	add	%i0, 64, %i0

2:
	FALIGN_D6
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D22
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd38
	add	%i0, 64, %i0

copyin_seg4:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D8
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D24
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D40
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg4

0:
	FALIGN_D24
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D40
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd8
	add	%i0, 64, %i0

1:
	FALIGN_D40
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D8
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd24
	add	%i0, 64, %i0

2:
	FALIGN_D8
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D24
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd40
	add	%i0, 64, %i0

copyin_seg5:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D10
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D26
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D42
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg5

0:
	FALIGN_D26
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D42
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd10
	add	%i0, 64, %i0

1:
	FALIGN_D42
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D10
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd26
	add	%i0, 64, %i0

2:
	FALIGN_D10
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D26
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd42
	add	%i0, 64, %i0

copyin_seg6:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D12
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D28
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D44
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg6

0:
	FALIGN_D28
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D44
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd12
	add	%i0, 64, %i0

1:
	FALIGN_D44
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D12
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd28
	add	%i0, 64, %i0

2:
	FALIGN_D12
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D28
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd44
	add	%i0, 64, %i0

copyin_seg7:
	! 1st chunk - %d0 low, %d16 high, %d32 pre, %d48 dst
	FALIGN_D14
	ldda	[%l7]ASI_BLK_AIUS, %d0
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 0f
	add	%i0, 64, %i0
	! 2nd chunk -  %d0 pre, %d16 low, %d32 high, %d48 dst
	FALIGN_D30
	ldda	[%l7]ASI_BLK_AIUS, %d16
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 1f
	add	%i0, 64, %i0
	! 3rd chunk -  %d0 high, %d16 pre, %d32 low, %d48 dst
	FALIGN_D46
	ldda	[%l7]ASI_BLK_AIUS, %d32
	stda	%d48, [%i0]ASI_BLK_P
	add	%l7, 64, %l7
	subcc	%i3, 64, %i3
	bz,pn	%ncc, 2f
	add	%i0, 64, %i0
	ba,a,pt	%ncc, copyin_seg7

0:
	FALIGN_D30
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D46
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd14
	add	%i0, 64, %i0

1:
	FALIGN_D46
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D14
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd30
	add	%i0, 64, %i0

2:
	FALIGN_D14
	stda	%d48, [%i0]ASI_BLK_P
	add	%i0, 64, %i0
	membar	#Sync
	FALIGN_D30
	stda	%d48, [%i0]ASI_BLK_P
	ba,pt	%ncc, copyin_blkd46
	add	%i0, 64, %i0


	!
	! dribble out the last partial block
	!
copyin_blkd0:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d0, %d2, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd2:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d2, %d4, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd4:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d4, %d6, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd6:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d6, %d8, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd8:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d8, %d10, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd10:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d10, %d12, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd12:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d12, %d14, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd14:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	fsrc1	%d14, %d0
	ba,a,pt	%ncc, copyin_blkleft

copyin_blkd16:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d16, %d18, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd18:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d18, %d20, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd20:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d20, %d22, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd22:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d22, %d24, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd24:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d24, %d26, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd26:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d26, %d28, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd28:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d28, %d30, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd30:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	fsrc1	%d30, %d0
	ba,a,pt	%ncc, copyin_blkleft
copyin_blkd32:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d32, %d34, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd34:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d34, %d36, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd36:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d36, %d38, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd38:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d38, %d40, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd40:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d40, %d42, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd42:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d42, %d44, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd44:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	faligndata %d44, %d46, %d48
	std	%d48, [%i0]
	add	%i0, 8, %i0
copyin_blkd46:
	subcc	%i4, 8, %i4
	blu,pn	%ncc, copyin_blkdone
	fsrc1	%d46, %d0

copyin_blkleft:
1:
	ldda	[%l7]ASI_USER, %d2
	add	%l7, 8, %l7
	subcc	%i4, 8, %i4
	faligndata %d0, %d2, %d8
	std	%d8, [%i0]
	blu,pn	%ncc, copyin_blkdone
	add	%i0, 8, %i0
	ldda	[%l7]ASI_USER, %d0
	add	%l7, 8, %l7
	subcc	%i4, 8, %i4
	faligndata %d2, %d0, %d8
	std	%d8, [%i0]
	bgeu,pt	%ncc, 1b
	add	%i0, 8, %i0

copyin_blkdone:
	tst	%i2
	bz,pt	%ncc, .copyin_exit
	and	%l3, 0x4, %l3		! fprs.du = fprs.dl = 0

7:	lduba	[%i1]ASI_USER, %i4
	inc	%i1
	inc	%i0
	deccc	%i2
	bgu	%ncc, 7b
	  stb	  %i4, [%i0 - 1]

.copyin_exit:
	membar	#StoreLoad|#StoreStore
	btst	1, %l6
	bz	1f
	  andn	%l6, 3, %l6

	ld	[%fp + STACK_BIAS - SAVED_GSR_OFFSET], %o2	! restore gsr
	wr	%o2, 0, %gsr

	ld	[%fp + STACK_BIAS - SAVED_FPRS_OFFSET], %o3
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o3
	bz	4f
	  nop

	! restore fpregs from stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d16
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d32
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d48
	membar	#Sync

	ba	2f
	  wr	%o3, 0, %fprs		! restore fprs

4:
	FZERO				! zero all of the fpregs
	wr	%o3, 0, %fprs		! restore fprs

2:	ldn	[THREAD_REG + T_LWP], %o2
	tst	%o2
	bnz,pt	%ncc, 1f
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%ncc, 1f
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%ncc, 1f
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

1:
	stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g0, 0, %o0

.small_copyin:
	subcc	%g0, %i2, %i3
	add	%i0, %i2, %i0
	bz	%ncc, 2f
	add	%i1, %i2, %i1
	lduba	[%i0+%i3]ASI_USER, %i4
1:	stb	%i4, [%i1+%i3]
	inccc	%i3
	bcc,a,pt %ncc, 1b
	  lduba	[%i0+%i3]ASI_USER, %i4
2:	andn	%l6, 3, %l6
	stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore %g0, 0, %o0		! return (0)
	SET_SIZE(default_copyin)

#endif	/* lint */

#ifdef	lint

/*ARGSUSED*/
int
default_xcopyin(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_xcopyin)

	TRACE_IN(TR_COPYIN_START, TR_copyin_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	set	.xcopyioerr, %l6	! .xcopyioerr is lofault value
	ldn	[THREAD_REG + T_LOFAULT], %l7	! set/save t_lofaul
	stn	%l6, [THREAD_REG + T_LOFAULT]
	b	.do_copyin		! common code
	  mov	%l7, %l6

/*
 * We got here because of a fault during xcopy{in,out}.
 * Errno value is in %g1.
 */
.xcopyioerr:
	btst	1, %l6
	bz	1f
	  andn	%l6, 3, %l6

	membar	#Sync

	ld	[%fp + STACK_BIAS - SAVED_GSR_OFFSET], %o2	! restore gsr
	wr	%o2, 0, %gsr

	ld	[%fp + STACK_BIAS - SAVED_FPRS_OFFSET], %o3
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o3
	bz	4f
	  nop

	! restore fpregs from stack
	membar	#Sync
	add	%fp, STACK_BIAS - 257, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d16
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d32
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d48
	membar	#Sync

	ba	2f
	  wr	%o3, 0, %fprs		! restore fprs

4:
	FZERO				! zero all of the fpregs
	wr	%o3, 0, %fprs		! restore fprs

2:	ldn	[THREAD_REG + T_LWP], %o2
	tst	%o2
	bnz,pt	%ncc, 1f
	  nop

	ldsb	[THREAD_REG + T_PREEMPT], %l0
	deccc	%l0
	bnz,pn	%ncc, 1f
	  stb	%l0, [THREAD_REG + T_PREEMPT]

	! Check for a kernel preemption request
	ldn	[THREAD_REG + T_CPU], %l0
	ldub	[%l0 + CPU_KPRUNRUN], %l0
	tst	%l0
	bz,pt	%ncc, 1f
	  nop

	! Attempt to preempt
	call	kpreempt
	  rdpr	  %pil, %o0		  ! pass %pil

1:
	stn	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g1, 0, %o0
	SET_SIZE(default_xcopyin)

#endif	/* lint */

#ifdef	lint

/*ARGSUSED*/
int
xcopyin_little(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(xcopyin_little)
	set	.xcopyio_err, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o4
	stn	%o5, [THREAD_REG + T_LOFAULT]	
	mov	%o4, %o5

	subcc	%g0, %o2, %o3
	add	%o0, %o2, %o0
	bz,pn	%ncc, 2f		! check for zero bytes
	sub	%o2, 1, %o4
	add	%o0, %o4, %o0		! start w/last byte	
	add	%o1, %o2, %o1
	lduba	[%o0+%o3]ASI_AIUSL, %o4

1:	stb	%o4, [%o1+%o3]
	inccc	%o3
	sub	%o0, 2, %o0		! get next byte
	bcc,a,pt %ncc, 1b
	  lduba	[%o0+%o3]ASI_AIUSL, %o4

2:	stn	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl
	mov	%g0, %o0		! return (0)

.xcopyio_err:
	stn	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl
	mov	%g1, %o0
	SET_SIZE(xcopyin_little)

#endif	/* lint */

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 */

#if defined(lint)

/* ARGSUSED */
int
default_copyinstr(const char *uaddr, char *kaddr, size_t maxlength,
    size_t *lencopied)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyinstr)
	set	.copystrerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o4	! catch faults
	stn	%o5, [THREAD_REG + T_LOFAULT]
	mov	%o4, %o5

	mov	%o2, %o4		! save original count

	! maxlength is unsigned so the only error is if it's 0
	brz,a,pn %o2, .cs_out
	mov	ENAMETOOLONG, %o0

	b	1f
	sub	%o0, %o1, %o0		! o0 gets the difference of src and dst

	!
	! Do a byte by byte loop.
	! We do this instead of a word by word copy because most strings
	! are small and this takes a small number of cache lines.
	!
0:
	stb	%g1, [%o1]		! store byte
	tst	%g1			! null byte?
	bnz	1f
	add	%o1, 1, %o1		! incr dst addr

	b	.cs_out			! last byte in string
	mov	0, %o0			! ret code = 0
1:
	subcc	%o2, 1, %o2		! test count
	bgeu,a	%ncc, 0b
	lduba	[%o0+%o1]ASI_USER, %g1	! delay slot, get source byte

	mov	0, %o2			! max number of bytes moved
	b	.cs_out
	  mov	ENAMETOOLONG, %o0	! ret code = ENAMETOOLONG
	SET_SIZE(default_copyinstr)

#endif	/* lint */

/*
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 */

#if defined(lint)

/* ARGSUSED */
int
default_copyoutstr(const char *kaddr, char *uaddr, size_t maxlength,
    size_t *lencopied)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyoutstr)
	set	.copystrerr, %o5
	ldn	[THREAD_REG + T_LOFAULT], %o4	! catch faults
	stn	%o5, [THREAD_REG + T_LOFAULT]
	mov	%o4, %o5

	mov	%o2, %o4		! save original count

	brz,a,pn %o2, .cs_out
	mov	ENAMETOOLONG, %o0

	b	1f
	sub	%o0, %o1, %o0		! o0 gets the difference of src and dst

	!
	! Do a byte by byte loop.
	! We do this instead of a word by word copy because most strings
	! are small and this takes a small number of cache lines.
	!
0:
	stba	%g1, [%o1]ASI_USER	! store byte
	tst	%g1			! null byte?
	bnz	1f
	add	%o1, 1, %o1		! incr dst addr

	b	.cs_out			! last byte in string
	mov	0, %o0			! ret code = 0
1:
	subcc	%o2, 1, %o2		! test count
	bgeu,a	%ncc, 0b
	ldub	[%o0+%o1], %g1	! delay slot, get source byte

	mov	0, %o2			! max number of bytes moved
	b	.cs_out
	  mov	ENAMETOOLONG, %o0	! ret code = ENAMETOOLONG

/*
 * Fault while trying to move from or to user space.
 * Set and return error code.
 */
.copystrerr:
	mov	EFAULT, %o0
	retl
	stn	%o5, [THREAD_REG + T_LOFAULT]
	SET_SIZE(default_copyoutstr)

#endif	/* lint */


/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * No fault handler installed (to be called under on_fault())
 */

#if defined(lint)

/* ARGSUSED */
void
copyin_noerr(const void *ufrom, void *kto, size_t count)
{}

#else	/* lint */
	ENTRY(copyin_noerr)

	TRACE_IN(TR_COPYIN_NOERR_START, TR_copyin_noerr_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	ldn	[THREAD_REG + T_LOFAULT], %l6
	brz,pn	%l6, .do_copyin
	  nop
	set	.copyioerr, %o2
	stn	%o2, [THREAD_REG + T_LOFAULT]	! set/save t_lofault
	b	.do_copyin
	  or	%l6, 2, %l6		! error should trampoline

	SET_SIZE(copyin_noerr)
#endif /* lint */

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * No fault handler installed (to be called under on_fault())
 */

#if defined(lint)

/* ARGSUSED */
void
copyout_noerr(const void *kfrom, void *uto, size_t count)
{}

#else	/* lint */
	ENTRY(copyout_noerr)

	TRACE_IN(TR_COPYOUT_NOERR_START, TR_copyout_noerr_start)

	save	%sp, -SA(MINFRAME + HWCOPYFRAMESIZE), %sp
	ldn	[THREAD_REG + T_LOFAULT], %l6
	brz,pn	%l6, .do_copyout
	  nop
	set	.copyioerr, %o2
	stn	%o2, [THREAD_REG + T_LOFAULT]	! set/save t_lofault
	b	.do_copyout
	  or	%l6, 2, %l6		! error should trampoline

	SET_SIZE(copyout_noerr)
#endif /* lint */

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * No fault handler installed (to be called under on_fault())
 */

#if defined(lint)
 
/* ARGSUSED */
void
ucopy(const void *ufrom, void *uto, size_t ulength)
{}
 
#else /* lint */
 
	ENTRY(ucopy)
	save	%sp, -SA(MINFRAME), %sp ! get another window

	subcc	%g0, %i2, %i3
	add	%i0, %i2, %i0
	bz	%ncc, 5f
	add	%i1, %i2, %i1
	lduba	[%i0 + %i3]ASI_USER, %i4
4:	stba	%i4, [%i1 + %i3]ASI_USER
	inccc	%i3
	bcc,a,pt %ncc, 4b
	lduba  [%i0 + %i3]ASI_USER, %i4
5:
	ret
	restore %g0, 0, %o0		! return (0)

	SET_SIZE(ucopy)
#endif /* lint */

#if defined(lint)

int use_hw_bcopy = 1;
int use_hw_copyio = 1;
int use_hw_bzero = 1;

#else /* !lint */

	.align	4
	DGDEF(use_hw_bcopy)
	.word	1
	DGDEF(use_hw_copyio)
	.word	1
	DGDEF(use_hw_bzero)
	.word	1

	.align	64
	.section ".text"
#endif /* !lint */
