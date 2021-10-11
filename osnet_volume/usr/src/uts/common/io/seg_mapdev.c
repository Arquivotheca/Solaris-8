/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1991, 1993-1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma	ident	"@(#)seg_mapdev.c	1.37	98/03/13 SMI"

/*
 * VM - mapdev segment of a mapped device.
 *
 * This segment driver is used when mapping character special devices.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/seg_mapdev.h>
#include <sys/thread.h>

#include <vm/page.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/vpage.h>
#include <sys/fs/snode.h>

#include <sys/ddi.h>
#include <sys/esunddi.h>

/*
 * Private seg op routines.
 */

static int	segmapdev_dup(struct seg *, struct seg *);
static int	segmapdev_unmap(struct seg *, caddr_t, size_t);
static void	segmapdev_free(struct seg *);
static faultcode_t segmapdev_fault(struct hat *, struct seg *, caddr_t, size_t,
		    enum fault_type, enum seg_rw);
static faultcode_t segmapdev_faulta(struct seg *, caddr_t);
static int	segmapdev_setprot(struct seg *, caddr_t, size_t, u_int);
static int	segmapdev_checkprot(struct seg *, caddr_t, size_t, u_int);
static void	segmapdev_badop(void);
static int	segmapdev_sync(struct seg *, caddr_t, size_t, int, u_int);
static size_t	segmapdev_incore(struct seg *, caddr_t, size_t, char *);
static int	segmapdev_lockop(struct seg *, caddr_t, size_t, int, int,
		    ulong *, size_t);
static int	segmapdev_getprot(struct seg *, caddr_t, size_t, u_int *);
static u_offset_t	segmapdev_getoffset(struct seg *, caddr_t);
static int	segmapdev_gettype(struct seg *, caddr_t);
static int	segmapdev_getvp(struct seg *, caddr_t, struct vnode **);
static int	segmapdev_advise(struct seg *, caddr_t, size_t, u_int);
static void	segmapdev_dump(struct seg *);
static int	segmapdev_pagelock(struct seg *, caddr_t, size_t,
			struct page ***, enum lock_type, enum seg_rw);
static int	segmapdev_getmemid(struct seg *, caddr_t, memid_t *);

static struct seg_ops segmapdev_ops = {
	segmapdev_dup,
	segmapdev_unmap,
	segmapdev_free,
	segmapdev_fault,
	segmapdev_faulta,
	segmapdev_setprot,
	segmapdev_checkprot,
	(int (*)())segmapdev_badop,	/* kluster */
	(size_t (*)(struct seg *))NULL,	/* swapout */
	segmapdev_sync,			/* sync */
	segmapdev_incore,
	segmapdev_lockop,		/* lockop */
	segmapdev_getprot,
	segmapdev_getoffset,
	segmapdev_gettype,
	segmapdev_getvp,
	segmapdev_advise,
	segmapdev_dump,
	segmapdev_pagelock,
	segmapdev_getmemid,
};

/*
 * Max number of ticks that a thread can block other threads from taking the
 * device context on MP machines.
 */
#define	LEAVE_ONCPU	1
static int segmapdev_leaveoncpu = LEAVE_ONCPU;
/*
 * List of device context private data.  One per device
 */
static struct segmapdev_ctx *devctx_list = NULL;
/*
 * Protects devctx_list
 */
static kmutex_t	devctx_lock;

#define	vpgtob(n)	((n) * sizeof (struct vpage))	/* For brevity */

#define	VTOCVP(vp)	(VTOS(vp)->s_commonvp)	/* we "know" it's an snode */

/*
 * Private support routines
 */

static struct segmapdev_data *sdp_alloc(void);

static void segmapdev_softunlock(struct hat *, struct seg *, caddr_t,
    size_t, enum seg_rw, cred_t *);

static int segmapdev_faultpage(struct hat *, struct seg *, caddr_t,
    struct vpage *, u_int *, enum fault_type, enum seg_rw, cred_t *);

static int segmapdev_faultpages(struct hat *, struct seg *, caddr_t,
    size_t, enum fault_type, enum seg_rw);

/*
 * The following routines are used to manage thread context callbacks and
 * are used to prevent thrashing
 */
static struct segmapdev_ctx *segmapdev_ctxfind(dev_t dev, ulong id);

/*
 * Find a devctx struct in the list of devctx structs.  If there is no devctx
 * struct for this device yet, return NULL
 */
static struct segmapdev_ctx *
segmapdev_ctxfind(dev_t dev, ulong id)
{
	struct segmapdev_ctx *devctx;

	ASSERT(mutex_owned(&devctx_lock));

	for (devctx = devctx_list; devctx != NULL; devctx = devctx->next)
		if ((devctx->dev == dev) && (devctx->id == id))
			return (devctx);
	return (NULL);
}

/*
 * Initialize the thread callbacks and thread private data.
 */
static struct segmapdev_ctx *
segmapdev_ctxinit(dev_t dev, ulong id)
{
	struct segmapdev_ctx *devctx;

	mutex_enter(&devctx_lock);

	/*
	 * Get the devctx struct for this device.  If one has not been
	 * created yet, create a new one.
	 */
	if ((devctx = segmapdev_ctxfind(dev, id)) == NULL) {
		devctx = kmem_alloc(sizeof (struct segmapdev_ctx), KM_SLEEP);
		devctx->dev = dev;
		devctx->id = id;
		devctx->oncpu = 0;
		devctx->refcnt = 0;
		devctx->timeout = 0;
		devctx->next = devctx_list;
		devctx_list = devctx;
		mutex_init(&devctx->lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&devctx->cv, NULL, CV_DEFAULT, NULL);
	}

	devctx->refcnt++;

	mutex_exit(&devctx_lock);
	return (devctx);

}

/*
 * Timeout callback called if a CPU has not given up the device context
 * within sdp->timeout_length ticks
 */
