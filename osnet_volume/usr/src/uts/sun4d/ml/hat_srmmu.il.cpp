/*
 * Copyright (c) 1992, Sun Microsystems,  Inc.
 */

#ident	"@(#)hat_srmmu.il.cpp	1.14	95/01/16 SMI"

/*
 * This file is run through cpp before being used as
 * an inline.  It is set up so it can be used as both an
 * inline or assembled into a dot-o, but it has only
 * been tested as an inline.
 */

#ifndef	INLINE

#include <sys/asm_linkage.h>

#else	INLINE

#define	ENTRY(x)	.inline	x, 0
#define	retl		/* nop */
#define	SET_SIZE(x)	.end

/*
 * An apparent bug in cc arranges for only the last of multiple
 * inlines to be applied to the file being compiled.  All
 * C files use sparc.il and some use this inline too.  This
 * inline is later on the command line so sparc.il is ignored.
 * We workaround this by including sparc.il in this inline.
 */
#include <ml/sparc.il>
#endif	INLINE

#if defined(lint)
#include <sys/types.h>
#endif

#include <sys/mmu.h>

#define	PAGE_MASK	0xFFF

/*
 * return srmmu context table pointer
 */

#if defined(lint)

u_int
srmmu_getctp(void)
{ return (0); }

#else	/* lint */

	ENTRY(srmmu_getctp)
	set	RMMU_CTP_REG, %o0
	retl
	lda	[%o0]ASI_MOD, %o0		! delay slot
	SET_SIZE(srmmu_getctp)

#endif	/* lint */

/*
 * return srmmu context register
 */

#if defined(lint)

u_int
srmmu_getctxreg(void)
{ return (0); }

#else	/* lint */

	ENTRY(srmmu_getctxreg)
	set	RMMU_CTX_REG, %o0
	retl
	lda	[%o0]ASI_MOD, %o0		! delay slot
	SET_SIZE(srmmu_getctxreg)

#endif	/* lint */

/*
 * set srmmu context register
 */

#if defined(lint)

/* ARGSUSED */
void
srmmu_setctxreg(u_int ctxt)
{}

#else	/* lint */

	ENTRY(srmmu_setctxreg)
	set	RMMU_CTX_REG, %o1
	retl
	sta	%o0, [%o1]ASI_MOD		! delay slot
	SET_SIZE(srmmu_setctxreg)

#endif	/* lint */

/*
 * srmmu probe, passed virt addr and probe type
 */

#if defined(lint)

/* ARGSUSED */
u_int
srmmu_probe_type(caddr_t vaddr, int type)
{ return (0); }

#else	/* lint */

	ENTRY(srmmu_probe_type)
	andn	%o0, PAGE_MASK, %o0	! make sure lower 12 bits are clear
	and	%o1, 0xF, %o1		! make sure type is valid
	sll	%o1, 8, %o1		! shift type over
	or	%o0, %o1, %o0		! or type into address
	lda	[%o0]ASI_FLPR, %o0	! do the probe
	set	RMMU_FSR_REG, %o1	! setup to clear fsr
	retl
	lda	[%o1]ASI_MOD, %o1	! clear fsr
	SET_SIZE(srmmu_probe_type)

#endif	/* lint */

/*
 * srmmu flush, passed virt addr and flush type
 *
 * VIKING_BUG_PTP2: see machdep.c for more details.  The workaround is to
 * use a context demap instead of a region demap on those processors that
 * exhibit this bug.
 */

#if defined(lint)

void
srmmu_flush_type(caddr_t vaddr, int type)
{}

#else	/* lint */

	ENTRY(srmmu_flush_type)
	andn	%o0, PAGE_MASK, %o0	! make sure lower 12 bits are clear
	and	%o1, 0xF, %o1		! make sure type is valid
#if defined(VIKING_BUG_PTP2)
	cmp	%o1, FT_RGN		! check for region demap
	bne	1f
	nop
	sethi	%hi(viking_ptp2_bug), %o2 ! see if we need Viking ptp2...
	ld	[%o2 + %lo(viking_ptp2_bug)], %o2    ! ...workaround
	tst	%o2
	bnz,a	1f			! if so, use a context demap instead
	mov	FT_CTX, %o1		! delay(annulled if no workaround)
1:
#endif
	sll	%o1, 8, %o1		! shift type over
	or	%o0, %o1, %o0		! or type into address
	retl
	sta	%g0, [%o0]ASI_FLPR	! do the flush
	SET_SIZE(srmmu_flush_type)

#endif	/* lint */

/*
 * A second bug in the optimizer dealing with swap and inlines
 * has been found (1092755).  Let's not use swaps in inlines
 * until it is fixed.  Even when it is fixed it will be a long
 * time before everyone out there has the fix and it would be
 * stupid to have the sun4d kernel dependent on people having
 * a fix to an obscure optimizer bug, so we'll probably never
 * try to use swaps in inlines.
 */
#if 0
/*
 * swap register contents with memory location contents,
 * addr, val argument order
 */
#if defined(lint)

/* ARGSUSED */
u_int
swap(caddr_t vaddr, u_int val)
{ return (0); }

#else	/* lint */

	ENTRY(swap)
	swap	[%o0], %o1
	retl
	mov	%o1, %o0
	SET_SIZE(swap)

#endif	/* lint */

/*
 * swap register contents with memory location contents,
 * val, addr argument order
 */
#if defined(lint)

/* ARGSUSED */
int
swapl(u_int val, int *vaddr)
{ return (0); }

#else	/* lint */

	ENTRY(swapl)
	retl
	swap	[%o1], %o0
	SET_SIZE(swapl)

#endif	/* lint */

#endif
