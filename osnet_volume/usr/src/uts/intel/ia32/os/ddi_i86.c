/*
 * Copyright (c) 1993-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_i86.c	1.23	99/07/14 SMI"

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>


/*
 * DDI DMA Engine functions for x86.
 * These functions are more naturally generic, but do not apply to SPARC.
 */

int
ddi_dmae_alloc(dev_info_t *dip, int chnl, int (*dmae_waitfp)(), caddr_t arg)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_ACQUIRE,
	    (off_t *)dmae_waitfp, (size_t *)arg, (caddr_t *)chnl, 0));
}

int
ddi_dmae_release(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_FREE, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_getlim(dev_info_t *dip, ddi_dma_lim_t *limitsp)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_GETLIM, 0, 0,
	    (caddr_t *)limitsp, 0));
}

int
ddi_dmae_getattr(dev_info_t *dip, ddi_dma_attr_t *attrp)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_GETATTR, 0, 0,
	    (caddr_t *)attrp, 0));
}

int
ddi_dmae_1stparty(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_1STPTY, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_prog(dev_info_t *dip, struct ddi_dmae_req *dmaereqp,
	ddi_dma_cookie_t *cookiep, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_PROG, (off_t *)dmaereqp,
	    (size_t *)cookiep, (caddr_t *)chnl, 0));
}

int
ddi_dmae_swsetup(dev_info_t *dip, struct ddi_dmae_req *dmaereqp,
	ddi_dma_cookie_t *cookiep, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_SWSETUP, (off_t *)dmaereqp,
	    (size_t *)cookiep, (caddr_t *)chnl, 0));
}

int
ddi_dmae_swstart(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_SWSTART, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_stop(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_STOP, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_enable(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_ENABLE, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_disable(dev_info_t *dip, int chnl)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_DISABLE, 0, 0,
	    (caddr_t *)chnl, 0));
}

int
ddi_dmae_getcnt(dev_info_t *dip, int chnl, int *countp)
{
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_E_GETCNT, 0, (size_t *)countp,
	    (caddr_t *)chnl, 0));
}

/*
 * implementation specific access handle and routines:
 */

static uintptr_t impl_acc_hdl_id = 0;

/*
 * access handle allocator
 */
ddi_acc_hdl_t *
impl_acc_hdl_get(ddi_acc_handle_t hdl)
{
	/*
	 * recast to ddi_acc_hdl_t instead of
	 * casting to ddi_acc_impl_t and then return the ah_platform_private
	 *
	 * this optimization based on the ddi_acc_hdl_t is the
	 * first member of the ddi_acc_impl_t.
	 */
	return ((ddi_acc_hdl_t *)hdl);
}

