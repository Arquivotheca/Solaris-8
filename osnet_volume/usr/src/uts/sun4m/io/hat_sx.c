/*
 * Copyright (c) 1992 Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)hat_sx.c 1.55	97/06/02 SMI"

/*
 * VM - Hardware Address Translation layer for SX.
 *
 * This file implements a psuedo HAT layer for SX graphics accelerator
 * on Sparcstation 20 platforms. The bulk of the work is done by the SRMMU
 * HAT. This HAT is mainly used as a front end to enforce cache consistency
 * (because all mappings to a page must be made non-cacheable for SX
 * accesses) and to have notification when the page daemon tries to unload a
 * page that SX is rendering upon.
 */

/*
 * Locking strategy is simple:
 *
 *	a) sxmmu_hment_lock: a global mutex to protect global data
 *
 *	b) A per SX HAT (per address space) lock
 */

#include <sys/machparam.h>
#include <sys/mman.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/signal.h>
#include <sys/user.h>	/* for u_ru.ru_minflt */
#include <sys/vtrace.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>

#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/devaddr.h>
#include <sys/kmem.h>
#include <sys/var.h>
#include <sys/sysmacros.h>

#include <vm/hat.h>
#include <vm/mhat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/seg_vn.h>
#include <vm/vpage.h>
#include <vm/seg_kp.h>
#include <vm/rm.h>
#include <sys/hat_sx.h>
#include <sys/sxreg.h>
#include <sys/sxio.h>

/* We allocate space for this ptr here, but init it in the SX driver.  Ick. */
volatile struct sx_register_address *sxregsp;

extern struct hatops *sys_hatops;	/* System MMU HAT operations vector */
extern struct hatsw hattab[];
extern int sx_vrfy_pfn(u_int, u_int);

/*
 * Protects free status of an sx_hmentblk.
 */

static kmutex_t	sxmmu_hment_lock;
static u_int nsxhments;
struct sx_hmentblk *sx_hmentblktab;
struct hment *sx_hmetab;

struct sx_hmentblk *sx_allocate_hmentblk(void);
void sx_unlink_hmentblk(struct sx_hmentblk *);
void sx_link_hmentblk(struct sx_hmentblk *, struct sx_hmentblk *);
void sx_mmu_add_vaddr(struct as *, caddr_t, caddr_t,  u_int);
void sx_mmu_del_vaddr(struct as *, caddr_t, caddr_t, u_int, u_int);
static struct sx_addr_memlist
	*sx_mmu_addr_in_origmemlist(struct sx_addr_memlist *, caddr_t, u_int);
static struct sx_addr_memlist
	*sx_mmu_addr_in_sxmemlist(struct sx_addr_memlist *, caddr_t, u_int);
void (*sx_mmu_unmap_callback)(struct as *, caddr_t, u_int);

/*
 * The SX hat operations vector
 */
static	void	sx_mmu_init();
static	void	sx_mmu_alloc(struct hat *, struct as *);
static	struct as *sx_mmu_setup(struct as *, int);
static	void	sx_mmu_free(struct hat *, struct as *);
static	int	sx_mmu_dup(struct hat *, struct as *, struct as *);
static	void	sx_mmu_swapin(struct hat *, struct as *);
static	void	sx_mmu_swapout(struct hat *, struct as *);
static	void 	sx_mmu_memload(struct hat *, struct as *, caddr_t,
				struct page *, u_int, int);
static	void 	sx_mmu_devload(struct hat *, struct as *, caddr_t,
				struct devpage *, u_int, u_int, int);
static	void 	sx_mmu_unlock(struct hat *, struct as *, caddr_t, u_int);
static	faultcode_t sx_mmu_fault(struct hat *, caddr_t);
static	void 	sx_mmu_chgprot(struct as *, caddr_t, u_int, u_int);
static	void 	sx_mmu_unload(struct as *, caddr_t, u_int, int);
static	void 	sx_mmu_sync(struct as *, caddr_t, u_int, u_int);
static	void 	sx_mmu_pageunload(struct page *, struct hment *);
static	int 	sx_mmu_pagesync(struct hat *, struct page *,
			struct hment *, u_int);
static	void 	sx_mmu_pagecachectl(struct page *, u_int);
static	u_int 	sx_mmu_getpfnum(struct as *, caddr_t);
static	int 	sx_mmu_map(struct hat *, struct as *, caddr_t, u_int, int);
static	void	sx_mmu_contig_devload(struct hat *, struct as *, caddr_t,
				devpage_t *, u_int, u_int, int, u_int);
static	void	sx_mmu_contig_memload(struct hat *, struct as *, caddr_t,
				struct page *, u_int, int, u_int);
static	int 	sx_mmu_probe(struct hat *, struct as *, caddr_t);
static	void	sx_mmu_lockinit();
static	int	sx_mmu_share(struct as *, caddr_t, struct as *,
			caddr_t, u_int);
static	void	sx_mmu_unshare(struct as *, caddr_t, u_int);
static	void	sx_killproc(void);
static	faultcode_t sx_mmu_softlock(struct hat *, caddr_t, size_t *,
			struct page **, u_int);
static	faultcode_t sx_mmu_pageflip(struct hat *, caddr_t, caddr_t, size_t *,
			struct page **, struct page **);

