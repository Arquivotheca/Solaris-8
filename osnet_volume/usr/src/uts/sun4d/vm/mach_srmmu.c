/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mach_srmmu.c	1.36	99/04/14 SMI"

#include <sys/t_lock.h>
#include <sys/devaddr.h>
#include <sys/memlist.h>
#include <sys/memlist_plat.h>
#include <sys/cpu.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/stack.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/sysmacros.h>
#include <vm/seg_kmem.h>
#include <vm/hat_srmmu.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <sys/vmsystm.h>

/*
 * External Routines:
 */
extern int srmmu_getctxreg(void);
extern void srmmu_setctxreg(u_int);
extern void srmmu_flush_type(caddr_t, int);
extern u_int srmmu_getctp(void);
extern u_int asm_ldphys36(unsigned long long phys_addr);

/*
 * External Data:
 */
extern caddr_t econtig;
extern struct memlist *virt_avail;
extern kmutex_t srmmu_demap_mutex;

/*
 * Global Data:
 */
u_int demap_retry_cnt;	/* demap retry count */

/*
 * Static Routines:
 */
static void load_l1(struct l1pt *, pa_t);
static void load_l2(struct ptbl *, pa_t, int);
static void load_l3(struct ptbl *, pa_t, int);
static int scan_l3(struct ptbl *ptbl);

extern int swapl(int, int *);

#ifdef DEBUG
extern int static_ctx;
#endif DEBUG

void
srmmu_tlbflush(int level, caddr_t addr, u_int ctx)
{
	int save_ctx;

	ASSERT(VALID_CTX((int)ctx));

	/*
	 * Make sure ctx is not changed while
	 * we change ctx to somewhere else.
	 */
	kpreempt_disable();

	save_ctx = srmmu_getctxreg();
	ASSERT(VALID_CTX(save_ctx));

	if (ctx != save_ctx) {
		/*
		 * flush user windows so overflows in the new
		 * context don't dump the registers to the
		 * wrong place
		 */
		flush_user_windows();
	}

	mutex_enter(&srmmu_demap_mutex);
	srmmu_setctxreg(ctx);
	demap_retry_cnt = 0;	/* Initialize demap retry count */
	srmmu_flush_type(addr, 3 - level);
	mutex_exit(&srmmu_demap_mutex);

	if (ctx != save_ctx)
		srmmu_setctxreg(save_ctx);

	kpreempt_enable();
}

int srmmu_use_inv = 0;

/*
 * Updates a PTE entry in a page table (will work with any level) in a
 * way that will avoid HAT race conditions where other CPUs could be
 * accessing this very same mapping before it has been flushed from
 * all CPU's TLBs.  In order to ensure cache consistency, we
 * need to go through an intermediate state where the PTE is set to
 * invalid.  An assumption being made here is that there will be no
 * important problems (eg. locked mappings) caused by temporarily
 * loading an invalid PTE.
 *
 * A pte is read in this file when the hat_mutex is held and at
 * fault time.  Reads in this file are easy because it only happens
 * when the hat_mutex is held.  The fault case is more complicated
 * because it can't wait for the hat_mutex.  Therefore we use a
 * second short-term spin-lock while writing the entry so the fault
 * code only needs to wait for this lock.
 *
 * Note that all stores of ptes are done with swaps.  The update
 * algorithm calls for swaps in certain places but all stores are
 * swaps to work around SuperSPARC bug #42.  The table-walk doesn't
 * snoop the store buffer so by using a swap we force the pte
 * out of the store buffer so a table-walk doesn't get a stale pte.
 */
