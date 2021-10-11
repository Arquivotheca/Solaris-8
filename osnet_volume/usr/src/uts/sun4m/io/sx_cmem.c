/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Pseudo driver/Segment Driver for pre-allocated physically contiguous DRAM.
 * For use by SPARCstation-10SX, SPARCstation-20 SX graphics libraries
 */

#pragma ident	"@(#)sx_cmem.c	1.94	99/12/04 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/vmem.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/ksynch.h>	/* Synchronization primitives */
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/vmsystm.h>

#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/hat.h>
#include <vm/mhat.h>

#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/obpdefs.h>
#include <sys/sx_cmemio.h>

#include <sys/vnode.h>

struct	vnode	sx_cmem_vnode;

/*
 * ALL chunks of pre-allocated physically contiguous DRAM will be managed by a
 * single arena.
 */
static vmem_t *sx_cmem_arena;
static struct cloneblk *cmem_clones;
static struct cmresv **cmem_table[CMTBLSZ];
char cmem_tblcnt[CMTBLSZ];

u_int sx_total_cpages = NULL;
u_int sx_cmem_mbleft = 0;
u_int sx_cmem_frag = 0;
extern u_int sx_cmem_mbreq;
extern struct sx_cmem_list *sx_cmem_head;
extern page_t *page_get_contig(u_int req_bytes, int addr_align);
extern int page_free_contig(page_t *pp);

/*
 * A single mutex is used to protect access to all driver private data thus
 * providing coarse granularity. The granularity might have to be finer if
 * performance is affected.
 */

static kmutex_t sx_cmem_mutex;

static void sx_cmem_init(void);
static	int segsx_cmem_create(/* seg, crargsp */);
static	struct cmresv *sx_cmem_cmfind(/* offset */);

static dev_info_t *sx_cmem_dip;	/* Private copy of the devinfo ptr */
extern struct mod_ops mod_driverops;

static int sx_cmem_open(dev_t *, int, int, cred_t *);
static int sx_cmem_close(dev_t, int, int, cred_t *);
static int sx_cmem_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int sx_cmem_mmap(dev_t, off_t, int);
static int sx_cmem_segmap(dev_t, off_t, struct as *, caddr_t *, off_t,
					u_int, u_int, u_int, cred_t *);
static int sx_cmem_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
						void **result);
static int sx_cmem_identify(dev_info_t *);
static int sx_cmem_attach(dev_info_t *, ddi_attach_cmd_t);
void sx_cmeminit(void);
static int sx_cmem_get_config(caddr_t arg);
static void sx_cmem_cmfree(struct cmresv *cm);
static int sx_cmem_insert(struct cmresv *cm);
static int sx_cmem_cmget(dev_t dev, struct sx_cmem_create *usrcmcr);
static int sx_cmem_vrfy_va(struct sx_cmem_valid_va *cmv);

#ifdef NOTYET
static int sx_cmem_detach(dev_info_t *, ddi_detach_cmd_t);
static int sx_cmem_reset(dev_info_t *, ddi_reset_cmd_t);
#endif NOTYET

extern int nodev();
extern int nulldev();

struct cb_ops   sx_cmem_cb_ops = {
	sx_cmem_open,		/* open */
	sx_cmem_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	sx_cmem_ioctl,		/* ioctl */
	nodev,			/* devmap */
	sx_cmem_mmap,		/* mmap */
	sx_cmem_segmap,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

struct dev_ops  sx_cmem_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	sx_cmem_info,		/* get_dev_info */
	sx_cmem_identify,	/* identify */
	nulldev,		/* probe */
	sx_cmem_attach,		/* attach */
#ifdef NOTYET
	sx_cmem_detach,		/* detach */
	sx_cmem_reset,		/* reset */
#else NOTYET
	nodev,			/* detach */
	nodev,			/* reset */
#endif NOTYET
	&sx_cmem_cb_ops,	/* driver operations */
	(struct bus_ops *)0	/* bus operations */
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Segment driver for physically contiguous DRAM",
	&sx_cmem_ops	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};


_init()
{
	int error = 0;
#if DEBUG
	char version[] = "1.94";
#endif DEBUG

	if ((error = mod_install(&modlinkage)) == 0) {
#if DEBUG
		cmn_err(CE_CONT,
			"?Contiguous Memory driver V%s loaded\n", version);
#endif DEBUG
	}
	return (error);
}


int
_fini()
{
	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}


static int
sx_cmem_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "sx_cmem") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

#define	ONE_MBYTE	0x100000
#define	PMEMPAGE_1MB	(ONE_MBYTE/MMU_PAGESIZE)
#define	PMEMPAGE_64MB	((64 * ONE_MBYTE)/MMU_PAGESIZE)
#define	PGALIGN1	(1 << MMU_STD_FIRSTSHIFT)
#define	PGALIGN2	(1 << MMU_STD_SECONDSHIFT)
#define	ALIGN_LVL1	mmu_ptob(PGALIGN1)
#define	ALIGN_LVL2	mmu_ptob(PGALIGN2)
#define	ALIGN_LVL3	NULL

#define	NO_MBREQ_MSG "?No contiguous memory requested for SX\n"
#define	SMALL_MEM_MSG "?Contiguous memory request exceeds cmem_mbleft limit\n"
#define	CMEM_FAIL_MSG \
	"?Contiguous memory request for %dMB cannot be satisfied\n"
#define	ALLOCERR_MSG "Cannot allocate memory for sx_cmem list\n"

