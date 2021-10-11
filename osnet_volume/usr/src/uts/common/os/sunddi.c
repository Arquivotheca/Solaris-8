/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sunddi.c	1.198	99/11/01 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/model.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/open.h>
#include <sys/user.h>
#include <sys/t_lock.h>
#include <sys/vm.h>
#include <sys/stat.h>
#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/as.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/sunndi.h>
#include <sys/kstat.h>
#include <sys/conf.h>
#include <sys/ddi_impldefs.h>	/* include implementation structure defs */
#include <sys/ndi_impldefs.h>	/* include prototypes */
#include <sys/hwconf.h>
#include <sys/pathname.h>
#include <sys/modctl.h>
#include <sys/epm.h>
#include <sys/devctl.h>
#include <sys/callb.h>
#include <sys/dc_ki.h>
#include <sys/cladm.h>
#include <sys/devfs_log_event.h>
#include <sys/dacf_impl.h>
#include <sys/ddidevmap.h>

#ifdef __sparcv9cpu
#include <sys/archsystm.h>	/* XXX	getpil/setpil */
#include <sys/membar.h>		/* XXX	membar_sync */
#endif

/*
 * operations vector for the functions exported to the kernel by
 * the clients of the device configuration server
 * Fill in with stubs to find out who calls before dc is init'ed.
 */
struct dc_ops dcops = {
	DCOPS_VERSION,
	dcstub_get_major,
	dcstub_free_major,
	dcstub_major_name,
	dcstub_sync_instances,
	dcstub_instance_path,
	dcstub_get_instance,
	dcstub_free_instance,
	dcstub_map_minor,
	dcstub_unmap_minor,
	dcstub_resolve_minor,
	dcstub_devconfig_lock,
	dcstub_devconfig_unlock,
	dcstub_service_config
};

/*
 * DDI(Sun) Function and flag definitions:
 */

#if defined(i386)
/*
 * Used to indicate which entries were chosen from a range.
 */
char	*chosen_reg = "chosen-reg";
#endif


/*
 * Function used to ring system console bell
 */
void (*ddi_console_bell_func)(clock_t duration);

int i_ddi_walk_devs(dev_info_t *, int (*f)(dev_info_t *, void *), void *);

static int i_ddi_resolve_pathname(char *, dev_info_t **, dev_t *);

/*
 * Creating register mappings and handling interrupts:
 */

/*
 * Generic ddi_map: Call parent to fulfill request...
 */

int
ddi_map(dev_info_t *dp, ddi_map_req_t *mp, off_t offset,
    off_t len, caddr_t *addrp)
{
	dev_info_t *pdip;

	ASSERT(dp);
	pdip = (dev_info_t *)DEVI(dp)->devi_parent;
	return ((DEVI(pdip)->devi_ops->devo_bus_ops->bus_map)(pdip,
	    dp, mp, offset, len, addrp));
}

/*
 * ddi_apply_range: (Called by nexi only.)
 * Apply ranges in parent node dp, to child regspec rp...
 */

int
ddi_apply_range(dev_info_t *dp, dev_info_t *rdip, struct regspec *rp)
{
	return (i_ddi_apply_range(dp, rdip, rp));
}

int
ddi_map_regs(dev_info_t *dip, uint_t rnumber, caddr_t *kaddrp, off_t offset,
    off_t len)
{
	ddi_map_req_t mr;
#if defined(i386)
	struct {
		int	bus;
		int	addr;
		int	size;
	} reg, *reglist;
	uint_t	length;
	int	rc;

	/*
	 * get the 'registers' or the 'reg' property.
	 * We look up the reg property as an array of
	 * int's.
	 */
	rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "registers", (int **)&reglist, &length);
	if (rc != DDI_PROP_SUCCESS)
		rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
				DDI_PROP_DONTPASS, "reg",
				(int **)&reglist, &length);
	if (rc == DDI_PROP_SUCCESS) {
		/*
		 * point to the required entry.
		 */
		reg = reglist[rnumber];
		reg.addr += offset;
		if (len != 0)
			reg.size = len;
		/*
		 * make a new property containing ONLY the required tuple.
		 */
		if (ddi_prop_update_int_array(DDI_DEV_T_NONE, dip,
		    chosen_reg, (int *)&reg, (sizeof (reg)/sizeof (int)))
		    != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN, "%s%d: cannot create '%s' "
			    "property", DEVI(dip)->devi_name,
			    DEVI(dip)->devi_instance, chosen_reg);
		}
		/*
		 * free the memory allocated by
		 * ddi_prop_lookup_int_array ().
		 */
		ddi_prop_free((void *)reglist);
	}

#endif
	mr.map_op = DDI_MO_MAP_LOCKED;
	mr.map_type = DDI_MT_RNUMBER;
	mr.map_obj.rnumber = rnumber;
	mr.map_prot = PROT_READ | PROT_WRITE;
	mr.map_flags = DDI_MF_KERNEL_MAPPING;
	mr.map_handlep = NULL;
	mr.map_vers = DDI_MAP_VERSION;

	/*
	 * Call my parent to map in my regs.
	 */

	return (ddi_map(dip, &mr, offset, len, kaddrp));
}

void
ddi_unmap_regs(dev_info_t *dip, uint_t rnumber, caddr_t *kaddrp, off_t offset,
    off_t len)
{
	ddi_map_req_t mr;

	mr.map_op = DDI_MO_UNMAP;
	mr.map_type = DDI_MT_RNUMBER;
	mr.map_flags = DDI_MF_KERNEL_MAPPING;
	mr.map_prot = PROT_READ | PROT_WRITE;	/* who cares? */
	mr.map_obj.rnumber = rnumber;
	mr.map_handlep = NULL;
	mr.map_vers = DDI_MAP_VERSION;

	/*
	 * Call my parent to unmap my regs.
	 */

	(void) ddi_map(dip, &mr, offset, len, kaddrp);
	*kaddrp = (caddr_t)0;
#if defined(i386)
	(void) ddi_prop_remove(DDI_DEV_T_NONE, dip, chosen_reg);
#endif
}

int
ddi_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	return (i_ddi_bus_map(dip, rdip, mp, offset, len, vaddrp));
}

/*
 * nullbusmap:	The/DDI default bus_map entry point for nexi
 *		not conforming to the reg/range paradigm (i.e. scsi, etc.)
 *		with no HAT/MMU layer to be programmed at this level.
 *
 *		If the call is to map by rnumber, return an error,
 *		otherwise pass anything else up the tree to my parent.
 *
 *		XXX: Is the name `nullbusmap' misleading?
 */

/*ARGSUSED*/
int
nullbusmap(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	if (mp->map_type == DDI_MT_RNUMBER)
		return (DDI_ME_UNSUPPORTED);

	return (ddi_map(dip, mp, offset, len, vaddrp));
}

/*
 * ddi_rnumber_to_regspec: Not for use by leaf drivers.
 *			   Only for use by nexi using the reg/range paradigm.
 */
struct regspec *
ddi_rnumber_to_regspec(dev_info_t *dip, int rnumber)
{
	return (i_ddi_rnumber_to_regspec(dip, rnumber));
}

/*
 * Peek and poke whilst giving a chance for the nexus drivers to
 * intervene to flush write buffers for us.
 *
 * Note that we allow the dip to be nil because we may be called
 * prior even to the instantiation of the devinfo tree itself - all
 * regular leaf and nexus drivers should always use a non-nil dip!
 *
 * We treat peek in a somewhat cavalier fashion .. assuming that we'll
 * simply get a synchronous fault as soon as we touch a missing address.
 *
 * Poke is rather more carefully handled because we might poke to a write
 * buffer, "succeed", then only find some time later that we got an
 * asynchronous fault that indicated that the address we were writing to
 * was not really backed by hardware.
 */

#ifndef	__sparcv9cpu
/*ARGSUSED*/
int
common_ddi_peek(dev_info_t *devi, size_t size,
    void *addr, void *value_p)
{
	label_t *saved_jb;
	int err = DDI_SUCCESS;
	label_t jb;
	longlong_t trash;

	/*
	 * arrange that peeking to a nil destination pointer silently succeeds
	 */
	if (value_p == (void *)0)
		value_p = &trash;

	saved_jb = curthread->t_nofault;
	curthread->t_nofault = &jb;

	if (!setjmp(&jb)) {
		switch (size) {
		case sizeof (int8_t):
			*(int8_t *)value_p = *(int8_t *)addr;
			break;

		case sizeof (int16_t):
			*(int16_t *)value_p = *(int16_t *)addr;
			break;

		case sizeof (int32_t):
			*(int32_t *)value_p = *(int32_t *)addr;
			break;

/* XXX Merced -- fix me */
#if !(defined(lint) || defined(__lint)) || !defined(__ia64)
		case sizeof (int64_t):
			*(int64_t *)value_p = *(int64_t *)addr;
			break;
#endif

		default:
			err = DDI_FAILURE;
			break;
		}
					/* if we get to here, it worked */
	} else {
		err = DDI_FAILURE;	/* else .. a fault occurred */
	}

	curthread->t_nofault = saved_jb;
	return (err);
}
#endif	/* !__sparcv9cpu */

/*
 * Keep ddi_peek() and ddi_poke() in case 3rd parties are calling this.
 * they shouldn't be, but the 9f manpage kind of pseudo exposes it.
 */
int
ddi_peek(dev_info_t *devi, size_t size, void *addr, void *value_p)
{
	return (i_ddi_peek(devi, size, addr, value_p));
}

int
ddi_poke(dev_info_t *devi, size_t size, void *addr, void *value_p)
{
	return (i_ddi_poke(devi, size, addr, value_p));
}

#ifdef _LP64
int
ddi_peek8(dev_info_t *dip, int8_t *addr, int8_t *val_p)
#else /* _ILP32 */
int
ddi_peekc(dev_info_t *dip, int8_t *addr, int8_t *val_p)
#endif
{
	return (i_ddi_peek(dip, sizeof (*val_p), addr, val_p));
}

#ifdef _LP64
int
ddi_peek16(dev_info_t *dip, int16_t *addr, int16_t *val_p)
#else /* _ILP32 */
int
ddi_peeks(dev_info_t *dip, int16_t *addr, int16_t *val_p)
#endif
{
	return (i_ddi_peek(dip, sizeof (*val_p), addr, val_p));
}

#ifdef _LP64
int
ddi_peek32(dev_info_t *dip, int32_t *addr, int32_t *val_p)
#else /* _ILP32 */
int
ddi_peekl(dev_info_t *dip, int32_t *addr, int32_t *val_p)
#endif
{
	return (i_ddi_peek(dip, sizeof (*val_p), addr, val_p));
}

#ifdef _LP64
int
ddi_peek64(dev_info_t *dip, int64_t *addr, int64_t *val_p)
#else /* _ILP32 */
int
ddi_peekd(dev_info_t *dip, int64_t *addr, int64_t *val_p)
#endif
{
	return (i_ddi_peek(dip, sizeof (*val_p), addr, val_p));
}


#ifndef	__sparcv9cpu
int
common_ddi_poke(dev_info_t *devi, size_t size,
    void *addr, void *value_p)
{
	label_t *saved_jb;
	int err = DDI_SUCCESS;
	label_t jb;

	saved_jb = curthread->t_nofault;
	curthread->t_nofault = &jb;

	if (!setjmp(&jb)) {

		/*
		 * Inform our parent nexi what we're about to do, giving them
		 * an early opportunity to tell us not to even try.
		 */
		if (devi && ddi_ctlops(devi, devi, DDI_CTLOPS_POKE_INIT,
		    addr, 0) != DDI_SUCCESS) {
			curthread->t_nofault = saved_jb;
			return (DDI_FAILURE);
		}

		switch (size) {
		case sizeof (int8_t):
			*(int8_t *)addr = *(int8_t *)value_p;
			break;

		case sizeof (int16_t):
			*(int16_t *)addr = *(int16_t *)value_p;
			break;

		case sizeof (int32_t):
			*(int32_t *)addr = *(int32_t *)value_p;
			break;

/* XXX Merced -- fix me */
#if !(defined(lint) || defined(__lint)) || !defined(__ia64)
		case sizeof (int64_t):
			*(int64_t *)addr = *(int64_t *)value_p;
			break;
#endif

		default:
			err = DDI_FAILURE;
			break;
		}

		/*
		 * Now give our parent(s) a chance to ensure that what we
		 * did really propagated through any intermediate buffers,
		 * returning failure if we detected any problems .. more
		 * likely though, the resulting flush will cause us to
		 * longjmp into the 'else' clause below ..
		 */
		if (devi && ddi_ctlops(devi, devi, DDI_CTLOPS_POKE_FLUSH,
		    addr, (void *)0) != DDI_SUCCESS)
			err = DDI_FAILURE;
		curthread->t_nofault = saved_jb;
	} else {
		err = DDI_FAILURE;		/* a fault occurred */
		curthread->t_nofault = saved_jb;
	}

	/*
	 * Give our parents a chance to tidy up after us.  If
	 * 'tidying up' causes faults, we crash and burn.
	 */
	if (devi)
		(void) ddi_ctlops(devi, devi, DDI_CTLOPS_POKE_FINI,
		    addr, (void *)0);

	return (err);
}
#endif	/* !__sparcv9cpu */

#ifdef _LP64
int
ddi_poke8(dev_info_t *dip, int8_t *addr, int8_t val)
#else /* _ILP32 */
int
ddi_pokec(dev_info_t *dip, int8_t *addr, int8_t val)
#endif
{
	return (i_ddi_poke(dip, sizeof (val), addr, &val));
}

#ifdef _LP64
int
ddi_poke16(dev_info_t *dip, int16_t *addr, int16_t val)
#else /* _ILP32 */
int
ddi_pokes(dev_info_t *dip, int16_t *addr, int16_t val)
#endif
{
	return (i_ddi_poke(dip, sizeof (val), addr, &val));
}

#ifdef _LP64
int
ddi_poke32(dev_info_t *dip, int32_t *addr, int32_t val)
#else /* _ILP32 */
int
ddi_pokel(dev_info_t *dip, int32_t *addr, int32_t val)
#endif
{
	return (i_ddi_poke(dip, sizeof (val), addr, &val));
}

#ifdef _LP64
int
ddi_poke64(dev_info_t *dip, int64_t *addr, int64_t val)
#else /* _ILP32 */
int
ddi_poked(dev_info_t *dip, int64_t *addr, int64_t val)
#endif
{
	return (i_ddi_poke(dip, sizeof (val), addr, &val));
}

/*
 * ddi_peekpokeio() is used primarily by the mem drivers for moving
 * data to and from uio structures via peek and poke.  Note that we
 * use "internal" routines ddi_peek and ddi_poke to make this go
 * slightly faster, avoiding the call overhead ..
 */
int
ddi_peekpokeio(dev_info_t *devi, struct uio *uio, enum uio_rw rw,
    caddr_t addr, size_t len, uint_t xfersize)
{
	int64_t	ibuffer;
	int8_t w8;
	size_t sz;
	int o;

	if (xfersize > sizeof (long))
		xfersize = sizeof (long);

	while (len != 0) {
		if ((len | (uintptr_t)addr) & 1) {
			sz = sizeof (int8_t);
			if (rw == UIO_WRITE) {
				if ((o = uwritec(uio)) == -1)
					return (DDI_FAILURE);
				if (ddi_poke8(devi, (int8_t *)addr,
				    (int8_t)o) != DDI_SUCCESS)
					return (DDI_FAILURE);
			} else {
				if (i_ddi_peek(devi, sz, (int8_t *)addr,
				    &w8) != DDI_SUCCESS)
					return (DDI_FAILURE);
				if (ureadc(w8, uio))
					return (DDI_FAILURE);
			}
		} else {
			switch (xfersize) {
/* XXX Merced -- fix me */
#if !(defined(lint) || defined(__lint)) || !defined(__ia64)
			case sizeof (int64_t):
				if (((len | (uintptr_t)addr) &
				    (sizeof (int64_t) - 1)) == 0) {
					sz = xfersize;
					break;
				}
				/*FALLTHROUGH*/
#endif
			case sizeof (int32_t):
				if (((len | (uintptr_t)addr) &
				    (sizeof (int32_t) - 1)) == 0) {
					sz = xfersize;
					break;
				}
				/*FALLTHROUGH*/
			default:
				/*
				 * This still assumes that we might have an
				 * I/O bus out there that permits 16-bit
				 * transfers (and that it would be upset by
				 * 32-bit transfers from such locations).
				 */
				sz = sizeof (int16_t);
				break;
			}

			if (rw == UIO_READ) {
				if (i_ddi_peek(devi, sz, addr,
				    &ibuffer) != DDI_SUCCESS)
					return (DDI_FAILURE);
			}

			if (uiomove(&ibuffer, sz, rw, uio))
				return (DDI_FAILURE);

			if (rw == UIO_WRITE) {
				if (i_ddi_poke(devi, sz, addr,
				    &ibuffer) != DDI_SUCCESS)
					return (DDI_FAILURE);
			}
		}
		addr += sz;
		len -= sz;
	}
	return (DDI_SUCCESS);
}

/*
 * These routines are used by drivers that do layered ioctls
 * On sparc, they're implemented in assembler to avoid spilling
 * register windows in the common (copyin) case ..
 */
#ifndef	sparc
int
ddi_copyin(const void *buf, void *kernbuf, size_t size, int flags)
{
	if (flags & FKIOCTL)
		return (kcopy(buf, kernbuf, size) ? -1 : 0);
	return (copyin(buf, kernbuf, size));
}
#endif	/* !sparc */

#ifndef	sparc
int
ddi_copyout(const void *buf, void *kernbuf, size_t size, int flags)
{
	if (flags & FKIOCTL)
		return (kcopy(buf, kernbuf, size) ? -1 : 0);
	return (copyout(buf, kernbuf, size));
}
#endif	/* !sparc */

/*
 * Conversions in nexus pagesize units.  We don't duplicate the
 * 'nil dip' semantics of peek/poke because btopr/btop/ptob are DDI/DKI
 * routines anyway.
 */
unsigned long
ddi_btop(dev_info_t *dip, unsigned long bytes)
{
	unsigned long pages;

	(void) ddi_ctlops(dip, dip, DDI_CTLOPS_BTOP, &bytes, &pages);
	return (pages);
}

unsigned long
ddi_btopr(dev_info_t *dip, unsigned long bytes)
{
	unsigned long pages;

	(void) ddi_ctlops(dip, dip, DDI_CTLOPS_BTOPR, &bytes, &pages);
	return (pages);
}

unsigned long
ddi_ptob(dev_info_t *dip, unsigned long pages)
{
	unsigned long bytes;

	(void) ddi_ctlops(dip, dip, DDI_CTLOPS_PTOB, &pages, &bytes);
	return (bytes);
}

int
ddi_intr_hilevel(dev_info_t *dip, uint_t inumber)
{
	return (i_ddi_intr_hilevel(dip, inumber));
}

int
ddi_get_iblock_cookie(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep)
{
	ddi_iblock_cookie_t	c;
	int	error;

	ASSERT(iblock_cookiep != NULL);

	error = ddi_add_intr(dip, inumber, &c, NULL, nullintr, NULL);
	if (error != DDI_SUCCESS)
		return (error);
	ddi_remove_intr(dip, inumber, c);

	*iblock_cookiep = c;
	return (DDI_SUCCESS);
}

int
ddi_get_soft_iblock_cookie(dev_info_t *dip, int preference,
    ddi_iblock_cookie_t *iblock_cookiep)
{
	ddi_iblock_cookie_t	c;
	int	error;
	ddi_softintr_t	id;

	ASSERT(iblock_cookiep != NULL);

	error = ddi_add_softintr(dip, preference, &id, &c, NULL,
	    nullintr, NULL);
	if (error != DDI_SUCCESS)
		return (error);
	ddi_remove_softintr(id);

	*iblock_cookiep = c;
	return (DDI_SUCCESS);
}

/* Comments in <sys/sunddi.h> */
int
ddi_add_intr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg)
{
	return (i_ddi_add_intr(dip, inumber, iblock_cookiep, idevice_cookiep,
	    int_handler, int_handler_arg));
}

int
ddi_add_fastintr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*hi_int_handler)(void))
{
	return (i_ddi_add_fastintr(dip, inumber, iblock_cookiep,
	    idevice_cookiep, hi_int_handler));
}

int
ddi_add_softintr(dev_info_t *dip, int preference, ddi_softintr_t *idp,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg)
{
	return (i_ddi_add_softintr(dip, preference, idp, iblock_cookiep,
	    idevice_cookiep, int_handler, int_handler_arg));
}

void
ddi_trigger_softintr(ddi_softintr_t id)
{
	i_ddi_trigger_softintr(id);
}

void
ddi_remove_softintr(ddi_softintr_t id)
{
	i_ddi_remove_softintr(id);
}

void
ddi_remove_intr(dev_info_t *dip, uint_t inum, ddi_iblock_cookie_t iblock_cookie)
{
	i_ddi_remove_intr(dip, inum, iblock_cookie);
}

unsigned int
ddi_enter_critical(void)
{
	return ((uint_t)spl7());
}

void
ddi_exit_critical(unsigned int spl)
{
	splx((int)spl);
}

/*
 * Nexus ctlops punter
 */

#if !defined(sparc) && !defined(i386)
/*
 * Request bus_ctl parent to handle a bus_ctl request
 *
 * (In sparc_ddi.s or ddi_i86_asm.s)
 */
int
ddi_ctlops(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t op, void *a, void *v)
{
	int (*fp)();

	if (!d || !r)
		return (DDI_FAILURE);

	if ((d = (dev_info_t *)DEVI(d)->devi_bus_ctl) == NULL)
		return (DDI_FAILURE);

	fp = DEVI(d)->devi_ops->devo_bus_ops->bus_ctl;
	return ((*fp)(d, r, op, a, v));
}
#endif

/*
 * DMA/DVMA setup
 */

#ifdef sparc
static ddi_dma_lim_t standard_limits = {
	(uint_t)0,	/* addr_t dlim_addr_lo */
	(uint_t)-1,	/* addr_t dlim_addr_hi */
	(uint_t)-1,	/* uint_t dlim_cntr_max */
	(uint_t)1,	/* uint_t dlim_burstsizes */
	(uint_t)1,	/* uint_t dlim_minxfer */
	0		/* uint_t dlim_dmaspeed */
};
#endif
#ifdef i386
static ddi_dma_lim_t standard_limits = {
	(uint_t)0,		/* addr_t dlim_addr_lo */
	(uint_t)0xffffff,	/* addr_t dlim_addr_hi */
	(uint_t)0,		/* uint_t dlim_cntr_max */
	(uint_t)0x00000001,	/* uint_t dlim_burstsizes */
	(uint_t)DMA_UNIT_8,	/* uint_t dlim_minxfer */
	(uint_t)0,		/* uint_t dlim_dmaspeed */
	(uint_t)0x86<<24+0,	/* uint_t dlim_version */
	(uint_t)0xffff,		/* uint_t dlim_adreg_max */
	(uint_t)0xffff,		/* uint_t dlim_ctreg_max */
	(uint_t)512,		/* uint_t dlim_granular */
	(int)1,			/* int dlim_sgllen */
	(uint_t)0xffffffff	/* uint_t dlim_reqsizes */
};
#endif
#ifdef __ia64
static ddi_dma_lim_t standard_limits = {
	(uint_t)0,		/* addr_t dlim_addr_lo */
	(uint_t)0xffffff,	/* addr_t dlim_addr_hi */
	(uint_t)0,		/* uint_t dlim_cntr_max */
	(uint_t)0x00000001,	/* uint_t dlim_burstsizes */
	(uint_t)DMA_UNIT_8,	/* uint_t dlim_minxfer */
	(uint_t)0,		/* uint_t dlim_dmaspeed */
	(uint_t)0x86<<24+0,	/* uint_t dlim_version */
	(uint_t)0xffff,		/* uint_t dlim_adreg_max */
	(uint_t)0xffff,		/* uint_t dlim_ctreg_max */
	(uint_t)512,		/* uint_t dlim_granular */
	(int)1,			/* int dlim_sgllen */
	(uint_t)0xffffffff	/* uint_t dlim_reqsizes */
};
#endif

int
ddi_dma_setup(dev_info_t *dip, struct ddi_dma_req *dmareqp,
    ddi_dma_handle_t *handlep)
{
	int (*funcp)() = ddi_dma_map;
	struct bus_ops *bop;
#ifdef sparc
	auto ddi_dma_lim_t dma_lim;

	if (dmareqp->dmar_limits == (ddi_dma_lim_t *)0) {
		dma_lim = standard_limits;
	} else {
		dma_lim = *dmareqp->dmar_limits;
	}
	dmareqp->dmar_limits = &dma_lim;
#endif
#if defined(i386)
	if (dmareqp->dmar_limits == (ddi_dma_lim_t *)0)
		return (DDI_FAILURE);
#endif

	/*
	 * Handle the case that the requestor is both a leaf
	 * and a nexus driver simultaneously by calling the
	 * requestor's bus_dma_map function directly instead
	 * of ddi_dma_map.
	 */
	bop = DEVI(dip)->devi_ops->devo_bus_ops;
	if (bop && bop->bus_dma_map)
		funcp = bop->bus_dma_map;
	return ((*funcp)(dip, dip, dmareqp, handlep));
}

int
ddi_dma_addr_setup(dev_info_t *dip, struct as *as, caddr_t addr, size_t len,
    uint_t flags, int (*waitfp)(), caddr_t arg,
    ddi_dma_lim_t *limits, ddi_dma_handle_t *handlep)
{
	int (*funcp)() = ddi_dma_map;
	ddi_dma_lim_t dma_lim;
	struct ddi_dma_req dmareq;
	struct bus_ops *bop;

	if (len == 0) {
		return (DDI_DMA_NOMAPPING);
	}
	if (limits == (ddi_dma_lim_t *)0) {
		dma_lim = standard_limits;
	} else {
		dma_lim = *limits;
	}
	dmareq.dmar_limits = &dma_lim;
	dmareq.dmar_flags = flags;
	dmareq.dmar_fp = waitfp;
	dmareq.dmar_arg = arg;
	dmareq.dmar_object.dmao_size = len;
	dmareq.dmar_object.dmao_type = DMA_OTYP_VADDR;
	dmareq.dmar_object.dmao_obj.virt_obj.v_as = as;
	dmareq.dmar_object.dmao_obj.virt_obj.v_addr = addr;
	dmareq.dmar_object.dmao_obj.virt_obj.v_priv = NULL;

	/*
	 * Handle the case that the requestor is both a leaf
	 * and a nexus driver simultaneously by calling the
	 * requestor's bus_dma_map function directly instead
	 * of ddi_dma_map.
	 */
	bop = DEVI(dip)->devi_ops->devo_bus_ops;
	if (bop && bop->bus_dma_map)
		funcp = bop->bus_dma_map;

	return ((*funcp)(dip, dip, &dmareq, handlep));
}

int
ddi_dma_buf_setup(dev_info_t *dip, struct buf *bp, uint_t flags,
    int (*waitfp)(), caddr_t arg, ddi_dma_lim_t *limits,
    ddi_dma_handle_t *handlep)
{
	int (*funcp)() = ddi_dma_map;
	ddi_dma_lim_t dma_lim;
	struct ddi_dma_req dmareq;
	struct bus_ops *bop;

	if (limits == (ddi_dma_lim_t *)0) {
		dma_lim = standard_limits;
	} else {
		dma_lim = *limits;
	}
	dmareq.dmar_limits = &dma_lim;
	dmareq.dmar_flags = flags;
	dmareq.dmar_fp = waitfp;
	dmareq.dmar_arg = arg;
	dmareq.dmar_object.dmao_size = (uint_t)bp->b_bcount;

	if ((bp->b_flags & (B_PAGEIO|B_REMAPPED)) == B_PAGEIO) {
		dmareq.dmar_object.dmao_type = DMA_OTYP_PAGES;
		dmareq.dmar_object.dmao_obj.pp_obj.pp_pp = bp->b_pages;
		dmareq.dmar_object.dmao_obj.pp_obj.pp_offset =
		    (uint_t)(((uintptr_t)bp->b_un.b_addr) & MMU_PAGEOFFSET);
	} else {
		dmareq.dmar_object.dmao_type = DMA_OTYP_VADDR;
		dmareq.dmar_object.dmao_obj.virt_obj.v_addr = bp->b_un.b_addr;
		if ((bp->b_flags & (B_SHADOW|B_REMAPPED)) == B_SHADOW) {
			dmareq.dmar_object.dmao_obj.virt_obj.v_priv =
							bp->b_shadow;
		} else {
			dmareq.dmar_object.dmao_obj.virt_obj.v_priv = NULL;
		}

		/*
		 * If the buffer has no proc pointer, or the proc
		 * struct has the kernel address space, or the buffer has
		 * been marked B_REMAPPED (meaning that it is now
		 * mapped into the kernel's address space), then
		 * the address space is kas (kernel address space).
		 */
		if (bp->b_proc == NULL || bp->b_proc->p_as == &kas ||
		    (bp->b_flags & B_REMAPPED) != 0) {
			dmareq.dmar_object.dmao_obj.virt_obj.v_as = 0;
		} else {
			dmareq.dmar_object.dmao_obj.virt_obj.v_as =
			    bp->b_proc->p_as;
		}
	}

	/*
	 * Handle the case that the requestor is both a leaf
	 * and a nexus driver simultaneously by calling the
	 * requestor's bus_dma_map function directly instead
	 * of ddi_dma_map.
	 */
	bop = DEVI(dip)->devi_ops->devo_bus_ops;
	if (bop && bop->bus_dma_map)
		funcp = bop->bus_dma_map;

	return ((*funcp)(dip, dip, &dmareq, handlep));
}

#if !defined(sparc) && !defined(i386) && !defined(__ia64)
/*
 * Request bus_dma_ctl parent to fiddle with a dma request.
 *
 * (In sparc_subr.s or ddi_i86_asm.s)
 */
int
ddi_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp, caddr_t *objp, uint_t flags)
{
	int (*fp)();

	dip = (dev_info_t *)DEVI(dip)->devi_bus_dma_ctl;
	fp = DEVI(dip)->devi_ops->devo_bus_ops->bus_dma_ctl;
	return ((*fp) (dip, rdip, handle, request, offp, lenp, objp, flags));
}
#endif

/*
 * For all DMA control functions, call the DMA control
 * routine and return status.
 *
 * Just plain assume that the parent is to be called.
 * If a nexus driver or a thread outside the framework
 * of a nexus driver or a leaf driver calls these functions,
 * it is up to them to deal with the fact that the parent's
 * bus_dma_ctl function will be the first one called.
 */

#define	HD	((ddi_dma_impl_t *)h)->dmai_rdip

int
ddi_dma_kvaddrp(ddi_dma_handle_t h, off_t off, size_t len, caddr_t *kp)
{
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_KVADDR, &off, &len, kp, 0));
}

int
ddi_dma_htoc(ddi_dma_handle_t h, off_t o, ddi_dma_cookie_t *c)
{
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_HTOC, &o, 0, (caddr_t *)c, 0));
}

int
ddi_dma_coff(ddi_dma_handle_t h, ddi_dma_cookie_t *c, off_t *o)
{
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_COFF,
	    (off_t *)c, 0, (caddr_t *)o, 0));
}

int
ddi_dma_movwin(ddi_dma_handle_t h, off_t *o, size_t *l, ddi_dma_cookie_t *c)
{
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_MOVWIN, o,
	    l, (caddr_t *)c, 0));
}

int
ddi_dma_curwin(ddi_dma_handle_t h, off_t *o, size_t *l)
{
	if ((((ddi_dma_impl_t *)h)->dmai_rflags & DDI_DMA_PARTIAL) == 0)
		return (DDI_FAILURE);
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_REPWIN, o, l, 0, 0));
}

/*
 * Note:  The astute might notice that in the next two routines
 * the SPARC case passes a pointer to a ddi_dma_win_t as the 5th
 * argument while the x86 case passes the ddi_dma_win_t directly.
 *
 * While it would be nice if the "correct" behavior was
 * platform independent and specified someplace, it isn't.
 * Until that point, what's required is that this call and
 * the relevant bus nexus drivers agree, and in this case they
 * do, at least for the cases I've looked at.
 */
int
ddi_dma_nextwin(ddi_dma_handle_t h, ddi_dma_win_t win,
    ddi_dma_win_t *nwin)
{
#ifdef sparc
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_NEXTWIN, (off_t *)&win, 0,
	    (caddr_t *)nwin, 0));
#elif defined(i386)
	return (((ddi_dma_impl_t *)h)->dmai_mctl(HD, HD, h, DDI_DMA_NEXTWIN,
		(off_t *)win, 0, (caddr_t *)nwin, 0));
#else
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_NEXTWIN,
		(off_t *)win, 0, (caddr_t *)nwin, 0));
#endif
}

int
ddi_dma_nextseg(ddi_dma_win_t win, ddi_dma_seg_t seg, ddi_dma_seg_t *nseg)
{
#ifdef sparc
	ddi_dma_handle_t h = (ddi_dma_handle_t)win;

	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_NEXTSEG, (off_t *)&win,
	    (size_t *)&seg, (caddr_t *)nseg, 0));
#else
	ddi_dma_handle_t h = (ddi_dma_handle_t)
	    ((impl_dma_segment_t *)win)->dmais_hndl;