u_int
mmu_writepte(
	struct pte *pte,	/* table entry virtual address */
	u_int entry,		/* value to write  */
	caddr_t va,		/* base address effected by pte update */
	int lvl,		/* page table level for tlb flush */
	struct hat *hat,	/* hat ptr to which pte belongs */
	int rmkeep)		/* R & M bits to carry over */
{
	u_int old_pte;
	u_int r;
	int *pte_va = (int *)pte;
	u_int mmu_writepte_noinv();
	u_int cxn;

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	ASSERT(static_ctx == 1);

	if (!srmmu_use_inv) {
		cxn = hattosrmmu(hat)->s_ctx;
		return (mmu_writepte_noinv(pte, entry, va, lvl, (int)cxn,
		    rmkeep));
	}

	cxn = hattosrmmu(hat)->s_ctx;

	old_pte = *(u_int *)pte;
	if ((rmkeep == PTE_RM_MASK) &&
		(((old_pte ^ entry) & ~PTE_RM_MASK) == 0)) {
		    return (old_pte & PTE_RM_MASK);
	}

	if (PTE_ETYPE(old_pte) == MMU_ET_PTE && cxn != (u_int)-1) {
		old_pte = 0;
		do {
			r = swapl(0, pte_va);
			srmmu_tlbflush(lvl, va, cxn);
			old_pte |= r;
		} while (*pte_va != 0);
	}

	entry |= (old_pte & rmkeep);
	(void) swapl(entry, pte_va);

	return (old_pte & PTE_RM_MASK);
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
	u_int old_pte;
	u_int r;
	int *pte_va = (int *)pte;
	u_int mmu_writepte_noinv();

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);

	if (!srmmu_use_inv)
		return (mmu_writepte_noinv(pte, entry, va, lvl, (int)cxn,
		    rmkeep));

	old_pte = *(u_int *)pte;
	if (PTE_ETYPE(old_pte) == MMU_ET_PTE && cxn != (u_int)-1) {
		old_pte = 0;
		do {
			r = swapl(0, pte_va);
			srmmu_tlbflush(lvl, va, cxn);
			old_pte |= r;
		} while (*pte_va != 0);
	}

	entry |= (old_pte & rmkeep);
	(void) swapl(entry, pte_va);

	return (old_pte & PTE_RM_MASK);
}

/*
 * The Presto driver relies on this routine and it's currently implemented
 * interface. Any change to this interface could break the Presto driver.
 */
int
srmmu_xlate(
	int not_used_cxn,	/* not used. but Presto cannot be changed. */
	caddr_t va,		/* virtual address */
	pa_t	*pap,		/* return physical address (36 bits) */
	union ptpe *ptep,	/* return page table entry */
	int	*lvlp)		/* return table level of mapping */
{
	u_int entry;	/* table entry */
	int lvl;	/* table level */
	struct pte *tmp;
	struct ptbl *ptbl;
	struct as *pas;
	kmutex_t *mtx;

	if (not_used_cxn != -1) {
		cmn_err(CE_PANIC, "srmmu_xlate: bad cxn %d", not_used_cxn);
	}

	if ((u_int) va >= (u_int) KERNELBASE)
		pas = &kas;
	else
		pas = curproc->p_as;

	tmp = srmmu_ptefind(pas, va, &lvl, &ptbl, &mtx, LK_PTBL_SHARED);
	mmu_readpte(tmp, &entry);
	unlock_ptbl(ptbl, mtx);

	if (PTE_ETYPE(entry) == MMU_ET_PTE) {
		/*
		 * Give them only what they asked for.
		 */
		if (pap != NULL) {
			switch (lvl) {
			case 3:
				*pap = ((pa_t)(entry & 0xFFFFFF00) << 4) |
					((u_int)va & 0xFFF);
				break;

			case 2:
				*pap = ((pa_t)(entry & 0xFFFFC000) << 4) |
					((u_int) va & 0x3FFFF);
				break;

			case 1:
				*pap = ((pa_t)(entry & 0xFFF00000) << 4) |
					((u_int) va & 0xFFFFFF);
				break;

			default:
				cmn_err(CE_PANIC, "bad level %d", lvl);
			}
		}

		if (ptep != NULL)
			ptep->ptpe_int = entry;

		if (lvlp != NULL)
			*lvlp = lvl;

		return (1);
	} else {
		return (0);
	}
}

#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

void
srmmu_ctxtab_to_dev_reg(
	struct regspec *drp)
{
	pa_t ctxtbl_pa;		/* context table phys addr (36 bits) */

	ctxtbl_pa = ptp_to_pa(srmmu_getctp());
	drp->regspec_bustype = (u_int) (ctxtbl_pa >> 32);
	drp->regspec_addr = (u_int) (ctxtbl_pa & 0xFFFFFFFFull);
	drp->regspec_size = nctxs * sizeof (struct ptp);
}

/*
 * called from resume to change the context register
 * (note that srmmu_setctxreg is an inline, this function
 * exists to provide an entry point)
 */
void
mmu_setctxreg(u_int ctx)
{
	ASSERT(VALID_CTX((int)ctx));

	srmmu_setctxreg(ctx);
}

u_int
mmu_getctx()
{
	u_int cxn;

	cxn = srmmu_getctxreg();

	ASSERT(VALID_CTX((int)cxn));

	return (cxn);
}

