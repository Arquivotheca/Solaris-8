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
 * 	Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma	ident	"@(#)seg_dev.c	1.108	99/09/23 SMI"

/*
 * VM - segment of a mapped device.
 *
 * This segment driver is used when mapping character special devices.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/sysmacros.h>
#include <sys/vtrace.h>
#include <sys/vmsystm.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/ddidevmap.h>

#include <vm/page.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_dev.h>
#include <vm/seg_kp.h>
#include <vm/vpage.h>

#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/fs/snode.h>

#if DEBUG
int segdev_debug;
#define	DEBUGF(level, args) { if (segdev_debug >= (level)) cmn_err args; }
#else
#define	DEBUGF(level, args)
#endif

#define	CTX_TIMEOUT_VALUE 0
#define	HOLD_COOKIE_LOCK(cookie)  { mutex_enter(&cookie->lock); \
			while (cookie->locked) \
				cv_wait(&cookie->cv, \
				&cookie->lock); \
			cookie->locked = 1;	\
			mutex_exit(&cookie->lock); }
#define	RELE_COOKIE_LOCK(cookie) { mutex_enter(&cookie->lock); \
			ASSERT(cookie->locked); \
			cookie->locked = 0; \
			cv_signal(&cookie->cv); \
			mutex_exit(&cookie->lock); }

#define	HOLD_SOFTLOCK_LOCK(slock, pages)  { mutex_enter(&slock->lock); \
			while (slock->softlocked) \
				cv_wait(&slock->cv, &slock->lock); \
			slock->softlocked += pages;	\
			mutex_exit(&slock->lock); }
#define	CHK_SOFTLOCK_LOCK(slock)  { mutex_enter(&slock->lock); \
			while (slock->softlocked) \
				cv_wait(&slock->cv, &slock->lock); \
			mutex_exit(&slock->lock); }
#define	RELE_SOFTLOCK_LOCK(slock, pages) { mutex_enter(&slock->lock); \
			ASSERT(slock->softlocked); \
			slock->softlocked -= pages; \
			cv_signal(&slock->cv); \
			mutex_exit(&slock->lock); }

#define	HOLD_DHP_LOCK(dhp)  { mutex_enter(&dhp->dh_lock); \
			while (dhp->dh_flags & DEVMAP_FAULTING) \
				cv_wait(&dhp->dh_cv, &dhp->dh_lock); \
			dhp->dh_flags |= DEVMAP_FAULTING; \
			mutex_exit(&dhp->dh_lock); }
#define	RELE_DHP_LOCK(dhp) { mutex_enter(&dhp->dh_lock); \
			if (dhp->dh_flags & DEVMAP_FAULTING) { \
				dhp->dh_flags &= ~DEVMAP_FAULTING; \
				cv_signal(&dhp->dh_cv); \
			} \
			mutex_exit(&dhp->dh_lock); }

#define	round_down_p2(a, s)	((a) & ~((s) - 1))
#define	round_up_p2(a, s)	(((a) + (s) - 1) & ~((s) - 1))

/*
 * to check if user and physical virtual addresses are aligned
 * with pgsize.
 */
#define	VA_PA_ALIGNED(uvaddr, paddr, pgsize)		\
	(((uvaddr | paddr) & (pgsize - 1)) == 0)
#define	VA_PA_PGSIZE_ALIGNED(uvaddr, paddr, pgsize)	\
	(((uvaddr ^ paddr) & (pgsize - 1)) == 0)

#define	vpgtob(n)	((n) * sizeof (struct vpage))	/* For brevity */

#define	VTOCVP(vp)	(VTOS(vp)->s_commonvp)	/* we "know" it's an snode */

static struct devmap_ctx *devmapctx_list = NULL;
static struct devmap_softlock *devmap_slist = NULL;

/*
 * Private seg op routines.
 */
static int	segdev_dup(struct seg *, struct seg *);
static int	segdev_unmap(struct seg *, caddr_t, size_t);
static void	segdev_free(struct seg *);
static faultcode_t segdev_fault(struct hat *, struct seg *, caddr_t, size_t,
		    enum fault_type, enum seg_rw);
static faultcode_t segdev_faulta(struct seg *, caddr_t);
static int	segdev_setprot(struct seg *, caddr_t, size_t, u_int);
static int	segdev_checkprot(struct seg *, caddr_t, size_t, u_int);
static void	segdev_badop(void);
static int	segdev_sync(struct seg *, caddr_t, size_t, int, u_int);
static size_t	segdev_incore(struct seg *, caddr_t, size_t, char *);
static int	segdev_lockop(struct seg *, caddr_t, size_t, int, int,
		    ulong *, size_t);
static int	segdev_getprot(struct seg *, caddr_t, size_t, u_int *);
static u_offset_t	segdev_getoffset(struct seg *, caddr_t);
static int	segdev_gettype(struct seg *, caddr_t);
static int	segdev_getvp(struct seg *, caddr_t, struct vnode **);
static int	segdev_advise(struct seg *, caddr_t, size_t, u_int);
static void	segdev_dump(struct seg *);
static int	segdev_pagelock(struct seg *, caddr_t, size_t,
		    struct page ***, enum lock_type, enum seg_rw);
static int	segdev_getmemid(struct seg *, caddr_t, memid_t *);

/*
 * XXX	this struct is used by rootnex_map_fault to identify
 *	the segment it has been passed. So if you make it
 *	"static" you'll need to fix rootnex_map_fault.
 */
struct seg_ops segdev_ops = {
	segdev_dup,
	segdev_unmap,
	segdev_free,
	segdev_fault,
	segdev_faulta,
	segdev_setprot,
	segdev_checkprot,
	(int (*)())segdev_badop,	/* kluster */
	(size_t (*)(struct seg *))NULL,	/* swapout */
	segdev_sync,			/* sync */
	segdev_incore,
	segdev_lockop,			/* lockop */
	segdev_getprot,
	segdev_getoffset,
	segdev_gettype,
	segdev_getvp,
	segdev_advise,
	segdev_dump,
	segdev_pagelock,
	segdev_getmemid,
};

/*
 * Private segdev support routines
 */
static struct segdev_data *sdp_alloc(void);

static void segdev_softunlock(struct hat *, struct seg *, caddr_t,
    size_t, enum seg_rw, struct cred *);

static int segdev_faultpage(struct hat *, struct seg *, caddr_t,
    struct vpage *, enum fault_type, enum seg_rw, struct cred *, size_t);

static int segdev_faultpages(struct hat *, struct seg *, caddr_t, size_t,
    enum fault_type, enum seg_rw, devmap_handle_t *dhp);

static struct devmap_ctx *devmap_ctxinit(dev_t, u_long);
static struct devmap_softlock *devmap_softlock_init(dev_t, u_long);
static void devmap_softlock_rele(devmap_handle_t *);
static void devmap_ctx_rele(devmap_handle_t *);

static void devmap_ctxto(void *);

static devmap_handle_t *devmap_find_handle(devmap_handle_t *dhp_head,
    caddr_t addr);

static u_long devmap_roundup(devmap_handle_t *dhp, u_long offset, size_t len,
    u_long *opfn, u_long *pagesize);

static void free_devmap_handle(devmap_handle_t *dhp);

static int devmap_handle_dup(devmap_handle_t *dhp, devmap_handle_t **new_dhp,
    struct seg *newseg);

static devmap_handle_t *devmap_handle_unmap(devmap_handle_t *dhp);

static void devmap_handle_unmap_head(devmap_handle_t *dhp, size_t len);

static void devmap_handle_unmap_tail(devmap_handle_t *dhp, caddr_t addr);

static int devmap_device(devmap_handle_t *dhp, struct as *as, caddr_t *addr,
    offset_t off, size_t len, u_int flags);

static void devmap_get_large_pgsize(devmap_handle_t *dhp, size_t len,
    caddr_t addr, size_t *llen, caddr_t *laddr);

static void devmap_handle_change_len(devmap_handle_t *dhp, size_t len);

/*
 * external functions
 */

extern size_t page_get_pagesize(u_int);

extern u_int page_num_pagesizes();

static kmutex_t devmapctx_lock;

static kmutex_t devmap_slock;

/*
 * Initialize the thread callbacks and thread private data.
 */
static struct devmap_ctx *
devmap_ctxinit(dev_t dev, u_long id)
{
	struct devmap_ctx	*devctx;
	struct devmap_ctx	*tmp;
	dev_info_t		*dip;

	tmp =  kmem_zalloc(sizeof (struct devmap_ctx), KM_SLEEP);

	mutex_enter(&devmapctx_lock);

	dip = e_ddi_get_dev_info(dev, VCHR);
	ASSERT(dip != NULL);
	ddi_rele_driver(getmajor(dev));

	for (devctx = devmapctx_list; devctx != NULL; devctx = devctx->next)
		if ((devctx->dip == dip) && (devctx->id == id))
			break;

	if (devctx == NULL) {
		devctx = tmp;
		devctx->dip = dip;
		devctx->id = id;
		mutex_init(&devctx->lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&devctx->cv, NULL, CV_DEFAULT, NULL);
		devctx->next = devmapctx_list;
		devmapctx_list = devctx;
	} else
		kmem_free(tmp, sizeof (struct devmap_ctx));

	mutex_enter(&devctx->lock);
	devctx->refcnt++;
	mutex_exit(&devctx->lock);
	mutex_exit(&devmapctx_lock);

	return (devctx);
}

/*
 * Timeout callback called if a CPU has not given up the device context
 * within dhp->dh_timeout_length ticks
 */
static void
devmap_ctxto(void *data)
{
	struct devmap_ctx *devctx = data;

	TRACE_1(TR_FAC_DEVMAP, TR_DEVMAP_CTXTO,
	    "devmap_ctxto:timeout expired, devctx=%p", (void *)devctx);
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
int
segdev_create(struct seg *seg, void *argsp)
{
	struct segdev_data *sdp;
	struct segdev_crargs *a = (struct segdev_crargs *)argsp;
	devmap_handle_t *dhp = (devmap_handle_t *)a->devmap_data;
	int error;

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segdev" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * The following call to hat_map presumes that translation
	 * resources are set up by the system MMU. This may cause
	 * problems when the resources are allocated/managed by the
	 * device's MMU or an MMU other than the system MMU. For now
	 * hat_map is no-op and not implemented by the Sun MMU, SRMMU, SFMMU,
	 * and X86 MMU drivers and should not pose any problems.
	 */
	hat_map(seg->s_as->a_hat, seg->s_base, seg->s_size, HAT_MAP);

	sdp = sdp_alloc();

	sdp->mapfunc = a->mapfunc;
	sdp->offset = a->offset;
	sdp->prot = a->prot;
	sdp->maxprot = a->maxprot;
	sdp->type = a->type;
	sdp->pageprot = 0;
	sdp->softlockcnt = 0;
	sdp->vpage = NULL;

	if (sdp->mapfunc == NULL)
		sdp->devmap_data = dhp;
	else
		sdp->devmap_data = dhp = NULL;

	sdp->hat_flags = a->hat_flags;
	sdp->hat_attr = a->hat_attr;

	/*
	 * Currently, hat_flags supports only HAT_LOAD_NOCONSIST
	 */
	ASSERT(!(sdp->hat_flags & ~HAT_LOAD_NOCONSIST));

	/*
	 * Hold shadow vnode -- segdev only deals with
	 * character (VCHR) devices. We use the common
	 * vp to hang pages on.
	 */
	sdp->vp = specfind(a->dev, VCHR);
	ASSERT(sdp->vp != NULL);

	seg->s_ops = &segdev_ops;
	seg->s_data = sdp;

	while (dhp != NULL) {
		dhp->dh_seg = seg;
		dhp = dhp->dh_next;
	}

	/*
	 * Inform the vnode of the new mapping.
	 */
	/*
	 * It is ok to use pass sdp->maxprot to ADDMAP rather than to use
	 * dhp specific maxprot because spec_addmap does not use maxprot.
	 */
	error = VOP_ADDMAP(VTOCVP(sdp->vp), sdp->offset,
	    seg->s_as, seg->s_base, seg->s_size,
	    sdp->prot, sdp->maxprot, sdp->type, CRED());

	if (error != 0) {
		sdp->devmap_data = NULL;
		hat_unload(seg->s_as->a_hat, seg->s_base, seg->s_size,
		    HAT_UNLOAD_UNMAP);
	}

	return (error);
}

static struct segdev_data *
sdp_alloc(void)
{
	struct segdev_data *sdp;

	sdp = kmem_zalloc(sizeof (struct segdev_data), KM_SLEEP);
	mutex_init(&sdp->lock, NULL, MUTEX_DEFAULT, NULL);

	return (sdp);
}

/*
 * Duplicate seg and return new segment in newseg.
 */
static int
segdev_dup(struct seg *seg, struct seg *newseg)
{
	struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	struct segdev_data *newsdp;
	devmap_handle_t *dhp = (devmap_handle_t *)sdp->devmap_data;
	size_t npages;
	int ret;

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_DUP,
	    "segdev_dup:start dhp=%p, seg=%p", (void *)dhp, (void *)seg);

	DEBUGF(3, (CE_CONT, "segdev_dup: dhp %p seg %p\n",
	    (void *)dhp, (void *)seg));

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segdev" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	newsdp = sdp_alloc();

	newseg->s_ops = seg->s_ops;
	newseg->s_data = (void *)newsdp;

	VN_HOLD(sdp->vp);
	newsdp->vp 	= sdp->vp;
	newsdp->mapfunc = sdp->mapfunc;
	newsdp->offset	= sdp->offset;
	newsdp->pageprot = sdp->pageprot;
	newsdp->prot	= sdp->prot;
	newsdp->maxprot = sdp->maxprot;
	newsdp->type = sdp->type;
	newsdp->hat_attr = sdp->hat_attr;
	newsdp->hat_flags = sdp->hat_flags;
	newsdp->softlockcnt = 0;

	/*
	 * Initialize per page data if the segment we are
	 * dup'ing has per page information.
	 */
	npages = seg_pages(newseg);

	if (sdp->vpage != NULL) {
		size_t nbytes = vpgtob(npages);

		newsdp->vpage = kmem_zalloc(nbytes, KM_SLEEP);
		bcopy(sdp->vpage, newsdp->vpage, nbytes);
	} else
		newsdp->vpage = NULL;

	/*
	 * duplicate devmap handles
	 */
	if (dhp != NULL) {
		ret = devmap_handle_dup(dhp,
			(devmap_handle_t **)&newsdp->devmap_data, newseg);
		if (ret != 0) {
			TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_DUP_CK1,
			    "segdev_dup:ret1 ret=%x, dhp=%p seg=%p",
			    ret, (void *)dhp, (void *)seg);
			DEBUGF(1, (CE_CONT,
			    "segdev_dup: ret %x dhp %p seg %p\n",
			    ret, (void *)dhp, (void *)seg));
			return (ret);
		}
	}

	/*
	 * Inform the common vnode of the new mapping.
	 */
	return (VOP_ADDMAP(VTOCVP(newsdp->vp),
		newsdp->offset, newseg->s_as,
		newseg->s_base, newseg->s_size, newsdp->prot,
		newsdp->maxprot, sdp->type, CRED()));
}

