/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 a/

#pragma ident	"@(#)sfdr.il.cpp	1.4	99/01/05 SMI"

/*
 * This file is through cpp before being used as
 * an inline.  It contains support routines used
 * only by DR for the copy-rename sequence.
 */

#if defined(lint)
#include <sys/types.h>
#endif /* lint */

#ifndef	INLINE

#include <sys/asm_linkage.h>

#else /* INLINE */

#define	ENTRY_NP(x)	.inline	x,0
#define	retl		/* nop */
#define	SET_SIZE(x)	.end

#endif /* INLINE */

#include <sys/privregs.h>
#include <sys/spitasi.h>
#include <sys/machparam.h>

/*
 * Bcopy routine used by DR to copy
 * between physical addresses. 
 * Borrowed from Starfire DR 2.6.
 */
#if defined(lint)

/*ARGSUSED*/
void
bcopy32_il(uint64_t paddr1, uint64_t paddr2)
{}

#else /* lint */

	ENTRY_NP(bcopy32_il)
	.register %g2, #scratch
	.register %g3, #scratch
#ifndef __sparcv9
	sllx    %o0, 32, %o0    ! shift upper 32 bits
        srl     %o1, 0, %o1     ! clear upper 32 bits
        or      %o0, %o1, %o0   ! form 64 bit physaddr in %o0 using (%o0,%o1)
        sllx    %o2, 32, %o2    ! shift upper 32 bits
        srl     %o3, 0, %o3     ! clear upper 32 bits
        or      %o2, %o3, %o1   ! form 64 bit physaddr in %o1 using (%o2,%o3)
#endif /* !__sparcv9 */
        rdpr    %pstate, %o4
        andn    %o4, PSTATE_IE | PSTATE_AM, %g3		! clear IE, AM bits
        wrpr    %g0, %g3, %pstate

        ldxa    [%o0]ASI_MEM, %o2
	add	%o0, 8, %o0
        ldxa    [%o0]ASI_MEM, %o3
	add	%o0, 8, %o0
        ldxa    [%o0]ASI_MEM, %g1
	add	%o0, 8, %o0
        ldxa    [%o0]ASI_MEM, %g2

	stxa    %o2, [%o1]ASI_MEM
	add	%o1, 8, %o1
	stxa    %o3, [%o1]ASI_MEM
	add	%o1, 8, %o1
	stxa    %g1, [%o1]ASI_MEM
	add	%o1, 8, %o1
	stxa    %g2, [%o1]ASI_MEM

	retl
        wrpr    %g0, %o4, %pstate       ! restore earlier pstate register value
	SET_SIZE(bcopy32_il)

#endif /* lint */

#if 0
#define	FALIGN_D0				\
	faligndata	%d0, %d2, %d48		;\
	faligndata	%d2, %d4, %d50		;\
	faligndata	%d4, %d6, %d52		;\
	faligndata	%d6, %d8, %d54		;\
	faligndata	%d8, %d10, %d56		;\
	faligndata	%d10, %d12, %d58	;\
	faligndata	%d12, %d14, %d60	;\
	faligndata 	%d14, %d16, %d62

#define	FALIGN_D16				\
	faligndata	%d16, %d18, %d48	;\
	faligndata	%d18, %d20, %d50	;\
	faligndata	%d20, %d22, %d52	;\
	faligndata	%d22, %d24, %d54	;\
	faligndata	%d24, %d26, %d56	;\
	faligndata	%d26, %d28, %d58	;\
	faligndata	%d28, %d30, %d60	;\
	faligndata	%d30, %d32, %d62

#if defined(lint)

/*ARGSUSED*/
void
bcopy128_il(uint64_t paddr1, uint64_t paddr2)
{}

#else /* lint */

	ENTRY_NP(bcopy128_il)
#ifndef __sparcv9
	sllx    %o0, 32, %o0    ! shift upper 32 bits
        srl     %o1, 0, %o1     ! clear upper 32 bits
        or      %o0, %o1, %o0   ! form 64 bit physaddr in %o0 using (%o0,%o1)
        sllx    %o2, 32, %o2    ! shift upper 32 bits
        srl     %o3, 0, %o3     ! clear upper 32 bits
        or      %o2, %o3, %o1   ! form 64 bit physaddr in %o1 using (%o2,%o3)
#endif /* !__sparcv9 */
        rdpr    %pstate, %o4
        andn    %o4, PSTATE_IE | PSTATE_AM, %g3		! clear IE, AM bits
        wrpr    %g0, %g3, %pstate

	FALIGN_D0
	ldda	[%o0]ASI_MEM, %d0
	stda	%d48, [%o1]ASI_MEM

	add	%o0, 64, %o0
	add	%o1, 64, %o1

	FALIGN_D16
	ldda	[%o0]ASI_MEM, %d16
	stda	%d48, [%o1]ASI_MEM

	retl
        wrpr    %g0, %o4, %pstate       ! restore earlier pstate register value
	SET_SIZE(bcopy128_il)

