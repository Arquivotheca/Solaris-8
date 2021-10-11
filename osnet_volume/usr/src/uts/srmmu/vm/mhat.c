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
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1994  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mhat.c	1.86	99/09/22 SMI"

/*
 * Generic Hardware Address Translation interface
 */

/*
 * The hat layer is responsible for all mappings in the system. It
 * uses and maintains a list of hats active within every address
 * space and a list of mappings to every page.
 *
 * An mmu driver (object manager) is responsible for the mmu dependent
 * operations that operate on the mappings within the context of a single
 * mapping device.  This file (hat.c) is responsible for multiplexing
 * a hat request across the required mmus(hats). The operataions are:
 *
 * Opeartions for startup
 *	init - initialize any needed data structures for an mmu/hat
 *
 * Operations on hat resources of an address space
 *	alloc - allocate a hat structure for managing the mappings of a hat
 *	setup - make an address space context the current active one
 *	free - free hat resources owned by an address space
 *	swapin - load up translations at swapin time
 *	swapout - unload translations not needed while swapped
 *	dup - duplicate the translations of an address space
 *
 * Operations on a named address within a segment
 * 	memload - load/lock the given page struct
 * 	devload - load/lock the given page frame number
 *	unlock - release lock to a range of address on a translation
 *	fault - validate a mapping using a cached translation
 *	map - allocate MMU resources to map address range <addr, addr + len>
 *
 * Operations over a address range within an address space
 *	chgprot - change protections for addr and len to prot
 *	unload - unmap starting at addr for len
 *	sync - synchronize mappings for a range
 *	unmap - Unload translations and free up MMU resources allocated for
 *		mapping address range <addr, addr + len>.
 *
 * Operations on all active translations of a page
 *	pageunload - unload all translations to a page
 * 	pagesync - update the pages structs ref and mod bits, zero ref
 *			and mod bits in mmu
 *	pagecachectl - control caching of a page
 *
 * Operations that return physical page numbers
 *	getkpfnum - return page frame number for given va
 *	getpfnum - return pfn for arbitrary address in an address space
 *
 * Support operations
 *	mem - allocate/free memory for a hat, use sparingly!
 *			called without per hat hat_mutex held
 */

/*
 * When a pagefault occurs, the particular hat resource which
 * the fault should be resolved in must be indentified uniquely.
 * A hat should be allocated and linked onto the address space
 * (via hat_alloc) if one does not exist.  Once the vm system
 * resolves the fault, it will use the hat when it calls the
 * hat layer to have any mapping changes made.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/vmsystm.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/vtrace.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <vm/faultcode.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/hat.h>
#include <vm/mhat.h>
#include <vm/page.h>
#include <vm/mach_page.h>

#define	MACH_PP	((machpage_t *)(pp))

kmutex_t	hat_res_mutex;		/* protect global freehat list */

struct	hat	*hats;
struct	hat	*hatsNHATS;
struct	hat	*hatfree = (struct hat *)NULL;
extern	struct	hatops	*sys_hatops;

int		nhats;
kcondvar_t 	hat_kill_procs_cv;
int		hat_inited = 0;

static struct hat *hat_gethat();
static void hat_add(struct hat *hat, struct as *as);
static void hat_sub(struct hat *hat, struct as *as);
static void ohat_freehat(struct hat *hat);

/*
 * Call the init routines for every configured hat.
 */
void
hat_init()
{
	register struct hat *hat;
	register struct hatsw *hsw = hattab;

	/*
	 * Initialize locking needed early on as the system boots.
	 * Assumes the first entry in the hattab is the system MMU.
	 */
	(*hsw->hsw_ops->h_lock_init)();

	/*
	 * Allocate mmu independent hat data structures.
	 */
	nhats = v.v_proc + (v.v_proc/2);
	if ((hats = (struct hat *)kmem_zalloc(sizeof (struct hat) * nhats,
	    KM_NOSLEEP)) == NULL)
		panic("hat_init - Cannot allocate memory for hat structs");
	hatsNHATS = hats + nhats;

	for (hat = hatsNHATS - 1; hat >= hats; hat--) {
		mutex_init(&hat->hat_unload_other, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&hat->hat_mutex, NULL, MUTEX_DEFAULT, NULL);
		ohat_freehat(hat);
	}

	for (hsw = hattab; hsw->hsw_name && hsw->hsw_ops; hsw++) {
		if (hsw->hsw_ops->h_init)
			(*hsw->hsw_ops->h_init)();
	}

	/*
	 * We grab the first hat for the kernel,
	 * the above initialization loop initialized sys_hatops and kctx.
	 */
	AS_LOCK_ENTER(&kas, &kas.a_lock, RW_WRITER);
	(void) hat_alloc(&kas);
	AS_LOCK_EXIT(&kas, &kas.a_lock);
}