/*
 * duplicate devmap handles
 */
static int
devmap_handle_dup(devmap_handle_t *dhp, devmap_handle_t **new_dhp,
    struct seg *newseg)
{
	devmap_handle_t *newdhp_save = NULL;
	devmap_handle_t *newdhp = NULL;
	struct devmap_callback_ctl *callbackops;

	while (dhp != NULL) {
		newdhp = kmem_alloc(sizeof (devmap_handle_t), KM_SLEEP);
		bcopy(dhp, newdhp, sizeof (devmap_handle_t));
		newdhp->dh_seg = newseg;
		newdhp->dh_next = NULL;
		if (newdhp_save != NULL)
			newdhp_save->dh_next = newdhp;
		else
			*new_dhp = newdhp;
		newdhp_save = newdhp;

		callbackops = &newdhp->dh_callbackops;

		if (dhp->dh_softlock != NULL)
			newdhp->dh_softlock = devmap_softlock_init(
			    newdhp->dh_dev,
			    (u_long)callbackops->devmap_access);
		if (dhp->dh_ctx != NULL)
			newdhp->dh_ctx = devmap_ctxinit(newdhp->dh_dev,
			    (u_long)callbackops->devmap_access);

		/*
		 * Initialize dh_lock and dh_cv if we want to do remap.
		 */
		if ((newdhp->dh_flags & DEVMAP_ALLOW_REMAP) &&
			(callbackops->devmap_access != NULL)) {
			mutex_init(&newdhp->dh_lock, NULL, MUTEX_DEFAULT, NULL);
			cv_init(&newdhp->dh_cv, NULL, CV_DEFAULT, NULL);
			newdhp->dh_flags |= DEVMAP_LOCK_INITED;
		}

		if (callbackops->devmap_dup != NULL) {
			int ret;

			/*
			 * Call the dup callback so that the driver can
			 * duplicate its private data.
			 */
			ret = (*callbackops->devmap_dup)(dhp, dhp->dh_pvtp,
				(devmap_cookie_t *)newdhp, &newdhp->dh_pvtp);

			if (ret != 0) {
				/*
				 * We want to free up this segment as the driver
				 * has indicated that we can't dup it.  But we
				 * don't want to call the drivers, devmap_unmap,
				 * callback function as the driver does not
				 * think this segment exists. The caller of
				 * devmap_dup will call seg_free on newseg
				 * as it was the caller that allocated the
				 * segment.
				 */
				DEBUGF(1, (CE_CONT, "devmap_handle_dup ERROR: "
				    "newdhp %p dhp %p\n", (void *)newdhp,
				    (void *)dhp));
				callbackops->devmap_unmap = NULL;
				return (ret);
			}
		}

		dhp = dhp->dh_next;
	}

	return (0);
}

/*
 * Split a segment at addr for length len.
 */
/*ARGSUSED*/
static int
segdev_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	register struct segdev_data *nsdp;
	register struct seg *nseg;
	register size_t	opages;		/* old segment size in pages */
	register size_t	npages;		/* new segment size in pages */
	register size_t	dpages;		/* pages being deleted (unmapped) */
	register size_t	nbytes;
	devmap_handle_t *dhp = (devmap_handle_t *)sdp->devmap_data;
	devmap_handle_t *dhpp;
	devmap_handle_t *newdhp;
	struct devmap_callback_ctl *callbackops;
	caddr_t nbase;
	offset_t off;
	u_long nsize;
	size_t mlen;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_UNMAP,
	    "segdev_unmap:start dhp=%p, seg=%p addr=%p len=%lx",
	    (void *)dhp, (void *)seg, (void *)addr, len);

	DEBUGF(3, (CE_CONT, "segdev_unmap: dhp %p seg %p addr %p len %lx\n",
	    (void *)dhp, (void *)seg, (void *)addr, len));

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segdev" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if (sdp->softlockcnt > 0) {
		/*
		 * Fail the unmap if pages are SOFTLOCKed through this mapping.
		 * softlockcnt is protected from change by the as write lock.
		 */
		TRACE_1(TR_FAC_DEVMAP, TR_DEVMAP_UNMAP_CK1,
		    "segdev_unmap:error softlockcnt = %ld",
		    sdp->softlockcnt);
		DEBUGF(1, (CE_CONT, "segdev_unmap: softlockcnt %ld\n",
		    sdp->softlockcnt));
		return (EAGAIN);
	}

	/*
	 * Check for bad sizes
	 */
	if (addr < seg->s_base || addr + len > seg->s_base + seg->s_size ||
	    (len & PAGEOFFSET) || ((uintptr_t)addr & PAGEOFFSET))
		cmn_err(CE_PANIC, "segdev_unmap");

	if (dhp != NULL) {
		u_int large_page = 0;
		/*
		 * XXX
		 * If large page size was used in hat_devload(),
		 * the same page size must be used in hat_unload().
		 */
		dhpp = devmap_find_handle(dhp, addr);
		while (dhpp != NULL) {
			if (dhpp->dh_flags & DEVMAP_FLAG_LARGE) {
				large_page++;
				break;
			}
			dhpp = dhpp->dh_next;
		}
		if (large_page) {
			size_t slen = len;
			size_t mlen;
			size_t soff;

			dhpp = devmap_find_handle(dhp, addr);
			soff = (u_long)(addr - dhpp->dh_uvaddr);
			while ((long)slen > 0) {
				mlen = MIN(slen, (dhpp->dh_len - soff));
				hat_unload(seg->s_as->a_hat, dhpp->dh_uvaddr,
					dhpp->dh_len, HAT_UNLOAD_UNMAP);
				dhpp = dhpp->dh_next;
				slen -= mlen;
				soff = 0;
			}
		} else
			hat_unload(seg->s_as->a_hat, addr, len,
				HAT_UNLOAD_UNMAP);
	} else {
		/*
		 * Unload any hardware translations in the range
		 * to be taken out.
		 */
		hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);
	}

	/*
	 * get the user offset which will used in the driver callbacks
	 */
	off = sdp->offset + (offset_t)(addr - seg->s_base);

	/*
	 * Inform the vnode of the unmapping.
	 */
	ASSERT(sdp->vp != NULL);
	VOP_DELMAP(VTOCVP(sdp->vp), off, seg->s_as, addr, len,
		sdp->prot, sdp->maxprot, sdp->type, CRED());

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
			register struct vpage *ovpage;

			ovpage = sdp->vpage;	/* keep pointer to vpage */

			nbytes = vpgtob(npages);
			sdp->vpage = kmem_alloc(nbytes, KM_SLEEP);
			bcopy(&ovpage[dpages], sdp->vpage, nbytes);

			/* free up old vpage */
			kmem_free(ovpage, vpgtob(opages));
		}

		/*
		 * free devmap handles from the beginning of the mapping.
		 */
		if (dhp != NULL)
			devmap_handle_unmap_head(dhp, (u_long)len);

		sdp->offset += (offset_t)len;

		seg->s_base += len;
		seg->s_size -= len;

		return (0);
	}

	/*
	 * Check for end of segment
	 */
	if (addr + len == seg->s_base + seg->s_size) {
		if (sdp->vpage != NULL) {
			register struct vpage *ovpage;

			ovpage = sdp->vpage;	/* keep pointer to vpage */

			nbytes = vpgtob(npages);
			sdp->vpage = kmem_alloc(nbytes, KM_SLEEP);
			bcopy(ovpage, sdp->vpage, nbytes);

			/* free up old vpage */
			kmem_free(ovpage, vpgtob(opages));
		}
		seg->s_size -= len;

		/*
		 * free devmap handles from addr to the end of the mapping.
		 */
		if (dhp != NULL)
			devmap_handle_unmap_tail(dhp, addr);

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
		cmn_err(CE_PANIC, "segdev_unmap seg_alloc");

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_UNMAP_CK2,
	    "segdev_unmap: seg=%p nseg=%p", (void *)seg, (void *)nseg);
	DEBUGF(3, (CE_CONT, "segdev_unmap: segdev_dup seg %p nseg %p\n",
	    (void *)seg, (void *)nseg));
	nsdp = sdp_alloc();

	nseg->s_ops = seg->s_ops;
	nseg->s_data = (void *)nsdp;

	VN_HOLD(sdp->vp);
	nsdp->mapfunc = sdp->mapfunc;
	nsdp->offset = sdp->offset + (offset_t)(nseg->s_base - seg->s_base);
	nsdp->vp 	= sdp->vp;
	nsdp->pageprot = sdp->pageprot;
	nsdp->prot	= sdp->prot;
	nsdp->maxprot = sdp->maxprot;
	nsdp->type = sdp->type;
	nsdp->hat_attr = sdp->hat_attr;
	nsdp->hat_flags = sdp->hat_flags;
	nsdp->softlockcnt = 0;

	/*
	 * Initialize per page data if the segment we are
	 * dup'ing has per page information.
	 */
	if (sdp->vpage != NULL) {
		/* need to split vpage into two arrays */
		register size_t nnbytes;
		register size_t nnpages;
		register struct vpage *ovpage;

		ovpage = sdp->vpage;		/* keep pointer to vpage */

		npages = seg_pages(seg);	/* seg has shrunk */
		nbytes = vpgtob(npages);
		nnpages = seg_pages(nseg);
		nnbytes = vpgtob(nnpages);

		sdp->vpage = kmem_alloc(nbytes, KM_SLEEP);
		bcopy(ovpage, sdp->vpage, nbytes);

		nsdp->vpage = kmem_alloc(nnbytes, KM_SLEEP);
		bcopy(&ovpage[npages + dpages], nsdp->vpage, nnbytes);

		/* free up old vpage */
		kmem_free(ovpage, vpgtob(opages));
	} else
		nsdp->vpage = NULL;

	/*
	 * unmap dhps.
	 */
	if (dhp == NULL)
		nsdp->devmap_data = NULL;
	else {
		devmap_handle_t *dhpp;

		while (dhp != NULL) {
			callbackops = &dhp->dh_callbackops;
			TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_UNMAP_CK3,
			    "segdev_unmap: dhp=%p addr=%p uvaddr=%p len=%lx",
			    (void *)dhp, (void *)addr,
			    (void *)dhp->dh_uvaddr, dhp->dh_len);
			DEBUGF(3, (CE_CONT, "unmap: dhp %p "
			    "addr %p uvaddr %p len %lx\n",
			    (void *)dhp, (void *)addr,
			    (void *)dhp->dh_uvaddr, dhp->dh_len));

			if (addr == (dhp->dh_uvaddr + dhp->dh_len)) {
				dhpp = dhp->dh_next;
				dhp->dh_next = NULL;
				dhp = dhpp;
			} else if (addr > (dhp->dh_uvaddr + dhp->dh_len)) {
				dhp = dhp->dh_next;
			} else if (addr > dhp->dh_uvaddr &&
					(addr + len) <
					(dhp->dh_uvaddr + dhp->dh_len)) {
				/*
				 * <addr, add+len> is enclosed by dhp.
				 * create a newdhp that begins at addr+len and
				 * ends at dhp->dh_uvaddr+dhp->dh_len.
				 */
				newdhp = kmem_alloc(sizeof (devmap_handle_t),
				    KM_SLEEP);
				bcopy(dhp, newdhp, sizeof (devmap_handle_t));
				newdhp->dh_seg = nseg;
				newdhp->dh_next = dhp->dh_next;
				if (dhp->dh_softlock != NULL)
					newdhp->dh_softlock =
						devmap_softlock_init(
						newdhp->dh_dev,
						(u_long)
						callbackops->devmap_access);
				if (dhp->dh_ctx != NULL)
					newdhp->dh_ctx =
						devmap_ctxinit(newdhp->dh_dev,
					(u_long)callbackops->devmap_access);
				if (newdhp->dh_flags & DEVMAP_LOCK_INITED) {
					mutex_init(&newdhp->dh_lock,
					    NULL, MUTEX_DEFAULT, NULL);
					cv_init(&newdhp->dh_cv,
					    NULL, CV_DEFAULT, NULL);
				}
				if (callbackops->devmap_unmap != NULL)
					(*callbackops->devmap_unmap)(dhp,
						dhp->dh_pvtp, off, len,
						dhp, &dhp->dh_pvtp,
						newdhp,  &newdhp->dh_pvtp);
				mlen = len + (addr - dhp->dh_uvaddr);
				devmap_handle_change_len(newdhp, mlen);
				nsdp->devmap_data = newdhp;
				dhp->dh_len = addr - dhp->dh_uvaddr;
				dhpp = dhp->dh_next;
				dhp->dh_next = NULL;
				dhp = dhpp;
			} else if (addr > dhp->dh_uvaddr &&
					(addr + len) >= (dhp->dh_uvaddr +
					dhp->dh_len)) {
				mlen = dhp->dh_len + dhp->dh_uvaddr - addr;
				/*
				 * <add, add+len> spans over dhps.
				 */
				if (callbackops->devmap_unmap != NULL)
					(*callbackops->devmap_unmap)(dhp,
						dhp->dh_pvtp, off, mlen,
						(devmap_cookie_t *)dhp,
						&dhp->dh_pvtp, NULL, NULL);
				dhp->dh_len = addr - dhp->dh_uvaddr;
				dhpp = dhp->dh_next;
				dhp->dh_next = NULL;
				dhp = dhpp;
				nsdp->devmap_data = dhp;
			} else if ((addr + len) >= (dhp->dh_uvaddr +
					dhp->dh_len)) {
				/*
				 * dhp is enclosed by <addr, addr+len>.
				 */
				dhp->dh_seg = nseg;
				nsdp->devmap_data = dhp;
				dhp = devmap_handle_unmap(dhp);
				nsdp->devmap_data = dhp;
			} else if ((addr + len) > dhp->dh_uvaddr &&
					(addr + len) < dhp->dh_uvaddr +
					dhp->dh_len) {
				mlen = addr + len - dhp->dh_uvaddr;
				if (callbackops->devmap_unmap != NULL)
					(*callbackops->devmap_unmap)(dhp,
						dhp->dh_pvtp,
						dhp->dh_uoff, mlen, NULL,
						NULL, dhp, &dhp->dh_pvtp);
				devmap_handle_change_len(dhp, mlen);
				nsdp->devmap_data = dhp;
				dhp->dh_seg = nseg;
				dhp = dhp->dh_next;
			} else {
				dhp->dh_seg = nseg;
				dhp = dhp->dh_next;
			}
		}
	}

	return (0);
}