static	u_int	sx_mmu_getattr(struct as *, caddr_t, u_int *);
static	void	sx_mmu_do_attr(struct as *, caddr_t, size_t, u_int, int);

struct hatops sx_mmu_hatops = {
	sx_mmu_init,
	sx_mmu_alloc,
	sx_mmu_setup,
	sx_mmu_free,
	sx_mmu_dup,
	sx_mmu_swapin,
	sx_mmu_swapout,
	sx_mmu_memload,
	sx_mmu_devload,
	sx_mmu_contig_memload,
	sx_mmu_contig_devload,
	sx_mmu_unlock,
	sx_mmu_fault,
	sx_mmu_chgprot,
	sx_mmu_unload,
	sx_mmu_sync,
	sx_mmu_pageunload,
	sx_mmu_pagesync,
	sx_mmu_pagecachectl,
	sx_mmu_getpfnum,
	sx_mmu_map,
	sx_mmu_probe,
	sx_mmu_lockinit,
	sx_mmu_share,
	sx_mmu_unshare,
	sx_mmu_do_attr,
	sx_mmu_getattr,
	sx_mmu_softlock,
	sx_mmu_pageflip
};

/*
 *  Initialize locking for the SX HAT layer, called early during boot
 */
static void
sx_mmu_lockinit(void)
{
	mutex_init(&sxmmu_hment_lock, NULL, MUTEX_DEFAULT, NULL);
}

/*
 *  Called just once, at boot time.
 */
static void
sx_mmu_init(void)
{
	extern int sx_ctlr_present;
	struct sx_hmentblk *block;
	int i;
	int npts_needed;

	/*
	 * Do not allocate resources if we are not running on a machine
	 * with the SX acclerator.
	 */

	if (!sx_ctlr_present)
		return;

	/*
	 *  Allocate our global pool of hme's.
	 *
	 * If the value is non-zero assume it was set via /etc/system
	 * and don't disturb it.
	 *
	 *  There are enough pt's to map memory 16 times over,
	 *  but we only want enough spam_hments to map it 4 times over.
	 *  XXX- We need to implement SX HME stealing.
	 */
	npts_needed = (physmem * 4)/NL3PTEPERPT;

	mutex_enter(&sxmmu_hment_lock);
	if (nsxhments == 0)
		nsxhments = (npts_needed * NL3PTEPERPT) / 4;

	if ((sx_hmetab = (struct hment *)kmem_zalloc(sizeof (struct hment) *
		nsxhments, KM_NOSLEEP))
				== (struct hment *)NULL) {

		mutex_exit(&sxmmu_hment_lock);
		cmn_err(CE_PANIC, "Could not allocate sx_hmetab.\n");
	}

	/*
	 * Each group of SX_HMENT_BLKSIZE hments is controlled by
	 * an sx_hmentblk
	 */
	sx_hmentblktab = (struct sx_hmentblk *)kmem_zalloc(
		sizeof (struct sx_hmentblk) *
		(nsxhments / SX_HMENT_BLKSIZE), KM_NOSLEEP);

	if (sx_hmentblktab == (struct sx_hmentblk *)NULL) {
		mutex_exit(&sxmmu_hment_lock);
		cmn_err(CE_PANIC, "Could not allocate sx_hmentblktab.\n");
	}

	/* Set sh_validcnts to -1 to mark them as free */
	for (i = 0,  block = sx_hmentblktab;
		i < (nsxhments/SX_HMENT_BLKSIZE); block++, i++)
		block->sh_validcnt = -1;

	mutex_exit(&sxmmu_hment_lock);
}

/*
 * Allocate memory for SX HAT private data and initialize the HAT private
 * data area. Called indirectly by the SX segment driver when a process
 * first mmaps the SX register set.
 */
static void
sx_mmu_alloc(struct hat *hat, struct as *as)
{
	struct sx_hat_data *sxdata;
	struct sx_hmentblk *sx_buckets;


	mutex_enter(&hat->hat_mutex);
	sxdata = kmem_zalloc(sizeof (struct sx_hat_data), KM_SLEEP);

	/*
	 *  This allocates our hashing table.
	 */
	sx_buckets = kmem_zalloc(sizeof (struct sx_hmentblk) *
		SX_HMENT_NBUCKETS, KM_SLEEP);

	sxdata->sx_mmu_as = as;
	sxdata->sx_mmu_hashtable = sx_buckets;
	sxdata->sx_mmu_orig_memlist =
		sxdata->sx_mmu_sx_memlist = (struct sx_addr_memlist *)NULL;
	hat->hat_data[0] = (u_int)sxdata;
	mutex_exit(&hat->hat_mutex);
}

/*ARGSUSED*/
static struct as *
sx_mmu_setup(struct as *as, int allocflag)
{
	return (NULL);
}

/*
 *  Free the SX HAT private data allocated for the SX HAT.
 *  We undo everything done in sx_mmu_alloc().
 */
