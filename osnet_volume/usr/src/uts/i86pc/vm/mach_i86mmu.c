/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mach_i86mmu.c	1.52	99/08/02 SMI"

#include <sys/t_lock.h>
#include <sys/memlist.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vm_machparam.h>
#include <sys/tss.h>
#include <sys/vnode.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/hat_i86.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>

static int lomem_check_limits(ddi_dma_attr_t *attr, uint_t lo, uint_t hi);

/*
 * External Data
 */
extern caddr_t econtig;		/* end of first block of contiguous kernel */
extern caddr_t eecontig;	/* end of segkp, which is after econtig */
extern struct memlist *virt_avail;
extern uint_t	phys_syslimit;
extern struct tss386 dftss;
extern	struct hat 	*kernel_hat;
extern int	hat_use4kpte;
extern	pteval_t	*kernel_only_pagedir;

extern pteval_t	*hat_ptefind(struct hat *, caddr_t);
extern void	hme_add(page_t *, pteval_t *, int);

/*
 * This procedure is callable only while the page directory and page
 * tables are mapped with their virtual addresses equal to their
 * physical addresses, i.e., before we attach kvseg.
 * Boot is in control and it uses 32 bit pagetable
 * entries that support pages of size 4K and 4Mb.
 *
 */
pfn_t
va_to_pfn(void *vaddr)
{
	register pte32_t *ptep;
	uint_t	pdir_entry, _4mb_pfn;

	ASSERT(kvseg.s_base == NULL);

	ptep = (pte32_t *)((cr3() & MMU_STD_PAGEMASK)) + MMU32_L1_INDEX(vaddr);
	if (!pte32_valid(ptep))
		return (PFN_INVALID);
	if (four_mb_page(ptep)) {
		pdir_entry = *(uint32_t *)ptep;
		_4mb_pfn = pdir_entry >> MMU_STD_PAGESHIFT;
		_4mb_pfn += MMU32_L2_INDEX(vaddr);
		return (_4mb_pfn);

	} else {
		ptep = ((pte32_t *)
			(ptep->PhysicalPageNumber<<MMU_STD_PAGESHIFT)) +
					MMU32_L2_INDEX(vaddr);
		return (pte32_valid(ptep) ?
			(pfn_t)(ptep->PhysicalPageNumber) : PFN_INVALID);
	}
}

#ifdef	PTE36

static pteval_t *alloc_two_ptepages(uint_t, pteval_t *, pte32_t *);
static pteval_t *alloc_two_ptelgpages(uint_t, pteval_t *, pte32_t *);