#if	defined(i386)
	return (((ddi_dma_impl_t *)h)->dmai_mctl(HD, HD, h, DDI_DMA_NEXTSEG,
		(off_t *)win, (size_t *)seg, (caddr_t *)nseg, 0));
#else
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_NEXTSEG,
		(off_t *)win, (size_t *)seg, (caddr_t *)nseg, 0));
#endif
#endif
}

int
ddi_dma_segtocookie(ddi_dma_seg_t seg, off_t *o, off_t *l,
    ddi_dma_cookie_t *cookiep)
{
#ifdef sparc
	ddi_dma_handle_t h = (ddi_dma_handle_t)seg;

	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_SEGTOC, o, (size_t *)l,
	    (caddr_t *)cookiep, 0));
#else
	ddi_dma_handle_t h = (ddi_dma_handle_t)
	    ((impl_dma_segment_t *)seg)->dmais_hndl;

#if	defined(i386)
	return (((ddi_dma_impl_t *)h)->dmai_mctl(HD, HD, h, DDI_DMA_SEGTOC,
		o, (size_t *)l, (caddr_t *)cookiep, (uint_t)seg));
#else
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_SEGTOC,
		o, (size_t *)l, (caddr_t *)cookiep, (uint_t)seg));
#endif
#endif
}

/*
 * The SPARC versions of these routines are done in assembler to
 * save register windows, so they're in sparc_subr.s.
 */
#if !defined(sparc)

#if !defined(i386)

int
ddi_dma_sync(ddi_dma_handle_t h, off_t o, size_t l, uint_t whom)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)h;
	dev_info_t *hdip, *dip;
	int (*funcp)(dev_info_t *, dev_info_t *, ddi_dma_handle_t, off_t,
		size_t, uint_t);

	/*
	 * the DMA nexus driver will set DMP_NOSYNC if the
	 * platform does not require any sync operation. For
	 * example if the memory is uncached or consistent
	 * and without any I/O write buffers involved.
	 */
	if ((hp->dmai_rflags & DMP_NOSYNC) == DMP_NOSYNC)
		return (DDI_SUCCESS);

	dip = hp->dmai_rdip;
	hdip = (dev_info_t *)DEVI(dip)->devi_bus_dma_flush;
	funcp = DEVI(hdip)->devi_ops->devo_bus_ops->bus_dma_flush;
	return ((*funcp)(hdip, dip, h, o, l, whom));
}

#endif

int
ddi_dma_unbind_handle(ddi_dma_handle_t h)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)h;
	dev_info_t *hdip, *dip;
	int (*funcp)(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

	dip = hp->dmai_rdip;
	hdip = (dev_info_t *)DEVI(dip)->devi_bus_dma_unbindhdl;
	funcp = DEVI(dip)->devi_bus_dma_unbindfunc;
	return ((*funcp)(hdip, dip, h));
}

#endif	/* !sparc */

int
ddi_dma_free(ddi_dma_handle_t h)
{
#if !defined(i386)
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_FREE, 0, 0, 0, 0));
#else
	return (((ddi_dma_impl_t *)h)->dmai_mctl(HD, HD, h, DDI_DMA_FREE,
		0, 0, 0, 0));
#endif
}

int
ddi_iopb_alloc(dev_info_t *dip, ddi_dma_lim_t *limp, uint_t len, caddr_t *iopbp)
{
	ddi_dma_lim_t defalt;
	if (!limp) {
		defalt = standard_limits;
		limp = &defalt;
	}
#ifdef sparc
	return (i_ddi_mem_alloc_lim(dip, limp, len, 0, 0, 0,
	    iopbp, NULL, NULL));
#else
	return (ddi_dma_mctl(dip, dip, 0, DDI_DMA_IOPB_ALLOC, (off_t *)limp,
	    (size_t *)len, iopbp, 0));
#endif
}

void
ddi_iopb_free(caddr_t iopb)
{
	i_ddi_mem_free(iopb, 0);
}

int
ddi_mem_alloc(dev_info_t *dip, ddi_dma_lim_t *limits, uint_t length,
	uint_t flags, caddr_t *kaddrp, uint_t *real_length)
{
	ddi_dma_lim_t defalt;
	if (!limits) {
		defalt = standard_limits;
		limits = &defalt;
	}
#ifdef sparc
	return (i_ddi_mem_alloc_lim(dip, limits, length, flags & 0x1, 1, 0,
	    kaddrp, real_length, NULL));
#else
	return (ddi_dma_mctl(dip, dip, (ddi_dma_handle_t)real_length,
	    DDI_DMA_SMEM_ALLOC, (off_t *)limits, (size_t *)length,
	    kaddrp, (flags & 0x1)));
#endif
}

void
ddi_mem_free(caddr_t kaddr)
{
	i_ddi_mem_free(kaddr, 1);
}

/*
 * DMA alignment, burst sizes, and transfer minimums
 */
int
ddi_dma_burstsizes(ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *dimp = (ddi_dma_impl_t *)handle;

	if (!dimp)
		return (0);
	else
		return (dimp->dmai_burstsizes);
}

int
ddi_dma_devalign(ddi_dma_handle_t handle, uint_t *alignment, uint_t *mineffect)
{
	ddi_dma_impl_t *dimp = (ddi_dma_impl_t *)handle;

	if (!dimp || !alignment || !mineffect)
		return (DDI_FAILURE);
	if (!(dimp->dmai_rflags & DDI_DMA_SBUS_64BIT)) {
		*alignment = 1 << ddi_ffs(dimp->dmai_burstsizes);
	} else {
		if (dimp->dmai_burstsizes & 0xff0000) {
			*alignment = 1 << ddi_ffs(dimp->dmai_burstsizes >> 16);
		} else {
			*alignment = 1 << ddi_ffs(dimp->dmai_burstsizes);
		}
	}
	*mineffect = dimp->dmai_minxfer;
	return (DDI_SUCCESS);
}

int
ddi_iomin(dev_info_t *a, int i, int stream)
{
	int r;

	/*
	 * Make sure that the initial value is sane
	 */
	if (i & (i - 1))
		return (0);
	if (i == 0)
		i = (stream) ? 4 : 1;

	r = ddi_ctlops(a, a, DDI_CTLOPS_IOMIN, (void *)stream, (void *)&i);
	if (r != DDI_SUCCESS || (i & (i - 1)))
		return (0);
	return (i);
}

/*
 * Given two DMA attribute structures, apply the attributes
 * of one to the other, following the rules of attributes
 * and the wishes of the caller.
 *
 * The rules of DMA attribute structures are that you cannot
 * make things *less* restrictive as you apply one set
 * of attributes to another.
 *
 */
void
ddi_dma_attr_merge(ddi_dma_attr_t *attr, ddi_dma_attr_t *mod)
{
	attr->dma_attr_addr_lo =
	    MAX(attr->dma_attr_addr_lo, mod->dma_attr_addr_lo);
	attr->dma_attr_addr_hi =
	    MIN(attr->dma_attr_addr_hi, mod->dma_attr_addr_hi);
	attr->dma_attr_count_max =
	    MIN(attr->dma_attr_count_max, mod->dma_attr_count_max);
	attr->dma_attr_align =
	    MAX(attr->dma_attr_align,  mod->dma_attr_align);
	attr->dma_attr_burstsizes =
	    (uint_t)(attr->dma_attr_burstsizes & mod->dma_attr_burstsizes);
	attr->dma_attr_minxfer =
	    maxbit(attr->dma_attr_minxfer, mod->dma_attr_minxfer);
	attr->dma_attr_maxxfer =
	    MIN(attr->dma_attr_maxxfer, mod->dma_attr_maxxfer);
	attr->dma_attr_seg = MIN(attr->dma_attr_seg, mod->dma_attr_seg);
	attr->dma_attr_sgllen = MIN((uint_t)attr->dma_attr_sgllen,
	    (uint_t)mod->dma_attr_sgllen);
	attr->dma_attr_granular =
	    MAX(attr->dma_attr_granular, mod->dma_attr_granular);
}

/*
 * mmap/segmap interface:
 */

/*
 * ddi_segmap:		setup the default segment driver. Calls the drivers
 *			XXmmap routine to validate the range to be mapped.
 *			Return ENXIO of the range is not valid.  Create
 *			a seg_dev segment that contains all of the
 *			necessary information and will reference the
 *			default segment driver routines. It returns zero
 *			on success or non-zero on failure.
 */
int
ddi_segmap(dev_t dev, off_t offset, struct as *asp, caddr_t *addrp, off_t len,
    uint_t prot, uint_t maxprot, uint_t flags, cred_t *credp)
{
	extern int spec_segmap(dev_t, off_t, struct as *, caddr_t *,
	    off_t, uint_t, uint_t, uint_t, struct cred *);

	return (spec_segmap(dev, offset, asp, addrp, len,
	    prot, maxprot, flags, credp));
}

/*
 * ddi_map_fault:	Resolve mappings at fault time.  Used by segment
 *			drivers. Allows each successive parent to resolve
 *			address translations and add its mappings to the
 *			mapping list supplied in the page structure. It
 *			returns zero on success	or non-zero on failure.
 */

int
ddi_map_fault(dev_info_t *dip, struct hat *hat, struct seg *seg,
    caddr_t addr, struct devpage *dp, pfn_t pfn, uint_t prot, uint_t lock)
{
	return (i_ddi_map_fault(dip, dip, hat, seg, addr, dp, pfn, prot, lock));
}

/*
 * ddi_device_mapping_check:	Called from ddi_mapdev_set_access_attr and
 *				ddi_segmap_setup.  Invokes the platform
 *				specific DDI to determine whether attributes
 *				specificed in the attr(9s) are valid for a
 *				region of memory that will be made available
 *				for direct access to a user process via the
 *				mmap(2) system call.
 */
int
ddi_device_mapping_check(dev_t dev, ddi_device_acc_attr_t *accattrp,
    uint_t rnumber, uint_t *hat_flags)
{
	ddi_acc_handle_t handle;
	ddi_map_req_t mr;
	ddi_acc_hdl_t *hp;
	int result;
	dev_info_t *dip;
	major_t maj = getmajor(dev);

	/*
	 * only deals with character (VCHR) devices.
	 */
	if (!(dip = e_ddi_get_dev_info(dev, VCHR)))  {
		/*
		 * e_ddi_get_dev_info() only returns with the driver
		 * held if it successfully translated its dev_t.
		 */
		return (-1);
	}

	ddi_rele_driver(maj);	/* for dev_get_dev_info() */

	/*
	 * Allocate and initialize the common elements of data
	 * access handle.
	 */
	handle = impl_acc_hdl_alloc(KM_SLEEP, NULL);
	if (handle == NULL)
		return (-1);

	hp = impl_acc_hdl_get(handle);
	hp->ah_vers = VERS_ACCHDL;
	hp->ah_dip = dip;
	hp->ah_rnumber = rnumber;
	hp->ah_offset = 0;
	hp->ah_len = 0;
	hp->ah_acc = *accattrp;

	/*
	 * Set up the mapping request and call to parent.
	 */
	mr.map_op = DDI_MO_MAP_HANDLE;
	mr.map_type = DDI_MT_RNUMBER;
	mr.map_obj.rnumber = rnumber;
	mr.map_prot = PROT_READ | PROT_WRITE;
	mr.map_flags = DDI_MF_KERNEL_MAPPING;
	mr.map_handlep = hp;
	mr.map_vers = DDI_MAP_VERSION;
	result = ddi_map(dip, &mr, 0, 0, NULL);

	/*
	 * Region must be mappable, pick up flags from the framework.
	 */
	*hat_flags = hp->ah_hat_flags;

	impl_acc_hdl_free(handle);

	/*
	 * check for end result.
	 */
	if (result != DDI_SUCCESS)
		return (-1);
	return (0);
}


/*
 * Property functions:	 See also, ddipropdefs.h.
 *
 * These functions are the framework for the property functions,
 * i.e. they support software defined properties.  All implementation
 * specific property handling (i.e.: self-identifying devices and
 * PROM defined properties are handled in the implementation specific
 * functions (defined in ddi_implfuncs.h).
 */

/*
 * nopropop:	Shouldn't be called, right?
 */

/* ARGSUSED */
int
nopropop(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	return (DDI_PROP_NOT_FOUND);
}

#ifdef	DDI_PROP_DEBUG
int ddi_prop_debug_flag = 0;

int
ddi_prop_debug(int enable)
{
	int prev = ddi_prop_debug_flag;

	if ((enable != 0) || (prev != 0))
		printf("ddi_prop_debug: debugging %s\n",
		    enable ? "enabled" : "disabled");
	ddi_prop_debug_flag = enable;
	return (prev);
}

#endif	DDI_PROP_DEBUG

/*
 * Search a property list for a match, if found return pointer
 * to matching prop struct, else return NULL.
 */

static ddi_prop_t *
ddi_prop_search(dev_t dev, char *name, uint_t flags, ddi_prop_t **list_head)
{
	ddi_prop_t	*propp;

	/*
	 * find the property in child's devinfo:
	 */

	/*
	 * Search order defined by this search function is
	 * first matching property with input dev ==
	 * DDI_DEV_T_ANY matching any dev or dev == propp->prop_dev,
	 * name == propp->name, and the correct data type as specified
	 * in the flags
	 */

	for (propp = *list_head; propp != NULL; propp = propp->prop_next)  {

		if (strcmp(propp->prop_name, name) != 0)
			continue;

		if ((dev != DDI_DEV_T_ANY) && (propp->prop_dev != dev))
			continue;

		if (((propp->prop_flags & flags) & DDI_PROP_TYPE_MASK) == 0)
			continue;

		return (propp);
	}

	return ((ddi_prop_t *)0);
}


static char *prop_no_mem_msg = "can't allocate memory for ddi property <%s>";

/*
 * ddi_prop_search_common:	Lookup and return the encoded value
 */
int
ddi_prop_search_common(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
    uint_t flags, char *name, void *valuep, uint_t *lengthp)
{
	ddi_prop_t	*propp;
	int		i;
	caddr_t		buffer;
	caddr_t		prealloc = NULL;
	int		plength = 0;
	dev_info_t	*pdip;
	int		(*bop)();
	int		major;
	char		*drv_name;

	/*CONSTANTCONDITION*/
	while (1)  {

		mutex_enter(&(DEVI(dip)->devi_lock));


		/*
		 * find the property in child's devinfo:
		 * Search order is:
		 *	1. driver defined properties
		 *	2. system defined properties
		 *	3. driver global properties
		 *	4. boot defined properties
		 */

		propp = ddi_prop_search(dev, name, flags,
		    &(DEVI(dip)->devi_drv_prop_ptr));
		if (propp == NULL)  {
			propp = ddi_prop_search(dev, name, flags,
			    &(DEVI(dip)->devi_sys_prop_ptr));
		}
		if (propp == NULL)  {
			drv_name = ddi_binding_name(dip);
			if ((drv_name != NULL) &&
			    ((major = ddi_name_to_major(drv_name)) != -1)) {;
				propp = ddi_prop_search(dev, name, flags,
				    &devnamesp[major].dn_global_prop_ptr);
			}
		}

		if (propp == NULL)  {
			propp = ddi_prop_search(dev, name, flags,
			    &(DEVI(dip)->devi_hw_prop_ptr));
		}

		/*
		 * Software property found?
		 */
		if (propp != (ddi_prop_t *)0)	{

			/*
			 * If explicit undefine, return now.
			 */
			if (propp->prop_flags & DDI_PROP_UNDEF_IT) {
				mutex_exit(&(DEVI(dip)->devi_lock));
				if (prealloc)
					kmem_free(prealloc, plength);
				return (DDI_PROP_UNDEFINED);
			}

			/*
			 * If we only want to know if it exists, return now
			 */
			if (prop_op == PROP_EXISTS) {
				mutex_exit(&(DEVI(dip)->devi_lock));
				ASSERT(prealloc == NULL);
				return (DDI_PROP_SUCCESS);
			}

			/*
			 * If length only request or prop length == 0,
			 * service request and return now.
			 */
			if ((prop_op == PROP_LEN) ||(propp->prop_len == 0)) {
				*lengthp = propp->prop_len;
				mutex_exit(&(DEVI(dip)->devi_lock));
				if (prealloc)
					kmem_free(prealloc, plength);
				return (DDI_PROP_SUCCESS);
			}

			/*
			 * If LEN_AND_VAL_ALLOC and the request can sleep,
			 * drop the mutex, allocate the buffer, and go
			 * through the loop again.  If we already allocated
			 * the buffer, and the size of the property changed,
			 * keep trying...
			 */
			if ((prop_op == PROP_LEN_AND_VAL_ALLOC) &&
			    (flags & DDI_PROP_CANSLEEP))  {
				if (prealloc && (propp->prop_len != plength)) {
					kmem_free(prealloc, plength);
					prealloc = NULL;
				}
				if (prealloc == NULL)  {
					plength = propp->prop_len;
					mutex_exit(&(DEVI(dip)->devi_lock));
					prealloc = kmem_alloc(plength,
					    KM_SLEEP);
					continue;
				}
			}

			/*
			 * Allocate buffer, if required.  Either way,
			 * set `buffer' variable.
			 */
			i = *lengthp;			/* Get callers length */
			*lengthp = propp->prop_len;	/* Set callers length */

			switch (prop_op) {

			case PROP_LEN_AND_VAL_ALLOC:

				if (prealloc == NULL) {
					buffer = kmem_alloc(propp->prop_len,
					    KM_NOSLEEP);
				} else {
					buffer = prealloc;
				}

				if (buffer == NULL)  {
					mutex_exit(&(DEVI(dip)->devi_lock));
					cmn_err(CE_CONT, prop_no_mem_msg, name);
					return (DDI_PROP_NO_MEMORY);
				}
				/* Set callers buf ptr */
				*(caddr_t *)valuep = buffer;
				break;

			case PROP_LEN_AND_VAL_BUF:

				if (propp->prop_len > (i)) {
					mutex_exit(&(DEVI(dip)->devi_lock));
					return (DDI_PROP_BUF_TOO_SMALL);
				}

				buffer = valuep;  /* Get callers buf ptr */
				break;
			}

			/*
			 * Do the copy.
			 */
			bcopy(propp->prop_val, buffer, propp->prop_len);
			mutex_exit(&(DEVI(dip)->devi_lock));
			return (DDI_PROP_SUCCESS);
		}

		mutex_exit(&(DEVI(dip)->devi_lock));
		if (prealloc)
			kmem_free(prealloc, plength);
		prealloc = NULL;

		/*
		 * Prop not found, call parent bus_ops to deal with possible
		 * h/w layer (possible PROM defined props, etc.) and to
		 * possibly ascend the hierarchy, if allowed by flags.
		 */
		pdip = (dev_info_t *)DEVI(dip)->devi_parent;

		/*
		 * One last call for the root driver PROM props?
		 */
		if (dip == ddi_root_node())  {
			return (ddi_bus_prop_op(dev, dip, dip, prop_op,
			    flags, name, valuep, (int *)lengthp));
		}

		/*
		 * We may have been called to check for properties
		 * within a single devinfo node that has no parent -
		 * see make_prop()
		 */
		if (pdip == NULL) {
			ASSERT((flags &
			    (DDI_PROP_DONTPASS | DDI_PROP_NOTPROM)) ==
			    (DDI_PROP_DONTPASS | DDI_PROP_NOTPROM));
			return (DDI_PROP_NOT_FOUND);
		}

		/*
		 * Instead of recursing, we do iterative calls up the tree.
		 * As a bit of optimization, skip the bus_op level if the
		 * node is a s/w node and if the parent's bus_prop_op function
		 * is `ddi_bus_prop_op', because we know that in this case,
		 * this function does nothing.
		 *
		 * 4225415: If the parent isn't attached, or the child
		 * hasn't been named by the parent yet, use the default
		 * ddi_bus_prop_op as a proxy for the parent.  This
		 * allows property lookups in any child/parent state to
		 * include 'prom' and inherited properties, even when
		 * there are no drivers attached to the child or parent.
		 */

		bop = ddi_bus_prop_op;
		if (DDI_CF1(dip) && DDI_CF2(pdip))
			bop = DEVI(pdip)->devi_ops->devo_bus_ops->bus_prop_op;

		i = DDI_PROP_NOT_FOUND;

		if ((bop != ddi_bus_prop_op) || ndi_dev_is_prom_node(dip)) {
			i = (*bop)(dev, pdip, dip, prop_op,
			    flags | DDI_PROP_DONTPASS,
			    name, valuep, lengthp);
		}

		if ((flags & DDI_PROP_DONTPASS) ||
		    (i != DDI_PROP_NOT_FOUND))
			return (i);

		dip = pdip;
	}
	/*NOTREACHED*/
}


/*
 * ddi_prop_op: The basic property operator for drivers.
 *
 * In ddi_prop_op, the type of valuep is interpreted based on prop_op:
 *
 *	prop_op			valuep
 *	------			------
 *
 *	PROP_LEN		<unused>
 *
 *	PROP_LEN_AND_VAL_BUF	Pointer to callers buffer
 *
 *	PROP_LEN_AND_VAL_ALLOC	Address of callers pointer (will be set to
 *				address of allocated buffer, if successful)
 */

ddi_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	int	i;

	ASSERT((mod_flags & DDI_PROP_TYPE_MASK) == 0);

	i = ddi_prop_search_common(dev, dip, prop_op,
		mod_flags | DDI_PROP_TYPE_ANY, name, valuep,
		(uint_t *)lengthp);
	if (i == DDI_PROP_FOUND_1275)
		return (DDI_PROP_SUCCESS);
	return (i);
}


/*
 * Variable length props...
 */

/*
 * ddi_getlongprop:	Get variable length property len+val into a buffer
 *		allocated by property provider via kmem_alloc. Requestor
 *		is responsible for freeing returned property via kmem_free.
 *
 *	Arguments:
 *
 *	dev_t:	Input:	dev_t of property.
 *	dip:	Input:	dev_info_t pointer of child.
 *	flags:	Input:	Possible flag modifiers are:
 *		DDI_PROP_DONTPASS:	Don't pass to parent if prop not found.
 *		DDI_PROP_CANSLEEP:	Memory allocation may sleep.
 *	name:	Input:	name of property.
 *	valuep:	Output:	Addr of callers buffer pointer.
 *	lengthp:Output:	*lengthp will contain prop length on exit.
 *
 *	Possible Returns:
 *
 *		DDI_PROP_SUCCESS:	Prop found and returned.
 *		DDI_PROP_NOT_FOUND:	Prop not found
 *		DDI_PROP_UNDEFINED:	Prop explicitly undefined.
 *		DDI_PROP_NO_MEMORY:	Prop found, but unable to alloc mem.
 */

int
ddi_getlongprop(dev_t dev, dev_info_t *dip, int flags,
    char *name, caddr_t valuep, int *lengthp)
{
	return (ddi_prop_op(dev, dip, PROP_LEN_AND_VAL_ALLOC,
	    flags, name, valuep, lengthp));
}

/*
 *
 * ddi_getlongprop_buf:		Get long prop into pre-allocated callers
 *				buffer. (no memory allocation by provider).
 *
 *	dev_t:	Input:	dev_t of property.
 *	dip:	Input:	dev_info_t pointer of child.
 *	flags:	Input:	DDI_PROP_DONTPASS or NULL
 *	name:	Input:	name of property
 *	valuep:	Input:	ptr to callers buffer.
 *	lengthp:I/O:	ptr to length of callers buffer on entry,
 *			actual length of property on exit.
 *
 *	Possible returns:
 *
 *		DDI_PROP_SUCCESS	Prop found and returned
 *		DDI_PROP_NOT_FOUND	Prop not found
 *		DDI_PROP_UNDEFINED	Prop explicitly undefined.
 *		DDI_PROP_BUF_TOO_SMALL	Prop found, callers buf too small,
 *					no value returned, but actual prop
 *					length returned in *lengthp
 *
 */

int
ddi_getlongprop_buf(dev_t dev, dev_info_t *dip, int flags,
    char *name, caddr_t valuep, int *lengthp)
{
	return (ddi_prop_op(dev, dip, PROP_LEN_AND_VAL_BUF,
	    flags, name, valuep, lengthp));
}

/*
 * Integer/boolean sized props.
 *
 * Call is value only... returns found boolean or int sized prop value or
 * defvalue if prop not found or is wrong length or is explicitly undefined.
 * Only flag is DDI_PROP_DONTPASS...
 *
 * By convention, this interface returns boolean (0) sized properties
 * as value (int)1.
 *
 * This never returns an error, if property not found or specifically
 * undefined, the input `defvalue' is returned.
 */

int
ddi_getprop(dev_t dev, dev_info_t *dip, int flags, char *name, int defvalue)
{
	int	propvalue = defvalue;
	int	proplength = sizeof (int);
	int	error;

	error = ddi_prop_op(dev, dip, PROP_LEN_AND_VAL_BUF,
	    flags, name, (caddr_t)&propvalue, &proplength);

	if ((error == DDI_PROP_SUCCESS) && (proplength == 0))
		propvalue = 1;

	return (propvalue);
}

/*
 * Get prop length interface: flags are 0 or DDI_PROP_DONTPASS
 * if returns DDI_PROP_SUCCESS, length returned in *lengthp.
 */

int
ddi_getproplen(dev_t dev, dev_info_t *dip, int flags, char *name, int *lengthp)
{
	return (ddi_prop_op(dev, dip, PROP_LEN, flags, name, NULL, lengthp));
}

/*
 * Allocate a struct prop_driver_data, along with 'size' bytes
 * for decoded property data.  This structure is freed by
 * calling ddi_prop_free(9F).
 */
static void *
ddi_prop_decode_alloc(size_t size, void (*prop_free)(struct prop_driver_data *))
{
	struct prop_driver_data *pdd;

	/*
	 * Allocate a structure with enough memory to store the decoded data.
	 */
	pdd = kmem_zalloc(sizeof (struct prop_driver_data) + size, KM_SLEEP);
	pdd->pdd_size = (sizeof (struct prop_driver_data) + size);
	pdd->pdd_prop_free = prop_free;

	/*
	 * Return a pointer to the location to put the decoded data.
	 */
	return ((void *)((caddr_t)pdd + sizeof (struct prop_driver_data)));
}

/*
 * Allocated the memory needed to store the encoded data in the property
 * handle.
 */
static int
ddi_prop_encode_alloc(prop_handle_t *ph, size_t size)
{
	/*
	 * If size is zero, then set data to NULL and size to 0.  This
	 * is a boolean property.
	 */
	if (size == 0) {
		ph->ph_size = 0;
		ph->ph_data = NULL;
		ph->ph_cur_pos = NULL;
		ph->ph_save_pos = NULL;
	} else {
		if (ph->ph_flags == DDI_PROP_DONTSLEEP) {
			ph->ph_data = kmem_zalloc(size, KM_NOSLEEP);
			if (ph->ph_data == NULL)
				return (DDI_PROP_NO_MEMORY);
		} else
			ph->ph_data = kmem_zalloc(size, KM_SLEEP);
		ph->ph_size = size;
		ph->ph_cur_pos = ph->ph_data;
		ph->ph_save_pos = ph->ph_data;
	}
	return (DDI_PROP_SUCCESS);
}

/*
 * Free the space allocated by the lookup routines.  Each lookup routine
 * returns a pointer to the decoded data to the driver.  The driver then
 * passes this pointer back to us.  This data actually lives in a struct
 * prop_driver_data.  We use negative indexing to find the beginning of
 * the structure and then free the entire structure using the size and
 * the free routine stored in the structure.
 */
void
ddi_prop_free(void *datap)
{
	struct prop_driver_data *pdd;

	/*
	 * Get the structure
	 */
	pdd = (struct prop_driver_data *)
		((caddr_t)datap - sizeof (struct prop_driver_data));
	/*
	 * Call the free routine to free it
	 */
	(*pdd->pdd_prop_free)(pdd);
}

/*
 * Free the data associated with an array of ints,
 * allocated with ddi_prop_decode_alloc().
 */
static void
ddi_prop_free_ints(struct prop_driver_data *pdd)
{
	kmem_free(pdd, pdd->pdd_size);
}

/*
 * Free a single string property or a single string contained within
 * the argv style return value of an array of strings.
 */
static void
ddi_prop_free_string(struct prop_driver_data *pdd)
{
	kmem_free(pdd, pdd->pdd_size);

}

/*
 * Free an array of strings.
 */
static void
ddi_prop_free_strings(struct prop_driver_data *pdd)
{
	kmem_free(pdd, pdd->pdd_size);
}

/*
 * Free the data associated with an array of bytes.
 */
static void
ddi_prop_free_bytes(struct prop_driver_data *pdd)
{
	kmem_free(pdd, pdd->pdd_size);
}

/*
 * Reset the current location pointer in the property handle to the
 * beginning of the data.
 */
void
ddi_prop_reset_pos(prop_handle_t *ph)
{
	ph->ph_cur_pos = ph->ph_data;
	ph->ph_save_pos = ph->ph_data;
}

/*
 * Restore the current location pointer in the property handle to the
 * saved position.
 */
void
ddi_prop_save_pos(prop_handle_t *ph)
{
	ph->ph_save_pos = ph->ph_cur_pos;
}

/*
 * Save the location that the current location poiner is pointing to..
 */
void
ddi_prop_restore_pos(prop_handle_t *ph)
{
	ph->ph_cur_pos = ph->ph_save_pos;
}

/*
 * Property encode/decode functions
 */

/*
 * Decode a single integer property
 */
static int
ddi_prop_fm_decode_int(prop_handle_t *ph, void *data, uint_t *nelements)
{
	int	i;
	int	tmp;

	/*
	 * If there is nothing to decode return an error
	 */
	if (ph->ph_size == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * Decode the property as a single integer and return it
	 * in data if we were able to decode it.
	 */
	i = DDI_PROP_INT(ph, DDI_PROP_CMD_DECODE, &tmp);
	if (i < DDI_PROP_RESULT_OK) {
		switch (i) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	*(int *)data = tmp;
	*nelements = 1;
	return (DDI_PROP_SUCCESS);
}

/*
 * Decode an array of integers property
 */
static int
ddi_prop_fm_decode_ints(prop_handle_t *ph, void *data, uint_t *nelements)
{
	int	i;
	int	cnt = 0;
	int	*tmp;
	int	*intp;
	int	n;

	/*
	 * Figure out how many array elements there are by going through the
	 * data without decoding it first and counting.
	 */
	for (;;) {
		i = DDI_PROP_INT(ph, DDI_PROP_CMD_SKIP, NULL);
		if (i < 0)
			break;
		cnt++;
	}

	/*
	 * If there are no elements return an error
	 */
	if (cnt == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * If we cannot skip through the data, we cannot decode it
	 */
	if (i == DDI_PROP_RESULT_ERROR)
		return (DDI_PROP_CANNOT_DECODE);

	/*
	 * Reset the data pointer to the beginning of the encoded data
	 */
	ddi_prop_reset_pos(ph);

	/*
	 * Allocated memory to store the decoded value in.
	 */
	intp = ddi_prop_decode_alloc((cnt * sizeof (int)),
		ddi_prop_free_ints);

	/*
	 * Decode each elemente and place it in the space we just allocated
	 */
	tmp = intp;
	for (n = 0; n < cnt; n++, tmp++) {
		i = DDI_PROP_INT(ph, DDI_PROP_CMD_DECODE, tmp);
		if (i < DDI_PROP_RESULT_OK) {
			/*
			 * Free the space we just allocated
			 * and return an error.
			 */
			ddi_prop_free(intp);
			switch (i) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_DECODE);
			}
		}
	}

	*nelements = cnt;
	*(int **)data = intp;

	return (DDI_PROP_SUCCESS);
}

/*
 * Encode an array of integers property (Can be one element)
 */
int
ddi_prop_fm_encode_ints(prop_handle_t *ph, void *data, uint_t nelements)
{
	int	i;
	int	*tmp;
	int	cnt;
	int	size;

	/*
	 * If there is no data, we cannot do anything
	 */
	if (nelements == 0)
		return (DDI_PROP_CANNOT_ENCODE);

	/*
	 * Get the size of an encoded int.
	 */
	size = DDI_PROP_INT(ph, DDI_PROP_CMD_GET_ESIZE, NULL);
	if (size < DDI_PROP_RESULT_OK) {
		switch (size) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_ENCODE);
		}
	}

	/*
	 * Allocate space in the handle to store the encoded int.
	 */
	if (ddi_prop_encode_alloc(ph, size * nelements) !=
		DDI_PROP_SUCCESS)
		return (DDI_PROP_NO_MEMORY);

	/*
	 * Encode the array of ints.
	 */
	tmp = (int *)data;
	for (cnt = 0; cnt < nelements; cnt++, tmp++) {
		i = DDI_PROP_INT(ph, DDI_PROP_CMD_ENCODE, tmp);
		if (i < DDI_PROP_RESULT_OK) {
			switch (i) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_ENCODE);
			}
		}
	}

	return (DDI_PROP_SUCCESS);
}



/*
 * Decode a single string property
 */