/*ARGSUSED*/
static void
sx_mmu_free(struct hat *hat, struct as *as)
{
	int i, j;
	struct sx_hat_data *sxdata;
	struct sx_hmentblk *sx_buckets, *hmeblk, *head;
	struct hment *hme;
	struct sx_addr_memlist *mp, *tmp;
	struct page *pp;

	mutex_enter(&hat->hat_mutex);
	sxdata = (struct sx_hat_data *)hat->hat_data[0];

	sx_buckets = sxdata->sx_mmu_hashtable;

	/*
	 * If there are any hmentblks and their associated hments still hashed
	 * in the per-address space hash table we must delete the hments
	 * from the page's mapping list and free the hmentblks.
	 */
	for (i = 0; i < SX_HMENT_NBUCKETS; i++) {

		/* Find the head...but it is not a real entry */
		head = &sx_buckets[i];

		/*
		 *  For every hmeblk on this bucket, free up all its entries
		 *  We will always just be removing the first block downstream
		 *  of the headnode.
		 */
		hmeblk = head->sh_next;
		while (hmeblk) {

			ASSERT(hmeblk != head);

			while (hmeblk->sh_validcnt != 0) {

				/*
				 * Keep hmeblk from going away by bumping count
				 */
				hmeblk->sh_validcnt++;

				/*
				 * Delete SX hme's from the page's mapping list
				 */
				for (j = 0; j < SX_HMENT_BLKSIZE; j++) {
					hme = &hmeblk->sh_hmebase[j];
					if (hme->hme_valid) {
						pp = hme->hme_page;
						mutex_exit(&hat->hat_mutex);
						ohat_mlist_enter(pp);
						mutex_enter(&hat->hat_mutex);
						/* hme may have changed */
						if (pp == hme->hme_page) {
							hme_sub(hme, pp);
							hme->hme_valid = 0;
							hmeblk->sh_validcnt--;
						}
						ohat_mlist_exit(pp);
					}
				}

				/* Restore count */
				hmeblk->sh_validcnt--;
				ASSERT(hmeblk->sh_validcnt == 0);
			}

			/* remove it from our chain */
			sx_unlink_hmentblk(hmeblk);

			hmeblk = head->sh_next;
		}
	}

	/* Now free the whole bucket list itself */
	kmem_free(sx_buckets, sizeof (struct sx_hmentblk) * SX_HMENT_NBUCKETS);

	/*
	 *  Walk our memlist, freeing up our {addr,len} pairs.
	 *  As with the hmentblks above, we will just be freeing up the
	 *  first node on the chain until we run out of nodes.
	 */
	mp = sxdata->sx_mmu_orig_memlist;
	while (mp) {
		tmp = mp->next;
		kmem_free(mp, sizeof (struct sx_addr_memlist));
		mp = tmp;
	}

	mp = sxdata->sx_mmu_sx_memlist;
	while (mp) {
		tmp = mp->next;
		kmem_free(mp, sizeof (struct sx_addr_memlist));
		mp = tmp;
	}

	/* ...and finally the hat private data */
	kmem_free(sxdata, sizeof (struct sx_hat_data));
	sxdata = (struct sx_hat_data *)NULL;
	hat->hat_data[0] = 0;
	mutex_exit(&hat->hat_mutex);
}

/*ARGSUSED*/
static int
sx_mmu_dup(struct hat *hat, struct as *as, struct as *newas)
{
	/* Just let the child fault them in when it forks */
	return (0);
}

/*ARGSUSED*/
static void
sx_mmu_swapin(struct hat *hat, struct as *as)
{
}

/*ARGSUSED*/
static void
sx_mmu_swapout(struct hat *hat, struct as *as)
{
}

/*
 * Set up addr to map to page pp with protection prot.
 * This routine is called indirectly by the SX segment driver to
 * set up translations to SX accelerated pageable DRAM. Since all
 * mappings to a page must be set up non-cached, hat_pagecachectl()
 * is called to enforce cache consistency.
 */
/*ARGSUSED*/
static void
sx_mmu_memload(struct hat *hat, struct as *as, caddr_t addr,
	struct page *pp, u_int prot, int attr)
{
	struct hat *syshat = as->a_hat;

	hat_pagecachectl(pp, HAT_UNCACHE);

	/*
	 * Let the SRMMU HAT load up the translations.
	 */
	HATOP_MEMLOAD(syshat, as, addr, pp, prot, attr);
}

/*
 *  This routine is indirectly called by the SX segment driver to load up
 *  translations for SX. The underlying physical memory could be any of
 *  pageable DRAM, Video RAM or pre-allocated physically contiguous DRAM.
 *  This routine could also be invoked by the CG14 or physically contiguous
 *  DRAM drivers (unwittingly) since the SX segment driver switches hats.
 *  XXX- Current implementation of the SX driver invokes this routine
 *  only for SX acceleration to memory backed by page structs. Translations
 *  to Physically contiguous memory and Video Memory are set up by the
 *  SRMMU HAT
 */