/* ARGSUSED */
static int
sx_cmem_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	register u_int req_pages, attempt_pages;
	u_int p_count;
	struct sx_cmem_list *new, *cur, *prev;
	page_t *pp, *tmp_pp;
	u_int align_setlevel;
	int i;
	extern int sx_ctlr_present;

	if (!sx_ctlr_present)
		return (DDI_FAILURE);

	switch (cmd) {
	    case DDI_ATTACH:
		i = ddi_get_instance(devi);
		sx_cmem_dip = devi;
		if (ddi_create_minor_node(devi, "sx_cmem", S_IFCHR,
		    i, DDI_PSEUDO, NULL) == DDI_FAILURE) {
			cmn_err(CE_WARN,
				"Could not create x_cmem minor node\n");
			goto sx_cmem_fail;
		}

		if (physinstalled == 0) {
			cmn_err(CE_WARN, "No phys_install memory found\n");
			goto sx_cmem_fail;
		}

		/*
		 * read out user request from sx_cmem.conf file,
		 * only if no override from global sx_cmem_mbreq
		 */
		if (sx_cmem_mbreq == NULL) {
			sx_cmem_mbreq = ddi_getprop(DDI_DEV_T_ANY, devi, NULL,
				"cmem_mbreq", 0);
		}
		if (sx_cmem_mbreq == 0) {
			cmn_err(CE_CONT, NO_MBREQ_MSG);
			goto sx_cmem_fail;
		}

		sx_cmem_frag = ddi_getprop(DDI_DEV_T_ANY, devi, NULL,
			"cmem_frag", NULL);

		sx_cmem_mbleft = ddi_getprop(DDI_DEV_T_ANY, devi, NULL,
			"cmem_mbleft", 32) * ONE_MBYTE / MMU_PAGESIZE;

		req_pages = (sx_cmem_mbreq * ONE_MBYTE) / MMU_PAGESIZE;


		attempt_pages = physinstalled - req_pages;

		/* Must have at least sx_cmem_mbleft left afterwards */
		if ((int)attempt_pages < sx_cmem_mbleft) {
			cmn_err(CE_CONT, SMALL_MEM_MSG);
			goto sx_cmem_fail;
		}

		attempt_pages = req_pages;
		while (attempt_pages >= PMEMPAGE_1MB) {

		    align_setlevel = (attempt_pages >= PGALIGN1) ?
			    ALIGN_LVL1 : ALIGN_LVL2;

		    pp = (page_t *)page_get_contig((attempt_pages *
			MMU_PAGESIZE), align_setlevel);

		    if (pp == NULL) {
			if (!sx_cmem_frag)
			    goto sx_cmem_fail;
			/*
			 * It's very likely that we will find a 64MB contiguous
			 * memory chunk on systems configured with 64MB DSIMMs,
			 * so try the size first. If fail, try smaller size.
			 */
			if (attempt_pages > PMEMPAGE_64MB)
			    attempt_pages = PMEMPAGE_64MB;
			else if (attempt_pages > PGALIGN1)
			    attempt_pages = PGALIGN1;
			else {
			    attempt_pages /= 2;
			    attempt_pages -= attempt_pages % PGALIGN2;
			}
		    } else {
			/*
			 * Successfully found a matching size, save the info
			 * (physical address/size) in a linked list structure.
			 */
			new = kmem_zalloc(sizeof (struct sx_cmem_list),
			    KM_SLEEP);

			/*
			 * The pages in the contiguous chunk of memory should
			 * be on a doubly linked circular list. Flush them
			 * from the system MMU caches. Assign them identities.
			 * This should actually be done by the SX driver.
			 */
			tmp_pp = pp;
			for (tmp_pp = pp, p_count = attempt_pages; p_count;
			    tmp_pp = page_next_raw(tmp_pp), p_count--) {
				hat_pagecachectl(tmp_pp, HAT_UNCACHE);
				page_downgrade(tmp_pp);
				tmp_pp->p_vnode = &sx_cmem_vnode;
				tmp_pp->p_offset =
					mmu_ptob(page_pptonum(tmp_pp));
			}

			/*
			 * Save the information of the cmem chunk in
			 * a sx_cmem_list structure. The information that
			 * we are intrested in are the low pp, high pp,
			 * low pfn, high pfn and number of pages.
			 */
			new->scl_lpp = pp;
			new->scl_hpp = pp->p_prev;
			new->scl_lpfn = page_pptonum(pp);
			new->scl_hpfn = page_pptonum(pp->p_prev);
			new->scl_pages = attempt_pages;
			new->scl_next = NULL;

			/*
			 * Insert the new chunk to the sx_cmem_list in
			 * increasing order. sx_cmem_head points to the head.
			 * Merge the adjacent chunks if they are contiguous.
			 */
			if (sx_cmem_head == NULL) {
				sx_cmem_head = new;
			} else {
				prev = cur = sx_cmem_head;
				while (cur && (cur->scl_lpp < new->scl_lpp)) {
					prev = cur;
					cur = cur->scl_next;
				}
				if (cur == sx_cmem_head) {
					/* put it in front, update head */
					new->scl_next = cur;
					sx_cmem_head = new;
				} else {
					/* otherwise, to the middle or end */
					prev->scl_next = new;
					new->scl_next = cur;
				}
				/* Merge with previous chunk if contiguous */
				if ((new != sx_cmem_head) &&
				    (prev->scl_hpfn + 1) == (new->scl_lpfn)) {
					prev->scl_hpp = new->scl_hpp;
					prev->scl_hpfn = new->scl_hpfn;
					prev->scl_pages += new->scl_pages;
					prev->scl_next = new->scl_next;
					kmem_free(new,
						sizeof (struct sx_cmem_list));
					new = prev;
				}
				/* Merge with next chunk if contiguous */
				if ((cur != NULL) &&
				    (new->scl_hpfn + 1) == (cur->scl_lpfn)) {
					new->scl_hpp = cur->scl_hpp;
					new->scl_hpfn = cur->scl_hpfn;
					new->scl_pages += cur->scl_pages;
					new->scl_next = cur->scl_next;
					kmem_free(cur,
						sizeof (struct sx_cmem_list));
				}
			}

			sx_total_cpages += attempt_pages;
			attempt_pages = req_pages - sx_total_cpages;
		    }
		}

		if (sx_total_cpages == NULL) {
			cmn_err(CE_CONT, CMEM_FAIL_MSG, sx_cmem_mbreq);
			goto sx_cmem_fail;
		}

		cmn_err(CE_CONT, "?sx_cmem: Installed %luMB\n",
			ptob(physinstalled) / ONE_MBYTE);
		cmn_err(CE_CONT, "?         Reserved %luMB\n",
			(ptob(sx_total_cpages)/ONE_MBYTE));
		cmn_err(CE_CONT, "?         Fragment %d\n", sx_cmem_frag);
		cmn_err(CE_CONT, "?         Avail For System Use %luMB\n",
			ptob(physinstalled - sx_total_cpages) / ONE_MBYTE);

		sx_cmem_init();
		ddi_report_dev(devi);
		return (DDI_SUCCESS);
sx_cmem_fail:
		ddi_remove_minor_node(devi, NULL);
		/* let it fall down and fail */
	    default:
		return (DDI_FAILURE);
	}
}

