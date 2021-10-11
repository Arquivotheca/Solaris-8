/*
 * Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mem.c	1.59	99/09/16 SMI"

/*
 * Memory special file
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/vm.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <sys/stat.h>
#include <sys/vmem.h>
#include <sys/memlist.h>
#include <sys/bootconf.h>

#include <vm/seg_vn.h>
#include <vm/seg_dev.h>
#include <vm/seg_kmem.h>
#include <vm/hat.h>

#include <sys/conf.h>
#include <sys/mem.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/memlist.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/debug.h>

/*
 * Turn a byte length into a pagecount.  The DDI btop takes a
 * 32-bit size on 32-bit machines, this handles 64-bit sizes for
 * large physical-memory 32-bit machines.
 */
#define	BTOP(x)	((pgcnt_t)((x) >> _pageshift))

static kmutex_t mm_lock;
static caddr_t mm_map;

static dev_info_t *mm_dip;	/* private copy of devinfo pointer */

/*ARGSUSED1*/
static int
mm_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int i;
	struct mem_minor {
		char *name;
		minor_t minor;
		int class;
	} mm[] = {
		{ "mem",	M_MEM, NODESPECIFIC_DEV},
		{ "kmem",	M_KMEM, NODESPECIFIC_DEV},
		{ "null",	M_NULL, GLOBAL_DEV},
		{ "zero",	M_ZERO, GLOBAL_DEV},
	};

	mutex_init(&mm_lock, NULL, MUTEX_DEFAULT, NULL);
	mm_map = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	for (i = 0; i < (sizeof (mm) / sizeof (mm[0])); i++) {
		if (ddi_create_minor_node(devi, mm[i].name, S_IFCHR,
		    mm[i].minor, NULL, mm[i].class) == DDI_FAILURE) {
			ddi_remove_minor_node(devi, NULL);
			return (DDI_FAILURE);
		}
	}

	mm_dip = devi;
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
mm_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)mm_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*ARGSUSED1*/
static int
mmopen(dev_t *devp, int flag, int typ, struct cred *cred)
{
	switch (getminor(*devp)) {
	case M_MEM:
	case M_KMEM:
	case M_NULL:
	case M_ZERO:
		/* standard devices */
		break;

	default:
		/* Unsupported or unknown type */
		return (EINVAL);
	}
	return (0);
}

struct pollhead	mm_pollhd;

/*ARGSUSED*/
static int
mmchpoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp)
{
	switch (getminor(dev)) {
	case M_NULL:
	case M_ZERO:
	case M_MEM:
	case M_KMEM:
		*reventsp = events & (POLLIN | POLLOUT | POLLPRI | POLLRDNORM |
			POLLWRNORM | POLLRDBAND | POLLWRBAND);
		/*
		 * A non NULL pollhead pointer should be returned in case
		 * user polls for 0 events.
		 */
		*phpp = !anyyet && !*reventsp ?
		    &mm_pollhd : (struct pollhead *)NULL;
		return (0);
	default:
		/* no other devices currently support polling */
		return (ENXIO);
	}
}

static int
mmio(struct uio *uio, enum uio_rw rw, pfn_t pfn, off_t pageoff)
{
	int error;
	size_t nbytes = MIN((size_t)(PAGESIZE - pageoff),
	    (size_t)uio->uio_iov->iov_len);

	mutex_enter(&mm_lock);
	hat_devload(kas.a_hat, mm_map, PAGESIZE, pfn,
	    (uint_t)(rw == UIO_READ ? PROT_READ : PROT_READ | PROT_WRITE),
	    HAT_LOAD_NOCONSIST | HAT_LOAD_LOCK);
	error = uiomove(&mm_map[pageoff], nbytes, rw, uio);
	hat_unload(kas.a_hat, mm_map, PAGESIZE, HAT_UNLOAD_UNLOCK);
	mutex_exit(&mm_lock);
	return (error);
}

/*ARGSUSED3*/
static int
mmrw(dev_t dev, struct uio *uio, enum uio_rw rw, cred_t *cred)
{
	pfn_t v;
	struct iovec *iov;
	int error = 0;
	void *zpage = NULL;
	size_t c;
	ssize_t oresid = uio->uio_resid;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}
		switch (getminor(dev)) {

		case M_MEM:
			memlist_read_lock();
			if (!address_in_memlist(phys_install,
			    uio->uio_loffset, 1)) {
				memlist_read_unlock();
				error = EFAULT;
				break;
			}
			memlist_read_unlock();

			v = BTOP(uio->uio_loffset);
			error = mmio(uio, rw, v, uio->uio_loffset & PAGEOFFSET);
			break;

		case M_KMEM:
			v = hat_getkpfnum((caddr_t)uio->uio_offset);
			if (v == PFN_INVALID) {
				error = EFAULT;
				break;
			}
			if (!pf_is_memory(v)) {
				c = iov->iov_len;
				if (ddi_peekpokeio((dev_info_t *)0, uio,
				    rw, (caddr_t)uio->uio_offset, c,
				    sizeof (int32_t)) != DDI_SUCCESS)
					error = EFAULT;
				break;
			}
			error = mmio(uio, rw, v, uio->uio_offset & PAGEOFFSET);
			break;

		case M_ZERO:
			if (zpage == NULL)
				zpage = kmem_zalloc(PAGESIZE, KM_SLEEP);
			if (rw == UIO_READ) {
				c = MIN(iov->iov_len, PAGESIZE);
				error = uiomove(zpage, c, rw, uio);
				break;
			}
			/* else it's a write, fall through to NULL case */
			/*FALLTHROUGH*/

		case M_NULL:
			if (rw == UIO_READ)
				return (0);
			c = iov->iov_len;
			iov->iov_base += c;
			iov->iov_len -= c;
			uio->uio_offset += c;
			uio->uio_resid -= c;
			break;

		}
	}
	if (zpage != NULL)
		kmem_free(zpage, PAGESIZE);
	return (uio->uio_resid == oresid ? error : 0);
}