static void
sx_mmu_devload(struct hat *hat, struct as *as, caddr_t addr,
	devpage_t *dp, u_int pf, u_int prot, int attr)
{
	struct sx_hat_data *sxdata;
	struct page *pp = (struct page *)dp;
	struct hat *syshat = as->a_hat;
	struct sx_hmentblk *head, *block;
	struct hment *entry;
	int bucket, index;

	extern void srmmu_convert_pmapping(page_t *);

	/*
	 * Hand the request off to the SRMMU hat to load the translations.
	 * If we have been called to set up translations to pageable memory
	 * then we assume that the caller has done a page cache control
	 * operation on the page.
	 */
	HATOP_DEVLOAD(syshat, as, addr, dp, pf, prot, attr);

	mutex_enter(&hat->hat_mutex);
	sxdata = (struct sx_hat_data *)hat->hat_data[0];

	/* Check to see if hat already destroyed */
	if (sxdata == NULL) {
		mutex_exit(&hat->hat_mutex);
		return;
	}


	/*
	 * If we are not setting up a translation to SXified memory
	 * return.
	 */
	if ((((pf >> 20) & 0xf)) != SX_BUSTYPE) {
		mutex_exit(&hat->hat_mutex);
		return;
	}

	/*
	 * If we are setting up translations only to non-pageable DRAM,
	 * we simply return
	 */
	if (dp == (struct devpage *)NULL) {
		mutex_exit(&hat->hat_mutex);
		return;
	}

	/*
	 * if physical contiguous memory, then assure the non-cachable
	 * bit is set and return (skip following unnecessary checking).
	 */
	if (sx_vrfy_pfn(SXPF_TOPF(pf), SXPF_TOPF(pf))) {
		ASSERT(PP_ISNC(dp));
		mutex_exit(&hat->hat_mutex);
		return;
	}

	/*
	 *  Now we know that we are setting up a translation to
	 *  pageable memory from the system pool, we must add a dummy hment
	 *  to the page for notifications in case of page unloads.
	 *  So we add a hment for each page in the list to which we are setting
	 *  up a translation. This hment is added only because a callback is
	 *  required before a page is unloaded by the pagedaemon. The SRMMU hat
	 *  is still responsible for PTEs for SX translations.
	 */

	/*
	 *  We hash on the virtual address rounded to a 64k boundary
	 *  by discarding the bits (0-15)
	 */
	bucket = SX_HMEBLK_HASH(addr);

	ASSERT(bucket >= 0 && bucket < SX_HMENT_NBUCKETS);

	head = (struct sx_hmentblk *)(sxdata->sx_mmu_hashtable) + bucket;

	for (block = head->sh_next; block != NULL; block = block->sh_next) {

		if ((caddr_t)((u_int)addr & SX_HMENT_MASK) ==
		    block->sh_baseaddr) {

			/*
			 *  Our addr "fits" into this hment block
			 */
			entry = (struct hment *)(&block->sh_hmebase[
				((u_int)addr >> MMU_PAGESHIFT)
					& (SX_HMENT_BLKSIZE - 1)]);

			ASSERT(entry >= sx_hmetab &&
				entry < (sx_hmetab + nsxhments));

			ASSERT(entry->hme_valid == NULL ||
				entry->hme_page == dp);
			/*
			 *  If we find that the hme is already marked valid,
			 *  it means we have had our h/w translation stolen by
			 *  the srmmu HAT. This just means we do not have to
			 *  setup any more of our own state, and can safely
			 *  just return.
			 */
			if (entry->hme_valid) {
				mutex_exit(&hat->hat_mutex);
				return;
			}

			block->sh_validcnt++;
			break;
		}
	}

	/*
	 *  We get to this point by
	 *  a) having no hmentblks in our bucket at all (this will be the
	 *	initial situation), or
	 *  b) by finding no matching baseaddr in any of the extant ones.
	 *
	 *  In either case, we just allocate a new hmentblk and proceed as
	 *  before.
	 */
	if (block == NULL) {
		block = sx_allocate_hmentblk();
		if (block == NULL) {
			/* XXX - need hmentblk stealing, but for now... */
			sx_killproc();
			mutex_exit(&hat->hat_mutex);
			return;
		}
		sx_link_hmentblk(block, head);
		block->sh_baseaddr = (caddr_t)((u_int)addr & SX_HMENT_MASK);
		index = block - sx_hmentblktab;
		block->sh_hmebase = sx_hmetab + (index * SX_HMENT_BLKSIZE);
		entry = (struct hment *)&block->sh_hmebase[
			((u_int)addr >> MMU_PAGESHIFT) & (SX_HMENT_BLKSIZE-1)];
		ASSERT(entry >= sx_hmetab && entry < (sx_hmetab + nsxhments));
		block->sh_validcnt++;
	}

	/*
	 * Release lock to avoid deadlock with srmmu_page_lock.
	 */
	mutex_exit(&hat->hat_mutex);

	/*
	 * Make sure p_mapping is not singly mapped so that
	 * hme_add() below won't blow up.
	 */
	srmmu_convert_pmapping(pp);

	ohat_mlist_enter(pp);
	mutex_enter(&hat->hat_mutex);

	/* Might have been done by someone else */
	if (!entry->hme_valid) {
		entry->hme_hat = hat - hats;
		entry->hme_valid = 1;
		hme_add(entry, pp);
	} else	/* If someone else beat us to it, fix validcnt */
		block->sh_validcnt--;

	mutex_exit(&hat->hat_mutex);
	ohat_mlist_exit(pp);
}

/*
 * In the current implementation all translations to Physically contiguous
 * DRAM and Video RAM are set up by the SRMMU HAT. Someday when we have to
 * set up SX accelerated mappings to pageable physically contiguous memory
 * we have to implement the code for contiguous loads.
 */
static void
sx_mmu_contig_memload(struct hat *hat, struct as *as, caddr_t addr,
	struct page *pp, u_int prot, int attr, u_int len)
{
	struct hat *syshat = as->a_hat;

#ifdef lint
	hat = hat;
#endif lint