static void
devmap_handle_change_len(devmap_handle_t *dhp, size_t len)
{
	/*
	 * adjust devmap handle fields
	 */
	ASSERT(len < dhp->dh_len);
	dhp->dh_len -= len;
	dhp->dh_uoff += (offset_t)len;
	dhp->dh_roff += (offset_t)len;
	dhp->dh_uvaddr += len;
	if (dhp->dh_flags & DEVMAP_FLAG_DEVMEM)
		dhp->dh_pfn += btop(len);
	else {
		struct ddi_umem_cookie *cp = dhp->dh_cookie;

		ASSERT(dhp->dh_roff < cp->size);
		ASSERT(dhp->dh_kvaddr >= cp->kvaddr &&
			dhp->dh_kvaddr < (cp->kvaddr + cp->size));
		ASSERT((dhp->dh_kvaddr + len) <= (cp->kvaddr + cp->size));

		dhp->dh_kvaddr += len;
	}
}

/*
 * Free devmap handle, dhp.
 * Return the next devmap handle on the linked list.
 */
static devmap_handle_t *
devmap_handle_unmap(devmap_handle_t *dhp)
{
	struct devmap_callback_ctl *callbackops = &dhp->dh_callbackops;
	struct segdev_data *sdp = (struct segdev_data *)dhp->dh_seg->s_data;
	devmap_handle_t *dhp_head = (devmap_handle_t *)sdp->devmap_data;
	devmap_handle_t *dhp_prev;
	devmap_handle_t *dhpp;

	ASSERT(dhp != NULL);

	/*
	 * before we free up dhp, call the driver's devmap_unmap entry point
	 * to free resources allocated for this dhp.
	 */
	if (callbackops->devmap_unmap != NULL) {
		(*callbackops->devmap_unmap)(dhp, dhp->dh_pvtp, dhp->dh_uoff,
			dhp->dh_len, NULL, NULL, NULL, NULL);
	}

	if (dhp_head == dhp) {
		dhpp = dhp->dh_next;
		sdp->devmap_data = dhpp;
	} else {
		/*
		 * find dhp_prev such that dhp_prev->dh_next == dhp
		 */
		dhp_prev = dhp_head;
		dhpp = dhp_head->dh_next;
		while (dhpp != dhp && dhpp != NULL) {
			dhp_prev = dhpp;
			dhpp = dhpp->dh_next;
		}
		dhp_prev->dh_next = dhpp = dhp->dh_next;
	}

	if (dhp->dh_softlock != NULL)
		devmap_softlock_rele(dhp);

	if (dhp->dh_ctx != NULL)
		devmap_ctx_rele(dhp);

	if (dhp->dh_flags & DEVMAP_LOCK_INITED) {
		mutex_destroy(&dhp->dh_lock);
		cv_destroy(&dhp->dh_cv);
	}
	kmem_free(dhp, sizeof (devmap_handle_t));

	return (dhpp);
}

/*
 * Free devmap handles from the beginning of the mapping to len.
 * dhp must be the first dhp of the mapping.
 */
static void
devmap_handle_unmap_head(devmap_handle_t *dhp, size_t len)
{
	register struct segdev_data *sdp =
				(struct segdev_data *)dhp->dh_seg->s_data;
	register devmap_handle_t *dhp_head =
				(devmap_handle_t *)sdp->devmap_data;
	struct devmap_callback_ctl *callbackops;

	ASSERT(dhp == dhp_head);

	while ((long)len > 0) {
		/*
		 * free the devmap handles covered by len.
		 */
		if (len >= dhp->dh_len) {
			len -= dhp->dh_len;
			dhp = devmap_handle_unmap(dhp);
		} else {
			callbackops = &dhp->dh_callbackops;

			/*
			 * Call the unmap callback so the drivers can make
			 * adjustment on its private data.
			 */
			if (callbackops->devmap_unmap != NULL)
				(*callbackops->devmap_unmap)(dhp, dhp->dh_pvtp,
					dhp->dh_uoff, len, NULL, NULL, dhp,
					&dhp->dh_pvtp);
			devmap_handle_change_len(dhp, len);
			break;
		}
	}
}

/*
 * Free devmap handles to truncate  the mapping after addr
 */
static void
devmap_handle_unmap_tail(devmap_handle_t *dhp, caddr_t addr)
{
	register struct seg *seg = dhp->dh_seg;
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	register devmap_handle_t *dhph = (devmap_handle_t *)sdp->devmap_data;
	struct devmap_callback_ctl *callbackops;
	register devmap_handle_t *dhpp;
	size_t maplen;
	u_long off;
	size_t len;

	maplen = (size_t)(addr - dhp->dh_uvaddr);
	dhph = devmap_find_handle(dhph, addr);

	while (dhph != NULL) {
		if (maplen == 0) {
			dhph =  devmap_handle_unmap(dhph);
		} else {
			callbackops = &dhph->dh_callbackops;
			len = dhph->dh_len - maplen;
			off = (u_long)sdp->offset + (addr - seg->s_base);
			/*
			 * Call the unmap callback so the driver
			 * can make adjustments on its private data.
			 */
			if (callbackops->devmap_unmap != NULL)
				(*callbackops->devmap_unmap)(dhph,
					dhph->dh_pvtp, off, len,
					(devmap_cookie_t *)dhph,
					&dhph->dh_pvtp, NULL, NULL);
			dhph->dh_len = maplen;
			maplen = 0;
			dhpp = dhph->dh_next;
			dhph->dh_next = NULL;
			dhph = dhpp;
		}
	} /* end while */
}

/*
 * Free a segment.
 */
static void
segdev_free(struct seg *seg)
{
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	devmap_handle_t *dhp = (devmap_handle_t *)sdp->devmap_data;

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_FREE,
	    "segdev_free: dhp=%p seg=%p", (void *)dhp, (void *)seg);
	DEBUGF(3, (CE_CONT, "segdev_free: dhp %p seg %p\n",
	    (void *)dhp, (void *)seg));

	/*
	 * Since the address space is "write" locked, we
	 * don't need the segment lock to protect "segdev" data.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	while (dhp != NULL)
		dhp = devmap_handle_unmap(dhp);

	VN_RELE(sdp->vp);
	if (sdp->vpage != NULL)
		kmem_free(sdp->vpage, vpgtob(seg_pages(seg)));

	mutex_destroy(&sdp->lock);
	kmem_free(sdp, sizeof (*sdp));
}

static void
free_devmap_handle(devmap_handle_t *dhp)
{
	register devmap_handle_t *dhpp;

	/*
	 * free up devmap handle
	 */
	while (dhp != NULL) {
		dhpp = dhp->dh_next;
		if (dhp->dh_flags & DEVMAP_LOCK_INITED) {
			mutex_destroy(&dhp->dh_lock);
			cv_destroy(&dhp->dh_cv);
		}

		if (dhp->dh_softlock != NULL)
			devmap_softlock_rele(dhp);

		if (dhp->dh_ctx != NULL)
			devmap_ctx_rele(dhp);

		kmem_free(dhp, sizeof (devmap_handle_t));
		dhp = dhpp;
	}
}

/*
 * Do a F_SOFTUNLOCK call over the range requested.
 * The range must have already been F_SOFTLOCK'ed.
 * The segment lock should be held.
 */
/*ARGSUSED*/
static void
segdev_softunlock(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* seg_dev of interest */
	caddr_t addr,			/* base address of range */
	size_t len,			/* number of bytes */
	enum seg_rw rw,			/* type of access at fault */
	struct cred *cr)		/* credentials */
{
	struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	devmap_handle_t *dhp_head = (devmap_handle_t *)sdp->devmap_data;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_SOFTUNLOCK,
	    "segdev_softunlock:dhp_head=%p lockcnt=%lx addr=%p len=%lx",
	    (void *)dhp_head, sdp->softlockcnt, (void *)addr, len);
	DEBUGF(3, (CE_CONT, "segdev_softunlock: dhp %p lockcnt %lx "
	    "addr %p len %lx\n",
	    (void *)dhp_head, sdp->softlockcnt, (void *)addr, len));

#ifdef lint
	cr = cr;
#endif

	hat_unlock(hat, addr, len);

	mutex_enter(&freemem_lock);
	ASSERT(sdp->softlockcnt >= btopr(len));
	sdp->softlockcnt -= btopr(len);
	mutex_exit(&freemem_lock);
	if (sdp->softlockcnt == 0) {
		/*
		 * All SOFTLOCKS are gone. Wakeup any waiting
		 * unmappers so they can try again to unmap.
		 * Check for waiters first without the mutex
		 * held so we don't always grab the mutex on
		 * softunlocks.
		 */
		if (AS_ISUNMAPWAIT(seg->s_as)) {
			mutex_enter(&seg->s_as->a_contents);
			if (AS_ISUNMAPWAIT(seg->s_as)) {
				AS_CLRUNMAPWAIT(seg->s_as);
				cv_broadcast(&seg->s_as->a_cv);
			}
			mutex_exit(&seg->s_as->a_contents);
		}
	}

	if (dhp_head != NULL) {
		devmap_handle_t *dhp;
		struct ddi_umem_cookie *cookie;
		struct devmap_callback_ctl *callbackops;
		struct devmap_softlock *slock;
		size_t mlen;
		size_t tlen = len;
		u_long off;

		/*
		 * Save the first dhp on the SOFTUNLOCK range to
		 * release the softlock later.
		 */
		dhp = devmap_find_handle(dhp_head, addr);

		off = (u_long)(addr - dhp->dh_uvaddr);
		while ((long)tlen > 0 && dhp) {
			ASSERT(dhp != NULL);

			slock = dhp->dh_softlock;
			callbackops = &dhp->dh_callbackops;
			/*
			 * unlock segkp memory.
			 */
			if (dhp->dh_flags & DEVMAP_FLAG_KPMEM) {
				int err;
				cookie = (struct ddi_umem_cookie *)
					dhp->dh_cookie;

				err = as_fault(kas.a_hat, &kas, cookie->kvaddr,
					cookie->size, F_SOFTUNLOCK, rw);
				if (err)
					panic("segdev_softunlock");
				RELE_COOKIE_LOCK(cookie);
			}
			mlen = MIN(tlen, (dhp->dh_len - off));

			/*
			 * Do the RELE_SOFTLOCK
			 */
			if (callbackops->devmap_access != NULL) {
				RELE_SOFTLOCK_LOCK(slock, btopr(mlen));
			}

			tlen -= mlen;
			dhp = dhp->dh_next;
			off = 0;
		}

	}
}

/*
 * Handle a single page.
 * Done in a separate routine so we can handle errors more easily.
 * This routine is called only from segdev_fault()
 * when looping over the range of addresses requested.  The
 * segment lock should be held.
 *
 * The basic algorithm here is:
 *		Find pfn from the driver's mmap function
 *		Load up the translation to the page
 *		return
 */
/*ARGSUSED*/
static int
segdev_faultpage(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* seg_dev of interest */
	caddr_t addr,			/* address in as */
	struct vpage *vpage,		/* pointer to vpage for seg, addr */
	enum fault_type type,		/* type of fault */
	enum seg_rw rw,			/* type of access at fault */
	struct cred *cr,		/* credentials */
	size_t len)			/* len of the page */
{
	struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	devmap_handle_t *dhp = NULL;
	devmap_handle_t *dhp_head = (devmap_handle_t *)sdp->devmap_data;
	u_int prot;
	pfn_t pfnum = 0;
	u_offset_t offset;
	u_int hat_flags;
	dev_info_t *dip;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_FAULTPAGE,
	    "segdev_faultpage: dhp_head=%p seg=%p addr=%p len=%lx",
	    (void *)dhp_head, (void *)seg, (void *)addr, len);
	DEBUGF(8, (CE_CONT, "segdev_faultpage: dhp %p seg %p "
	    "addr %p len %lx\n",
	    (void *)dhp_head, (void *)seg, (void *)addr, len));
#ifdef lint
	cr = cr;