static int
ddi_prop_fm_decode_string(prop_handle_t *ph, void *data, uint_t *nelements)
{
	char		*tmp;
	char		*str;
	int		i;
	int		size;

	/*
	 * If there is nothing to decode return an error
	 */
	if (ph->ph_size == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * Get the decoded size of the encoded string.
	 */
	size = DDI_PROP_STR(ph, DDI_PROP_CMD_GET_DSIZE, NULL);
	if (size < DDI_PROP_RESULT_OK) {
		switch (size) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	/*
	 * Allocated memory to store the decoded value in.
	 */
	str = ddi_prop_decode_alloc((size_t)size, ddi_prop_free_string);

	ddi_prop_reset_pos(ph);

	/*
	 * Decode the str and place it in the space we just allocated
	 */
	tmp = str;
	i = DDI_PROP_STR(ph, DDI_PROP_CMD_DECODE, tmp);
	if (i < DDI_PROP_RESULT_OK) {
		/*
		 * Free the space we just allocated
		 * and return an error.
		 */
		ddi_prop_free(str);
		switch (i) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	*(char **)data = str;
	*nelements = 1;

	return (DDI_PROP_SUCCESS);
}

/*
 * Decode an array of strings.
 */
int
ddi_prop_fm_decode_strings(prop_handle_t *ph, void *data, uint_t *nelements)
{
	int		cnt = 0;
	char		**strs;
	char		**tmp;
	char		*ptr;
	int		i;
	int		n;
	int		size;
	size_t		nbytes;

	/*
	 * Figure out how many array elements there are by going through the
	 * data without decoding it first and counting.
	 */
	for (;;) {
		i = DDI_PROP_STR(ph, DDI_PROP_CMD_SKIP, NULL);
		if (i < 0)
			break;
		cnt++;
	}

	/*
	 * If there are no elements return an error
	 */
	if (cnt == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * If we cannot skip through the data, we cannot decode it
	 */
	if (i == DDI_PROP_RESULT_ERROR)
		return (DDI_PROP_CANNOT_DECODE);

	/*
	 * Reset the data pointer to the beginning of the encoded data
	 */
	ddi_prop_reset_pos(ph);

	/*
	 * Figure out how much memory we need for the sum total
	 */
	nbytes = (cnt + 1) * sizeof (char *);

	for (n = 0; n < cnt; n++) {
		/*
		 * Get the decoded size of the current encoded string.
		 */
		size = DDI_PROP_STR(ph, DDI_PROP_CMD_GET_DSIZE, NULL);
		if (size < DDI_PROP_RESULT_OK) {
			switch (size) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_DECODE);
			}
		}

		nbytes += size;
	}

	/*
	 * Allocate memory in which to store the decoded strings.
	 */
	strs = ddi_prop_decode_alloc(nbytes, ddi_prop_free_strings);

	/*
	 * Set up pointers for each string by figuring out yet
	 * again how long each string is.
	 */
	ddi_prop_reset_pos(ph);
	ptr = (caddr_t)strs + ((cnt + 1) * sizeof (char *));
	for (tmp = strs, n = 0; n < cnt; n++, tmp++) {
		/*
		 * Get the decoded size of the current encoded string.
		 */
		size = DDI_PROP_STR(ph, DDI_PROP_CMD_GET_DSIZE, NULL);
		if (size < DDI_PROP_RESULT_OK) {
			ddi_prop_free(strs);
			switch (size) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_DECODE);
			}
		}

		*tmp = ptr;
		ptr += size;
	}

	/*
	 * String array is terminated by a NULL
	 */
	*tmp = NULL;

	/*
	 * Finally, we can decode each string
	 */
	ddi_prop_reset_pos(ph);
	for (tmp = strs, n = 0; n < cnt; n++, tmp++) {
		i = DDI_PROP_STR(ph, DDI_PROP_CMD_DECODE, *tmp);
		if (i < DDI_PROP_RESULT_OK) {
			/*
			 * Free the space we just allocated
			 * and return an error
			 */
			ddi_prop_free(strs);
			switch (i) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_DECODE);
			}
		}
	}

	*(char ***)data = strs;
	*nelements = cnt;

	return (DDI_PROP_SUCCESS);
}

/*
 * Encode a string.
 */
int
ddi_prop_fm_encode_string(prop_handle_t *ph, void *data, uint_t nelements)
{
	char		**tmp;
	int		size;
	int		i;

	/*
	 * If there is no data, we cannot do anything
	 */
	if (nelements == 0)
		return (DDI_PROP_CANNOT_ENCODE);

	/*
	 * Get the size of the encoded string.
	 */
	tmp = (char **)data;
	size = DDI_PROP_STR(ph, DDI_PROP_CMD_GET_ESIZE, *tmp);
	if (size < DDI_PROP_RESULT_OK) {
		switch (size) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_ENCODE);
		}
	}

	/*
	 * Allocate space in the handle to store the encoded string.
	 */
	if (ddi_prop_encode_alloc(ph, size) != DDI_PROP_SUCCESS)
		return (DDI_PROP_NO_MEMORY);

	ddi_prop_reset_pos(ph);

	/*
	 * Encode the string.
	 */
	tmp = (char **)data;
	i = DDI_PROP_STR(ph, DDI_PROP_CMD_ENCODE, *tmp);
	if (i < DDI_PROP_RESULT_OK) {
		switch (i) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_ENCODE);
		}
	}

	return (DDI_PROP_SUCCESS);
}


/*
 * Encode an array of strings.
 */
int
ddi_prop_fm_encode_strings(prop_handle_t *ph, void *data, uint_t nelements)
{
	int		cnt = 0;
	char		**tmp;
	int		size;
	uint_t		total_size;
	int		i;

	/*
	 * If there is no data, we cannot do anything
	 */
	if (nelements == 0)
		return (DDI_PROP_CANNOT_ENCODE);

	/*
	 * Get the total size required to encode all the strings.
	 */
	total_size = 0;
	tmp = (char **)data;
	for (cnt = 0; cnt < nelements; cnt++, tmp++) {
		size = DDI_PROP_STR(ph, DDI_PROP_CMD_GET_ESIZE, *tmp);
		if (size < DDI_PROP_RESULT_OK) {
			switch (size) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_ENCODE);
			}
		}
		total_size += (uint_t)size;
	}

	/*
	 * Allocate space in the handle to store the encoded strings.
	 */
	if (ddi_prop_encode_alloc(ph, total_size) != DDI_PROP_SUCCESS)
		return (DDI_PROP_NO_MEMORY);

	ddi_prop_reset_pos(ph);

	/*
	 * Encode the array of strings.
	 */
	tmp = (char **)data;
	for (cnt = 0; cnt < nelements; cnt++, tmp++) {
		i = DDI_PROP_STR(ph, DDI_PROP_CMD_ENCODE, *tmp);
		if (i < DDI_PROP_RESULT_OK) {
			switch (i) {
			case DDI_PROP_RESULT_EOF:
				return (DDI_PROP_END_OF_DATA);

			case DDI_PROP_RESULT_ERROR:
				return (DDI_PROP_CANNOT_ENCODE);
			}
		}
	}

	return (DDI_PROP_SUCCESS);
}


/*
 * Decode an array of bytes.
 */
static int
ddi_prop_fm_decode_bytes(prop_handle_t *ph, void *data, uint_t *nelements)
{
	uchar_t		*tmp;
	int		nbytes;
	int		i;

	/*
	 * If there are no elements return an error
	 */
	if (ph->ph_size == 0)
		return (DDI_PROP_END_OF_DATA);

	/*
	 * Get the size of the encoded array of bytes.
	 */
	nbytes = DDI_PROP_BYTES(ph, DDI_PROP_CMD_GET_DSIZE,
		data, ph->ph_size);
	if (nbytes < DDI_PROP_RESULT_OK) {
		switch (nbytes) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	/*
	 * Allocated memory to store the decoded value in.
	 */
	tmp = ddi_prop_decode_alloc(nbytes, ddi_prop_free_bytes);

	/*
	 * Decode each element and place it in the space we just allocated
	 */
	i = DDI_PROP_BYTES(ph, DDI_PROP_CMD_DECODE, tmp, nbytes);
	if (i < DDI_PROP_RESULT_OK) {
		/*
		 * Free the space we just allocated
		 * and return an error
		 */
		ddi_prop_free(tmp);
		switch (i) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	*(uchar_t **)data = tmp;
	*nelements = nbytes;

	return (DDI_PROP_SUCCESS);
}

/*
 * Encode an array of bytes.
 */
int
ddi_prop_fm_encode_bytes(prop_handle_t *ph, void *data, uint_t nelements)
{
	int		size;
	int		i;

	/*
	 * If there are no elements, then this is a boolean property,
	 * so just create a property handle with no data and return.
	 */
	if (nelements == 0) {
		(void) ddi_prop_encode_alloc(ph, 0);
		return (DDI_PROP_SUCCESS);
	}

	/*
	 * Get the size of the encoded array of bytes.
	 */
	size = DDI_PROP_BYTES(ph, DDI_PROP_CMD_GET_ESIZE, (uchar_t *)data,
		nelements);
	if (size < DDI_PROP_RESULT_OK) {
		switch (size) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_DECODE);
		}
	}

	/*
	 * Allocate space in the handle to store the encoded bytes.
	 */
	if (ddi_prop_encode_alloc(ph, (uint_t)size) != DDI_PROP_SUCCESS)
		return (DDI_PROP_NO_MEMORY);

	/*
	 * Encode the array of bytes.
	 */
	i = DDI_PROP_BYTES(ph, DDI_PROP_CMD_ENCODE, (uchar_t *)data,
		nelements);
	if (i < DDI_PROP_RESULT_OK) {
		switch (i) {
		case DDI_PROP_RESULT_EOF:
			return (DDI_PROP_END_OF_DATA);

		case DDI_PROP_RESULT_ERROR:
			return (DDI_PROP_CANNOT_ENCODE);
		}
	}

	return (DDI_PROP_SUCCESS);
}

/*
 * OBP 1275 integer, string and byte operators.
 *
 * DDI_PROP_CMD_DECODE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot decode the data
 *	DDI_PROP_RESULT_EOF:		end of data
 *	DDI_PROP_OK:			data was decoded
 *
 * DDI_PROP_CMD_ENCODE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot encode the data
 *	DDI_PROP_RESULT_EOF:		end of data
 *	DDI_PROP_OK:			data was encoded
 *
 * DDI_PROP_CMD_SKIP:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot skip the data
 *	DDI_PROP_RESULT_EOF:		end of data
 *	DDI_PROP_OK:			data was skipped
 *
 * DDI_PROP_CMD_GET_ESIZE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot get encoded size
 *	DDI_PROP_RESULT_EOF:		end of data
 *	> 0:				the encoded size
 *
 * DDI_PROP_CMD_GET_DSIZE:
 *
 *	DDI_PROP_RESULT_ERROR:		cannot get decoded size
 *	DDI_PROP_RESULT_EOF:		end of data
 *	> 0:				the decoded size
 */

/*
 * OBP 1275 integer operator
 *
 * OBP properties are a byte stream of data, so integers may not be
 * properly aligned.  Therefore we need to copy them one byte at a time.
 */
int
ddi_prop_1275_int(prop_handle_t *ph, uint_t cmd, int *data)
{
	int	i;

	switch (cmd) {
	case DDI_PROP_CMD_DECODE:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0)
			return (DDI_PROP_RESULT_ERROR);
		if (ph->ph_flags & PH_FROM_PROM) {
			i = MIN(ph->ph_size, PROP_1275_INT_SIZE);
			if ((int *)ph->ph_cur_pos > ((int *)ph->ph_data +
				ph->ph_size - i))
				return (DDI_PROP_RESULT_ERROR);
		} else {
			if (ph->ph_size < sizeof (int) ||
			((int *)ph->ph_cur_pos > ((int *)ph->ph_data +
				ph->ph_size - sizeof (int))))
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Copy the integer, using the implementation-specific
		 * copy function if the property is coming from the PROM.
		 */
		if (ph->ph_flags & PH_FROM_PROM) {
			*data = impl_ddi_prop_int_from_prom(
				(uchar_t *)ph->ph_cur_pos,
				(ph->ph_size < PROP_1275_INT_SIZE) ?
				ph->ph_size : PROP_1275_INT_SIZE);
		} else {
			bcopy(ph->ph_cur_pos, data, sizeof (int));
		}

		/*
		 * Move the current location to the start of the next
		 * bit of undecoded data.
		 */
		ph->ph_cur_pos = (uchar_t *)ph->ph_cur_pos +
			PROP_1275_INT_SIZE;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_ENCODE:
		/*
		 * Check that there is room to encoded the data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
			ph->ph_size < PROP_1275_INT_SIZE ||
			((int *)ph->ph_cur_pos > ((int *)ph->ph_data +
				ph->ph_size - sizeof (int))))
			return (DDI_PROP_RESULT_ERROR);

		/*
		 * Encode the integer into the byte stream one byte at a
		 * time.
		 */
		bcopy(data, ph->ph_cur_pos, sizeof (int));

		/*
		 * Move the current location to the start of the next bit of
		 * space where we can store encoded data.
		 */
		ph->ph_cur_pos = (uchar_t *)ph->ph_cur_pos + PROP_1275_INT_SIZE;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_SKIP:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
				ph->ph_size < PROP_1275_INT_SIZE)
			return (DDI_PROP_RESULT_ERROR);


		if ((caddr_t)ph->ph_cur_pos ==
				(caddr_t)ph->ph_data + ph->ph_size) {
			return (DDI_PROP_RESULT_EOF);
		} else if ((caddr_t)ph->ph_cur_pos >
				(caddr_t)ph->ph_data + ph->ph_size) {
			return (DDI_PROP_RESULT_EOF);
		}

		/*
		 * Move the current location to the start of the next bit of
		 * undecoded data.
		 */
		ph->ph_cur_pos = (uchar_t *)ph->ph_cur_pos + PROP_1275_INT_SIZE;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_GET_ESIZE:
		/*
		 * Return the size of an encoded integer on OBP
		 */
		return (PROP_1275_INT_SIZE);

	case DDI_PROP_CMD_GET_DSIZE:
		/*
		 * Return the size of a decoded integer on the system.
		 */
		return (sizeof (int));

#ifdef	DEBUG
	default:
		cmn_err(CE_PANIC, "File %s, line %d: 0x%x impossible",
			__FILE__, __LINE__, cmd);
#endif	/* DEBUG */
	}

	/*NOTREACHED*/
}

/*
 * OBP 1275 string operator.
 *
 * OBP strings are NULL terminated.
 */
int
ddi_prop_1275_string(prop_handle_t *ph, uint_t cmd, char *data)
{
	int	n;
	char	*p;
	char	*end;

	switch (cmd) {
	case DDI_PROP_CMD_DECODE:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0) {
			return (DDI_PROP_RESULT_ERROR);
		}

		n = strlen((char *)ph->ph_cur_pos) + 1;
		if ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
				ph->ph_size - n)) {
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Copy the NULL terminated string
		 */
		bcopy(ph->ph_cur_pos, data, n);

		/*
		 * Move the current location to the start of the next bit of
		 * undecoded data.
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + n;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_ENCODE:
		/*
		 * Check that there is room to encoded the data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0) {
			return (DDI_PROP_RESULT_ERROR);
		}

		n = strlen(data) + 1;
		if ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
				ph->ph_size - n)) {
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Copy the NULL terminated string
		 */
		bcopy(data, ph->ph_cur_pos, n);

		/*
		 * Move the current location to the start of the next bit of
		 * space where we can store encoded data.
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + n;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_SKIP:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0) {
			return (DDI_PROP_RESULT_ERROR);
		}

		/*
		 * Return the string length plus one for the NULL
		 * We know the size of the property, we need to
		 * ensure that the string is properly formatted,
		 * since we may be looking up random OBP data.
		 */
		p = (char *)ph->ph_cur_pos;
		end = (char *)ph->ph_data + ph->ph_size;

		if (p == end) {
			return (DDI_PROP_RESULT_EOF);
		}

		for (n = 0; p < end; n++) {
			if (*p++ == 0) {
				ph->ph_cur_pos = p;
				return (DDI_PROP_RESULT_OK);
			}
		}

		return (DDI_PROP_RESULT_ERROR);

	case DDI_PROP_CMD_GET_ESIZE:
		/*
		 * Return the size of the encoded string on OBP.
		 */
		return (strlen(data) + 1);

	case DDI_PROP_CMD_GET_DSIZE:
		/*
		 * Return the string length plus one for the NULL
		 * We know the size of the property, we need to
		 * ensure that the string is properly formatted,
		 * since we may be looking up random OBP data.
		 */
		p = (char *)ph->ph_cur_pos;
		end = (char *)ph->ph_data + ph->ph_size;
		for (n = 0; p < end; n++) {
			if (*p++ == 0) {
				ph->ph_cur_pos = p;
				return (n+1);
			}
		}
		return (DDI_PROP_RESULT_ERROR);

#ifdef	DEBUG
	default:
		cmn_err(CE_PANIC, "File %s, line %d: 0x%x impossible",
			__FILE__, __LINE__, cmd);
#endif	/* DEBUG */
	}

	/*NOTREACHED*/
}

/*
 * OBP 1275 byte operator
 *
 * Caller must specify the number of bytes to get.  OBP encodes bytes
 * as a byte so there is a 1-to-1 translation.
 */
int
ddi_prop_1275_bytes(prop_handle_t *ph, uint_t cmd, uchar_t *data,
	uint_t nelements)
{
	switch (cmd) {
	case DDI_PROP_CMD_DECODE:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
			ph->ph_size < nelements ||
			((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
				ph->ph_size - nelements)))
			return (DDI_PROP_RESULT_ERROR);

		/*
		 * Copy out the bytes
		 */
		bcopy(ph->ph_cur_pos, data, nelements);

		/*
		 * Move the current location
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + nelements;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_ENCODE:
		/*
		 * Check that there is room to encode the data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
			ph->ph_size < nelements ||
			((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
				ph->ph_size - nelements)))
			return (DDI_PROP_RESULT_ERROR);

		/*
		 * Copy in the bytes
		 */
		bcopy(data, ph->ph_cur_pos, nelements);

		/*
		 * Move the current location to the start of the next bit of
		 * space where we can store encoded data.
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + nelements;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_SKIP:
		/*
		 * Check that there is encoded data
		 */
		if (ph->ph_cur_pos == NULL || ph->ph_size == 0 ||
				ph->ph_size < nelements)
			return (DDI_PROP_RESULT_ERROR);

		if ((char *)ph->ph_cur_pos > ((char *)ph->ph_data +
				ph->ph_size - nelements))
			return (DDI_PROP_RESULT_EOF);

		/*
		 * Move the current location
		 */
		ph->ph_cur_pos = (char *)ph->ph_cur_pos + nelements;
		return (DDI_PROP_RESULT_OK);

	case DDI_PROP_CMD_GET_ESIZE:
		/*
		 * The size in bytes of the encoded size is the
		 * same as the decoded size provided by the caller.
		 */
		return (nelements);

	case DDI_PROP_CMD_GET_DSIZE:
		/*
		 * Just return the number of bytes specified by the caller.
		 */
		return (nelements);

#ifdef	DEBUG
	default:
		cmn_err(CE_PANIC, "File %s, line %d: 0x%x impossible",
			__FILE__, __LINE__, cmd);
#endif	/* DEBUG */
	}

	/*NOTREACHED*/
}

/*
 * Used for properties that come from the OBP, hardware configuration files,
 * or that are created by calls to ddi_prop_update(9F).
 */
static struct prop_handle_ops prop_1275_ops = {
	ddi_prop_1275_int,
	ddi_prop_1275_string,
	ddi_prop_1275_bytes
};


/*
 * Interface to create/modify a managed property on child's behalf...
 * Flags interpreted are:
 *	DDI_PROP_CANSLEEP:	Allow memory allocation to sleep.
 *	DDI_PROP_SYSTEM_DEF:	Manipulate system list rather than driver list.
 *
 * Use same dev_t when modifying or undefining a property.
 * Search for properties with DDI_DEV_T_ANY to match first named
 * property on the list.
 *
 * Properties are stored LIFO and subsequently will match the first
 * `matching' instance.
 */

/*
 * ddi_prop_add:	Add a software defined property
 */

/*
 * define to get a new ddi_prop_t.
 * km_flags are KM_SLEEP or KM_NOSLEEP.
 */

#define	DDI_NEW_PROP_T(km_flags)	\
	(kmem_zalloc(sizeof (ddi_prop_t), km_flags))

static int
ddi_prop_add(dev_t dev, dev_info_t *dip, int flags,
    char *name, caddr_t value, int length)
{
	ddi_prop_t	*new_propp, *propp;
	ddi_prop_t	**list_head = &(DEVI(dip)->devi_drv_prop_ptr);
	int		km_flags = KM_NOSLEEP;
	int		name_buf_len;

	/*
	 * If dev_t is DDI_DEV_T_ANY or name's length is zero return error.
	 */

	if (dev == DDI_DEV_T_ANY || name == (char *)0 || strlen(name) == 0)
		return (DDI_PROP_INVAL_ARG);

	if (flags & DDI_PROP_CANSLEEP)
		km_flags = KM_SLEEP;

	if (flags & DDI_PROP_SYSTEM_DEF)
		list_head = &(DEVI(dip)->devi_sys_prop_ptr);
	else if (flags & DDI_PROP_HW_DEF)
		list_head = &(DEVI(dip)->devi_hw_prop_ptr);

	if ((new_propp = DDI_NEW_PROP_T(km_flags)) == NULL)  {
		cmn_err(CE_CONT, prop_no_mem_msg, name);
		return (DDI_PROP_NO_MEMORY);
	}

	/*
	 * If dev is major number 0, then we need to do a ddi_name_to_major
	 * to get the real major number for the device.  This needs to be
	 * done because some drivers need to call ddi_prop_create in their
	 * attach routines but they don't have a dev.  By creating the dev
	 * ourself if the major number is 0, drivers will not have to know what
	 * their major number.	They can just create a dev with major number
	 * 0 and pass it in.  For device 0, we will be doing a little extra
	 * work by recreating the same dev that we already have, but its the
	 * price you pay :-).
	 *
	 * This fixes bug #1098060.
	 */
	if (getmajor(dev) == DDI_MAJOR_T_UNKNOWN) {
		new_propp->prop_dev =
		    makedevice(ddi_name_to_major(DEVI(dip)->devi_binding_name),
		    getminor(dev));
	} else
		new_propp->prop_dev = dev;

	/*
	 * Allocate space for property name and copy it in...
	 */

	name_buf_len = strlen(name) + 1;
	new_propp->prop_name = kmem_alloc(name_buf_len, km_flags);
	if (new_propp->prop_name == 0)	{
		kmem_free(new_propp, sizeof (ddi_prop_t));
		cmn_err(CE_CONT, prop_no_mem_msg, name);
		return (DDI_PROP_NO_MEMORY);
	}
	bcopy(name, new_propp->prop_name, name_buf_len);

	/*
	 * Set the property type
	 */
	new_propp->prop_flags = flags & DDI_PROP_TYPE_MASK;

	/*
	 * Set length and value ONLY if not an explicit property undefine:
	 * NOTE: value and length are zero for explicit undefines.
	 */

	if (flags & DDI_PROP_UNDEF_IT) {
		new_propp->prop_flags |= DDI_PROP_UNDEF_IT;
	} else {
		if ((new_propp->prop_len = length) != 0) {
			new_propp->prop_val = kmem_alloc(length, km_flags);
			if (new_propp->prop_val == 0)  {
				kmem_free(new_propp->prop_name, name_buf_len);
				kmem_free(new_propp, sizeof (ddi_prop_t));
				cmn_err(CE_CONT, prop_no_mem_msg, name);
				return (DDI_PROP_NO_MEMORY);
			}
			bcopy(value, new_propp->prop_val, length);
		}
	}

	/*
	 * Link property into beginning of list. (Properties are LIFO order.)
	 */

	mutex_enter(&(DEVI(dip)->devi_lock));
	propp = *list_head;
	new_propp->prop_next = propp;
	*list_head = new_propp;
	mutex_exit(&(DEVI(dip)->devi_lock));
	return (DDI_PROP_SUCCESS);
}


/*
 * ddi_prop_modify_common:	Modify a software managed property value
 *
 *			Set new length and value if found.
 *			returns DDI_PROP_NOT_FOUND, if s/w defined prop
 *			not found or no exact match of input dev_t.
 *			DDI_PROP_INVAL_ARG if dev is DDI_DEV_T_ANY or
 *			input name is the NULL string.
 *			DDI_PROP_NO_MEMORY if unable to allocate memory
 *
 *			Note: an undef can be modified to be a define,
 *			(you can't go the other way.)
 */

static int
ddi_prop_change(dev_t dev, dev_info_t *dip, int flags,
    char *name, caddr_t value, int length)
{
	ddi_prop_t	*propp;
	int		km_flags = KM_NOSLEEP;
	caddr_t		p = NULL;

	if (dev == DDI_DEV_T_ANY || name == (char *)0 || strlen(name) == 0)
		return (DDI_PROP_INVAL_ARG);

	if (flags & DDI_PROP_CANSLEEP)
		km_flags = KM_SLEEP;

	/*
	 * Preallocate buffer, even if we don't need it...
	 */
	if (length != 0)  {
		p = kmem_alloc(length, km_flags);
		if (p == NULL)	{
			cmn_err(CE_CONT, prop_no_mem_msg, name);
			return (DDI_PROP_NO_MEMORY);
		}
	}

	mutex_enter(&(DEVI(dip)->devi_lock));

	propp = DEVI(dip)->devi_drv_prop_ptr;
	if (flags & DDI_PROP_SYSTEM_DEF)
		propp = DEVI(dip)->devi_sys_prop_ptr;
	else if (flags & DDI_PROP_HW_DEF)
		propp = DEVI(dip)->devi_hw_prop_ptr;

	while (propp != NULL) {
		if (strcmp(name, propp->prop_name) == 0 &&
		    dev == propp->prop_dev) {

			/*
			 * Need to reallocate buffer?  If so, do it
			 * (carefully). (Reuse same space if new prop
			 * is same size and non-NULL sized).
			 */

			if (length != 0)
				bcopy(value, p, length);

			if (propp->prop_len != 0)
				kmem_free(propp->prop_val, propp->prop_len);

			propp->prop_len = length;
			propp->prop_val = p;
			propp->prop_flags &= ~DDI_PROP_UNDEF_IT;
			mutex_exit(&(DEVI(dip)->devi_lock));
			return (DDI_PROP_SUCCESS);
		}
		propp = propp->prop_next;
	}

	mutex_exit(&(DEVI(dip)->devi_lock));
	if (length != 0)
		kmem_free(p, length);
	return (DDI_PROP_NOT_FOUND);
}



/*
 * Common update routine used to update and encode a property.	Creates
 * a property handle, calls the property encode routine, figures out if
 * the property already exists and updates if it does.	Otherwise it
 * creates if it does not exist.
 */
int
ddi_prop_update_common(dev_t match_dev, dev_info_t *dip, int flags,
    char *name, void *data, uint_t nelements,
    int (*prop_create)(prop_handle_t *, void *data, uint_t nelements))
{
	prop_handle_t	ph;
	int		rval;
	uint_t		ourflags;
	dev_t		search_dev;

	/*
	 * If dev_t is DDI_DEV_T_ANY or name's length is zero,
	 * return error.
	 */
	if (match_dev == DDI_DEV_T_ANY || name == NULL || strlen(name) == 0)
		return (DDI_PROP_INVAL_ARG);

	/*
	 * Create the handle
	 */
	ph.ph_data = NULL;
	ph.ph_cur_pos = NULL;
	ph.ph_save_pos = NULL;
	ph.ph_size = 0;
	ph.ph_ops = &prop_1275_ops;

	/*
	 * ourflags:
	 * For compatibility with the old interfaces.  The old interfaces
	 * didn't sleep by default and slept when the flag was set.  These
	 * interfaces to the opposite.	So the old interfaces now set the
	 * DDI_PROP_DONTSLEEP flag by default which tells us not to sleep.
	 *
	 * ph.ph_flags:
	 * Blocked data or unblocked data allocation
	 * for ph.ph_data in ddi_prop_encode_alloc()
	 */
	if (flags & DDI_PROP_DONTSLEEP) {
		ourflags = flags;
		ph.ph_flags = DDI_PROP_DONTSLEEP;
	} else {
		ourflags = flags | DDI_PROP_CANSLEEP;
		ph.ph_flags = DDI_PROP_CANSLEEP;
	}

	/*
	 * Encode the data and store it in the property handle by
	 * calling the prop_encode routine.
	 */
	if ((rval = (*prop_create)(&ph, data, nelements)) !=
	    DDI_PROP_SUCCESS) {
		if (rval == DDI_PROP_NO_MEMORY)
			cmn_err(CE_CONT, prop_no_mem_msg, name);
		if (ph.ph_size != 0)
			kmem_free(ph.ph_data, ph.ph_size);
		return (rval);
	}


	/*
	 * If we are doing a wildcard update, we need to use the wildcard
	 * search dev DDI_DEV_T_ANY when checking to see if the property
	 * exists.
	 */
	search_dev = (match_dev == DDI_DEV_T_NONE) ?
		DDI_DEV_T_ANY : match_dev;

	/*
	 * The old interfaces use a stacking approach to creating
	 * properties.	If we are being called from the old interfaces,
	 * the DDI_PROP_STACK_CREATE flag will be set, so we just do a
	 * create without checking.
	 *
	 * Otherwise we check to see if the property exists.  If so we
	 * modify it else we create it.  We only check the driver
	 * property list.  So if a property exists on another list or
	 * the PROM, we will create one of our own.
	 */
	if ((flags & DDI_PROP_STACK_CREATE) ||
	    !ddi_prop_exists(search_dev, dip,
	    (ourflags | DDI_PROP_DONTPASS | DDI_PROP_NOTPROM), name)) {
		rval = ddi_prop_add(match_dev, dip,
		    ourflags, name, ph.ph_data, ph.ph_size);
	} else {
		rval = ddi_prop_change(match_dev, dip,
		    ourflags, name, ph.ph_data, ph.ph_size);
	}

	/*
	 * Free the encoded data allocated in the prop_encode routine.
	 */
	if (ph.ph_size != 0)
		kmem_free(ph.ph_data, ph.ph_size);

	return (rval);
}


/*
 * ddi_prop_create:	Define a managed property:
 *			See above for details.
 */

int
ddi_prop_create(dev_t dev, dev_info_t *dip, int flag,
    char *name, caddr_t value, int length)
{
	if (!(flag & DDI_PROP_CANSLEEP))
		flag |= DDI_PROP_DONTSLEEP;
	flag &= ~DDI_PROP_SYSTEM_DEF;
	return (ddi_prop_update_common(dev, dip,
	    (flag | DDI_PROP_STACK_CREATE | DDI_PROP_TYPE_ANY), name,
	    value, length, ddi_prop_fm_encode_bytes));
}

int
e_ddi_prop_create(dev_t dev, dev_info_t *dip, int flag,
    char *name, caddr_t value, int length)
{
	if (!(flag & DDI_PROP_CANSLEEP))
		flag |= DDI_PROP_DONTSLEEP;
	return (ddi_prop_update_common(dev, dip,
	    (flag | DDI_PROP_SYSTEM_DEF | DDI_PROP_STACK_CREATE |
	    DDI_PROP_TYPE_ANY),
	    name, value, length, ddi_prop_fm_encode_bytes));
}

ddi_prop_modify(dev_t dev, dev_info_t *dip, int flag,
    char *name, caddr_t value, int length)
{
	ASSERT((flag & DDI_PROP_TYPE_MASK) == 0);

	/*
	 * If dev_t is DDI_DEV_T_ANY or name's length is zero,
	 * return error.
	 */
	if (dev == DDI_DEV_T_ANY || name == NULL || strlen(name) == 0)
		return (DDI_PROP_INVAL_ARG);

	if (!(flag & DDI_PROP_CANSLEEP))
		flag |= DDI_PROP_DONTSLEEP;
	flag &= ~DDI_PROP_SYSTEM_DEF;
	if (ddi_prop_exists((dev == DDI_DEV_T_NONE) ? DDI_DEV_T_ANY : dev,
	    dip, (flag | DDI_PROP_NOTPROM), name) == 0)
		return (DDI_PROP_NOT_FOUND);

	return (ddi_prop_update_common(dev, dip,
	    (flag | DDI_PROP_TYPE_BYTE), name,
	    value, length, ddi_prop_fm_encode_bytes));
}

int
e_ddi_prop_modify(dev_t dev, dev_info_t *dip, int flag,
    char *name, caddr_t value, int length)
{
	ASSERT((flag & DDI_PROP_TYPE_MASK) == 0);

	/*
	 * If dev_t is DDI_DEV_T_ANY or name's length is zero,
	 * return error.
	 */
	if (dev == DDI_DEV_T_ANY || name == NULL || strlen(name) == 0)
		return (DDI_PROP_INVAL_ARG);

	if (ddi_prop_exists((dev == DDI_DEV_T_NONE) ? DDI_DEV_T_ANY : dev,
	    dip, (flag | DDI_PROP_SYSTEM_DEF), name) == 0)
		return (DDI_PROP_NOT_FOUND);

	if (!(flag & DDI_PROP_CANSLEEP))
		flag |= DDI_PROP_DONTSLEEP;
	return (ddi_prop_update_common(dev, dip,
		(flag | DDI_PROP_SYSTEM_DEF | DDI_PROP_TYPE_BYTE),
		name, value, length, ddi_prop_fm_encode_bytes));
}


/*
 * Common lookup routine used to lookup and decode a property.
 * Creates a property handle, searches for the raw encoded data,
 * fills in the handle, and calls the property decode functions
 * passed in.
 *
 * This routine is not static because ddi_bus_prop_op() which lives in
 * ddi_impl.c calls it.  No driver should be calling this routine.
 */