	HATOP_CONTIG_MEMLOAD(syshat, as, addr, pp, prot, attr, len);
}

static void
sx_mmu_contig_devload(struct hat *hat, struct as *as, caddr_t addr,
	devpage_t *dp, u_int pf, u_int prot, int attr, u_int len)
{
	struct hat *syshat = as->a_hat;

#ifdef lint
	hat = hat;
#endif lint

	HATOP_CONTIG_DEVLOAD(syshat, as, addr, dp, pf, prot, attr, len);
}

/*ARGSUSED*/
static void
sx_mmu_unlock(struct hat *hat, struct as *as, caddr_t addr, u_int len)
{
}


/*ARGSUSED*/
static faultcode_t
sx_mmu_fault(struct hat *hat, caddr_t addr)
{
	return (FC_NOMAP);
}

/*ARGSUSED*/
static void
sx_mmu_chgprot(struct as *as, caddr_t addr, u_int len, u_int vprot)
{
}

/*
 * Called at munmap time and at process exit time
 * By the time we get here the virtual to physical address translations
 * for the entire virtual address range specified here is already
 * unloaded by the SRMMU HAT. All we do here is minimally wait for the
 * SX pipeline to drain and if the virtual address specified here maps
 * pageable DRAM we do the appropriate SX hme management.
 */
static void
sx_mmu_unload(struct as *as, caddr_t addr, u_int len, int flags)
{
	int bucket;
	struct sx_hmentblk *block, *head;
	struct hment *entry;
	caddr_t va;

	struct hat *sx_hat;
	struct sx_hat_data *sxdata;
	struct page *pp;
	struct sx_addr_memlist *mp, *tmp_mp;

	if (flags & SX_TMPUNLOAD)
		return;
	/*
	 *  We need to get the SX HAT; the only way we can get it here
	 *  is to look for SX HAT in the process's address space.
	 *  Note that this list walk will fail if as->a_hat already points
	 *  to a HAT downstream of our sxhat!
	 */
	sx_hat = as->a_hat;
	for (; sx_hat != NULL; sx_hat = sx_hat->hat_next) {
		if (sx_hat->hat_op == &sx_mmu_hatops)
			break;
	}

	/*
	 *  If the SX HAT is already free (by hat_free()) we simply return
	 */
	if (sx_hat == NULL)
		return;

	mutex_enter(&sx_hat->hat_mutex);

	sxdata = (struct sx_hat_data *)sx_hat->hat_data[0];

	/*
	 * If our private data is gone, then our hat is in a
	 * transitional period of destruction.  Just exit.
	 */
	if (sxdata == NULL) {
		mutex_exit(&sx_hat->hat_mutex);
		return;
	}

	/*
	 * If address being unmapped is cloned by an SX mapping, do
	 * call back into SX segment driver to notify unmapping of the
	 * original virtual address. If the TMPUNLOAD flag is set then the
	 * SX driver is doing a context switch, unloading SX translations and
	 * therefore we need not waste time determining whether the address
	 * range being unloaded here is cloned for SX.
	 */
	if (flags & HAT_UNLOAD_UNMAP) {
		/*
		 * Determine if the address range being unloaded
		 * is cloned for SX.
		 */
		if (mp = (struct sx_addr_memlist *)sx_mmu_addr_in_origmemlist(
		    sxdata->sx_mmu_orig_memlist, addr, len)) {
			mutex_exit(&sx_hat->hat_mutex);
			while (mp) {
				/*
				 * Call back into the SX driver to notify of the
				 * original address ranges being unloaded here
				 */
				(*sx_mmu_unmap_callback)(as, mp->sx_vaddr,
					mp->size);
				tmp_mp = mp;
				mp = mp->next;
				kmem_free(tmp_mp,
					sizeof (struct sx_addr_memlist));
			}
			return;
		}
	}

	/*
	 *  We have to check if addr being unloaded is backed by
	 *  pageable DRAM. If it is, then we need to free up the
	 *  hments. We can tell if this page is used by SX by
	 *  simply using our nifty hashtable to look for it.
	 *  We come here even if this particular range of address
	 *  is not mapped for SXified accesses. hat_unload()
	 *  calls the unload routine for every hat in the address space
	 */

	/*
	 *  Check to see if a SXified mapping exists; if not,
	 *  don't waste any time.
	 */
	if (sx_mmu_addr_in_sxmemlist(sxdata->sx_mmu_sx_memlist,
	    addr, len) == NULL) {
		mutex_exit(&sx_hat->hat_mutex);
		return;
	}

	for (va = addr; va < addr + len; va += MMU_PAGESIZE) {
		bucket = SX_HMEBLK_HASH(va);
		ASSERT(bucket >= 0 && bucket < SX_HMENT_NBUCKETS);
		head = (struct sx_hmentblk *)
				&sxdata->sx_mmu_hashtable[bucket];

		for (block = head->sh_next;
			block != (struct sx_hmentblk *)NULL;
				block = block->sh_next) {

			if ((caddr_t)((u_int)va & SX_HMENT_MASK) ==
			    block->sh_baseaddr) {

				/*
				 *  Our addr is in this hment block
				 */
				entry = (struct hment *)&block->sh_hmebase[
					((u_int)va >> MMU_PAGESHIFT)
						& SX_HMENT_BLKSIZE-1];

				/*
				 *  If this entry is not in use, then we can
				 *  disregard the rest of the chain
				 */
				if (entry->hme_valid != 0) {

					pp = entry->hme_page;
					mutex_exit(&sx_hat->hat_mutex);
					ohat_mlist_enter(pp);
					mutex_enter(&sx_hat->hat_mutex);

					/* Check hme_page again */
					if (pp == entry->hme_page) {
						hme_sub(entry, pp);
						entry->hme_valid = 0;
						entry->hme_hat = NULL;
						block->sh_validcnt--;
					}
					ohat_mlist_exit(pp);

					if (block->sh_validcnt == 0)
						sx_unlink_hmentblk(block);
				}

				/* We will not hash to any other blocks, so.. */
				break;
			}
		}
	}
	mutex_exit(&sx_hat->hat_mutex);
}