#endif /* lint */
#endif /* 0 */

#if defined(lint)

/*ARGSUSED*/
void
flush_ecache_il(uint64_t physaddr, uint_t size)
{}

#else /* lint */

	ENTRY_NP(flush_ecache_il)
#ifndef __sparcv9
	sllx	%o0, 32, %o0		! shift upper 32 bits
	srl	%o1, 0, %o1		! clear upper 32 bits
	or	%o0, %o1, %o0		! form 64 bit physaddr in %o0
					! using (%o0, %o1)
	srl	%o2, 0, %o1
#else
	srl	%o1, 0, %o1		! clear upper 32 bits
#endif /* !__sparcv9 */
	set	ecache_linesize, %o2
	ld	[%o2], %o2
	srl	%o2, 0, %o2		! clear upper 32 bits

	rdpr	%pstate, %o3
	andn	%o3, PSTATE_IE | PSTATE_AM, %o4
	wrpr	%g0, %o4, %pstate	! clear AM to access 64 bit physaddr
	b	2f
	  nop
1:
	ldxa	[%o0 + %o1]ASI_MEM, %g0	! start reading from physaddr + size
2:
	subcc	%o1, %o2, %o1
	bgeu,a	1b
	  nop

	! retl
	wrpr	%g0, %o3, %pstate	! restore earlier pstate
	SET_SIZE(flush_ecache_il)

#endif /* lint */

#if defined(lint)

/*ARGUSED*/
void
stphysio_il(uint64_t physaddr, u_int value)
{}
 
/*ARGSUSED*/
u_int
ldphysio_il(uint64_t physaddr)
{ return(0); }

#else /* lint */

	ENTRY_NP(stphysio_il)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
	srl	%o2, 0, %o1
#endif
	rdpr	%pstate, %o2		/* read PSTATE reg */
	andn	%o2, PSTATE_IE | PSTATE_AM, %o3
	wrpr	%g0, %o3, %pstate
	stwa	%o1, [%o0]ASI_IO        /* store value via bypass ASI */
	retl
	wrpr	%g0, %o2, %pstate		/* restore the PSTATE */
	SET_SIZE(stphysio_il)

	!
	! load value at physical address in I/O space
	!
	! u_int   ldphysio_il(uint64_t physaddr)
	!
	ENTRY_NP(ldphysio_il)
#ifndef	__sparcv9
	sllx	%o0, 32, %o0		/* shift upper 32 bits */
	srl	%o1, 0, %o1		/* clear upper 32 bits */
	or	%o0, %o1, %o0		/* form 64 bit physaddr in %o0 */
#endif
	rdpr	%pstate, %o2		/* read PSTATE reg */
	andn	%o2, PSTATE_IE | PSTATE_AM, %o3
	wrpr	%g0, %o3, %pstate
	lduwa	[%o0]ASI_IO, %o0	/* load value via bypass ASI */
	retl
	wrpr	%g0, %o2, %pstate	/* restore pstate */
	SET_SIZE(ldphysio_il)

#endif /* lint */

#if defined(lint)

/*
 * Argument to sfdr_exec_script_il is a pointer to:
 *
 * typedef struct {
 *	uint64_t	masr_addr;
 *	uint_t		masr;
 *	uint_t		_filler;
 * } sfdr_rename_script_t;
 */

/*ARGUSED*/
void
sfdr_exec_script_il(void *sp)
{}
 
#else /* lint */

	ENTRY_NP(sfdr_exec_script_il)
	mov	%o0, %o2
0:					/* cache script */
	ldx	[%o2], %o1
	cmp	%g0, %o1
	bnz,pt	%xcc, 0b
	add	%o2, 16, %o2

	rdpr	%pstate, %o4		/* read PSTATE reg */
	andn	%o4, PSTATE_IE | PSTATE_AM, %o1
	wrpr	%g0, %o1, %pstate

	b	2f			/* cache it */
	nop
1:
	ldx	[%o0], %o1
	cmp	%g0, %o1
	bz,pn	%xcc, 5f
	ld	[%o0 + 8], %o2
	b	3f
	stwa	%o2, [%o1]ASI_IO
2:
	b	4f			/* cache it */
	nop
3:
	add	%o0, 16, %o0
	b	1b
	lduwa	[%o1]ASI_IO, %g0	/* read back to insure written */
4:
	b	1b			/* caching done */
	nop
5:	
	retl
	wrpr	%g0, %o4, %pstate	/* restore the PSTATE */
	SET_SIZE(sfdr_exec_script_il)

#endif /* lint */