int
ddi_prop_lookup_common(dev_t match_dev, dev_info_t *dip,
    uint_t flags, char *name, void *data, uint_t *nelements,
    int (*prop_decoder)(prop_handle_t *, void *data, uint_t *nelements))
{
	int		rval;
	uint_t		ourflags;
	prop_handle_t	ph;

	if ((match_dev == DDI_DEV_T_NONE) ||
		(name == NULL) || (strlen(name) == 0))
		return (DDI_PROP_INVAL_ARG);

	ourflags = (flags & DDI_PROP_DONTSLEEP) ? flags :
		flags | DDI_PROP_CANSLEEP;

	/*
	 * Get the encoded data
	 */
	bzero(&ph, sizeof (prop_handle_t));
	rval = ddi_prop_search_common(match_dev, dip, PROP_LEN_AND_VAL_ALLOC,
	    ourflags, name, &ph.ph_data, &ph.ph_size);
	if (rval != DDI_PROP_SUCCESS && rval != DDI_PROP_FOUND_1275) {
		ASSERT(ph.ph_data == NULL);
		ASSERT(ph.ph_size == 0);
		return (rval);
	}

	/*
	 * If the encoded data came from a OBP or software
	 * use the 1275 OBP decode/encode routines.
	 */
	ph.ph_cur_pos = ph.ph_data;
	ph.ph_save_pos = ph.ph_data;
	ph.ph_ops = &prop_1275_ops;
	ph.ph_flags = (rval == DDI_PROP_FOUND_1275) ? PH_FROM_PROM : 0;

	rval = (*prop_decoder)(&ph, data, nelements);

	/*
	 * Free the encoded data
	 */
	if (ph.ph_size != 0)
		kmem_free(ph.ph_data, ph.ph_size);

	return (rval);
}

/*
 * Lookup and return an array of composit properties.  The driver must
 * provide the decode routine.
 */
int
ddi_prop_lookup(dev_t match_dev, dev_info_t *dip,
    uint_t flags, char *name, void *data, uint_t *nelements,
    int (*prop_decoder)(prop_handle_t *, void *data, uint_t *nelements))
{
	return (ddi_prop_lookup_common(match_dev, dip,
	    (flags | DDI_PROP_TYPE_COMPOSITE), name,
	    data, nelements, prop_decoder));
}

/*
 * Return 1 if a property exists (no type checking done).
 * Return 0 if it does not exist.
 */
int
ddi_prop_exists(dev_t match_dev, dev_info_t *dip, uint_t flags, char *name)
{
	int	i;
	uint_t	x = 0;
	uint_t	ourflags;

	if (flags & DDI_PROP_TYPE_MASK)
		ourflags = flags;
	else
		ourflags = flags | DDI_PROP_TYPE_ANY;

	i = ddi_prop_search_common(match_dev, dip, PROP_EXISTS,
		ourflags, name, NULL, &x);
	return (i == DDI_PROP_SUCCESS || i == DDI_PROP_FOUND_1275);
}


/*
 * Update an array of composite properties.  The driver must
 * provide the encode routine.
 */
int
ddi_prop_update(dev_t match_dev, dev_info_t *dip,
    char *name, void *data, uint_t nelements,
    int (*prop_create)(prop_handle_t *, void *data, uint_t nelements))
{
	return (ddi_prop_update_common(match_dev, dip, DDI_PROP_TYPE_COMPOSITE,
	    name, data, nelements, prop_create));
}

/*
 * Get a single integer or boolean property and return it.
 * If the property does not exists, or cannot be decoded,
 * then return the defvalue passed in.
 *
 * This routine always succedes.
 */
int
ddi_prop_get_int(dev_t match_dev, dev_info_t *dip, uint_t flags,
    char *name, int defvalue)
{
	int	data;
	uint_t	nelements;
	int	rval;

	if (flags & ~(DDI_PROP_DONTPASS|DDI_PROP_NOTPROM)) {
#ifdef DEBUG
		cmn_err(CE_WARN, "ddi_prop_get_int: invalid flag"
		    " 0x%x (prop = %s, node = %s%d); invalid bits ignored",
		    flags, name, ddi_get_name(dip), ddi_get_instance(dip));
#endif /* DEBUG */
		flags &= DDI_PROP_DONTPASS|DDI_PROP_NOTPROM;
	}

	if ((rval = ddi_prop_lookup_common(match_dev, dip,
	    (flags | DDI_PROP_TYPE_INT), name, &data, &nelements,
	    ddi_prop_fm_decode_int)) != DDI_PROP_SUCCESS) {
		if (rval == DDI_PROP_END_OF_DATA)
			data = 1;
		else
			data = defvalue;
	}
	return (data);
}

/*
 * Get an array of integer property
 */
int
ddi_prop_lookup_int_array(dev_t match_dev, dev_info_t *dip, uint_t flags,
    char *name, int **data, uint_t *nelements)
{
	if (flags & ~(DDI_PROP_DONTPASS|DDI_PROP_NOTPROM)) {
#ifdef DEBUG
		cmn_err(CE_WARN, "ddi_prop_lookup_int_array: invalid flag"
		    " 0x%x (prop = %s, node = %s%d); invalid bits ignored",
		    flags, name, ddi_get_name(dip), ddi_get_instance(dip));
#endif /* DEBUG */
		flags &= DDI_PROP_DONTPASS|DDI_PROP_NOTPROM;
	}

	return (ddi_prop_lookup_common(match_dev, dip,
	    (flags | DDI_PROP_TYPE_INT), name, data,
	    nelements, ddi_prop_fm_decode_ints));
}

/*
 * Update a single integer property.  If the propery exists on the drivers
 * property list it updates, else it creates it.
 */
int
ddi_prop_update_int(dev_t match_dev, dev_info_t *dip,
    char *name, int data)
{
	return (ddi_prop_update_common(match_dev, dip, DDI_PROP_TYPE_INT,
	    name, &data, 1, ddi_prop_fm_encode_ints));
}

int
e_ddi_prop_update_int(dev_t match_dev, dev_info_t *dip,
    char *name, int data)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_SYSTEM_DEF | DDI_PROP_TYPE_INT,
	    name, &data, 1, ddi_prop_fm_encode_ints));
}

/*
 * Update an array of integer property.  If the propery exists on the drivers
 * property list it updates, else it creates it.
 */
int
ddi_prop_update_int_array(dev_t match_dev, dev_info_t *dip,
    char *name, int *data, uint_t nelements)
{
	return (ddi_prop_update_common(match_dev, dip, DDI_PROP_TYPE_INT,
	    name, data, nelements, ddi_prop_fm_encode_ints));
}

int
e_ddi_prop_update_int_array(dev_t match_dev, dev_info_t *dip,
    char *name, int *data, uint_t nelements)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_SYSTEM_DEF | DDI_PROP_TYPE_INT,
	    name, data, nelements, ddi_prop_fm_encode_ints));
}


/*
 * Get a single string property.
 */
int
ddi_prop_lookup_string(dev_t match_dev, dev_info_t *dip, uint_t flags,
    char *name, char **data)
{
	uint_t x;

	if (flags & ~(DDI_PROP_DONTPASS|DDI_PROP_NOTPROM)) {
#ifdef DEBUG
		cmn_err(CE_WARN, "ddi_prop_lookup_string: invalid flag"
		    " 0x%x (prop = %s, node = %s%d); invalid bits ignored",
		    flags, name, ddi_get_name(dip), ddi_get_instance(dip));
#endif /* DEBUG */
		flags &= DDI_PROP_DONTPASS|DDI_PROP_NOTPROM;
	}

	return (ddi_prop_lookup_common(match_dev, dip,
	    (flags | DDI_PROP_TYPE_STRING), name, data,
	    &x, ddi_prop_fm_decode_string));
}

/*
 * Get an array of strings property.
 */
int
ddi_prop_lookup_string_array(dev_t match_dev, dev_info_t *dip, uint_t flags,
    char *name, char ***data, uint_t *nelements)
{
	if (flags & ~(DDI_PROP_DONTPASS|DDI_PROP_NOTPROM)) {
#ifdef DEBUG
		cmn_err(CE_WARN, "ddi_prop_lookup_string_array: invalid flag"
		    " 0x%x (prop = %s, node = %s%d); invalid bits ignored",
		    flags, name, ddi_get_name(dip), ddi_get_instance(dip));
#endif /* DEBUG */
		flags &= DDI_PROP_DONTPASS|DDI_PROP_NOTPROM;
	}

	return (ddi_prop_lookup_common(match_dev, dip,
	    (flags | DDI_PROP_TYPE_STRING), name, data,
	    nelements, ddi_prop_fm_decode_strings));
}

/*
 * Update a single string property.
 */
int
ddi_prop_update_string(dev_t match_dev, dev_info_t *dip,
    char *name, char *data)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_TYPE_STRING, name, &data, 1,
	    ddi_prop_fm_encode_string));
}

int
e_ddi_prop_update_string(dev_t match_dev, dev_info_t *dip,
    char *name, char *data)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_SYSTEM_DEF | DDI_PROP_TYPE_STRING,
	    name, &data, 1, ddi_prop_fm_encode_string));
}


/*
 * Update an array of strings property.
 */
int
ddi_prop_update_string_array(dev_t match_dev, dev_info_t *dip,
    char *name, char **data, uint_t nelements)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_TYPE_STRING, name, data, nelements,
	    ddi_prop_fm_encode_strings));
}

int
e_ddi_prop_update_string_array(dev_t match_dev, dev_info_t *dip,
    char *name, char **data, uint_t nelements)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_SYSTEM_DEF | DDI_PROP_TYPE_STRING,
	    name, data, nelements,
	    ddi_prop_fm_encode_strings));
}


/*
 * Get an array of bytes property.
 */
int
ddi_prop_lookup_byte_array(dev_t match_dev, dev_info_t *dip, uint_t flags,
    char *name, uchar_t **data, uint_t *nelements)
{
	if (flags & ~(DDI_PROP_DONTPASS|DDI_PROP_NOTPROM)) {
#ifdef DEBUG
		cmn_err(CE_WARN, "ddi_prop_lookup_byte_array: invalid flag"
		    " 0x%x (prop = %s, node = %s%d); invalid bits ignored",
		    flags, name, ddi_get_name(dip), ddi_get_instance(dip));
#endif /* DEBUG */
		flags &= DDI_PROP_DONTPASS|DDI_PROP_NOTPROM;
	}

	return (ddi_prop_lookup_common(match_dev, dip,
	    (flags | DDI_PROP_TYPE_BYTE), name, data,
	    nelements, ddi_prop_fm_decode_bytes));
}

/*
 * Update an array of bytes property.
 */
int
ddi_prop_update_byte_array(dev_t match_dev, dev_info_t *dip,
    char *name, uchar_t *data, uint_t nelements)
{
	if (nelements == 0)
		return (DDI_PROP_INVAL_ARG);

	return (ddi_prop_update_common(match_dev, dip, DDI_PROP_TYPE_BYTE,
	    name, data, nelements, ddi_prop_fm_encode_bytes));
}


int
e_ddi_prop_update_byte_array(dev_t match_dev, dev_info_t *dip,
    char *name, uchar_t *data, uint_t nelements)
{
	if (nelements == 0)
		return (DDI_PROP_INVAL_ARG);

	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_SYSTEM_DEF | DDI_PROP_TYPE_BYTE,
	    name, data, nelements, ddi_prop_fm_encode_bytes));
}


/*
 * ddi_prop_remove_common:	Undefine a managed property:
 *			Input dev_t must match dev_t when defined.
 *			Returns DDI_PROP_NOT_FOUND, possibly.
 *			DDI_PROP_INVAL_ARG is also possible if dev is
 *			DDI_DEV_T_ANY or incoming name is the NULL string.
 */
int
ddi_prop_remove_common(dev_t dev, dev_info_t *dip, char *name, int flag)
{
	ddi_prop_t	**list_head = &(DEVI(dip)->devi_drv_prop_ptr);
	ddi_prop_t	*propp;
	ddi_prop_t	*lastpropp = NULL;

	if ((dev == DDI_DEV_T_ANY) || (name == (char *)0) ||
	    (strlen(name) == 0)) {
		return (DDI_PROP_INVAL_ARG);
	}

	if (flag & DDI_PROP_SYSTEM_DEF)
		list_head = &(DEVI(dip)->devi_sys_prop_ptr);
	else if (flag & DDI_PROP_HW_DEF)
		list_head = &(DEVI(dip)->devi_hw_prop_ptr);

	mutex_enter(&(DEVI(dip)->devi_lock));

	for (propp = *list_head; propp != NULL; propp = propp->prop_next)  {
		if ((strcmp(name, propp->prop_name) == 0) &&
		    (dev == propp->prop_dev)) {
			/*
			 * Unlink this propp allowing for it to
			 * be first in the list:
			 */

			if (lastpropp == NULL)
				*list_head = propp->prop_next;
			else
				lastpropp->prop_next = propp->prop_next;

			mutex_exit(&(DEVI(dip)->devi_lock));

			/*
			 * Free memory and return...
			 */
			kmem_free(propp->prop_name,
			    strlen(propp->prop_name) + 1);
			if (propp->prop_len != 0)
				kmem_free(propp->prop_val, propp->prop_len);
			kmem_free(propp, sizeof (ddi_prop_t));
			return (DDI_PROP_SUCCESS);
		}
		lastpropp = propp;
	}
	mutex_exit(&(DEVI(dip)->devi_lock));
	return (DDI_PROP_NOT_FOUND);
}

int
ddi_prop_remove(dev_t dev, dev_info_t *dip, char *name)
{
	return (ddi_prop_remove_common(dev, dip, name, 0));
}

int
e_ddi_prop_remove(dev_t dev, dev_info_t *dip, char *name)
{
	return (ddi_prop_remove_common(dev, dip, name, DDI_PROP_SYSTEM_DEF));
}

/*
 * e_ddi_prop_list_delete: remove a list of properties
 *	Note that the caller needs to provide the required protection
 *	(eg. devi_lock if these properties are still attached to a devi)
 */
void
e_ddi_prop_list_delete(ddi_prop_t *props)
{
	ddi_prop_t	*propp;
	ddi_prop_t	*freep;

	propp = props;
	while (propp != NULL)  {
		freep = propp;
		propp = propp->prop_next;
		kmem_free(freep->prop_name, strlen(freep->prop_name) + 1);
		if (freep->prop_len != 0) {
			kmem_free(freep->prop_val, freep->prop_len);
		}
		kmem_free(freep, sizeof (ddi_prop_t));
	}
}

/*
 * ddi_prop_remove_all_common:
 *	Used before unloading a driver to remove
 *	all properties. (undefines all dev_t's props.)
 *	Also removes `explicitly undefined' props.
 *	No errors possible.
 */
void
ddi_prop_remove_all_common(dev_info_t *dip, int flag)
{
	ddi_prop_t	**list_head;

	mutex_enter(&(DEVI(dip)->devi_lock));
	if (flag & DDI_PROP_SYSTEM_DEF) {
		list_head = &(DEVI(dip)->devi_sys_prop_ptr);
	} else if (flag & DDI_PROP_HW_DEF) {
		list_head = &(DEVI(dip)->devi_hw_prop_ptr);
	} else {
		list_head = &(DEVI(dip)->devi_drv_prop_ptr);
	}
	e_ddi_prop_list_delete(*list_head);
	*list_head = NULL;
	mutex_exit(&(DEVI(dip)->devi_lock));
}


/*
 * ddi_prop_remove_all:		Remove all driver prop definitions.
 */

void
ddi_prop_remove_all(dev_info_t *dip)
{
	ddi_prop_remove_all_common(dip, 0);
}

/*
 * e_ddi_prop_remove_all:	Remove all system prop definitions.
 */

void
e_ddi_prop_remove_all(dev_info_t *dip)
{
	ddi_prop_remove_all_common(dip, (int)DDI_PROP_SYSTEM_DEF);
}


/*
 * ddi_prop_undefine:	Explicitly undefine a property.  Property
 *			searches which match this property return
 *			the error code DDI_PROP_UNDEFINED.
 *
 *			Use ddi_prop_remove to negate effect of
 *			ddi_prop_undefine
 *
 *			See above for error returns.
 *
 * XXX:			These interfaces are currently not called anywhere
 *			in the framework. Should that change in the future,
 *			a new interface, ndi_prop_undefine, should be added
 *			to complete the story.
 */

int
ddi_prop_undefine(dev_t dev, dev_info_t *dip, int flag, char *name)
{
	if (!(flag & DDI_PROP_CANSLEEP))
		flag |= DDI_PROP_DONTSLEEP;
	return (ddi_prop_update_common(dev, dip,
	    (flag | DDI_PROP_STACK_CREATE | DDI_PROP_UNDEF_IT |
	    DDI_PROP_TYPE_BYTE), name, NULL, 0, ddi_prop_fm_encode_bytes));
}

int
e_ddi_prop_undefine(dev_t dev, dev_info_t *dip, int flag, char *name)
{
	if (!(flag & DDI_PROP_CANSLEEP))
		flag |= DDI_PROP_DONTSLEEP;
	return (ddi_prop_update_common(dev, dip,
	    (flag | DDI_PROP_SYSTEM_DEF | DDI_PROP_STACK_CREATE |
		DDI_PROP_UNDEF_IT | DDI_PROP_TYPE_BYTE),
	    name, NULL, 0, ddi_prop_fm_encode_bytes));
}

/*
 * The ddi_bus_prop_op default bus nexus prop op function.
 *
 * The implementation of this routine is in ddi_impl.c
 *
 * Code to search hardware layer (PROM), if it exists,
 * on behalf of child, then, if appropriate, ascend and check
 * my own software defined properties...
 *
 * if input dip != child_dip, then call is on behalf of child
 * to search PROM, do it via ddi_bus_prop_op and ascend only
 * if allowed.
 *
 * if input dip == ch_dip (child_dip), call is on behalf of root driver,
 * to search for PROM defined props only.
 *
 * Note that the PROM search is done only if the requested dev
 * is either DDI_DEV_T_ANY or DDI_DEV_T_NONE. PROM properties
 * have no associated dev, thus are automatically associated with
 * DDI_DEV_T_NONE.
 *
 * Modifying flag DDI_PROP_NOTPROM inhibits the search in the h/w layer.
 */

int
ddi_bus_prop_op(dev_t dev, dev_info_t *dip, dev_info_t *ch_dip,
    ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	int	error;

	error = impl_ddi_bus_prop_op(dev, dip, ch_dip, prop_op, mod_flags,
				    name, valuep, lengthp);

	if (error == DDI_PROP_SUCCESS || error == DDI_PROP_FOUND_1275)
		return (error);

	if (error == DDI_PROP_NO_MEMORY) {
		cmn_err(CE_CONT, prop_no_mem_msg, name);
		return (DDI_PROP_NO_MEMORY);
	}

	/*
	 * Check the 'options' node as a last resort
	 */
	if ((mod_flags & DDI_PROP_DONTPASS) != 0)
		return (DDI_PROP_NOT_FOUND);

	if (ch_dip == ddi_root_node())	{
		static char options[] = "options";
		static dev_info_t *options_dip;
		/*
		 * As a last resort, when we've reached
		 * the top and still haven't found the
		 * property, find the 'options' node
		 * (if it exists) and see if the desired
		 * property is attached to it.
		 *
		 * Load the driver if we haven't already done it,
		 * and we know it is not unloadable.
		 */
		if ((options_dip == NULL) &&
		    (ddi_install_driver(options) == DDI_SUCCESS))  {
			options_dip = ddi_find_devinfo(options, -1, 0);
		}
		ASSERT(options_dip != NULL);
		/*
		 * Force the "don't pass" flag to *just* see
		 * what the options node has to offer.
		 */
		return (ddi_prop_search_common(dev, options_dip, prop_op,
		    mod_flags|DDI_PROP_DONTPASS, name, valuep,
		    (uint_t *)lengthp));
	}

	/*
	 * Otherwise, continue search with parent's s/w defined properties...
	 * NOTE: Using `dip' in following call increments the level.
	 */

	return (ddi_prop_search_common(dev, dip, prop_op, mod_flags,
	    name, valuep, (uint_t *)lengthp));
}

/*
 * External property functions used by other parts of the kernel...
 */

/*
 * e_ddi_getlongprop: See comments for ddi_get_longprop.
 *			dev to dip conversion performed by driver.
 */

int
e_ddi_getlongprop(dev_t dev, vtype_t type, char *name, int flags,
    caddr_t valuep, int *lengthp)
{
	dev_info_t *devi;
	ddi_prop_op_t prop_op = PROP_LEN_AND_VAL_ALLOC;
	int error;

	if ((devi = e_ddi_get_dev_info(dev, type)) == NULL)
		return (DDI_PROP_NOT_FOUND);	/* XXX */
	error = cdev_prop_op(dev, devi, prop_op, flags, name, valuep, lengthp);
	ddi_rele_driver(getmajor(dev));
	return (error);
}

/*
 * e_ddi_getlongprop_buf:	See comments for ddi_getlongprop_buf.
 *				dev to dip conversion done by driver.
 */

int
e_ddi_getlongprop_buf(dev_t dev, vtype_t type, char *name, int flags,
    caddr_t valuep, int *lengthp)
{
	dev_info_t *devi;
	ddi_prop_op_t prop_op = PROP_LEN_AND_VAL_BUF;
	int error;

	if ((devi = e_ddi_get_dev_info(dev, type)) == NULL)
		return (DDI_PROP_NOT_FOUND);	/* XXX */
	error = cdev_prop_op(dev, devi, prop_op, flags, name, valuep, lengthp);
	ddi_rele_driver(getmajor(dev));
	return (error);
}

/*
 * e_ddi_getprop:	See comments for ddi_getprop.
 *			dev to dip conversion done by driver.
 */
int
e_ddi_getprop(dev_t dev, vtype_t type, char *name, int flags, int defvalue)
{
	dev_info_t *devi;
	ddi_prop_op_t prop_op = PROP_LEN_AND_VAL_BUF;
	int	propvalue = defvalue;
	int	proplength = sizeof (int);
	int	error;

	if ((devi = e_ddi_get_dev_info(dev, type)) == NULL)
		return (defvalue);
	error = cdev_prop_op(dev, devi, prop_op,
	    flags, name, (caddr_t)&propvalue, &proplength);
	ddi_rele_driver(getmajor(dev));

	if ((error == DDI_PROP_SUCCESS) && (proplength == 0))
		propvalue = 1;

	return (propvalue);
}

/*
 * e_ddi_getprop_int64:	See comments for ddi_getprop.
 *			dev to dip conversion done by driver.
 */
int64_t
e_ddi_getprop_int64(dev_t dev, vtype_t type, char *name,
	int flags, int64_t defvalue)
{
	dev_info_t *devi;
	ddi_prop_op_t prop_op = PROP_LEN_AND_VAL_BUF;
	int64_t	propvalue = defvalue;
	int	proplength = sizeof (int64_t);
	int	error;

	if ((devi = e_ddi_get_dev_info(dev, type)) == NULL)
		return (defvalue);
	error = cdev_prop_op(dev, devi, prop_op,
	    flags, name, (caddr_t)&propvalue, &proplength);
	ddi_rele_driver(getmajor(dev));

	if ((error == DDI_PROP_SUCCESS) && (proplength == 0))
		propvalue = 1;

	return (propvalue);
}

/*
 * e_ddi_getproplen:	See comments for ddi_getproplen.
 *			dev to dip conversion done by driver.
 */
int
e_ddi_getproplen(dev_t dev, vtype_t type, char *name, int flags, int *lengthp)
{
	dev_info_t *devi;
	ddi_prop_op_t prop_op = PROP_LEN;
	int error;

	if ((devi = e_ddi_get_dev_info(dev, type)) == NULL)
		return (DDI_PROP_NOT_FOUND);	/* XXX */
	error = cdev_prop_op(dev, devi, prop_op, flags, name, NULL, lengthp);
	ddi_rele_driver(getmajor(dev));
	return (error);
}


/*
 * e_ddi_get_dev_info:	Call driver's devo_getinfo entry point if defined.
 *
 * NOTE: The dev_get_dev_info() routine returns with a hold on the
 * devops for the underlying driver.  The caller should ensure that
 * they get decremented again (once the object is freeable!)
 */

static char bad_dev[] =
	"e_ddi_get_dev_info: Illegal major device number <%d>";

dev_info_t *
e_ddi_get_dev_info(dev_t dev, vtype_t type)
{
	dev_info_t *dip = (dev_info_t *)0;

	switch (type) {
	case VCHR:
	case VBLK:
		if (getmajor(dev) >= devcnt)
			cmn_err(CE_CONT, bad_dev, getmajor(dev));
		else
			dip = dev_get_dev_info(dev, OTYP_CHR);
		break;

	default:
		break;
	}

	return (dip);
}

/*
 * Functions for the manipulation of dev_info structures
 */