ddi_acc_handle_t
impl_acc_hdl_alloc(int (*waitfp)(caddr_t), caddr_t arg)
{
	ddi_acc_impl_t *hp;
	int sleepflag;

	sleepflag = ((waitfp == (int (*)())KM_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	/*
	 * Allocate and initialize the data access handle.
	 */
	hp = kmem_zalloc(sizeof (ddi_acc_impl_t), sleepflag);
	if (!hp) {
		if ((waitfp != (int (*)())KM_SLEEP) &&
			(waitfp != (int (*)())KM_NOSLEEP))
			ddi_set_callback(waitfp, arg, &impl_acc_hdl_id);
		return (NULL);
	}

	hp->ahi_common.ah_platform_private = (void *)hp;
	return ((ddi_acc_handle_t)hp);
}

void
impl_acc_hdl_free(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hp;

	hp = (ddi_acc_impl_t *)handle;
	if (hp) {
		kmem_free(hp, sizeof (*hp));
		if (impl_acc_hdl_id)
			ddi_run_callback(&impl_acc_hdl_id);
	}
}

void
impl_acc_hdl_init(ddi_acc_hdl_t *handlep)
{
	ddi_acc_impl_t *hp;

	if (!handlep)
		return;
	hp = (ddi_acc_impl_t *)handlep->ah_platform_private;

	if (hp->ahi_acc_attr & DDI_ACCATTR_IO_SPACE) {
		hp->ahi_get8 = i_ddi_io_get8;
		hp->ahi_put8 = i_ddi_io_put8;
		hp->ahi_rep_get8 = i_ddi_io_rep_get8;
		hp->ahi_rep_put8 = i_ddi_io_rep_put8;

		/* temporary set these 64 functions to no-ops */
		hp->ahi_get64 = i_ddi_io_get64;
		hp->ahi_put64 = i_ddi_io_put64;
		hp->ahi_rep_get64 = i_ddi_io_rep_get64;
		hp->ahi_rep_put64 = i_ddi_io_rep_put64;

		/*
		 * check for BIG endian access
		 */
		if (handlep->ah_acc.devacc_attr_endian_flags ==
			DDI_STRUCTURE_BE_ACC) {
			hp->ahi_get16 = i_ddi_io_swap_get16;
			hp->ahi_get32 = i_ddi_io_swap_get32;
			hp->ahi_put16 = i_ddi_io_swap_put16;
			hp->ahi_put32 = i_ddi_io_swap_put32;
			hp->ahi_rep_get16 = i_ddi_io_swap_rep_get16;
			hp->ahi_rep_get32 = i_ddi_io_swap_rep_get32;
			hp->ahi_rep_put16 = i_ddi_io_swap_rep_put16;
			hp->ahi_rep_put32 = i_ddi_io_swap_rep_put32;
		} else {
			hp->ahi_acc_attr |= DDI_ACCATTR_DIRECT;
			hp->ahi_get16 = i_ddi_io_get16;
			hp->ahi_get32 = i_ddi_io_get32;
			hp->ahi_put16 = i_ddi_io_put16;
			hp->ahi_put32 = i_ddi_io_put32;
			hp->ahi_rep_get16 = i_ddi_io_rep_get16;
			hp->ahi_rep_get32 = i_ddi_io_rep_get32;
			hp->ahi_rep_put16 = i_ddi_io_rep_put16;
			hp->ahi_rep_put32 = i_ddi_io_rep_put32;
		}

	} else if (hp->ahi_acc_attr & DDI_ACCATTR_CPU_VADDR) {

		hp->ahi_get8 = i_ddi_vaddr_get8;
		hp->ahi_put8 = i_ddi_vaddr_put8;
		hp->ahi_rep_get8 = i_ddi_vaddr_rep_get8;
		hp->ahi_rep_put8 = i_ddi_vaddr_rep_put8;

		/*
		 * check for BIG endian access
		 */
		if (handlep->ah_acc.devacc_attr_endian_flags ==
			DDI_STRUCTURE_BE_ACC) {

			hp->ahi_get16 = i_ddi_vaddr_swap_get16;
			hp->ahi_get32 = i_ddi_vaddr_swap_get32;
			hp->ahi_get64 = i_ddi_vaddr_swap_get64;
			hp->ahi_put16 = i_ddi_vaddr_swap_put16;
			hp->ahi_put32 = i_ddi_vaddr_swap_put32;
			hp->ahi_put64 = i_ddi_vaddr_swap_put64;
			hp->ahi_rep_get16 = i_ddi_vaddr_swap_rep_get16;
			hp->ahi_rep_get32 = i_ddi_vaddr_swap_rep_get32;
			hp->ahi_rep_get64 = i_ddi_vaddr_swap_rep_get64;
			hp->ahi_rep_put16 = i_ddi_vaddr_swap_rep_put16;
			hp->ahi_rep_put32 = i_ddi_vaddr_swap_rep_put32;
			hp->ahi_rep_put64 = i_ddi_vaddr_swap_rep_put64;
		} else {
			hp->ahi_acc_attr |= DDI_ACCATTR_DIRECT;
			hp->ahi_get16 = i_ddi_vaddr_get16;
			hp->ahi_get32 = i_ddi_vaddr_get32;
			hp->ahi_get64 = i_ddi_vaddr_get64;
			hp->ahi_put16 = i_ddi_vaddr_put16;
			hp->ahi_put32 = i_ddi_vaddr_put32;
			hp->ahi_put64 = i_ddi_vaddr_put64;
			hp->ahi_rep_get16 = i_ddi_vaddr_rep_get16;
			hp->ahi_rep_get32 = i_ddi_vaddr_rep_get32;
			hp->ahi_rep_get64 = i_ddi_vaddr_rep_get64;
			hp->ahi_rep_put16 = i_ddi_vaddr_rep_put16;
			hp->ahi_rep_put32 = i_ddi_vaddr_rep_put32;
			hp->ahi_rep_put64 = i_ddi_vaddr_rep_put64;
		}
	}
	hp->ahi_fault_check = i_ddi_acc_fault_check;
	hp->ahi_fault_notify = i_ddi_acc_fault_notify;
}

/*
 * The followings are low-level routines for data access.
 *
 * All of these routines should be implemented in assembly. Those
 * that have been rewritten be found in ~ml/ddi_i86_asm.s
 */

/*ARGSUSED*/
uint16_t
i_ddi_vaddr_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (ddi_swap16(*addr));
}

/*ARGSUSED*/
uint16_t
i_ddi_io_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (ddi_swap16(inw((uint_t)addr)));
}

