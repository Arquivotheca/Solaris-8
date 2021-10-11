/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)mach_srmmu.c 1.40     98/10/25 SMI"

#include <sys/sysmacros.h>
#include <sys/t_lock.h>
#include <sys/devaddr.h>
#include <sys/obpdefs.h>
#include <sys/cpu.h>
#include <sys/scb.h>
#include <vm/seg_kmem.h>
#include <vm/hat_srmmu.h>
#include <sys/mmu.h>
#include <sys/debug.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/promif.h>
#include <sys/iommu.h>
#include <sys/cmn_err.h>
#include <sys/vm_machparam.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/physaddr.h>
#include <sys/memlist.h>
#include <sys/memlist_plat.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/vmsystm.h>

/*
 * In almost ALL parts of the HAT layer we have avoided using a
 * capture-release protocol when changing mappings and flushing the
 * caches, since the routine, hat_update_ptes() protects us from races
 * where other CPUs could be accessing a mapping while we are changing
 * that very same mapping.
 *
 * However, there is still one part which seems to require
 * capture-release: the VAC code for changing mappings from either
 * cached to uncached or cached to invalid when loading a new mapping
 * to a page would cause a cache consistency problem.  For experimental
 * purposes, one can try enabling the following variable to avoid using
 * capture-release even there.
 *
 * NOTE: WHEN CAPTURE-RELEASE IS NOT USED, IT IS NECESSARY TO
 * UNLOAD ALL MAPPINGS TO A PAGE BEFORE ANY OF THESE MAPPINGS BECOME
 * VALID AND UNCACHEABLE.  IN OTHER WORDS, YOU CAN'T SIMPLY TAKE EACH
 * MAPPING ONE AT A TIME AND MAKE IT GO FROM CACHEABLE TO INVALID TO
 * UNCACHEABLE.  OTHERWISE, MP CACHE CONSISTENCY WOULD BE BROKEN
 * BECAUSE ONE CPU COULD HAVE A CACHABLE MAPPING AND ANOTHER CPU
 * COULD HAVE AN UNCACHEABLE MAPPING.
 */
u_int avoid_capture_release = 0;

/*
 * This array maps the pte access permissions field to an 'ls -l' style
 * bit mask of the permissions it grants.  The 1st set is for the kernel,
 * and the 2nd set is for the users.
 */
u_int	srmmu_perms[] = {
	044,			/* r--r-- */
	066,			/* rw-rw- */
	055,			/* r-xr-x */
	077,			/* rwxrwx */
	011,			/* --x--x */
	064,			/* rw-r-- */
	050,			/* r-x--- */
	070			/* rwx--- */
};


/*
 * External Data
 */
extern caddr_t econtig;		/* end of first block of contiguous kernel */
extern struct memlist *virt_avail;
extern int mxcc;

/*
 * Global Data:
 */
int use_table_walk = 1;

/*
 * Static Routines:
 */
static void load_l1(struct l1pt *, u_int);
static void load_l2(struct ptbl *, u_int, int);
static void load_l3(struct ptbl *, u_int, int);
static int scan_l3(struct ptbl *ptbl);

extern int swapl(int, int *);

/*
 * Perform the appropriate flush for the particular
 * level using the supplied address and context.
 */
void
srmmu_tlbflush(int level, caddr_t addr, u_int cxn, u_int flags)
{
	ASSERT(VALID_CTX((int)cxn));

	if (level == 3) {
		mmu_flushpagectx(addr, cxn, flags);
	} else if (level == 2) {
		mmu_flushseg(addr, cxn, flags);
	} else if (level == 1) {
		mmu_flushrgn(addr, cxn, flags);
	} else if (level == 0) {
		mmu_flushctx(cxn, flags);
	} else {
		cmn_err(CE_PANIC, "srmmu_tlbflush: bad level %x", level);
	}
}

/*
 * Perform the appropriate flush for the particular
 * level using the supplied address and context.
 */
void
srmmu_vacflush(int level, caddr_t addr, u_int cxn, u_int flags)
{
	ASSERT(VALID_CTX((int)cxn));

	if (level == 3)
		vac_pageflush(addr, cxn, flags);
	else if (level == 2)
		vac_segflush(addr, cxn, flags);
	else if (level == 1)
		vac_rgnflush(addr, cxn, flags);
	else if (level == 0)
		vac_ctxflush(cxn, flags);
	else {
		cmn_err(CE_PANIC, "srmmu_vacflush: bad level %x", level);
	}
}