void
hat_kern_setup()
{
	struct	hat	*hat;
	pte32_t		*bootpte;
	pteval_t	*kernpte;
	pteptr_t	pagedir, pagetbl;
	pteval_t	pagedirvalue;
	int		i;
	uint_t		vaddr, enable_pae_phyaddr, kpgdircnt;
	extern void	setup_121_andcall();
	extern void	enable_pae();
	extern uint_t	user_pgdirpttblents;
	extern uint_t	kernel_pdelen;
	/*
	 * Initialize the rest of the kas i86mmu hat.
	 */
	hat = kas.a_hat;
	hat->hat_cpusrunning = 1;


	/*
	 * Copy boot page tables that map the kernel into
	 * the kernel's hat structures.
	 * we convert 32 bit ptes to 64 bit ptes.
	 * All kernel segments are 4Mb aligned.
	 */
	bootpte = (pte32_t *)(cr3() & MMU_STD_PAGEMASK);
	kernpte = CPU->cpu_pagedir;
	for (i = 0; i < MMU32_NPTE_ONE; bootpte++, kernpte += 2, i++) {
		vaddr = MMU32_L1_VA(i);
		if (vaddr < (uint_t)kernelbase) {
			/*
			 * Invalidate all level-1 entries below kernelbase and
			 * those which are otherwise unused except boot code
			 */
			if (!pte32_valid(bootpte)) {
				*kernpte = MMU_STD_INVALIDPTE;
				*(kernpte + 1) = MMU_STD_INVALIDPTE;
			} else {
				/*
				 * This should be boot code, we need to map this
				 */
				(void) alloc_two_ptepages(vaddr, kernpte,
				    (pte32_t *)(bootpte->PhysicalPageNumber *
				    MMU_STD_PAGESIZE));
			}
			continue;
		}
		if (pte32_valid(bootpte)) {
			/*
			 * If this is a 4Mb page, then we map this
			 * as two 2Mb page.
			 */
			if (four_mb_page(bootpte)) {
				(void) alloc_two_ptelgpages(vaddr, kernpte,
						bootpte);
			} else {
				(void) alloc_two_ptepages(vaddr, kernpte,
				    (pte32_t *)(bootpte->PhysicalPageNumber *
				    MMU_STD_PAGESIZE));
			}
		} else if ((vaddr < (uint_t)eecontig) ||
		    (vaddr >= (uint_t)SEGKMAP_START))
			/* Just allocate ptes for used virtual space */
			(void) alloc_two_ptepages(vaddr, kernpte, NULL);
	}

	for (i = 0, vaddr = (uint_t)CPU->cpu_pagedir; i < NPDPERAS;
	    i++, vaddr += MMU_PAGESIZE) {
		CPU->cpu_pgdirpttbl[i] =
		    PTBL_ENT(va_to_pfn((void *)vaddr));
	}

	/*
	 * enable_pae() is called to switch from 32 bit pte's to 64 bit pte's.
	 * We need to setup one-to-one mapping for this function in
	 * the 64 bit pte  pagetable.
	 * The function setup_121_andcall() does the same in the 32 bit pte
	 * pagetable. When we disable paging in enable_pae(), we would be using
	 * 32 bit pte's. When we enable paging after having set PAE bit
	 * we would be using 64 bit pte's.
	 * we use kernel_only_pagedir as second level pagetable
	 * There is an assumption here that the physical address of the
	 * function enable_pae (enable_pae_phyaddr) is not above kernelbase.
	 * The system will page fault, if the following is true
	 * MMU_L1_INDEX(enable_pae_phyaddr) == MMU_L1_INDEX(CPU->cpu_pagedir)
	 * The problem is that the kernel picks the virtual address and
	 * boot picks the physical address. It would be nice to request
	 * boot to allocate a given physical page.
	 */
	enable_pae_phyaddr = (va_to_pfn((void *)enable_pae) <<
	    MMU_STD_PAGESHIFT) + ((uint_t)enable_pae & PAGEOFFSET);
	pagedir = CPU->cpu_pagedir;
	pagetbl = (pteptr_t)kernel_only_pagedir;
	pagedirvalue = pagedir[MMU_L1_INDEX(enable_pae_phyaddr)];
	pagedir[MMU_L1_INDEX(enable_pae_phyaddr)] =
	    PTEOF_C(va_to_pfn(kernel_only_pagedir), MMU_STD_SRWXURWX);
	pagetbl[MMU_L2_INDEX(enable_pae_phyaddr)] =
	    PTEOF_C(va_to_pfn((void *)enable_pae), MMU_STD_SRWXURWX);



	CPU->cpu_cr3 = (uint_t)(va_to_pfn(CPU->cpu_pgdirpttbl) <<
	    MMU_STD_PAGESHIFT) + ((uint_t)CPU->cpu_pgdirpttbl & PAGEOFFSET);
	dftss.t_cr3 = kernel_only_cr3;
	CPU->cpu_tss->t_cr3 = CPU->cpu_cr3;

	/*
	 * Enable Physical Address Extension (PAE)
	 */
	setup_121_andcall(enable_pae, CPU->cpu_cr3);
	/* destroy the one-to-one mapping that we setup before */
	pagedir[MMU_L1_INDEX(enable_pae_phyaddr)] = pagedirvalue;
	pagetbl[MMU_L2_INDEX(enable_pae_phyaddr)] = MMU_STD_INVALIDPTE;

	kpgdircnt = NPDPERAS - user_pgdirpttblents + (kernel_pdelen ? 1 : 0);
	bcopy((caddr_t)CPU->cpu_pagedir +
		((NPDPERAS - kpgdircnt) * MMU_PAGESIZE),
		kernel_only_pagedir, kpgdircnt * MMU_STD_PAGESIZE);

}