#endif

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

	if (type == F_SOFTLOCK) {
		mutex_enter(&freemem_lock);
		sdp->softlockcnt++;
		mutex_exit(&freemem_lock);
	}

	hat_flags = ((type == F_SOFTLOCK) ? HAT_LOAD_LOCK : HAT_LOAD);
	offset = sdp->offset + (u_offset_t)(addr - seg->s_base);
	/*
	 * In the devmap framework, sdp->mapfunc is set to NULL.  we can get
	 * pfnum from dhp->dh_pfn (at beginning of segment) and offset from
	 * seg->s_base.
	 */
	if (dhp_head == NULL) {
		pfnum = (pfn_t)cdev_mmap(sdp->mapfunc, sdp->vp->v_rdev,
			(off_t)offset, prot);
		if (pfnum == (pfn_t)-1L)
			return (FC_MAKE_ERR(EFAULT));
	} else {
		u_long off;
		/*
		 * find the dhp that contains addr.
		 */
		if ((dhp = devmap_find_handle(dhp_head, addr)) == NULL)
			return (FC_NOMAP);

		off = addr - dhp->dh_uvaddr;
		if (dhp->dh_flags & DEVMAP_FLAG_DEVMEM)
			pfnum = dhp->dh_pfn + btop(off);
		else {
			struct ddi_umem_cookie *cp = dhp->dh_cookie;

			ASSERT((dhp->dh_flags & DEVMAP_FLAG_KMEM) ||
				(dhp->dh_flags & DEVMAP_FLAG_KPMEM));
			ASSERT(dhp->dh_roff < cp->size);
			ASSERT(dhp->dh_kvaddr >= cp->kvaddr &&
				dhp->dh_kvaddr < (cp->kvaddr + cp->size));
			ASSERT((dhp->dh_kvaddr + off) <=
				(cp->kvaddr + cp->size));
			ASSERT((dhp->dh_kvaddr + off + PAGESIZE) <=
				(cp->kvaddr + cp->size));

			switch (cp->type) {
			case UMEM_LOCKED:
				if (cp->pparray != NULL) {
				    ASSERT((dhp->dh_roff & PAGEOFFSET) == 0);
				    pfnum = page_pptonum(
					cp->pparray[btop(off + dhp->dh_roff)]);
				} else {
				    pfnum = hat_getpfnum(
					((proc_t *)cp->procp)->p_as->a_hat,
					cp->kvaddr + off);
				}
				break;
			case KMEM_PAGEABLE:
			case KMEM_NON_PAGEABLE:
				pfnum = hat_getkpfnum(dhp->dh_kvaddr + off);
				break;
			default:
				pfnum = (pfn_t)-1L;
				break;
			}
		}
	}

	if (pfnum == (pfn_t)-1L)
		return (FC_MAKE_ERR(EFAULT));

	TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_FAULTPAGE_CK1,
	    "segdev_faultpage: pfnum=%lx memory=%x flags=%x",
	    pfnum, pf_is_memory(pfnum), hat_flags);
	DEBUGF(9, (CE_CONT, "segdev_faultpage: pfnum %lx "
	    "memory %x flags %x\n",
	    pfnum, pf_is_memory(pfnum), hat_flags));

	if (pf_is_memory(pfnum)) {
		if (dhp_head == NULL) {
			hat_devload(hat, addr, PAGESIZE, pfnum,
				prot | sdp->hat_attr,
				hat_flags | sdp->hat_flags);
		} else {
			hat_devload(hat, addr, PAGESIZE, pfnum,
				prot | dhp->dh_hat_attr,
				hat_flags | sdp->hat_flags);
		}

		return (0);
	}

	dip = VTOS(VTOCVP(sdp->vp))->s_dip;
	ASSERT(dip);

	if (dhp_head == NULL) {
		if (ddi_map_fault(dip, hat, seg, addr, NULL, pfnum, prot,
			(u_int)(type == F_SOFTLOCK)) != DDI_SUCCESS)
			return (FC_MAKE_ERR(EFAULT));
	} else {
		/*
		 * It's not _really_ required here to pass sdp->hat_flags
		 * to hat_devload eventhough we do it.
		 * This is because hat figures it out DEVMEM mappings
		 * are non-consistent, anyway.
		 */
		hat_devload(hat, addr, PAGESIZE, pfnum,
			prot | dhp->dh_hat_attr,
			hat_flags | sdp->hat_flags);
	}

	return (0);
}

/*
 * This routine is called via a machine specific fault handling routine.
 * It is also called by software routines wishing to lock or unlock
 * a range of addresses.
 *
 */
static faultcode_t
segdev_fault(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* the seg_dev of interest */
	caddr_t addr,			/* the address of the fault */
	size_t len,			/* the length of the range */
	enum fault_type type,		/* type of fault */
	enum seg_rw rw)			/* type of access at fault */
{
	struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	devmap_handle_t *dhp_head = (devmap_handle_t *)sdp->devmap_data;
	struct cred *cr = CRED();
	int err;

	TRACE_5(TR_FAC_DEVMAP, TR_DEVMAP_FAULT,
	    "segdev_fault: dhp_head=%p seg=%p addr=%p len=%lx type=%x",
	    (void *)dhp_head, (void *)seg, (void *)addr, len, type);
	DEBUGF(7, (CE_CONT, "segdev_fault: dhp_head %p seg %p "
	    "addr %p len %lx type %x\n",
	    (void *)dhp_head, (void *)seg, (void *)addr, len, type));

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (dhp_head != NULL) {
		devmap_handle_t *dhp = NULL;
		devmap_handle_t *dhpp = NULL;
		struct ddi_umem_cookie *cookie = NULL;
		struct devmap_softlock *slock = NULL;
		caddr_t laddr = addr;
		size_t llen = len;
		size_t slen = len;
		u_long slpage = 0;
		caddr_t maddr = addr;
		size_t mlen = len;
		u_long off;

		/*
		 * If type is F_SOFTLOCK or F_SOFTUNLOCK, we softlock
		 * or softunlock extended length that covers all the
		 * large pages spanning the range of the fault.
		 *
		 * The same length must be used when we softlock and
		 * softunlock. We don't support softunlocking
		 * lengths less than the original length when
		 * there is large page support.
		 */

		if ((dhp = devmap_find_handle(dhp_head, maddr)) == NULL)
			return (FC_NOMAP);

		/*
		 * To synchronize the SOFTLOCK threads if the drivers
		 * are going to do context switching.
		 * HOLD_SOFTLOCK_LOCK has to be called outside of while loop
		 * to preven a deadlock if len spans ove dhps.
		 * XXXX - what if the first dhp does not support context
		 *			switching but the subsequent dhps do.
		 */
		if (type == F_SOFTLOCK || type == F_INVAL) {
			/*
			 * keep a count of pages that RELE_SOFTLOCK_LOCK()
			 * should decrement in segdev_softunlock().
			 * Number of page count should exclude pages in
			 * the dhps that do not support devmap_access().
			 *
			 * For F_INVAL, we need to check if SOFTLOCK_LOCK
			 * is held by other processes if devmap_access()
			 * is not null in the range (addr, addr+len).
			 */
			dhpp = dhp;
			off = (u_long)(addr - dhpp->dh_uvaddr);
			while ((ssize_t)slen > 0) {
				mlen = MIN(slen, (dhpp->dh_len - off));
				/*
				 * softlock count should be incremented
				 * only for the pages in dhp's that support
				 * devmap_access().
				 */
				if (dhpp->dh_callbackops.devmap_access
					!= NULL) {
					/*
					 * softlock the extended length if
					 * large pagesize is used for loading
					 * address translations.
					 */
					if ((sdp->pageprot == 0) &&
					(dhpp->dh_flags & DEVMAP_FLAG_LARGE)) {
						/*
						 * XXX - We assume, for now,
						 * only devmem uses large pages.
						 * This code must be changed
						 * in future when we support
						 * large pages for
						 * kernel exported memory.
						 */
						ASSERT(dhpp->dh_flags &
							DEVMAP_FLAG_DEVMEM);

						devmap_get_large_pgsize(dhpp,
							mlen, maddr,
							&llen, &laddr);

						ASSERT(llen >= mlen &&
							laddr <= maddr);
						ASSERT(maddr == addr ||
							laddr == maddr);

						slpage += btopr(llen);
					} else {
						slpage += btopr(mlen);
					}

					slock = dhpp->dh_softlock;
				}
				maddr += mlen;
				dhpp = dhpp->dh_next;
				off = 0;
				slen -= mlen;
			}

			if (slpage != 0)
				HOLD_SOFTLOCK_LOCK(slock, slpage);
		}

		/*
		 * reset 'maddr' to the start addr of the range of fault.
		 */
		maddr = addr;

		/*
		 * calculate the offset corresponds to 'addr' in the first dhp.
		 */
		off = (u_long)(addr - dhp->dh_uvaddr);

		/*
		 * For SOFTLOCK, the fault length may span over multiple dhps.
		 * Loop until the total length is satisifed.
		 */
		while ((ssize_t)len > 0) {
			ASSERT(dhp != NULL);
			/*
			 * mlen is the smaller of 'len' and the length
			 * from addr to the end of mapping defined by dhp.
			 */
			mlen = MIN(len, (dhp->dh_len - off));

			/*
			 * XXX -  We assume, for now, only devmem uses
			 * large pages; we should not softlock extended
			 * length in case of kernel exported memory. So,
			 * this fragment might have to be changed,
			 * in future, when we decide to support large pages
			 * for kernel exported memory.
			 *
			 * Get the extended length and address if large pagesize
			 * is used for loading address translations.
			 * Extended length and address will be passed to
			 * devmap_access instead of the original len and addr.
			 */
			if ((sdp->pageprot == 0) &&
			    (dhp->dh_flags & DEVMAP_FLAG_LARGE)) {
				ASSERT(dhp->dh_flags & DEVMAP_FLAG_DEVMEM);

				devmap_get_large_pgsize(dhp, mlen, maddr,
					&llen, &laddr);

				ASSERT(llen >= mlen && laddr <= maddr);

				ASSERT((maddr == addr) || (laddr == maddr));
				/*
				 * We have to recalculate mlen because the
				 * new mapping address and length may extend
				 * beyond the original mapping.  We can not use
				 * llen either because the mapping address may
				 * fall below the original address.
				 * mlen = (laddr + llen) - maddr;
				 */

			} else {
				llen = mlen;
				laddr = maddr;
			}

			if (dhp->dh_flags & DEVMAP_FLAG_KPMEM) {
				pfn_t kpfn;
				/*
				 * For kernel pageable memory, do a F_SOFTLOCK
				 * to segkp to load and lock the kernel memory.
				 * We hold the F_SOFTLOCK until the ompletion of
				 * this fault.
				 */
				cookie = (struct ddi_umem_cookie *)
					dhp->dh_cookie;

				ASSERT(cookie != NULL);
				/*
				 * A fault to segkp has to be single-threaded.
				 */
				HOLD_COOKIE_LOCK(cookie);

				/*
				 * do as_fault() regardless the memory is
				 * avail or not. Fault in the pages with lock.
				 * We want to hold the lock until all pages
				 *  have been loaded.
				 */
				if ((err = as_fault(kas.a_hat, &kas,
					cookie->kvaddr,
					cookie->size, F_SOFTLOCK, rw)) != 0) {

					RELE_COOKIE_LOCK(cookie);

					if (type == F_SOFTLOCK) {
						/*
						 * If not first dhp,
						 * use segdev_softunlock
						 */
						if (maddr > addr)
							segdev_softunlock(hat,
							    seg, addr,
							    (size_t)(maddr -
							    addr),
							    S_OTHER, cr);
						else
							RELE_SOFTLOCK_LOCK(
							    slock, slpage);
					} else if (type == F_INVAL)
						RELE_SOFTLOCK_LOCK(slock,
						    slpage);

					return (err);
				}

				ASSERT((kpfn = hat_getkpfnum(dhp->dh_kvaddr)) !=
						-1);
				TRACE_1(TR_FAC_DEVMAP, TR_DEVMAP_FAULT_CK1,
				    "segdev_fault: kpfn=%lx", kpfn);
				DEBUGF(2, (CE_CONT, "segdev_fault: kpfn %lx\n",
				    kpfn));
			}

			/*
			 * call the driver's devmap_access entry point
			 * before we load the translations.
			 */
			if (dhp->dh_callbackops.devmap_access != NULL) {
				offset_t aoff;

				/*
				 * This fault needs to be single threaded
				 * on a per dhp bases if a remap on dhp
				 * is allowed.  This is because devmap_access
				 * will call either devmap_devmem_remap or
				 * devmap_umem_remap to change the mapping
				 * on dhp during remap.
				 * But we don't want to lock dhp again if it
				 * is already locked, e,g. if dhp is for kernel
				 * pageable memory.
				 */
				if (dhp->dh_flags & DEVMAP_ALLOW_REMAP)
					HOLD_DHP_LOCK(dhp);

				aoff = sdp->offset + (offset_t)(laddr -
				    seg->s_base);

				err = (*dhp->dh_callbackops.devmap_access)(dhp,
				    (void *)dhp->dh_pvtp, aoff, llen, type, rw);

				if (dhp->dh_flags & DEVMAP_ALLOW_REMAP)
					RELE_DHP_LOCK(dhp);
			} else {
				err = segdev_faultpages(hat, seg, laddr, llen,
				    type, rw, dhp);
			}

			if ((dhp->dh_flags & DEVMAP_FLAG_KPMEM) &&
			    (err || (type != F_SOFTLOCK))) {
				/*
				 * unlock the pages if the memory being
				 * referenced is pageable kernel memory
				 * and type is not F_SOFTLOCK
				 * Also unlock the pages if there was an
				 * error.
				 * For F_SOFTLOCK this is done in
				 * segdev_softunlock
				 */
				ASSERT(cookie != NULL);
				if (as_fault(kas.a_hat, &kas, cookie->kvaddr,
				    cookie->size, F_SOFTUNLOCK, rw))
					panic("segdev_fault: "
					    "segkp softunlock");
				RELE_COOKIE_LOCK(cookie);
			}

			if (err) {
				if (type == F_SOFTLOCK) {
					/*
					 * If not first dhp, use
					 * segdev_softunlock
					 */
					if (maddr > addr) {
						segdev_softunlock(hat, seg,
						    addr,
						    (size_t)(maddr - addr),
						    S_OTHER, cr);
					} else {
						RELE_SOFTLOCK_LOCK(slock,
						    slpage);
					}
				} else if (type == F_INVAL) {
					RELE_SOFTLOCK_LOCK(slock, slpage);
				}
				return (FC_MAKE_ERR(err));
			}

			if (type == F_INVAL && slock != NULL && slpage)
				RELE_SOFTLOCK_LOCK(slock, slpage);

			maddr += mlen;
			off = 0;
			len -= mlen;
			dhp = dhp->dh_next;

			ASSERT(!dhp || len == 0 || maddr == dhp->dh_uvaddr);
		}
	} else {
		err = segdev_faultpages(hat, seg, addr, len, type, rw, NULL);
	}

