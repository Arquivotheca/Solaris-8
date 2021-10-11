/*
 * Copyright (c) 1990-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sparc_ddi.s	1.26	99/09/01 SMI"

/*
 * Assembler routines to make some DDI routines go faster.
 * These routines should ONLY be ISA-dependent.
 */

#if defined(lint)

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/sunddi.h>

#else	/* lint */

#include <sys/asm_linkage.h>
#ifdef __sparcv9cpu
#include <sys/clock.h>
#include <sys/intreg.h>
#endif

#include "assym.h"		/* for FKIOCTL etc. */

#endif	/* lint */


/*
 * Layered driver routines.
 *
 * At the time of writing, the compiler converts
 *
 * a() { return (b()); }
 *
 * into
 *	save, call b, restore
 *
 * Though this is sort of ok, if the called routine is leaf routine,
 * then we just burnt a register window.
 *
 * When the compiler understands this optimization, many
 * of these routines can go back to C again.
 */

#define	FLATCALL(routine)	\
	mov	%o7, %g1;	\
	call	routine;	\
	mov	%g1, %o7

#ifdef	lint

int
ddi_copyin(const void *buf, void *kernbuf, size_t size, int flags)
{
	if (flags & FKIOCTL)
		return (kcopy(buf, kernbuf, size) ? -1 : 0);
	return (copyin(buf, kernbuf, size));
}

#else	/* lint */

	ENTRY(ddi_copyin)
	set	FKIOCTL, %o4
	andcc	%o3, %o4, %g0
	bne	.do_kcopy	! share code with ddi_copyout
	FLATCALL(copyin)
	/*NOTREACHED*/

.do_kcopy:
	save	%sp, -SA(MINFRAME), %sp
	mov	%i2, %o2
	mov	%i1, %o1
	call	kcopy
	mov	%i0, %o0
	orcc	%g0, %o0, %i0	! if kcopy returns EFAULT ..
	bne,a	1f
	mov	-1, %i0		! .. we return -1
1:	ret
	restore
	SET_SIZE(ddi_copyin)

#endif	/* lint */

#ifdef	lint

int
ddi_copyout(const void *buf, void *kernbuf, size_t size, int flags)
{
	if (flags & FKIOCTL)
		return (kcopy(buf, kernbuf, size) ? -1 : 0);
	return (copyout(buf, kernbuf, size));
}

#else	/* lint */

	ENTRY(ddi_copyout)
	set	FKIOCTL, %o4
	andcc	%o3, %o4, %g0
	bne	.do_kcopy	! share code with ddi_copyin
	FLATCALL(copyout)
	/*NOTREACHED*/
	SET_SIZE(ddi_copyout)

#endif	/* lint */

/*
 * DDI spine wrapper routines - here so as to not have to
 * buy register windows when climbing the device tree (which cost!)
 */

#if	defined(lint)

/*ARGSUSED*/
int
ddi_ctlops(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t op, void *a, void *v)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_ctlops)
	tst	%o0		! dip != 0?
	be	2f		! nope
	tst	%o1		! rdip != 0?
	be	2f		! nope
	ldn	[%o0 + DEVI_BUS_CTL], %o0
				! dip = (dev_info_t *)DEVI(dip)->devi_bus_ctl;
	tst	%o0
#ifdef __sparcv9
	be,pn	%xcc, 2f
#else
	be	2f