static void
segmapdev_ctxto(void *data)
{
	struct segmapdev_ctx *devctx = data;

	mutex_enter(&devctx->lock);
	/*
	 * Set oncpu = 0 so the next mapping trying to get the device context
	 * can.
	 */
	devctx->oncpu = 0;
	devctx->timeout = 0;
	cv_signal(&devctx->cv);
	mutex_exit(&devctx->lock);
}

/*
 * Create a device segment.
 */
static int
segmapdev_create(struct seg *seg, void *argsp)
{
	register struct segmapdev_data *sdp;
	register struct segmapdev_crargs *a = (struct segmapdev_crargs *)argsp;
	register int error;

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segmapdev" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

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

	if ((sdp = sdp_alloc()) == NULL)
		return (ENOMEM);

	sdp->mapfunc = a->mapfunc;
	sdp->offset = a->offset;
	sdp->pageprot = 0;
	sdp->prot = a->prot;
	sdp->maxprot = a->maxprot;
	sdp->flags = a->flags;
	sdp->vpage = NULL;
	sdp->pagehat_flags = 0;
	sdp->hat_flags = 0;
	sdp->vpage_hat_flags = NULL;
	sdp->vpage_inter = NULL;
	sdp->hat = NULL;
	sdp->dev = a->dev;

	/*
	 * Set to intercept by default
	 */
	sdp->flags = SEGMAPDEV_INTER;

	/*
	 * Set default number of clock ticks to keep ctx
	 */
	sdp->timeout_length = segmapdev_leaveoncpu;

	/*
	 * Save away driver private data and callbacks
	 */
	sdp->m_ops.mapdev_rev = a->m_ops->mapdev_rev;
	sdp->m_ops.mapdev_access = a->m_ops->mapdev_access;
	sdp->m_ops.mapdev_free = a->m_ops->mapdev_free;
	sdp->m_ops.mapdev_dup = a->m_ops->mapdev_dup;
	sdp->private_data = a->private_data;

	/*
	 * Return the ddi_mapdev_handle.
	 */
	*a->handle = seg;

	/*
	 * Hold common vnode -- segmapdev only deals with
	 * character (VCHR) devices, and uses the common
	 * vp to hang devpages on.
	 */
	sdp->vp = specfind(a->dev, VCHR);
	ASSERT(sdp->vp != NULL);

	/*
	 * Initialize the device context data
	 */
	sdp->devctx = segmapdev_ctxinit(sdp->dev,
	    (uintptr_t)sdp->m_ops.mapdev_access);

	seg->s_ops = &segmapdev_ops;
	seg->s_data = sdp;

	/*
	 * Inform the vnode of the new mapping.
	 */
	error = VOP_ADDMAP(VTOCVP(sdp->vp), (offset_t)sdp->offset,
	    seg->s_as, seg->s_base, seg->s_size,
	    sdp->prot, sdp->maxprot, MAP_SHARED, CRED());
	if (error != 0)
		hat_unload(seg->s_as->a_hat, seg->s_base, seg->s_size,
			HAT_UNLOAD_UNMAP);

	return (error);
}

static struct segmapdev_data *
sdp_alloc(void)
{
	register struct segmapdev_data *sdp;

	sdp = (struct segmapdev_data *)
		kmem_zalloc(sizeof (struct segmapdev_data), KM_SLEEP);
	mutex_init(&sdp->lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&sdp->wait, NULL, CV_DEFAULT, NULL);
	return (sdp);
}

/*
 * Duplicate seg and return new segment in newseg.
 */
static int
segmapdev_dup(struct seg *seg, struct seg *newseg)
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register struct segmapdev_data *newsdp;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if ((newsdp = sdp_alloc()) == NULL)
		return (ENOMEM);

	newseg->s_ops = seg->s_ops;
	newseg->s_data = (void *)newsdp;

	mutex_enter(&sdp->lock);
	VN_HOLD(sdp->vp);
	newsdp->mapfunc = sdp->mapfunc;
	newsdp->offset	= sdp->offset;
	newsdp->vp 	= sdp->vp;
	newsdp->pageprot = sdp->pageprot;
	newsdp->prot	= sdp->prot;
	newsdp->maxprot = sdp->maxprot;
	newsdp->flags = sdp->flags;
	newsdp->pagehat_flags = sdp->pagehat_flags;
	newsdp->hat_flags = sdp->hat_flags;
	newsdp->hat = NULL;
	newsdp->dev 	= sdp->dev;

	/*
	 * Set timeout_length
	 */
	newsdp->timeout_length = sdp->timeout_length;

	/*
	 * Initialize the device context data
	 */
	newsdp->devctx = segmapdev_ctxinit(newsdp->dev,
	    (uintptr_t)newsdp->m_ops.mapdev_access);

	/*
	 * Save away driver private data and callbacks
	 */
	newsdp->private_data = sdp->private_data;
	newsdp->m_ops.mapdev_rev = sdp->m_ops.mapdev_rev;
	newsdp->m_ops.mapdev_access = sdp->m_ops.mapdev_access;
	newsdp->m_ops.mapdev_free = sdp->m_ops.mapdev_free;
	newsdp->m_ops.mapdev_dup = sdp->m_ops.mapdev_dup;

	/*
	 * Initialize per page interception data if the segment we are
	 * dup'ing has per page interception information.
	 */
	if (sdp->vpage_inter != NULL) {
		newsdp->vpage_inter = kmem_alloc(seg_pages(newseg), KM_SLEEP);
		bcopy(sdp->vpage_inter, newsdp->vpage_inter, seg_pages(newseg));
	} else
		newsdp->vpage_inter = NULL;

	/*
	 * Initialize per page hat-flag data if the segment we are
	 * dup'ing has per page hat-flag information.
	 */
	if (sdp->vpage_hat_flags != NULL) {
		newsdp->vpage_hat_flags =
		    kmem_alloc(seg_pages(newseg), KM_SLEEP);
		bcopy(sdp->vpage_hat_flags,
		    newsdp->vpage_hat_flags, seg_pages(newseg));
	} else
		newsdp->vpage_hat_flags = NULL;

	if (sdp->vpage != NULL) {
		register size_t nbytes = vpgtob(seg_pages(seg));
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

	mutex_exit(&sdp->lock);

	if (sdp->m_ops.mapdev_dup) {
		int ret;

		/*
		 * Call the dup callback so that the driver can duplicate
		 * its private data.
		 */
		ret = (*sdp->m_ops.mapdev_dup)
		    (seg, sdp->private_data, newseg, &newsdp->private_data);

		if (ret != 0) {
			/*
			 * We want to free up this segment as the driver has
			 * indicated that we can't dup it.  But we don't want
			 * to call the drivers callback func as the driver
			 * does not think this segment exists.
			 *
			 * The caller of segmapdev_dup will call seg_free on
			 * newseg as it was the caller that allocated the
			 * segment.
			 */
			newsdp->m_ops.mapdev_free = NULL;
			return (ret);
		}
	}

	/*
	 * Inform the common vnode of the new mapping.
	 */
	return (VOP_ADDMAP(VTOCVP(newsdp->vp),
	    (offset_t)newsdp->offset, newseg->s_as,
	    newseg->s_base, newseg->s_size, newsdp->prot,
	    newsdp->maxprot, MAP_SHARED, CRED()));
}