/*
 * Map in the context table.  The prom has allocated the physical
 * memory for a context table of maximum size and we want to
 * map that into kernel virtual space.  We grab some space from
 * the kernel map and ask the prom to map it for us.
 */
void
map_ctxtbl(void)
{
	u_int size, npages;	/* size in bytes and pages of area to map */
	pa_t tbl_pa;		/* context table physical address */
	u_int space, addr;	/* broken-out phys addr for prom_map */
	caddr_t kvaddr;		/* kernel virtual address */

	size = nctxs * sizeof (struct ptp);
	npages = btopr(size);
	kvaddr = vmem_alloc(heap_arena, ptob(npages), VM_SLEEP);

	tbl_pa = ptp_to_pa(srmmu_getctp());
	space = tbl_pa >> 32;
	addr = tbl_pa & 0xFFFFFFFF;
	contexts = (struct ptp *)prom_map(kvaddr, space, addr, size);
	if ((caddr_t)contexts != kvaddr)
		prom_panic("map_ctxtbl: context table mismapped");
	econtexts = contexts + nctxs;
}

void
cache_flushctx(u_int ctx)
{
	int save_ctx;

	save_ctx = srmmu_getctxreg();

	ASSERT(VALID_CTX((int)ctx));
	ASSERT(VALID_CTX(save_ctx));

	srmmu_setctxreg(ctx);

	mutex_enter(&srmmu_demap_mutex);
	srmmu_flush_type(0, FT_CTX);
	mutex_exit(&srmmu_demap_mutex);

	srmmu_setctxreg(save_ctx);
}

/*ARGSUSED5*/
void
mmu_writeptp_locked(
	struct ptp *ptp,	/* table entry virtual address */
	u_int value,		/* value to write  */
	caddr_t addr,		/* base address effected by ptp update */
	int level,		/* page table level for tlb flush */
	u_int  cxn,		/* context number in which to do the flush */
	int flags)
{
	u_int old;

	old = swapl(value, (int *)ptp);
	if (PTE_ETYPE(old) != MMU_ET_INVALID && cxn != (u_int)-1)
		srmmu_tlbflush(level, addr, cxn);
}

/*ARGSUSED5*/
void
mmu_writeptp(
	struct ptp *ptp,	/* table entry virtual address */
	u_int value,		/* value to write  */
	caddr_t addr,		/* base address effected by ptp update */
	int level,		/* page table level for tlb flush */
	struct hat *hat,	/* hat ptr to its ctx should be flushed */
	int flags)
{
	u_int cxn;
	u_int old;
	struct srmmu *srmmu = hattosrmmu(hat);

	ASSERT(static_ctx == 1);

	cxn = srmmu->s_ctx;

	old = swapl(value, (int *)ptp);
	if ((PTE_ETYPE(old) != MMU_ET_INVALID) && (cxn != (u_int)-1)) {
		srmmu_tlbflush(level, addr, cxn);
	}
}

/*
 * A simpler version of mmu_writepte used before kvm_init()
 * that doesn't use the invalidate algorithm.  This is safe
 * to do because we're UP at least through kvm_init().
 */
u_int
mmu_writepte_noinv(
	struct pte *pte,	/* table entry virtual address */
	u_int entry,		/* value to write  */
	caddr_t va,		/* base address effected by pte update */
	int lvl,		/* page table level for tlb flush */
	int cxn,		/* context number in which to do the flush */
	int rmkeep)		/* R & M bits to carry over */
{
	u_int old_pte;

	old_pte = *(u_int *)pte;

	entry |= (old_pte & rmkeep);
	(void) swapl(entry, (int *)pte);

	if (PTE_ETYPE(old_pte) == MMU_ET_PTE && cxn != -1)
		srmmu_tlbflush(lvl, va, cxn);

	return (old_pte & PTE_RM_MASK);
}

/*
 * This is just like srmmu_getkpfnum, but returns a pfn of -1
 * if the virtual address isn't mapped.
 */
pfn_t
va_to_pfn(void *vaddr)
{
	union ptpe pt;

	pt.ptpe_int = mmu_probe(vaddr, NULL);
	if (pt.ptpe_int != 0 && pt.pte.EntryType == MMU_ET_PTE)
		return (pt.pte.PhysicalPageNumber);
	else
		return (PFN_INVALID);
}