static pteval_t *
allocptepage(uint_t vaddr, pteval_t *kernpte, pte32_t *bootpte)
{
	pteval_t 	*ptep, *ptep1;
	int 		i;

	ptep = ptep1 = hat_ptefind(kernel_hat, (caddr_t)vaddr);

	if (!ptep) {
		ASSERT(vaddr < kernelbase);
		/* We are trying to map addr below kernelbase (at 0) */
		KMEM_ALLOC_STAT(ptep1, MMU_PAGESIZE, KM_NOSLEEP);
		ptep = ptep1;
		ASSERT(ptep);			/* ### */
	}
	/* Set kernel page directory entry */
	/* LINTED constant operand to op: "!" */
	*kernpte = PTEOF(va_to_pfn(ptep), 1, MMU_STD_SRWX);

	/* Copy boot ptes, if provided, to just-allocated page table */
	if (bootpte) {
		for (i = NPTEPERPT; --i >= 0; ptep++, bootpte++) {
			MOVPTE32_2_PTE64(bootpte, ptep);
			/*
			 * make sure no one does an hme_sub by setting
			 * noconsist. It is stretching the definition of
			 * noconsist, but IA32 has not other use for consist
			 */
			setpte_noconsist(ptep);
		}
	}
	return (ptep1);
}
/*
 * Allocate and copy in a page of kernel ptes.
 * The pagetables allocated are contiguous and
 * grow towards lower virtual address.
 */
static pteval_t *
alloc_two_ptepages(uint_t vaddr, pteval_t *kernpte, register pte32_t *bootpte)
{
	pteval_t	*pte;

	/*
	 * setup the upper 2 MB of the 4MB chunk
	 */
	pte = allocptepage(vaddr + TWOMB_PAGESIZE, kernpte+1,
		(bootpte) ? bootpte + NPTEPERPT : NULL);
	/*
	 * Setup the lower 2Mb.
	 */
	pte = allocptepage(vaddr, kernpte, bootpte);

	return (pte);
}

static pteval_t *
allocptelgpage(uint_t vaddr, pteval_t *kernpte, register pte32_t *bootpte)
{
	pteval_t		*pte;
	uint32_t		pfnum;
	int 			i;
	extern int		hat_gb_enable;

	pte = hat_ptefind(kernel_hat, (caddr_t)vaddr);
	MOV4MBPTE32_2_2MBPTE64(bootpte, kernpte);
	pfnum = bootpte->PhysicalPageNumber;
	for (i = 0; i < NPTEPERPT; i++, pfnum++) {
		*pte++ = PTEOF_C(pfnum, MMU_STD_SRWX) | hat_gb_enable;
	}

	return (pte);
}
static pteval_t *
alloc_two_ptelgpages(uint_t vaddr, pteval_t *kernpte, register pte32_t *bootpte)
{
	pteval_t 	*pte;
	pte32_t		pte32;

	/*
	 * Setup the upper 2Mb page
	 */
	pte32 = *bootpte;
	pte32.PhysicalPageNumber += NPTEPERPT;
	pte = allocptelgpage(vaddr + TWOMB_PAGESIZE, kernpte+1, &pte32);

	/*
	 * Setup the lower 2Mb page
	 */
	pte32 = *bootpte;
	pte = allocptelgpage(vaddr, kernpte, &pte32);

	return (pte);
}

#else		/* PTE36 */

static pteval_t *allocptepage(uint_t, pteval_t *, pteval_t *);