	return (err);
}

/*
 * segdev_faultpages
 *
 * Used to fault in seg_dev segment pages instead of segdev_fault. Called
 * from segdev_fault or segdev_load.  This routine returns a
 * faultcode_t.  The faultcode_t value as a return value for segdev_fault.
 */
static int
segdev_faultpages(
	struct hat *hat,		/* the hat */
	struct seg *seg,		/* the seg_dev of interest */
	caddr_t addr,			/* the address of the fault */
	size_t len,			/* the length of the range */
	enum fault_type type,		/* type of fault */
	enum seg_rw rw,			/* type of access at fault */
	devmap_handle_t *dhp)		/* devmap handle */
{
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	register caddr_t a;
	struct vpage *vpage;
	struct cred *cr = CRED();
	int err;
	size_t page;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_FAULTPAGES,
	    "segdev_faultpages: dhp=%p seg=%p addr=%p len=%lx",
	    (void *)dhp, (void *)seg, (void *)addr, len);
	DEBUGF(5, (CE_CONT, "segdev_faultpages: "
	    "dhp %p seg %p addr %p len %lx\n",
	    (void *)dhp, (void *)seg, (void *)addr, len));

	if (type == F_PROT) {
		/*
		 * Since the seg_dev driver does not implement copy-on-write,
		 * this means that a valid translation is already loaded,
		 * but we got an fault trying to access the device.
		 * Return an error here to prevent going in an endless
		 * loop reloading the same translation...
		 */
		return (FC_PROT);
	}

	/*
	 * First handle the easy stuff
	 * Since devmap_access() may change the fault length
	 * segdev_softunlock() has to be called here rather than
	 * in segdev_fault().
	 * XXX - how do we handle the inconsistency between pyhsio
	 *		softlock length and actually softlock length as
	 *		devmap_access() may change the length.
	 */
	if (type == F_SOFTUNLOCK) {
		segdev_softunlock(hat, seg, addr, len, rw, cr);
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

	/*
	 * we do large page size translation if
	 *   - devmap framework (dhp is not NULL),
	 *   - pageprot == 0, no page protection set and
	 *   - use large pagesize flag is set.
	 */
	if ((sdp->pageprot == 0) && (dhp != NULL) &&
		(dhp->dh_flags & DEVMAP_FLAG_LARGE)) {
		pfn_t pfnum;
		u_int hat_flags;

		ASSERT(dhp->dh_flags & DEVMAP_FLAG_DEVMEM);

		if (type == F_SOFTLOCK) {
			mutex_enter(&freemem_lock);
			sdp->softlockcnt += btopr(len);
			mutex_exit(&freemem_lock);
		}

		hat_flags = ((type == F_SOFTLOCK) ? HAT_LOAD_LOCK : HAT_LOAD);
		pfnum = dhp->dh_pfn + btop(addr - dhp->dh_uvaddr);

		/*
		 * We shouldn't, really, care about sdp->hat_flags here,
		 * since /dev/mem and /dev/kmem mappings don't support
		 * large pages.
		 */
		hat_devload(hat, addr, len, pfnum,
			sdp->prot | dhp->dh_hat_attr,
			hat_flags | sdp->hat_flags);
	} else {
		page = seg_page(seg, addr);
		if (sdp->vpage == NULL)
			vpage = NULL;
		else
			vpage = &sdp->vpage[page];

		/* loop over the address range handling each fault */
		for (a = addr; a < addr + len; a += PAGESIZE) {
			err = segdev_faultpage(hat, seg, a, vpage, type,
					rw, cr, 0);
			if (err) {
				if (type == F_SOFTLOCK && a > addr)
					/*
					 * XXXX There is bug in a corner case.
					 * If there are multiple dhps and
					 * devmap_access, then the
					 * SOFTLOCK is released multiple
					 * times - one in this softunlock
					 * and another in the caller.
					 */
					segdev_softunlock(hat, seg, addr,
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
segdev_faulta(struct seg *seg, caddr_t addr)
{
	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_FAULTA,
	    "segdev_faulta: seg=%p addr=%p", (void *)seg, (void *)addr);
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

static int
segdev_setprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	register devmap_handle_t *dhp;
	register struct vpage *vp, *evp;
	devmap_handle_t *dhp_head = (devmap_handle_t *)sdp->devmap_data;
	u_long off;
	size_t mlen;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_SETPROT,
	    "segdev_setprot:start seg=%p addr=%p len=%lx prot=%x",
	    (void *)seg, (void *)addr, len, prot);
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (sdp->softlockcnt > 0 && dhp_head != NULL) {
		/*
		 * Fail the setprot if pages are SOFTLOCKed through this
		 * mapping.
		 * Softlockcnt is protected from change by the as read lock.
		 */
		TRACE_1(TR_FAC_DEVMAP, TR_DEVMAP_SETPROT_CK1,
		    "segdev_setprot:error softlockcnt=%lx",
		    sdp->softlockcnt);
		DEBUGF(1, (CE_CONT, "segdev_setprot: softlockcnt %ld\n",
		    sdp->softlockcnt));
		return (EAGAIN);
	}

	if (dhp_head != NULL) {
		if ((dhp = devmap_find_handle(dhp_head, addr)) == NULL)
			return (EINVAL);

		/*
		 * check if violate maxprot.
		 */
		off = (u_long)(addr - dhp->dh_uvaddr);
		mlen  = len;
		while (dhp) {
			if ((dhp->dh_maxprot & prot) != prot)
				return (EACCES);	/* violated maxprot */

			if (mlen > (dhp->dh_len - off)) {
				mlen -= dhp->dh_len - off;
				dhp = dhp->dh_next;
				off = 0;
			} else
				break;
		}
	} else {
		if ((sdp->maxprot & prot) != prot)
			return (EACCES);
	}

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

	if (dhp_head != NULL) {
		u_int large_page = 0;
		/*
		 * If large page size was used in hat_devload(),
		 * the same page size must be used in hat_unload().
		 */
		dhp = devmap_find_handle(dhp_head, addr);
		while (dhp != NULL) {
			if (dhp->dh_flags & DEVMAP_FLAG_LARGE) {
				large_page++;
				break;
			}
			dhp = dhp->dh_next;
		}
		if (large_page) {
			size_t slen = len;
			size_t mlen;
			size_t soff;

			dhp = devmap_find_handle(dhp_head, addr);
			soff = (u_long)(addr - dhp->dh_uvaddr);
			while ((long)slen > 0) {
				mlen = MIN(slen, (dhp->dh_len - soff));
				hat_unload(seg->s_as->a_hat, dhp->dh_uvaddr,
					dhp->dh_len, HAT_UNLOAD);
				dhp = dhp->dh_next;
				slen -= mlen;
				soff = 0;
			}

			return (0);
		}
	}

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

	return (0);
}

static int
segdev_checkprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	struct vpage *vp, *evp;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_CHECKPROT,
	    "segdev_checkprot:start seg=%p addr=%p len=%lx prot=%x",
	    (void *)seg, (void *)addr, len, prot);
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
segdev_getprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	struct segdev_data *sdp = (struct segdev_data *)seg->s_data;
	size_t pgno;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_GETPROT,
	    "segdev_getprot:start seg=%p addr=%p len=%lx protv=%p",
	    (void *)seg, (void *)addr, len, (void *)protv);
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

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
segdev_getoffset(register struct seg *seg, caddr_t addr)
{
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_GETOFFSET,
	    "segdev_getoffset:start seg=%p addr=%p", (void *)seg, (void *)addr);

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return ((u_offset_t)sdp->offset);
}

/*ARGSUSED*/
static int
segdev_gettype(register struct seg *seg, caddr_t addr)
{
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_GETTYPE,
	    "segdev_gettype:start seg=%p addr=%p", (void *)seg, (void *)addr);

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (sdp->type);
}


/*ARGSUSED*/
static int
segdev_getvp(register struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	register struct segdev_data *sdp = (struct segdev_data *)seg->s_data;

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_GETVP,
	    "segdev_getvp:start seg=%p addr=%p", (void *)seg, (void *)addr);

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Note that this vp is the common_vp of the device, where the
	 * pages are hung ..
	 */
	*vpp = VTOCVP(sdp->vp);

	return (0);
}

static void
segdev_badop(void)
{
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_SEGDEV_BADOP,
		"segdev_badop:start");
	cmn_err(CE_PANIC, "segdev_badop");
	/*NOTREACHED*/
}

/*
 * segdev pages are not in the cache, and thus can't really be controlled.
 * Hence, syncs are simply always successful.
 */
/*ARGSUSED*/
static int
segdev_sync(struct seg *seg, caddr_t addr, size_t len, int attr, u_int flags)
{
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_SYNC, "segdev_sync:start");

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segdev pages are always "in core".
 */
/*ARGSUSED*/
static size_t
segdev_incore(struct seg *seg, caddr_t addr, size_t len, char *vec)
{
	size_t v = 0;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_INCORE, "segdev_incore:start");

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	for (len = (len + PAGEOFFSET) & PAGEMASK; len; len -= PAGESIZE,
	    v += PAGESIZE)
		*vec++ = 1;
	return (v);
}

/*
 * segdev pages are not in the cache, and thus can't really be controlled.
 * Hence, locks are simply always successful.
 */
/*ARGSUSED*/
static int
segdev_lockop(struct seg *seg, caddr_t addr,
    size_t len, int attr, int op, u_long *lockmap, size_t pos)
{
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_LOCKOP, "segdev_lockop:start");

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segdev pages are not in the cache, and thus can't really be controlled.
 * Hence, advise is simply always successful.
 */
/*ARGSUSED*/
static int
segdev_advise(struct seg *seg, caddr_t addr, size_t len, u_int behav)
{
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_ADVISE, "segdev_advise:start");

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segdev pages are not dumped, so we just return
 */
/*ARGSUSED*/
static void
segdev_dump(struct seg *seg)
{}

/*
 * ddi_segmap_setup:	Used by drivers who wish specify mapping attributes
 *			for a segment.	Called from a drivers segmap(9E)
 *			routine.
 */
/*ARGSUSED*/
int
ddi_segmap_setup(dev_t dev, off_t offset, struct as *as, caddr_t *addrp,
    off_t len, u_int prot, u_int maxprot, u_int flags, cred_t *cred,
    ddi_device_acc_attr_t *accattrp, u_int rnumber)
{
	struct segdev_crargs dev_a;
	int (*mapfunc)(dev_t dev, off_t off, int prot);
	u_int hat_attr;
	pfn_t pfn;
	int	error, i;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_SEGMAP_SETUP,
	    "ddi_segmap_setup:start");

	if ((mapfunc = devopsp[getmajor(dev)]->devo_cb_ops->cb_mmap) == nodev)
		return (ENODEV);

	/*
	 * Character devices that support the d_mmap
	 * interface can only be mmap'ed shared.
	 */
	if ((flags & MAP_TYPE) != MAP_SHARED)
		return (EINVAL);

	/*
	 * Check that this region is indeed mappable on this platform.
	 * Use the mapping function.
	 */
	if (ddi_device_mapping_check(dev, accattrp, rnumber, &hat_attr) == -1)
		return (ENXIO);

	/*
	 * Check to ensure that the entire range is
	 * legal and we are not trying to map in
	 * more than the device will let us.
	 */
	for (i = 0; i < len; i += PAGESIZE) {
		if (i == 0) {
			/*
			 * Save the pfn at offset here. This pfn will be
			 * used later to get user address.
			 */
			if ((pfn = (pfn_t)cdev_mmap(mapfunc, dev, offset,
				maxprot)) == (pfn_t)-1)
				return (ENXIO);
		} else {
			if (cdev_mmap(mapfunc, dev, offset + i, maxprot) == -1)
				return (ENXIO);
		}
	}

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		/*
		 * Pick an address w/o worrying about
		 * any vac alignment constraints.
		 */
		map_addr(addrp, len, ptob(pfn), 0, flags);
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
	dev_a.offset = (offset_t)offset;
	dev_a.type = flags & MAP_TYPE;
	dev_a.prot = (u_char)prot;
	dev_a.maxprot = (u_char)maxprot;
	dev_a.hat_attr = hat_attr;
	dev_a.hat_flags = 0;
	dev_a.devmap_data = NULL;

	error = as_map(as, *addrp, len, segdev_create, &dev_a);
	as_rangeunlock(as);
	return (error);

}

/*ARGSUSED*/
static int
segdev_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_PAGELOCK,
	    "segdev_pagelock:start");
	return (ENOTSUP);
}

/*
 * devmap_device: Used by devmap framework to establish mapping
 *                called by devmap_seup(9F) during map setup time.
 */
