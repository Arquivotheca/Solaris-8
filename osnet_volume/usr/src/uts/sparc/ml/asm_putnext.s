/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)asm_putnext.s	1.21	99/02/15 SMI"

/*
 * This is the SPARC assembly version of uts/common/os/putnext.c.
 * The code is very closely aligned with the C version but uses the
 * following tricks:
 *	- It does a manual save and restore of the syncq pointer and
 *	  the return address (%i7) into sq_save except for SQ_CIPUT syncqs.
 *	- It accesses sq_count and sq_flags in one load/store assuming
 *	  that sq_flags is the low-order 16 bytes on the word.
 *	- Moving out the uncommon code sections to improve the I cache
 *	  performance.
 *
 * The DDI used to define putnext() as a function returning int, so to be
 * conservative we always return zero.  (This has no effect on performance.)
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/mutex.h>
#include <sys/t_lock.h>
#include <sys/asm_linkage.h>
#include <sys/machlock.h>
#include <sys/machthread.h>

#if !defined(lint)

#undef DEBUG
#ifdef DEBUG
	.seg	".data"
	.global	putnext_early;
putnext_early:
	.word	0
	.global	putnext_not_early;
putnext_not_early:
	.word	0
	.seg	".text"
	.align	4
#endif DEBUG

#ifdef TRACE
	.global	TR_put_start;
TR_put_start:
	.asciz	"put:(%lX, %lX)";
	.global	TR_putnext_start;
TR_putnext_start:
	.asciz	"putnext_start:(%lX, %lX)";
	.global	TR_putnext_end;
TR_putnext_end:
	.asciz	"putnext_end:(%X, %lX, %lX) done";
	.align	4
#endif	/* TRACE */

! Need defines for
!	SD_LOCK
!	Q_FLAG Q_NEXT Q_STREAM Q_SYNCQ Q_QINFO
!	QI_PUTP
!	SQ_LOCK SQ_CHECK SQ_WAIT SQ_SAVE

/*
 * Assumes that sq_flags and sq_count can be read as int with sq_count
 * being the high-order part.
 */
#define	SQ_COUNTSHIFT	16
#define	SQ_CHECK	SQ_COUNT

! Performance:
!	mutex_enter(v9): 7 instr, 1 ld/use stall
!	mutex_exit(v9): 9 instr, 2? ld/use stalls
!	putnext uses 3 mutex_enter/exit pairs; put only uses 2.
!	putnext hot: -8 instr (+window overflow)
!		6 loads, 2 stores
!	put: putnext -(10) instr
!
! Registers:
!	%i0: qp
!	%i1: mp
!	%i2: stp
!	%i3: sq_check
!	%i4: q_flag, q_qinfo, qi_putp
!	%i5: sq
!	%l0: temp (1 << SQ_COUNTSHIFT)
!	%l1: temp (1 << SQ_COUNTSHIFT) - 1 (mask for sq_flags)

#endif	/* !lint */

#if defined(lint)

/* ARGSUSED */
void
put(queue_t *qp, mblk_t *mp)
{}

#else	/* lint */

#if (QHOT & 0x3ff) != 0
#error - QHOT is not settable with sethi
#endif

! %i0 qp
! %i1 mp
	ENTRY(put)
	ld	[%o0 + Q_FLAG], %o2
	sethi	%hi(QHOT), %o3
	btst	%o3, %o2
	bne	_c_put
	nop
	save	%sp, -SA(MINFRAME), %sp
	TRACE_ASM_2(%o3, TR_FAC_STREAMS_FR, TR_PUT_START,
		TR_put_start, %i0, %i1);
	ldn	[%i0 + Q_SYNCQ], %i5
	sethi	%hi(1<<SQ_COUNTSHIFT), %l0
	call	mutex_enter, 1
	add	%i5, SQ_LOCK, %o0		! delay
	ld	[%i5 + SQ_CHECK], %i3
	ba	.put_entry
	ldn	[%i0 + Q_QINFO], %i4		! delay
	SET_SIZE(put)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
putnext(queue_t *qp, mblk_t *mp)
{}

#else	/* lint */