void
hat_kern_setup(void)
{
	struct	hat	*hat;
	pteval_t	*bootpte, *kernpte;
	int		i, _4K_pfnum, j;
	pteval_t	*_4kpte;
	uint_t		vaddr;

	/*
	 * Initialize the rest of the kas i86mmu hat.
	 */
	hat = kas.a_hat;
	hat->hat_cpusrunning = 1;


	/*
	 * Copy the boot page tables that map the kernel into
	 * the kernel's hat structures.
	 */
	bootpte = (pteval_t *)(cr3() & MMU_STD_PAGEMASK) + MMU_NPTE_ONE - 1;
	kernpte = (pteval_t *)(CPU->cpu_pagedir) + MMU_NPTE_ONE - 1;
	for (i = MMU_NPTE_ONE; --i >= 0; --bootpte, --kernpte) {
		vaddr = MMU_L1_VA(i);

		/* Leave map of lower 4 meg for bootops -- VERY temporary */
		if (vaddr >= (uint_t)kernelbase || vaddr == 0) {
			if (pte_valid(bootpte)) {
				/*
				 * If this is a 4Mb page, then we need to
				 * retain the pagedirectory entry and fill in
				 * the pagetable with incremental pfnum
				 * i86mmu_ptefind() will return 4k pfnum
				 * even though they are really backed by a
				 * 4Mb page
				 */
				if (four_mb_page(bootpte)) {
					_4kpte =
					allocptepage(vaddr, kernpte, NULL);
					*kernpte = *bootpte;
					_4K_pfnum =
					((pte_t *)bootpte)->PhysicalPageNumber;
					for (j = 0; j < NPTEPERPT; j++,
						_4K_pfnum++) {

						*(uint_t *)_4kpte++ = (uint_t)
						(MMU_L2_VA(_4K_pfnum)|
						PTE_PERMS(MMU_STD_SRWX)|PG_V);
					}
				} else {
				/*
				 * Copy maps for boot-allocated pages.
				 */
					(void) allocptepage(vaddr, kernpte,
						(pteval_t *)(MMU_STD_PAGESIZE *
						((pte_t *)bootpte)->
							PhysicalPageNumber));
				}
			} else if ((vaddr < (uint_t)eecontig)	||
				(vaddr >= (uint_t)SEGKMAP_START))
				/* Just allocate ptes for used virtual space */
				(void) allocptepage(vaddr, kernpte, NULL);
			continue;
		}

		/*
		 * Invalidate all level-1 entries below kernelbase and
		 * those which are otherwise unused.
		 */
		*kernpte = MMU_STD_INVALIDPTE;
	}

	/*
	 * Load CR3 to start mapping with the kernel page directory.
	 * Loading CR3 flushes the entire TLB.
	 */
	CPU->cpu_cr3 = (uint_t)(va_to_pfn(CPU->cpu_pagedir) <<
	    MMU_STD_PAGESHIFT);
	setcr3(CPU->cpu_cr3);
	dftss.t_cr3 = CPU->cpu_cr3;
	CPU->cpu_tss->t_cr3 = CPU->cpu_cr3;


	(void) hat_setup(kas.a_hat, HAT_ALLOC);
	(void) bcopy((caddr_t)CPU->cpu_pagedir,
		kernel_only_pagedir, MMU_STD_PAGESIZE);

}

/*
 * Allocate and copy in a page of kernel ptes.
 * The pagetables allocated are contiguous and
 * grow towards lower virtual address.
 */
static pteval_t *
allocptepage(uint_t vaddr, pteval_t *kernpte, pteval_t *bootpte)
{
	pteval_t	*ptep, *ptep1;
	int		i;
	extern int	hat_gb_enable;
	int		gbenable = hat_gb_enable;

	ptep = ptep1 = hat_ptefind(kernel_hat, (caddr_t)vaddr);

	if (!ptep) {
		ASSERT(vaddr < kernelbase);
		/* We are trying to map addr below kernelbase (at 0) */
		KMEM_ALLOC_STAT(ptep1, MMU_PAGESIZE, KM_NOSLEEP);
		ptep = ptep1;
		ASSERT(ptep);
		gbenable = 0;
	}
	/* Set kernel page directory entry */
	/* LINTED constant operand to op: "!" */
	*kernpte = PTEOF(va_to_pfn(ptep), 1, MMU_STD_SRWX);

	/* Copy boot ptes, if provided, to just-allocated page table */
	if (bootpte) {
		for (i = NPTEPERPT; --i >= 0; ptep++, bootpte++) {
			*ptep = *bootpte | gbenable;
			setpte_noconsist(ptep);
		}
	}
	return (ptep1);
}