/*ARGSUSED*/
static int
devmap_device(devmap_handle_t *dhp, struct as *as, caddr_t *addr,
    offset_t off, size_t len, u_int flags)
{
	devmap_handle_t *rdhp, *maxdhp;
	struct segdev_crargs dev_a;
	int	err;
	u_int maxprot = PROT_ALL;
	offset_t offset = 0;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_DEVICE,
	    "devmap_device:start dhp=%p addr=%p off=%llx, len=%lx",
	    (void *)dhp, (void *)addr, off, len);

	DEBUGF(2, (CE_CONT, "devmap_device: dhp %p addr %p off %llx len %lx\n",
	    (void *)dhp, (void *)addr, off, len));

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		offset_t aligned_off;

		rdhp = maxdhp = dhp;
		while (rdhp != NULL) {
			maxdhp = (maxdhp->dh_len > rdhp->dh_len) ?
				maxdhp : rdhp;
			rdhp = rdhp->dh_next;
			maxprot |= dhp->dh_maxprot;
		}
		offset = maxdhp->dh_uoff - dhp->dh_uoff;

		/*
		 * Use the dhp that has the
		 * largest len to get user address.
		 */
		if (dhp->dh_flags & DEVMAP_FLAG_DEVMEM) {
			aligned_off = (offset_t)ptob(maxdhp->dh_pfn) - offset;
		} else {
			aligned_off = (offset_t)maxdhp->dh_kvaddr - offset;
		}

		/*
		 * Pick an address aligned to dh_cookie.
		 * for kernel memory, cookie is kvaddr.
		 * for device memory, cookie is physical address.
		 */
		map_addr(addr, len, aligned_off, 1, flags);
		if (*addr == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/*
		 * User-specified address; blow away any previous mappings.
		 */
		(void) as_unmap(as, *addr, len);
	}

	dev_a.mapfunc = NULL;
	dev_a.dev = dhp->dh_dev;
	dev_a.type = flags & MAP_TYPE;
	dev_a.offset = off;
	/*
	 * sdp->maxprot has the least restrict protection of all dhps.
	 */
	dev_a.maxprot = maxprot;
	dev_a.prot = dhp->dh_prot;
	/*
	 * devmap uses dhp->dh_hat_attr for hat.
	 */
	dev_a.hat_flags = 0;
	dev_a.hat_attr = 0;
	dev_a.devmap_data = (void *)dhp;

	err = as_map(as, *addr, len, segdev_create, &dev_a);
	as_rangeunlock(as);
	return (err);
}

int
devmap_do_ctxmgt(devmap_cookie_t dhc, void *pvtp, offset_t off, size_t len,
    u_int type, u_int rw, int (*ctxmgt)(devmap_cookie_t, void *, offset_t,
    size_t, u_int, u_int))
{
	register devmap_handle_t *dhp = (devmap_handle_t *)dhc;
	struct devmap_ctx *devctx;
	int do_timeout = 0;
	int ret;

#ifdef lint
	pvtp = pvtp;
#endif

	TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_DO_CTXMGT,
	    "devmap_do_ctxmgt:start dhp=%p off=%llx, len=%lx",
	    (void *)dhp, off, len);
	DEBUGF(7, (CE_CONT, "devmap_do_ctxmgt: dhp %p off %llx len %lx\n",
	    (void *)dhp, off, len));

	if (ctxmgt == NULL)
		return (FC_HWERR);

	devctx = dhp->dh_ctx;

	/*
	 * If we are on an MP system with more than one cpu running
	 * and if a thread on some CPU already has the context, wait
	 * for it to finish if there is a hysteresis timeout.
	 *
	 * We call cv_wait() instead of cv_wait_sig() because
	 * it does not matter much if it returned due to a signal
	 * or due to a cv_signal() or cv_broadcast().  In either event
	 * we need to complete the mapping otherwise the processes
	 * will die with a SEGV.
	 */
	if ((dhp->dh_timeout_length > 0) && (ncpus > 1)) {
		TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_DO_CTXMGT_CK1,
		    "devmap_do_ctxmgt:doing hysteresis,"
		    "devctl=%p for t=%d",
		    (void *)devctx, dhp->dh_timeout_length);
		do_timeout = 1;
		mutex_enter(&devctx->lock);
		while (devctx->oncpu)
			cv_wait(&devctx->cv, &devctx->lock);
		devctx->oncpu = 1;
		mutex_exit(&devctx->lock);
	}

	/*
	 * Call the contextmgt callback so that the driver can handle
	 * the fault.
	 */
	ret = (*ctxmgt)(dhp, dhp->dh_pvtp, off, len, type, rw);

	/*
	 * If devmap_access() returned -1, then there was a hardware
	 * error so we need to convert the return value to something
	 * that trap() will understand.  Otherwise, the return value
	 * is already a fault code generated by devmap_unload()
	 * or devmap_load().
	 */
	if (ret) {
		TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_DO_CTXMGT_CK2,
		    "devmap_do_ctxmgt: ret=%x dhp=%p oncpu=%x",
		    ret, (void *)dhp, devctx->oncpu);
		DEBUGF(1, (CE_CONT, "devmap_do_ctxmgt: ret %x dhp %p\n",
		    ret, (void *)dhp));
		if (devctx->oncpu) {
			mutex_enter(&devctx->lock);
			devctx->oncpu = 0;
			cv_signal(&devctx->cv);
			mutex_exit(&devctx->lock);
		}
		return (FC_HWERR);
	}

	/*
	 * Setup the timeout if we need to
	 */
	if (do_timeout) {
		mutex_enter(&devctx->lock);
		if (dhp->dh_timeout_length > 0) {
			TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_DO_CTXMGT_CK3,
			    "devmap_do_ctxmgt:timeout set");
			devctx->timeout = timeout(devmap_ctxto,
			    devctx, dhp->dh_timeout_length);
		} else {
			/*
			 * We don't want to wait so set oncpu to
			 * 0 and wake up anyone waiting.
			 */
			TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_DO_CTXMGT_CK4,
			    "devmap_do_ctxmgt:timeout not set");
			devctx->oncpu = 0;
			cv_signal(&devctx->cv);
		}
		mutex_exit(&devctx->lock);
	}

	return (DDI_SUCCESS);
}

/*
 *                                       end of mapping
 *                    poff   fault_offset         |
 *            base     |        |                 |
 *              |      |        |                 |
 *              V      V        V                 V
 *  +-----------+---------------+-------+---------+-------+
 *              ^               ^       ^         ^
 *              |<--- offset--->|<-len->|         |
 *              |<--- dh_len(size of mapping) --->|
 *                     |<--  pg -->|
 *                              -->|rlen|<--
 */
static u_long
devmap_roundup(devmap_handle_t *dhp, u_long offset, size_t len,
    u_long *opfn, u_long *pagesize)
{
	register int level;
	u_long pg;
	u_long poff;
	u_long base;
	caddr_t uvaddr;
	long rlen;

	TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_ROUNDUP,
	    "devmap_roundup:start dhp=%p off=%lx len=%lx",
	    (void *)dhp, offset, len);
	DEBUGF(2, (CE_CONT, "devmap_roundup: dhp %p off %lx len %lx\n",
	    (void *)dhp, offset, len));

	/*
	 * get the max. pagesize that is aligned within the range
	 * <dh_pfn, dh_pfn+offset>.
	 *
	 * The calculations below use physical address to determine
	 * the page size to use. The same calculations can use the
	 * virtual address to etermine the page size.
	 */
	base = (u_long)ptob(dhp->dh_pfn);
	for (level = dhp->dh_mmulevel; level >= 0; level--) {
		pg = page_get_pagesize(level);
		poff = ((base + offset) & ~(pg - 1));
		uvaddr = dhp->dh_uvaddr + (poff - base);
		if ((poff >= base) &&
		    ((poff + pg) <= (base + dhp->dh_len)) &&
		    VA_PA_ALIGNED((uintptr_t)uvaddr, poff, pg))
			break;
	}

	TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_ROUNDUP_CK1,
	    "devmap_roundup: base=%lx poff=%lx pfn=%lx",
	    base, poff, dhp->dh_pfn);
	DEBUGF(2, (CE_CONT, "devmap_roundup: base %lx poff %lx pfn %lx\n",
	    base, poff, dhp->dh_pfn));

	ASSERT(VA_PA_ALIGNED((uintptr_t)uvaddr, poff, pg));
	ASSERT(level >= 0);

	*pagesize = pg;
	*opfn = dhp->dh_pfn + btop(poff - base);

	rlen = len + offset - (poff - base + pg);

	ASSERT(rlen < (long)len);

	TRACE_5(TR_FAC_DEVMAP, TR_DEVMAP_ROUNDUP_CK2,
	    "devmap_roundup:ret dhp=%p level=%x rlen=%lx psiz=%lx opfn=%lx",
	    (void *)dhp, level, rlen, *pagesize, *opfn);
	DEBUGF(1, (CE_CONT, "devmap_roundup: dhp %p "
	    "level %x rlen %lx psize %lx opfn %lx\n",
	    (void *)dhp, level, rlen, *pagesize, *opfn));

	return ((u_long)((rlen > 0) ? rlen : 0));
}

/*
 * find the dhp that contains addr.
 */
static devmap_handle_t *
devmap_find_handle(devmap_handle_t *dhp_head, caddr_t addr)
{
	devmap_handle_t *dhp;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_FIND_HANDLE,
	    "devmap_find_handle:start");

	dhp = dhp_head;
	while (dhp) {
		if (addr >= dhp->dh_uvaddr &&
		    addr < (dhp->dh_uvaddr + dhp->dh_len))
			return (dhp);
		dhp = dhp->dh_next;
	}

	return ((devmap_handle_t *)NULL);
}

/*
 * devmap_unload:
 *			Marks a segdev segment or pages if offset->offset+len
 *			is not the entire segment as intercept and unloads the
 *			pages in the range offset -> offset+len.
 */
int
devmap_unload(devmap_cookie_t dhc, offset_t offset, size_t len)
{
	register devmap_handle_t *dhp = (devmap_handle_t *)dhc;
	caddr_t	addr;
	u_long	size;
	ssize_t	soff;

	TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_UNLOAD,
	    "devmap_unload:start dhp=%p offset=%llx len=%lx",
	    (void *)dhp, offset, len);
	DEBUGF(7, (CE_CONT, "devmap_unload: dhp %p offset %llx len %lx\n",
	    (void *)dhp, offset, len));

	soff = (ssize_t)(offset - dhp->dh_uoff);
	soff = round_down_p2(soff, PAGESIZE);
	if (soff < 0 || soff >= dhp->dh_len)
		return (FC_MAKE_ERR(EINVAL));

	/*
	 * Address and size must be page aligned.  Len is set to the
	 * number of bytes in the number of pages that are required to
	 * support len.  Offset is set to the byte offset of the first byte
	 * of the page that contains offset.
	 */
	len = round_up_p2(len, PAGESIZE);

	/*
	 * If len is == 0, then calculate the size by getting
	 * the number of bytes from offset to the end of the segment.
	 */
	if (len == 0)
		size = dhp->dh_len - soff;
	else {
		size = len;
		if ((soff + size) > dhp->dh_len)
			return (FC_MAKE_ERR(EINVAL));
	}

	/*
	 * The address is offset bytes from the base address of
	 * the dhp.
	 */
	addr = (caddr_t)(soff + dhp->dh_uvaddr);

	/*
	 * If large page size was used in hat_devload(),
	 * the same page size must be used in hat_unload().
	 */
	if (dhp->dh_flags & DEVMAP_FLAG_LARGE) {
		hat_unload(dhp->dh_seg->s_as->a_hat, dhp->dh_uvaddr,
			dhp->dh_len, HAT_UNLOAD|HAT_UNLOAD_OTHER);
	} else {
		hat_unload(dhp->dh_seg->s_as->a_hat,  addr, size,
			HAT_UNLOAD|HAT_UNLOAD_OTHER);
	}

	return (0);
}

/*
 * calculates the optimal page size that will be used for hat_devload().
 */
static void
devmap_get_large_pgsize(devmap_handle_t *dhp, size_t len, caddr_t addr,
    size_t *llen, caddr_t *laddr)
{
	u_long off;
	u_long pfn;
	u_long pgsize;
	u_int first = 1;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_GET_LARGE_PGSIZE,
	    "devmap_get_large_pgsize:start");

	*llen = 0;
	off = (u_long)(addr - dhp->dh_uvaddr);
	while ((long)len > 0) {
		/*
		 * get the optimal pfn to minimize address translations.
		 * devmap_roundup() returns residure bytes for next round
		 * calculations.
		 */
		len = devmap_roundup(dhp, off, len, &pfn, &pgsize);

		if (first) {
			*laddr = dhp->dh_uvaddr + ptob(pfn - dhp->dh_pfn);
			first = 0;
		}

		*llen += pgsize;
		off = ptob(pfn - dhp->dh_pfn) + pgsize;
	}
}

/*
 * Initialize the devmap_softlock structure.
 */
static struct devmap_softlock *
devmap_softlock_init(dev_t dev, u_long id)
{
	struct devmap_softlock *slock;
	struct devmap_softlock *tmp;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_SOFTLOCK_INIT,
	    "devmap_softlock_init:start");

	tmp = kmem_zalloc(sizeof (struct devmap_softlock), KM_SLEEP);
	mutex_enter(&devmap_slock);

	for (slock = devmap_slist; slock != NULL; slock = slock->next)
		if ((slock->dev == dev) && (slock->id == id))
			break;

	if (slock == NULL) {
		slock = tmp;
		slock->dev = dev;
		slock->id = id;
		mutex_init(&slock->lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&slock->cv, NULL, CV_DEFAULT, NULL);
		slock->next = devmap_slist;
		devmap_slist = slock;
	} else
		kmem_free(tmp, sizeof (struct devmap_softlock));

	mutex_enter(&slock->lock);
	slock->refcnt++;
	mutex_exit(&slock->lock);
	mutex_exit(&devmap_slock);

	return (slock);
}