int
i_ddi_while_sibling(dev_info_t *dev, int (*f)(dev_info_t *, void *), void *arg)
{
	dev_info_t *dip;
	dev_info_t *sib;

	sib = dip = dev;

	while (dip != (dev_info_t *)NULL) {
		sib = (dev_info_t *)DEVI(dip)->devi_sibling;
		switch ((*f)(dip, arg)) {
		case DDI_WALK_TERMINATE:
			return (DDI_WALK_TERMINATE);

		case DDI_WALK_PRUNESIB:
			if (DEVI(dip)->devi_child == NULL) {
				return (DDI_WALK_CONTINUE);
			}
			/* else fall through and visit the child */
			/*FALLTHRU*/

		case DDI_WALK_CONTINUE:
			/* If no child, continue down the sibling chain */
			if (DEVI(dip)->devi_child == NULL) {
				break;
			}
			/* Recurse on child. */
			if (i_ddi_walk_devs(
					(dev_info_t *)DEVI(dip)->devi_child,
					f, arg) == DDI_WALK_TERMINATE) {
				return (DDI_WALK_TERMINATE);
			}
			break;

		case DDI_WALK_PRUNECHILD:
		default:
			/* Continue down the sibling chain */
			break;
		}
		dip = sib;
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * The implementation of ddi_walk_devs().
 *
 * It is very important that the the function 'f' not remove the node passed
 * into it.
 */
int
i_ddi_walk_devs(dev_info_t *dev, int (*f)(dev_info_t *, void *), void *arg)
{
	dev_info_t *lw = dev;

	while (lw != (dev_info_t *)NULL) {

		switch ((*f)(lw, arg)) {
		case DDI_WALK_TERMINATE:
			/*
			 * Caller is done!  Just return.
			 */
			return (DDI_WALK_TERMINATE);
			/*NOTREACHED*/

		case DDI_WALK_PRUNESIB:
			/*
			 * Caller has told us not to continue with our siblings.
			 * If we have children, then set lw to point the them
			 * and start over.  Else we are done.
			 */
			if (DEVI(lw)->devi_child == NULL)
				return (DDI_WALK_CONTINUE);
			lw = (dev_info_t *)DEVI(lw)->devi_child;
			break;

		case DDI_WALK_PRUNECHILD:
			/*
			 * Caller has told us that we don't need to go down to
			 * our child.  So we can move onto the next node
			 * (sibling) without having to use recursion.  There
			 * is no need to come back to this node ever.
			 */
			lw = (dev_info_t *)DEVI(lw)->devi_sibling;
			break;

		case DDI_WALK_CONTINUE:
		default:
			/*
			 * If we have a child node, we need to stop, and use
			 * recursion to continue with our sibling nodes.  When
			 * all sibling nodes and their children are done, then
			 * we can do our child node.
			 */
			if (DEVI(lw)->devi_child != NULL) {
				if (i_ddi_while_sibling(
				    (dev_info_t *)DEVI(lw)->devi_sibling,
				    f, arg) == DDI_WALK_TERMINATE) {
					return (DDI_WALK_TERMINATE);
				}

				/*
				 * Set lw to our child node and start over.
				 */
				lw = (dev_info_t *)DEVI(lw)->devi_child;
				break;
			}
			/*
			 * else we can move onto the next node (sibling)
			 * without having to use recursion.  This is because
			 * there is no child node so we don't have to come
			 * back to this node.  We are done with it forever.
			 */
			lw = (dev_info_t *)DEVI(lw)->devi_sibling;
			break;
		}

	}

	return (DDI_WALK_CONTINUE);
}

/*
 * This general-purpose routine traverses the tree of dev_info nodes,
 * starting from the given node, and calls the given function for each
 * node that it finds with the current node and the pointer arg (which
 * can point to a structure of information that the function
 * needs) as arguments.
 *
 * It does the walk a layer at a time, not depth-first.
 *
 * The given function must return one of the values defined above and must
 * not remove the dev_info_t passed into it..
 *
 */

void
ddi_walk_devs(dev_info_t *dev, int (*f)(dev_info_t *, void *), void *arg)
{
	(void) i_ddi_walk_devs(dev, f, arg);
}

/*
 * Routines to get at elements of the dev_info structure
 */

/*
 * ddi_binding_name: Return the driver binding name of the devinfo node
 *		This is the name the OS used to bind the node to a driver.
 */
char *
ddi_binding_name(dev_info_t *dip)
{
	return (DEVI(dip)->devi_binding_name);
}

/*
 * ddi_driver_name: Return the normalized driver name. this is the
 *		actual driver name
 */
const char *
ddi_driver_name(dev_info_t *devi)
{
	const char *name = ddi_binding_name(devi);
	major_t major;

	if (name) {
		major = ddi_name_to_major((char *)name);
		if (major != (major_t)-1) {
			name = (const char *)ddi_major_to_name(major);
		}
		return (name);
	}

	return ((const char *) ddi_node_name(devi));
}

/*
 * i_ddi_set_binding_name:	Set binding name.
 *
 *	Set the binding name to the given name.
 *	This routine is for use by the ddi implementation, not by drivers.
 */
void
i_ddi_set_binding_name(dev_info_t *dip, char *name)
{
	DEVI(dip)->devi_binding_name = name;

}

/*
 * ddi_get_name: A synonym of ddi_binding_name() ... returns a name
 * the implementation has used to bind the node to a driver.
 */
char *
ddi_get_name(dev_info_t *dip)
{
	return (DEVI(dip)->devi_binding_name);
}

/*
 * ddi_node_name: Return the name property of the devinfo node
 *		This may differ from ddi_binding_name if the node name
 *		does not define a binding to a driver.
 */
char *
ddi_node_name(dev_info_t *dip)
{
	return (DEVI(dip)->devi_node_name);
}


/*
 * ddi_get_nodeid:	Get nodeid stored in dev_info structure.
 */
int
ddi_get_nodeid(dev_info_t *dip)
{
	return (DEVI(dip)->devi_nodeid);
}

int
ddi_get_instance(dev_info_t *dip)
{
	return (DEVI(dip)->devi_instance);
}

struct dev_ops *
ddi_get_driver(dev_info_t *dip)
{
	return (DEVI(dip)->devi_ops);
}

void
ddi_set_driver(dev_info_t *dip, struct dev_ops *devo)
{
	DEVI(dip)->devi_ops = devo;
}

/*
 * ddi_set_driver_private/ddi_get_driver_private:
 * Get/set device driver private data in devinfo.
 */
void
ddi_set_driver_private(dev_info_t *dip, caddr_t data)
{
	DEVI(dip)->devi_driver_data = data;
}

caddr_t
ddi_get_driver_private(dev_info_t *dip)
{
	return ((caddr_t)DEVI(dip)->devi_driver_data);
}

/*
 * ddi_get_parent, ddi_get_child, ddi_get_next_sibling
 */

dev_info_t *
ddi_get_parent(dev_info_t *dip)
{
	return ((dev_info_t *)DEVI(dip)->devi_parent);
}

dev_info_t *
ddi_get_child(dev_info_t *dip)
{
	return ((dev_info_t *)DEVI(dip)->devi_child);
}

dev_info_t *
ddi_get_next_sibling(dev_info_t *dip)
{
	return ((dev_info_t *)DEVI(dip)->devi_sibling);
}

dev_info_t *
ddi_get_next(dev_info_t *dip)
{
	return ((dev_info_t *)DEVI(dip)->devi_next);
}

void
ddi_set_next(dev_info_t *dip, dev_info_t *nextdip)
{
	DEVI(dip)->devi_next = DEVI(nextdip);
}

/*
 * ddi_add_child:	Add a child dev_info to the specified parent.
 *			A zeroed dev_info structure is allocated
 *			and added as a child of the given "dip".
 *			The new dev_info pointer is returned if
 *			successful.
 */

dev_info_t *
ddi_add_child(dev_info_t *pdip, char *name, uint_t nodeid, uint_t unit)
{
	return (i_ddi_add_child(pdip, name, nodeid, unit));
}


/*
 * dn list management
 */
#define	DDI_LOCK_HELD	1
#define	DDI_LIST_HELD	1
void i_ddi_remove_from_dn_list(struct devnames *dnp, dev_info_t **list,
    dev_info_t *dip, uint_t listheld, uint_t lockheld);
void i_ddi_add_to_dn_list(struct devnames *dnp, dev_info_t **list,
    dev_info_t *dip, uint_t listheld, uint_t lockheld);


/*
 * ddi_remove_child:	Remove the given child and free the space
 *			occupied by the dev_info structure.
 *
 *			Parent and driver private data should already
 *			be released before calling this function.
 *
 *			If there are children devices to this dev, we
 *			refuse to free the device.
 */
int
ddi_remove_child(dev_info_t *dip, int lockheld)
{
	NDI_CONFIG_DEBUG((CE_CONT,
		"ddi_remove_child: %s%d\n", ddi_get_name(dip),
		ddi_get_instance(dip)));

	/*
	 * remove from new dev request queue
	 */
	i_ndi_remove_new_devinfo(dip);

	return (i_ddi_remove_child(dip, lockheld));
}


/*
 * This routine appends the second argument onto the children list for the
 * first argument. It always appends it to the end of the list.
 * It also adds the new instance to the linked list of driver instances.
 */
void
ddi_append_dev(dev_info_t *pdip, dev_info_t *cdip)
{
	struct dev_info *dev;
	major_t maj;
	struct devnames *dnp;

	/*
	 * We allow the implementation to determine how to bind
	 * a node to a driver by calling i_ddi_bind_node_to_driver.
	 *
	 * The default and first action should be to see if
	 * devi_node_name binds to a driver name; if it
	 * does, set devi_binding_name using i_ddi_set_binding_name
	 * and return the major number of the driver;
	 *
	 * If not, determine a possible alternate binding, and set
	 * devi_binding_name to that alternate binding name using
	 * i_ddi_set_binding_name, and return the major number
	 * of the driver;
	 *
	 * If there is no binding, return -1 and leave devi_binding_name
	 * unchanged.
	 *
	 * XXX The dev_info node binding name must be set before the node
	 *	is linked to the device tree.
	 */
	maj = i_ddi_bind_node_to_driver(cdip);

	rw_enter(&(devinfo_tree_lock), RW_WRITER);
	if ((dev = DEVI(pdip)->devi_child) == (struct dev_info *)NULL) {
		DEVI(pdip)->devi_child = DEVI(cdip);
	} else {
		while (dev->devi_sibling != (struct dev_info *)NULL)
			dev = dev->devi_sibling;
		dev->devi_sibling = DEVI(cdip);
	}
	DEVI(cdip)->devi_parent = DEVI(pdip);
	DEVI(cdip)->devi_bus_ctl = DEVI(pdip);	/* XXX until 1106021 is fixed */
	rw_exit(&(devinfo_tree_lock));

	/*
	 * Add hardware devinfo nodes to the orphan list, add unbound
	 * nodes to the orphan list ... any time a driver is installed,
	 * we scan the orphan list.  Any time a driver is unloaded,
	 * its hardware nodes migrate back to the orphan list.
	 */
	dnp = &orphanlist;
	if ((ndi_dev_is_persistent_node(cdip) == 0) && (maj != (major_t)-1))
		dnp = &(devnamesp[maj]);

	/*
	 * Add this instance to the linked list of instances.
	 */
	DEVI(cdip)->devi_next = NULL;
	i_ddi_add_to_dn_list(dnp, &dnp->dn_head, cdip, 0, 0);

#ifdef	BIND_DEBUG
	if (strcmp(ddi_node_name(cdip), ddi_binding_name(cdip)) != 0)
		cmn_err(CE_CONT, "?Node %x <%s> bound to <%s>\n",
		    ddi_get_nodeid(cdip), ddi_node_name(cdip),
		    ddi_binding_name(cdip));
#endif	BIND_DEBUG
}

/*
 * add a list of device nodes to the device node list in the
 * devnames structure
 */
void
i_ddi_add_to_dn_list(struct devnames *dnp, dev_info_t **plist, dev_info_t *dip,
    uint_t listheld, uint_t lockheld)
{
	dev_info_t *list;
	int listcnt;

	if (!lockheld) {
		LOCK_DEV_OPS(&(dnp->dn_lock));
	} else {
		ASSERT(mutex_owned(&(dnp->dn_lock)));
	}
	if (!listheld) {
		e_ddi_enter_driver_list(dnp, &listcnt);
	}

	list = *plist;
	while (list && (list != dip)) {
		plist = (dev_info_t **)(&DEVI(list)->devi_next);
		list = (dev_info_t *)DEVI(list)->devi_next;
	}

	if (list == NULL) {
		*plist = dip;
	}

	if (!listheld) {
		e_ddi_exit_driver_list(dnp, listcnt);
	}
	if (!lockheld) {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
	}
}

void
i_ddi_remove_from_dn_list(struct devnames *dnp, dev_info_t **plist,
    dev_info_t *dip, uint_t listheld, uint_t lockheld)
{
	dev_info_t *list;
	int listcnt;

	if (!lockheld) {
		LOCK_DEV_OPS(&(dnp->dn_lock));
	} else {
		ASSERT(mutex_owned(&(dnp->dn_lock)));
	}
	if (!listheld) {
		e_ddi_enter_driver_list(dnp, &listcnt);
	}

	list = *plist;
	while (list) {
		if (list == dip) {
			*plist = (dev_info_t *)(DEVI(dip)->devi_next);
			DEVI(dip)->devi_next = NULL;
			break;
		}
		plist = (dev_info_t **)(&DEVI(list)->devi_next);
		list = (dev_info_t *)DEVI(list)->devi_next;
	}

	if (!listheld) {
		e_ddi_exit_driver_list(dnp, listcnt);
	}
	if (!lockheld) {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
	}
}


/*
 * Add this *list* of devinfos to the end of the orphan list.
 * Not an external ddi function!
 * If the node is already in the orphanlist, just return
 * If a single node is added, the called must zero devi_next!
 */
void
ddi_orphan_devs(dev_info_t *dip)
{
#ifdef DEBUG
	major_t major = ddi_name_to_major(ddi_get_name(dip));

	ASSERT(!((major != (major_t)-1) &&
				(CB_DRV_INSTALLED(devopsp[major]))));
#endif

	NDI_CONFIG_DEBUG((CE_CONT,
		"ddi_orphan_devs: %s, major=%d\n",
		ddi_get_name(dip),
		ddi_name_to_major(ddi_get_name(dip))));

	i_ddi_add_to_dn_list(&orphanlist, &orphanlist.dn_head, dip, 0, 0);
}

/*
 * Add this list of devinfos to the end of the new dev list.
 * Not an external ddi function!
 * If the node is already in the orphanlist or per-driver list
 * remove it.
 */
void
ddi_remove_orphan(dev_info_t *dip)
{
	i_ddi_remove_from_dn_list(&orphanlist, &orphanlist.dn_head, dip, 0, 0);
}

/*
 * If we re-read the major number binding file, try to
 * add any orphaned devinfo nodes to the devices instance list.
 * Not an external DDI function!
 */
void
ddi_unorphan_devs(major_t target)
{
	major_t major;
	struct devnames *dnp;
	dev_info_t *dip;
	dev_info_t *ndip, *pdip;

#ifdef lint
	ndip = NULL;	/* See 1094364 */
#endif

	LOCK_DEV_OPS(&(orphanlist.dn_lock));
	pdip = NULL;
	for (dip = orphanlist.dn_head; dip != NULL; dip = ndip)  {
		ndip = ddi_get_next(dip);

		if (((major = i_ddi_bind_node_to_driver(dip)) == -1) ||
		    (major != target)) {
			pdip = dip;
			continue;
		}

		/* Unlink dip from orphanlist. */
		if (pdip == NULL)
			orphanlist.dn_head = (dev_info_t *)DEVI(dip)->devi_next;
		else
			DEVI(pdip)->devi_next = DEVI(ndip);

		/*
		 * Add it to major's instance list.
		 *
		 * do not enter the driver list, modload_thread
		 * will deadlock with modload.
		 * It is assumed that there is no need to enter the
		 * driver list, either it is already held or it is
		 * safe not to enter (startup).
		 */
		dnp = &devnamesp[major];
		DEVI(dip)->devi_next = NULL;
		i_ddi_add_to_dn_list(dnp, &dnp->dn_head, dip,
			DDI_LIST_HELD, 0);

	}
	UNLOCK_DEV_OPS(&(orphanlist.dn_lock));
}

/*
 * ddi_load_driver(name)
 * Load the device driver bound to 'name'.
 */
int
ddi_load_driver(char *name)
{
	major_t major;
	int	listcnt;

	if ((modloadonly("drv", name) == -1) ||
	    ((major = ddi_name_to_major(name)) == -1))
		return (DDI_FAILURE);

	ddi_unorphan_devs(major);	/* For swapgeneric */

	/*
	 * make sure we are the only thread in the per-driver list
	 */
	LOCK_DEV_OPS(&(devnamesp[major].dn_lock));
	e_ddi_enter_driver_list(&devnamesp[major], &listcnt);
	UNLOCK_DEV_OPS(&(devnamesp[major].dn_lock));

	(void) impl_make_parlist(major);

	LOCK_DEV_OPS(&(devnamesp[major].dn_lock));
	e_ddi_exit_driver_list(&devnamesp[major], listcnt);
	UNLOCK_DEV_OPS(&(devnamesp[major].dn_lock));
	return (DDI_SUCCESS);
}

struct dev_ops *
ddi_hold_installed_driver(major_t major)
{
	struct devnames *dnp;
	int circular;
	struct dev_ops *ops;
	char *name = ddi_major_to_name(major);

	NDI_CONFIG_DEBUG((CE_CONT,
	    "ddi_hold_installed_driver: major=%d (%s)\n",
	    major, ((name == NULL) ? "<none>" : name)));

	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&dnp->dn_lock);
	/*
	 * Fast path for performance
	 *
	 * Check to see if the driver is already there with no
	 * circular dependency. If so just increment the refcnt
	 * and return. Otherwise, attempt to load the driver.
	 */
	if ((dnp->dn_flags & DN_WALKED_TREE) &&
	    !DN_BUSY_CHANGING(dnp->dn_flags)) {
		ops = devopsp[major];
		INCR_DEV_OPS_REF(ops);
		UNLOCK_DEV_OPS(&dnp->dn_lock);
		return (ops);
	}

	/*
	 * single thread driver entry
	 */
	e_ddi_enter_driver_list(dnp, &circular);
	if ((dnp->dn_flags & DN_WALKED_TREE) && (circular == 0)) {
		ops = devopsp[major];
		INCR_DEV_OPS_REF(ops);
		e_ddi_exit_driver_list(dnp, circular);
		UNLOCK_DEV_OPS(&dnp->dn_lock);
		return (ops);
		/*NOTREACHED*/
	}
	UNLOCK_DEV_OPS(&dnp->dn_lock);

	/*
	 * Load the driver; if this fails there is nothing left to do.
	 */
	if ((ops = mod_hold_dev_by_major(major)) == NULL) {
		LOCK_DEV_OPS(&(dnp->dn_lock));
		e_ddi_exit_driver_list(dnp, circular);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (NULL);
	}

	ASSERT(dnp->dn_flags & DN_BUSY_LOADING);
	ASSERT(dnp->dn_busy_thread == curthread);

	/*
	 * If dev_info nodes haven't been made from the hwconf file
	 * for this driver, make them.	If we are in a circular dependency,
	 * and this driver is a nexus driver, skip this to avoid
	 * infinite recursion -- non-sid nexus cycles are not supported.
	 * (a -> b -> a where `a' is a nexus is not supported unless the
	 * nodes are self-identifying.
	 */
	if ((!((circular) && NEXUS_DRV(ops))) &&
	    (!(dnp->dn_flags & DN_DEVI_MADE))) {
		(void) impl_make_devinfos(major);
	}

	/*
	 * Now we try to attach the driver to hw devinfos (if any)
	 * and check to make sure it was attached somewhere.
	 * If it wasn't attached anywhere, then we can toss it.
	 * We make sure we only go through this code once, on behalf
	 * of the first call into this function.
	 */
	if ((!circular) && (!(dnp->dn_flags & DN_WALKED_TREE))) {
		attach_driver_to_hw_nodes(major, ops);
		LOCK_DEV_OPS(&(dnp->dn_lock));
		dnp->dn_flags |= DN_WALKED_TREE;
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		(void) i_ndi_attach_new_devinfo(major);
		if (!(dnp->dn_flags & DN_DEVS_ATTACHED)) {
			ddi_rele_driver(major);
			ops = NULL;
			LOCK_DEV_OPS(&(dnp->dn_lock));
			dnp->dn_flags = DN_BUSY_UNLOADING;
			UNLOCK_DEV_OPS(&(dnp->dn_lock));
			(void) mod_remove_by_name(ddi_major_to_name(major));
		}
	}

	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_exit_driver_list(dnp, circular);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	return (ops);
}

void
ddi_rele_driver(major_t maj)
{
	mod_rele_dev_by_major(maj);
}

struct dev_ops *
ddi_hold_devi(dev_info_t *devi)
{
	return (mod_hold_dev_by_devi(devi));
}

void
ddi_rele_devi(dev_info_t *devi)
{
	mod_rele_dev_by_devi(devi);
}

/*
 * ddi_install_driver(name)
 *
 * Driver installation is currently a byproduct of driver loading.  This
 * may change.
 */

int
ddi_install_driver(char *name)
{
	major_t major;

	if (((major = ddi_name_to_major(name)) == -1) ||
	    (ddi_hold_installed_driver(major) == NULL))
		return (DDI_FAILURE);
	ddi_rele_driver(major);
	return (DDI_SUCCESS);
}

/*
 * ddi_root_node:		Return root node of devinfo tree
 */

dev_info_t *
ddi_root_node(void)
{
	return (top_devinfo);
}

/*
 * ddi_find_devinfo- these routines look for a specifically named device
 */

static dev_info_t *
ddi_find_devinfo_search_for_major(dev_info_t *dip, int major, int unit)
{
	dev_info_t *rdip;

	while (dip) {
		if (major == ddi_name_to_major(ddi_binding_name(dip)) &&
		    (unit == -1 || DEVI(dip)->devi_instance == unit)) {
			return (dip);
		}
		if (DEVI(dip)->devi_child) {
			rdip = ddi_find_devinfo_search_for_major((dev_info_t *)
			    DEVI(dip)->devi_child, major, unit);
			if (rdip) {
				return (rdip);
			}
		}
		dip = (dev_info_t *)DEVI(dip)->devi_sibling;
	}
	return (dip);
}

dev_info_t *
ddi_find_devinfo_bymajor(int major, int unit)
{
	dev_info_t *dip = ddi_root_node();

	if (dip != NULL) {
		rw_enter(&(devinfo_tree_lock), RW_READER);
		dip = ddi_find_devinfo_search_for_major(dip, major, unit);
		rw_exit(&(devinfo_tree_lock));
	}
	return (dip);
}

/*
 * ddi_find_devinfo- these routines look for a specifically named device
 */

static dev_info_t *
ddi_find_devinfo_search(dev_info_t *dip, char *name, int unit, int need_drv)
{
	dev_info_t *rdip;

	while (dip) {
		if ((DEVI(dip)->devi_node_name &&
		    strcmp(DEVI(dip)->devi_node_name, name) == 0) &&
		    (unit == -1 || DEVI(dip)->devi_instance == unit) &&
		    (need_drv == 0 || DEVI(dip)->devi_ops)) {
			return (dip);
		}
		if (DEVI(dip)->devi_child) {
			rdip = ddi_find_devinfo_search((dev_info_t *)
			    DEVI(dip)->devi_child, name, unit, need_drv);
			if (rdip) {
				return (rdip);
			}
		}
		dip = (dev_info_t *)DEVI(dip)->devi_sibling;
	}
	return (dip);
}

dev_info_t *
ddi_find_devinfo(char *name, int unit, int need_drv)
{
	dev_info_t *dip = ddi_root_node();

	if (dip != NULL) {
		rw_enter(&(devinfo_tree_lock), RW_READER);
		dip = ddi_find_devinfo_search(dip, name, unit, need_drv);
		rw_exit(&(devinfo_tree_lock));
	}
	return (dip);
}

/*
 * Miscellaneous functions:
 */

/*
 * Implementation specific hooks
 */

void
ddi_report_dev(dev_info_t *d)
{
	char *b;

	(void) ddi_ctlops(d, d, DDI_CTLOPS_REPORTDEV, (void *)0, (void *)0);

	/*
	 * If this devinfo node has cb_ops, it's implicitly accessible from
	 * userland, so we print its full name together with the instance
	 * number 'abbreviation' that the driver may use internally.
	 */
	if (DEVI(d)->devi_ops->devo_cb_ops != (struct cb_ops *)0 &&
	    (b = kmem_zalloc(MAXPATHLEN, KM_NOSLEEP))) {
		cmn_err(CE_CONT, "?%s%d is %s\n",
		    ddi_driver_name(d), ddi_get_instance(d),
		    ddi_pathname(d, b));
		kmem_free(b, MAXPATHLEN);
	}
}

int
ddi_dev_regsize(dev_info_t *dev, uint_t rnumber, off_t *result)
{
	return (ddi_ctlops(dev, dev, DDI_CTLOPS_REGSIZE,
	    (void *)&rnumber, (void *)result));
}

int
ddi_dev_nregs(dev_info_t *dev, int *result)
{
	return (ddi_ctlops(dev, dev, DDI_CTLOPS_NREGS, 0, (void *)result));
}

int
ddi_dev_nintrs(dev_info_t *dev, int *result)
{
	return (i_ddi_dev_nintrs(dev, result));
}

int
ddi_dev_is_sid(dev_info_t *d)
{
	return (ddi_ctlops(d, d, DDI_CTLOPS_SIDDEV, (void *)0, (void *)0));
}

int
ddi_slaveonly(dev_info_t *d)
{
	return (ddi_ctlops(d, d, DDI_CTLOPS_SLAVEONLY, (void *)0, (void *)0));
}

int
ddi_dev_affinity(dev_info_t *a, dev_info_t *b)
{
	return (ddi_ctlops(a, a, DDI_CTLOPS_AFFINITY, (void *)b, (void *)0));
}

int
ddi_streams_driver(dev_info_t *dip)
{
	if (!DDI_CF2(dip))
		return (DDI_FAILURE);
	if (DEVI(dip)->devi_ops->devo_cb_ops == NULL)
		return (DDI_FAILURE);
	if (DEVI(dip)->devi_ops->devo_cb_ops->cb_str != NULL)
		return (DDI_SUCCESS);
	return (DDI_FAILURE);
}

/*
 * callback free list
 */

static int ncallbacks;
static int nc_low = 170;
static int nc_med = 512;
static int nc_high = 2048;
static struct ddi_callback *callbackq;
static struct ddi_callback *callbackqfree;

/*
 * set/run callback lists
 */

struct {
	uint_t nc_asked;
	uint_t nc_new;
	uint_t nc_run;
	uint_t nc_delete;
	uint_t nc_maxreq;
	uint_t nc_maxlist;
	uint_t nc_alloc;
	uint_t nc_runouts;
	uint_t nc_L2;
} cbstats;

static kmutex_t ddi_callback_mutex;
static struct kmem_cache *ddi_callback_cache;

/*
 * callbacks are handled using a L1/L2 cache. The L1 cache
 * comes out of kmem_cache_alloc and can expand/shrink dynamically. If
 * we can't get callbacks from the L1 cache [because pageout is doing
 * I/O at the time freemem is 0], we allocate callbacks out of the
 * L2 cache. The L2 cache is static and depends on the memory size.
 * [We might also count the number of devices at probe time and
 * allocate one structure per device and adjust for deferred attach]
 */
void
impl_ddi_callback_init(void)
{
	int i;
	uint_t physmegs;

	ddi_callback_cache = kmem_cache_create("ddi_callback_cache",
		sizeof (struct ddi_callback), 0, NULL, NULL, NULL,
		NULL, NULL, 0);

	physmegs = physmem >> (20 - PAGESHIFT);
	if (physmegs < 48) {
		ncallbacks = nc_low;
	} else if (physmegs < 128) {
		ncallbacks = nc_med;
	} else {
		ncallbacks = nc_high;
	}

	/*
	 * init free list
	 */
	callbackq = kmem_zalloc(
	    ncallbacks * sizeof (struct ddi_callback), KM_SLEEP);
	for (i = 0; i < ncallbacks-1; i++)
		callbackq[i].c_nfree = &callbackq[i+1];
	callbackqfree = callbackq;
}

static void
callback_insert(int (*funcp)(caddr_t), caddr_t arg, uintptr_t *listid,
	int count)
{
	struct ddi_callback *list, *marker, *new;

	list = marker = (struct ddi_callback *)*listid;
	while (list != NULL) {
		if (list->c_call == funcp && list->c_arg == arg) {
			list->c_count += count;
			return;
		}
		marker = list;
		list = list->c_nlist;
	}
	new = kmem_cache_alloc(ddi_callback_cache, KM_NOSLEEP);
	if (new == NULL) {
		new = callbackqfree;
		if (new == NULL)
			cmn_err(CE_PANIC,
				"callback_insert: no callback structures");
		callbackqfree = new->c_nfree;
		cbstats.nc_L2++;
	}
	if (marker != NULL) {
		marker->c_nlist = new;
	} else {
		*listid = (uintptr_t)new;
	}
	new->c_nlist = NULL;
	new->c_call = funcp;
	new->c_arg = arg;
	new->c_count = count;
	cbstats.nc_new++;
	cbstats.nc_alloc++;
	if (cbstats.nc_alloc > cbstats.nc_maxlist)
		cbstats.nc_maxlist = cbstats.nc_alloc;
}

void
ddi_set_callback(int (*funcp)(caddr_t), caddr_t arg, uintptr_t *listid)
{
	mutex_enter(&ddi_callback_mutex);
	cbstats.nc_asked++;
	if ((cbstats.nc_asked - cbstats.nc_run) > cbstats.nc_maxreq)
		cbstats.nc_maxreq = (cbstats.nc_asked - cbstats.nc_run);
	(void) callback_insert(funcp, arg, listid, 1);
	mutex_exit(&ddi_callback_mutex);
}

static void
real_callback_run(void *Queue)
{
	int (*funcp)(caddr_t);
	caddr_t arg;
	int count, rval;
	uintptr_t *listid;
	struct ddi_callback *list, *marker;
	int check_pending = 1;
	int pending = 0;

	do {
		mutex_enter(&ddi_callback_mutex);
		listid = Queue;
		list = (struct ddi_callback *)*listid;
		if (list == NULL) {
			mutex_exit(&ddi_callback_mutex);
			return;
		}
		if (check_pending) {
			marker = list;
			while (marker != NULL) {
				pending += marker->c_count;
				marker = marker->c_nlist;
			}
			check_pending = 0;
		}
		ASSERT(pending > 0);
		ASSERT(list->c_count > 0);
		funcp = list->c_call;
		arg = list->c_arg;
		count = list->c_count;
		*(uintptr_t *)Queue = (uintptr_t)list->c_nlist;
		if (list >= &callbackq[0] &&
		    list <= &callbackq[ncallbacks-1]) {
			list->c_nfree = callbackqfree;
			callbackqfree = list;
		} else
			kmem_cache_free(ddi_callback_cache, list);

		cbstats.nc_delete++;
		cbstats.nc_alloc--;
		mutex_exit(&ddi_callback_mutex);

		do {
			if ((rval = (*funcp)(arg)) == 0) {
				pending -= count;
				mutex_enter(&ddi_callback_mutex);
				(void) callback_insert(funcp, arg, listid,
					count);
				cbstats.nc_runouts++;
			} else {
				pending--;
				mutex_enter(&ddi_callback_mutex);
				cbstats.nc_run++;
			}
			mutex_exit(&ddi_callback_mutex);
		} while (rval != 0 && (--count > 0));
	} while (pending > 0);
}

void
ddi_run_callback(uintptr_t *listid)
{
	softcall(real_callback_run, listid);
}

/*ARGSUSED*/
dev_info_t *
nodevinfo(dev_t dev, int otyp)
{
	return ((dev_info_t *)0);
}

/*ARGSUSED*/
int
ddi_no_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
ddifail(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
ddi_no_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareqp, ddi_dma_handle_t *handlep)
{
	return (DDI_DMA_NOMAPPING);
}

/*ARGSUSED*/
int
ddi_no_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	return (DDI_DMA_BADATTR);
}

/*ARGSUSED*/
int
ddi_no_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
ddi_no_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cp, uint_t *ccountp)
{
	return (DDI_DMA_NOMAPPING);
}

/*ARGSUSED*/
int
ddi_no_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
ddi_no_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, size_t len,
    uint_t cache_flags)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
ddi_no_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	return (DDI_FAILURE);
}


/*ARGSUSED*/
int
ddi_no_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp, caddr_t *objp, uint_t flags)
{
	return (DDI_FAILURE);
}

void
ddivoid(void)
{}

/*ARGSUSED*/
int
nochpoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **pollhdrp)
{
	return (ENXIO);
}

cred_t *
ddi_get_cred(void)
{
	return (CRED());
}

clock_t
ddi_get_lbolt(void)
{
	return (lbolt);
}

time_t
ddi_get_time(void)
{
	if (hrestime.tv_sec == 0) {
		timestruc_t ts;
		mutex_enter(&tod_lock);
		ts = tod_get();
		mutex_exit(&tod_lock);
		return (ts.tv_sec);
	} else {
		return (hrestime.tv_sec);
	}
}

pid_t
ddi_get_pid(void)
{
	return (ttoproc(curthread)->p_pid);
}

/*
 * Swap bytes in 16-bit [half-]words
 */
void
swab(void *src, void *dst, size_t nbytes)
{
	uchar_t *pf = (uchar_t *)src;
	uchar_t *pt = (uchar_t *)dst;
	uchar_t tmp;
	int nshorts;

	nshorts = nbytes >> 1;

	while (--nshorts >= 0) {
		tmp = *pf++;
		*pt++ = *pf++;
		*pt++ = tmp;
	}
}

static void
ddi_append_minor_node(dev_info_t *ddip, struct ddi_minor_data *dmdp)
{
	struct ddi_minor_data *dp;

	mutex_enter(&(DEVI(ddip)->devi_lock));
	i_devi_enter(ddip, DEVI_S_MD_UPDATE, DEVI_S_MD_UPDATE, 1);

	if ((dp = DEVI(ddip)->devi_minor) == (struct ddi_minor_data *)NULL) {
		DEVI(ddip)->devi_minor = dmdp;
	} else {
		while (dp->next != (struct ddi_minor_data *)NULL)
			dp = dp->next;
		dp->next = dmdp;
	}

	i_devi_exit(ddip, DEVI_S_MD_UPDATE, 1);
	mutex_exit(&(DEVI(ddip)->devi_lock));
}

minor_t
ddi_getiminor(dev_t dev)
{
	minor_t	lminor;
	dev_type_t class_type;

	if (DC_RESOLVE_MINOR(&dcops, getmajor(dev), getminor(dev), &lminor,
	    &class_type) != 0) {
		return ((minor_t)NODEV);
	} else {
		return (lminor);
	}
}

static int
i_log_devfs_minor_create(dev_info_t *dip, char *minor_name)
{
	log_event_tuple_t	tuples[5];
	int			argc = 0;
	char *pathname;
	int rv;

	pathname = kmem_alloc(MAXPATHLEN, KM_NOSLEEP);
	if (pathname == NULL) {
		return (DDI_FAILURE);
	}

	tuples[argc].attr = LOGEVENT_CLASS;
	tuples[argc++].val = EC_DEVFS;
	tuples[argc].attr = LOGEVENT_TYPE;
	tuples[argc++].val = ET_DEVFS_MINOR_CREATE;
	tuples[argc].attr = DEVFS_PATHNAME;
	(void) ddi_pathname(dip, pathname);
	ASSERT(strlen(pathname));
	tuples[argc++].val = pathname;

	/*
	 * allow for NULL minor names
	 */
	if (minor_name != NULL) {
		tuples[argc].attr = DEVFS_MINOR_NAME;
		tuples[argc++].val = minor_name;
	}

	if ((rv = i_ddi_log_event(argc, tuples, KM_SLEEP)) != DDI_SUCCESS)
		cmn_err(CE_WARN, "devfs_log_minor_create: failed log_event");

	kmem_free(pathname, MAXPATHLEN);
	return (rv);
}

static int
i_log_devfs_minor_remove(dev_info_t *dip, char *minor_name)
{

	log_event_tuple_t	tuples[5];
	int			argc = 0;
	char *pathname;
	int rv;

	/*
	 * only log ddi_remove_minor_node() calls outside the scope
	 * of attach/detach reconfigurations and when the dip is
	 * still in CF1
	 */
	if (DEVI_IS_ATTACHING(dip) || DEVI_IS_DETACHING(dip) ||
	    !(DDI_CF1(dip))) {
		return (DDI_SUCCESS);
	}

	pathname = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	tuples[argc].attr = LOGEVENT_CLASS;
	tuples[argc++].val = EC_DEVFS;
	tuples[argc].attr = LOGEVENT_TYPE;
	tuples[argc++].val = ET_DEVFS_MINOR_REMOVE;
	(void) ddi_pathname(dip, pathname);
	ASSERT(strlen(pathname));
	tuples[argc].attr = DEVFS_PATHNAME;
	tuples[argc++].val = pathname;
	/*
	 * allow for NULL minor names
	 */
	if (minor_name != NULL) {
		tuples[argc].attr = DEVFS_MINOR_NAME;
		tuples[argc++].val = minor_name;
	}

	if ((rv = i_ddi_log_event(argc, tuples, KM_SLEEP)) != DDI_SUCCESS)
		cmn_err(CE_WARN, "devfs_log_minor_remove: failed log_event");

	kmem_free(pathname, MAXPATHLEN);
	return (rv);
}

/*
 * ddi_create_minor_common:	Create a  ddi_minor_data structure and
 *				attach it to the given devinfo node.
 */