struct hat *
hat_alloc(struct as *as)
{
	return (ohat_alloc(as, sys_hatops));
}

/*
 * Allocate a hat structure.
 * Called when an address space first uses a hat.
 * Links allocated hat onto the hat list for the address space (as->a_hat)
 */
struct hat *
ohat_alloc(struct as *as, struct hatops *hatops)
{
	register struct hat *hat;

	if ((hat = hat_gethat()) == NULL)
		panic("hat_alloc - no hats");

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));
	hat->hat_op = hatops;
	hat->hat_as = as;
	hat->hat_next = NULL;
	hat_add(hat, as);

	HATOP_ALLOC(hat, as);

	return (hat);
}

/*
 * Hat_setup, makes an address space context the current active one;
 * uses the default hat, calls the setup routine for the system mmu.
 */
void
hat_setup(struct hat *hat, int allocflag)
{
	HATOP_SETUP(hat->hat_as, allocflag);
}

void
ohat_free(struct hat *hat, struct as *as)
{
	ASSERT(AS_WRITE_HELD(as, &as->a_lock));

	HATOP_FREE(hat, as);
	hat_sub(hat, as);
	ohat_freehat(hat);
}

/*
 * Free all of the mapping resources.
 */
void
hat_free_start(struct hat *hat)
{
	struct as *as = hat->hat_as;
	struct hat *syshat, *nhat;

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));

	hat = syshat = as->a_hat;
	do {
		HATOP_FREE(hat, as);
		/*
		 * Remember hat_next before ohat_freehat()
		 * wipes it out.
		 */
		nhat = hat->hat_next;
		if (hat != syshat) {
			hat_sub(hat, as);
			ohat_freehat(hat);
		}
		hat = nhat;
	} while (hat != NULL);
}

/*
 * Free all of the hat structures.
 */
void
hat_free_end(struct hat *hat)
{
	struct as *as = hat->hat_as;

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));
	ASSERT(hat->hat_next == NULL);

	hat_sub(hat, as);
	ohat_freehat(hat);
}

/*
 * Duplicate the translations of an as into another newas
 */
/* ARGSUSED */
int
hat_dup(struct hat *hat, struct hat *newhat, caddr_t addr, size_t len,
	u_int flag)
{
	int err = 0;
	struct as *as = hat->hat_as;
	struct as *newas = newhat->hat_as;

	ASSERT((flag == 0) || (flag == HAT_DUP_ALL) || (flag == HAT_DUP_COW));

	if (flag == HAT_DUP_COW)
		cmn_err(CE_PANIC, "hat_dup: HAT_DUP_COW not supported");
	else if (flag != HAT_DUP_ALL)
		return (0);

	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		if (err = HATOP_DUP(hat, as, newas)) {
			break;
		}
	}
	return (err);
}

/*
 * Set up any translation structures, for the specified address space,
 * that are needed or preferred when the process is being swapped in.
 */
void
hat_swapin(struct hat *hat)
{
	struct as *as = hat->hat_as;

	ASSERT(AS_LOCK_HELD(as, &as->a_lock));
	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_SWAPIN(hat, as);
	}
}


/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 */
void
hat_swapout(struct hat *hat)
{
	struct as *as = hat->hat_as;

	ASSERT(AS_LOCK_HELD(as, &as->a_lock));
	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_SWAPOUT(hat, as);
	}
}