/*
 * unmap a mapdev'd region.
 */
/*ARGSUSED*/
static int
segmapdev_unmap(register struct seg *seg, register caddr_t addr, size_t len)
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segmapdev" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * For mapdev, we require that the entire segment be unmapped.
	 * We do not support partial unmapping of segments.
	 */
	if (addr != seg->s_base || len != seg->s_size)
		return (EINVAL);

	/*
	 * Unload any hardware translations in the range
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);

	/*
	 * Inform the vnode of the unmapping.
	 */
	ASSERT(sdp->vp != NULL);
	VOP_DELMAP(VTOCVP(sdp->vp),
	    (offset_t)sdp->offset + (addr - seg->s_base),
	    seg->s_as, addr, len, sdp->prot, sdp->maxprot,
	    MAP_SHARED, CRED());

	/*
	 * Segmapdev_free will call the free callback into the driver.
	 * We don't have to do anything more here.
	 */
	seg_free(seg);
	return (0);
}

/*
 * Free a segment.
 */
static void
segmapdev_free(struct seg *seg)
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register size_t nbytes = vpgtob(seg_pages(seg));
	struct segmapdev_ctx	*devctx;
	struct segmapdev_ctx	*parent;
	struct segmapdev_ctx	*tmp;

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segmapdev" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Call the free callback so that the driver can free up its
	 * private data.
	 */
	if (sdp->m_ops.mapdev_free)
		(*sdp->m_ops.mapdev_free)(seg, sdp->private_data);

	/*
	 * If we have per page interception information free it up.
	 */
	if (sdp->vpage_inter)
		kmem_free(sdp->vpage_inter, seg_pages(seg));

	/*
	 * If we have per page hat-flags information free it up.
	 */
	if (sdp->vpage_hat_flags)
		kmem_free(sdp->vpage_hat_flags, seg_pages(seg));

	VN_RELE(sdp->vp);
	if (sdp->vpage != NULL)
		kmem_free(sdp->vpage, nbytes);

	devctx = sdp->devctx;

	/*
	 * Untimeout any threads using this mapping as they are about
	 * to go away.
	 */
	if (devctx->timeout != 0)
		(void) untimeout(devctx->timeout);

	mutex_enter(&devctx_lock);
	mutex_enter(&devctx->lock);

	/*
	 * If a mapping is waiting for this device context, set it free.
	 */
	devctx->oncpu = 0;
	cv_signal(&devctx->cv);

	/*
	 * Decrement reference count
	 */
	devctx->refcnt--;

	/*
	 * If no one is using the device, free up the devctx data.
	 */
	if (devctx->refcnt <= 0) {
		if (devctx_list == devctx)
			devctx_list = devctx->next;
		else {
			parent = devctx_list;
			for (tmp = devctx_list->next; tmp != NULL;
			    tmp = tmp->next) {
				if (tmp == devctx) {
					parent->next = tmp->next;
					break;
				}
				parent = tmp;
			}
		}
		mutex_exit(&devctx->lock);
		mutex_destroy(&devctx->lock);
		cv_destroy(&devctx->cv);
		kmem_free(devctx, sizeof (struct segmapdev_ctx));
	} else
		mutex_exit(&devctx->lock);
	mutex_exit(&devctx_lock);
	mutex_destroy(&sdp->lock);
	cv_destroy(&sdp->wait);
	kmem_free(sdp, sizeof (*sdp));
}

/*
 * Do a F_SOFTUNLOCK call over the range requested.
 * The range must have already been F_SOFTLOCK'ed.
 * The segment lock should be held.
 */
/*ARGSUSED*/
static void
segmapdev_softunlock(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* seg_dev of interest */
	register caddr_t addr,		/* base address of range */
	size_t len,			/* number of bytes */
	enum seg_rw rw,			/* type of access at fault */
	cred_t *cr)			/* credentials */
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register u_offset_t	offset;
	register caddr_t a;

	offset = sdp->offset + (addr - seg->s_base);
	for (a = addr; a < addr + len; a += PAGESIZE) {
		hat_unlock(hat, a, PAGESIZE);

		offset += PAGESIZE;
	}
}

/*
 * Handle a single devpage.
 * Done in a separate routine so we can handle errors more easily.
 * This routine is called only from segmapdev_fault()
 * when looping over the range of addresses requested.  The
 * segment lock should be held.
 *
 * The basic algorithm here is:
 *		Find pfn from the driver's mmap function
 *		Load up the translation to the devpage
 *		return
 */