int
ddi_create_minor_common(dev_info_t *dip, char *name, int spec_type,
    minor_t minor_num, char *node_type, int flag, ddi_minor_type mtype)
{
	static char warning[] = "Cannot create minor node for <%s> <%s> <%d>";
	struct ddi_minor_data *dmdp, *dmdap;
	major_t major;
	minor_t	eminor;
	dev_info_t *ddip;
	int class;
	static dev_info_t *clone_dip;
	static major_t clone_major;
	extern int is_pseudo_device(dev_info_t *);

	if (spec_type != S_IFCHR && spec_type != S_IFBLK)
		return (DDI_FAILURE);

	if (name == NULL)
		return (DDI_FAILURE);

	/*
	 * Ensure that the minor number the driver is creating
	 * will be expressible on the on-disk filesystem (currently
	 * this is limited to 18 bits both by UFS, and by a strong
	 * desire not to confuse existing 32-bit applications)
	 *
	 * At some point, i.e. when we've been using a new on-disk
	 * filesystem format for a few years, this code should be
	 * removed, but for now it remains here to try to avoid
	 * compatibility problems.
	 */
	if (minor_num > L_MAXMIN32) {
		cmn_err(CE_WARN,
		    "%s%d:%s minor 0x%x too big for 32-bit applications",
		    ddi_get_name(dip), ddi_get_instance(dip), name, minor_num);
		return (DDI_FAILURE);
	}

	/*
	 * We don't expect the driver to know it's major number.
	 * So we look through the table of major to name mappings.
	 */
	if ((major = ddi_name_to_major(DEVI(dip)->devi_binding_name)) == -1)  {
		cmn_err(CE_WARN, warning, ddi_binding_name(dip),
		    name, minor_num);
		cmn_err(CE_CONT, "Can't find major dev number for <%s>\n",
		    ddi_binding_name(dip));
		return (DDI_FAILURE);	/* Not found so error */
	}

	/*
	 * Take care of class information for the node.
	 */


	switch (CLASS_DEV(flag)) {
	case GLOBAL_DEV:		/* global device (/dev/le /dev/ip) */
	case NODEBOUND_DEV:		/* bound to one node  (/dev/tiocts) */
	case NODESPECIFIC_DEV:		/* specific to node only (/dev/kmem) */
	case ENUMERATED_DEV:		/* each node distinguished (c0t0d0s0) */
		class = CLASS_DEV(flag);
		break;

	default:
		/*
		 * Since most drivers will not be modified to declare the
		 * type of the minor node created, we will use heuristics
		 * to figures out what the device node's likely behavior
		 * is going to be.
		 * Most non-pseudo devices are enumerated, in other words,
		 * they tend to be device nodes correspending to a
		 * physical piece of hardware, or define an alternate
		 * behavior for the said hardware.  For instance /dev/rmt/0
		 * vs. /dev/rmt/0l.  The pseudo devices on the other
		 * hand tend to be global devices, although this can be
		 * stated not quite as strongly as the previous statement.
		 * The default behvior which matches the most prevelant
		 * mode for such a device is global, i.e. much like le
		 */

		if (is_pseudo_device(dip) || (flag & CLONE_DEV)) {
			class = GLOBAL_DEV;
		} else {
			class = ENUMERATED_DEV;
		}
	}

	/*
	 * Take care of minor number information for the node.
	 */

	/*
	 * If the minor number includes the instance number we will
	 * have a unique minor number. This effectively
	 * saves us from having to ask for a unique minor number
	 * from the DC server. But we still have to inform the DC
	 * server that we in fact intend to use this minor number
	 * so that the right magic can happen for this device to get
	 * opened here no matter where the process that intends to open
	 * it is located.
	 *
	 * Having said that, we don't much care about the global
	 * minor number. This minor number is the business of physical
	 * filesystem node, the DCS and specfs in deciding on which node
	 * and using what "internal" minor number this node belongs
	 * to, therefore we leave the minor node unassigned until
	 * it is truely needed. The global minor numbers are needed
	 * for make_node() calls.
	 */

	/*
	 * It is possible for cb_ops not to have been initialized
	 * by the time we get here.
	 */

	eminor = EMINOR_UNKNOWN;

	/*
	 * If the driver is making a clone minor device then we find the
	 * clone driver, cache it's devinfo node in clone_dip and use that
	 * as the devinfo node for the minor device.  The major we found
	 * above becomes the minor and we find the major number of the clone
	 * device.
	 */
	if (flag & CLONE_DEV) {
		if (clone_major == NULL) {
			clone_major = ddi_name_to_major("clone");
			if (clone_major == (dev_t)-1l)	{
				cmn_err(CE_WARN, warning,
				    ddi_binding_name(dip), name, minor_num);
				cmn_err(CE_CONT,
				    "Can't find clone major dev number\n");
				return (DDI_FAILURE);
			}
		}
		if ((ddip = clone_dip) == NULL) {
			(void) ddi_hold_installed_driver(clone_major);
			ddip = ddi_find_devinfo("clone", -1, 1);
			if (ddip == NULL)  {
				cmn_err(CE_WARN, warning,
				    ddi_binding_name(dip), name, minor_num);
				cmn_err(CE_CONT,
				    "Can't find clone devinfo node\n");
				return (DDI_FAILURE);
			}
			clone_dip = ddip;
		} else
			(void) ddi_hold_installed_driver(clone_major);
		minor_num = major;
		major = clone_major;
	} else {
		ddip = dip;
	}
	if ((dmdp = kmem_zalloc(sizeof (struct ddi_minor_data),
	    KM_NOSLEEP)) == NULL) {
		if (flag & CLONE_DEV)
			ddi_rele_driver(clone_major);
		return (DDI_FAILURE);
	}
	if ((dmdp->ddm_name = kmem_zalloc(strlen(name) + 1,
	    KM_NOSLEEP)) == NULL) {
		kmem_free(dmdp, sizeof (struct ddi_minor_data));
		if (flag & CLONE_DEV)
			ddi_rele_driver(clone_major);
		return (DDI_FAILURE);
	}
	bcopy(name, dmdp->ddm_name, strlen(name));
	dmdp->dip = ddip;
	dmdp->ddm_dev = makedevice(major, minor_num);
	dmdp->ddm_eminor = eminor;
	dmdp->ddm_class = class;
	dmdp->ddm_spec_type = spec_type;
	dmdp->ddm_node_type = node_type;
	dmdp->type = mtype;
	ddi_append_minor_node(ddip, dmdp);
	if (flag & CLONE_DEV) {
		dmdap = kmem_zalloc(sizeof (struct ddi_minor_data), KM_NOSLEEP);
		if (dmdap == NULL) {
			ddi_remove_minor_node(ddip, name);
			ddi_rele_driver(clone_major);
			return (DDI_FAILURE);
		}
		dmdap->type = DDM_ALIAS;
		dmdap->ddm_admp = dmdp;
		dmdap->dip = dip;
		ddi_append_minor_node(dip, dmdap);

	}
	/*
	 * If the device class is NODESPECIFIC we will need to
	 * names created in the name space. For example /dev/kmem
	 * and /dev/kmem$nodeid. This code calls ddi_create_minor_node()
	 * to create the enumerated version of the node (the one
	 * with the nodeid.)
	 */
	if (class == DEV_NODESPECIFIC) {
		flag &= ~DEVCLASS_MASK;
		flag |= DEV_ENUMERATED;

		return (ddi_create_minor_common(dip, name, spec_type,
		    minor_num, node_type, flag, mtype));
	}

	/*
	 * only log ddi_create_minor_node() calls which occur
	 * outside the scope of attach(9e)/detach(9e) reconfigurations
	 */
	if ((!(DEVI_IS_ATTACHING(dip) || DEVI_IS_DETACHING(dip))) &&
	    (i_log_devfs_minor_create(ddip, name) != DDI_SUCCESS)) {
		ddi_remove_minor_node(ddip, name);
		if (flag & CLONE_DEV) {
			ddi_remove_minor_node(dip, name);
			ddi_rele_driver(clone_major);
		}
		return (DDI_FAILURE);
	}

	/*
	 * Check if any dacf rules match the creation of this minor node
	 */
	dacfc_match_create_minor(name, node_type, ddip, dmdp, flag);
	return (DDI_SUCCESS);
}

int
ddi_create_minor_node(dev_info_t *dip, char *name, int spec_type,
    minor_t minor_num, char *node_type, int flag)
{
	return (ddi_create_minor_common(dip, name, spec_type, minor_num,
	    node_type, flag, DDM_MINOR));
}

int
ddi_create_default_minor_node(dev_info_t *dip, char *name, int spec_type,
    minor_t minor_num, char *node_type, int flag)
{
	return (ddi_create_minor_common(dip, name, spec_type, minor_num,
	    node_type, flag, DDM_DEFAULT));
}

/*
 * Internal (non-ddi) routine for drivers to export names known
 * to the kernel (especially ddi_pathname_to_dev_t and friends)
 * but not exported externally to /devices
 */
int
ddi_create_internal_pathname(dev_info_t *dip, char *name, int spec_type,
    minor_t minor_num)
{
	return (ddi_create_minor_common(dip, name, spec_type, minor_num,
	    "internal", 0, DDM_INTERNAL_PATH));
}

void
ddi_remove_minor_alias_node(dev_info_t *dip, struct ddi_minor_data *dmdc)
{
	struct ddi_minor_data *dmdp, *dmdp1;
	struct ddi_minor_data **dmdp_prev;

	mutex_enter(&(DEVI(dip)->devi_lock));
	dmdp_prev = &DEVI(dip)->devi_minor;
	dmdp = DEVI(dip)->devi_minor;
	while (dmdp != NULL) {
		dmdp1 = dmdp->next;
		if (dmdp == dmdc) {
			if (dmdp->ddm_name != NULL)
				kmem_free(dmdp->ddm_name,
				    strlen(dmdp->ddm_name) + 1);

			/*
			 * Release dacf client data associated with this minor
			 * node by storing NULL.
			 */
			dacf_store_info((dacf_infohdl_t)dmdp, NULL);
			kmem_free(dmdp, sizeof (struct ddi_minor_data));
			*dmdp_prev = dmdp1;
			break;
		} else {
			dmdp_prev = &dmdp->next;
		}
		dmdp = dmdp1;
	}
	mutex_exit(&(DEVI(dip)->devi_lock));
}

void
ddi_remove_minor_node(dev_info_t *dip, char *name)
{
	struct ddi_minor_data *dmdp, *dmdp1;
	struct ddi_minor_data **dmdp_prev;
	major_t major;

	mutex_enter(&(DEVI(dip)->devi_lock));
	i_devi_enter(dip, DEVI_S_MD_UPDATE, DEVI_S_MD_UPDATE, 1);

	dmdp_prev = &DEVI(dip)->devi_minor;
	dmdp = DEVI(dip)->devi_minor;
	while (dmdp != NULL) {
		dmdp1 = dmdp->next;
		if (((dmdp->type == DDM_MINOR) || (dmdp->type == DDM_DEFAULT) ||
		    (dmdp->type == DDM_INTERNAL_PATH)) &&
		    (name == NULL || (dmdp->ddm_name != NULL &&
		    strcmp(name, dmdp->ddm_name) == 0))) {
			if (dmdp->ddm_name != NULL) {
				(void) i_log_devfs_minor_remove(dip,
				    dmdp->ddm_name);
				kmem_free(dmdp->ddm_name,
				    strlen(dmdp->ddm_name) + 1);
			}
			/*
			 * Release dacf client data associated with this minor
			 * node by storing NULL.
			 */
			dacf_store_info((dacf_infohdl_t)dmdp, NULL);
			kmem_free(dmdp, sizeof (struct ddi_minor_data));
			*dmdp_prev = dmdp1;
			/*
			 * OK, we found it, so get out now -- if we drive on,
			 * we will strcmp against garbage.  See 1139209.
			 */
			if (name != NULL)
				break;
		} else if (dmdp->type == DDM_ALIAS &&
		    (name == NULL || (((dmdp->ddm_atype == DDM_MINOR) ||
				(dmdp->ddm_atype == DDM_INTERNAL_PATH) ||
				(dmdp->ddm_atype == DDM_DEFAULT)) &&
				(dmdp->ddm_aname != NULL &&
				strcmp(name, dmdp->ddm_aname) == 0)))) {
			major = getmajor(dmdp->ddm_adev);
			(void) i_log_devfs_minor_remove(dip, dmdp->ddm_aname);
			ddi_remove_minor_alias_node(dmdp->ddm_adip,
			    dmdp->mu.d_alias.dmp);
			ddi_rele_driver(major);
			kmem_free(dmdp, sizeof (struct ddi_minor_data));
			*dmdp_prev = dmdp1;
		} else {
			dmdp_prev = &dmdp->next;
		}
		dmdp = dmdp1;
	}

	i_devi_exit(dip, DEVI_S_MD_UPDATE, 1);
	mutex_exit(&(DEVI(dip)->devi_lock));
}


int
ddi_in_panic()
{
	return (panicstr != NULL ? 1 : 0);
}


/*
 * Find first bit set in a mask (returned counting from 1 up)
 */

int
ddi_ffs(long mask)
{
	extern int ffs(long mask);
	return (ffs(mask));
}

/*
 * Find last bit set. Take mask and clear
 * all but the most significant bit, and
 * then let ffs do the rest of the work.
 *
 * Algorithm courtesy of Steve Chessin.
 */

int
ddi_fls(long mask)
{
	extern int ffs(long);

	while (mask) {
		long nx;

		if ((nx = (mask & (mask - 1))) == 0)
			break;
		mask = nx;
	}
	return (ffs(mask));
}

/*
 * The next five routines comprise generic storage management utilities
 * for driver soft state structures (in "the old days," this was done
 * with a statically sized array - big systems and dynamic loading
 * and unloading make heap allocation more attractive)
 */

/*
 * Allocate a set of pointers to 'n_items' objects of size 'size'
 * bytes.  Each pointer is initialized to nil.
 *
 * The 'size' and 'n_items' values are stashed in the opaque
 * handle returned to the caller.
 *
 * This implementation interprets 'set of pointers' to mean 'array
 * of pointers' but note that nothing in the interface definition
 * precludes an implementation that uses, for example, a linked list.
 * However there should be a small efficiency gain from using an array
 * at lookup time.
 *
 * XXX	As an optimization, we make our growable array allocations in
 *	powers of two (bytes), since that's how much kmem_alloc (currently)
 *	gives us anyway.  It should save us some free/realloc's ..
 *
 * XXX	As a further optimization, we make the growable array start out
 *	with MIN_N_ITEMS in it.
 */

#define	MIN_N_ITEMS	8	/* 8 void *'s == 32 bytes */

int
ddi_soft_state_init(void **state_p, size_t size, size_t n_items)
{
	struct i_ddi_soft_state *ss;

	if (state_p == NULL || *state_p != NULL || size == 0)
		return (EINVAL);

	ss = kmem_zalloc(sizeof (*ss), KM_SLEEP);
	mutex_init(&ss->lock, NULL, MUTEX_DRIVER, NULL);
	ss->size = size;

	if (n_items < MIN_N_ITEMS)
		ss->n_items = MIN_N_ITEMS;
	else {
		int bitlog;

		if ((bitlog = ddi_fls(n_items)) == ddi_ffs(n_items))
			bitlog--;
		ss->n_items = 1 << bitlog;
	}

	ASSERT(ss->n_items >= n_items);

	ss->array = kmem_zalloc(ss->n_items * sizeof (void *), KM_SLEEP);

	*state_p = ss;

	return (0);
}


/*
 * Allocate a state structure of size 'size' to be associated
 * with item 'item'.
 *
 * In this implementation, the array is extended to
 * allow the requested offset, if needed.
 */
int
ddi_soft_state_zalloc(void *state, int item)
{
	struct i_ddi_soft_state *ss;
	void **array;
	void *new_element;

	if ((ss = state) == NULL || item < 0)
		return (DDI_FAILURE);

	mutex_enter(&ss->lock);
	if (ss->size == 0) {
		mutex_exit(&ss->lock);
		cmn_err(CE_WARN, "ddi_soft_state_zalloc: bad handle");
		return (DDI_FAILURE);
	}

	array = ss->array;	/* NULL if ss->n_items == 0 */
	ASSERT(ss->n_items != 0 && array != NULL);

	/*
	 * refuse to tread on an existing element
	 */
	if (item < ss->n_items && array[item] != NULL) {
		mutex_exit(&ss->lock);
		return (DDI_FAILURE);
	}

	/*
	 * Allocate a new element to plug in
	 */
	new_element = kmem_zalloc(ss->size, KM_SLEEP);

	/*
	 * Check if the array is big enough, if not, grow it.
	 */
	if (item >= ss->n_items) {
		void	**new_array;
		size_t	new_n_items;
		struct i_ddi_soft_state *dirty;

		/*
		 * Allocate a new array of the right length, copy
		 * all the old pointers to the new array, then
		 * if it exists at all, put the old array on the
		 * dirty list.
		 *
		 * Note that we can't kmem_free() the old array.
		 *
		 * Why -- well the 'get' operation is 'mutex-free', so we
		 * can't easily catch a suspended thread that is just about
		 * to dereference the array we just grew out of.  So we
		 * cons up a header and put it on a list of 'dirty'
		 * pointer arrays.  (Dirty in the sense that there may
		 * be suspended threads somewhere that are in the middle
		 * of referencing them).  Fortunately, we -can- garbage
		 * collect it all at ddi_soft_state_fini time.
		 */
		new_n_items = ss->n_items;
		while (new_n_items < (1 + item))
			new_n_items <<= 1;	/* double array size .. */

		ASSERT(new_n_items >= (1 + item));	/* sanity check! */

		new_array = kmem_zalloc(new_n_items * sizeof (void *),
		    KM_SLEEP);
		/*
		 * Copy the pointers into the new array
		 */
		bcopy(array, new_array, ss->n_items * sizeof (void *));

		/*
		 * Save the old array on the dirty list
		 */
		dirty = kmem_zalloc(sizeof (*dirty), KM_SLEEP);
		dirty->array = ss->array;
		dirty->n_items = ss->n_items;
		dirty->next = ss->next;
		ss->next = dirty;

		ss->array = (array = new_array);
		ss->n_items = new_n_items;
	}

	ASSERT(array != NULL && item < ss->n_items && array[item] == NULL);

	array[item] = new_element;

	mutex_exit(&ss->lock);
	return (DDI_SUCCESS);
}


/*
 * Fetch a pointer to the allocated soft state structure.
 *
 * This is designed to be cheap.
 *
 * There's an argument that there should be more checking for
 * nil pointers and out of bounds on the array.. but we do a lot
 * of that in the alloc/free routines.
 *
 * An array has the convenience that we don't need to lock read-access
 * to it c.f. a linked list.  However our "expanding array" strategy
 * means that we should hold a readers lock on the i_ddi_soft_state
 * structure.
 *
 * However, from a performance viewpoint, we need to do it without
 * any locks at all -- this also makes it a leaf routine.  The algorithm
 * is 'lock-free' because we only discard the pointer arrays at
 * ddi_soft_state_fini() time.
 */
void *
ddi_get_soft_state(void *state, int item)
{
	struct i_ddi_soft_state *ss = state;

	ASSERT(ss != NULL && item >= 0);

	if (item < ss->n_items && ss->array != NULL)
		return (ss->array[item]);
	return (NULL);
}

/*
 * Free the state structure corresponding to 'item.'   Freeing an
 * element that has either gone or was never allocated is not
 * considered an error.  Note that we free the state structure, but
 * we don't shrink our pointer array, or discard 'dirty' arrays,
 * since even a few pointers don't really waste too much memory.
 *
 * Passing an item number that is out of bounds, or a null pointer will
 * provoke an error message.
 */
void
ddi_soft_state_free(void *state, int item)
{
	struct i_ddi_soft_state *ss;
	void **array;
	void *element;
	static char msg[] = "ddi_soft_state_free:";

	if ((ss = state) == NULL) {
		cmn_err(CE_WARN, "%s null handle", msg);
		return;
	}

	element = NULL;

	mutex_enter(&ss->lock);

	if ((array = ss->array) == NULL || ss->size == 0) {
		cmn_err(CE_WARN, "%s bad handle", msg);
	} else if (item < 0 || item >= ss->n_items) {
		cmn_err(CE_WARN, "%s item %d not in range [0..%lu]",
		    msg, item, ss->n_items - 1);
	} else if (array[item] != NULL) {
		element = array[item];
		array[item] = NULL;
	}

	mutex_exit(&ss->lock);

	if (element)
		kmem_free(element, ss->size);
}


/*
 * Free the entire set of pointers, and any
 * soft state structures contained therein.
 *
 * Note that we don't grab the ss->lock mutex, even though
 * we're inspecting the various fields of the data strucuture.
 *
 * There is an implicit assumption that this routine will
 * never run concurrently with any of the above on this
 * particular state structure i.e. by the time the driver
 * calls this routine, there should be no other threads
 * running in the driver.
 */
void
ddi_soft_state_fini(void **state_p)
{
	struct i_ddi_soft_state *ss, *dirty;
	int item;
	static char msg[] = "ddi_soft_state_fini:";

	if (state_p == NULL || (ss = *state_p) == NULL) {
		cmn_err(CE_WARN, "%s null handle", msg);
		return;
	}

	if (ss->size == 0) {
		cmn_err(CE_WARN, "%s bad handle", msg);
		return;
	}

	if (ss->n_items > 0) {
		for (item = 0; item < ss->n_items; item++)
			ddi_soft_state_free(ss, item);
		kmem_free(ss->array, ss->n_items * sizeof (void *));
	}

	/*
	 * Now delete any dirty arrays from previous 'grow' operations
	 */
	for (dirty = ss->next; dirty; dirty = ss->next) {
		ss->next = dirty->next;
		kmem_free(dirty->array, dirty->n_items * sizeof (void *));
		kmem_free(dirty, sizeof (*dirty));
	}

	mutex_destroy(&ss->lock);
	kmem_free(ss, sizeof (*ss));

	*state_p = NULL;
}


/*
 * This sets the devi_addr entry in the dev_info structure 'dip' to 'name'
 * If name is NULL, this frees the devi_addr entry, if any.
 */
void
ddi_set_name_addr(dev_info_t *dip, char *name)
{
	char *p = NULL;
	char *oldname = DEVI(dip)->devi_addr;
	char *last_name = DEVI(dip)->devi_last_addr;

	if (last_name != NULL) {
		kmem_free(last_name, strlen(last_name) + 1);
	}

	if (name != NULL)  {
		p = kmem_alloc(strlen(name) + 1, KM_SLEEP);
		(void) strcpy(p, name);
		if (oldname) {
			kmem_free(oldname, strlen(oldname) + 1);
		}
		DEVI(dip)->devi_last_addr = NULL;
	} else {
		DEVI(dip)->devi_last_addr = oldname;
	}

	DEVI(dip)->devi_addr = p;
}

char *
ddi_get_name_addr(dev_info_t *dip)
{
	return (DEVI(dip)->devi_addr);
}

void
ddi_set_parent_data(dev_info_t *dip, caddr_t pd)
{
	DEVI(dip)->devi_parent_data = pd;
}

caddr_t
ddi_get_parent_data(dev_info_t *dip)
{
	return (DEVI(dip)->devi_parent_data);
}

/*
 * ddi_initchild:	Transform the prototype dev_info node into a
 *			canonical form 1 dev_info node.
 */
int
ddi_initchild(dev_info_t *parent, dev_info_t *proto)
{
	dev_info_t *cdip, *ndip;
	char *p_deviaddr, *p_nodename;
	char *m_deviaddr;
	int rv;
	major_t major;
	int p_isconfnode, m_isconfnode;

	/*
	 * return if node is initialized or the platform-specific INITCHILD
	 * fails
	 */
	if (DDI_CF1(proto))
		return (DDI_SUCCESS);

	if ((rv = i_ddi_initchild(parent, proto)) != DDI_SUCCESS)
		return (rv);


	/*
	 * remember if this node is from a .conf file
	 * cache the node name and node address for duplicate matching
	 */
	p_isconfnode = (ndi_dev_is_persistent_node(proto) == 0);
	p_nodename = ddi_node_name(proto);
	p_deviaddr = ddi_get_name_addr(proto);

	major = ddi_name_to_major(ddi_binding_name(proto));
	if (major == (major_t)-1)
		return (DDI_FAILURE);
	ASSERT(devnamesp[major].dn_busy_thread == curthread);


	/*
	 * reject duplicate device nodes
	 * duplicate device nodes have the same parent node and
	 * matching "node_name@devi_addr" and not a .conf merge node
	 */
	for (cdip = devnamesp[major].dn_head; cdip != NULL; cdip = ndip)  {
		ndip = ddi_get_next(cdip);

		if ((ddi_get_parent(cdip) != parent) || (cdip == proto))
			continue;

		/* parent matches, compare node_name and device address */
		if ((((m_deviaddr = ddi_get_name_addr(cdip)) != NULL) &&
		    (strcmp(p_deviaddr, m_deviaddr) == 0)) &&
		    (strcmp(p_nodename, ddi_node_name(cdip)) == 0)) {
			char *pn;

			m_isconfnode = (ndi_dev_is_persistent_node(cdip) == 0);

			/* allow merge nodes (.conf + HW) to pass */
			if ((p_isconfnode && !m_isconfnode) ||
			    (m_isconfnode && !p_isconfnode)) {
				return (DDI_SUCCESS);
			}

			/* log duplicate node message */
			pn = kmem_alloc(MAXPATHLEN, KM_SLEEP);
			(void) ddi_pathname(proto, pn);
			cmn_err(CE_CONT, "?duplicate device node %s\n", pn);
			kmem_free(pn, MAXPATHLEN);

			/*
			 * extra hold on parent for UNINITCHILD
			 * unitialize the duplicate node, returning
			 * DDI_FAILURE to prevent the framework from
			 * attempting to attach this node
			 */
			(void) ddi_hold_devi(parent);
			(void) ddi_uninitchild(proto);
			return (DDI_FAILURE);
		}
	}
	return (DDI_SUCCESS);
}

/*
 * ddi_uninitchild:	Transform dev_info node back into a
 *			prototype form dev_info node.
 */

int
ddi_uninitchild(dev_info_t *dip)
{
	dev_info_t *pdip;
	struct dev_ops *ops;
	int (*f)();
	int error;

	/*
	 * If it's already a prototype node, we're done.
	 */
	if (!DDI_CF1(dip))
		return (DDI_SUCCESS);

	pdip = ddi_get_parent(dip);
	if ((pdip == NULL) || ((ops = ddi_get_driver(pdip)) == NULL) ||
	    (ops->devo_bus_ops == NULL) ||
	    ((f = ops->devo_bus_ops->bus_ctl) == NULL))
		return (DDI_FAILURE);

	error = (*f)(pdip, pdip, DDI_CTLOPS_UNINITCHILD, dip, (void *)NULL);
	ASSERT(error == DDI_SUCCESS);

	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);
	impl_rem_dev_props(dip);

	ddi_rele_devi(pdip);
	return (error);
}

/*
 * ddi_name_to_major: Returns the major number of a module given its name.
 */
major_t
ddi_name_to_major(char *name)
{
	return (mod_name_to_major(name));
}

/*
 * ddi_major_to_name: Returns the module name bound to a major number.
 */
char *
ddi_major_to_name(major_t major)
{
	return (mod_major_to_name(major));
}

/*
 * Return the name of the devinfo node pointed at by 'dip' in the buffer
 * pointed at by 'name.'  A devinfo node is named as a result of calling
 * ddi_initchild().
 *
 * Note: the driver must be held before calling this function!
 */
char *
ddi_deviname(dev_info_t *dip, char *name)
{
	char *addrname;
	char none = '\0';

	if (dip == ddi_root_node()) {
		*name = '\0';
		return (name);
	}


	if (!DDI_CF1(dip)) {
		addrname = DEVI(dip)->devi_last_addr;
		if (addrname == NULL) {
			addrname = &none;
		}
	} else {
		addrname = ddi_get_name_addr(dip);
	}

	if (*addrname == '\0') {
		(void) sprintf(name, "/%s", ddi_node_name(dip));
	} else {
		(void) sprintf(name, "/%s@%s", ddi_node_name(dip), addrname);
	}

	return (name);
}

/*
 * Spits out the name of device node, typically name@addr, for a given node,
 * using the driver name, not the nodename.
 *
 * Used by match_parent. Not to be used elsewhere.
 */
char *
i_ddi_parname(dev_info_t *dip, char *name)
{
	char *addrname;

	if (dip == ddi_root_node()) {
		*name = '\0';
		return (name);
	}

	ASSERT(DDI_CF1(dip));	/* Replaces ddi_initchild call */

	if (*(addrname = ddi_get_name_addr(dip)) == '\0')
		(void) sprintf(name, "/%s", ddi_binding_name(dip));
	else
		(void) sprintf(name, "/%s@%s", ddi_binding_name(dip), addrname);
	return (name);
}

char *
ddi_fpathname(dev_info_t *dip, char *path)
{
	return (ddi_pathname_work(dip, path));
}

char *
ddi_pathname_work(dev_info_t *dip, char *path)
{
	char *bp;

	if (dip == ddi_root_node()) {
		*path = '\0';
		return (path);
	}
	(void) ddi_pathname_work(ddi_get_parent(dip), path);
	bp = path + strlen(path);
	(void) ddi_deviname(dip, bp);
	return (path);
}

char *
ddi_pathname(dev_info_t *dip, char *path)
{
	return (ddi_pathname_work(dip, path));
}

/*
 * Given a dev_t, return the pathname of the corresponding device in the
 * buffer pointed at by "name."  The buffer is assumed to be large enough
 * to hold the pathname of the device.
 *
 * The pathname of a device is the pathname of the devinfo node to which
 * the device "belongs," concatenated with the character ':' and the name
 * of the minor node corresponding to the dev_t.
 */
int
ddi_dev_pathname(dev_t devt, char *name)
{
	dev_info_t *dip;
	char *bp;
	struct ddi_minor_data *dmn;
	major_t maj = getmajor(devt);
	int error = DDI_FAILURE;

	if (ddi_hold_installed_driver(maj) == NULL)
		return (DDI_FAILURE);

	if (!(dip = e_ddi_get_dev_info(devt, VCHR)))  {
		/*
		 * e_ddi_get_dev_info() only returns with the driver
		 * held if it successfully translated its dev_t.
		 * So we only need to do a rele for ddi_hold_installed_driver
		 * at this point
		 */
		ddi_rele_driver(maj);
		return (DDI_FAILURE);
	}

	ddi_rele_driver(maj);	/* 1st for dev_get_dev_info() */

	(void) ddi_pathname(dip, name);
	bp = name + strlen(name);

	mutex_enter(&(DEVI(dip)->devi_lock));
	for (dmn = DEVI(dip)->devi_minor; dmn; dmn = dmn->next) {
		if (((dmn->type == DDM_MINOR) ||
			(dmn->type == DDM_INTERNAL_PATH) ||
			(dmn->type == DDM_DEFAULT)) &&
			(dmn->ddm_dev == devt)) {
				*bp++ = ':';
				(void) strcpy(bp, dmn->ddm_name);
				error = DDI_SUCCESS;
				break;
				/*NOTREACHED*/
		} else if (dmn->type == DDM_ALIAS &&
		    dmn->ddm_adev == devt) {
			*bp++ = ':';
			(void) strcpy(bp, dmn->ddm_aname);
			error = DDI_SUCCESS;
			break;
			/*NOTREACHED*/
		}
	}
	mutex_exit(&(DEVI(dip)->devi_lock));

	ddi_rele_driver(maj);	/* 2nd for ddi_hold_installed_driver */
	return (error);
}

static void
parse_name(char *name, char **drvname, char **addrname, char **minorname)
{
	char *cp, ch;
	static char nulladdrname[] = ":\0";

	cp = *drvname = name;
	*addrname = *minorname = NULL;
	while ((ch = *cp) != '\0') {
		if (ch == '@')
			*addrname = ++cp;
		else if (ch == ':')
			*minorname = ++cp;
		++cp;
	}
	if (!*addrname)
		*addrname = &nulladdrname[1];
	*((*addrname)-1) = '\0';
	if (*minorname)
		*((*minorname)-1) = '\0';
}

static char *
bind_child(dev_info_t *parent, char *child_name, char *unit_address)
{
	char *p, *binding;

	/*
	 * Construct the pathname and ask the implementation
	 * if it can do a driver = f(pathname) for us, if not
	 * we'll just default to using the node-name that
	 * was given to us.  We want to do this first to
	 * allow the platform to use 'generic' names for
	 * legacy device drivers.
	 */
	p = kmem_zalloc(MAXPATHLEN, KM_SLEEP);
	(void) ddi_pathname(parent, p);
	(void) strcat(p, "/");
	(void) strcat(p, child_name);
	if (unit_address && *unit_address) {
		(void) strcat(p, "@");
		(void) strcat(p, unit_address);
	}

	/*
	 * Get the binding, if there is none, default to the child_name,
	 * and let the caller deal with it.
	 */
	if ((binding = i_path_to_drv(p)) == NULL)
		binding = child_name;

	kmem_free(p, MAXPATHLEN);

	return (binding);
}

/*
 * Given the pathname of a device, fill in the dev_info_t value and/or the
 * dev_t value, depending on which parameters are non-NULL.
 * If there is an error, this function returns -1.
 *
 * NOTE: If this function returns the dev_info_t structure, then it
 * does so with a hold on the devops for the underlying driver.  The caller
 * should ensure that they get decremented again.
 *
 * This should only be called during boot since it doesn't block the
 * device tree from changes by the hotplugging daemon.
 *
 * The only callers of this are ddi_pathname_to_dev_t and i_ddi_path_to_devi
 */
static int
i_ddi_resolve_pathname(char *pathname, dev_info_t **dipp, dev_t *devtp)
{
	struct pathname pn;
	struct ddi_minor_data *dmn;
	int error = 0;
	dev_info_t *parent;
	dev_info_t *nparent;
	char component[MAXNAMELEN];
	char *drvname, *unit_address, *minorname, *nodename;
	dev_t devt;
	major_t major, lastmajor;
#if defined(i386)
	int repeat = 1;
#endif

	devt = (dev_t)-1l;		/* There is no match yet */
	major = (major_t)-1;		/* There is no driver to release */
	lastmajor = (major_t)-1;	/* There is no parent drv to release */

	if (*pathname != '/')
		return (EINVAL);
	if (ddi_install_driver("rootnex") != DDI_SUCCESS)
		return (ENODEV);
	if (error = pn_get(pathname, UIO_SYSSPACE, &pn))
		return (error);

#if defined(i386)
	/*
	 * The need to do the loop twice is to fix a problem in installing
	 * system on IDE disk when the system is booted thru scsi CD device.
	 * The problem is that the way device configuration works it
	 * picks up the boot device (e.g EHA controller and not ATA
	 * controller) as the primary controller but we want the system to
	 * use ATA as the primary controller for setting up the bootpath for
	 * ATA device.
	 * XXX Until there is another proven way to fix this problem this is
	 * a working solution.
	 */
	pn_setlast(&pn);
#else
	pn_skipslash(&pn);
#endif

	parent = ddi_root_node();	/* Begin at the top of the tree */

again:
	while (pn_pathleft(&pn)) {
		(void) pn_getcomponent(&pn, component);
		parse_name(component, &nodename, &unit_address, &minorname);

		/*
		 * Given my parent, my nodename and my unit address,
		 * get the driver bound to this specific instance of
		 * this name, returning the driver binding name.
		 * (The default action is to return the nodename.)
		 */
		drvname = bind_child(parent, nodename, unit_address);

		lastmajor = major;
		if ((major = ddi_name_to_major(drvname)) == (major_t)-1) {
			error = 1;
			break;
		}
		if (ddi_hold_installed_driver(major) == NULL)  {
			major = (major_t)-1;
			error = 1;
			break;
		}

		nparent = ddi_findchild(parent, drvname, unit_address);
		if (nparent == NULL)  {
			error = 1;
			break;
		}
		if (lastmajor != (major_t)-1)  { /* Release the prev parent */
			ddi_rele_driver(lastmajor);
			lastmajor = (major_t)-1;
		}
		parent = nparent;
		pn_skipslash(&pn);
	}
#if defined(i386)
	if (repeat) {
		repeat = 0;
		pn_free(&pn);
		(void) pn_get(pathname, UIO_SYSSPACE, &pn);
		error = 0;
		devt = (dev_t)-1l;
		major = (major_t)-1;
		lastmajor = (major_t)-1;
		pn_skipslash(&pn);
		goto again;
	}
#endif

	if ((devtp != NULL) && (!error)) {
		if (minorname) {
			mutex_enter(&(DEVI(parent)->devi_lock));
			for (dmn = DEVI(parent)->devi_minor; dmn;
			    dmn = dmn->next) {
				if (((dmn->type == DDM_MINOR) ||
				    (dmn->type == DDM_INTERNAL_PATH) ||
				    (dmn->type == DDM_DEFAULT)) &&
				    strcmp(dmn->ddm_name, minorname) == 0) {
					devt = dmn->ddm_dev;
					break;
				}
			}
			mutex_exit(&(DEVI(parent)->devi_lock));
		} else {
			/* check for a default entry */
			mutex_enter(&(DEVI(parent)->devi_lock));
			for (dmn = DEVI(parent)->devi_minor; dmn;
			    dmn = dmn->next) {
				if (dmn->type == DDM_DEFAULT) {
					devt = dmn->ddm_dev;
					goto got_one;
				}
			}
			/* No default minor node - just return the first one */
			if ((dmn = DEVI(parent)->devi_minor) != NULL &&
			    ((dmn->type == DDM_MINOR) ||
			    (dmn->type == DDM_INTERNAL_PATH))) {
				devt = dmn->ddm_dev;
			} else {
				/* Assume 1-to-1 mapping of instance to minor */
				devt = makedevice(major,
				    ddi_get_instance(parent));
			}
got_one:
			mutex_exit(&(DEVI(parent)->devi_lock));
		}
	}

	pn_free(&pn);

	/*
	 * Release the hold on the driver associated with 'lastmajor' now that
	 * we are done with it.
	 */
	if (lastmajor != (major_t)-1)
		ddi_rele_driver(lastmajor);

	/*
	 * If we are returning the dev_info_t structure, we need to return
	 * with the devops of the underlying driver held.  So, free up the
	 * underlying driver only if we are not returning the dev_info_t
	 * structure.
	 */
	if (((error) || (dipp == NULL)) && (major != (major_t)-1)) {
		ddi_rele_driver(major);
	}

	/*
	 * If there is no error, return the appropriate parameters
	 */
	if (!error) {
		if (devtp != NULL) {
			*devtp = devt;
		}
		if (dipp != NULL) {
			*dipp = parent;
		}
	}

	return (error);
}

