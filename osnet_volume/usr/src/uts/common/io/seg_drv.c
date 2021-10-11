/*
 * Copyright (c) 1992-1997, by Sun Microsytems, Inc.
 * All rights reserved.
 */

/*
 * VM - segment of a mapped device.
 *
 * This segment driver is used when mapping character special devices.
 */

#pragma ident	"@(#)seg_drv.c	1.36	98/08/06 SMI"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>

#include <vm/page.h>			/* for PP_SETREF, PP_SETMOD, etc */
#include <vm/hat.h>
#include <vm/vpage.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <sys/seg_drv.h>
#include <vm/pvn.h>
#include <sys/vmsystm.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/fs/snode.h>

/*
 * Private seg op routines.
 */
static int	segdrv_dup(struct seg *seg, struct seg *newsegp);
static int	segdrv_unmap(struct seg *seg, caddr_t addr, size_t len);
static void	segdrv_free(struct seg *seg);
static faultcode_t	segdrv_fault(struct hat *hat, struct seg *seg,
				caddr_t addr, size_t len, enum fault_type type,
				enum seg_rw rw);
static faultcode_t	segdrv_faulta(struct seg *seg, caddr_t addr);
static int	segdrv_setprot(struct seg *seg, caddr_t addr, size_t len,
			u_int prot);
static int	segdrv_checkprot(struct seg *seg, caddr_t addr, size_t len,
			u_int prot);
static int	segdrv_sync(struct seg *seg, caddr_t addr, size_t len,
			int attr, u_int flags);
static size_t	segdrv_incore(struct seg *seg, caddr_t addr, size_t len,
			char *vec);
static int	segdrv_lockop(struct seg *seg, caddr_t addr, size_t len,
			int attr, int op, ulong *bitmap, size_t pos);
static int	segdrv_getprot(struct seg *seg, caddr_t addr, size_t len,
			u_int *prot);
static u_offset_t	segdrv_getoffset(struct seg *seg, caddr_t addr);
static int	segdrv_gettype(struct seg *seg, caddr_t addr);
static int	segdrv_getvp(struct seg *seg, caddr_t addr,
			struct vnode **vpp);
static int	segdrv_advise(struct seg *seg, caddr_t addr, size_t len,
			u_int behav);
static void	segdrv_dump(struct seg *seg);
static int	segdrv_pagelock(struct seg *seg, caddr_t addr, size_t len,
			struct page ***ppp, enum lock_type type,
			enum seg_rw rw);
static int	segdrv_getmemid(struct seg *seg, caddr_t addr,
			memid_t *memidp);
static int	segdrv_badop();

/*
 * seg_dev comments:
 * XXX	this struct is used by rootnex_map_fault to identify
 *	the segment it has been passed. So if you make it
 *	"static" you'll need to fix rootnex_map_fault.
 *
 *	"Fixed" in seg_drv by calling hat_devload() instead of ddi_fault_page()
 */
static struct seg_ops segdrv_ops = {
	segdrv_dup,
	segdrv_unmap,
	segdrv_free,
	segdrv_fault,
	segdrv_faulta,
	segdrv_setprot,
	segdrv_checkprot,
	segdrv_badop,			/* kluster */
	(size_t (*)(struct seg *))NULL,	/* swapout */
	segdrv_sync,			/* sync */
	segdrv_incore,
	segdrv_lockop,			/* lockop */
	segdrv_getprot,
	segdrv_getoffset,
	segdrv_gettype,
	segdrv_getvp,
	segdrv_advise,
	segdrv_dump,
	segdrv_pagelock,
	segdrv_getmemid,
};

#define	vpgtob(n)	((n) * sizeof (struct vpage))	/* For brevity */

#define	VTOCVP(vp)	(VTOS(vp)->s_commonvp)	/* we "know" it's an snode */

static	long pagesize, pageoffset, pagemask;

/*
 * Private support routines
 */

static struct segdrv_data *sdp_alloc(void);



/* debugging */