/*ARGSUSED*/
static int
segmapdev_faultpage(
	struct hat *hat,	/* the hat */
	struct seg *seg,	/* seg_dev of interest */
	caddr_t addr,		/* address in as */
	struct vpage *vpage,	/* pointer to vpage for seg, addr */
	u_int *vpage_hat_flags,	/* pointer to vpage_hat_flags for seg, addr */
	enum fault_type type,	/* type of fault */
	enum seg_rw rw,		/* type of access at fault */
	cred_t *cr)		/* credentials */
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register u_int prot;
	register pfn_t pfnum;
	register u_offset_t offset;
	u_int hat_flags;
	dev_info_t *dip;

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

	pfnum = cdev_mmap(sdp->mapfunc, sdp->vp->v_rdev, offset, prot);
	if (pfnum == (pfn_t)-1)
		return (FC_MAKE_ERR(EFAULT));

	hat_flags = (type == F_SOFTLOCK) ? HAT_LOAD_LOCK : HAT_LOAD;
	if (pf_is_memory(pfnum)) {
		hat_devload(hat, addr, PAGESIZE, pfnum,
		    prot | *vpage_hat_flags, hat_flags | HAT_LOAD_NOCONSIST);
		return (0);
	}

	dip = VTOS(VTOCVP(sdp->vp))->s_dip;
	ASSERT(dip);

	/*
	 * For performance reasons, call had_devload directly instead of
	 * calling up the tree with ddi_map_fault()
	 */
	hat_devload(hat, addr, PAGESIZE, pfnum,
	    prot | *vpage_hat_flags, hat_flags);

	return (0);
}

/*
 * This routine is called via a machine specific fault handling routine.
 * It is also called by software routines wishing to lock or unlock
 * a range of addresses.
 */
static faultcode_t
segmapdev_fault(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* the seg_dev of interest */
	register caddr_t addr,		/* the address of the fault */
	size_t len,			/* the length of the range */
	enum fault_type type,		/* type of fault */
	register enum seg_rw rw)	/* type of access at fault */
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register caddr_t a;
	long	page;
	char	call_access = 0;
	int	ret;
	int	do_timeout = 0;
	struct segmapdev_ctx *devctx;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (type == F_PROT) {
		/*
		 * Since the seg_mapdev driver does not implement copy-on-write,
		 * this means that a valid translation is already loaded,
		 * but we got an fault trying to access the device.
		 * Return an error here to prevent going in an endless
		 * loop reloading the same translation...
		 */
		return (FC_PROT);
	}

	/*
	 * We need to call the driver's
	 * access callback routine to handle the fault.  The driver will at
	 * some point call ddi_mapdev_nointercept on this segment which will
	 * in turn call segmapdev_faultpages to fault in the pages.
	 * We will not fault in the pages of a mapdev segment now.
	 */
	mutex_enter(&sdp->lock);

	/*
	 * This needs to be single threaded on a per segment bases
	 * due to the fact that we need to pass some data through the driver
	 * via the segment private data structure.
	 *
	 * We don't need to check the return value from cv_wait_sig() as it
	 * does not matter much if it returned due to a signal or due to a
	 * cv_signal() or cv_broadcast().  In either event we need to complete
	 * the mapping otherwise the processes will die with a SEGV.
	 */
	while (sdp->flags & SEGMAPDEV_FAULTING)
		(void) cv_wait_sig(&sdp->wait, &sdp->lock);

	/*
	 * Mark this mapdev segment as faulting so that we are single
	 * threaded through the driver and so that
	 * segmapdev_nointer knows if the segment it got
	 * from the access callback (via ddi_mapdev_nointercept)
	 * needs to be faulted or just marked as nointercept.
	 */
	sdp->flags |= SEGMAPDEV_FAULTING;

	/*
	 * Check to see if we have per page interception information.
	 * If so is one of the pages that is faulting marked as
	 * intercept?
	 *
	 * If sdp->vpage_inter != NULL, then we assert that sdp->flags
	 * does not have SEGMAPDEV_INTER set.  Else we don't assert anything
	 */
	ASSERT(((sdp->vpage_inter != NULL) ?
	    !(sdp->flags & SEGMAPDEV_INTER) : 1));

	/*
	 * If we have per page interception information, see if the pages we
	 * are faulting on have their interception bit set.
	 */
	if (sdp->vpage_inter) {
		for (a = addr; a < addr + len; a += PAGESIZE) {
			page = seg_page(seg, a);
			if (sdp->vpage_inter[page]) {
				call_access = 1;
				break;
			}
		}
	}

	/*
	 * If we need to intercept this fault then call the
	 * driver access callback.
	 */
	if (((sdp->flags & SEGMAPDEV_INTER) ||
	    (call_access)) && (sdp->m_ops.mapdev_access)) {

		/*
		 * Save away the hat, type of fault, and type of
		 * access so that segmapdev_faultpages will be able to
		 * handle the fault.
		 */
		sdp->hat = hat;
		sdp->type = type;
		sdp->rw = rw;

		devctx = sdp->devctx;
		mutex_exit(&sdp->lock);

		/*
		 * If we are on an MP system with more than one cpu running
		 * and if a thread on some CPU already has the context, wait
		 * for it to finish if there is a hysteresis timeout.
		 *
		 * We don't need to check the return value from cv_wait_sig()
		 * as it does not matter much if it returned due to a signal
		 * or due to a cv_signal() or cv_broadcast().  In either event
		 * we need to complete the mapping otherwise the processes
		 * will die with a SEGV.
		 *
		 */
		if ((sdp->timeout_length > 0) && (ncpus > 1)) {
			do_timeout = 1;
			mutex_enter(&devctx->lock);
			while (devctx->oncpu)
				(void) cv_wait_sig(&devctx->cv, &devctx->lock);
			devctx->oncpu = 1;
			mutex_exit(&devctx->lock);
		}

		/*
		 * Call the access callback so that the driver can handle
		 * the fault.
		 */
		ret = (*sdp->m_ops.mapdev_access)
		    (seg, sdp->private_data,
		    (off_t)(addr - seg->s_base));

		/*
		 * If mapdev_access() returned -1, then there was a hardware
		 * error so we need to convert the return value to something
		 * that trap() will understand.  Otherwise, the return value
		 * is already a fault code generated by ddi_mapdev_intercept()
		 * or ddi_mapdev_nointercept().
		 */
		ASSERT(ret >= -1);
		if (ret == -1)
			ret = FC_HWERR;

		/*
		 * Setup the timeout if we need to
		 */
		if (do_timeout) {
			mutex_enter(&devctx->lock);
			if (sdp->timeout_length > 0) {
				devctx->timeout = timeout(segmapdev_ctxto,
				    devctx, sdp->timeout_length);
			} else {
				/*
				 * We don't want to wait so set oncpu to
				 * 0 and wake up anyone waiting.
				 */
				devctx->oncpu = 0;
				cv_signal(&devctx->cv);
			}
			mutex_exit(&devctx->lock);
		}

		mutex_enter(&sdp->lock);

	} else {
		/*
		 * Else the driver is not interested in being
		 * notified that this mapdev segment is faulting so
		 * just fault in the pages.
		 */
		ret = segmapdev_faultpages(hat, seg, addr, len,
		    type, rw);
	}

	/*
	 * Remove the faulting flag.
	 */

	sdp->flags &= ~SEGMAPDEV_FAULTING;

	/*
	 * Wake up any other faults on this segment
	 * that are waiting to complete.
	 */
	cv_signal(&sdp->wait);
	mutex_exit(&sdp->lock);
	return (ret);

}