/*
 * Given the pathname of a device, return the dev_t of the corresponding
 * device.  Returns ((dev_t)-1l) on failure.  Should only be called
 * during boot.
 */
dev_t
ddi_pathname_to_dev_t(char *pathname)
{
	dev_t devt;
	int error;

	error = i_ddi_resolve_pathname(pathname, NULL, &devt);

	return ((error != 0) ? ((dev_t)-1l) : devt);
}

/*
 * Given the pathname of a device, return the dev_info_t structure of the
 * corresponding device.  Returns NULL on failure.  Should only be called
 * during boot.
 *
 * Note that this call returns with the devops of the underlying driver held -
 * the caller is responsible for freeing it up (using ddi_rele_devi()).
 */
dev_info_t *
i_ddi_path_to_devi(char *pathname)
{
	dev_info_t *dip;
	int error;

	error = i_ddi_resolve_pathname(pathname, &dip, NULL);

	return ((error != 0) ? NULL : dip);
}

/*
 * Find a child of 'p' whose name matches the parameters cname@caddr.
 * The caller must ensure that we are single threading anything that
 * can change the per-driver's instance list!
 */

dev_info_t *
ddi_findchild(dev_info_t *p, char *cname, char *caddr)
{
	dev_info_t *cdip, *ndip;
	char *naddr;
	major_t major;

#ifdef i386
	extern int x86_old_bootpath_name_addr_match(dev_info_t *,
	    char *, char *);
#endif

#ifdef lint
	ndip = NULL;	/* See 1094364 */
#endif

	if (p == NULL)
		return (NULL);

	major = ddi_name_to_major(cname);
	ASSERT(major != (major_t)-1);

	/*
	 * Using the drivers instance list, init each child as we look for
	 * a match.
	 */

	for (cdip = devnamesp[major].dn_head; cdip != NULL; cdip = ndip)  {
		ndip = ddi_get_next(cdip);
		if (ddi_get_parent(cdip) == p)	{
			if (ddi_initchild(p, cdip) != DDI_SUCCESS)  {
				if (ndi_dev_is_persistent_node(cdip) == 0)
					(void) ddi_remove_child(cdip, 0);
				continue;
			}
			if ((naddr = ddi_get_name_addr(cdip)) != NULL &&
			    (strcmp(caddr, naddr) == 0))
				break;
#ifdef i386
/*
 * This is temporary, but absolutely necessary.  If we are being
 * booted with a device tree created by the DevConf project's bootconf
 * program, then we have device information nodes that reflect
 * reality.  At this point in time in the Solaris release schedule, the
 * kernel drivers aren't prepared for reality.	They still depend on their
 * own ad-hoc interpretations of the properties created when their .conf
 * files were interpreted. These drivers use an "ignore-hardware-nodes"
 * property to prevent them from using the nodes passed up from the bootconf
 * device tree.
 *
 * Trying to assemble root file system drivers as we are booting from
 * devconf will fail if the kernel driver is basing its name_addr's on the
 * psuedo-node device info while the bootpath passed up from bootconf is using
 * reality-based name_addrs.  We help the boot along in this case by
 * looking at the pre-bootconf bootpath and determining if we would have
 * successfully matched if that had been the bootpath we had chosen.
 */
			else {
				if (naddr &&
				    x86_old_bootpath_name_addr_match(cdip,
					caddr, naddr) == DDI_SUCCESS)
					break;
			}
#endif
		}
	}
	return (cdip);
}

/*
 * e_ddi_deferred_attach:	Attempt to attach either a specific
 *		devinfo or all unattached devinfos to a driver
 *
 *	dev_t == NODEV means try all unattached instances.
 *
 * Specific case returns DDI_SUCCESS if the devinfo was attached.
 *
 * DDI framework layer, only. (Not an exported interface).
 */

int
e_ddi_deferred_attach(major_t major, dev_t dev)
{
	struct devnames *dnp = &devnamesp[major];
	dev_info_t *dip;
	dev_info_t *ndip;
	int instance;
	int error = DDI_FAILURE;
	int listcnt;
	mta_handle_t *mta;

	/*
	 * The driver must be held before calling e_ddi_deferred_attach
	 */
	ASSERT(DEV_OPS_HELD(devopsp[major]));

	/*
	 * Prevent other threads from loading/holding/attaching/unloading this
	 * driver while we are attempting deferred attach...
	 */
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	if (dev != NODEV)  {

		/*
		 * Specific dev_t case ... Call the driver to get the
		 * instance number of the given dev_t and if it is valid,
		 * and we have the given instance, try to transform it
		 * it to CF2.
		 */
		if ((instance = dev_to_instance(dev)) < 0)
			goto out;

		for (dip = dnp->dn_head; dip != NULL; dip = ddi_get_next(dip))
			if (ddi_get_instance(dip) == instance)
				break;

		if ((dip == NULL) || (DEVI_IS_DEVICE_OFFLINE(dip))) {
			goto out;	/* Not found or offline */
		}

		if (DDI_CF2(dip)) {
			error = DDI_SUCCESS;
			goto out;	/* Already attached */
		}

		if ((error = impl_proto_to_cf2(dip)) == DDI_SUCCESS)
			ddi_rele_driver(major);	/* Undo extra hold */
		goto out;
	}

	/*
	 * Non-specific dev_t case ... (always succeeds, even if it does
	 * nothing)
	 *
	 * Attempt multi-threaded attach to speed up reconfiguration boot
	 */

	error = DDI_SUCCESS;
	mta = mta_get_handle(major);

	for (dip = dnp->dn_head; dip != NULL; dip = ndip)  {
		ndip = ddi_get_next(dip);

		/*
		 * If the devinfo is named, but not attached, attempt
		 * to attach the node. Skip any devinfo nodes that
		 * have been placed in the Offline state.
		 */
		if (!(DDI_CF1(dip)) || DDI_CF2(dip) ||
		    DEVI_IS_DEVICE_OFFLINE(dip))
			continue;

		/*
		 * Set driver ops before calling impl_initdev
		 */
		if (impl_initnode(dip) != DDI_SUCCESS)
			continue;

		if (mta == NULL)
			(void) impl_initdev(dip);
		else
			mta_add_dip(mta, dip);	/* queue for mt attach */
	}

	if (mta != NULL)
		mta_attach_devi_list(mta);

	/*
	 * attach any nodes that happen to be on the newdevs list
	 * matching this major number
	 */
	(void) i_ndi_attach_new_devinfo(major);

out:

	/*
	 * Give up the busy/changing lock on this device driver
	 */
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_exit_driver_list(dnp, listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	return (error);
}

/*
 * DDI Console bell functions.
 *
 * XXXX: These are not yet part of the DDI!
 */
void
ddi_ring_console_bell(clock_t duration)
{
	if (ddi_console_bell_func != NULL)
		(*ddi_console_bell_func)(duration);
}

void
ddi_set_console_bell(void (*bellfunc)(clock_t duration))
{
	ddi_console_bell_func = bellfunc;
}

/*
 * Here we try and anticipate the case where 'call parent'
 * immediately results in another 'call parent' by looking at
 * what our parent nexus driver would do if we asked it.
 *
 * If the order we do things seems a bit back-to-front, then
 * remember that we instantiate the devinfo tree top down, which
 * means that at any point in the tree, we can assume our parents
 * are already optimized.
 */
/* #define	DEBUG_DTREE */

#ifdef DEBUG_DTREE
#ifndef DEBUG
#define	DEBUG
#endif /* !DEBUG */
static void debug_dtree(dev_info_t *, struct dev_info *, char *);
#endif /* DEBUG_DTREE */

#ifdef DEBUG
/*
 * Set this variable to '0' to disable the optimization.
 */
int optimize_dtree = 1;
#endif /* DEBUG */

void
ddi_optimize_dtree(dev_info_t *devi)
{
	struct dev_info *pdevi;
	struct bus_ops *b;

	ASSERT(DDI_CF1(devi));
	pdevi = DEVI(devi)->devi_parent;
	ASSERT(pdevi);
	b = pdevi->devi_ops->devo_bus_ops;

#ifdef DEBUG
	/*
	 * Last chance to bailout..
	 */
	if (!optimize_dtree) {
		DEVI(devi)->devi_bus_map_fault = pdevi;
		DEVI(devi)->devi_bus_dma_map = pdevi;
		DEVI(devi)->devi_bus_dma_allochdl = pdevi;
		DEVI(devi)->devi_bus_dma_freehdl = pdevi;
		DEVI(devi)->devi_bus_dma_bindhdl = pdevi;
		DEVI(devi)->devi_bus_dma_unbindhdl = pdevi;
		DEVI(devi)->devi_bus_dma_flush = pdevi;
		DEVI(devi)->devi_bus_dma_win = pdevi;
		DEVI(devi)->devi_bus_dma_ctl = pdevi;
		DEVI(devi)->devi_bus_ctl = pdevi;
		return;
	}
#endif	/* DEBUG */

	/*
	 * XXX	This one is a bit dubious, because i_ddi_map_fault
	 *	is currently (wrongly) an implementation dependent
	 *	function.  However, given that it's only i_ddi_map_fault
	 *	that -uses- the devi_bus_map_fault pointer, this is ok.
	 */
	if (i_ddi_map_fault == b->bus_map_fault) {
		DEVI(devi)->devi_bus_map_fault = pdevi->devi_bus_map_fault;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_map_fault,
		    "bus_map_fault");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_map_fault = pdevi;

	if (ddi_dma_map == b->bus_dma_map) {
		DEVI(devi)->devi_bus_dma_map = pdevi->devi_bus_dma_map;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_map, "bus_dma_map");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_dma_map = pdevi;

	if (ddi_dma_allochdl == b->bus_dma_allochdl) {
		DEVI(devi)->devi_bus_dma_allochdl =
		    pdevi->devi_bus_dma_allochdl;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_allochdl,
		    "bus_dma_allochdl");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_dma_allochdl = pdevi;

	if (ddi_dma_freehdl == b->bus_dma_freehdl) {
		DEVI(devi)->devi_bus_dma_freehdl = pdevi->devi_bus_dma_freehdl;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_freehdl,
		    "bus_dma_freehdl");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_dma_freehdl = pdevi;

	if (ddi_dma_bindhdl == b->bus_dma_bindhdl) {
		DEVI(devi)->devi_bus_dma_bindhdl = pdevi->devi_bus_dma_bindhdl;
		DEVI(devi)->devi_bus_dma_bindfunc =
		    pdevi->devi_bus_dma_bindhdl->devi_ops->
			devo_bus_ops->bus_dma_bindhdl;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_bindhdl,
		    "bus_dma_bindhdl");
#endif	/* DEBUG_DTREE */
	} else {
		DEVI(devi)->devi_bus_dma_bindhdl = pdevi;
		DEVI(devi)->devi_bus_dma_bindfunc =
		    pdevi->devi_ops->devo_bus_ops->bus_dma_bindhdl;
	}

	if (ddi_dma_unbindhdl == b->bus_dma_unbindhdl) {
		DEVI(devi)->devi_bus_dma_unbindhdl =
		    pdevi->devi_bus_dma_unbindhdl;
		DEVI(devi)->devi_bus_dma_unbindfunc =
		    pdevi->devi_bus_dma_unbindhdl->devi_ops->
			devo_bus_ops->bus_dma_unbindhdl;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_unbindhdl,
		    "bus_dma_unbindhdl");
#endif	/* DEBUG_DTREE */
	} else {
		DEVI(devi)->devi_bus_dma_unbindhdl = pdevi;
		DEVI(devi)->devi_bus_dma_unbindfunc =
		    pdevi->devi_ops->devo_bus_ops->bus_dma_unbindhdl;
	}

	if (ddi_dma_flush == b->bus_dma_flush) {
		DEVI(devi)->devi_bus_dma_flush =
		    pdevi->devi_bus_dma_flush;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_flush,
		    "bus_dma_flush");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_dma_flush = pdevi;

	if (ddi_dma_win == b->bus_dma_win) {
		DEVI(devi)->devi_bus_dma_win =
		    pdevi->devi_bus_dma_win;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_win,
		    "bus_dma_win");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_dma_win = pdevi;

	if (ddi_dma_mctl == b->bus_dma_ctl) {
		DEVI(devi)->devi_bus_dma_ctl = pdevi->devi_bus_dma_ctl;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_dma_ctl, "bus_dma_ctl");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_dma_ctl = pdevi;

	if (ddi_ctlops == b->bus_ctl) {
		DEVI(devi)->devi_bus_ctl = pdevi->devi_bus_ctl;
#ifdef	DEBUG_DTREE
		debug_dtree(devi, DEVI(devi)->devi_bus_ctl, "bus_ctl");
#endif	/* DEBUG_DTREE */
	} else
		DEVI(devi)->devi_bus_ctl = pdevi;
}

#ifdef	DEBUG_DTREE
static void
debug_dtree(dev_info_t *devi, struct dev_info *adevi, char *service)
{
#ifdef	DEBUG
	char *adevi_node_name;
	char *adevi_addr;

	if ((dev_info_t *)adevi == ddi_root_node()) {
		adevi_node_name = "root";
		adevi_addr = "0";
	} else {
		adevi_node_name = adevi->devi_node_name;
		adevi_addr = adevi->devi_addr;
	}
	cmn_err(CE_CONT, "%s@%s %s -> %s@%s\n",
	    ddi_node_name(devi), ddi_get_name_addr(devi),
	    service, adevi_node_name, adevi_addr);
#endif /* DEBUG */
}
#endif /* DEBUG_DTREE */

int
ddi_dma_alloc_handle(dev_info_t *dip, ddi_dma_attr_t *attr,
	int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	int (*funcp)() = ddi_dma_allochdl;
	ddi_dma_attr_t dma_attr;
	struct bus_ops *bop;

	if (attr == (ddi_dma_attr_t *)0)
		return (DDI_DMA_BADATTR);

	dma_attr = *attr;

	bop = DEVI(dip)->devi_ops->devo_bus_ops;
	if (bop && bop->bus_dma_allochdl)
		funcp = bop->bus_dma_allochdl;

	return ((*funcp)(dip, dip, &dma_attr, waitfp, arg, handlep));
}

void
ddi_dma_free_handle(ddi_dma_handle_t *handlep)
{
	ddi_dma_handle_t h = *handlep;
	(void) ddi_dma_freehdl(HD, HD, h);
}

static uintptr_t dma_mem_list_id = 0;


int
ddi_dma_mem_alloc(ddi_dma_handle_t handle, size_t length,
	ddi_device_acc_attr_t *accattrp, uint_t xfermodes,
	int (*waitfp)(caddr_t), caddr_t arg, caddr_t *kaddrp,
	size_t *real_length, ddi_acc_handle_t *handlep)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	dev_info_t *dip = hp->dmai_rdip;
	ddi_acc_hdl_t *ap;
	ddi_dma_attr_t *attrp = &hp->dmai_attr;
	uint_t sleepflag;
	int (*fp)(caddr_t);
	int rval;

	if (waitfp == DDI_DMA_SLEEP)
		fp = (int (*)())KM_SLEEP;
	else if (waitfp == DDI_DMA_DONTWAIT)
		fp = (int (*)())KM_NOSLEEP;
	else
		fp = waitfp;
	*handlep = impl_acc_hdl_alloc(fp, arg);
	if (*handlep == NULL)
		return (DDI_FAILURE);

	/*
	 * initialize the common elements of data access handle
	 */
	ap = impl_acc_hdl_get(*handlep);
	ap->ah_vers = VERS_ACCHDL;
	ap->ah_offset = 0;
	ap->ah_len = 0;
	ap->ah_xfermodes = xfermodes;
	ap->ah_acc = *accattrp;

	sleepflag = ((waitfp == DDI_DMA_SLEEP) ? 1 : 0);
	if (xfermodes == DDI_DMA_CONSISTENT) {
		rval = i_ddi_mem_alloc(dip, attrp, length, sleepflag, 0,
			    accattrp, kaddrp, NULL, ap);
		*real_length = length;
	} else {
		rval = i_ddi_mem_alloc(dip, attrp, length, sleepflag, 1,
			    accattrp, kaddrp, real_length, ap);
	}
	if (rval == DDI_SUCCESS) {
		ap->ah_len = (off_t)(*real_length);
		ap->ah_addr = *kaddrp;
	} else {
		impl_acc_hdl_free(*handlep);
		*handlep = (ddi_acc_handle_t)NULL;
		if (waitfp != DDI_DMA_SLEEP && waitfp != DDI_DMA_DONTWAIT) {
			ddi_set_callback(waitfp, arg, &dma_mem_list_id);
		}
		rval = DDI_FAILURE;
	}
	return (rval);
}

void
ddi_dma_mem_free(ddi_acc_handle_t *handlep)
{
	ddi_acc_hdl_t *ap;

	ap = impl_acc_hdl_get(*handlep);
	ASSERT(ap);

	if (ap->ah_xfermodes == DDI_DMA_CONSISTENT) {
		i_ddi_mem_free((caddr_t)ap->ah_addr, 0);
	} else {
		i_ddi_mem_free((caddr_t)ap->ah_addr, 1);
	}

	/*
	 * free the handle
	 */
	impl_acc_hdl_free(*handlep);
	*handlep = (ddi_acc_handle_t)NULL;

	if (dma_mem_list_id != 0) {
		ddi_run_callback(&dma_mem_list_id);
	}
}

int
ddi_dma_buf_bind_handle(ddi_dma_handle_t handle, struct buf *bp,
	uint_t flags, int (*waitfp)(caddr_t), caddr_t arg,
	ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	dev_info_t *hdip, *dip;
	struct ddi_dma_req dmareq;
	int (*funcp)();

	dmareq.dmar_flags = flags;
	dmareq.dmar_fp = waitfp;
	dmareq.dmar_arg = arg;
	dmareq.dmar_object.dmao_size = (uint_t)bp->b_bcount;

	if ((bp->b_flags & (B_PAGEIO|B_REMAPPED)) == B_PAGEIO) {
		dmareq.dmar_object.dmao_type = DMA_OTYP_PAGES;
		dmareq.dmar_object.dmao_obj.pp_obj.pp_pp = bp->b_pages;
		dmareq.dmar_object.dmao_obj.pp_obj.pp_offset =
		    (uint_t)(((uintptr_t)bp->b_un.b_addr) & MMU_PAGEOFFSET);
	} else {
		dmareq.dmar_object.dmao_type = DMA_OTYP_VADDR;
		dmareq.dmar_object.dmao_obj.virt_obj.v_addr = bp->b_un.b_addr;
		if ((bp->b_flags & (B_SHADOW|B_REMAPPED)) == B_SHADOW) {
			dmareq.dmar_object.dmao_obj.virt_obj.v_priv =
							bp->b_shadow;
		} else {
			dmareq.dmar_object.dmao_obj.virt_obj.v_priv = NULL;
		}

		/*
		 * If the buffer has no proc pointer, or the proc
		 * struct has the kernel address space, or the buffer has
		 * been marked B_REMAPPED (meaning that it is now
		 * mapped into the kernel's address space), then
		 * the address space is kas (kernel address space).
		 */
		if (bp->b_proc == NULL || bp->b_proc->p_as == &kas ||
		    (bp->b_flags & B_REMAPPED) != 0) {
			dmareq.dmar_object.dmao_obj.virt_obj.v_as = 0;
		} else {
			dmareq.dmar_object.dmao_obj.virt_obj.v_as =
			    bp->b_proc->p_as;
		}
	}

	dip = hp->dmai_rdip;
	hdip = (dev_info_t *)DEVI(dip)->devi_bus_dma_bindhdl;
	funcp = DEVI(dip)->devi_bus_dma_bindfunc;
	return ((*funcp)(hdip, dip, handle, &dmareq, cookiep, ccountp));
}

int
ddi_dma_addr_bind_handle(ddi_dma_handle_t handle, struct as *as,
	caddr_t addr, size_t len, uint_t flags, int (*waitfp)(caddr_t),
	caddr_t arg, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	dev_info_t *hdip, *dip;
	struct ddi_dma_req dmareq;
	int (*funcp)();

	if (len == (uint_t)0) {
		return (DDI_DMA_NOMAPPING);
	}
	dmareq.dmar_flags = flags;
	dmareq.dmar_fp = waitfp;
	dmareq.dmar_arg = arg;
	dmareq.dmar_object.dmao_size = len;
	dmareq.dmar_object.dmao_type = DMA_OTYP_VADDR;
	dmareq.dmar_object.dmao_obj.virt_obj.v_as = as;
	dmareq.dmar_object.dmao_obj.virt_obj.v_addr = addr;
	dmareq.dmar_object.dmao_obj.virt_obj.v_priv = NULL;

	dip = hp->dmai_rdip;
	hdip = (dev_info_t *)DEVI(dip)->devi_bus_dma_bindhdl;
	funcp = DEVI(dip)->devi_bus_dma_bindfunc;
	return ((*funcp)(hdip, dip, handle, &dmareq, cookiep, ccountp));
}

void
ddi_dma_nextcookie(ddi_dma_handle_t handle, ddi_dma_cookie_t *cookiep)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	ddi_dma_cookie_t *cp;

	cp = hp->dmai_cookie;
	ASSERT(cp);

	cookiep->dmac_notused = cp->dmac_notused;
	cookiep->dmac_type = cp->dmac_type;
	cookiep->dmac_address = cp->dmac_address;
	cookiep->dmac_size = cp->dmac_size;
	hp->dmai_cookie++;
}

int
ddi_dma_numwin(ddi_dma_handle_t handle, uint_t *nwinp)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	if ((hp->dmai_rflags & DDI_DMA_PARTIAL) == 0) {
		return (DDI_FAILURE);
	} else {
		*nwinp = hp->dmai_nwin;
		return (DDI_SUCCESS);
	}
}

int
ddi_dma_getwin(ddi_dma_handle_t h, uint_t win, off_t *offp,
	size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	int (*funcp)() = ddi_dma_win;
	struct bus_ops *bop;

	bop = DEVI(HD)->devi_ops->devo_bus_ops;
	if (bop && bop->bus_dma_win)
		funcp = bop->bus_dma_win;

	return ((*funcp)(HD, HD, h, win, offp, lenp, cookiep, ccountp));
}

int
ddi_dma_set_sbus64(ddi_dma_handle_t h, ulong_t burstsizes)
{
	return (ddi_dma_mctl(HD, HD, h, DDI_DMA_SET_SBUS64, 0,
		&burstsizes, 0, 0));
}

int
i_ddi_dma_fault_check(ddi_dma_impl_t *hp)
{
	return (hp->dmai_fault);
}

int
ddi_check_dma_handle(ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	int (*check)(ddi_dma_impl_t *);

	if ((check = hp->dmai_fault_check) == NULL)
		check = i_ddi_dma_fault_check;

	return ((*check)(hp));
}

void
i_ddi_dma_set_fault(ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	void (*notify)(ddi_dma_impl_t *);

	if (!hp->dmai_fault) {
		hp->dmai_fault = 1;
		if ((notify = hp->dmai_fault_notify) != NULL)
			(*notify)(hp);
	}
}

void
i_ddi_dma_clr_fault(ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	void (*notify)(ddi_dma_impl_t *);

	if (hp->dmai_fault) {
		hp->dmai_fault = 0;
		if ((notify = hp->dmai_fault_notify) != NULL)
			(*notify)(hp);
	}
}

/*
 * register mapping routines.
 */
int
ddi_regs_map_setup(dev_info_t *dip, uint_t rnumber, caddr_t *addrp,
	offset_t offset, offset_t len, ddi_device_acc_attr_t *accattrp,
	ddi_acc_handle_t *handle)
{
	ddi_map_req_t mr;
	ddi_acc_hdl_t *hp;
	int result;

	/*
	 * Allocate and initialize the common elements of data access handle.
	 */
	*handle = impl_acc_hdl_alloc(KM_SLEEP, NULL);
	hp = impl_acc_hdl_get(*handle);
	hp->ah_vers = VERS_ACCHDL;
	hp->ah_dip = dip;
	hp->ah_rnumber = rnumber;
	hp->ah_offset = offset;
	hp->ah_len = len;
	hp->ah_acc = *accattrp;

	/*
	 * Set up the mapping request and call to parent.
	 */
	mr.map_op = DDI_MO_MAP_LOCKED;
	mr.map_type = DDI_MT_RNUMBER;
	mr.map_obj.rnumber = rnumber;
	mr.map_prot = PROT_READ | PROT_WRITE;
	mr.map_flags = DDI_MF_KERNEL_MAPPING;
	mr.map_handlep = hp;
	mr.map_vers = DDI_MAP_VERSION;
	result = ddi_map(dip, &mr, offset, len, addrp);

	/*
	 * check for end result
	 */
	if (result != DDI_SUCCESS) {
		impl_acc_hdl_free(*handle);
		*handle = (ddi_acc_handle_t)NULL;
	} else {
		hp->ah_addr = *addrp;
	}

	return (result);
}

void
ddi_regs_map_free(ddi_acc_handle_t *handlep)
{
	ddi_map_req_t mr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(*handlep);
	ASSERT(hp);

	mr.map_op = DDI_MO_UNMAP;
	mr.map_type = DDI_MT_RNUMBER;
	mr.map_obj.rnumber = hp->ah_rnumber;
	mr.map_prot = PROT_READ | PROT_WRITE;
	mr.map_flags = DDI_MF_KERNEL_MAPPING;
	mr.map_handlep = hp;
	mr.map_vers = DDI_MAP_VERSION;

	/*
	 * Call my parent to unmap my regs.
	 */
	(void) ddi_map(hp->ah_dip, &mr, hp->ah_offset,
		hp->ah_len, &hp->ah_addr);
	/*
	 * free the handle
	 */
	impl_acc_hdl_free(*handlep);
	*handlep = (ddi_acc_handle_t)NULL;
}

int
ddi_device_zero(ddi_acc_handle_t handle, caddr_t dev_addr, size_t bytecount,
	ssize_t dev_advcnt, uint_t dev_datasz)
{
	uint8_t *b;
	uint16_t *w;
	uint32_t *l;
	uint64_t *ll;

	/* check for total byte count is multiple of data transfer size */
	if (bytecount != ((bytecount / dev_datasz) * dev_datasz))
		return (DDI_FAILURE);

	switch (dev_datasz) {
	case DDI_DATA_SZ01_ACC:
		for (b = (uint8_t *)dev_addr;
			bytecount != 0; bytecount -= 1, b += dev_advcnt)
			ddi_put8(handle, b, 0);
		break;
	case DDI_DATA_SZ02_ACC:
		for (w = (uint16_t *)dev_addr;
			bytecount != 0; bytecount -= 2, w += dev_advcnt)
			ddi_put16(handle, w, 0);
		break;
	case DDI_DATA_SZ04_ACC:
		for (l = (uint32_t *)dev_addr;
			bytecount != 0; bytecount -= 4, l += dev_advcnt)
			ddi_put32(handle, l, 0);
		break;
	case DDI_DATA_SZ08_ACC:
		for (ll = (uint64_t *)dev_addr;
			bytecount != 0; bytecount -= 8, ll += dev_advcnt)
			ddi_put64(handle, ll, 0x0ll);
		break;
	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

int
ddi_device_copy(
	ddi_acc_handle_t src_handle, caddr_t src_addr, ssize_t src_advcnt,
	ddi_acc_handle_t dest_handle, caddr_t dest_addr, ssize_t dest_advcnt,
	size_t bytecount, uint_t dev_datasz)
{
	uint8_t *b_src, *b_dst;
	uint16_t *w_src, *w_dst;
	uint32_t *l_src, *l_dst;
	uint64_t *ll_src, *ll_dst;

	/* check for total byte count is multiple of data transfer size */
	if (bytecount != ((bytecount / dev_datasz) * dev_datasz))
		return (DDI_FAILURE);

	switch (dev_datasz) {
	case DDI_DATA_SZ01_ACC:
		b_src = (uint8_t *)src_addr;
		b_dst = (uint8_t *)dest_addr;

		for (; bytecount != 0; bytecount -= 1) {
			ddi_put8(dest_handle, b_dst,
				ddi_get8(src_handle, b_src));
			b_dst += dest_advcnt;
			b_src += src_advcnt;
		}
		break;
	case DDI_DATA_SZ02_ACC:
		w_src = (uint16_t *)src_addr;
		w_dst = (uint16_t *)dest_addr;

		for (; bytecount != 0; bytecount -= 2) {
			ddi_put16(dest_handle, w_dst,
				ddi_get16(src_handle, w_src));
			w_dst += dest_advcnt;
			w_src += src_advcnt;
		}
		break;
	case DDI_DATA_SZ04_ACC:
		l_src = (uint32_t *)src_addr;
		l_dst = (uint32_t *)dest_addr;

		for (; bytecount != 0; bytecount -= 4) {
			ddi_put32(dest_handle, l_dst,
				ddi_get32(src_handle, l_src));
			l_dst += dest_advcnt;
			l_src += src_advcnt;
		}
		break;
	case DDI_DATA_SZ08_ACC:
		ll_src = (uint64_t *)src_addr;
		ll_dst = (uint64_t *)dest_addr;

		for (; bytecount != 0; bytecount -= 8) {
			ddi_put64(dest_handle, ll_dst,
				ddi_get64(src_handle, ll_src));
			ll_dst += dest_advcnt;
			ll_src += src_advcnt;
		}
		break;
	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

int
pci_config_setup(dev_info_t *dip, ddi_acc_handle_t *handle)
{
	caddr_t	cfgaddr;
	ddi_device_acc_attr_t attr;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	return (ddi_regs_map_setup(dip, 0, &cfgaddr, 0, 0, &attr, handle));
}

void
pci_config_teardown(ddi_acc_handle_t *handle)
{
	ddi_regs_map_free(handle);
}

#ifdef _LP64
uint8_t
pci_config_get8(ddi_acc_handle_t handle, off_t offset)
#else /* _ILP32 */
uint8_t
pci_config_getb(ddi_acc_handle_t handle, off_t offset)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	return (ddi_get8(handle, (uint8_t *)cfgaddr));
}

#ifdef _LP64
uint16_t
pci_config_get16(ddi_acc_handle_t handle, off_t offset)
#else /* _ILP32 */
uint16_t
pci_config_getw(ddi_acc_handle_t handle, off_t offset)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	return (ddi_get16(handle, (uint16_t *)cfgaddr));
}

#ifdef _LP64
uint32_t
pci_config_get32(ddi_acc_handle_t handle, off_t offset)
#else /* _ILP32 */
uint32_t
pci_config_getl(ddi_acc_handle_t handle, off_t offset)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	return (ddi_get32(handle, (uint32_t *)cfgaddr));
}

#ifdef _LP64
uint64_t
pci_config_get64(ddi_acc_handle_t handle, off_t offset)
#else /* _ILP32 */
uint64_t
pci_config_getll(ddi_acc_handle_t handle, off_t offset)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	return (ddi_get64(handle, (uint64_t *)cfgaddr));
}

#ifdef _LP64
void
pci_config_put8(ddi_acc_handle_t handle, off_t offset, uint8_t value)
#else /* _ILP32 */
void
pci_config_putb(ddi_acc_handle_t handle, off_t offset, uint8_t value)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	ddi_put8(handle, (uint8_t *)cfgaddr, value);
}

#ifdef _LP64
void
pci_config_put16(ddi_acc_handle_t handle, off_t offset, uint16_t value)
#else /* _ILP32 */
void
pci_config_putw(ddi_acc_handle_t handle, off_t offset, uint16_t value)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	ddi_put16(handle, (uint16_t *)cfgaddr, value);
}

#ifdef _LP64
void
pci_config_put32(ddi_acc_handle_t handle, off_t offset, uint32_t value)
#else /* _ILP32 */
void
pci_config_putl(ddi_acc_handle_t handle, off_t offset, uint32_t value)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	ddi_put32(handle, (uint32_t *)cfgaddr, value);
}