#endif
	nop			! Delay slot
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_CTL], %g1	! dip->dev_ops->devo_bus_ops->bus_ctl
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
2:	retl
	sub	%g0, 1, %o0	! return (DDI_FAILURE);
	SET_SIZE(ddi_ctlops)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareqp, ddi_dma_handle_t *handlep)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_map)
	ldn	[%o0 + DEVI_BUS_DMA_MAP], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_map;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_MAP], %g1 ! dip->dev_ops->devo_bus_ops->bus_dma_map
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_map)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attr,
	int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_allochdl)
	ldn	[%o0 + DEVI_BUS_DMA_ALLOCHDL], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_allochdl;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_ALLOCHDL], %g1
			! dip->dev_ops->devo_bus_ops->bus_dma_allochdl
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_allochdl)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_freehdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handlep)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_freehdl)
	ldn	[%o0 + DEVI_BUS_DMA_FREEHDL], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_freehdl;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_FREEHDL], %g1
			! dip->dev_ops->devo_bus_ops->bus_dma_freehdl
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_freehdl)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
	ddi_dma_cookie_t *cp, u_int *ccountp)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_bindhdl)
	ldn	[%o0 + DEVI_BUS_DMA_BINDHDL], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_bindhdl;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_BINDHDL], %g1
			! dip->dev_ops->devo_bus_ops->bus_dma_bindhdl
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_bindhdl)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_unbindhdl)
	ldn	[%o0 + DEVI_BUS_DMA_UNBINDHDL], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_unbindhdl;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_UNBINDHDL], %g1
			! dip->dev_ops->devo_bus_ops->bus_dma_unbindhdl
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_unbindhdl)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_flush(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, off_t off, size_t len,
	u_int cache_flags)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_flush)
	ldn	[%o0 + DEVI_BUS_DMA_FLUSH], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_flush;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_FLUSH], %g1
			! dip->dev_ops->devo_bus_ops->bus_dma_flush
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_flush)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_win(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, uint_t win, off_t *offp,
	size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_win)
	ldn	[%o0 + DEVI_BUS_DMA_WIN], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_win;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_WIN], %g1
			! dip->dev_ops->devo_bus_ops->bus_dma_win
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_win)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_sync(ddi_dma_handle_t h, off_t o, size_t l, u_int whom)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_sync)
	ld	[%o0 + DMA_HANDLE_RFLAGS], %o4	! hp->dmai_rflags;
	sethi	%hi(DMP_NOSYNC), %o5
	and	%o4, %o5, %o4
	cmp	%o4, %o5
	bne	1f
	mov	%o3, %o5
	retl
	clr	%o0
1:	mov	%o1, %o3
	ldn	[%o0 + DMA_HANDLE_RDIP], %o1	! dip = hp->dmai_rdip;
	mov	%o0, %g2
	ldn	[%o1 + DEVI_BUS_DMA_FLUSH], %o0
			! dip = DEVI(dip)->devi_bus_dma_flush;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	mov	%o2, %o4
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	mov	%g2, %o2
	ldn	[%g1 + OPS_FLUSH], %g1
			! dip->dev_ops->devo_bus_ops->bus_dma_flush
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_sync)

#endif	/* lint */

#if	defined(lint)

/* ARGSUSED */
int
ddi_dma_unbind_handle(ddi_dma_handle_t h)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_unbind_handle)
	ldn	[%o0 + DMA_HANDLE_RDIP], %o1	! dip = hp->dmai_rdip;
	mov	%o0, %o2
	ldn	[%o1 + DEVI_BUS_DMA_UNBINDFUNC ], %g1
		    ! funcp = DEVI(dip)->devi_bus_dma_unbindfunc;
	jmpl	%g1, %g0	! bop off to new routine
	ldn	[%o1 + DEVI_BUS_DMA_UNBINDHDL], %o0
		    ! hdip = (dev_info_t *)DEVI(dip)->devi_bus_dma_unbindhdl;
	SET_SIZE(ddi_dma_unbind_handle)

#endif	/* lint */


#if	defined(lint)

/*ARGSUSED*/
int
ddi_dma_mctl(register dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp, caddr_t *objp, u_int flags)
{
	return (DDI_SUCCESS);
}

#else	/* lint */

	ENTRY(ddi_dma_mctl)
	ldn	[%o0 + DEVI_BUS_DMA_CTL], %o0
			! dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_ctl;
	ldn	[%o0 + DEVI_DEV_OPS], %g1	! dip->dev_ops
	ldn	[%g1 + DEVI_BUS_OPS], %g1	! dip->dev_ops->devo_bus_ops
	ldn	[%g1 + OPS_MCTL], %g1 ! dip->dev_ops->devo_bus_ops->bus_dma_ctl
	jmpl	%g1, %g0	! bop off to new routine
	nop			! as if we had never been here
	SET_SIZE(ddi_dma_mctl)