/*
 * Asynchronous page fault.  We simply do nothing since this
 * entry point is not supposed to load up the translation.
 */
/*ARGSUSED*/
static faultcode_t
segmapdev_faulta(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

static int
segmapdev_setprot(
	register struct seg *seg,
	register caddr_t addr,
	register size_t len,
	register u_int prot)
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register struct vpage *vp, *evp;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if ((sdp->maxprot & prot) != prot)
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
			sdp->vpage =
			    kmem_zalloc(vpgtob(seg_pages(seg)), KM_SLEEP);
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

	if ((prot & PROT_WRITE) != 0 || (prot & ~PROT_USER) == PROT_NONE) {
		hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD);
	} else {
		/*
		 * RFE: the segment should keep track of all attributes
		 * allowing us to remove the deprecated hat_chgprot
		 * and use hat_chgattr.
		 */
		hat_chgprot(seg->s_as->a_hat, addr, len, prot);
	}
	return (0);
}

static int
segmapdev_checkprot(
	register struct seg *seg,
	register caddr_t addr,
	register size_t len,
	register u_int prot)
{
	struct segmapdev_data *sdp = (struct segmapdev_data *)seg->s_data;
	register struct vpage *vp, *evp;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * If segment protection can be used, simply check against them
	 */
	mutex_enter(&sdp->lock);
	if (sdp->pageprot == 0) {
		register int err;

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
segmapdev_getprot(
	register struct seg *seg,
	register caddr_t addr,
	register size_t len,
	register u_int *protv)
{
	struct segmapdev_data *sdp = (struct segmapdev_data *)seg->s_data;
	register ulong pgno;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;
	if (pgno != 0) {
		mutex_enter(&sdp->lock);
		if (sdp->pageprot == 0) {
			do
				protv[--pgno] = sdp->prot;
			while (pgno != 0);
		} else {
			register ulong pgoff = seg_page(seg, addr);

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
segmapdev_getoffset(register struct seg *seg, caddr_t addr)
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return ((u_offset_t)sdp->offset);
}

/*ARGSUSED*/
static int
segmapdev_gettype(register struct seg *seg, caddr_t addr)
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return ((sdp->flags & MAP_TYPE));
}


/*ARGSUSED*/
static int
segmapdev_getvp(register struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Note that this vp is the common_vp of the device, where the
	 * devpages are hung ..
	 */
	*vpp = VTOCVP(sdp->vp);

	return (0);
}

static void
segmapdev_badop(void)
{
	cmn_err(CE_PANIC, "segmapdev_badop");
	/*NOTREACHED*/
}

/*
 * segmapdev pages are not in the cache, and thus can't really be controlled.
 * Hence, syncs are simply always successful.
 */
/*ARGSUSED*/
static int
segmapdev_sync(struct seg *seg, caddr_t addr, size_t len, int attr, u_int flags)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segmapdev pages are always "in core".
 */
/*ARGSUSED*/
static size_t
segmapdev_incore(struct seg *seg, caddr_t addr,
    register size_t len, register char *vec)
{
	register size_t v = 0;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	for (len = (len + PAGEOFFSET) & PAGEMASK; len; len -= PAGESIZE,
	    v += PAGESIZE)
		*vec++ = 1;
	return (v);
}

/*
 * segmapdev pages are not in the cache, and thus can't really be controlled.
 * Hence, locks are simply always successful.
 */
/*ARGSUSED*/
static int
segmapdev_lockop(struct seg *seg, caddr_t addr,
    size_t len, int attr, int op, u_long *lockmap, size_t pos)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segmapdev pages are not in the cache, and thus can't really be controlled.
 * Hence, advise is simply always successful.
 */
/*ARGSUSED*/
static int
segmapdev_advise(struct seg *seg, caddr_t addr, size_t len, u_int behav)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segmapdev pages are not dumped, so we just return
 */
/*ARGSUSED*/
static void
segmapdev_dump(struct seg *seg)
{
}

/*ARGSUSED*/
static int
segmapdev_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*
 * segmapdev_inter()
 *
 * Called from ddi_mapdev_intercept
 *
 * Marks the mapdev segment or individual pages with in that segment as
 * intercept and then unloads those pages to force a fault the next
 * time they are accessed.
 */
static int
segmapdev_inter(struct seg *seg, caddr_t addr, off_t len)
{
	register u_char	*vpage_inter;
	register caddr_t	a;
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register u_char			*evp;

	mutex_enter(&sdp->lock);

	/*
	 * If we are intercepting the entire segment, then mark the segment
	 * as intercept and remove any per page interception information.
	 */
	if ((addr == seg->s_base) && (len == seg->s_size)) {
		sdp->flags |= SEGMAPDEV_INTER;
		if (sdp->vpage_inter) {
			kmem_free(sdp->vpage_inter, seg_pages(seg));
			sdp->vpage_inter = NULL;
		}

		/*
		 * Unload the pages to force a fault next time they are
		 * accessed
		 * This unload is done for a different address space,
		 * thus the HAT_UNLOAD_OTHER
		 */
		hat_unload(seg->s_as->a_hat, seg->s_base, seg->s_size,
			HAT_UNLOAD_OTHER);

		mutex_exit(&sdp->lock);
		return (0);
	}

	/*
	 * If we don't already have per page interception information
	 * allocate the per page data structure (array of bytes) and initialize
	 * them to the same value as the segment interception flag.
	 */
	if (!sdp->vpage_inter) {
		sdp->vpage_inter = kmem_zalloc(seg_pages(seg), KM_SLEEP);
		evp = &sdp->vpage_inter[seg_pages(seg)];
		for (vpage_inter = sdp->vpage_inter; vpage_inter < evp;
		    vpage_inter++) {
			*vpage_inter =
			    ((sdp->flags & SEGMAPDEV_INTER) == SEGMAPDEV_INTER);
		}
		/*
		 * Turn off the segment intercept flag as we can't
		 * have both the segment intercept flag and per page
		 * intercept info at the same time.
		 */
		sdp->flags &= ~SEGMAPDEV_INTER;
	}

	/*
	 * For each page in addr -> addr+len mark those pages as
	 * interceptable.
	 */
	for (a = addr; a < (addr + len); a += PAGESIZE) {
		sdp->vpage_inter[seg_page(seg, a)] = 1;
	}


	/*
	 * Unload the pages to force a fault next time they are
	 * accessed
	 * This unload is done for a different address space,
	 * thus the HAT_UNLOAD_OTHER
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_OTHER);

	mutex_exit(&sdp->lock);
	return (0);
}

/*
 * segmapdev_faultpages
 *
 * Used to fault in mapdev segment pages instead of segmapdev_fault.  Called
 * from segmapdev_fault or segmapdev_nointer.  This routine returns a
 * faultcode_t.  The faultcode_t value as a return value for segmapdev_fault.
 */
static int
segmapdev_faultpages(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* seg_dev of interest */
	caddr_t addr,			/* address in as */
	size_t len,			/* the length of the range */
	enum fault_type type,		/* type of fault */
	enum seg_rw rw)			/* type of access at fault */
{
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register caddr_t a;
	register struct vpage *vpage;
	register u_int *vpage_hat_flags;
	int err;
	long page;
	cred_t *cr = CRED();

	ASSERT(mutex_owned(&sdp->lock));
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * First handle the easy stuff
	 */
	if (type == F_SOFTUNLOCK) {
		segmapdev_softunlock(hat, seg, addr, len, rw, cr);
		return (0);
	}

	/*
	 * If we have the same protections for the entire segment,
	 * insure that the access being attempted is legitimate.
	 *
	 * Per page protections are checked in segmapdev_faultpage.
	 */
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

		if ((sdp->prot & protchk) == 0)
			return (FC_PROT);	/* illegal access type */
	}

	page = seg_page(seg, addr);

	if (sdp->vpage == NULL)
		vpage = NULL;
	else
		vpage = &sdp->vpage[page];

	if (sdp->pagehat_flags)
		vpage_hat_flags = &sdp->vpage_hat_flags[page];
	else
		vpage_hat_flags = &sdp->hat_flags;

	/*
	 * loop over the address range handling each fault
	 *
	 * XXXX This should eventually use hat_contig_devload.  Currently it
	 * calls segmapdev_faultpage which calls hat_devload
	 *
	 * Per page protections are checked in segmapdev_faultpage.
	 */

	for (a = addr; a < addr + len; a += PAGESIZE) {
		err = segmapdev_faultpage(hat, seg, a, vpage, vpage_hat_flags,
			type, rw, cr);
		if (err) {
			if (type == F_SOFTLOCK && a > addr)
				segmapdev_softunlock(hat, seg, addr,
				    (uintptr_t)(a - addr), S_OTHER, cr);
			/* FC_MAKE_ERR done by segmapdev_faultpage */
			return (err);
		}

		if (sdp->pagehat_flags)
			vpage_hat_flags++;

		if (vpage != NULL)
			vpage++;
	}
	return (0);
}

/*
 * segmapdev_nointer()
 *
 * Called from ddi_mapdev_nointercept
 *
 * Marks the mapdev segment or individual pages with in that segment as
 * no intercept and then fault in the pages if this segment is faulting.
 * If this segment is not faulting, but the driver is just expressing
 * dis-interest in these pages, just mark them as no intercept so that when
 * do get a fault on these pages, we can just map them in without notifing
 * the driver.
 */
static int
segmapdev_nointer(struct seg *seg, caddr_t addr, off_t len)
{

	register u_char		*vpage_inter;
	register caddr_t	a;
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register u_char		*evp;
	int			err;

	mutex_enter(&sdp->lock);

	/*
	 * If we are not intercepting the entire segment, then mark the segment
	 * as no intercept and remove any per page interception information.
	 */
	if ((addr == seg->s_base) && (len == seg->s_size)) {
		sdp->flags &= ~SEGMAPDEV_INTER;
		if (sdp->vpage_inter) {
			kmem_free(sdp->vpage_inter, seg_pages(seg));
			sdp->vpage_inter = NULL;
		}
	} else {

		/*
		 * If we don't already have per page interception information
		 * allocate the per page data structure (array of bytes) and
		 * initialize them to the same value as the segment
		 * interception flag.
		 */
		if (!sdp->vpage_inter) {
			sdp->vpage_inter = kmem_zalloc(seg_pages(seg),
			    KM_SLEEP);
			evp = &sdp->vpage_inter[seg_pages(seg)];
			for (vpage_inter = sdp->vpage_inter; vpage_inter < evp;
			    vpage_inter++) {
				*vpage_inter =
				    ((sdp->flags & SEGMAPDEV_INTER) ==
				    SEGMAPDEV_INTER);
			}
			/*
			 * Turn off the segment intercept flag as we can't
			 * have both the segment intercept flag and per page
			 * intercept info at the same time.
			 */
			sdp->flags &= ~SEGMAPDEV_INTER;
		}

		/*
		 * For each page in addr -> addr+len mark those pages as
		 * not interceptable.
		 */
		for (a = addr; a < (addr + len); a += PAGESIZE) {
			sdp->vpage_inter[seg_page(seg, a)] = 0;
		}
	}

	/*
	 * If this segment is not faulting then the driver just wanted to
	 * mark this segment as nointercept.  So just return.
	 */
	if (!(sdp->flags & SEGMAPDEV_FAULTING)) {
		mutex_exit(&sdp->lock);
		return (0);
	}


	err = segmapdev_faultpages(sdp->hat, seg, addr, len,
	    sdp->type, sdp->rw);

	mutex_exit(&sdp->lock);
	return (err);
}

/*
 * segmapdev_set_access_attr()
 *
 * Called from ddi_mapdev_set_access_attr
 *
 * Marks the mapdev segment or individual pages within that segment with
 * hat-related access attributes and then unloads the pages so they are
 * faulted in with the new attributes.
 */
static int
segmapdev_set_access_attr(struct seg *seg, caddr_t addr, off_t len,
    u_int hat_flags)
{
	register u_int		*vpage_hat_flags;
	register caddr_t	a;
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	register u_int		*evp;

	/* hat_flags should not have PROT_* set */
	ASSERT(!(hat_flags & HAT_PROT_MASK));
	mutex_enter(&sdp->lock);

	/*
	 * If we are assigning hat-flags for the entire segment, then mark the
	 * segment with hat-flags and remove any per page hat-flags information.
	 */
	if ((addr == seg->s_base) && (len == seg->s_size)) {
		sdp->pagehat_flags = 0;
		sdp->hat_flags = hat_flags;
		if (sdp->vpage_hat_flags) {
			kmem_free(sdp->vpage_hat_flags, seg_pages(seg));
			sdp->vpage_hat_flags = NULL;
		}

		/*
		 * Unload the pages to force a fault next time they are
		 * accessed
		 */
		hat_unload(seg->s_as->a_hat, seg->s_base, seg->s_size,
		    HAT_UNLOAD);

		mutex_exit(&sdp->lock);
		return (0);
	}

	/*
	 * If we don't already have per page hat-flags information
	 * allocate the per page data structure (array of u_ints) and initialize
	 * them to the same value as the segment hat-flags flag.
	 */
	if (!sdp->vpage_hat_flags) {
		sdp->vpage_hat_flags = kmem_zalloc(seg_pages(seg), KM_SLEEP);
		evp = &sdp->vpage_hat_flags[seg_pages(seg)];
		for (vpage_hat_flags = sdp->vpage_hat_flags;
		    vpage_hat_flags < evp; vpage_hat_flags++) {
			*vpage_hat_flags = sdp->hat_flags;
		}

		/*
		 * Turn off the segment hat-flags flag as we can't
		 * have both the segment hat-flags flag and per page
		 * hat-flags info at the same time.
		 */
		sdp->pagehat_flags = 1;
		sdp->hat_flags = 0;
	}

	/*
	 * For each page in addr -> addr+len mark those pages as
	 * interceptable.
	 */
	for (a = addr; a < (addr + len); a += PAGESIZE) {
		sdp->vpage_hat_flags[seg_page(seg, a)] = hat_flags;
	}


	/*
	 * Unload the pages to force a fault next time they are
	 * accessed
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD);

	mutex_exit(&sdp->lock);
	return (0);
}

/*
 * XXXX	The below code is to make this a loadable module.  This code needs
 *	to be removed if seg_mapdev gets merged back into the kernel.
 */

#include <sys/modctl.h>

int _fini(void);
int _info(struct modinfo *modinfop);
int _init(void);

static	struct modlmisc modlmisc = {
	&mod_miscops,
	"Mapdev Segment Device Driver",
};

static	struct modlinkage modlinkage = {
	MODREV_1,
	&modlmisc,
	NULL
};

int
_init(void)
{
	int	e;

	mutex_init(&devctx_lock, NULL, MUTEX_DEFAULT, NULL);
	if ((e = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&devctx_lock);
		return (e);
	}
	return (0);
}

int
_fini(void)
{
#ifdef DEBUG_MAPDEV
	int	error;

	mutex_enter(&devctx_lock)
	if (devctx_list != NULL) {
		mutex_exit(&devctx_lock);
		return (EBUSY);
	}
	mutex_exit(&devctx_lock);

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);
	mutex_destroy(&devctx_lock);
	return (0);
#else
	/*
	 * Don't allow module to be unloaded.  We don't know if a new thread
	 * starts using devctx_list.  And to check would cause a race
	 * condition.
	 */
	return (EBUSY);
#endif
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * XXXX	The below code is to make this a loadbale module.  This code needs to
 *	be merged into sunddi.c if seg_mapdev gets merged back into the
 *	kernel.
 */

/*
 * ddi_mapdev:		Used by drivers who wish to be notified that a
 *			segment is faulting so that they can do context
 *			switching.  Called from a drivers segmap(9E) routine.
 */
/*ARGSUSED*/
int
ddi_mapdev(dev_t dev, off_t off, struct as *as, caddr_t *addrp, off_t len,
    u_int prot, u_int maxprot, u_int flags, cred_t *cred,
    struct ddi_mapdev_ctl *m_ops, ddi_mapdev_handle_t *handle,
    void *private_data)
{
	struct segmapdev_crargs dev_a;
	int (*mapfunc)(dev_t dev, off_t off, int prot);
	int	error;

	if ((mapfunc = devopsp[getmajor(dev)]->devo_cb_ops->cb_mmap) ==
	    nodev)
		return (ENODEV);

	/*
	 * Its up to the driver to make sure that the mapping
	 * is in the correct range and to return an error if
	 * MAP_PRIVATE is not supported.
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
	dev_a.m_ops = m_ops;
	dev_a.private_data = private_data;
	dev_a.handle = handle;

	error = as_map(as, *addrp, len, segmapdev_create, &dev_a);
	as_rangeunlock(as);
	return (error);

}

/*
 * ddi_mapdev_intercept:
 *			Marks a mapdev segment or pages if offset->offset+len
 *			is not the entire segment as intercept and unloads the
 *			pages in the range offset -> offset+len.
 */
int
ddi_mapdev_intercept(ddi_mapdev_handle_t handle, off_t offset, off_t len)
{

	register struct seg *seg = (struct seg *)handle;
	caddr_t	addr;
	off_t	size;

	if (offset > seg->s_size)
		return (0);

	/*
	 * Address and size must be page aligned.  Len is set to the
	 * number of bytes in the number of pages that are required to
	 * support len.  Offset is set to the byte offset of the first byte
	 * of the page that contains offset.
	 */
	len = mmu_ptob(mmu_btopr(len));
	offset = mmu_ptob(mmu_btop(offset));

	/*
	 * If len is == 0, then calculate the size by getting
	 * the number of bytes from offset to the end of the segment.
	 */
	if (len == 0)
		size = seg->s_size - offset;
	else {
		size = len;
		if ((offset+size) > seg->s_size)
			return (0);
	}

	/*
	 * The address is offset bytes from the base address of
	 * the segment.
	 */
	addr = offset + seg->s_base;

	return (segmapdev_inter(seg, addr, size));

}

/*
 * ddi_mapdev_nointercept:
 *			Marks a mapdev segment or pages if offset->offset+len
 *			is not the entire segment as nointercept and faults in
 *			the pages in the range offset -> offset+len.
 */
int
ddi_mapdev_nointercept(ddi_mapdev_handle_t handle, off_t offset, off_t len)
{
	register struct seg *seg = (struct seg *)handle;
	caddr_t	addr;
	off_t	size;

	if (offset > seg->s_size)
		return (DDI_FAILURE);

	/*
	 * Address and size must be page aligned.  Len is set to the
	 * number of bytes in the number of pages that are required to
	 * support len.  Offset is set to the byte offset of the first byte
	 * of the page that contains offset.
	 */
	len = mmu_ptob(mmu_btopr(len));
	offset = mmu_ptob(mmu_btop(offset));

	/*
	 * If len is == 0, then calculate the size by getting
	 * the number of bytes from offset to the end of the segment.
	 */
	if (len == 0)
		size = seg->s_size - offset;
	else {
		size = len;
		if ((offset+size) > seg->s_size)
			return (FC_MAKE_ERR(EINVAL));
	}

	/*
	 * The address is offset bytes from the base address of
	 * the segment.
	 */
	addr = offset + seg->s_base;

	return (segmapdev_nointer(seg, addr, size));
}


/*
 * ddi_mapdev_set_device_acc_attr:
 *			Assigns a mapdev segment or pages, if offset->offset+len
 *			is not the entire segment, with the attributes defined
 *			by the access(9s) and then unloads the pages so they
 *			are faulted in with those attributes.
 */
int
ddi_mapdev_set_device_acc_attr(ddi_mapdev_handle_t handle, off_t offset,
    off_t len, ddi_device_acc_attr_t *accattrp, uint_t rnumber)
{
	register struct seg *seg = (struct seg *)handle;
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;
	caddr_t	addr;
	off_t	size;
	u_int hat_flags;

	if (offset > seg->s_size)
		return (ENXIO);

	/*
	 * Address and size must be page aligned.  Len is set to the
	 * number of bytes in the number of pages that are required to
	 * support len.  Offset is set to the byte offset of the first byte
	 * of the page that contains offset.
	 */
	len = mmu_ptob(mmu_btopr(len));
	offset = mmu_ptob(mmu_btop(offset));

	/*
	 * If len is == 0, then calculate the size by getting
	 * the number of bytes from offset to the end of the segment.
	 */
	if (len == 0)
		size = seg->s_size - offset;
	else {
		size = len;
		if ((offset+size) > seg->s_size)
			return (FC_MAKE_ERR(EINVAL));
	}

	/*
	 * The address is offset bytes from the base address of
	 * the segment.
	 */
	addr = offset + seg->s_base;

	/*
	 * Check that this region is indeed mappable on this platform.
	 * Use the mapping function.
	 */
	if (ddi_device_mapping_check(sdp->dev, accattrp, rnumber, &hat_flags)
	    == -1)
		return (ENXIO);

	return (segmapdev_set_access_attr(seg, addr, size, hat_flags));
}

void
ddi_mapdev_set_keep_ctx(ddi_mapdev_handle_t handle, clock_t ticks)
{
	register struct seg *seg = (struct seg *)handle;
	register struct segmapdev_data *sdp =
	    (struct segmapdev_data *)seg->s_data;

	sdp->timeout_length = ticks;
}

static int
segmapdev_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	register struct segmapdev_data *sdp =
				(struct segmapdev_data *)seg->s_data;

	if (sdp->flags & MAP_PRIVATE) {
		memidp->val[0] = (u_longlong_t)seg->s_as;
		memidp->val[1] = (u_longlong_t)addr;
	} else if (sdp->flags & MAP_SHARED) {
		memidp->val[0] = (u_longlong_t)VTOCVP(sdp->vp);
		memidp->val[1] = sdp->offset + (addr - seg->s_base);
	}
	return (0);
}