#if	SEG_DEBUG
int	seg_debug = 0;
#define	_STMT(op)	do { op } while (0)
/* CSTYLED */
#define	DEBUGF(l, args)	_STMT(if (seg_debug >= l) printf args;)
#define	ASSERT2(e)	_STMT(if (!(e)) \
	/* CSTYLED */			\
    printf("segdrv: assertion failed \"%s\", line %d\n", #e, __LINE__);)
#else
#define	DEBUGF(l, args)
#define	ASSERT2(e)
#endif


/*
 * This routine is called by the device driver to handle
 * non-traditional mmap'able devices that support a d_mmap function.
 *
 * It is assumed at this point that the driver has already validated
 * the offset and length.  If the driver does not support private mappings,
 * it is the driver's responsibility to reject before calling this function.
 */

/*ARGSUSED*/
int
segdrv_segmap(
	dev_t		dev,
	off_t		off,
	struct as	*as,
	caddr_t		*addrp,
	off_t		len,
	u_int		prot,
	u_int		maxprot,
	u_int		flags,
	struct cred	*cred,
	caddr_t		client,
	struct seg_ops	*client_ops,
	int		(*client_create)())
{
	struct segdrv_crargs dev_a;
	int (*mapfunc)(dev_t dev, off_t off, int prot);
	int	error;

	DEBUGF(2, ("segdrv_segmap: d=%x, of=%llx, l=%llx, c=%lx, o=%p, cr=%p\n",
		dev, off, len, client, client_ops, client_create));

	/*
	 * do not check that xx_mmap() exists.  However the driver MUST
	 * provide it if it intends to let seg_drv handle any faults
	 */
	mapfunc = devopsp[getmajor(dev)]->devo_cb_ops->cb_mmap;

	/*
	 * do not check that map_type is MAP_SHARED; this is the
	 * driver's responsibility.
	 */

	/*
	 * do not check range to make sure it's legal; this is the
	 * driver's responsibility.
	 */


	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		/*
		 * Pick an address w/o worrying about
		 * any vac alignment contraints.
		 */
		map_addr(addrp, len, (offset_t)off, 0, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/*
		 * User-specified address; blow away any previous mappings.
		 */
		(void) as_unmap(as, *addrp, len);
	}

	dev_a.mapfunc = mapfunc;
	dev_a.dev = dev;
	dev_a.offset = off;
	dev_a.prot = (u_char)prot;
	dev_a.maxprot = (u_char)maxprot;
	dev_a.flags = flags;
	dev_a.client = client;
	dev_a.client_segops = client_ops;
	dev_a.client_create = client_create;

	error = as_map(as, *addrp, len, segdrv_create, &dev_a);
	as_rangeunlock(as);
	return (error);
}


/*
 * Create a device segment.
 */
int
segdrv_create(struct seg *seg, void *argsp)
{
	struct segdrv_data *sdp;
	struct segdrv_crargs *a = (struct segdrv_crargs *)argsp;
	int error;

	DEBUGF(2, ("segdrv_create: seg=%x, argsp=%p\n", seg, argsp));

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segdrv" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if ((sdp = sdp_alloc()) == NULL)
		return (ENOMEM);

	seg->s_ops = &segdrv_ops;
	seg->s_data = sdp;

	sdp->mapfunc = a->mapfunc;
	sdp->offset = a->offset;
	sdp->prot = a->prot;
	sdp->maxprot = a->maxprot;
	sdp->pageprot = 0;
	sdp->vpage = NULL;
	sdp->flags = a->flags;
	sdp->client = a->client;
	sdp->client_segops = a->client_segops;
	sdp->client_create = a->client_create;

	{
	/*
	 * $#@! compiler doesn't do register tracking and I
	 * want this to be fast
	 */
		register int (*fptr)() = a->client_create;
		if (fptr) {
			DEBUGF(3, ("create calling user proc %p(%p,NULL)\n",
			    fptr, seg));
			error = (*fptr)(seg, NULL);
			if (error == SEGDRV_IGNORE)
				return (0);
			if (error > 0)
				return (error);
		}
	}

	/*
	 * The following call to hat_map presumes that translation resources
	 * are set up by the system MMU. This may cause problems when the
	 * resources are allocated/managed by the device's MMU or an MMU
	 * other than the system MMU. For now hat_map is no-op and not
	 * implemented by the Sun MMU, SRMMU and X86 MMU drivers and should
	 * not pose any problems.
	 */

	hat_map(seg->s_as->a_hat, seg->s_base, seg->s_size, HAT_MAP);

	/* TODO: try concatenation */

	/*
	 * Hold shadow vnode -- segdrv only deals with
	 * character (VCHR) devices. We use the common
	 * vp to hang devpages on.
	 */
	sdp->vp = specfind(a->dev, VCHR);
	ASSERT(sdp->vp != NULL);
#ifdef	COMMENT
	printf("create: mapcnt = %d, pages=%x\n",
	    VTOS(sdp->vp)->s_mapcnt, sdp->vp->v_pages);
#endif	/* COMMENT */

	/*
	 * Inform the vnode of the new mapping.
	 */
	error = VOP_ADDMAP(VTOCVP(sdp->vp), (offset_t)sdp->offset,
	    seg->s_as, seg->s_base, seg->s_size,
	    sdp->prot, sdp->maxprot, MAP_SHARED, CRED());

#ifdef	COMMENT
	printf("create addmap: mapcnt = %d, pages=%x\n",
	    VTOS(sdp->vp)->s_mapcnt, sdp->vp->v_pages);
#endif	/* COMMENT */

	if (error != 0)
		hat_unload(seg->s_as->a_hat, seg->s_base, seg->s_size,
			HAT_UNLOAD_UNMAP);

	return (error);
}

static struct segdrv_data *
sdp_alloc(void)
{
	struct segdrv_data *sdp;

	sdp = kmem_zalloc(sizeof (struct segdrv_data), KM_SLEEP);
	mutex_init(&sdp->lock, NULL, MUTEX_DEFAULT, NULL);
	return (sdp);
}

/*
 * Duplicate seg and return new segment in newseg.
 */
static int
segdrv_dup(struct seg *seg, struct seg *newseg)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	struct segdrv_data *newsdp;
	size_t nbytes;
	int	error;

	DEBUGF(2, ("segdrv_dup: seg=%p, newseg=%p\n", seg, newseg));

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if ((newsdp = sdp_alloc()) == NULL)
		return (ENOMEM);

	newseg->s_ops = seg->s_ops;
	newseg->s_data = (void *)newsdp;

	mutex_enter(&sdp->lock);
	VN_HOLD(sdp->vp);
	newsdp->vp 	= sdp->vp;
	newsdp->mapfunc = sdp->mapfunc;
	newsdp->offset	= sdp->offset;
	newsdp->flags	= sdp->flags;
	newsdp->prot	= sdp->prot;
	newsdp->maxprot = sdp->maxprot;
	newsdp->pageprot = sdp->pageprot;
	newsdp->client	= sdp->client;
	newsdp->client_segops = sdp->client_segops;
	if (sdp->vpage != NULL) {
		nbytes = vpgtob(seg_pages(seg));
		/*
		 * Release the segment lock while allocating
		 * the vpage structure for the new segment
		 * since nobody can create it and our segment
		 * cannot be destroyed as the address space
		 * has a "read" lock.
		 */
		mutex_exit(&sdp->lock);
		newsdp->vpage = kmem_alloc(nbytes, KM_SLEEP);
		mutex_enter(&sdp->lock);
		bcopy(sdp->vpage, newsdp->vpage, nbytes);
	} else
		newsdp->vpage = NULL;

	{
		int (*fptr)() = sdp->client_segops->dup;

		if (fptr) {
			DEBUGF(3, ("dup calling user proc at %p\n", fptr));
			error = (*fptr)(seg, newseg);
			if (error > 0) {
				if (newsdp->vpage != NULL)
					kmem_free(newsdp->vpage, nbytes);
				VN_RELE(sdp->vp);
				newseg->s_data = NULL;
				kmem_free(newsdp, sizeof (*newsdp));
				mutex_exit(&sdp->lock);
				return (error);
			}
		}
	}

	mutex_exit(&sdp->lock);

	/*
	 * Inform the common vnode of the new mapping.
	 */
	return (VOP_ADDMAP(VTOCVP(newsdp->vp), (offset_t)newsdp->offset,
	    newseg->s_as, newseg->s_base, newseg->s_size, newsdp->prot,
	    newsdp->maxprot, MAP_SHARED, CRED()));
}