/*
 * Wake up processes that sleep on softlocked.
 * Free dh_softlock if refcnt is 0.
 */
static void
devmap_softlock_rele(devmap_handle_t *dhp)
{
	struct devmap_softlock *slock = dhp->dh_softlock;
	struct devmap_softlock *tmp;
	struct devmap_softlock *parent;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_SOFTLOCK_RELE,
	    "devmap_softlock_rele:start");

	mutex_enter(&devmap_slock);
	mutex_enter(&slock->lock);

	ASSERT(slock->refcnt > 0);

	slock->refcnt--;

	/*
	 * If no one is using the device, free up the slock data.
	 */
	if (slock->refcnt == 0) {
		slock->softlocked = 0;
		cv_signal(&slock->cv);

		if (devmap_slist == slock)
			devmap_slist = slock->next;
		else {
			parent = devmap_slist;
			for (tmp = devmap_slist->next; tmp != NULL;
				tmp = tmp->next) {
				if (tmp == slock) {
					parent->next = tmp->next;
					break;
				}
				parent = tmp;
			}
		}
		mutex_exit(&slock->lock);
		mutex_destroy(&slock->lock);
		cv_destroy(&slock->cv);
		kmem_free(slock, sizeof (struct devmap_softlock));
	} else
		mutex_exit(&slock->lock);

	mutex_exit(&devmap_slock);
}

/*
 * Wake up processes that sleep on dh_ctx->locked.
 * Free dh_ctx if refcnt is 0.
 */
static void
devmap_ctx_rele(devmap_handle_t *dhp)
{
	struct devmap_ctx *devctx = dhp->dh_ctx;
	struct devmap_ctx *tmp;
	struct devmap_ctx *parent;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_CTX_RELE,
	    "devmap_ctx_rele:start");

	mutex_enter(&devmapctx_lock);
	mutex_enter(&devctx->lock);

	ASSERT(devctx->refcnt > 0);

	devctx->refcnt--;

	/*
	 * If no one is using the device, free up the devctx data.
	 */
	if (devctx->refcnt == 0) {
		/*
		 * Untimeout any threads using this mapping as they are about
		 * to go away.
		 */
		if (devctx->timeout != 0) {
			TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_CTX_RELE_CK1,
			    "devmap_ctx_rele:untimeout ctx->timeout");
			(void) untimeout(devctx->timeout);
		}

		devctx->oncpu = 0;
		cv_signal(&devctx->cv);

		if (devmapctx_list == devctx)
			devmapctx_list = devctx->next;
		else {
			parent = devmapctx_list;
			for (tmp = devmapctx_list->next; tmp != NULL;
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
		kmem_free(devctx, sizeof (struct devmap_ctx));
	} else
		mutex_exit(&devctx->lock);

	mutex_exit(&devmapctx_lock);
}

/*
 * devmap_load:
 *			Marks a segdev segment or pages if offset->offset+len
 *			is not the entire segment as nointercept and faults in
 *			the pages in the range offset -> offset+len.
 */
int
devmap_load(devmap_cookie_t dhc, offset_t offset, size_t len, u_int type,
    u_int rw)
{
	devmap_handle_t *dhp = (devmap_handle_t *)dhc;
	caddr_t	addr;
	u_long	size;
	ssize_t	soff;	/* offset from the beginning of the segment */

	TRACE_3(TR_FAC_DEVMAP, TR_DEVMAP_LOAD,
	    "devmap_load:start dhp=%p offset=%llx len=%lx",
		(void *)dhp, offset, len);

	ASSERT(dhp != NULL);

	DEBUGF(7, (CE_CONT, "devmap_load: dhp %p offset %llx len %lx\n",
	    (void *)dhp, offset, len));

	soff = (ssize_t)(offset - dhp->dh_uoff);
	soff = round_down_p2(soff, PAGESIZE);
	if (soff < 0 || soff >= dhp->dh_len)
		return (FC_MAKE_ERR(EINVAL));

	/*
	 * Address and size must be page aligned.  Len is set to the
	 * number of bytes in the number of pages that are required to
	 * support len.  Offset is set to the byte offset of the first byte
	 * of the page that contains offset.
	 */
	len = round_up_p2(len, PAGESIZE);

	/*
	 * If len == 0, then calculate the size by getting
	 * the number of bytes from offset to the end of the segment.
	 */
	if (len == 0)
		size = dhp->dh_len - soff;
	else {
		size = len;
		if ((soff + size) > dhp->dh_len)
			return (FC_MAKE_ERR(EINVAL));
	}

	/*
	 * The address is offset bytes from the base address of
	 * the segment.
	 */
	addr = (caddr_t)(soff + dhp->dh_uvaddr);

	return (segdev_faultpages(dhp->dh_seg->s_as->a_hat,
			dhp->dh_seg, addr, size, type, rw, dhp));
}

int
devmap_setup(dev_t dev, offset_t off, struct as *as, caddr_t *addrp,
    size_t len, u_int prot, u_int maxprot, u_int flags, struct cred *cred)
{
	register devmap_handle_t *dhp;
	int (*devmap)(dev_t, devmap_cookie_t, offset_t, size_t,
		size_t *, uint_t);
	int (*mmap)(dev_t, off_t, int);
	struct devmap_callback_ctl *callbackops;
	devmap_handle_t *dhp_head = NULL;
	devmap_handle_t *dhp_prev = NULL;
	devmap_handle_t *dhp_curr;
	caddr_t addr;
	int level;
	int map_flag;
	int ret;
	u_long pgsize;
	u_long total_len;
	size_t map_len;
	size_t resid_len = len;
	offset_t map_off = off;
	u_int n = page_num_pagesizes();

#ifdef lint
	cred = cred;
#endif

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_SETUP,
	    "devmap_setup:start off=%llx len=%lx", off, len);
	DEBUGF(3, (CE_CONT, "devmap_setup: off %llx len %lx\n",
	    off, len));

	devmap = devopsp[getmajor(dev)]->devo_cb_ops->cb_devmap;
	mmap = devopsp[getmajor(dev)]->devo_cb_ops->cb_mmap;

	/*
	 * driver must provide devmap(9E) entry point in cb_ops to use the
	 * devmap framework.
	 */
	if (devmap == NULL || devmap == nulldev || devmap == nodev)
		return (EINVAL);

	/*
	 * To protect from an inadvertent entry because the devmap entry point
	 * is not NULL, return error if D_DEVMAP bit is not set in cb_flag and
	 * mmap is NULL.
	 */
	map_flag = devopsp[getmajor(dev)]->devo_cb_ops->cb_flag;
	if ((map_flag & D_DEVMAP) == 0 && (mmap == NULL || mmap == nulldev))
		return (EINVAL);

	/*
	 * devmap allows mmap(2) to map multiple registers.
	 * one devmap_handle is created for each register mapped.
	 */
	for (total_len = 0; total_len < len; total_len += map_len) {
		dhp = kmem_zalloc(sizeof (devmap_handle_t), KM_SLEEP);

		if (dhp_prev != NULL)
			dhp_prev->dh_next = dhp;
		else
			dhp_head = dhp;
		dhp_prev = dhp;

		dhp->dh_prot = prot;
		dhp->dh_maxprot = maxprot;
		dhp->dh_dev = dev;
		dhp->dh_timeout_length = CTX_TIMEOUT_VALUE;
		dhp->dh_uoff = map_off;

		/*
		 * Get mapping specific info from
		 * the driver, such as rnumber, roff, len, callbackops,
		 * accattrp and, if the mapping is for kernel memory,
		 * ddi_umem_cookie.
		 */
		if ((ret = cdev_devmap(dev, dhp, map_off,
		    resid_len, &map_len, get_udatamodel())) != 0) {
			free_devmap_handle(dhp_head);
			return (ENXIO);
		}

		if (map_len & PAGEOFFSET) {
			free_devmap_handle(dhp_head);
			return (EINVAL);
		}

		callbackops = &dhp->dh_callbackops;

		if ((callbackops->devmap_access == NULL) ||
			(callbackops->devmap_access == nulldev) ||
			(callbackops->devmap_access == nodev)) {
			/*
			 * Normally devmap does not support MAP_PRIVATE unless
			 * the drivers provide a valid devmap_access routine.
			 */
			if ((flags & MAP_PRIVATE) != 0) {
				free_devmap_handle(dhp_head);
				return (EINVAL);
			}
		} else {
			/*
			 * Initialize dhp_softlock and dh_ctx if the drivers
			 * provide devmap_access.
			 */
			dhp->dh_softlock = devmap_softlock_init(dev,
				(u_long)callbackops->devmap_access);
			dhp->dh_ctx = devmap_ctxinit(dev,
				(u_long)callbackops->devmap_access);
		}

		map_off += map_len;
		resid_len -= map_len;
	}

	/*
	 * get the user virtual address and establish the mapping between
	 * uvaddr and device physical address.
	 */
	if ((ret = devmap_device(dhp_head, as, addrp, off, len, flags))
			!= 0) {
		/*
		 * free devmap handles if error during the mapping.
		 */
		free_devmap_handle(dhp_head);

		return (ret);
	}

	/*
	 * call the driver's devmap_map callback to do more after the mapping,
	 * such as to allocate driver private data for context management.
	 */
	dhp = dhp_head;
	map_off = off;
	addr = *addrp;
	while (dhp != NULL) {
		callbackops = &dhp->dh_callbackops;
		dhp->dh_uvaddr = addr;
		dhp_curr = dhp;
		if (callbackops->devmap_map != NULL) {
			ret = (*callbackops->devmap_map)((devmap_cookie_t)dhp,
					dev, flags, map_off,
					dhp->dh_len, &dhp->dh_pvtp);
			if (ret != 0) {
				struct segdev_data *sdp;

				/*
				 * call driver's devmap_unmap entry point
				 * to free driver resources.
				 */
				dhp = dhp_head;
				map_off = off;
				while (dhp != dhp_curr) {
					callbackops = &dhp->dh_callbackops;
					if (callbackops->devmap_unmap != NULL) {
						(*callbackops->devmap_unmap)(
							dhp, dhp->dh_pvtp,
							map_off, dhp->dh_len,
							NULL, NULL, NULL, NULL);
					}
					map_off += dhp->dh_len;
					dhp = dhp->dh_next;
				}
				sdp = dhp_head->dh_seg->s_data;
				sdp->devmap_data = NULL;
				free_devmap_handle(dhp_head);
				return (ENXIO);
			}
		}
		/*
		 * Calculate the max. page size for this mapping.
		 * this page size will be used in fault routine for
		 * optimal page size calculations.
		 */
		if (dhp->dh_flags & DEVMAP_FLAG_LARGE) {
			u_long base;

			level = 1;
			pgsize = page_get_pagesize(level);

			DEBUGF(4, (CE_CONT, "devmap_setup: dhp %p "
			    "pgsize %lx addr %p n %d\n",
			    (void *)dhp, pgsize, (void *)(*addrp), n));

			base = (u_long)ptob(dhp->dh_pfn);
			while ((level < n) && (dhp->dh_len >= pgsize) &&
				VA_PA_PGSIZE_ALIGNED((uintptr_t)dhp->dh_uvaddr,
				base, pgsize)) {
				if (++level >= n)
					break;
				pgsize = page_get_pagesize(level);
			}
			dhp->dh_mmulevel = level - 1;

			if (dhp->dh_mmulevel == 0)
				dhp->dh_flags &= ~DEVMAP_FLAG_LARGE;
		}
		map_off += dhp->dh_len;
		addr += dhp->dh_len;
		dhp = dhp->dh_next;
	}

	return (0);
}

int
ddi_devmap_segmap(dev_t dev, off_t off, ddi_as_handle_t as, caddr_t *addrp,
    off_t len, u_int prot, u_int maxprot, u_int flags, struct cred *cred)
{
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_SEGMAP,
	    "devmap_segmap:start");
	return (devmap_setup(dev, (offset_t)off, (struct as *)as, addrp,
	    (size_t)len, prot, maxprot, flags, cred));
}

/*
 * Called by driver devmap routine to pass device specific info to
 * the framework.    used for device memory mapping only.
 */