#ifdef NOTYET
static int
sx_cmem_reset(dev_info_t *dip, ddi_reset_cmd_t cmd)
{
	/* No mutex_enter()'s are allowed in this entry point */

	switch (cmd) {
	    case DDI_RESET_FORCE:
		cmn_err(CE_CONT,
			"sx_cmem_reset(): do not know what to do yet\n");
		return (DDI_FAILURE);
	    default:
		return (DDI_FAILURE);
	}
}

static int
sx_cmem_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct sx_cmem_list *listp;
	page_t *pp;
	u_int	npages;

	switch (cmd) {
	case DDI_DETACH:
		cmn_err(CE_CONT, "sx_cmem_detach(): can not be detached\n");
		return (DDI_FAILURE);
	default:
		return (DDI_FAILURE);
	}
}
#endif NOTYET

/*ARGSUSED*/
static int
sx_cmem_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)sx_cmem_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t)arg);
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*ARGSUSED*/
int
sx_cmem_open(dev_t *devp, int flag, int otype, cred_t *cred)
{
	struct cloneblk *cb, **clp;
	int i, base;

	dev_t dev = *devp;
	/*
	 * We use the minor to tell which clone we are, so only 0 can
	 * be "opened."
	 */
	if (getminor(dev) != 0)
		return (ENODEV);

	/*
	 * If there was no contiguous DRAM reserved we should fail.
	 */
	if (sx_cmem_head == NULL)
		return (ENOMEM);

	/*
	 * Grab the mutex for driver private data access
	 */
	mutex_enter(&sx_cmem_mutex);
	/*
	 * find a unique minor for this open.
	 */
	for (clp = &cmem_clones, cb = *clp, base = 0; cb;
	    clp = &cb->clb_next, cb = *clp, base += CLONEBLKSZ) {
		for (i = 0; i < CLONEBLKSZ; i++) {
			if (cb->clb_use[i] == 0) {
				cb->clb_use[i] = 1;
				*devp = makedevice(getmajor(dev), i + base);
				mutex_exit(&sx_cmem_mutex);
				return (0);
			}
		}
	}
	/*
	 * No free ones, allocate new block.
	 */
	*clp = cb = (struct cloneblk *)
		kmem_zalloc(sizeof (struct cloneblk), KM_SLEEP);
	cb->clb_use[0] = 1;
	*devp = makedevice(getmajor(dev), base);
	mutex_exit(&sx_cmem_mutex);
	return (0);
}

/*ARGSUSED*/
int
sx_cmem_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	struct cloneblk *cb;
	struct cmresv *cm, *ncm;
	int i;

	mutex_enter(&sx_cmem_mutex);
	for (i = getminor(dev), cb = cmem_clones; i >= CLONEBLKSZ;
	    i -= CLONEBLKSZ, cb = cb->clb_next) {
		if (cb == NULL) {
			mutex_exit(&sx_cmem_mutex);
			cmn_err(CE_PANIC, "sx_cmem_close");
		}
	}

	/*
	 * free up any reservations we made
	 */
	cm = cb->clb_rlist[i];
	while (cm) {
		ncm = cm->cmr_next;
		if (--cm->cmr_refcnt == 0)
			sx_cmem_cmfree(cm);
		else {
			cm->cmr_next = NULL;
			if (cm->cmr_proc)
				cm->cmr_proc = (void *)-1l;
		}
		cm = ncm;
	}
	cb->clb_rlist[i] = NULL;
	cb->clb_use[i] = 0;
	mutex_exit(&sx_cmem_mutex);
	return (0);
}

/*
 * vmem walker function to implement the RMAPINFO ioctl, which provides
 * a zero-terminated list of free cmem segment sizes to the user.
 */