/*
 * Make a mapping at addr to map page pp with protection prot.
 */
void
hat_memload(struct hat *hat, caddr_t addr, struct page *pp,
	u_int attr, u_int flags)
{
	ASSERT((hat->hat_as == &kas) ||
	    AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ASSERT(PAGE_LOCKED(pp));

	if (PP_ISFREE(pp))
		cmn_err(CE_PANIC,
		    "hat_memload: loading a mapping to free page %x", (int)pp);
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_memload: unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	HATOP_MEMLOAD(hat, hat->hat_as, addr, pp, attr, flags);
}

/*
 * XXX- For now only!! Use large page mappings later if possible
 */
void
hat_memload_array(struct hat *hat, caddr_t addr, size_t len, struct page **ppa,
	u_int attr, u_int flags)
{
	caddr_t eaddr = addr + len;

	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));

	for (; addr < eaddr; addr += PAGESIZE, ppa++) {
		ASSERT(PAGE_LOCKED(*ppa));
		hat_memload(hat, addr, *ppa, attr, flags);
	}
}

/*
 * Cons up a struct pte using the device's pf bits and protection
 * prot to load into the hardware for address addr; treat as minflt.
 */
void
hat_devload(struct hat *hat, caddr_t addr, size_t len, u_long pfn,
	u_int attr, int flags)
{
	struct page *pp;
	int origflags = flags;

	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));
	if (len == 0)
		cmn_err(CE_PANIC, "hat_devload: zero len");
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_devload: unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);
	ASSERT((len & MMU_PAGEOFFSET) == 0);

	/*
	 * If it's a memory page find its pp
	 */

	/*
	 * For now, we just spin through pages ..
	 */
	while (len != 0) {
		pp = NULL;
		if (pf_is_memory(pfn)) {
			if (!(flags & HAT_LOAD_NOCONSIST)) {
				pp = page_numtopp_nolock(pfn);
				if (pp == NULL) {
					flags |= HAT_LOAD_NOCONSIST;
				}
			}
		}

		HATOP_DEVLOAD(hat, hat->hat_as, addr, pp, pfn, attr, flags);

		flags = origflags;
		len -= MMU_PAGESIZE;
		addr += MMU_PAGESIZE;
		pfn++;
	}
}

/*
 * Set up translations to a range of virtual addresses backed by
 * physically contiguous memory. The MMU driver can take advantage
 * of underlying hardware to set up translations using larger than
 * 4K bytes size pages. The caller must ensure that the pages are
 * locked down and that their identity will not change.
 */
void
ohat_contig_memload(hat, as, addr, pp, prot, attr, len)
	register struct hat *hat;
	register struct as *as;
	register caddr_t addr;
	register page_t *pp;
	register u_int prot;
	register int attr;
	register u_int len;
{
	ASSERT(hat != NULL);

	HATOP_CONTIG_MEMLOAD(hat, as, addr, pp, prot, attr, len);
}

/*
 * Release one hardware address translation lock on the given address.
 */
void
hat_unlock(struct hat *hat, caddr_t addr, size_t len)
{
	ASSERT(hat->hat_as == &kas ||
		AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ASSERT(hat != NULL);

	HATOP_UNLOCK(hat, hat->hat_as, addr, len);
}

/* ARGSUSED */
u_int
hat_getattr(struct hat *hat, caddr_t addr, u_int *attr)
{
	return (HATOP_GETATTR(hat, hat->hat_as, addr, attr));
}

/*
 * Enables more attributes on specified address range (ie. logical OR)
 */
void
hat_setattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	struct as *as = hat->hat_as;

	ASSERT(as == &kas || AS_LOCK_HELD(as, &as->a_lock));

	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_DO_ATTR(hat, as, addr, len, attr, MHAT_SETATTR);
	}
}

/*
 * Assigns attributes to the specified address range.  All the attributes
 * are specified.
 */
void
hat_chgattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	struct as *as = hat->hat_as;

	ASSERT(as == &kas || AS_LOCK_HELD(as, &as->a_lock));

	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_DO_ATTR(hat, as, addr, len, attr, MHAT_CHGATTR);
	}
}