#if (QNEXTHOT & 0x3ff) != 0
#error - QNEXTHOT is not settable with sethi
#endif

	ENTRY(putnext)
	ld	[%o0 + Q_FLAG], %o2
	sethi	%hi(QNEXTHOT), %o3
	btst	%o3, %o2
	bne	_c_putnext
	nop
	save	%sp, -SA(MINFRAME), %sp
	TRACE_ASM_2(%o3, TR_FAC_STREAMS_FR, TR_PUTNEXT_START,
		TR_putnext_start, %i0, %i1);
	ldn	[%i0 + Q_STREAM], %i2
	call	mutex_enter, 1
	add	%i2, SD_LOCK, %o0		! delay
	ldn	[%i0 + Q_NEXT], %i0
	sethi	%hi(1<<SQ_COUNTSHIFT), %l0
	ldn	[%i0 + Q_SYNCQ], %i5
	ldn	[%i0 + Q_QINFO], %i4
	call	mutex_enter, 1
	add	%i5, SQ_LOCK, %o0		! delay
	ld	[%i5 + SQ_CHECK], %i3
	call	mutex_exit, 1
	add	%i2, SD_LOCK, %o0		! delay
.put_entry:
	btst	(SQ_GOAWAY|SQ_CIPUT), %i3	! sq_flags set?
	bne	.flags_in
	ldn	[%i4 + QI_PUTP], %i4		! delay
	or	%l0, SQ_EXCL, %l0
	add	%i3, %l0, %i3			! set SQ_EXCL, inc sq_count
	st	%i3, [%i5 + SQ_CHECK]
	call	mutex_exit, 1
	add	%i5, SQ_LOCK, %o0		! delay

.call_putp:
	! Unrotate window and save o5 and o7
	restore
	stn	%l4, [%o5 + SQ_SAVE]
	stn	%l5, [%o5 + SQ_SAVE + CPTRSIZE]
	mov	%o7, %l5
	call	%o4, 2
	mov	%o5, %l4			! delay
	! restore o5 and o7 and rotate window back
	mov	%l4, %o5
	mov	%l5, %o7
	ldn	[%o5 + SQ_SAVE], %l4
	ldn	[%o5 + SQ_SAVE + CPTRSIZE], %l5
	save	%sp, -SA(MINFRAME), %sp

.putp_done:
	call	mutex_enter, 1
	add	%i5, SQ_LOCK, %o0		! delay
	ld	[%i5 + SQ_CHECK], %i3		! get sq_check
	sethi	%hi(1<<SQ_COUNTSHIFT), %l0
	btst	(SQ_QUEUED|SQ_WANTWAKEUP|SQ_WANTEXWAKEUP), %i3
	bne	.flags_out			! above flags set?
	sub	%i3, %l0, %i3			! delay, count--
	andn	%i3, SQ_EXCL, %i3
	st	%i3, [%i5 + SQ_CHECK]
	call	mutex_exit, 1
	add	%i5, SQ_LOCK, %o0		! delay
	TRACE_ASM_3(%o3, TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
		TR_putnext_end, %g0, %i5, %i1);
	ret
	restore	%g0, 0, %o0			! delay

!
! Handle flags that are set when entering the syncq:
!	- SQ_GOAWAY: put on syncq
!	- SQ_CIPUT: do not set SQ_EXCL, call put procedure with rotated window
!
.flags_in:
	! Handle the different flags
	btst	SQ_GOAWAY, %i3
	be,a	.do_ciput
	add	%i3, %l0, %i3			! delay, add one to sq_count

	mov	%i1, %o2
	mov	%i0, %o1
	clr	%o3
	call	fill_syncq, 4
	mov	%i5, %o0			! delay
	call	mutex_exit, 1
	add	%i5, SQ_LOCK, %o0		! delay
	TRACE_ASM_3(%o3, TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
		TR_putnext_end, %i0, %i1, %g0);
	ret
	restore	%g0, 0, %o0			! delay

.do_ciput:
	st	%i3, [%i5 + SQ_CHECK]
	call	mutex_exit, 1
	add	%i5, SQ_LOCK, %o0		! delay
	mov	%i0, %o0
	call	%i4, 2
	mov	%i1, %o1			! delay
	ba,a	.putp_done

!
! Handle flags when leaving the syncq:
!	- SQ_QUEUED but not SQ_STAYAWAY: drain the syncq
!	- SQ_WANTWAKEUP: cv_broadcast on sq_wait
!	- SQ_WANTEXWAKEUP: cv_broadcast on sq_exitwait
!
.flags_out:
	st	%i3, [%i5 + SQ_CHECK]
	mov	%i5, %i0			! sq
	! mp already in %i1
	sub	%l0, 1, %l1			! 0xffff
	and	%i3, %l1, %i2			! flags
	call	putnext_tail, 3
	restore					! delay

	SET_SIZE(putnext)

#endif	/* lint */
