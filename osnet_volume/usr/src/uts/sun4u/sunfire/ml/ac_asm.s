/*
 *	Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved
 */

#pragma	ident	"@(#)ac_asm.s	1.2	98/04/20	SMI"

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

#if defined(lint)

#else	/* lint */
#include "assym.h"
#endif	/* lint */

#define	TT_HSM	0x99

#if defined(lint)
/* ARGSUSED */
void
ac_blkcopy(caddr_t src, caddr_t dst, u_int linecount, u_int linesize)
{}
#else /* !lint */
!
! Move a single cache line of data.  Survive UE and CE on the read
!
! i0 = src va
! i1 = dst va
! i2 = line count
! i3 = line size
! i4 = cache of fpu state
!
	ENTRY(ac_blkcopy)

	! TODO: can we safely SAVE here
	save	%sp, -SA(MINFRAME + 2*64), %sp

	! XXX do we need to save the state of the fpu?
	rd	%fprs, %i4
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %i4

	! always enable FPU
	wr	%g0, FPRS_FEF, %fprs

	bz,a	1f
	 nop

	! save in-use fpregs on stack
	membar	#Sync
	add	%fp, STACK_BIAS - 81, %o2
	and	%o2, -64, %o2
	stda	%d0, [%o2]ASI_BLK_P
	membar	#Sync

1:
	brz,pn	%i2, 2f				! while (linecount) {
	 nop
	ldda	[%i0]ASI_BLK_P, %d0		! *dst = *src;
	membar	#Sync
	stda	%d0, [%i1]ASI_BLK_COMMIT_P
	membar	#Sync

	add	%i0, %i3, %i0			! dst++, src++;
	add	%i1, %i3, %i1

	ba	1b				! linecount-- }
	 dec	%i2

2:
	membar	#Sync

	! restore fp to the way we got it
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %i4
	bz,a	3f
	 nop

	! restore fpregs from stack
	add	%fp, STACK_BIAS - 81, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	membar	#Sync

3:
	wr	%g0, %i4, %fprs			! fpu back to the way it was
	ret
	 restore
	SET_SIZE(ac_blkcopy)
#endif /* lint */