/*
 * Remove attributes on the specified address range (ie. loginal NAND)
 */
void
hat_clrattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	struct as *as = hat->hat_as;

	ASSERT(as == &kas || AS_LOCK_HELD(as, &as->a_lock));

	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_DO_ATTR(hat, as, addr, len, attr, MHAT_CLRATTR);
	}
}

/*
 * XXX- going away!!
 *
 * Change the protections in the virtual address range
 * given to the specified virtual protection.  If
 * vprot == ~PROT_WRITE, then all the write permission
 * is taken away for the current translations, else if
 * vprot == ~PROT_USER, then all the user permissions
 * are taken away for the current translations, otherwise
 * vprot gives the new virtual protections to load up.
 *
 * addr and len must be MMU_PAGESIZE aligned.
 */
void
hat_chgprot(struct hat *hat, caddr_t addr, size_t len, u_int vprot)
{
	struct as *as = hat->hat_as;

	ASSERT(as == &kas || AS_LOCK_HELD(as, &as->a_lock));

	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_CHGPROT(hat, as, addr, len, vprot);
	}
}

/*
 * Unload all the mappings in the range [addr..addr+len).
 */
void
hat_unload(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{
	struct as *as = hat->hat_as;

	ASSERT(as == &kas || (flags & HAT_UNLOAD_OTHER) || \
	    AS_LOCK_HELD(as, &as->a_lock));

	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_UNLOAD(hat, as, addr, len, flags);
	}
}

/*
 * Synchronize all the mappings in the range [addr..addr+len).
 */
void
hat_sync(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{
	register struct as *as = hat->hat_as;

	ASSERT(as == &kas || AS_LOCK_HELD(as, &as->a_lock));


	for (hat = as->a_hat; hat != NULL; hat = hat->hat_next) {
		HATOP_SYNC(hat, as, addr, len, flags);
	}

}

/*
 * Remove all mappings to page 'pp'.
 * XXX forceflag and return code.
 */
int
hat_pageunload(struct page *pp, u_int forceflag)
{
#ifdef lint
	forceflag = forceflag;
#endif

	(*(sys_hatops->h_pageunload))(pp, NULL);

	return (0);
}

/*
 * synchronize software page struct with hardware,
 * zeros the reference and modified bits
 */
u_int
hat_pagesync(struct page *pp, u_int clearflag)
{
	(*(sys_hatops->h_pagesync))(NULL, pp, NULL, clearflag);

	return (PP_GENERIC_ATTR(pp));
}

/*
 * Mark the page as cached or non-cached (depending on flag). Make all mappings
 * to page 'pp' cached or non-cached. This is permanent as long as the page
 * identity remains the same.
 */

void
hat_pagecachectl(page_t *pp, int flag)
{
	ASSERT(PAGE_LOCKED(pp));

	(*(sys_hatops->h_pagecachectl))(pp, flag);
}

/*
 * Get the page frame number for a particular user virtual address.
 */
u_long
hat_getpfnum(struct hat *hat, caddr_t addr)
{
	u_long pf;

	pf = HATOP_GETPFNUM(hat, hat->hat_as, addr);
	return (pf);
}

/*
 * Return the number of mappings to a particular page.
 * This number is an approximation of the number of
 * number of people sharing the page.
 */
u_long
hat_page_getshare(page_t *pp)
{
	return (MACH_PP->p_share);
}

/*
 * Kill process(es) that use the given page. (Used for parity recovery)
 * If we encounter the kernel's address space, give up (return -1).
 * Otherwise, we return 0.
 * XXX ---- ????
 */
int
hat_kill_procs(page_t *pp, caddr_t addr)
{
#ifdef lint
	pp = pp;
	addr = addr;
#endif
	cmn_err(CE_PANIC, "hat_kill_procs");
	return (0);
}

/*
 * Add a hat to the address space hat list
 * The main mmu hat is always allocated at the time the address space
 * is created. It is always the first hat on the list.  All others
 * are added after it.
 */
static void
hat_add(struct hat *hat, struct as *as)
{
	struct hat *nhat;

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));

	if (as->a_hat == NULL)
		as->a_hat = hat;
	else {
		nhat = as->a_hat->hat_next;
		hat->hat_next = nhat;
		as->a_hat->hat_next = hat;
	}
}