pfn_t
va_to_pfn(void *vaddr)
{
	union ptpe pt;

	pt.ptpe_int = mmu_probe(vaddr, NULL);
	if (pt.ptpe_int != 0 && pt.pte.EntryType != MMU_ET_INVALID)
		return (pt.pte.PhysicalPageNumber);
	else
		return (PFN_INVALID);
}

uint64_t
va_to_pa(void *vaddr)
{
	pfn_t pfn;

	if ((pfn = va_to_pfn(vaddr)) != PFN_INVALID)
		return ((pfn << MMU_STD_PAGESHIFT) |
		    ((uintptr_t)vaddr & MMU_PAGEOFFSET));
	else
		return ((uint64_t)-1);
}

/*
 * For hw block copy, this routine returns a 64 bit phys addr in pa,
 * of the form:
 *
 * |63 |62	37|36|35		5|4   0|
 * |RDY|   rsvd	  |C |	   PA<35:5>	 |00000|
 *
 * returns -1 if failure.
 */

void
va_to_pa64(caddr_t va, pa_t *pa)
{
	union ptpe pt;

	pt.ptpe_int = mmu_probe(va, NULL);
	if (pt.ptpe_int == 0 || pt.pte.EntryType == MMU_ET_INVALID)
		*pa = (u_int)-1;
	else
		*pa = ((pa_t)pt.pte.Cacheable << BC_CACHE_SHIFT) |
			((pa_t)pt.pte.PhysicalPageNumber << MMU_PAGESHIFT) |
			((u_int)va & MMU_PAGEOFFSET);
}

u_int
mmu_writepte(
	struct pte *pte,	/* table entry virtual address */
	u_int entry,		/* value to write  */
	caddr_t va,		/* base address effected by pte update */
	int lvl,		/* page table level for tlb flush */
	struct hat *hat,	/* hat ptr to which pte belongs */
	int rmkeep)		/* R & M bits to carry over */
{
	int cxn;
	u_int old_pte;
	extern int static_ctx;

	if (!static_ctx)
		mutex_enter(&hat->hat_mutex);

	cxn = hattosrmmu(hat)->s_ctx;
	old_pte = mod_writepte(pte, entry, va, lvl, cxn, rmkeep);

	if (!static_ctx)
		mutex_exit(&hat->hat_mutex);

	return (old_pte);
}

/*
 * This version assumes hat_mutex is held by caller, thus cxn
 * can be trusted.
 */
u_int
mmu_writepte_locked(
	struct pte *pte,	/* table entry virtual address */
	u_int entry,		/* value to write  */
	caddr_t va,		/* base address effected by pte update */
	int lvl,		/* page table level for tlb flush */
	u_int cxn,		/* context number */
	int rmkeep)		/* R & M bits to carry over */
{
	return (mod_writepte(pte, entry, va, lvl, cxn, rmkeep));
}

void
mmu_writeptp(
	struct ptp *ptp,	/* table entry virtual address */
	u_int entry,		/* value to write  */
	caddr_t va,		/* base address effected by pte update */
	int lvl,		/* page table level for tlb flush */
	struct hat *hat,	/* hat ptr to which pte belongs */
	int flag)
{
	int cxn;
	extern int static_ctx;

	if (!static_ctx)
		mutex_enter(&hat->hat_mutex);

	cxn = hattosrmmu(hat)->s_ctx;
	mod_writeptp(ptp, entry, va, lvl, cxn, flag);

	if (!static_ctx)
		mutex_exit(&hat->hat_mutex);
}

/*
 * This version assumes hat_mutex is held by caller, thus cxn
 * can be trusted.
 */
void
mmu_writeptp_locked(
	struct ptp *ptp,	/* table entry virtual address */
	u_int entry,		/* value to write  */
	caddr_t va,		/* base address effected by pte update */
	int lvl,		/* page table level for tlb flush */
	u_int cxn,		/* context number */
	int flag)
{
	mod_writeptp(ptp, entry, va, lvl, cxn, flag);
}