#endif


/*
 * Kernel lomem memory allocation/freeing
 */

static struct lomemlist {
	struct lomemlist	*lomem_next;	/* next in a list */
	ulong_t		lomem_paddr;	/* base kernel virtual */
	ulong_t		lomem_size;	/* size of space (bytes) */
} lomemusedlist, lomemfreelist;

static kmutex_t	lomem_lock;		/* mutex protecting lomemlist data */
static int lomemused, lomemmaxused;
static caddr_t lomem_startva;
static caddr_t lomem_endva;
static ulong_t  lomem_startpa;
static kcondvar_t lomem_cv;
static ulong_t  lomem_wanted;


#ifdef DEBUG
static int lomem_debug = 0;
#endif /* DEBUG */

void
lomem_init(void)
{
	pgcnt_t nfound;
	pfn_t pfn;
	struct lomemlist *dlp;
	int biggest_group = 0;
	caddr_t addr;
	int i;

	extern struct vnode kvp;
	extern long lomempages;
	/*
	 * Try to find lomempages pages of contiguous memory below 16meg.
	 * If we can't find lomempages, find the biggest group.
	 * With 64K alignment being needed for devices that have
	 * only 16 bit address counters (next byte fixed), make sure
	 * that the first physical page is 64K aligned; if lomempages
	 * is >= 16, we have at least one 64K segment that doesn't
	 * span the address counter's range.
	 */

again:
	if (lomempages <= 0)
		return;

	for (nfound = 0, pfn = 0; pfn < btop(16*1024*1024); pfn++) {
		if (page_numtopp_alloc(pfn) == NULL) {
			/* Encountered an unallocated page. Back out.  */
			if (nfound > biggest_group)
				biggest_group = nfound;
			for (; nfound; --nfound)
				page_free(page_numtopp_nolock(pfn-nfound), 1);
			/*
			 * nfound is back to zero to continue search.
			 * Bump pfn so next pfn is on a 64Kb boundary.
			 */
			pfn |= (btop(64*1024) - 1);
		} else {
			if (++nfound >= lomempages)
				break;
		}
	}

	if (nfound < lomempages) {

		/*
		 * Ran beyond 16 meg.  pfn is last in group + 1.
		 * This is *highly* unlikely, as this search happens
		 *   during startup, so there should be plenty of
		 *   pages below 16mb.
		 */

		if (nfound > biggest_group)
			biggest_group = nfound;

		cmn_err(CE_WARN, "lomem_init: obtained only %d of %d pages.\n",
				biggest_group, (int)lomempages);

		if (nfound != biggest_group) {
			/*
			 * The last group isn't the biggest.
			 * Free it and try again for biggest_group.
			 */
			for (; nfound; --nfound) {
				page_free(page_numtopp_nolock(pfn-nfound), 1);
			}
			lomempages = biggest_group;
			goto again;
		}

		--pfn;	/* Adjust to be pfn of last in group */
	}


	/* pfn is last page frame number; compute  first */
	pfn -= (nfound - 1);
	lomem_startva = i86devmap(pfn, nfound, PROT_READ|PROT_WRITE);
	lomem_endva = lomem_startva + ptob(lomempages);

	/* Set up first free block */
	lomemfreelist.lomem_next = dlp =
		(struct lomemlist *)kmem_alloc(sizeof (struct lomemlist), 0);
	dlp->lomem_next = NULL;
	dlp->lomem_paddr = lomem_startpa = ptob(pfn);
	dlp->lomem_size  = ptob(nfound);

	/*
	 * crash dump, libkvm support:
	 *
	 * Add the allocated pages to the page-hash list so that
	 * libkvm can find these pages when looking at the crash
	 * dumps.
	 */
	for (addr = lomem_startva, i = lomempages; i; --i) {
		page_t *pp;

		pp = page_numtopp_nolock(pfn);
		(void) page_hashin(pp, &kvp, (u_offset_t)(uint_t)addr, NULL);
		addr += PAGESIZE;
		pfn++;
	}

#ifdef DEBUG
	if (lomem_debug)
		printf("lomem_init: %d pages, phys=%x virt=%x\n",
		    (int)lomempages, (int)dlp->lomem_paddr,
		    (int)lomem_startva);
#endif /* DEBUG */

	mutex_init(&lomem_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&lomem_cv, NULL, CV_DEFAULT, NULL);
}