/* ARGSUSED */
static void
sx_cmem_walk(void *arg, void *base, size_t size)
{
	uint32_t **ipp = arg;

	if (ipp[0] != NULL && suword32(ipp[0]++, ptob(size)) != 0)
		ipp[0] = NULL;
}

/*ARGSUSED*/
int
sx_cmem_ioctl(dev_t dev, int cmd, intptr_t arg,
	int mode, cred_t *cred, int *rvalp)
{
	int error = 0;

	mutex_enter(&sx_cmem_mutex);
	switch (cmd) {

	case SX_CMEM_CREATE:
		error = sx_cmem_cmget(dev, (struct sx_cmem_create *)arg);
		break;

	case SX_CMEM_VALID_VA:
		error = sx_cmem_vrfy_va((struct sx_cmem_valid_va *)arg);
		break;

	case SX_CMEM_GET_RMAPINFO:
		vmem_walk(sx_cmem_arena, VMEM_FREE, sx_cmem_walk, &arg);
		error = (arg == NULL || suword32((void *)arg, 0)) ? EFAULT : 0;
		break;

	case SX_CMEM_GET_CONFIG:
		error = sx_cmem_get_config((caddr_t)arg);
		break;

	default:
		error = ENOTTY;
	}
	mutex_exit(&sx_cmem_mutex);
	return (error);
}

/*ARGSUSED*/
static int
sx_cmem_mmap(dev_t dev, off_t off, int prot)
{
	return (0);
}

static int
sx_cmem_segmap(dev_t dev, off_t off, struct as *as, caddr_t *addr,
    off_t len, u_int prot, u_int maxprot, u_int flags, cred_t *cred)
{
	struct segsx_cmem_crargs a;
	struct segsx_cmem_data *sscd;
	int error;
	struct seg *seg;
	page_t  *pp;
	u_int pf;

#if defined(lint)
	maxprot = maxprot;
	cred = cred;
#endif
	if ((flags & MAP_TYPE) != MAP_SHARED)
		return (EINVAL);

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		/*
		 * Pick an address
		 */
		map_addr(addr, len, (offset_t)0, 0, flags);
		if (*addr == NULL)
			return (ENOMEM);
	} else {
		/*
		 * User specified address -
		 * Blow away any previous mappings.
		 */
		(void) as_unmap(as, *addr, len);
	}

	a.offset = (u_int)off;
	a.prot = prot;
	a.dev = dev;

	error = as_map(as, *addr, len, segsx_cmem_create, &a);

	if (error == 0) {
		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
		seg = as_segat(as, *addr);

		if (seg == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}

		sscd = (struct segsx_cmem_data *)seg->s_data;
		pf = (sscd->cm->cmr_pfbase) + btop(*addr - seg->s_base);
		pp = page_numtopp_nolock(pf);

		ASSERT(pp != NULL);
		ASSERT(PP_ISNC(pp));

		ohat_contig_memload(as->a_hat, as, *addr, pp, prot,
			HAT_LOAD, len);
		AS_LOCK_EXIT(as, &as->a_lock);
	}

	as_rangeunlock(as);

	return (error);
}

/*
 * segment driver routines
 */

static	int segsx_cmem_dup(struct seg *seg, struct seg *newseg);
static	int segsx_cmem_unmap(struct seg *seg, caddr_t addr, size_t len);
static	void segsx_cmem_free(struct seg *seg);
static	faultcode_t segsx_cmem_fault(struct hat *hat, struct seg *seg,
			caddr_t addr, size_t len, enum fault_type type,
			enum seg_rw rw);
static	faultcode_t segsx_cmem_faulta(struct seg *seg, caddr_t addr);
static	int segsx_cmem_setprot(struct seg *seg, caddr_t addr, size_t len,
		u_int prot);
static	int segsx_cmem_checkprot(struct seg *seg, caddr_t addr, size_t len,
		u_int prot);
static	int segsx_cmem_kluster(struct seg *seg, caddr_t addr, ssize_t delta);
static	size_t segsx_cmem_swapout(struct seg *seg);
static	int segsx_cmem_sync(struct seg *seg, caddr_t addr, size_t len,
		int attr, u_int flags);
static	size_t segsx_cmem_incore(struct seg *seg, caddr_t addr, size_t len,
		char *vec);
static	int segsx_cmem_lockop(struct seg *seg, caddr_t addr, size_t len,
		int attr, int op, ulong *lockmap, size_t pos);
static	int segsx_cmem_getprot(struct seg *seg, caddr_t addr, size_t len,
		u_int *protv);
static	u_offset_t segsx_cmem_getoffset(struct seg *seg, caddr_t addr);
static  int segsx_cmem_gettype(struct seg *seg, caddr_t addr);
static	int segsx_cmem_getvp(struct seg *seg, caddr_t addr,
		struct vnode **vpp);
static	int segsx_cmem_advise(struct seg *seg, caddr_t addr, size_t len,
		u_int behav);
static	void segsx_cmem_dump(struct seg *seg);
static	int segsx_cmem_pagelock(struct seg *seg, caddr_t addr, size_t len,
		struct page ***ppp, enum lock_type type, enum seg_rw rw);
static	int segsx_cmem_getmemid(struct seg *seg, caddr_t addr,
			memid_t *memidp);

struct	seg_ops segsx_cmem_ops =  {