#ifdef _LP64
void
pci_config_put64(ddi_acc_handle_t handle, off_t offset, uint64_t value)
#else /* _ILP32 */
void
pci_config_putll(ddi_acc_handle_t handle, off_t offset, uint64_t value)
#endif
{
	caddr_t	cfgaddr;
	ddi_acc_hdl_t *hp;

	hp = impl_acc_hdl_get(handle);
	cfgaddr = hp->ah_addr + offset;
	ddi_put64(handle, (uint64_t *)cfgaddr, value);
}

int
pci_report_pmcap(dev_info_t *dip, int cap, void *arg)
{
	power_req_t	request;

	request.request_type = PMR_REPORT_PMCAP;
	request.req.report_pmcap_req.who = dip;
	request.req.report_pmcap_req.cap = cap;
	request.req.report_pmcap_req.arg = arg;
	return (ddi_ctlops(dip, dip, DDI_CTLOPS_POWER, &request, NULL));
}

#define	swap16(value)  \
	((((value) & 0xff) << 8) | ((value) >> 8))

#define	swap32(value)	\
	(((uint32_t)swap16((uint16_t)((value) & 0xffff)) << 16) | \
	(uint32_t)swap16((uint16_t)((value) >> 16)))

#define	swap64(value)	\
	(((uint64_t)swap32((uint32_t)((value) & 0xffffffff)) \
	    << 32) | \
	(uint64_t)swap32((uint32_t)((value) >> 32)))

uint16_t
ddi_swap16(uint16_t value)
{
	return (swap16(value));
}

uint32_t
ddi_swap32(uint32_t value)
{
	return (swap32(value));
}

uint64_t
ddi_swap64(uint64_t value)
{
	return (swap64(value));
}

/*
 * Convert a binding name to a driver name.
 * A binding name is the name used to determine the driver for a
 * device - it may be either an alias for the driver or the name
 * of the driver itself.
 */
char *
i_binding_to_drv_name(char *bname)
{
	major_t major_no;

	ASSERT(bname != NULL);

	if ((major_no = ddi_name_to_major(bname)) == -1)
		return (NULL);
	return (ddi_major_to_name(major_no));
}

/*
 * Validate device id.
 * Device drivers use this to validate that the fabricated
 * device id, stored in the reserved cylinder, is a valid one.
 */
int
ddi_devid_valid(ddi_devid_t devid)
{
	impl_devid_t	*id = (impl_devid_t *)devid;
	ushort_t		type;

	if (id->did_magic_hi != DEVID_MAGIC_MSB)
		return (DDI_FAILURE);

	if (id->did_magic_lo != DEVID_MAGIC_LSB)
		return (DDI_FAILURE);

	if (id->did_rev_hi != DEVID_REV_MSB)
		return (DDI_FAILURE);

	if (id->did_rev_lo != DEVID_REV_LSB)
		return (DDI_FAILURE);

	type = DEVID_GETTYPE(id);
	if ((type == DEVID_NONE) || (type > DEVID_MAXTYPE))
		return (DDI_FAILURE);

	return (DDI_SUCCESS);
}

/*
 * Register device id into DDI framework.
 * Must be called when device is attached.
 */
int
ddi_devid_register(dev_info_t *dip, ddi_devid_t devid)
{
	impl_devid_t	*i_devid = (impl_devid_t *)devid;
	size_t		driver_len;
	char		*driver_name;

	if (ddi_devid_valid(devid) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/* Updating driver name hint */
	driver_name = ddi_binding_name(dip);
	driver_len = strlen(driver_name);
	if (driver_len > DEVID_HINT_SIZE) {
		/* Pick up last four characters of driver name */
		driver_name += driver_len - DEVID_HINT_SIZE;
		driver_len = DEVID_HINT_SIZE;
	}
	bzero(i_devid->did_driver, DEVID_HINT_SIZE);
	bcopy(driver_name, i_devid->did_driver, driver_len);

	mutex_enter(&(DEVI(dip)->devi_lock));

	/* Check - device id already registered */
	if (DEVI(dip)->devi_devid) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		return (DDI_FAILURE);
	}

	/* Register device id */
	DEVI(dip)->devi_devid = devid;
	mutex_exit(&(DEVI(dip)->devi_lock));
	return (DDI_SUCCESS);
}

/*
 * Remove (unregister) device id from DDI framework.
 * Must be called when device is detached.
 */
void
ddi_devid_unregister(dev_info_t *dip)
{

	mutex_enter(&(DEVI(dip)->devi_lock));

	/* If no device id is registered, then we are done. */
	if (DEVI(dip)->devi_devid == NULL) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		return;
	}

	/* Unregister device id */
	DEVI(dip)->devi_devid = NULL;
	mutex_exit(&(DEVI(dip)->devi_lock));
}

extern char	hw_serial[];

/*
 * Allocate and initialize a device id.
 */
int
ddi_devid_init(
	dev_info_t	*dip,
	ushort_t		devid_type,
	ushort_t		nbytes,
	void		*id,
	ddi_devid_t	*ret_devid)
{
	impl_devid_t	*i_devid;
	int		sz = sizeof (*i_devid) + nbytes - sizeof (char);
	int		driver_len;
	char		*driver_name;

	switch (devid_type) {
	case DEVID_SCSI3_WWN:
		/*FALLTHRU*/
	case DEVID_SCSI_SERIAL:
		/*FALLTHRU*/
	case DEVID_ENCAP:
		if (nbytes == 0)
			return (DDI_FAILURE);
		if (id == NULL)
			return (DDI_FAILURE);
		break;
	case DEVID_FAB:
		if (nbytes != 0)
			return (DDI_FAILURE);
		if (id != NULL)
			return (DDI_FAILURE);
		nbytes = sizeof (int) + sizeof (struct timeval32);
		sz += nbytes;
		break;
	default:
		return (DDI_FAILURE);
	}

	if ((i_devid = kmem_zalloc(sz, KM_SLEEP)) == NULL)
		return (DDI_FAILURE);

	i_devid->did_magic_hi = DEVID_MAGIC_MSB;
	i_devid->did_magic_lo = DEVID_MAGIC_LSB;
	i_devid->did_rev_hi = DEVID_REV_MSB;
	i_devid->did_rev_lo = DEVID_REV_LSB;
	DEVID_FORMTYPE(i_devid, devid_type);
	DEVID_FORMLEN(i_devid, nbytes);

	/* Fill in driver name hint */
	driver_name = ddi_binding_name(dip);
	driver_len = strlen(driver_name);
	if (driver_len > DEVID_HINT_SIZE) {
		/* Pick up last four characters of driver name */
		driver_name += driver_len - DEVID_HINT_SIZE;
		driver_len = DEVID_HINT_SIZE;
	}

	bcopy(driver_name, i_devid->did_driver, driver_len);

	/* Fill in id field */
	if (devid_type == DEVID_FAB) {
		char		*cp;
		int		hostid;
		char		*hostid_cp = &hw_serial[0];
		struct timeval32 timestamp32;
		int		i;
		int		*ip;

		cp = i_devid->did_id;

		/* Fill in host id (big-endian byte ordering) */
		hostid = stoi(&hostid_cp);
		*cp++ = hibyte(hiword(hostid));
		*cp++ = lobyte(hiword(hostid));
		*cp++ = hibyte(loword(hostid));
		*cp++ = lobyte(loword(hostid));

		/*
		 * Fill in timestamp (big-endian byte ordering)
		 *
		 * (Note that the format may have to be changed
		 * before 2038 comes around, though it's arguably
		 * unique enough as it is..)
		 */
		uniqtime32(&timestamp32);
		ip = (int *)&timestamp32;
		for (i = 0;
		    i < sizeof (timestamp32) / sizeof (int); i++, ip++) {
			int	val;
			val = *ip;
			*cp++ = hibyte(hiword(val));
			*cp++ = lobyte(hiword(val));
			*cp++ = hibyte(loword(val));
			*cp++ = lobyte(loword(val));
		}
	} else
		bcopy(id, i_devid->did_id, nbytes);

	/* return device id */
	*ret_devid = (ddi_devid_t)i_devid;
	return (DDI_SUCCESS);
}

/*
 * return size of device id in bytes
 */
size_t
ddi_devid_sizeof(ddi_devid_t devid)
{
	impl_devid_t	*id = (impl_devid_t *)devid;

	return (sizeof (*id) + DEVID_GETLEN(id) - sizeof (char));
}

/*
 * free device id
 */
void
ddi_devid_free(ddi_devid_t devid)
{
	ASSERT(devid != NULL);
	kmem_free(devid, ddi_devid_sizeof(devid));
}

/*
 * Compare device ids.
 * Ignore driver name hint information.
 */
int
ddi_devid_compare(ddi_devid_t id1, ddi_devid_t id2)
{
	uchar_t		*cp1 = (uchar_t *)id1;
	uchar_t		*cp2 = (uchar_t *)id2;
	int		len1 = ddi_devid_sizeof(id1);
	int		i;
	impl_devid_t	*id = (impl_devid_t *)id1;
	int		skip_offset;

	/*
	 * The driver name is not a part of the equality
	 */
	skip_offset = ((uintptr_t)&id->did_driver[0]) - (uintptr_t)id;

	/*
	 * The length is part if the ddi_devid_t,
	 * so if they are different sized ddi_devid_t's then
	 * the loop will stop before we run off the end of
	 * one of the device id's.
	 */
	i = 0;
	while (i < len1) {
		int diff;

		if (i == skip_offset) {
			i += DEVID_HINT_SIZE;
			continue;
		}

		diff = cp1[i] - cp2[i];
		if (diff < 0)
			return (-1);
		if (diff > 0)
			return (1);
		i++;
	}
	return (0);
}

/*
 * Return a copy of the device id for dev_t
 */
int
ddi_lyr_get_devid(dev_t dev, ddi_devid_t *ret_devid)
{
	dev_info_t	*dip;
	major_t		major = getmajor(dev);
	size_t		alloc_sz, sz;

	if ((dip = e_ddi_get_dev_info(dev, VCHR)) == NULL)
		return (DDI_FAILURE);

	mutex_enter(&(DEVI(dip)->devi_lock));
	if (DEVI(dip)->devi_devid == NULL) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		ddi_rele_driver(major);	/* held by e_ddi_get_dev_info() */
		return (DDI_FAILURE);
	}

	/* make a copy */
	alloc_sz = ddi_devid_sizeof(DEVI(dip)->devi_devid);
retry:
	/* drop lock to allocate memory */
	mutex_exit(&(DEVI(dip)->devi_lock));
	*ret_devid = kmem_alloc(alloc_sz, KM_SLEEP);
	mutex_enter(&(DEVI(dip)->devi_lock));

	/* re-check things, since we dropped the lock */
	if (DEVI(dip)->devi_devid == NULL) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		kmem_free(*ret_devid, alloc_sz);
		*ret_devid = NULL;
		ddi_rele_driver(major);	/* held by e_ddi_get_dev_info() */
		return (DDI_FAILURE);
	}

	/* verify size is the same */
	sz = ddi_devid_sizeof(DEVI(dip)->devi_devid);
	if (alloc_sz != sz) {
		kmem_free(*ret_devid, alloc_sz);
		alloc_sz = sz;
		goto retry;
	}

	/* sz == alloc_sz - make a copy */
	bcopy(DEVI(dip)->devi_devid, *ret_devid, sz);

	mutex_exit(&(DEVI(dip)->devi_lock));
	ddi_rele_driver(major);	/* held by e_ddi_get_dev_info() */
	return (DDI_SUCCESS);
}

static char *
ddi_get_minor_name_common(dev_info_t *dip, dev_t dev, int spec_type)
{
	struct ddi_minor_data	*dmdp;

	ASSERT(MUTEX_HELD(&(DEVI(dip)->devi_lock)));

	for (dmdp = DEVI(dip)->devi_minor; dmdp; dmdp = dmdp->next) {

		if (dmdp->type != DDM_MINOR)
			continue;
		if (dmdp->ddm_dev != dev)
			continue;
		if (dmdp->ddm_spec_type != spec_type)
			continue;
		return (dmdp->ddm_name);
	}
	return (NULL);
}

/*
 * Return a copy of the minor name for dev_t and spec_type
 */
int
ddi_lyr_get_minor_name(dev_t dev, int spec_type, char **minor_name)
{
	dev_info_t	*dip;
	major_t		major = getmajor(dev);
	char		*nm;
	size_t		alloc_sz, sz;

	if ((dip = e_ddi_get_dev_info(dev, VCHR)) == NULL)
		return (DDI_FAILURE);

	mutex_enter(&(DEVI(dip)->devi_lock));

	if ((nm = ddi_get_minor_name_common(dip, dev, spec_type)) == NULL) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		ddi_rele_driver(major);	/* held by e_ddi_get_dev_info() */
		return (DDI_FAILURE);
	}

	/* make a copy */
	alloc_sz = strlen(nm) + 1;
retry:
	/* drop lock to allocate memory */
	mutex_exit(&(DEVI(dip)->devi_lock));
	*minor_name = kmem_alloc(alloc_sz, KM_SLEEP);
	mutex_enter(&(DEVI(dip)->devi_lock));

	/* re-check things, since we dropped the lock */
	if ((nm = ddi_get_minor_name_common(dip, dev, spec_type)) == NULL) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		kmem_free(*minor_name, alloc_sz);
		*minor_name = NULL;
		ddi_rele_driver(major);	/* held by e_ddi_get_dev_info() */
		return (DDI_FAILURE);
	}

	/* verify size is the same */
	sz = strlen(nm) + 1;
	if (alloc_sz != sz) {
		kmem_free(*minor_name, alloc_sz);
		alloc_sz = sz;
		goto retry;
	}

	/* sz == alloc_sz - make a copy */
	(void) strcpy(*minor_name, nm);

	mutex_exit(&(DEVI(dip)->devi_lock));
	ddi_rele_driver(major);	/* held by e_ddi_get_dev_info() */
	return (DDI_SUCCESS);
}


#define	GETDEVS	(1)
#define	CNTDEVS	(0)

static int
ddi_common_devid_to_devlist(
	struct devnames	*dnp,		/* Driver to search */
	ddi_devid_t	devid,		/* device id to match */
	char		*minor_name,	/* minor name to match */
	int		get_devs,	/* Boolean, get or count dev_t's */
	int		ndev,		/* # dev_t in array, get_devs only */
	dev_t		*devs)		/* dev_t array, get_devs only */
{
	dev_info_t		*dip;
	struct ddi_minor_data	*dmdp;
	int			i = 0;
	int			dev_count = 0;
	major_t			major;

	major = ddi_name_to_major(dnp->dn_name);
	ASSERT(major != (major_t)-1);
	ASSERT(DEV_OPS_HELD(devopsp[major]));

	for (dip = dnp->dn_head; dip != NULL; dip = ddi_get_next(dip)) {
		if (!DDI_CF2(dip))
			continue;

		mutex_enter(&(DEVI(dip)->devi_lock));

		if (DEVI(dip)->devi_devid == NULL) {
			mutex_exit(&(DEVI(dip)->devi_lock));
			continue;
		}

		/* Does device id match */
		if (ddi_devid_compare(DEVI(dip)->devi_devid, devid)
		    != DDI_SUCCESS) {
			mutex_exit(&(DEVI(dip)->devi_lock));
			continue;
		}

		/* Find a matching minor name */
		for (dmdp = DEVI(dip)->devi_minor; dmdp; dmdp = dmdp->next) {

			if (dmdp->type != DDM_MINOR)
				continue;

			if (strcmp(dmdp->ddm_name, minor_name) != 0)
				continue;

			dev_count++;

			if (get_devs == GETDEVS)
				if (i < ndev)
					devs[i++] = dmdp->ddm_dev;
		}

		mutex_exit(&(DEVI(dip)->devi_lock));
	}

	return (dev_count);
}

static int
ddi_major_devid_to_devlist(
	major_t		major,
	ddi_devid_t	devid,
	char		*minor_name,
	int		*retndevs,
	dev_t		**retdevs)
{
	struct dev_ops	*ops;
	struct devnames	*dnp;
	int		cnt, real_cnt;
	int		alreadyloaded;
	int		error = DDI_FAILURE;
	int		circular;

	/* Hold/install the driver */
	alreadyloaded = CB_DRV_INSTALLED(devopsp[major]);
	if ((ops = ddi_hold_installed_driver(major)) == NULL)
		return (DDI_FAILURE);

	/* If not attached then bail */
	dnp = &(devnamesp[major]);
	if ((!CB_DRV_INSTALLED(ops)) || (ops->devo_getinfo == NULL) ||
	    ((dnp->dn_flags & DN_DEVS_ATTACHED) == 0)) {
		ddi_rele_driver(major);
		return (DDI_FAILURE);
	}

	if (alreadyloaded)
		(void) e_ddi_deferred_attach(major, NODEV);

	/* Single thread the devinfo list walking */
	e_ddi_enter_driver_list(dnp, &circular);

again:
	/* Count the number of dev_t's */
	cnt = ddi_common_devid_to_devlist(dnp, devid, minor_name,
	    CNTDEVS, 0, NULL);
	if (cnt == 0)
		goto out;

	/* Allocate the dev_t array */
	*retdevs = kmem_alloc(sizeof (dev_t) * cnt, KM_SLEEP);

	/* Get the dev_t's */
	real_cnt = ddi_common_devid_to_devlist(dnp, devid, minor_name,
	    GETDEVS, cnt, *retdevs);

	/*
	 * If counts do not match
	 *    then device id's were registered/unregistered
	 *    between the CNTDEV and GETDEV; in that case re-try
	 */
	if (real_cnt != cnt) {
		kmem_free(*retdevs, sizeof (dev_t) * cnt);
		*retdevs = NULL;
		goto again;
	}

	/* Sucess - return the dev_t array and count */
	*retndevs = cnt;
	error = DDI_SUCCESS;
out:
	e_ddi_exit_driver_list(dnp, circular);

	ddi_rele_driver(major);
	return (error);
}

static int
ddi_hint_devid_to_devlist(
	char		*hint,
	ddi_devid_t	devid,
	char		*minor_name,
	int		*retndevs,
	dev_t		**retdevs)
{
	int	i, len;
	char	*name;

	for (i = 0; i < devcnt; i++) {

		/* Is there a driver name? */
		if ((name = devnamesp[i].dn_name) == NULL || *name == '\0')
			continue;

		/* Ignore driver names that are not larger than 4 chars */
		if ((len = strlen(name)) <= DEVID_HINT_SIZE)
			continue;

		/* Are last four characters the same? */
		if (bcmp(&name[len - DEVID_HINT_SIZE], hint,
		    DEVID_HINT_SIZE) != 0)
			continue;

		if (ddi_major_devid_to_devlist((major_t)i, devid,
		    minor_name, retndevs, retdevs) == DDI_SUCCESS)
			return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

static int
ddi_all_drivers_devid_to_devlist(
	ddi_devid_t	devid,
	char		*minor_name,
	int		*retndevs,
	dev_t		**retdevs)
{
	int	i;
	char	*name;

	for (i = 0; i < devcnt; i++) {

		/* Is there a driver name? */
		if ((name = devnamesp[i].dn_name) == NULL || *name == '\0')
			continue;

		if (ddi_major_devid_to_devlist((major_t)i, devid, minor_name,
		    retndevs, retdevs) == DDI_SUCCESS)
			return (DDI_SUCCESS);
	}

	return (DDI_FAILURE);
}

int
ddi_lyr_devid_to_devlist(
	ddi_devid_t	devid,
	char		*minor_name,
	int		*retndevs,
	dev_t		**retdevs)
{
	impl_devid_t	*id = (impl_devid_t *)devid;
	major_t		major;
	int		i, len;
	int		err;
	char		hint[DEVID_HINT_SIZE + 1];

	/* Count non-null bytes */
	for (i = 0; i < DEVID_HINT_SIZE; i++)
		if (id->did_driver[i] == '\0')
			break;

	/* Make a copy of the driver hint */
	bcopy(id->did_driver, hint, i);
	hint[i] = '\0';
	len = i;

	major = ddi_name_to_major(hint);
	if (major != (major_t)-1) {
		if (ddi_major_devid_to_devlist(major, devid, minor_name,
		    retndevs, retdevs) == DDI_SUCCESS)
			return (DDI_SUCCESS);
	}

	/*
	 * If hint is exactly four characters
	 *    then load/search all drivers which end in
	 *    same last four characters
	 */
	if (len == DEVID_HINT_SIZE) {
		if (ddi_hint_devid_to_devlist(hint, devid, minor_name,
		    retndevs, retdevs) == DDI_SUCCESS)
			return (DDI_SUCCESS);
	}

	/*
	 * If all else fails,
	 *    then load/search all the drivers
	 */
	err = ddi_all_drivers_devid_to_devlist(devid, minor_name,
	    retndevs, retdevs);
	return (err);
}

void
ddi_lyr_free_devlist(dev_t *devlist, int ndevs)
{
	kmem_free(devlist, sizeof (dev_t) * ndevs);
}

/*
 * Note: This will need to be fixed if we ever allow processes to
 * have more than one data model per exec.
 */
model_t
ddi_mmap_get_model(void)
{
	return (get_udatamodel());
}

model_t
ddi_model_convert_from(model_t model)
{
	return ((model & DDI_MODEL_MASK) & ~DDI_MODEL_NATIVE);
}

/*
 * ddi interfaces managing storage and retrieval of eventcookies.
 */

/*
 * Invoke bus nexus driver's implementation of the
 * (*bus_remove_eventcall)() interface to remove a registered
 * callback handler for "event".
 */
int
ddi_remove_eventcall(dev_info_t *dip, ddi_eventcookie_t event)
{
	return (ndi_busop_remove_eventcall(dip, dip, event));
}

/*
 * Invoke bus nexus driver's implementation of the
 * (*bus_add_eventcall)() interface to register a callback handler
 * for "event".
 */
int
ddi_add_eventcall(dev_info_t *dip, ddi_eventcookie_t event,
    int (*handler)(dev_info_t *, ddi_eventcookie_t, void *, void *),
    void *arg)
{
	return (ndi_busop_add_eventcall(dip, dip, event, handler, arg));
}


/*
 * Return a handle for event "name" by calling up the device tree
 * hierarchy via  (*bus_get_eventcookie)() interface until claimed
 * by a bus nexus or top of dev_info tree is reached.
 */
int
ddi_get_eventcookie(dev_info_t *dip, char *name,
    ddi_eventcookie_t *event_cookiep, ddi_plevel_t *plevelp,
    ddi_iblock_cookie_t *iblock_cookiep)
{
	return (ndi_busop_get_eventcookie(dip, dip,
	    name, event_cookiep, plevelp, iblock_cookiep));
}

/*
 * single thread access to dev_info node and set state
 */
void
i_devi_enter(dev_info_t *dip, uint_t s_mask, uint_t w_mask, int has_lock)
{
	if (!has_lock)
		mutex_enter(&(DEVI(dip)->devi_lock));

	ASSERT(mutex_owned(&(DEVI(dip)->devi_lock)));

	/*
	 * wait until state(s) have been changed
	 */
	while ((DEVI(dip)->devi_state & w_mask) != 0) {
		cv_wait(&(DEVI(dip)->devi_cv), &(DEVI(dip)->devi_lock));
	}
	DEVI(dip)->devi_state |= s_mask;

	if (!has_lock)
		mutex_exit(&(DEVI(dip)->devi_lock));
}

void
i_devi_exit(dev_info_t *dip, uint_t c_mask, int has_lock)
{
	if (!has_lock)
		mutex_enter(&(DEVI(dip)->devi_lock));

	ASSERT(mutex_owned(&(DEVI(dip)->devi_lock)));

	/*
	 * clear the state(s) and wakeup any threads waiting
	 * for state change
	 */
	DEVI(dip)->devi_state &= ~c_mask;
	cv_broadcast(&(DEVI(dip)->devi_cv));

	if (!has_lock)
		mutex_exit(&(DEVI(dip)->devi_lock));
}

/*
 * Clustering: Return the base (without the node-specific entry) of the
 * device-specific path for global devices.
 */
const char *
i_ddi_get_dpath_base()
{
	if (cluster_bootflags & CLUSTER_CONFIGURED) {
		return (CLUSTER_DEVICE_BASE);
	} else {
		return ("");
	}

}

/*
 * Clustering: Return the full (including node-specific entry)
 * device-specific path for global devices.
 */
const char *
i_ddi_get_dpath_prefix()
{
	if ((cluster_bootflags & CLUSTER_CONFIGURED) &&
	    (clconf_get_nodeid() > 0)) {
		static char path_prefix[CLUSTER_DEVICE_BASE_LEN + 4] = {NULL};

		if (path_prefix[0] == NULL) {
			(void) sprintf(path_prefix, "%s%d",
			    i_ddi_get_dpath_base(), clconf_get_nodeid());
		}
		return (path_prefix);
	} else {
		return ("");
	}
}

nodeid_t
ddi_get_dev_nodeid(major_t major, int instance)
{
	char *path = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	nodeid_t nnum;

	if (DC_INSTANCE_PATH(&dcops, major, path, instance)) {
		nnum = NODEID_UNKNOWN;
	} else {
		char *pptr = path;

		nnum = 0;
		pptr += 5; /* skip '/node' */
		while (*++pptr != '/') {
			if ('0' <= *pptr && *pptr <= '9') {
				nnum = nnum * 10 + *pptr - '0';
			} else {
				/*
				 * Not a number?
				 */
				nnum = NODEID_UNKNOWN;
				break;
			}
		}
	}
	kmem_free(path, MAXPATHLEN);
	return (nnum);
}

/*
 * Lock the virtual address range in the current process and create a
 * ddi_umem_cookie (of type UMEM_LOCKED). This can be used to pass to
 * ddi_umem_iosetup to create a buf or do devmap_umem_setup/remap to export
 * to user space.
 *
 * addr, size should be PAGESIZE aligned
 * flags - DDI_UMEMLOCK_READ, DDI_UMEMLOCK_WRITE or both
 *	identifies whether the locked memory will be read or written or both
 *
 * Returns 0 on success
 * 	EINVAL - for invalid parameters
 *	EPERM, ENOMEM and other error codes returned by as_pagelock
 */
int
ddi_umem_lock(caddr_t addr, size_t len, int flags, ddi_umem_cookie_t *cookie)
{
	int	error;
	struct ddi_umem_cookie *p;

	*cookie = NULL;		/* in case of any error return */

	/* These are the only two valid flags */
	if ((flags & ~(DDI_UMEMLOCK_READ | DDI_UMEMLOCK_WRITE)) != 0) {
		return (EINVAL);
	}

	/* At least one of the two flags (or both) must be set */
	if ((flags & (DDI_UMEMLOCK_READ | DDI_UMEMLOCK_WRITE)) == 0) {
		return (EINVAL);
	}

	/* addr and len must be page-aligned */
	if (((uintptr_t)addr & PAGEOFFSET) != 0) {
		return (EINVAL);
	}

	if ((len & PAGEOFFSET) != 0) {
		return (EINVAL);
	}

	/* Allocate memory for the cookie */
	p = kmem_zalloc(sizeof (struct ddi_umem_cookie), KM_SLEEP);

	/* Convert the flags to seg_rw type */
	if (flags & DDI_UMEMLOCK_WRITE) {
		p->s_flags = S_WRITE;
	} else {
		p->s_flags = S_READ;
	}

	/* Store curproc in cookie for later iosetup/unlock */
	p->procp = (void *)curproc;

	/* Lock the pages corresponding to addr, len in memory */
	error = as_pagelock(((proc_t *)p->procp)->p_as, &(p->pparray),
	    addr, len, p->s_flags);
	if (error != 0) {
		kmem_free(p, sizeof (struct ddi_umem_cookie));
		*cookie = (ddi_umem_cookie_t)NULL;
		return (error);
	}

	/* Initialize the fields in the ddi_umem_cookie */
	p->kvaddr = addr;
	p->size = len;
	p->type = UMEM_LOCKED;

	*cookie = (ddi_umem_cookie_t)p;
	return (error);
}

/*
 * Unlock the pages locked by ddi_umem_lock and free the cookie
 */
void
ddi_umem_unlock(ddi_umem_cookie_t cookie)
{
	struct ddi_umem_cookie *p = (struct ddi_umem_cookie *)cookie;

	ASSERT(p->type == UMEM_LOCKED);

	as_pageunlock(((proc_t *)p->procp)->p_as, p->pparray,
	    p->kvaddr, p->size, p->s_flags);
	kmem_free(p, sizeof (struct ddi_umem_cookie));
}

/*
 * Create a buf structure from a ddi_umem_cookie
 * cookie - is a ddi_umem_cookie for from ddi_umem_lock and ddi_umem_alloc
 *		(only UMEM_LOCKED & KMEM_NON_PAGEABLE types supported)
 * off, len - identifies the portion of the memory represented by the cookie
 *		that the buf points to.
 *	NOTE: off, len need to follow the alignment/size restrictions of the
 *		device (dev) that this buf will be passed to. Some devices
 *		will accept unrestricted alignment/size, whereas others (such as
 *		st) require some block-size alignment/size. It is the caller's
 *		responsibility to ensure that the alignment/size restrictions
 *		are met (we cannot assert as we do not know the restrictions)
 *
 * direction - is one of B_READ or B_WRITE and needs to be compatible with
 *		the flags used in ddi_umem_lock
 *
 * The following three arguments are used to initialize fields in the
 * buf structure and are uninterpreted by this routine.
 *
 * dev
 * blkno
 * iodone
 *
 * sleepflag - is one of DDI_UMEM_SLEEP or DDI_UMEM_NOSLEEP
 *
 * Returns a buf structure pointer on success (to be freed by freerbuf)
 *	NULL on any parameter error or memory alloc failure
 *
 */
struct buf *
ddi_umem_iosetup(ddi_umem_cookie_t cookie, off_t off, size_t len,
	int direction, dev_t dev, daddr_t blkno,
	int (*iodone)(struct buf *), int sleepflag)
{
	struct ddi_umem_cookie *p = (struct ddi_umem_cookie *)cookie;
	struct buf *bp;

	/*
	 * check for valid cookie offset, len
	 */
	if ((off + len) > p->size) {
		return (NULL);
	}

	if (len > p->size) {
		return (NULL);
	}

	/* direction has to be one of B_READ or B_WRITE */
	if ((direction != B_READ) && (direction != B_WRITE)) {
		return (NULL);
	}

	/* These are the only two valid sleepflags */
	if ((sleepflag != DDI_UMEM_SLEEP) && (sleepflag != DDI_UMEM_NOSLEEP)) {
		return (NULL);
	}

	/*
	 * Only cookies of type UMEM_LOCKED and KMEM_NON_PAGEABLE are supported
	 */
	if ((p->type != UMEM_LOCKED) && (p->type != KMEM_NON_PAGEABLE)) {
		return (NULL);
	}

	/* If type is KMEM_NON_PAGEABLE procp is NULL */
	ASSERT((p->type == KMEM_NON_PAGEABLE) ?
		(p->procp == NULL) : (p->procp != NULL));

	bp = kmem_alloc(sizeof (struct buf), sleepflag);
	if (bp == NULL) {
		return (NULL);
	}
	bioinit(bp);

	bp->b_flags = B_KERNBUF | B_BUSY | B_PHYS | direction;
	bp->b_edev = dev;
	bp->b_lblkno = blkno;
	bp->b_iodone = iodone;
	bp->b_bcount = len;
	bp->b_proc = (proc_t *)p->procp;
	ASSERT(((uintptr_t)(p->kvaddr) & PAGEOFFSET) == 0);
	bp->b_un.b_addr = (caddr_t)((uintptr_t)(p->kvaddr) + off);
	if (p->pparray != NULL) {
		bp->b_flags |= B_SHADOW;
		ASSERT(((uintptr_t)(p->kvaddr) & PAGEOFFSET) == 0);
		bp->b_shadow = p->pparray + btop(off);
	}
	return (bp);
}

/*
 * Fault-handling and related routines
 */

ddi_devstate_t
ddi_get_devstate(dev_info_t *dip)
{
	if (DEVI_IS_DEVICE_OFFLINE(dip))
		return (DDI_DEVSTATE_OFFLINE);
	else if (DEVI_IS_DEVICE_DOWN(dip) || DEVI_IS_BUS_DOWN(dip))
		return (DDI_DEVSTATE_DOWN);
	else if (DEVI_IS_BUS_QUIESCED(dip))
		return (DDI_DEVSTATE_QUIESCED);
	else if (DEVI_IS_DEVICE_DEGRADED(dip))
		return (DDI_DEVSTATE_DEGRADED);
	else
		return (DDI_DEVSTATE_UP);
}

void
ddi_dev_report_fault(dev_info_t *dip, ddi_fault_impact_t impact,
	ddi_fault_location_t location, const char *message)
{
	struct ddi_fault_event_data fd;
	ddi_eventcookie_t ec;

	/*
	 * Assemble all the information into a fault-event-data structure
	 */
	fd.f_dip = dip;
	fd.f_impact = impact;
	fd.f_location = location;
	fd.f_message = message;
	fd.f_oldstate = ddi_get_devstate(dip);

	/*
	 * Tell the world about it ...
	 */
	ec = ndi_event_getcookie(DDI_DEVI_FAULT_EVENT);
	if (ndi_post_event(dip, dip, ec, &fd) == DDI_FAILURE) {
		/*
		 * If the event couldn't be posted normally (presumably due
		 * to some intermediate nexus not supporting the event-related
		 * entry points), try to post it directly to rootnex ...
		 */
		(void) i_ddi_rootnex_post_event(ddi_root_node(), dip, ec, &fd);
	}
}