static int
mmread(dev_t dev, struct uio *uio, cred_t *cred)
{
	return (mmrw(dev, uio, UIO_READ, cred));
}

static int
mmwrite(dev_t dev, struct uio *uio, cred_t *cred)
{
	return (mmrw(dev, uio, UIO_WRITE, cred));
}

/*
 * Private ioctl for libkvm to support kvm_physaddr().
 * Given an address space and a VA, compute the PA.
 */
/*ARGSUSED*/
static int
mmioctl(dev_t dev, int cmd, intptr_t data, int flag, cred_t *cred, int *rvalp)
{
	mem_vtop_t mem_vtop;
	proc_t *p;
	pfn_t pfn = (pfn_t)PFN_INVALID;
	pid_t pid = 0;
	struct as *as;
	struct seg *seg;

	if (getminor(dev) != M_KMEM || cmd != MEM_VTOP)
		return (ENXIO);

	if (copyin((void *)data, &mem_vtop, sizeof (mem_vtop_t)))
		return (EFAULT);
	if (mem_vtop.m_as == &kas) {
		pfn = hat_getkpfnum(mem_vtop.m_va);
	} else if (mem_vtop.m_as == NULL) {
		return (EIO);
	} else {
		mutex_enter(&pidlock);
		for (p = practive; p != NULL; p = p->p_next) {
			if (p->p_as == mem_vtop.m_as) {
				pid = p->p_pid;
				break;
			}
		}
		mutex_exit(&pidlock);
		if (p == NULL)
			return (EIO);
		p = sprlock(pid);
		if (p == NULL)
			return (EIO);
		as = p->p_as;
		if (as == mem_vtop.m_as) {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
			    seg = AS_SEGP(as, seg->s_next))
				if ((uintptr_t)mem_vtop.m_va -
				    (uintptr_t)seg->s_base < seg->s_size)
					break;
			if (seg != NULL)
				pfn = hat_getpfnum(as->a_hat, mem_vtop.m_va);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		sprunlock(p);
	}
	mem_vtop.m_pfn = pfn;
	if (pfn == PFN_INVALID)
		return (EIO);
	if (copyout(&mem_vtop, (void *)data, sizeof (mem_vtop_t)))
		return (EFAULT);
	return (0);
}

/*ARGSUSED2*/
static int
mmmmap(dev_t dev, off_t off, int prot)
{
	pfn_t pf;
	struct memlist *pmem;

	switch (getminor(dev)) {
	case M_MEM:
		pf = btop(off);
		memlist_read_lock();
		for (pmem = phys_install;
		    pmem != (struct memlist *)NULL; pmem = pmem->next) {
			if (pf >= BTOP(pmem->address) &&
			    pf < BTOP(pmem->address + pmem->size)) {
				memlist_read_unlock();
				return (impl_obmem_pfnum(pf));
			}
		}
		memlist_read_unlock();
		break;

	case M_KMEM:
		if ((pf = hat_getkpfnum((caddr_t)off)) == PFN_INVALID)
			break;
		if (pf > (pfn_t)UINT_MAX)
			break;
		return ((int)pf);

	case M_ZERO:
		/*
		 * We shouldn't be mmap'ing to /dev/zero here as
		 * mmsegmap() should have already converted
		 * a mapping request for this device to a mapping
		 * using seg_vn for anonymous memory.
		 */
		break;

	}
	return (-1);
}

/*
 * This function is called when a memory device is mmap'ed.
 * Set up the mapping to the correct device driver.
 */