/*
 * Allocate contiguous, memory below 16meg.
 * Only used for ddi_iopb_alloc (and ddi_memm_alloc) - os/ddi_impl.c.
 */
caddr_t
lomem_alloc(uint_t nbytes, ddi_dma_attr_t *attr, int align, int cansleep)
{
	struct lomemlist *dlp;	/* lomem list ptr scans free list */
	struct lomemlist *dlpu; /* New entry for used list if needed */
	struct lomemlist *dlpf;	/* New entry for free list if needed */
	struct lomemlist *pred;	/* Predecessor of dlp */
	struct lomemlist *bestpred = NULL;
	ulong_t left, right;
	ulong_t leftrounded, rightrounded;

#ifdef lint
	align = align;
#endif

	/* make sure lomem_init() has been called */
	ASSERT(lomem_endva != 0);

	if ((dlpu = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL)
			return (NULL);

	/* In case we need a second lomem list element ... */
	if ((dlpf = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL) {
			kmem_free(dlpu, sizeof (struct lomemlist));
			return (NULL);
	}

	/* Force 16-byte multiples and alignment; great simplification. */
	nbytes = (nbytes + 15) & (~15);

	mutex_enter(&lomem_lock);

again:
	for (pred = &lomemfreelist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp) {
		/*
		 * The criteria for choosing lomem space are:
		 *   1. Leave largest possible free block after allocation.
		 *	From this follows:
		 *		a. Use space in smallest usable block.
		 *		b. Avoid fragments (i.e., take from end).
		 *	Note: This may mean that we fragment a smaller
		 *	block when we could have allocated from the end
		 *	of a larger one, but c'est la vie.
		 *
		 *   2. Prefer taking from right (high) end.  We start
		 *	with 64Kb aligned space, so prefer not to break
		 *	up the first chunk until we have to.  In any event,
		 *	reduce fragmentation by being consistent.
		 */
		if (dlp->lomem_size < nbytes ||
			(bestpred &&
			dlp->lomem_size > bestpred->lomem_next->lomem_size))
				continue;

		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
		leftrounded = ((left + attr->dma_attr_seg - 1) &
						~attr->dma_attr_seg);
		rightrounded = right & ~attr->dma_attr_seg;

		/*
		 * See if this block will work, either from left, from
		 * right, or after rounding up left to be on an "address
		 * increment" (dlim_adreg_max) boundary.
		 */
		if (lomem_check_limits(attr, right - nbytes, right - 1) ||
		    lomem_check_limits(attr, left, left + nbytes - 1) ||
		    (leftrounded + nbytes <= right &&
			lomem_check_limits(attr, leftrounded,
						leftrounded+nbytes-1))) {
			bestpred = pred;
		}
	}

	if (bestpred == NULL) {
		if (cansleep) {
			if (lomem_wanted == 0 || nbytes < lomem_wanted)
				lomem_wanted = nbytes;
			cv_wait(&lomem_cv, &lomem_lock);
			goto again;
		}
		mutex_exit(&lomem_lock);
		kmem_free(dlpu, sizeof (struct lomemlist));
		kmem_free(dlpf, sizeof (struct lomemlist));
		return (NULL);
	}

	/* bestpred is predecessor of block we're going to take from */
	dlp = bestpred->lomem_next;

	if (dlp->lomem_size == nbytes) {
		/* Perfect fit.  Just use whole block. */
		ASSERT(lomem_check_limits(attr,  dlp->lomem_paddr,
				dlp->lomem_paddr + dlp->lomem_size - 1));
		bestpred->lomem_next = dlp->lomem_next;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	} else {
		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
		leftrounded = ((left + attr->dma_attr_seg - 1) &
						~attr->dma_attr_seg);
		rightrounded = right & ~attr->dma_attr_seg;

		if (lomem_check_limits(attr, right - nbytes, right - 1)) {
			/* Take from right end */
			dlpu->lomem_paddr = right - nbytes;
			dlp->lomem_size -= nbytes;
		} else if (lomem_check_limits(attr, left, left+nbytes-1)) {
			/* Take from left end */
			dlpu->lomem_paddr = left;
			dlp->lomem_paddr += nbytes;
			dlp->lomem_size -= nbytes;
		} else if (rightrounded - nbytes >= left &&
			lomem_check_limits(attr, rightrounded - nbytes,
							rightrounded - 1)) {
			/* Take from right after rounding down */
			dlpu->lomem_paddr = rightrounded - nbytes;
			dlpf->lomem_paddr = rightrounded;
			dlpf->lomem_size  = right - rightrounded;
			dlp->lomem_size -= (nbytes + dlpf->lomem_size);
			dlpf->lomem_next = dlp->lomem_next;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		} else {
			ASSERT(leftrounded + nbytes <= right &&
				lomem_check_limits(attr, leftrounded,
						leftrounded + nbytes - 1));
			/* Take from left after rounding up */
			dlpu->lomem_paddr = leftrounded;
			dlpf->lomem_paddr = leftrounded + nbytes;
			dlpf->lomem_size  = right - dlpf->lomem_paddr;
			dlpf->lomem_next  = dlp->lomem_next;
			dlp->lomem_size = leftrounded - dlp->lomem_paddr;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		}
		dlp = dlpu;
		dlpu = NULL;	/* Don't free it */
		dlp->lomem_size = nbytes;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	}

	if ((lomemused += nbytes) > lomemmaxused)
		lomemmaxused = lomemused;

	mutex_exit(&lomem_lock);

	if (dlpu) kmem_free(dlpu, sizeof (struct lomemlist));
	if (dlpf) kmem_free(dlpf, sizeof (struct lomemlist));

#ifdef DEBUG
	if (lomem_debug) {
		printf("lomem_alloc: alloc paddr 0x%x size %d\n",
		    (int)dlp->lomem_paddr, (int)dlp->lomem_size);
	}
#endif /* DEBUG */
	return (lomem_startva + (dlp->lomem_paddr - lomem_startpa));
}

static int
lomem_check_limits(ddi_dma_attr_t *attr, uint_t lo, uint_t hi)
{
	return (lo >= attr->dma_attr_addr_lo && hi <= attr->dma_attr_addr_hi &&
		((hi & ~(attr->dma_attr_seg)) == (lo & ~(attr->dma_attr_seg))));
}

void
lomem_free(caddr_t kaddr)
{
	struct lomemlist *dlp, *pred, *dlpf;
	ulong_t paddr;

	/* make sure lomem_init() has been called */
	ASSERT(lomem_endva != 0);


	/* Convert kaddr from virtual to physical */
	paddr = (kaddr - lomem_startva) + lomem_startpa;

	mutex_enter(&lomem_lock);

	/* Find the allocated block in the used list */
	for (pred = &lomemusedlist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp)
		if (dlp->lomem_paddr == paddr)
			break;

	if ((dlp == NULL) || (dlp->lomem_paddr != paddr)) {
		cmn_err(CE_WARN, "lomem_free: bad addr=0x%x paddr=0x%x\n",
			(int)kaddr, (int)paddr);
		mutex_exit(&lomem_lock);
		return;
	}

	lomemused -= dlp->lomem_size;

	/* Remove from used list */
	pred->lomem_next = dlp->lomem_next;

	/* Insert/merge into free list */
	for (pred = &lomemfreelist; (dlpf = pred->lomem_next) != NULL;
	    pred = dlpf) {
		if (paddr <= dlpf->lomem_paddr)
			break;
	}

	/* Insert after pred; dlpf may be NULL */
	if (pred->lomem_paddr + pred->lomem_size == dlp->lomem_paddr) {
		/* Merge into pred */
		pred->lomem_size += dlp->lomem_size;
		kmem_free(dlp, sizeof (struct lomemlist));
	} else {
		/* Insert after pred */
		dlp->lomem_next = dlpf;
		pred->lomem_next = dlp;
		pred = dlp;
	}

	if (dlpf &&
		pred->lomem_paddr + pred->lomem_size == dlpf->lomem_paddr) {
		pred->lomem_next = dlpf->lomem_next;
		pred->lomem_size += dlpf->lomem_size;
		kmem_free(dlpf, sizeof (struct lomemlist));
	}

	if (pred->lomem_size >= lomem_wanted) {
		lomem_wanted = 0;
		cv_broadcast(&lomem_cv);
	}

	mutex_exit(&lomem_lock);

#ifdef DEBUG
	if (lomem_debug) {
		printf("lomem_free: freeing addr 0x%x -> addr=0x%x, size=%d\n",
		    (int)paddr, (int)pred->lomem_paddr, (int)pred->lomem_size);
	}
#endif /* DEBUG */
}

caddr_t
i86devmap(pfn_t pf, pgcnt_t npf, uint_t prot)
{
	caddr_t addr;
	page_t *pp;
	caddr_t addr1;

	addr1 = addr = vmem_alloc(heap_arena, ptob(npf), VM_SLEEP);

	for (; npf != 0; addr += MMU_PAGESIZE, ++pf, npf--) {
		if ((pp = page_numtopp_nolock(pf)) == NULL)
			hat_devload(kas.a_hat,  addr,
			    MMU_PAGESIZE, pf, prot|HAT_NOSYNC, HAT_LOAD_LOCK);
		else
			hat_memload(kas.a_hat, addr, pp,
			    prot|HAT_NOSYNC, HAT_LOAD_LOCK);
	}

	return (addr1);
}

/*
 * This routine is like page_numtopp, but accepts only free pages, which
 * it allocates (unfrees) and returns with the exclusive lock held.
 * It is used by machdep.c/dma_init() to find contiguous free pages.
 */
page_t *
page_numtopp_alloc(pfn_t pfnum)
{
	page_t *pp;

retry:
	pp = page_numtopp_nolock(pfnum);
	if (pp == NULL) {
		return (NULL);
	}

	if (!page_trylock(pp, SE_EXCL)) {
		return (NULL);
	}

	if (page_pptonum(pp) != pfnum) {
		page_unlock(pp);
		goto retry;
	}

	if (!PP_ISFREE(pp)) {
		page_unlock(pp);
		return (NULL);
	}

	/* If associated with a vnode, destroy mappings */

	if (pp->p_vnode) {

		page_destroy_free(pp);

		if (!page_lock(pp, SE_EXCL, (kmutex_t *)NULL, P_NO_RECLAIM)) {
			return (NULL);
		}

		if (page_pptonum(pp) != pfnum) {
			page_unlock(pp);
			goto retry;
		}
	}

	if (!PP_ISFREE(pp) || !page_reclaim(pp, (kmutex_t *)NULL)) {
		page_unlock(pp);
		return (NULL);
	}

	return (pp);
}

/* check if the buffer is allocated from lomem pages */
int
islomembuf(void *buf)
{
	if (((caddr_t)buf < lomem_endva) && ((caddr_t)buf >= lomem_startva))
		return (1);

	return (0);
}