/*
 * Split a segment at addr for length len.
 */
/*ARGSUSED*/
static int
segdrv_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	Segdrv_Data *sdp = (struct segdrv_data *)seg->s_data;
	Segdrv_Data *nsdp;
	struct seg *nseg;
	size_t	opages;		/* old segment size in pages */
	size_t	npages;		/* new segment size in pages */
	size_t	dpages;		/* pages being deleted (unmapped) */
	caddr_t nbase;
	ssize_t	nsize;
	int	error;

	DEBUGF(2, ("segdrv_unmap: seg=%p, addr=%p, len=%lx\n", seg, addr, len));

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segdrv" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		int (*fptr)() = sdp->client_segops->unmap;
		if (fptr) {
			DEBUGF(3, ("unmap calling user proc at %p\n", fptr));
			error = (*fptr)(seg, addr, len);
			if (error == SEGDRV_IGNORE)
				return (0);
			if (error > 0)
				return (error);
		}
	}


	/*
	 * Check for bad sizes
	 */
	if ((uintptr_t)addr < (uintptr_t)seg->s_base ||
	    (uintptr_t)addr + len > (uintptr_t)seg->s_base + seg->s_size ||
	    (len & pageoffset) || ((uintptr_t)addr & pageoffset))
		cmn_err(CE_PANIC, "segdrv_unmap");

	/*
	 * Unload any hardware translations in the range to be taken out.
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);

	/*
	 * Inform the vnode of the unmapping.
	 */
	ASSERT(sdp->vp != NULL);