/*ARGSUSED*/
uint32_t
i_ddi_vaddr_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (ddi_swap32(*addr));
}

/*ARGSUSED*/
uint32_t
i_ddi_io_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (ddi_swap32(inl((uint_t)addr)));
}

/*ARGSUSED*/
uint64_t
i_ddi_vaddr_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return (ddi_swap64(*addr));
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	*addr = ddi_swap16(value);
}

/*ARGSUSED*/
void
i_ddi_io_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	outw((uint_t)addr, ddi_swap16(value));
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	*addr = ddi_swap32(value);
}

/*ARGSUSED*/
void
i_ddi_io_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	outl((uint_t)addr, ddi_swap32(value));
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	*addr = ddi_swap64(value);
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t	*h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = *d++;
	else
		for (; repcount; repcount--)
			*h++ = *d;
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = *d++;
	else
		for (; repcount; repcount--)
			*h++ = *d;
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = ddi_swap16(*d++);
	else
		for (; repcount; repcount--)
			*h++ = ddi_swap16(*d);
}

/*ARGSUSED*/
void
i_ddi_io_swap_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h;
	int port;

	h = host_addr;
	port = (int)dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--, port += 2)
			*h++ = ddi_swap16(inw(port));
	else
		for (; repcount; repcount--)
			*h++ = ddi_swap16(inw(port));
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = *d++;
	else
		for (; repcount; repcount--)
			*h++ = *d;
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = ddi_swap32(*d++);
	else
		for (; repcount; repcount--)
			*h++ = ddi_swap32(*d);
}

/*ARGSUSED*/
void
i_ddi_io_swap_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h;
	int port;

	h = host_addr;
	port = (int)dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--, port += 4)
			*h++ = ddi_swap32(inl(port));
	else
		for (; repcount; repcount--)
			*h++ = ddi_swap32(inl(port));
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = *d++;
	else
		for (; repcount; repcount--)
			*h++ = *d;
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = ddi_swap64(*d++);
	else
		for (; repcount; repcount--)
			*h++ = ddi_swap64(*d);
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t	*h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*d++ = *h++;
	else
		for (; repcount; repcount--)
			*d = *h++;
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*d++ = *h++;
	else
		for (; repcount; repcount--)
			*d = *h++;
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*d++ = ddi_swap16(*h++);
	else
		for (; repcount; repcount--)
			*d = ddi_swap16(*h++);
}