	segsx_cmem_dup,		/* dup */
	segsx_cmem_unmap,		/* unmap */
	segsx_cmem_free,		/* free */
	segsx_cmem_fault,		/* fault */
	segsx_cmem_faulta,		/* asynchronous faults */
	segsx_cmem_setprot,		/* setprot */
	segsx_cmem_checkprot,		/* checkprot */
	segsx_cmem_kluster,		/* kluster */
	segsx_cmem_swapout,		/* swapout */
	segsx_cmem_sync,		/* sync */
	segsx_cmem_incore,		/* incore */
	segsx_cmem_lockop,		/* lock_op */
	segsx_cmem_getprot,		/* get protoections */
	segsx_cmem_getoffset,		/* get offset into device */
	segsx_cmem_gettype,		/* get mapping type */
	segsx_cmem_getvp,		/* get vnode for this segment */
	segsx_cmem_advise,		/* advise */
	segsx_cmem_dump,		/* dump */
	segsx_cmem_pagelock,		/* pagelock */
	segsx_cmem_getmemid,		/* getmemid */
};

/*
 * Initialize segment segops and private data fields. The segment provides a
 * mapping to a contiguous chunk of memory.
 */

static int
segsx_cmem_create(seg, argsp)
	struct seg *seg;
	caddr_t argsp;
{
	struct segsx_cmem_crargs *a;
	struct cmresv *cm;
	struct segsx_cmem_data *sscd;
	int error = 0;
	extern struct vnode *specfind(), *common_specvp();

	a = (struct segsx_cmem_crargs *)argsp;
	mutex_enter(&sx_cmem_mutex);
	cm = sx_cmem_cmfind(a->offset);
	if (cm == NULL) {
		mutex_exit(&sx_cmem_mutex);
		return (EINVAL);
	}
	if (cm->cmr_proc && cm->cmr_proc != curproc) {
		mutex_exit(&sx_cmem_mutex);
		return (EACCES);
	}
	if (a->offset + seg->s_size > cm->cmr_offset + cm->cmr_size) {
		mutex_exit(&sx_cmem_mutex);
		return (EINVAL);
	}
	cm->cmr_refcnt++;
	sscd = (struct segsx_cmem_data *)
		kmem_zalloc(sizeof (struct segsx_cmem_data), KM_SLEEP);
	sscd->cm = cm;
	sscd->offset = a->offset;
	sscd->dev = a->dev;
	sscd->maxprot = PROT_READ | PROT_WRITE | PROT_USER;
	sscd->prot = a->prot;

	/*
	 *  Find the shadow vnode.  It will be held by specfind().
	 *  Holding the shadow causes the common to be held.
	 */
	sscd->vp = specfind(a->dev, VCHR);
	ASSERT(sscd->vp != NULL);

	seg->s_ops = &segsx_cmem_ops;
	seg->s_data = (caddr_t)sscd;

	error = VOP_ADDMAP(common_specvp(sscd->vp), (offset_t)sscd->offset,
			seg->s_as, seg->s_base, seg->s_size, sscd->prot,
			sscd->maxprot, MAP_SHARED, CRED());

	mutex_exit(&sx_cmem_mutex);

	return (error);
}

/*
 * Duplicate seg and return new segment in newsegp.
 */

static int
segsx_cmem_dup(struct seg *seg, struct seg *newseg)
{
	struct segsx_cmem_data *sscd, *nsscd;
	struct cmresv *cm;
	extern struct vnode *specfind(), *common_specvp();

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	mutex_enter(&sx_cmem_mutex);
	sscd = (struct segsx_cmem_data *)seg->s_data;
	cm = sscd->cm;
	cm->cmr_refcnt++;
	nsscd = (struct segsx_cmem_data *)
		kmem_zalloc(sizeof (struct segsx_cmem_data), KM_SLEEP);
	nsscd->cm =  sscd->cm;
	nsscd->offset =  sscd->offset;
	nsscd->prot = sscd->prot;
	nsscd->maxprot = sscd->maxprot;

	nsscd->vp = specfind(sscd->dev, VCHR);
	ASSERT(nsscd->vp != NULL);

	newseg->s_ops = &segsx_cmem_ops;
	newseg->s_data = (caddr_t)nsscd;
	mutex_exit(&sx_cmem_mutex);

	return (VOP_ADDMAP(common_specvp(nsscd->vp), (offset_t)nsscd->offset,
		newseg->s_as, newseg->s_base, newseg->s_size, nsscd->prot,
		nsscd->maxprot, MAP_SHARED, CRED()));
}