#ifdef	COMMENT
	printf("unmap: mapcnt = %d, pages=%p\n",
	    VTOS(sdp->vp)->s_mapcnt, sdp->vp->v_pages);
#endif	/* COMMENT */

	VOP_DELMAP(VTOCVP(sdp->vp),
	    (offset_t)sdp->offset + (addr - seg->s_base),
	    seg->s_as, addr, len, sdp->prot, sdp->maxprot,
	    MAP_SHARED, CRED());

#ifdef	COMMENT
	printf("unmap delmap: mapcnt = %d, pages=%p\n",
	    VTOS(sdp->vp)->s_mapcnt, sdp->vp->v_pages);
#endif	/* COMMENT */

	/*
	 * Check for entire segment
	 */
	if (addr == seg->s_base && len == seg->s_size) {
		seg_free(seg);
		return (0);
	}

	opages = seg_pages(seg);
	dpages = btop(len);
	npages = opages - dpages;

	/*
	 * Check for beginning of segment
	 */
	if (addr == seg->s_base) {
		if (sdp->vpage != NULL) {
			size_t nbytes;
			struct vpage *ovpage;

			ovpage = sdp->vpage;	/* keep pointer to vpage */

			nbytes = vpgtob(npages);
			sdp->vpage = kmem_alloc(nbytes, KM_SLEEP);
			bcopy(&ovpage[dpages], sdp->vpage, nbytes);

			/* free up old vpage */
			kmem_free(ovpage, vpgtob(opages));
		}
		sdp->offset += len;

		seg->s_base += len;
		seg->s_size -= len;
		return (0);
	}

	/*
	 * Check for end of segment
	 */
	if (addr + len == seg->s_base + seg->s_size) {
		if (sdp->vpage != NULL) {
			size_t nbytes;
			struct vpage *ovpage;

			ovpage = sdp->vpage;	/* keep pointer to vpage */

			nbytes = vpgtob(npages);
			sdp->vpage = kmem_alloc(nbytes, KM_SLEEP);
			bcopy(ovpage, sdp->vpage, nbytes);

			/* free up old vpage */
			kmem_free(ovpage, vpgtob(opages));
		}
		seg->s_size -= len;
		return (0);
	}

	/*
	 * The section to go is in the middle of the segment,
	 * have to make it into two segments.  nseg is made for
	 * the high end while seg is cut down at the low end.
	 */
	nbase = addr + len;				/* new seg base */
	nsize = (seg->s_base + seg->s_size) - nbase;	/* new seg size */
	seg->s_size = addr - seg->s_base;		/* shrink old seg */
	nseg = seg_alloc(seg->s_as, nbase, nsize);
	if (nseg == NULL)
		cmn_err(CE_PANIC, "segdrv_unmap seg_alloc");

	nseg->s_ops = seg->s_ops;
	nsdp = sdp_alloc();
	nseg->s_data = (void *)nsdp;
	nsdp->mapfunc = sdp->mapfunc;
	nsdp->offset = sdp->offset + nseg->s_base - seg->s_base;
	nsdp->flags = sdp->flags;
	nsdp->vp = sdp->vp;
	nsdp->pageprot = sdp->pageprot;
	nsdp->prot = sdp->prot;
	nsdp->maxprot = sdp->maxprot;
	nsdp->prot = sdp->prot;
	nsdp->client = sdp->client;
	nsdp->client_segops = sdp->client_segops;
	nsdp->client_create = sdp->client_create;
	VN_HOLD(sdp->vp);	/* Hold vnode associated with the new seg */

	if (sdp->vpage == NULL)
		nsdp->vpage = NULL;
	else {
		/* need to split vpage into two arrays */
		size_t nbytes;
		struct vpage *ovpage;

		ovpage = sdp->vpage;		/* keep pointer to vpage */

		npages = seg_pages(seg);	/* seg has shrunk */
		nbytes = vpgtob(npages);
		sdp->vpage = kmem_alloc(nbytes, KM_SLEEP);

		bcopy(ovpage, sdp->vpage, nbytes);

		npages = seg_pages(nseg);
		nbytes = vpgtob(npages);
		nsdp->vpage = kmem_alloc(nbytes, KM_SLEEP);

		bcopy(&ovpage[opages - npages], nsdp->vpage, nbytes);

		/* free up old vpage */
		kmem_free(ovpage, vpgtob(opages));
	}

	{
		int (*fptr)() = sdp->client_create;
		if (fptr)
			(void) (*fptr)(nseg, sdp->client);
	}

	return (0);
}