void
hat_kern_setup(void)
{
	union ptpe ct, l1;
	u_int i, pa, *ip;
	extern struct ptp *contexts;
	extern caddr_t ncbase, ncend;

	if (vac)
		vac_flush(ncbase, (unsigned)ncend - (unsigned)ncbase);

	hat_enter(kas.a_hat);

	ASSERT(kl1pt != NULL);
	ASSERT(kl1ptp.ptpe_int != 0);

	/*
	 * Get context table pointer from prom.
	 */
	ct.ptpe_int = mmu_getctp();
	pa = ct.ptp.PageTablePointer << MMU_STD_PTPSHIFT;

	/*
	 * Get the level-1 table from prom's contex 0.
	 */
	l1.ptpe_int = ldphys(pa);
	pa = l1.ptp.PageTablePointer<<MMU_STD_PTPSHIFT;

	/*
	 * Copy in level 1 page tables.
	 *
	 * This is where the kernel looks at what the OBP has mapped
	 * and copies some of these mappings (the ones for the kernel)
	 * over to the kernel's hat structures.
	 */
	load_l1(kl1pt, pa);

	/*
	 * Setup the context table.  Make every entry initially point to
	 * the kernel's l1 so we can switch contexts without worrying
	 * about whether its root pointer is valid.
	 */
	for (ip = (u_int *)contexts, i = 0; i < nctxs; ip++, i++)
		(void) swapl(kl1ptp.ptpe_int, (int *)ip);

	if ((pa = va_to_pa(contexts)) == -1)
		prom_panic("Invalid physical address for context table");


	/*
	 * Switch to kernel context and page tables.
	 */
	mmu_setctp((pa >> MMU_STD_PTPSHIFT) << RMMU_CTP_SHIFT);
	hat_exit(kas.a_hat);

	(void) hat_setup(kas.a_hat, HAT_ALLOC);
}

/*
 * Allocate and copy in the Kernel level-1 page table.
 */
static void
load_l1(struct l1pt *l1pt, u_int pa)
{
	struct ptbl *l2ptbl;
	union ptpe rp;
	caddr_t vaddr;
	u_int i;
	struct ptbl *l1ptbl;
	kmutex_t *l1mtx, *l2mtx;

	/*
	 * The kernel level-1 page table has been allocated statically.
	 *
	 * There're NL1PTEPERPT entries in a level-1 table.
	 *
	 *	If the content of the entry is a PTP then
	 *	allocate a kernel level-2 table and copy in
	 *	prom's level-2 page table.
	 *
	 *	Otherwise, in the case of PTE or INVALID,
	 *	copy in the content of the entry to the kernel level-1
	 *	page table.
	 */

	l1ptbl = kl1ptbl;
	(void) lock_ptbl(l1ptbl, 0, &kas, 0, 1, &l1mtx);

	for (i = 0; i < NL1PTEPERPT; i++, pa += sizeof (struct ptp)) {
		ptbl_t *nl1ptbl;

		rp.ptpe_int = ldphys(pa);
		vaddr = (caddr_t)MMU_L1_VA(i);

		/*
		 * Since L1 is made of 4 small L3 sized ptbls, we
		 * need to change ptbl when cross ptbl boundary.
		 */
		nl1ptbl = kl1ptbl + ((u_int)vaddr >> 30);
		if (nl1ptbl != l1ptbl) {
			unlock_ptbl(l1ptbl, l1mtx);
			(void) lock_ptbl(nl1ptbl, 0, &kas, vaddr, 1, &l1mtx);
			l1ptbl = nl1ptbl;
		}

		/*
		 * Invalidate all level-1 entries below KERNELBASE.
		 */
		if (vaddr < (caddr_t)KERNELBASE) {
			l1pt->ptpe[i].ptpe_int = MMU_STD_INVALIDPTE;
			continue;
		}

		switch (rp.ptp.EntryType) {
		case MMU_ET_PTP:
			l2ptbl = srmmu_ptblreserve(vaddr, 2, l1ptbl, &l2mtx);
			l1pt->ptpe[i].ptpe_int = ptbltopt_ptp(l2ptbl);
			load_l2(l2ptbl,
				rp.ptp.PageTablePointer << MMU_STD_PTPSHIFT, 0);
			unlock_ptbl(l2ptbl, l2mtx);
			break;

		case MMU_ET_INVALID:
			/*
			 * Preallocate all level-2 page table for kernel
			 * virtual space. This prevents the allocation
			 * of level-2 page table during fault time and
			 * the subsequent copying of this level-2 entry
			 * to other user level contexts.
			 */
			l2ptbl = srmmu_ptblreserve(vaddr, 2, l1ptbl, &l2mtx);
			l1pt->ptpe[i].ptpe_int = ptbltopt_ptp(l2ptbl);
			load_l2(l2ptbl, 0, 1);
			unlock_ptbl(l2ptbl, l2mtx);
			break;

		default:
			l1pt->ptpe[i].ptpe_int = rp.ptpe_int;
			break;
		}
	}

	unlock_ptbl(l1ptbl, l1mtx);
}