/*ARGSUSED*/
static void
sx_mmu_sync(struct as *as, caddr_t addr, u_int len, u_int flags)
{
}

/*
 *  Remove all mappings to page 'pp'.
 *  The pagedaemon will call the hat_pageunload routine for every hme
 *  on the mapping list. The virtual to physical address translation
 *  would already have been unloaded by the SRMMU HAT by the time we get
 *  here. All we do here is wait minimally for the SX pipeline to
 *  drain. We can't wait forever for the SX IQ to drain because any
 *  writes to SXified memory by any process can keep the pipeline full and
 *  we have no way of knowing the specific address to which SX is
 *  rendering. However, we are guaranteed that the SX instructions fed
 *  to the page we are unloading will finish by the time we are done here.
 *  Later on, as an optimization we can can check if the page being unloaded
 *  belongs to the currently active SX process and if it does not,  skip the
 *  waiting.
 */

static void
sx_mmu_pageunload(struct page *pp, struct hment *hme)
{
	int index;
	struct hat *sxhat;
	struct sx_hmentblk *block;

	ASSERT(pp != NULL ? ohat_mlist_held(pp) : 1);
	sxhat = &hats[hme->hme_hat];
	ASSERT(sxhat->hat_op == &sx_mmu_hatops);

	mutex_enter(&sxhat->hat_mutex);

	hme_sub(hme, pp);

	hme->hme_valid = 0;

	/*
	 * Get the offset of the hme in the sx hment table
	 * and get the corresponding sx hmentblock.
	 */
	index = hme - sx_hmetab;
	ASSERT((index >= 0) && (index < nsxhments));

	block = &sx_hmentblktab[index/SX_HMENT_BLKSIZE];
	ASSERT(block->sh_validcnt > 0);

	block->sh_validcnt--;
	if (block->sh_validcnt == 0)
		sx_unlink_hmentblk(block);

	/*
	 * Write signature to HW sync port to cause drain of SX queue
	 */
	sxregsp->s_sync = 1;

	mutex_exit(&sxhat->hat_mutex);
}

/*ARGSUSED*/
static int
sx_mmu_pagesync(struct hat *hat, struct page *pp,
	struct hment *hme, u_int clearflag)
{
	return (HAT_DONE);
}


/*ARGSUSED*/
static void
sx_mmu_pagecachectl(struct page *pp, u_int flag)
{
}

/*ARGSUSED*/
static u_int
sx_mmu_getpfnum(struct as *as, caddr_t addr)
{
	return (NULL);
}

static int
sx_mmu_map(struct hat *hat, struct as *as, caddr_t addr, u_int size, int flags)
{
#ifdef lint
	hat = hat;
	as = as;
	addr = addr;
	size = size;
	flags = flags;
#endif lint

	return (0);
}

/*ARGSUSED*/
static int
sx_mmu_probe(struct hat *hat, struct as *as, caddr_t addr)
{
	return (MMU_ET_INVALID);
}

static int
sx_mmu_share(struct as *as, caddr_t addr, struct as *sptas,
	caddr_t sptaddr, u_int size)
{
#ifdef lint
	as = as;
	addr = addr;
	sptas = sptas;
	sptaddr = sptaddr;
	size = size;
#endif lint

	return (0);
}

static void
sx_mmu_unshare(struct as *as, caddr_t addr, u_int size)
{
#ifdef lint
	as = as;
	addr = addr;
	size = size;
#endif lint
}

/*
 *  Supporting routines for the SX HAT.
 *  This routine simply allocates items of type struct sx_hmentblk from
 *  the global sx_hmentblktab data struct.
 */

struct sx_hmentblk *
sx_allocate_hmentblk(void)
{
	int i;
	struct sx_hmentblk *block;

	mutex_enter(&sxmmu_hment_lock);

	/*
	 * search our array of hmentblks for a free block
	 */
	for (i = 0,  block = (struct sx_hmentblk *)sx_hmentblktab;
	    i < (nsxhments/SX_HMENT_BLKSIZE); block++, i++) {

		if (block->sh_validcnt == -1) {
			block->sh_validcnt++;
			mutex_exit(&sxmmu_hment_lock);
			return (block);
		}
	}
	mutex_exit(&sxmmu_hment_lock);

	/*
	 * XXX - need to implement hme-stealing so we can avoid this condition
	 */
	cmn_err(CE_WARN, "SX: could not find a free sx_hmentblk--- "
	    "killing process.");

	return (NULL);
}