/*
 * Free a segment.
 */
static void
segdrv_free(struct seg *seg)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	size_t nbytes = vpgtob(seg_pages(seg));

	DEBUGF(2, ("segdrv_free: seg=%p\n", seg));

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segdrv" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		void (*fptr)() = sdp->client_segops->free;
		if (fptr) {
			DEBUGF(3, ("free calling user proc at %p\n", fptr));
			(*fptr)(seg);
		}
	}

	if (sdp->vp != NULL)
		VN_RELE(sdp->vp);

	if (sdp->vpage != NULL)
		kmem_free(sdp->vpage, nbytes);
	mutex_destroy(&sdp->lock);
	kmem_free(sdp, sizeof (*sdp));
}


/*
 * Do a F_SOFTUNLOCK call over the range requested.
 * The range must have already been F_SOFTLOCK'ed.
 * The segment lock should be held.
 */
/*ARGSUSED*/
static void
segdrv_softunlock(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* seg_drv of interest */
	caddr_t addr,			/* base address of range */
	size_t len,			/* number of bytes */
	enum seg_rw rw,			/* type of access at fault */
	struct cred *cr)		/* credentials */
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	u_offset_t offset;
	caddr_t a;

	offset = sdp->offset + (addr - seg->s_base);
	for (a = addr; a < addr + len; a += pagesize) {
		hat_unlock(hat, a, PAGESIZE);

		offset += pagesize;
	}
}

/*
 * Handle a single devpage.
 * Done in a separate routine so we can handle errors more easily.
 * This routine is called only from segdrv_fault()
 * when looping over the range of addresses requested.  The
 * segment lock should be held.
 *
 * The basic algorithm here is:
 *		Find pfn from the driver's mmap function
 *		Load up the translation to the devpage
 *		return
 */