static int
segsx_cmem_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	struct seg *nseg;
	struct cmresv *cm;
	struct segsx_cmem_data *sscd, *nsscd;
	caddr_t nbase;
	u_int nsize;
	extern struct vnode *common_specvp();

	/*
	 * Enforce policy of disallowing parital unmaps until
	 * page demotions are supported
	 */
	if ((addr != seg->s_base) || (len != seg->s_size))
		return (EACCES);
	sscd = (struct segsx_cmem_data *)seg->s_data;
	/*
	 * Since the address space is write locked before this routine is
	 * called we don't need to acquire the lock for segment data
	 * manipulation
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Check for bad sizes.
	 */
	if (addr < seg->s_base || addr + len > seg->s_base + seg->s_size ||
	    (len & PAGEOFFSET) || ((u_int)addr & PAGEOFFSET))
		cmn_err(CE_PANIC, "segsx_cmem_unmap");

	/*
	 *  Inform the vnode of the unmapping.
	 */
	ASSERT(sscd->vp != NULL);
	VOP_DELMAP(common_specvp(sscd->vp), (offset_t)sscd->offset +
		(addr - seg->s_base), seg->s_as, addr, len,
		sscd->prot, sscd->maxprot, MAP_SHARED, CRED());

	/*
	 * Unload any hardware translations in the range to be taken out.
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);

	/*
	 * Check for entire segment
	 */
	if (addr == seg->s_base && len == seg->s_size) {
		seg_free(seg);
		return (0);
	}

	/*
	 * Check for beginning of segment
	 */
	if (addr == seg->s_base) {
		sscd->offset += len;
		seg->s_base += len;
		seg->s_size -= len;
		return (0);
	}

	/*
	 * Check for end of segment
	 */
	if (addr + len == seg->s_base + seg->s_size) {
		seg->s_size -= len;
		return (0);
	}
	nbase = addr + len;
	nsize = (seg->s_base + seg->s_size) - nbase;
	seg->s_size = addr - seg->s_base;
	nseg = seg_alloc(seg->s_as, nbase, nsize);
	if (nseg == NULL)
		cmn_err(CE_PANIC, "segsx_cmem_unmap seg_alloc");
	mutex_enter(&sx_cmem_mutex);
	cm = sscd->cm;
	cm->cmr_refcnt++;
	nsscd = (struct segsx_cmem_data *)
		kmem_zalloc(sizeof (struct segsx_cmem_data), KM_SLEEP);
	nsscd->cm = cm;
	nsscd->offset = sscd->offset + nseg->s_base - seg->s_base;
	nsscd->prot = sscd->prot;
	nsscd->maxprot = sscd->maxprot;
	nseg->s_ops = &segsx_cmem_ops;
	nseg->s_data = (caddr_t)nsscd;
	mutex_exit(&sx_cmem_mutex);
	return (0);
}

static void
segsx_cmem_free(struct seg *seg)
{
	struct segsx_cmem_data *sscd;
	struct cmresv *cm;

	sscd = (struct segsx_cmem_data *)seg->s_data;

	/*
	 *  Since the address space is write locked before this routine is
	 *  called we don't need to acquire the lock for segment data
	 *  manipulation.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	mutex_enter(&sx_cmem_mutex);
	VN_RELE(sscd->vp);	/* release the shadow */

	cm = sscd->cm;
	if (--cm->cmr_refcnt == 0)
		sx_cmem_cmfree(cm);

	kmem_free((caddr_t)sscd, sizeof (struct segsx_cmem_data));
	mutex_exit(&sx_cmem_mutex);
}

/*
 * Handle a fault for non-existent translations to physically contiguous DRAM
 */

/*ARGSUSED*/
static faultcode_t
segsx_cmem_fault(struct hat *hat, struct seg *seg, caddr_t addr, size_t len,
	enum fault_type type, enum seg_rw rw)
{
	struct segsx_cmem_data *sscd;
	struct cmresv *cm;
	page_t *pp;
	u_int  prot, pf;

	if (type != F_INVAL && type != F_SOFTLOCK && type != F_SOFTUNLOCK)
		return (FC_MAKE_ERR(EFAULT));

	/*
	 * The SOFTLOCK and SOFTUNLOCK operations are applied to the
	 * entire address range mapped by this segment because we assume
	 * that translations for cmem are set up using large page sizes
	 * of the SRMMU and that the SRMMU level 2 tables are always
	 * locked.  This assumption is OK until full support for multiple
	 * page sizes is available in the VM/MMU layers of SunOS.
	 */

	if (type == F_SOFTUNLOCK) {
		hat_unlock(hat, seg->s_base, seg->s_size);
		return (NULL);
	}

	switch (rw) {
	case S_READ:
		prot = PROT_READ;
		break;

	case S_WRITE:
		prot = PROT_WRITE;
		break;

	case S_EXEC:
		prot = PROT_EXEC;
		break;

	case S_OTHER:
	default:
		prot = PROT_READ | PROT_WRITE;
		break;
	}

	sscd = (struct segsx_cmem_data *)seg->s_data;

	if ((sscd->prot & prot) == 0) {
		return (FC_MAKE_ERR(EACCES));
	}
	cm = sscd->cm;
	pf = cm->cmr_pfbase;
	pp = page_numtopp_nolock(pf);

	ASSERT(pp != NULL);
	ASSERT(PP_ISNC(pp));

	ohat_contig_memload(hat, seg->s_as, seg->s_base, pp, sscd->prot,
	    HAT_LOAD | ((type == F_SOFTLOCK) ? HAT_LOAD_LOCK : 0), seg->s_size);

	return (NULL);
}

/*
 * asynchronous fault is a no op. Fail silently.
 */

/*ARGSUSED*/
static faultcode_t
segsx_cmem_faulta(struct seg *seg, caddr_t addr)
{
	return (0);
}

/*ARGSUSED*/
static int
segsx_cmem_setprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	/*
	 * Since we load mappings to physically contiguous memory
	 * using SRMMU level1 and leve2 pages, we do not
	 * allow changing the protection until we implement handling
	 * page demotions in the SRMMU driver.
	 */
	return (EACCES);
}


/*ARGSUSED*/
static int
segsx_cmem_checkprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	struct segsx_cmem_data *sscd;
	int error;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	/*
	 * Since we only use segment level protection, simply check against
	 * them.
	 */
	sscd = (struct segsx_cmem_data *)seg->s_data;
	error = ((sscd->prot & prot) != prot) ? -1 : 0;
	return (error);

}

/*ARGSUSED*/
static int
segsx_cmem_kluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	return (-1);
}

/*ARGSUSED*/
static size_t
segsx_cmem_swapout(struct seg *seg)
{
	return (0);
}