/*
 *  This routine unlinks the hmentblk from the bucket-table
 */
void
sx_unlink_hmentblk(struct sx_hmentblk *block)
{
	struct sx_hmentblk *next = block->sh_next, *prev = block->sh_prev;

	/* unlink block from chain, if it's unused */
	if (next) {
		next->sh_prev = block->sh_prev;
	}
	if (prev) {
		prev->sh_next = next;
	}
	block->sh_next = block->sh_prev = NULL;
	block->sh_baseaddr = NULL;

	mutex_enter(&sxmmu_hment_lock);
	block->sh_validcnt--;		/* Set to -1 to mark it free */
	mutex_exit(&sxmmu_hment_lock);
}

/*
 *  This routine links the hmentblk into the bucket-table
 */
void
sx_link_hmentblk(struct sx_hmentblk *block, struct sx_hmentblk *head)
{
	block->sh_next = head->sh_next;
	block->sh_prev = head;
	head->sh_next = block;
	if (block->sh_next)
		block->sh_next->sh_prev = block;
}

/*
 * Keep a list of sxified {addr,len} values for an address
 * range which is cloned for SX access. This routine is called from the SX
 * driver at mmap(2) time or when a process with cloned mappings to SX is
 * forked (from segsx_dup()).
 * The information in the list is used when we unload a range of virtual
 * addresses and  we have to verify whether the virtual address range is
 * cloned for SX or not.
 */
void
sx_mmu_add_vaddr(struct as *as, caddr_t orig_vaddr,
	caddr_t sx_vaddr, u_int size)
{
	struct hat *sx_hat;
	struct sx_hat_data *sxdata;
	struct sx_addr_memlist *p, *q;

	sx_hat = as->a_hat;
	for (; sx_hat != NULL; sx_hat = sx_hat->hat_next) {
		if (sx_hat->hat_op == &sx_mmu_hatops)
			break;
	}

	if (sx_hat == NULL)
		return;

	mutex_enter(&sx_hat->hat_mutex);
	ASSERT(sx_hat->hat_op == &sx_mmu_hatops);
	sxdata = (struct sx_hat_data *)sx_hat->hat_data[0];

	ASSERT(sxdata != NULL);

	p = (struct sx_addr_memlist *)
		kmem_zalloc(sizeof (struct sx_addr_memlist), KM_SLEEP);

	q = (struct sx_addr_memlist *)
		kmem_zalloc(sizeof (struct sx_addr_memlist), KM_SLEEP);

	p->orig_vaddr = q->orig_vaddr = orig_vaddr;
	p->sx_vaddr = q->sx_vaddr = sx_vaddr;
	p->size = q->size = size;

	/*
	 *  Just stick it into the front of the list
	 *  p->prev will always be NULL
	 */

	/*
	 * Add information about the address range being cloned on the
	 * list used to search for original virtual address ranges.
	 */

	p->next = sxdata->sx_mmu_orig_memlist;
	if (sxdata->sx_mmu_orig_memlist != NULL)
		sxdata->sx_mmu_orig_memlist->prev = p;
	sxdata->sx_mmu_orig_memlist = p;

	/*
	 * Add information about the address range being cloned on the
	 * list used to search for cloned SX virtual address ranges.
	 */

	q->next = sxdata->sx_mmu_sx_memlist;
	if (sxdata->sx_mmu_sx_memlist != NULL)
		sxdata->sx_mmu_sx_memlist->prev = q;
	sxdata->sx_mmu_sx_memlist = q;

	mutex_exit(&sx_hat->hat_mutex);
}

/*
 *  Remove an {addr,len} pair from the list of mappings maintained in the
 *  SX HAT. Note we might be coming in here at process exit time
 *  which means the HAT has already been freed.
 */
void
sx_mmu_del_vaddr(struct as *as, caddr_t orig_vaddr,
	caddr_t sx_vaddr, u_int size, u_int flag)
{
	struct hat *sx_hat;
	struct sx_hat_data *sxdata;
	struct sx_addr_memlist *mp;

	/*
	 *  We need to get the SX HAT; the only way we can get it here
	 *  is to look for SX HAT in the process's address space.
	 *  Note that this list walk will fail if as->a_hat already points
	 *  to a HAT downstream of our sxhat!
	 */
	sx_hat = as->a_hat;
	for (; sx_hat != NULL; sx_hat = sx_hat->hat_next) {
		if (sx_hat->hat_op == &sx_mmu_hatops)
			break;
	}

	/*
	 *  If the SX HAT is already free (by hat_free()) we simply return
	 */
	if (sx_hat == NULL)
		return;

	mutex_enter(&sx_hat->hat_mutex);

	ASSERT(sx_hat->hat_op == &sx_mmu_hatops);
	sxdata = (struct sx_hat_data *)sx_hat->hat_data[0];
	ASSERT(sxdata != NULL);

	if (flag == SX_ADDR) { /* Delete information on the SX list */
		mp = sxdata->sx_mmu_sx_memlist;
		ASSERT(mp != NULL);

		while (mp != 0)	 {
		    if ((sx_vaddr == mp->sx_vaddr) && (size == mp->size)) {
			if (mp->prev != NULL)
				mp->prev->next = mp->next;
			if (mp->next != NULL)
				mp->next->prev = mp->prev;

			if (sxdata->sx_mmu_sx_memlist == mp)
				sxdata->sx_mmu_sx_memlist = mp->next;

			kmem_free(mp, sizeof (struct sx_addr_memlist));
			break;
		    }
		    mp = mp->next;
		}
	} else {
	    if (flag == ORIG_ADDR) { /* Delete information on orig list */
		mp = sxdata->sx_mmu_orig_memlist;
		ASSERT(mp != NULL);

		while (mp != 0)	 {
		    if ((orig_vaddr == mp->orig_vaddr) &&
			(sx_vaddr == mp->sx_vaddr) && (size == mp->size)) {
			    if (mp->prev != NULL)
				mp->prev->next = mp->next;
			    if (mp->next != NULL)
				mp->next->prev = mp->prev;

			    if (sxdata->sx_mmu_orig_memlist == mp)
				sxdata->sx_mmu_orig_memlist = mp->next;

			kmem_free(mp, sizeof (struct sx_addr_memlist));
			break;
		    }
		    mp = mp->next;
		}
	    }
	}
	mutex_exit(&sx_hat->hat_mutex);
}