/*
 * Allocate and copy in a level-2 page table.
 */
static void
load_l2(struct ptbl *l2ptbl, u_int pa, int alloc_only)
{
	struct ptbl *l3ptbl;
	union ptpe rp, ptpe, *l3pte;
	struct pte *l2pte;
	caddr_t va, vaddr;
	int i, j;
	u_char vcnt;
	extern void srmmu_ptbl_free(struct ptbl *, kmutex_t *);
	kmutex_t *l3mtx;

	l2pte = (struct pte *)ptbltopt_va(l2ptbl);
	va = (caddr_t)(l2ptbl->ptbl_base << 16);

	/*
	 * There're NL2PTEPERPT entries in a level-2 table.
	 *
	 * 	If the content of the entry is a PTP,
	 *	allocate a kernel level-3 table and copy in
	 *	prom's level-3 page table.
	 *
	 *	Otherwise, in the case of PTE or INVALID,
	 *	copy in the content of the entry to the kernel level-2
	 *	page table.
	 */
	for (i = 0; i < NL2PTEPERPT; i++, pa += sizeof (struct ptp)) {
		/*
		 * If "alloc_only" is set, don't use the physical address.
		 */
		if (alloc_only)
			rp.ptp.EntryType = MMU_ET_INVALID;
		else
			rp.ptpe_int = ldphys(pa);

		vaddr = va + MMU_L2_VA(i);

		switch (rp.ptp.EntryType) {
		case MMU_ET_PTP:
			l3ptbl = srmmu_ptblreserve(vaddr, 3, l2ptbl, &l3mtx);

			ptpe.ptpe_int = ptbltopt_ptp(l3ptbl);
			load_l3(l3ptbl,
				rp.ptp.PageTablePointer << MMU_STD_PTPSHIFT, 0);

			vcnt = l3ptbl->ptbl_validcnt;
			if (vcnt == NL3PTEPERPT && scan_l3(l3ptbl)) {
				/*
				 * Clear this L3. We don't need it. We'll
				 * use a single L2 PTE instead.
				 *
				 * XXXX maybe better to do scan_l3() before
				 * allocating/loading L3 ptbl ....
				 */
				l3pte = (union ptpe *)ptbltopt_va(l3ptbl);
				ptpe.pte = l3pte->pte;
				for (j = 0; j < NL3PTEPERPT; j++) {
					l3pte->ptpe_int = MMU_ET_INVALID;
					l3pte++;
				}

				l3ptbl->ptbl_validcnt = 0;
				l3ptbl->ptbl_flags &= ~PTBL_KEEP;

				srmmu_ptbl_free(l3ptbl, l3mtx);
			} else {
				unlock_ptbl(l3ptbl, l3mtx);
			}
			break;

		case MMU_ET_INVALID:	/* extra ptes for kernel */
			if (vaddr != MMU_L2_BASE(V_TBR_ADDR_BASE)) {
				ptpe.ptpe_int = MMU_STD_INVALIDPTP;
				break;
			}

			l3ptbl = srmmu_ptblreserve(vaddr, 3, l2ptbl, &l3mtx);

			ptpe.ptpe_int = ptbltopt_ptp(l3ptbl);
			load_l3(l3ptbl, 0, 1);
			unlock_ptbl(l3ptbl, l3mtx);
			break;

		default:
			ptpe = rp;
			break;
		}

		if (ptpe.ptp.EntryType != MMU_ET_INVALID) {
			l2ptbl->ptbl_validcnt++;
			ASSERT(PTBL_VALIDCNT(l2ptbl->ptbl_validcnt));
		}

		l2pte[i] = ptpe.pte;
	}
}

int
ldphys(int pa)
{
	extern u_int disable_traps();
	extern void enable_traps(u_int);

	u_int psr = disable_traps();
	u_int tmp;

	tmp = asm_ldphys(pa);
	enable_traps(psr);

	return (tmp);
}

/*
 * Allocate and copy in a level-3 page table.
 */
