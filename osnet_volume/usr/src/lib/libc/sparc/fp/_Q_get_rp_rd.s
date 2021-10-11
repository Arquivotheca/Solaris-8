! Copyright (c) 1989, 1991, by Sun Microsystems, Inc.
!
! _Q_get_rp_rd: 
!
! _QswapRD(rd)  exchanges rd with the current rounding direction.
! _QswapRP(rp)  exchanges rp with the current rounding precision.
! Note: we use %sp+0x44 (shadow area of %o0) for temporary space

.ident	"@(#)_Q_get_rp_rd.s	1.3	92/07/14 SMI"

#include "synonyms.h"
#include <sys/asm_linkage.h>

	ENTRY(_QswapRD)
	add     %sp,-SA(MINFRAME),%sp
	set	0xc0000000,%o4		! mask of rounding direction bits
	sll	%o0,30,%o1		! shift input to RD bit location
	st	%fsr,[%sp+ARGPUSH]
	ld	[%sp+ARGPUSH],%o0	! o0 = fsr
	and	%o1,%o4,%o1
	andn	%o0,%o4,%o2		
	or	%o1,%o2,%o1		! o1 = new fsr
	st	%o1,[%sp+ARGPUSH]
	ld	[%sp+ARGPUSH],%fsr
	and	%o0,%o4,%o0
	srl	%o0,30,%o0
	retl
	add     %sp,SA(MINFRAME),%sp

	SET_SIZE(_QswapRD)

	ENTRY(_QswapRP)
	add     %sp,-SA(MINFRAME),%sp
	set	0x30000000,%o4		! mask of rounding precision bits
	sll	%o0,28,%o1		! shift input to RP bit location
	st	%fsr,[%sp+ARGPUSH]
	ld	[%sp+ARGPUSH],%o0	! o0 = fsr
	and	%o1,%o4,%o1
	andn	%o0,%o4,%o2		
	or	%o1,%o2,%o1		! o1 = new fsr
	st	%o1,[%sp+ARGPUSH]
	ld	[%sp+ARGPUSH],%fsr
	and	%o0,%o4,%o0
	srl	%o0,28,%o0
	retl
	add     %sp,SA(MINFRAME),%sp

	SET_SIZE(_QswapRP)

	ENTRY(_QgetRD)
	add     %sp,-SA(MINFRAME),%sp
        st      %fsr,[%sp+ARGPUSH]
        ld      [%sp+ARGPUSH],%o0	! o0 = fsr
        srl     %o0,30,%o0              ! return round control value
        retl
	add     %sp,SA(MINFRAME),%sp

	SET_SIZE(_QgetRD)

	ENTRY(_QgetRP)
	add     %sp,-SA(MINFRAME),%sp
        st      %fsr,[%sp+ARGPUSH]
	set	0x30000000,%o4		! mask of rounding precision bits
        ld      [%sp+ARGPUSH],%o0	! o0 = fsr
	and	%o0,%o4,%o0
        srl     %o0,28,%o0              ! return round control value
        retl
	add     %sp,SA(MINFRAME),%sp

	SET_SIZE(_QgetRP)
