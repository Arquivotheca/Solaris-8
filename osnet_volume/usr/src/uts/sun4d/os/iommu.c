/*
 * Copyright (c) 1990, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)iommu.c	1.24	97/03/14 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <vm/hat.h>

#include <sys/machparam.h>
#include <sys/iommu.h>

struct sbus_private sbus_pri_data[MX_SBUS];

/*
 * Deafault is autoconfigure for now.
 */
u_int slot_burst_patch[MX_SBUS][MX_SBUS_SLOTS] = {
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},

	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},

	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0}
};

int nsbus;

void
iommu_init(iommu_pte_t *va_xpt)
{
	int i;

	/* clear all iommu ptes */
	for (i = 0; i < IOMMU_DVMA_RANGE/IOMMU_PAGE_SIZE; va_xpt++, i++) {
		va_xpt->iopte = 0;
	}
}

int
iommu_pteload(iommu_pte_t *piopte, u_int mempfn, u_int flag)
{
	iommu_pte_t tmp_pte;

	if (piopte == NULL) {
		printf("iommu_pteload: no iopte!\n");
		return (-1);
	}

	tmp_pte.iopte = 0;

	tmp_pte.iopte = (mempfn << IOPTE_PFN_SHIFT) | IOPTE_VALID | flag;
	*piopte = tmp_pte;

	return (0);
}

int
iommu_unload(iommu_pte_t *va_xpt, caddr_t dvma_addr, int npf)
{
	iommu_pte_t *piopte;

	if ((piopte = iommu_ptefind(va_xpt, dvma_addr)) == NULL) {
		printf("iommu_unload: no iopte! dvma_addr = 0x%p\n",
					(void *)dvma_addr);
		return (-1);
	}
	/* in-line pteunload. invalid iopte */
	while (npf-- > 0)
#ifdef DEBUG
		piopte++->iopte &= (~IOPTE_VALID);
#else DEBUG
		piopte++->iopte = 0;
#endif DEBUG

	return (0);
}

int
iommu_pteunload(iommu_pte_t *piopte)
{
#ifdef DEBUG
	piopte->iopte &= (~IOPTE_VALID);
#else DEBUG
	piopte->iopte = 0;
#endif DEBUG

	return (0);
}

iommu_pte_t *
iommu_ptefind(iommu_pte_t *va_xpt, caddr_t dvma_addr)
{
	u_int dvma_pfn;

	ASSERT(dvma_addr >= (caddr_t)IOMMU_DVMA_BASE);

	dvma_pfn = iommu_btop(dvma_addr - (caddr_t)IOMMU_DVMA_BASE);
	return (&va_xpt[dvma_pfn]);
}