static void
load_l3(struct ptbl *l3ptbl, u_int pa, int alloc_only)
{
	union ptpe rp;
	struct pte  *l3pte;
	caddr_t va, vaddr;
	int i;
	extern char etext[];
	extern caddr_t ncbase, ncend;

	l3pte = (struct pte *)ptbltopt_va(l3ptbl);
	va = (caddr_t)(l3ptbl->ptbl_base << 16);

	for (i = 0; i < NL3PTEPERPT; i++, pa += sizeof (struct pte)) {
		u_int pfn;

		/*
		 * If "alloc_only" is set, don't use the physical address.
		 */
		vaddr = va + MMU_L3_VA(i);
		if (alloc_only)
			rp.ptpe_int = MMU_STD_INVALIDPTE;
		else {
			/*
			 * If the va is in the virt_avail list,
			 * then it should not be valid, and we invalidate
			 * it.  The virt_avail list is the interface by which
			 * the proms commnunicates this information and it
			 * overrules any valid prom mappings.
			 * For optimizaion, do it only when vaddr >= econtig.
			 */
			if ((vaddr >= econtig) &&
			    (address_in_memlist(virt_avail, (uint64_t)vaddr,
			    0x1000)))
				rp.ptpe_int = MMU_STD_INVALIDPTE;
			else
				rp.ptpe_int = ldphys(pa);
		}

		/*
		 * Ensure that all the srmmu tables are not cached except
		 * for Viking w/ MXCC and TC bit on. All other pages of
		 * the kernel text, data, and bss, and valloc'ed region
		 * should be cached.
		 *
		 * The IOMMU table should always be non-cached.
		 *
		 * We also setup the trap addresses here.
		 */
		if (vaddr >= (caddr_t)KERNELBASE &&
		    vaddr < (caddr_t)roundup((u_int)etext, L3PTSIZE)) {
			pfn = rp.pte.PhysicalPageNumber;
			rp.ptpe_int = PTEOF(0, pfn, MMU_STD_SRX, 1);
		} else if (vaddr >= ncbase && vaddr < ncend) {
			pfn = rp.pte.PhysicalPageNumber;
			rp.ptpe_int = PTEOF(0, pfn, MMU_STD_SRWX, 0);
		} else if (vaddr >= (caddr_t)V_TBR_ADDR_BASE &&
			vaddr <= (caddr_t)V_TBR_WR_ADDR) {
			pfn = va_to_pfn(&scb);
			if (vaddr == (caddr_t)V_TBR_WR_ADDR)
				rp.ptpe_int = PTEOF(0, pfn, MMU_STD_SRWX, 1);
			else
				rp.ptpe_int = PTEOF(0, pfn, MMU_STD_SRX, 1);
		} else if (vaddr >= (caddr_t)KERNELBASE &&
			vaddr < (caddr_t)MONSTART &&
			rp.ptpe_int != MMU_STD_INVALIDPTE) {

			int cacheable = 1;

			pfn = rp.pte.PhysicalPageNumber;

			if (pf_is_memory(pfn)) {
				page_t *pp;

				pp = page_numtopp_nolock(pfn);
				if (pp && PP_ISPNC(pp)) {
					cacheable = 0;
				}
			} else {
				cacheable = 0;
			}

			rp.ptpe_int = PTEOF(0, pfn, MMU_STD_SRWX,
				cacheable);
		}

		if (rp.ptpe_int != MMU_STD_INVALIDPTE) {
			l3ptbl->ptbl_validcnt++;
		}
		l3pte[i] = rp.pte;
	}
}

/*
 * Returns whether the ptbl can be mapped by a single l2 pte.
 */
static int
scan_l3(struct ptbl *ptbl)
{
	struct pte *pte = (struct pte *)ptbltopt_va(ptbl);
	int i;
	u_int pfn, c, perms;
	extern int mmu_l3only;
	caddr_t taddr;

	if (mmu_l3only)
		return (0);

	pfn = pte->PhysicalPageNumber;
	if ((pfn & (NL3PTEPERPT - 1)) != 0)
		return (0);

	/*
	 * check for any accidental alignments in kvseg.
	 * kvseg doesn't support l2 mappings yet.
	 */
	taddr = (caddr_t)(ptbl->ptbl_base << 16);
	if (taddr >= kernelheap && taddr < ekernelheap && pf_is_memory(pfn))
		return (0);

	c = pte->Cacheable;
	perms = pte->AccessPermissions;

	for (i = 0; i < NL3PTEPERPT; i++) {
		if (pte->PhysicalPageNumber != pfn || pte->Cacheable != c ||
		    pte->AccessPermissions != perms)
			return (0);
		pte++, pfn++;
	}

	return (1);
}