static void
hat_sub(struct hat *hat, struct as *as)
{
	ASSERT(AS_WRITE_HELD(as, &as->a_lock));

	if (as->a_hat == hat) {
		as->a_hat = as->a_hat->hat_next;
	} else {
		struct hat *prv, *cur;
		for (prv = as->a_hat, cur = as->a_hat->hat_next;
		    cur != NULL;
		    cur = cur->hat_next) {
			if (cur == hat) {
				prv->hat_next = cur->hat_next;
				return;
			} else
				prv = cur;
		}
		panic("hat_sub - no hat to remove");
	}
}


/*
 * generic routines for manipulating the mapping list of a page.
 */

/*
 * Enter a hme on the mapping list for page pp
 */
void
hme_add(struct hment *hme, page_t *pp)
{
	ASSERT(ohat_mlist_held(pp));

	hme->hme_prev = NULL;
	hme->hme_next = MACH_PP->p_mapping;
	hme->hme_page = pp;
	if (MACH_PP->p_mapping) {
		((struct hment *)MACH_PP->p_mapping)->hme_prev = hme;
		ASSERT(MACH_PP->p_share > 0);
	} else  {
		ASSERT(MACH_PP->p_share == 0);
	}
	MACH_PP->p_mapping = hme;
	MACH_PP->p_share++;
}

/*
 * remove a hme from the mapping list for page pp
 */
void
hme_sub(struct hment *hme, page_t *pp)
{
	ASSERT(ohat_mlist_held(pp));
	ASSERT(hme->hme_page == pp);

	if (MACH_PP->p_mapping == NULL)
		panic("hme_sub - no mappings");

	ASSERT(MACH_PP->p_share > 0);
	MACH_PP->p_share--;

	if (hme->hme_prev) {
		ASSERT(MACH_PP->p_mapping != hme);
		ASSERT(hme->hme_prev->hme_page == pp);
		hme->hme_prev->hme_next = hme->hme_next;
	} else {
		ASSERT(MACH_PP->p_mapping == hme);
		MACH_PP->p_mapping = hme->hme_next;
		ASSERT((MACH_PP->p_mapping == NULL) ?
		    (MACH_PP->p_share == 0) : 1);
	}

	if (hme->hme_next) {
		ASSERT(hme->hme_next->hme_page == pp);
		hme->hme_next->hme_prev = hme->hme_prev;
	}

	/*
	 * zero out the entry
	 */
	hme->hme_next = NULL;
	hme->hme_prev = NULL;
	hme->hme_hat = NULL;
	hme->hme_page = (page_t *)NULL;
}

/*
 * Get a hat structure from the freelist
 */
static struct hat *
hat_gethat()
{
	struct hat *hat;

	mutex_enter(&hat_res_mutex);
	if ((hat = hatfree) == NULL)	/* "shouldn't happen" */
		panic("hat_gethat - out of hats");

	hatfree = hat->hat_next;
	hat->hat_next = NULL;

	mutex_exit(&hat_res_mutex);
	return (hat);
}

static void
ohat_freehat(struct hat *hat)
{
	int i;

	mutex_enter(&hat->hat_mutex);

	mutex_enter(&hat_res_mutex);
	hat->hat_op = (struct hatops *)NULL;
	hat->hat_as = (struct as *)NULL;

	for (i = 0; i < HAT_PRIVSIZ; i++)
		hat->hat_data[i] = 0;

	mutex_exit(&hat->hat_mutex);

	hat->hat_next = hatfree;
	hatfree = hat;
	mutex_exit(&hat_res_mutex);
}

/*
 * hat_probe returns 1 if the translation for the address 'addr' is
 * loaded, zero otherwise.
 *
 * hat_probe should be used only for advisorary purposes because it may
 * occasionally return the wrong value. The implementation must guarantee that
 * returning the wrong value is a very rare event. hat_probe is used
 * to implement optimizations in the segment drivers.
 *
 * hat_probe doesn't acquire hat_mutex.
 */
