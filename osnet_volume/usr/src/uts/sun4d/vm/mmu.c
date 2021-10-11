/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)mmu.c	1.32	96/06/24 SMI"


#include <sys/types.h>
#include <sys/param.h>
#include <sys/pte.h>
#include <sys/proc.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/hat_srmmu.h>
#include <vm/seg_kmem.h>

struct pte mmu_pteinvalid = {
	0,		/* page number */
	0,		/* cacheable */
	0,		/* modified */
	0,		/* referenced */
	0,		/* Access permissions */
	MMU_ET_INVALID	/* pte type */
};

/* Only here for compatibility */

void
mmu_setpte(base, pte)
	caddr_t base;
	struct pte pte;
{
	srmmu_devload(kas.a_hat, &kas, base, NULL, pte.PhysicalPageNumber,
		srmmu_ptov_prot(&pte) | HAT_NOSYNC, HAT_LOAD);
}

void
mmu_getpte(base, ppte)
	caddr_t base;
	struct pte *ppte;
{
	struct pte *tmp;
	struct ptbl *ptbl;
	int level;
	union ptpe tmp2;
	struct as *pas;
	kmutex_t *mtx;
	int off;

	if ((u_int) base >= KERNELBASE)
		pas = &kas;
	else
		pas = curproc->p_as;

	tmp = srmmu_ptefind(pas, base, &level, &ptbl, &mtx, LK_PTBL_SHARED);
	mmu_readpte(tmp, &tmp2.ptpe_int);
	unlock_ptbl(ptbl, mtx);

	if (pte_valid(&tmp2.pte)) {
		switch (level) {
		case 3:
			*ppte = tmp2.pte;
			return;

		case 2:
			off = (u_int)base & 0x3FFFF;
			off >>= MMU_PAGESHIFT;
			tmp2.pte.PhysicalPageNumber += off;
			*ppte = tmp2.pte;
			return;

		case 1:
			off = (u_int)base & 0xFFFFFF;
			off >>= MMU_PAGESHIFT;
			tmp2.pte.PhysicalPageNumber += off;
			*ppte = tmp2.pte;
			return;
		}
	} else {
		tmp2.ptpe_int = 0;
		*ppte = tmp2.pte;
	}
}

void
mmu_getkpte(base, ppte)
	caddr_t base;
	struct pte *ppte;
{
	mmu_getpte(base, ppte);
}