#endif	/* lint */

/*
 * With the introduction of UltraSparc III the original scheme where
 * usec_delay/drv_usecwait counted instruction cycles is no longer
 * sufficient. With UltraSparc III we can mix cpus of different
 * speed in the same system and with power saving mode the
 * clock rate is variable.
 */

/*
 * Delay by spinning.
 *
 * drv_usecwait(clock_t n)	[DDI/DKI - section 9F]
 * usec_delay(int n)		[compatibility - should go one day]
 *
 * delay for n microseconds.  numbers <= 0 delay 1 usec
 *
 * the inner loop counts for sparc v7/v8, initialized in startup():
 * inner loop is 2 cycles, the outer loop adds 3 more cycles.
 * Cpudelay*cycletime(ns)*2 + cycletime(ns)*3 >= 1000
 *
 * model	cycletime(ns)	Cpudelay
 * 110		66		7
 * 260		60		7
 * 330		40		11
 * 460		30		16
 *
 * XXX	These should probably be platform-dependent functions, since
 *	MP machines with different speed cpu's may want to do something
 *	a little more snazzy here
 *
 *  For sun4u, the inner loop only takes 1 cycle due to its
 *  pipelining etc. Cpudelay is computed as follows:
 * 	Cpudelay*cycletime(ns)*1 + cycletime(ns)*3 > = 1000
 *  The value of Cpudelay is set differently and depends on how this code
 *  is written.
 *	 DO NOT CHANGE THIS CODE WITHOUT AJDUSTING CPUDELAY.
 */

#if defined(lint)

/*ARGSUSED*/
void
drv_usecwait(clock_t n)
{}

/*ARGSUSED*/
void
usec_delay(int n)
{}

#else	/* lint */


/*
 * 	For sun4u machines, if the 2 instructions inside the inner loop
 *	reside on different cache lines, the pipeliningg will make it
 *	take only 2 cycles  instead of 1 to execute, hence increasing
 *	the time by 2x. On sun4u we calculate Cpudelay assuming the
 *	inner loop would take only 1 cycle for the 2 instr.
 *	Therefore we need to make sure they resides in
 *	the same cacheline, ie 32byte block.  Since this should not
 *	affect other archs, we have add it here. Ideally this routine
 *	should be arch-dependent.
 */
	ENTRY(drv_usecwait)
	ALTENTRY(usec_delay)
#ifdef __sparcv9cpu
	sethi	%hi(use_stick), %o5
	lduw	[%o5 + %lo(use_stick)], %o4
	brnz,a	%o4, 2f
	nop
#endif
	sethi	%hi(Cpudelay), %o5
	ld	[%o5 + %lo(Cpudelay)], %o4 ! microsecond countdown counter
	orcc	%o4, 0, %o3		! set cc bits to nz
	ba,a	1f
	! keep the next two instructions in the same i$ line
	.align	8
1:	bnz	1b			! microsecond countdown loop
	subcc	%o3, 1, %o3		! 2 instructions in loop

	subcc	%o0, 1, %o0		! now, for each microsecond...
	bg	1b			! go n times through above loop
	orcc    %o4, 0, %o3
	retl
	nop
#ifdef __sparcv9cpu
2:
/*
 * With UltraSparc III the combination of supporting mixed
 * speed CPUs and variable clock rate for power management
 * precludes using the instruction count loop above.
 */
	sethi	%hi(cpu_clock_mhz), %o5
	ld	[%o5 + %lo(cpu_clock_mhz)], %o4
	mulx	%o4, %o0, %o0		! Scale to usec to stick/tick
	rd	STICK, %o4		! Grab initial stick
	add	%o0, %o4, %o5		! Calculate target stick
1:
	subcc	%o5, %o4, %o4
	bge	1b
	rd	STICK, %o4		! Grab stick
	retl
	nop
#endif
	SET_SIZE(usec_delay)
	SET_SIZE(drv_usecwait)

#endif	/* lint */