/*ARGSUSED*/
faultcode_t
segdrv_faultpage(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* seg_drv of interest */
	caddr_t addr,			/* address in as */
	struct vpage *vpage,		/* pointer to vpage for seg, addr */
	enum fault_type type,		/* type of fault */
	enum seg_rw rw,			/* type of access at fault */
	struct cred *cr)		/* credentials */
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	u_int prot;
	pfn_t pfnum;		/* XXVM64 cdev_mmap returns int */
	u_offset_t offset;

	/*
	 * Initialize protection value for this page.
	 * If we have per page protection values check it now.
	 */
	if (sdp->pageprot) {
		u_int protchk;

		switch (rw) {
		case S_READ:
			protchk = PROT_READ;
			break;
		case S_WRITE:
			protchk = PROT_WRITE;
			break;
		case S_EXEC:
			protchk = PROT_EXEC;
			break;
		case S_OTHER:
		default:
			protchk = PROT_READ | PROT_WRITE | PROT_EXEC;
			break;
		}

		prot = VPP_PROT(vpage);
		if ((prot & protchk) == 0)
			return (FC_PROT);	/* illegal access type */
	} else {
		prot = sdp->prot;
	}

	offset = sdp->offset + (addr - seg->s_base);

	if (sdp->mapfunc == nodev)
		return (FC_NOMAP);

	pfnum = (pfn_t)cdev_mmap(sdp->mapfunc, sdp->vp->v_rdev, offset, prot);
	if (pfnum == (pfn_t)-1)
		return (FC_MAKE_ERR(EFAULT));

	hat_devload(hat, addr, PAGESIZE, pfnum, prot,
		((type == F_SOFTLOCK) ? HAT_LOAD_LOCK : HAT_LOAD) |
		HAT_LOAD_NOCONSIST);

	return (0);
}

/*
 * This routine is called via a machine specific fault handling routine.
 * It is also called by software routines wishing to lock or unlock
 * a range of addresses.
 *
 * Here is the basic algorithm:
 *	If unlocking
 *		Call segdrv_softunlock
 *		Return
 *	endif
 *	Checking and set up work
 *	Loop over all addresses requested
 *		Call segdrv_faultpage to load up translations.
 *	endloop
 */
static faultcode_t
segdrv_fault(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* the seg_drv of interest */
	caddr_t addr,			/* the address of the fault */
	size_t len,			/* the length of the range */
	enum fault_type type,		/* type of fault */
	enum seg_rw rw)			/* type of access at fault */
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	caddr_t a;
	struct vpage *vpage;
	ssize_t page;
	struct cred *cr = CRED();
	faultcode_t err;
	int	cret;

	DEBUGF(2, ("segdrv_fault: hat=%p, seg=%p, addr=%p, len=%lx\n",
	    hat, seg, addr, len));

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		faultcode_t (*fptr)() = sdp->client_segops->fault;
		if (fptr) {
			DEBUGF(3, ("fault calling user proc at %p\n", fptr));
			cret = (*fptr)(hat, seg, addr, len, type, rw);
			if (cret == SEGDRV_IGNORE)
				return (0);
			if (cret > 0)
				return (cret);
		} else
			cret = SEGDRV_CONTINUE;
	}

	if (type == F_PROT) {
		/*
		 * Since the seg_drv driver does not implement copy-on-write,
		 * this means that a valid translation is already loaded,
		 * but we got an fault trying to access the device.
		 * Return an error here to prevent going in an endless
		 * loop reloading the same translation...
		 */
		return (FC_PROT);
	}

	/*
	 * First handle the easy stuff
	 */
	if (type == F_SOFTUNLOCK) {
		segdrv_softunlock(hat, seg, addr, len, rw, cr);
		return (0);
	}

	/*
	 * If we have the same protections for the entire segment,
	 * insure that the access being attempted is legitimate.
	 */
	mutex_enter(&sdp->lock);
	if (sdp->pageprot == 0) {
		u_int protchk;

		switch (rw) {
		case S_READ:
			protchk = PROT_READ;
			break;
		case S_WRITE:
			protchk = PROT_WRITE;
			break;
		case S_EXEC:
			protchk = PROT_EXEC;
			break;
		case S_OTHER:
		default:
			protchk = PROT_READ | PROT_WRITE | PROT_EXEC;
			break;
		}

		if ((sdp->prot & protchk) == 0) {
			mutex_exit(&sdp->lock);
			return (FC_PROT);	/* illegal access type */
		}
	}

	if (cret != SEGDRV_HANDLED) {
		page = seg_page(seg, addr);
		if (sdp->vpage == NULL)
			vpage = NULL;
		else
			vpage = &sdp->vpage[page];

		/* loop over the address range handling each fault */

		for (a = addr; a < addr + len; a += pagesize) {
			err = segdrv_faultpage(hat, seg, a, vpage, type,
			    rw, cr);
			if (err) {
				if (type == F_SOFTLOCK && a > addr)
					segdrv_softunlock(hat, seg, addr,
					    (size_t)(a - addr), S_OTHER, cr);
				mutex_exit(&sdp->lock);
				return (err);
			}
			if (vpage != NULL)
				vpage++;
		}
	}
	mutex_exit(&sdp->lock);
	return (0);
}