int
devmap_devmem_setup(devmap_cookie_t dhc, dev_info_t *dip,
    struct devmap_callback_ctl *callbackops, u_int rnumber, offset_t roff,
    size_t len, u_int maxprot, u_int flags, ddi_device_acc_attr_t *accattrp)
{
	devmap_handle_t *dhp = (devmap_handle_t *)dhc;
	ddi_acc_handle_t handle;
	ddi_map_req_t mr;
	ddi_acc_hdl_t *hp;
	int err;

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_DEVMEM_SETUP,
	    "devmap_devmem_setup:start dhp=%p offset=%llx rnum=%d len=%lx",
	    (void *)dhp, roff, rnumber, (u_int)len);
	DEBUGF(2, (CE_CONT, "devmap_devmem_setup: dhp %p offset %llx "
	    "rnum %d len %lx\n", (void *)dhp, roff, rnumber, len));

	if (callbackops != NULL) {
		if ((flags & DEVMAP_ALLOW_REMAP &&
		    callbackops->devmap_access == NULL)) {
			return (DDI_FAILURE);
		}
	}

	/*
	 * First to check if this function has been called for this dhp.
	 */
	if (dhp->dh_flags & DEVMAP_SETUP_DONE)
		return (DDI_FAILURE);

	if (flags & DEVMAP_MAPPING_INVALID) {
		/*
		 * Don't go up the tree to get pfn if the driver specifies
		 * DEVMAP_MAPPING_INVALID in flags.
		 *
		 * If DEVMAP_MAPPING_INVALID is specified, we have to grant
		 * remap permission.
		 */
		flags |= DEVMAP_ALLOW_REMAP;
	} else {
		handle = impl_acc_hdl_alloc(KM_SLEEP, NULL);
		if (handle == NULL)
			return (DDI_FAILURE);

		hp = impl_acc_hdl_get(handle);
		hp->ah_vers = VERS_ACCHDL;
		hp->ah_dip = dip;
		hp->ah_rnumber = rnumber;
		hp->ah_offset = roff;
		hp->ah_len = len;
		if (accattrp != NULL)
			hp->ah_acc = *accattrp;

		mr.map_op = DDI_MO_MAP_LOCKED;
		mr.map_type = DDI_MT_RNUMBER;
		mr.map_obj.rnumber = rnumber;
		mr.map_prot = maxprot & dhp->dh_maxprot;
		mr.map_flags = DDI_MF_DEVICE_MAPPING;
		mr.map_handlep = hp;
		mr.map_vers = DDI_MAP_VERSION;

		/*
		 * up the device tree to get pfn.
		 * The rootnex_map_regspec() routine in nexus drivers has been
		 * modified to return pfn if map_flags is DDI_MF_DEVICE_MAPPING.
		 */
		err = ddi_map(dip, &mr, roff, len,
				(caddr_t *)&dhp->dh_pfn);

		dhp->dh_hat_attr = hp->ah_hat_flags;
		impl_acc_hdl_free(handle);

		if (err)
			return (DDI_FAILURE);
	}

	dhp->dh_flags |= (DEVMAP_FLAG_DEVMEM | flags);
	dhp->dh_len = ptob(btopr(len));
	/*
	 * use large page size only if:
	 *  1. mmu supports multiple page sizes,
	 *  2. mapping length is larger than the second
	 *		level page size, and
	 *  3. device memory.
	 */
	if (page_num_pagesizes() > 1 &&
			(dhp->dh_len >= page_get_pagesize(1)))
		dhp->dh_flags |= DEVMAP_FLAG_LARGE;

	dhp->dh_roff = ptob(btop(roff));
	dhp->dh_maxprot = maxprot & dhp->dh_maxprot;

	/*
	 * XXX error code for protection viloation during mapping setup.
	 */
	if ((dhp->dh_prot & dhp->dh_maxprot) != dhp->dh_prot)
		return (DDI_FAILURE);

	if (callbackops != NULL) {
		bcopy(callbackops, &dhp->dh_callbackops,
		    sizeof (struct devmap_callback_ctl));

		/*
		 * Initialize dh_lock and dh_cv if we want to do remap.
		 */
		if ((dhp->dh_flags & DEVMAP_ALLOW_REMAP) &&
		    (callbackops->devmap_access != NULL)) {
			mutex_init(&dhp->dh_lock, NULL, MUTEX_DEFAULT, NULL);
			cv_init(&dhp->dh_cv, NULL, CV_DEFAULT, NULL);
			dhp->dh_flags |= DEVMAP_LOCK_INITED;
		}
	}

	dhp->dh_flags |= DEVMAP_SETUP_DONE;

	return (DDI_SUCCESS);
}

#ifdef later
int
devmap_devmem_remap(devmap_cookie_t dhc, u_int rnumber, offset_t roff,
    size_t len, u_int maxprot, ddi_device_acc_attr_t *accattrp)
{
	devmap_handle_t *dhp = (devmap_handle_t *)dhc;

#ifdef lint
	rnumber = rnumber;
	roff = roff;
	len = len;
	maxprot = maxprot;
	accattrp = accattrp;
#endif
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_DEVMEM_REMAP,
	    "devmap_devmem_remap:start");

	/*
	 * Return failure if setup has not been done or no remap permission
	 * has been granted during the setup.
	 */
	if ((dhp->dh_flags & DEVMAP_SETUP_DONE) == 0 ||
	    (dhp->dh_flags & DEVMAP_ALLOW_REMAP) == 0)
		return (DDI_FAILURE);

	return (DDI_FAILURE);
}
#endif later

/*
 * called by driver devmap routine to pass kernel virtual address  mapping
 * info to the framework.    used only for kernel memory
 * allocated from ddi_umem_alloc().
 */
int
devmap_umem_setup(devmap_cookie_t dhc, dev_info_t *dip,
    struct devmap_callback_ctl *callbackops, ddi_umem_cookie_t cookie,
    offset_t off, size_t len, u_int maxprot, u_int flags,
    ddi_device_acc_attr_t *accattrp)
{
	devmap_handle_t *dhp = (devmap_handle_t *)dhc;
	struct ddi_umem_cookie *cp = (struct ddi_umem_cookie *)cookie;

#ifdef lint
	dip = dip;
	accattrp = accattrp;
#endif

	TRACE_4(TR_FAC_DEVMAP, TR_DEVMAP_UMEM_SETUP,
	    "devmap_umem_setup:start dhp=%p offset=%llx cookie=%p len=%lx",
	    (void *)dhp, off, cookie, len);
	DEBUGF(2, (CE_CONT, "devmap_umem_setup: dhp %p offset %llx "
	    "cookie %p len %lx\n", (void *)dhp, off, (void *)cookie, len));

	if (cookie == NULL)
		return (DDI_FAILURE);

	if ((off + len) > cp->size)
		return (DDI_FAILURE);

	if (callbackops != NULL &&
	    (flags & DEVMAP_ALLOW_REMAP) != 0 &&
	    callbackops->devmap_access == NULL) {
		return (DDI_FAILURE);
	}

	/*
	 * First to check if this function has been called for this dhp.
	 */
	if (dhp->dh_flags & DEVMAP_SETUP_DONE)
		return (DDI_FAILURE);

	if (flags & DEVMAP_MAPPING_INVALID) {
		/*
		 * If DEVMAP_MAPPING_INVALID is specified, we have to grant
		 * remap permission.
		 */
		flags |= DEVMAP_ALLOW_REMAP;
	} else {
		if (cp->type == KMEM_PAGEABLE)
			dhp->dh_flags |= DEVMAP_FLAG_KPMEM;
		else
			dhp->dh_flags |= DEVMAP_FLAG_KMEM;

		dhp->dh_cookie = cookie;
		dhp->dh_roff = ptob(btop(off));
		dhp->dh_kvaddr = cp->kvaddr + dhp->dh_roff;
	}

	/*
	 * The default is _not_ to pass HAT_LOAD_NOCONSIST to hat_devload();
	 * we pass HAT_LOAD_NOCONSIST _only_ in cases where hat tries to
	 * create consistent mappings but our intention was to create
	 * non-consistent mappings.

	 * DEVMEM: hat figures it out it's DEVMEM and creates non-consistent
	 * mappings.

	 * kernel exported memory: hat figures it out it's memory and always
	 * creates consistent mappings.

	 * /dev/mem: non-consistent mappings. See comments in common/io/mem.c

	 * /dev/kmem: consistent mappings are created unless they are
	 * MAP_FIXED. We _explicitly_ tell hat to create non-consistent
	 * mappings by passing HAT_LOAD_NOCONSIST in case of MAP_FIXED
	 * mappings of /dev/kmem. See common/io/mem.c
	 */
	dhp->dh_flags |= flags;
	dhp->dh_len = ptob(btopr(len));
	dhp->dh_maxprot = maxprot & dhp->dh_maxprot;
	if ((dhp->dh_prot & dhp->dh_maxprot) != dhp->dh_prot)
		return (DDI_FAILURE);

	if (callbackops != NULL) {
		bcopy(callbackops, &dhp->dh_callbackops,
		    sizeof (struct devmap_callback_ctl));

		/*
		 * Initialize dh_lock and dh_cv if we want to do remap.
		 */
		if ((dhp->dh_flags & DEVMAP_ALLOW_REMAP &&
		    callbackops->devmap_access != NULL)) {
			mutex_init(&dhp->dh_lock, NULL, MUTEX_DEFAULT, NULL);
			cv_init(&dhp->dh_cv, NULL, CV_DEFAULT, NULL);
			dhp->dh_flags |= DEVMAP_LOCK_INITED;
		}
	}

	dhp->dh_flags |= DEVMAP_SETUP_DONE;

	return (DDI_SUCCESS);
}

#ifdef later
int
devmap_umem_remap(devmap_cookie_t dhc, ddi_umem_cookie_t cookie,
    offset_t off, size_t len, u_int maxprot, ddi_device_acc_attr_t *accattrp)
{
	devmap_handle_t *dhp = (devmap_handle_t *)dhc;

#ifdef lint
	cookie = cookie;
	off = off;
	len = len;
	maxprot = maxprot;
	accattrp = accattrp;
#endif

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_UMEM_REMAP,
	    "devmap_umem_remap:start");
	/*
	 * Reture failure if setup has not been done or no remap permission
	 * has been granted during the setup.
	 */
	if ((dhp->dh_flags & DEVMAP_SETUP_DONE) == 0 ||
		(dhp->dh_flags & DEVMAP_ALLOW_REMAP) == 0)
		return (DDI_FAILURE);

	return (DDI_FAILURE);

}
#endif later

/*
 * to set timeout value for the driver's context management callback, e.g.
 * devmap_access().
 */
void
devmap_set_ctx_timeout(devmap_cookie_t dhc, clock_t ticks)
{
	devmap_handle_t *dhp = (devmap_handle_t *)dhc;

	TRACE_2(TR_FAC_DEVMAP, TR_DEVMAP_SET_CTX_TIMEOUT,
	    "devmap_set_ctx_timeout:start dhp=%p ticks=%x",
	    (void *)dhp, ticks);
	dhp->dh_timeout_length = ticks;
}

int
devmap_default_access(devmap_cookie_t dhp, void *pvtp, offset_t off,
    size_t len, u_int type, u_int rw)
{
#ifdef lint
	pvtp = pvtp;
#endif

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_DEFAULT_ACCESS,
	    "devmap_default_access:start");
	if (type == F_PROT) {
		/*
		 * Since the seg_dev driver does not implement copy-on-write,
		 * this means that a valid translation is already loaded,
		 * but we got an fault trying to access the device.
		 * Return an error here to prevent going in an endless
		 * loop reloading the same translation...
		 */
		return (FC_PROT);
	}

	return (devmap_load(dhp, off, len, type, rw));
}

/*
 * allocate page aligned kernel memory for exporting to user land.
 * The devmap framework will use the cookie allocated by ddi_umem_alloc()
 * to find a user virtual address that is in same color as the address
 * allocated here.
 */
void *
ddi_umem_alloc(size_t size, int flags, ddi_umem_cookie_t *cookie)
{
	register size_t len = ptob(btopr(size));
	void *buf = NULL;
	struct ddi_umem_cookie *cp;
	int iflags = 0;

	*cookie = NULL;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_UMEM_ALLOC,
	    "devmap_umem_alloc:start");
	if (size == 0)
		return ((void *)NULL);

	/*
	 * allocate cookie
	 */
	if ((cp = kmem_zalloc(sizeof (struct ddi_umem_cookie),
	    flags & KM_NOSLEEP)) == NULL) {
		return ((void *)NULL);
	}

	if (flags & DDI_UMEM_PAGEABLE) {
		/* initialize resource with 0 */
		iflags = KPD_ZERO;

		/*
		 * to allocate unlocked pageable memory, use segkp_get() to
		 * create a segkp segment.  Since segkp can only service kas,
		 * other segment drivers such as segdev have to do
		 * as_fault(segkp, SOFTLOCK) in its fault routine,
		 */
		if (flags & DDI_UMEM_NOSLEEP)
			iflags |= KPD_NOWAIT;

		if ((buf = segkp_get(segkp, len, iflags)) == NULL) {
			kmem_free(cp, sizeof (struct ddi_umem_cookie));
			return ((void *)NULL);
		}
		cp->type = KMEM_PAGEABLE;
		mutex_init(&cp->lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&cp->cv, NULL, CV_DEFAULT, NULL);
		cp->locked = 0;
	} else {
		if ((buf = kmem_zalloc(len, flags & DDI_UMEM_NOSLEEP ?
		    KM_NOSLEEP : KM_SLEEP)) == NULL) {
			kmem_free(cp, sizeof (struct ddi_umem_cookie));
			return ((void *)NULL);
		}

		cp->type = KMEM_NON_PAGEABLE;
	}

	/*
	 * need to save size here.  size will be used when
	 * we do kmem_free.
	 */
	cp->size = len;
	cp->kvaddr = (caddr_t)buf;

	*cookie =  (void *)cp;
	return (buf);
}

void
ddi_umem_free(ddi_umem_cookie_t cookie)
{
	struct ddi_umem_cookie *cp;

	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_UMEM_FREE,
	    "devmap_umem_free:start");

	/*
	 * if cookie is NULL, no effects on the system
	 */
	if (cookie == NULL)
		return;

	cp = (struct ddi_umem_cookie *)cookie;

	ASSERT(cp->kvaddr != NULL && cp->size != 0);

	if (cp->type == KMEM_PAGEABLE) {
		segkp_release(segkp, cp->kvaddr);

		/*
		 * release mutex and cv associated with this cookie.
		 */
		mutex_enter(&cp->lock);
		cp->locked = 0;
		cv_signal(&cp->cv);
		mutex_exit(&cp->lock);
		mutex_destroy(&cp->lock);
		cv_destroy(&cp->cv);
	} else
		kmem_free(cp->kvaddr, cp->size);

	kmem_free(cookie, sizeof (struct ddi_umem_cookie));

}


static int
segdev_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	struct segdev_data *sdp = (struct segdev_data *)seg->s_data;

	/*
	 * It looks as if it is always mapped shared
	 */
	TRACE_0(TR_FAC_DEVMAP, TR_DEVMAP_GETMEMID,
	    "segdev_getmemid:start");
	memidp->val[0] = (u_longlong_t)VTOCVP(sdp->vp);
	memidp->val[1] = sdp->offset + (addr - seg->s_base);
	return (0);
}