uint64_t
va_to_pa(void *vaddr)
{
	pa_t pa;
	u_int pfn;

	if ((int)(pfn = va_to_pfn(vaddr)) == -1)
		return ((pa_t)-1);

	pa = ((pa_t)pfn << PAGESHIFT) | ((u_int)vaddr & PAGEOFFSET);
	return (pa);
}

void
hat_kern_setup(void)
{
	pa_t pa;
	int entry, i, *ip;

	hat_enter(kas.a_hat);

	ASSERT(kl1pt != NULL);
	ASSERT(kl1ptp.ptpe_int != 0);

	/*
	 * Get the level-1 table from prom's contex 0.
	 */
	entry = *((u_int *)&contexts[0]);
	pa = ptp_to_pa(entry);

	/*
	 * Copy in level 1 page tables.
	 *
	 * This is where the kernel looks at what the OBP has mapped
	 * and copies some of these mappings (the ones for the kernel)
	 * over to the kernel's hat structures.
	 */
	load_l1(kl1pt, pa);

	for (ip = (int *)contexts, i = 0; i < nctxs; ip++, i++)
		(void) swapl(kl1ptp.ptpe_int, ip);

	/*
	 * Note: 4m sets up h/w ctp here, 4d does not need to since
	 * we'll just be using the same context table.
	 */

	/*
	 * We've just copied all the page tables, let's do
	 * an entire TLB flush to guarantee nothing is left
	 * over from the old tables.
	 */
	mutex_enter(&srmmu_demap_mutex);
	srmmu_flush_type(0, FT_ALL);
	mutex_exit(&srmmu_demap_mutex);

	hat_exit(kas.a_hat);

	(void) hat_setup(kas.a_hat, HAT_ALLOC);
}

/*
 * Allocate and copy in the Kernel level-1 page table.
 */