/*
 * Asynchronous page fault.  We simply do nothing since this
 * entry point is not supposed to load up the translation.
 */
/*ARGSUSED*/
static faultcode_t
segdrv_faulta(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* TODO: let driver interpose here? */

	return (0);
}

static int
segdrv_setprot(
	struct seg *seg,
	caddr_t addr,
	size_t len,
	u_int prot)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	struct vpage *vp, *evp;
	int	cret;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		int (*fptr)() = sdp->client_segops->setprot;
		if (fptr) {
			cret = (*fptr)(seg, addr, len, prot);
			if (cret == SEGDRV_IGNORE)
				return (0);
			if (cret > 0)
				return (cret);
		}
	}

	if ((sdp->maxprot & prot) != prot && cret != SEGDRV_HANDLED)
		return (EACCES);		/* violated maxprot */

	mutex_enter(&sdp->lock);
	if (addr == seg->s_base && len == seg->s_size && sdp->pageprot == 0) {
		if (sdp->prot == prot) {
			mutex_exit(&sdp->lock);
			return (0);			/* all done */
		}
		sdp->prot = (u_char)prot;
	} else {
		sdp->pageprot = 1;
		if (sdp->vpage == NULL) {
			/*
			 * First time through setting per page permissions,
			 * initialize all the vpage structures to prot
			 */
			sdp->vpage = kmem_zalloc(vpgtob(seg_pages(seg)),
			    KM_SLEEP);
			evp = &sdp->vpage[seg_pages(seg)];
			for (vp = sdp->vpage; vp < evp; vp++)
				VPP_SETPROT(vp, sdp->prot);
		}
		/*
		 * Now go change the needed vpages protections.
		 */
		evp = &sdp->vpage[seg_page(seg, addr + len)];
		for (vp = &sdp->vpage[seg_page(seg, addr)]; vp < evp; vp++)
			VPP_SETPROT(vp, prot);
	}
	mutex_exit(&sdp->lock);

	if (cret != SEGDRV_HANDLED) {
		if ((prot & ~PROT_USER) == PROT_NONE) {
			hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD);
		} else {
			/*
			 * RFE: the segment should keep track of all attributes
			 * allowing us to remove the deprecated hat_chgprot
			 * and use hat_chgattr.
			 */
			hat_chgprot(seg->s_as->a_hat, addr, len, prot);
		}
	}
	return (0);
}

static int
segdrv_checkprot(
	struct seg *seg,
	caddr_t addr,
	size_t len,
	u_int prot)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	struct vpage *vp, *evp;
	int	cret;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		int (*fptr)() = sdp->client_segops->checkprot;
		if (fptr) {
			cret = (*fptr)(seg, addr, len, prot);
			if (cret == SEGDRV_HANDLED || cret == SEGDRV_IGNORE)
				return (0);
			if (cret > 0)
				return (cret);
		}
	}

	/*
	 * If segment protection can be used, simply check against them
	 */
	mutex_enter(&sdp->lock);
	if (sdp->pageprot == 0) {
		int err;

		err = ((sdp->prot & prot) != prot) ? EACCES : 0;
		mutex_exit(&sdp->lock);
		return (err);
	}

	/*
	 * Have to check down to the vpage level
	 */
	evp = &sdp->vpage[seg_page(seg, addr + len)];
	for (vp = &sdp->vpage[seg_page(seg, addr)]; vp < evp; vp++) {
		if ((VPP_PROT(vp) & prot) != prot) {
			mutex_exit(&sdp->lock);
			return (EACCES);
		}
	}
	mutex_exit(&sdp->lock);
	return (0);
}