static int
mmsegmap(dev_t dev, off_t off, struct as *as, caddr_t *addrp, off_t len,
    uint_t prot, uint_t maxprot, uint_t flags, struct cred *cred)
{
	struct segvn_crargs vn_a;
	struct segdev_crargs dev_a;
	int error;
	minor_t minor;
	off_t i;

	minor = getminor(dev);

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		/*
		 * No need to worry about vac alignment on /dev/zero
		 * since this is a "clone" object that doesn't yet exist.
		 */
		map_addr(addrp, len, (offset_t)off,
				(minor == M_MEM) || (minor == M_KMEM), flags);

		if (*addrp == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/*
		 * User specified address -
		 * Blow away any previous mappings.
		 */
		(void) as_unmap(as, *addrp, len);
	}

	switch (minor) {
	case M_MEM:
		/* /dev/mem cannot be mmap'ed with MAP_PRIVATE */
		if ((flags & MAP_TYPE) != MAP_SHARED) {
			as_rangeunlock(as);
			return (EINVAL);
		}

		/*
		 * Check to ensure that the entire range is
		 * legal and we are not trying to map in
		 * more than the device will let us.
		 */
		for (i = 0; i < len; i += PAGESIZE) {
			if (mmmmap(dev, off + i, maxprot) == -1) {
				as_rangeunlock(as);
				return (ENXIO);
			}
		}

		/*
		 * Use seg_dev segment driver for /dev/mem mapping.
		 */
		dev_a.mapfunc = mmmmap;
		dev_a.dev = dev;
		dev_a.offset = off;
		dev_a.type = (flags & MAP_TYPE);
		dev_a.prot = (uchar_t)prot;
		dev_a.maxprot = (uchar_t)maxprot;
		dev_a.hat_attr = 0;

		/*
		 * Make /dev/mem mappings non-consistent since we can't
		 * alias pages that don't have page structs behind them,
		 * such as kernel stack pages. If someone mmap()s a kernel
		 * stack page and if we give him a tte with cv, a line from
		 * that page can get into both pages of the spitfire d$.
		 * But snoop from another processor will only invalidate
		 * the first page. This later caused kernel (xc_attention)
		 * to go into an infinite loop at pil 13 and no interrupts
		 * could come in. See 1203630.
		 *
		 */
		dev_a.hat_flags = HAT_LOAD_NOCONSIST;
		dev_a.devmap_data = NULL;

		error = as_map(as, *addrp, len, segdev_create, &dev_a);
		break;

	case M_ZERO:
		/*
		 * Use seg_vn segment driver for /dev/zero mapping.
		 * Passing in a NULL amp gives us the "cloning" effect.
		 */
		vn_a.vp = NULL;
		vn_a.offset = 0;
		vn_a.type = (flags & MAP_TYPE);
		vn_a.prot = prot;
		vn_a.maxprot = maxprot;
		vn_a.flags = flags & ~MAP_TYPE;
		vn_a.cred = cred;
		vn_a.amp = NULL;
		error = as_map(as, *addrp, len, segvn_create, &vn_a);
		break;

	case M_KMEM:
		/*
		 * if there isn't a page there, we fail the mmap with ENXIO
		 */
		for (i = 0; i < len; i += PAGESIZE)
			if ((hat_getkpfnum((caddr_t)off + i)) == -1UL) {
				as_rangeunlock(as);
				return (ENXIO);
			}
		/*
		 * Use seg_dev segment driver for /dev/kmem mapping.
		 */
		dev_a.mapfunc = mmmmap;
		dev_a.dev = dev;
		dev_a.offset = off;
		dev_a.type = (flags & MAP_TYPE);
		dev_a.prot = (uchar_t)prot;
		dev_a.maxprot = (uchar_t)maxprot;
		dev_a.hat_attr = 0;

		/*
		 * Make /dev/kmem mappings consistent unless it's MAP_FIXED.
		 *
		 * XXX - Change it to non-consistent when libdevinfo finally
		 * gets fixed. One could get into the same scenario as
		 * described in /dev/mem comments, if one mmaps segkp page
		 * and if that page gets freed/remapped to another kvaddr,
		 * which is not aligned with the earlier addr,it was mapped to.
		 */
		dev_a.hat_flags = (flags & MAP_FIXED) ? HAT_LOAD_NOCONSIST : 0;
		error = as_map(as, *addrp, len, segdev_create, &dev_a);
		break;

	case M_NULL:
		/*
		 * Use seg_dev segment driver for /dev/null mapping.
		 */
		dev_a.mapfunc = mmmmap;
		dev_a.dev = dev;
		dev_a.offset = off;
		dev_a.type = 0;		/* neither PRIVATE nor SHARED */
		dev_a.prot = dev_a.maxprot = (uchar_t)PROT_NONE;
		dev_a.hat_attr = 0;
		dev_a.hat_flags = 0;
		error = as_map(as, *addrp, len, segdev_create, &dev_a);
		break;

	default:
		error = ENXIO;
	}

	as_rangeunlock(as);
	return (error);
}

static struct cb_ops mm_cb_ops = {
	mmopen,			/* open */
	nulldev,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	mmread,			/* read */
	mmwrite,		/* write */
	mmioctl,		/* ioctl */
	nodev,			/* devmap */
	mmmmap,			/* mmap */
	mmsegmap,		/* segmap */
	mmchpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP | D_64BIT	/* Driver compatibility flag */
};

static struct dev_ops mm_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	mm_info,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	mm_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	&mm_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* bus operations */
};

static struct modldrv modldrv = {
	&mod_driverops, "memory driver", &mm_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}