int
hat_probe(struct hat *hat, caddr_t addr)
{
	return (HATOP_PROBE(hat, hat->hat_as, addr));
}

/*
 * hat_map() is called when an address range is created to pre-allocate any
 * MMU resources such as segment descriptor tables, page tables etc that are
 * necessary to map the address range <addr, addr + len>. hat_map() does not
 * pre-load virtual to physical address translations.
 */

void
hat_map(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{
	HATOP_MAP(hat, hat->hat_as, addr, len, flags);
}

/*
 * Copy top level mapping elements (L1 ptes or whatever)
 * that map from saddr to (saddr + len) in sas
 * to top level mapping elements from daddr in das.
 *
 * Hat_share()/unshare() return an (non-zero) error
 * when saddr and daddr are not properly aligned.
 *
 * The top level mapping element determines the alignment
 * requirement for saddr and daddr, depending on different
 * architectures.
 *
 * When hat_share()/unshare() are not supported,
 * HATOP_SHARE()/UNSHARE() return 0.
 */
int
hat_share(struct hat *dhat, caddr_t daddr,
	struct hat *shat, caddr_t saddr, size_t size)
{
	return (HATOP_SHARE(dhat, dhat->hat_as, daddr, shat->hat_as,
	    saddr, size));
}

/*
 * Invalidate top level mapping elements in as
 * starting from addr to (addr + size).
 */
void
hat_unshare(struct hat *hat, caddr_t addr, size_t size)

{
	HATOP_UNSHARE(hat, hat->hat_as, addr, size);
}

/*
 * Find pages behind [addr..addr+len] under "as" and try to acquire
 * their shared locks. The mapping has to be user-readable. If
 * HAT_COW is set in flags, the mapping will be changed to readonly.
 * This is for the caller who wants to set up copy-on-write.
 * On exit, the number of bytes that fail to softlock is returned, with
 * "ppp" containing the list of pages it locked.
 */
faultcode_t
hat_softlock(hat, addr, lenp, ppp, flags)
	struct  hat *hat;
	caddr_t	addr;
	size_t	*lenp;
	page_t	**ppp;
	u_int	flags;
{
	return (HATOP_SOFTLOCK(hat, addr, lenp, ppp, flags));
}

/*
 * This function works closely with "page_flip" to try to flip pages
 * behind (hat, addr_to) and (kas, kaddr). So you have to also study
 * "page_flip" to see how the work is divided.
 *
 * It tries to excl lock pages behind "addr_to" and "kaddr", and flip
 * all their mappings. Since the idea behind page flipping is to serve
 * as a fast path for "copyout()", mappings behind (hat, addr_to)
 * will be checked for user-writable. If passed, P_REF|P_MOD will be
 * set on the target pages after flipped (pp_from).
 * hat_pageflip is also responsible for updating "p_nrm" (P_PNC, P_TNC,
 * P_RO), "p_mapping", "p_vcolor", "p_index", "p_share", "p_inuse",
 * "p_wanted" fields.
 *
 * Pages found behind "addr_to" is returned in "pp_to", and pages behind
 * "kaddr" in "pp_from". The number of bytes that fail to flip is
 * returned to the caller, which is expected to complete the unfinished
 * part by using "copyout()".
 *
 * Error return:
 * FC_NOMAP: no valid user mapping loaded.
 * FC_PROT: protection violation encountered.
 * FC_OBJERR: page busy.
 * FC_NOSUPPORT: operation not supported.
 */
faultcode_t
hat_pageflip(hat, addr_to, kaddr, lenp, pp_to, pp_from)
	struct hat *hat;
	caddr_t addr_to, kaddr;
	size_t	*lenp;
	page_t	**pp_to, **pp_from;
{
	ASSERT(hat == curproc->p_as->a_hat);
	/*
	 * Otherwise we have to make sure "hat" won't disappear on us.
	 * ASSERT (AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	 */

	return (HATOP_PAGEFLIP(hat, addr_to, kaddr, lenp,
			&pp_to[0], &pp_from[0]));
}