/*
 * Return a pointer to the memlist structure containing the {orig_vaddr,len}
 * range. Called from sx_mmu_unload() to determine whether a callback to the
 * SX driver is required.
 */
static struct sx_addr_memlist *
sx_mmu_addr_in_origmemlist(struct sx_addr_memlist *mp, caddr_t va, u_int len)
{
	struct sx_addr_memlist *tmp_mp = NULL, *tmp_mp_head = NULL;
	caddr_t saddr, eaddr;

	while (mp != 0) {
		/*
		 * as long as the {vaddr,vaddr_len} falls in the mapped
		 * range, we should dup the orig list out so we can
		 * deleted/unmaped the cloned sx range
		 */
		saddr = mp->orig_vaddr;
		eaddr = saddr + mp->size;

		if (((va < saddr) && ((va + len) > saddr)) ||
		    ((va >= saddr) && (va < eaddr))) {
			tmp_mp = kmem_zalloc(sizeof (struct sx_addr_memlist),
				KM_SLEEP);
			/*
			 * initialize temporary list
			 */
			tmp_mp->orig_vaddr = saddr;
			tmp_mp->sx_vaddr = mp->sx_vaddr;
			tmp_mp->size = mp->size;

			if (tmp_mp_head == NULL) {
				tmp_mp_head = tmp_mp;
			} else {
				tmp_mp->next = tmp_mp_head;
				tmp_mp_head->prev = tmp_mp;
				tmp_mp_head = tmp_mp;
			}
		}
		mp = mp->next;
	}
	return (tmp_mp_head);
}

/*
 * Return a pointer to the memlist structure containing the {sx_vaddr,len}
 * range. Called from sx_mmu_unload() to determine whether the address range
 * being unloaded is for SX accesses.
 */
static struct sx_addr_memlist *
sx_mmu_addr_in_sxmemlist(struct sx_addr_memlist *mp, caddr_t va, u_int len)
{
	while (mp != 0) {
		if ((va >= mp->sx_vaddr) &&
		    (va + len <= (mp->sx_vaddr + mp->size))) {
			break;
		}
		mp = mp->next;
	}
	return (mp);
}

/*
 * Routine to check for SX mappings (SX hments) on the given page's mapping
 * list. Returns 1 if there is a SX mapping to this page, otherwise 0 is
 * returned. Used by the SX driver to determine whether a page can be marked
 * as cached or not.
 */
int
sx_mmu_vrfy_sxpp(struct page *pp)
{
	extern int srmmu_fd_hment_exist(page_t *, struct hatops *);

	ASSERT(pp != NULL);

	return (srmmu_fd_hment_exist(pp, &sx_mmu_hatops));
}

static void
sx_killproc(void)
{
	struct proc *p;

	/* Note: Stolen shamelessly from common/vm/vm_rm.c:rm_outofanon() */

	/*
	 * To be sure no looping (e.g. in vmsched trying to
	 * swap out) mark process locked in core (as though
	 * done by user) after killing it so no one will try
	 * to swap it out.
	 */
	p = ttoproc(curthread);
	psignal(p, SIGSEGV);
	mutex_enter(&p->p_lock);
	p->p_flag |= SLOCK;
	mutex_exit(&p->p_lock);
}

/*ARGSUSED*/
static u_int
sx_mmu_getattr(struct as *as, caddr_t addr, u_int *attr)
{
	return (0);
}

/*ARGSUSED*/
static void
sx_mmu_do_attr(struct as *as, caddr_t addr, size_t len, u_int attr, int mode)
{
}

/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
static faultcode_t
sx_mmu_softlock(hat, addr, lenp, ppp, flags)
	struct  hat *hat;
	caddr_t addr;
	size_t   *lenp;
	page_t  **ppp;
	u_int	flags;
{
	return (FC_NOSUPPORT);
}

/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
static faultcode_t
sx_mmu_pageflip(hat, addr_to, kaddr, lenp, pp_to, pp_from)
	struct hat *hat;
	caddr_t addr_to, kaddr;
	u_int   *lenp;
	page_t  **pp_to, **pp_from;
{
	return (FC_NOSUPPORT);
}