/*ARGSUSED*/
static int
segsx_cmem_sync(struct seg *seg, caddr_t addr, size_t len, int attr,
    u_int flags)
{
	return (0);
}

/*
 * Pre-allocated physically contiguous memory pages are always in core.
 */

/*ARGSUSED*/
static size_t
segsx_cmem_incore(struct seg *seg, caddr_t addr, size_t len, char *vec)
{
	u_int v = 0;

	for (len = roundup(len, PAGESIZE); len; len -= PAGESIZE, v += PAGESIZE)
		*vec++ = 1;
	return (v);
}

/*ARGSUSED*/
static int
segsx_cmem_lockop(struct seg *seg, caddr_t addr, size_t len, int attr,
	int op, ulong *lockmap, size_t pos)
{
	return (0);
}

/*
 * Return protections for the specified range. This segment driver does not
 * implement page-level protections. The same protection is used for the
 * entire range of virtual address mapped by this segment.
 */

static	int
segsx_cmem_getprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	struct segsx_cmem_data *sdp = (struct segsx_cmem_data *)seg->s_data;
	u_int pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	if (pgno != 0) {
		do
			protv[--pgno] = sdp->prot;
		while (pgno != 0);
	}
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segsx_cmem_getoffset(struct seg *seg, caddr_t addr)
{

	return ((u_offset_t)0);
}

/*ARGSUSED*/
static int
segsx_cmem_gettype(struct seg *seg, caddr_t addr)
{
	return (MAP_SHARED);
}

/*ARGSUSED*/
static int
segsx_cmem_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	struct segsx_cmem_data *sscd = (struct segsx_cmem_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	*vpp = sscd->vp;	/* return the shadowvp */

	return (0);
}

/*ARGSUSED*/
static int
segsx_cmem_advise(struct seg *seg, caddr_t addr, size_t len, u_int behav)
{
	return (0);
}

/*ARGSUSED*/
static void
segsx_cmem_dump(struct seg *seg)
{
}

/*ARGSUSED*/
static int
segsx_cmem_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*ARGSUSED*/
static int
segsx_cmem_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	return (ENODEV);
}

/*
 * Driver internal routines
 */
void
sx_cmem_init()
{
	struct sx_cmem_list *listp = sx_cmem_head;

	ASSERT(listp && listp->scl_pages);

	/*
	 * First open of the device. Free up each chunk of physically
	 * contiguous DRAM allocated by the PROM into a resource map.
	 */
	sx_cmem_arena = vmem_create("sx_cmem", NULL, 0, 1, NULL, NULL, NULL, 0,
	    VM_SLEEP);
	for (; listp; listp = listp->scl_next)
		(void) vmem_add(sx_cmem_arena, (void *)listp->scl_lpfn,
		    listp->scl_pages, VM_SLEEP);

	/*
	 * Initialize the mutex for driver private data.
	 */
	mutex_init(&sx_cmem_mutex, NULL, MUTEX_DRIVER, NULL);
}

/*
 * Supporting routines.
 */

/*
 * create a reservation
 */
static int
sx_cmem_cmget(dev_t dev, struct sx_cmem_create *usrcmcr)
{
	int i, a, error;
	struct cloneblk *cb;
	struct cmresv *cm;
	struct sx_cmem_create cmcr;

	ASSERT(MUTEX_HELD(&sx_cmem_mutex));

	if (copyin(usrcmcr, &cmcr, sizeof (*usrcmcr)) != 0)
		return (EFAULT);

	if (cmcr.scm_size == 0 || cmcr.scm_size > CMTBLSZ * CMSEGSZ ||
	    cmcr.scm_size & PAGEOFFSET)
		return (EINVAL);

	/*
	 * allocate a chunk from cmem arena
	 */
	if (sx_cmem_arena == NULL)
		return (ENOMEM);
	a = (int)vmem_alloc(sx_cmem_arena, btop(cmcr.scm_size), VM_SLEEP);

	if (a == 0)
		return (ENOMEM);

	/*
	 * insert reservation on both the rlist of this open, and into
	 * the cmem_table (in sx_cmem_insert)
	 */
	for (i = getminor(dev), cb = cmem_clones; i >= CLONEBLKSZ;
	    i -= CLONEBLKSZ, cb = cb->clb_next)
		if (cb == NULL)
			cmn_err(CE_PANIC, "sx_cmem_cmget");
	cm = kmem_zalloc(sizeof (struct cmresv), KM_SLEEP);
	cm->cmr_size = cmcr.scm_size;
	error = sx_cmem_insert(cm);
	if (error) {
		kmem_free(cm, sizeof (struct cmresv));
		vmem_free(sx_cmem_arena, (void *)a, btop(cmcr.scm_size));
		return (error);
	}
	cm->cmr_refcnt = 1;
	cm->cmr_pfbase = a;
	if (cmcr.scm_type == SX_CMEM_PRIVATE)
		cm->cmr_proc = curproc;
	else
		cm->cmr_proc = NULL;

	cm->cmr_next = cb->clb_rlist[i];
	cb->clb_rlist[i] = cm;
	cmcr.scm_offset = (off_t)cm->cmr_offset;
	if (copyout(&cmcr, usrcmcr, sizeof (*usrcmcr)) != 0)
		return (EFAULT);
	return (0);
}

/*
 * Insert cm into cmem_table
 */