static int
segdrv_getprot(
	struct seg *seg,
	caddr_t addr,
	size_t len,
	u_int *protv)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	size_t pgno;
	int	cret;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		int (*fptr)() = sdp->client_segops->getprot;
		if (fptr) {
			cret = (*fptr)(seg, addr, len, protv);
			if (cret == SEGDRV_HANDLED || cret == SEGDRV_IGNORE)
				return (0);
			if (cret > 0)
				return (cret);
		}
	}
	pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;
	if (pgno != 0) {
		mutex_enter(&sdp->lock);
		if (sdp->pageprot == 0) {
			do
				protv[--pgno] = sdp->prot;
			while (pgno != 0);
		} else {
			size_t pgoff = seg_page(seg, addr);

			do {
				pgno--;
				protv[pgno] =
				    VPP_PROT(&sdp->vpage[pgno + pgoff]);
			} while (pgno != 0);
		}
		mutex_exit(&sdp->lock);
	}
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segdrv_getoffset(struct seg *seg, caddr_t addr)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		u_offset_t (*fptr)() = sdp->client_segops->getoffset;
		if (fptr)
			return ((*fptr)(seg, addr));
	}

	return ((u_offset_t)sdp->offset);
}

/*ARGSUSED*/
static int
segdrv_gettype(struct seg *seg, caddr_t addr)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		int (*fptr)() = sdp->client_segops->gettype;
		if (fptr)
			return ((*fptr)(seg, addr));
	}

	return (sdp->flags & MAP_TYPE);
}


/*ARGSUSED*/
static int
segdrv_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	{
		int (*fptr)() = sdp->client_segops->getvp;
		if (fptr)
			return (*fptr)(seg, addr, vpp);
	}

	/*
	 * Note that this vp is the common_vp of the device, where the
	 * devpages are hung ..
	 */
	*vpp = VTOCVP(sdp->vp);

	return (0);
}

static int
segdrv_badop(void)
{
	cmn_err(CE_PANIC, "segdrv_badop");
	return (0);
	/*NOTREACHED*/
}

/*
 * segdrv pages are not in the cache, and thus can't really be controlled.
 * Hence, syncs are simply always successful.
 */
/*ARGSUSED*/
static int
segdrv_sync(struct seg *seg, caddr_t addr, size_t len, int attr, u_int flags)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* TODO: let driver interpose here? */

	return (0);
}

/*
 * segdrv pages are always "in core".
 */
/*ARGSUSED*/
static size_t
segdrv_incore(struct seg *seg, caddr_t addr,
    size_t len, char *vec)
{
	size_t v = 0;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* TODO: let driver interpose here? */

	for (len = (len + pageoffset) & pagemask; len; len -= pagesize,
	    v += pagesize)
		*vec++ = 1;
	return (v);
}

/*
 * segdrv pages are not in the cache, and thus can't really be controlled.
 * Hence, locks are simply always successful.
 */
/*ARGSUSED*/
static int
segdrv_lockop(struct seg *seg, caddr_t addr,
    size_t len, int attr, int op, u_long *lockmap, size_t pos)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* TODO: let driver interpose here? */

	return (0);
}

/*
 * segdrv pages are not in the cache, and thus can't really be controlled.
 * Hence, advise is simply always successful.
 */
/*ARGSUSED*/
static int
segdrv_advise(struct seg *seg, caddr_t addr, size_t len, u_int behav)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* TODO: let driver interpose here? */

	return (0);
}

/*
 * segdrv pages are not dumped, so we just return
 */
/*ARGSUSED*/
static void
segdrv_dump(struct seg *seg)
{}

/*ARGSUSED*/
static int
segdrv_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

#include <sys/modctl.h>

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, "Segment Device Driver v1.1",
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	DEBUGF(1, ("seg_drv: compiled %s, %s\n", __TIME__, __DATE__));

	pagesize = ptob(1);
	pageoffset = pagesize - 1;
	pagemask = ~pageoffset;

	return (mod_install(&modlinkage));
}

/*
 * Unloading is MT-safe because our client drivers use
 * the _depends_on[] mechanism - we won't go while they're
 * still around.
 */
int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
segdrv_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	struct segdrv_data *sdp = (struct segdrv_data *)seg->s_data;
	int (*fptr)() = sdp->client_segops->getmemid;

	if (fptr)
		return ((*fptr)(seg, addr, memidp));

	if (sdp->flags & MAP_PRIVATE) {
		memidp->val[0] = (u_longlong_t)seg->s_as;
		memidp->val[1] = (u_longlong_t)addr;
	} else if (sdp->flags & MAP_SHARED) {
		memidp->val[0] = (u_longlong_t)VTOCVP(sdp->vp);
		memidp->val[1] = (u_longlong_t)sdp->offset
					+ (addr - seg->s_base);
	}
	return (0);
}