/*ARGSUSED*/
void
i_ddi_io_swap_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h;
	int port;

	h = host_addr;
	port = (int)dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--, port += 2)
			outw(port, ddi_swap16(*h++));
	else
		for (; repcount; repcount--)
			outw(port, ddi_swap16(*h++));
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*d++ = *h++;
	else
		for (; repcount; repcount--)
			*d = *h++;
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*d++ = ddi_swap32(*h++);
	else
		for (; repcount; repcount--)
			*d = ddi_swap32(*h++);
}

/*ARGSUSED*/
void
i_ddi_io_swap_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h;
	int port;

	h = host_addr;
	port = (int)dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--, port += 4)
			outl(port, ddi_swap32(*h++));
	else
		for (; repcount; repcount--)
			outl(port, ddi_swap32(*h++));
}

/*ARGSUSED*/
void
i_ddi_vaddr_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*d++ = *h++;
	else
		for (; repcount; repcount--)
			*d = *h++;
}

/*ARGSUSED*/
void
i_ddi_vaddr_swap_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*d++ = ddi_swap64(*h++);
	else
		for (; repcount; repcount--)
			*d = ddi_swap64(*h++);
}

/*ARGSUSED*/
uint64_t
i_ddi_io_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	cmn_err(CE_PANIC, "ddi_get64 from i/o space");
	return (0);
}

/*ARGSUSED*/
void
i_ddi_io_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr, uint64_t value)
{
	cmn_err(CE_PANIC, "ddi_put64 to i/o space");
}

#ifdef _LP64
void
ddi_io_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_getb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get8)
		((ddi_acc_impl_t *)handle, host_addr, dev_addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_io_rep_get16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_getw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get16)
		((ddi_acc_impl_t *)handle, host_addr, dev_addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_io_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_getl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_get32)
		((ddi_acc_impl_t *)handle, host_addr, dev_addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

/*ARGSUSED*/
void
i_ddi_io_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	cmn_err(CE_PANIC, "ddi_rep_get64 from i/o space");
}

#ifdef _LP64
void
ddi_io_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_putb(ddi_acc_handle_t handle,
	uint8_t *host_addr, uint8_t *dev_addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put8)
		((ddi_acc_impl_t *)handle, host_addr, dev_addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_io_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_putw(ddi_acc_handle_t handle,
	uint16_t *host_addr, uint16_t *dev_addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put16)
		((ddi_acc_impl_t *)handle, host_addr, dev_addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

#ifdef _LP64
void
ddi_io_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount)
#else /* _ILP32 */
void
ddi_io_rep_putl(ddi_acc_handle_t handle,
	uint32_t *host_addr, uint32_t *dev_addr, size_t repcount)
#endif
{
	(((ddi_acc_impl_t *)handle)->ahi_rep_put32)
		((ddi_acc_impl_t *)handle, host_addr, dev_addr,
		repcount, DDI_DEV_NO_AUTOINCR);
}

/*ARGSUSED*/
void
i_ddi_io_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	cmn_err(CE_PANIC, "ddi_rep_put64 to i/o space");
}

/*
 * These next two functions could be translated into assembler someday
 */
int
ddi_check_acc_handle(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hdlp = (ddi_acc_impl_t *)handle;
	return ((*hdlp->ahi_fault_check)(hdlp));
}

int
i_ddi_acc_fault_check(ddi_acc_impl_t *hdlp)
{
	/* Default version, just returns flag value */
	return (hdlp->ahi_fault);
}

/*ARGSUSED*/
void
i_ddi_acc_fault_notify(ddi_acc_impl_t *hdlp)
{
	/* Default version, does nothing for now */
}

void
i_ddi_acc_set_fault(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hdlp = (ddi_acc_impl_t *)handle;

	if (!hdlp->ahi_fault) {
		hdlp->ahi_fault = 1;
		(*hdlp->ahi_fault_notify)(hdlp);
	}
}

void
i_ddi_acc_clr_fault(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hdlp = (ddi_acc_impl_t *)handle;

	if (hdlp->ahi_fault) {
		hdlp->ahi_fault = 0;
		(*hdlp->ahi_fault_notify)(hdlp);
	}
}