static int
sx_cmem_insert(struct cmresv *cm)
{
	int nsg, i, j, k;
	struct cmresv **cmbase;

	ASSERT(MUTEX_HELD(&sx_cmem_mutex));
	/*
	 * cmem_table is a 2 level lookup table.
	 * It consists of CMTBLSZ pointers to secondary tables which
	 * contain CMTBLSZ pointers to cmresv structs.  Each slot in
	 * the secondary table represents an area of CMSEGSZ, so we
	 * need to find 'nsg' free areas.  cmem_tblcnt contains the
	 * number of valid pointers in the secondary table.
	 */
	nsg = roundup(cm->cmr_size, CMSEGSZ) / CMSEGSZ;
	for (i = 0; i < CMTBLSZ; i++) {
		if (cmem_tblcnt[i] <= CMTBLSZ - nsg) {
			if (cmem_table[i] == NULL) {
				cmem_table[i] = (struct cmresv **)
					kmem_zalloc(CMTBLSZ *
					    sizeof (struct cmresv *), KM_SLEEP);
			}
			cmbase = cmem_table[i];
			for (j = 0; j <= CMTBLSZ - nsg; j++) {
				for (k = 0; k < nsg; k++)
					if (cmbase[j+k])
						break;
				if (k == nsg)
					goto gotit;
			}
		}
	}
	return (ENOMEM);
gotit:
	/*
	 * i is the index into the primary table
	 * j is the index into the secondary table
	 */
	cm->cmr_offset = CMEM_MKOFFSET(i, j);
	cmem_tblcnt[i] += nsg;
	for (k = 0; k < nsg; k++)
		cmbase[j+k] = cm;
	return (0);
}

static struct cmresv *
sx_cmem_cmfind(off)
	u_int off;
{
	int l1, l2;

	ASSERT(MUTEX_HELD(&sx_cmem_mutex));
	off /= CMSEGSZ;
	l1 = off / CMTBLSZ;
	l2 = off % CMTBLSZ;
	if (cmem_table[l1] == NULL)
		return (NULL);
	return (cmem_table[l1][l2]);
}

static void
sx_cmem_cmfree(struct cmresv *cm)
{
	int l1, l2, nsg;
	u_int off;

	ASSERT(MUTEX_HELD(&sx_cmem_mutex));

	off = cm->cmr_offset / CMSEGSZ;
	l1 = off / CMTBLSZ;
	l2 = off % CMTBLSZ;
	nsg = roundup(cm->cmr_size, CMSEGSZ) / CMSEGSZ;
	cmem_tblcnt[l1] -= nsg;
	while (nsg--)
		cmem_table[l1][l2+nsg] = NULL;
	vmem_free(sx_cmem_arena, (void *)cm->cmr_pfbase, btop(cm->cmr_size));
	kmem_free((caddr_t)cm, sizeof (struct cmresv));
}

static int
sx_cmem_vrfy_va(struct sx_cmem_valid_va *cmv)
{
	struct as *as;
	struct seg *seg;
	caddr_t saddr, eaddr, segbase, segend;

	ASSERT(MUTEX_HELD(&sx_cmem_mutex));

	as = curproc->p_as;
	saddr = cmv->scm_vaddr;
	eaddr = cmv->scm_vaddr + cmv->scm_len;
	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		if (seg->s_ops != &segsx_cmem_ops)
			continue;
		segbase = seg->s_base;
		segend = seg->s_base + seg->s_size;
		if ((segbase >= saddr && segbase < eaddr) ||
		    (segend > saddr && segend <= eaddr)) {
			cmv->scm_base_vaddr = MAX(saddr, segbase);
			cmv->scm_base_len = MIN(eaddr, segend) -
				cmv->scm_base_vaddr;
			return (0);
		}
		if ((saddr >= segbase && saddr < segend) ||
		    (eaddr > segbase && eaddr <= segend)) {
			cmv->scm_base_vaddr = MAX(saddr, segbase);
			cmv->scm_base_len = MIN(eaddr, segend) -
				cmv->scm_base_vaddr;
			return (0);
		}
	}
	cmv->scm_base_vaddr = 0;
	cmv->scm_base_len = 0;
	return (0);
}

static int
sx_cmem_get_config(caddr_t arg)
{
	struct sx_cmem_config *cmem_config;
	struct sx_cmem_list *chunkp;
	int i;

	ASSERT(MUTEX_HELD(&sx_cmem_mutex));
	cmem_config = kmem_zalloc(sizeof (struct sx_cmem_config), KM_SLEEP);
	cmem_config->scm_meminstalled = physinstalled / PMEMPAGE_1MB;
	cmem_config->scm_cmem_mbreq = sx_cmem_mbreq;
	cmem_config->scm_cmem_mbrsv = sx_total_cpages / PMEMPAGE_1MB;
	cmem_config->scm_cmem_mbleftreq = sx_cmem_mbleft / PMEMPAGE_1MB;
	cmem_config->scm_cmem_mbleft =
		(physinstalled - sx_total_cpages) / PMEMPAGE_1MB;
	cmem_config->scm_cmem_frag = sx_cmem_frag;
	for (i = 0, chunkp = sx_cmem_head;
		(chunkp != NULL) && (i < SX_CMEM_CHNK_NUM);
		i++, chunkp = chunkp->scl_next) {
			(cmem_config->scm_cmem_chunks)++;
			cmem_config->scm_cmem_chunksz[i] =
				chunkp->scl_pages / PMEMPAGE_1MB;
	}

	if (copyout((caddr_t)cmem_config, (caddr_t)arg,
		sizeof (struct sx_cmem_config)) != 0)
		return (EFAULT);

	kmem_free(cmem_config, sizeof (struct sx_cmem_config));
	return (0);
}