static void
load_l1(struct l1pt *l1pt, pa_t pa)
{
	struct ptbl *l2ptbl;
	register union ptpe rp;
	caddr_t vaddr;
	int i;
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
	 *	If the entry is a PTE, copy it.  If the entry
	 *	is invalid we allocate a level-2 table and put
	 *	in the ptp that points to it.  We want level-2 tables
	 *	for all kernel addresses so new kernel mappings show
	 *	up in all contexts.  If we didn't do this we would
	 *	have to modify all active level-1 tables when a kernel
	 *	level-2 table was allocated.
	 *
	 * We don't bother lock L1 here since this is still running
	 * single processor/thread at this time.
	 */

	l1ptbl = kl1ptbl;
	(void) lock_ptbl(l1ptbl, 0, &kas, 0, 1, &l1mtx);

	for (i = 0; i < NL1PTEPERPT; i++, pa += sizeof (struct ptp)) {

		ptbl_t *nl1ptbl;

		vaddr = (caddr_t)MMU_L1_VA(i);
		nl1ptbl = kl1ptbl + ((u_int)vaddr >> 30);
		if (nl1ptbl != l1ptbl) {
			unlock_ptbl(l1ptbl, l1mtx);
			(void) lock_ptbl(nl1ptbl, 0, &kas, vaddr, 1, &l1mtx);
			l1ptbl = nl1ptbl;
		}

		rp.ptpe_int = ldphys36(pa);

		/*
		 * Invalidate all level-1 entries below KERNELBASE.
		 */
		if (vaddr < (caddr_t)KERNELBASE) {
			kl1pt->ptpe[i].ptpe_int = MMU_ET_INVALID;
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
load_l2(struct ptbl *l2ptbl, pa_t pa, int alloc_only)
{
	struct ptbl *l3ptbl;
	union ptpe rp, ptpe, *l3pte;
	struct pte  *l2pte;
	caddr_t va, vaddr;
	u_int i, j;
#ifdef PREALLOC_L3
	caddr_t wkbase_addr = (caddr_t)(V_WKBASE_ADDR & ~(L3PTSIZE - 1));
#endif
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
			rp.ptpe_int = MMU_ET_INVALID;
		else
			rp.ptpe_int = ldphys36(pa);

		vaddr = va + MMU_L2_VA(i);

		switch (rp.ptp.EntryType) {
		case MMU_ET_PTP:

			l3ptbl = srmmu_ptblreserve(vaddr, 3, l2ptbl, &l3mtx);

			ptpe.ptpe_int = ptbltopt_ptp(l3ptbl);
			load_l3(l3ptbl,
				rp.ptp.PageTablePointer << MMU_STD_PTPSHIFT, 0);

			if (l3ptbl->ptbl_validcnt == NL3PTEPERPT &&
			    scan_l3(l3ptbl)) {
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
#ifdef PREALLOC_L3
			/*
			 * Allocate level-3 page tables to map the region
			 * between econtig and the fixed virtual
			 * addresses.
			 */
			if ((vaddr == wkbase_addr) ||
			    (vaddr >= kernelheap && vaddr < ekernelheap) ||
			    (vaddr >= (caddr_t)DVMABASE && vaddr <
				(caddr_t)(DVMABASE + mmu_ptob(dvmasize))) ||
			    (vaddr >= (caddr_t)PPMAPBASE && vaddr <
				(caddr_t)(PPMAPBASE + PPMAPSIZE))) {
					l3ptbl = srmmu_ptblreserve(vaddr, 3,
						l2ptbl, &l3mtx);

					ptpe.ptp.ptpe_int =
					    ptbltopt_ptp(l3ptbl);
					load_l3(l3ptbl, 0, 1);
					unlock_ptbl(l3ptbl, l3mtx);
			} else {
				ptpe.ptpe_int = MMU_STD_INVALIDPTP;
			}
#else PREALLOC_L3
			/* Don't pre-allocate it. Just invalidate L2 entry. */
			ptpe.ptpe_int = MMU_STD_INVALIDPTP;
#endif
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

u_int
ldphys36(pa_t pa)

{
	u_int psr = disable_traps();
	u_int tmp;

	tmp = asm_ldphys36(pa);
	enable_traps(psr);

	return (tmp);
}

/*
 * Allocate and copy in a level-3 page table.
 */
static void
load_l3(struct ptbl *l3ptbl, pa_t pa, int alloc_only)
{
	register union ptpe rp;
	register struct pte  *l3pte;
	caddr_t va, vaddr;
	int i;
	extern char etext[];

	l3pte = (struct pte *)ptbltopt_va(l3ptbl);

	va = (caddr_t)(l3ptbl->ptbl_base << 16);

	for (i = 0; i < NL3PTEPERPT; i++, pa += sizeof (struct pte)) {
		u_int pfn;

		/*
		 * If "alloc_only" is set, don't use the physical address.
		 */
		vaddr = va + MMU_L3_VA(i);

		if (alloc_only) {
			rp.ptpe_int = MMU_STD_INVALIDPTE;
#ifndef OLD_PROM
		/*
		 * OPB isn't ready for this change yet because it is
		 * using virt addrs set up by POST that aren't deducted
		 * from the virt_avail list.  OBP needs to remap the
		 * POST data into its virt addr range.
		 */
		} else {
			/*
			 * If the va is in the virt_avail list,
			 * then it should not be valid, and we invalidate
			 * it.  The virt_avail list is the interface by which
			 * the proms commnunicates this information and it
			 * overrules any valid prom mappings.
			 * For optimizaion, do it only when vaddr >= econtig.
			 */
			if (((caddr_t)vaddr >= econtig) &&
			    (address_in_memlist(virt_avail, (uint64_t)vaddr,
				0x1000))) {
#define	MORE_DEBUG
#ifdef MORE_DEBUG
				rp.ptpe_int = ldphys36(pa);
				if (pte_valid((struct pte *)&rp.ptpe_int)) {
					printf("load_l3: invalidating 0x%p\n",
					    (void *)vaddr);
				}
#endif MORE_DEBUG
				rp.ptpe_int = MMU_ET_INVALID;
			} else {
				rp.ptpe_int = ldphys36(pa);
			}
#else OLD_PROM
		} else {
			rp.ptpe_int = ldphys36(pa);
#endif OLD_PROM
		}
		/*
		 * Change protection for kernel text to read-only.
		 */
		if (vaddr >= (caddr_t)KERNELBASE &&
		    vaddr < (caddr_t)roundup((u_int)etext, L3PTSIZE)) {
			pfn = rp.pte.PhysicalPageNumber;
			rp.ptpe_int = PTEOF(0, pfn, MMU_STD_SRX, 1);
		}

		if (rp.ptpe_int != MMU_ET_INVALID) {
			l3ptbl->ptbl_validcnt++;
			ASSERT(PTBL_VALIDCNT(l3ptbl->ptbl_validcnt));
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
	caddr_t taddr;
	extern int mmu_l3only;

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
