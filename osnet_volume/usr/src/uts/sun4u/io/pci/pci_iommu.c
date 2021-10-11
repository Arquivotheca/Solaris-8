/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_iommu.c	1.9	99/12/03 SMI"

/*
 * PCI iommu initialization and configuration
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/async.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/vmem.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/

static void iommu_tsb_free(iommu_t *iommu_p);

void
iommu_create(pci_t *pci_p)
{
	dev_info_t *dip = pci_p->pci_dip;
	iommu_t *iommu_p;
	uintptr_t a;
	dvma_addr_t dvma_pg;
	int i;
	size_t size;

	uint32_t *dvma_prop;
	int dvma_prop_len;
	extern uint64_t va_to_pa(void *);

	/*
	 * Allocate iommu state structure and link it to the
	 * pci state structure.
	 */
	iommu_p = (iommu_t *)kmem_zalloc(sizeof (iommu_t), KM_SLEEP);
	pci_p->pci_iommu_p = iommu_p;
	iommu_p->iommu_pci_p = pci_p;

	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
			"virtual-dma", (caddr_t)&dvma_prop, &dvma_prop_len)
			== DDI_SUCCESS) {
		pci_iommu_dvma_end = dvma_prop[0] + (dvma_prop[1] - 1);
		kmem_free((caddr_t)dvma_prop, dvma_prop_len);
	}

	/*
	 * Invalidate the spare tsb field.
	 */
	iommu_p->iommu_spare_slot = MAX_UPA;

	a = pci_iommu_setup(iommu_p);

	/*
	 * Determine the virtual address of iommu registers.
	 */
	iommu_p->iommu_ctrl_reg =
		(uint64_t *)(a + COMMON_IOMMU_CTRL_REG_OFFSET);
	iommu_p->iommu_tsb_base_addr_reg =
		(uint64_t *)(a + COMMON_IOMMU_TSB_BASE_ADDR_REG_OFFSET);
	iommu_p->iommu_flush_page_reg =
		(uint64_t *)(a + COMMON_IOMMU_FLUSH_PAGE_REG_OFFSET);
	iommu_p->iommu_tlb_tag_diag_acc =
		(uint64_t *)(a + COMMON_IOMMU_TLB_TAG_DIAG_ACC_OFFSET);
	iommu_p->iommu_tlb_data_diag_acc =
		(uint64_t *)(a + COMMON_IOMMU_TLB_DATA_DIAG_ACC_OFFSET);

	/*
	 * Configure the rest of the iommu parameters.
	 */
	iommu_p->iommu_tsb_entries = (1 << iommu_p->iommu_tsb_size) * 1024;
	iommu_p->iommu_tsb_paddr = va_to_pa((caddr_t)iommu_p->iommu_tsb_vaddr);
	iommu_p->iommu_dvma_cache_locks =
		kmem_zalloc(pci_dvma_page_cache_entries, KM_SLEEP);

	iommu_p->iommu_dvma_base = pci_iommu_dvma_end + 1
		- (iommu_p->iommu_tsb_entries * IOMMU_PAGE_SIZE);
	iommu_p->dvma_base_pg = IOMMU_BTOP(iommu_p->iommu_dvma_base);
	iommu_p->iommu_dvma_reserve = iommu_p->iommu_tsb_entries >> 1;
	iommu_p->iommu_dvma_end = pci_iommu_dvma_end;
	iommu_p->dvma_end_pg = IOMMU_BTOP(iommu_p->iommu_dvma_end);
	iommu_p->iommu_dma_bypass_base = COMMON_IOMMU_BYPASS_BASE;
	iommu_p->iommu_dma_bypass_end = COMMON_IOMMU_BYPASS_END;

	DEBUG2(DBG_ATTACH, dip, "iommu_create: ctrl=%x, tsb=%x\n",
		iommu_p->iommu_ctrl_reg, iommu_p->iommu_tsb_base_addr_reg);
	DEBUG2(DBG_ATTACH, dip, "iommu_create: page_flush=%x, ctx_flush=%x\n",
		iommu_p->iommu_flush_page_reg, iommu_p->iommu_flush_ctx_reg);
	DEBUG2(DBG_ATTACH, dip, "iommu_create: allocated vaddr=%x size=%x\n",
		iommu_tsb_vaddr[pci_p->pci_id],
		iommu_tsb_alloc_size[pci_p->pci_id]);
	DEBUG2(DBG_ATTACH, dip, "iommu_create: tsb vaddr=%x tsb_paddr=%x\n",
		iommu_p->iommu_tsb_vaddr, iommu_p->iommu_tsb_paddr);
	DEBUG3(DBG_ATTACH, dip,
		"iommu_create: tsb size=%x, tsb entries=%x, dvma base=%x\n",
		iommu_p->iommu_tsb_size, iommu_p->iommu_tsb_entries,
		iommu_p->iommu_dvma_base);
	DEBUG2(DBG_ATTACH, dip,
		"iommu_create: dvma_cache_locks=%x cache_entries=%x\n",
		iommu_p->iommu_dvma_cache_locks, pci_dvma_page_cache_entries);

	/*
	 * Invalidate all entries in the iommu tsb.
	 * XXX cannot do this if we need to support USB - shao
	 */
	dvma_pg = iommu_p->dvma_base_pg;
	for (i = 0; i < iommu_p->iommu_tsb_entries; i++)
		IOMMU_UNLOAD_TTE(iommu_p, dvma_pg++);

	/*
	 * Create a virtual memory map for dvma address space.
	 * Reserve 'size' bytes of low dvma space for fast track cache.
	 */
	(void) snprintf(iommu_p->iommu_dvma_map_name,
	    sizeof (iommu_p->iommu_dvma_map_name), "%s%d_dvma",
	    ddi_driver_name(dip), ddi_get_instance(dip));

	size = IOMMU_PTOB(pci_dvma_page_cache_entries *
	    pci_dvma_page_cache_clustsz);

	iommu_p->iommu_dvma_map = vmem_create(iommu_p->iommu_dvma_map_name,
	    (void *)(iommu_p->iommu_dvma_base + size),
	    IOMMU_PTOB(iommu_p->iommu_tsb_entries) - size, PAGESIZE,
	    NULL, NULL, NULL, 0, VM_SLEEP);

	if (pci_preserve_iommu_tsb)
		iommu_preserve_tsb(iommu_p);

	/*
	 * Now that the iommu instance is created, call it's configuration
	 * routine.
	 */
	iommu_configure(iommu_p);
}

void
iommu_destroy(pci_t *pci_p)
{
#ifdef DEBUG
	dev_info_t *dip = pci_p->pci_dip;
#endif
	iommu_t *iommu_p = pci_p->pci_iommu_p;

	DEBUG0(DBG_DETACH, dip, "iommu_destroy:\n");

	/*
	 * Return the boot time allocated tsb.
	 */
	iommu_tsb_free(iommu_p);

	/*
	 * Teardown any implementation-specific structures set up in
	 * pci_iommu_setup.
	 */
	pci_iommu_teardown(iommu_p);

	/*
	 * Free the dvma resource map.
	 */
	vmem_destroy(iommu_p->iommu_dvma_map);

	/*
	 * Fee the iommu state structure.
	 */
	kmem_free(iommu_p, sizeof (iommu_t));
	pci_p->pci_iommu_p = NULL;
}

void
iommu_configure(iommu_t *iommu_p)
{
	int i;

	if (pci_preserve_iommu_tsb) {
		iommu_obp_to_kernel(iommu_p);
		return;
	}

	/*
	 * Invalidate the iommu TLB entries using the diagnostic access
	 * registers.
	 */
	*iommu_p->iommu_ctrl_reg = COMMON_IOMMU_CTRL_DIAG_ENABLE;
	for (i = 0; i < COMMON_IOMMU_TLB_SIZE; i++) {
		*iommu_p->iommu_tlb_tag_diag_acc = 0x0ull;
		*iommu_p->iommu_tlb_data_diag_acc = 0x0ull;
	}
	*iommu_p->iommu_ctrl_reg &= ~COMMON_IOMMU_CTRL_DIAG_ENABLE;

	/*
	 * Configure the iommu.
	 */
	*iommu_p->iommu_tsb_base_addr_reg = iommu_p->iommu_tsb_paddr;
	*iommu_p->iommu_ctrl_reg = (uint64_t)
		((iommu_p->iommu_tsb_size << COMMON_IOMMU_CTRL_TSB_SZ_SHIFT) |
			(0 /* 8k page */ << COMMON_IOMMU_CTRL_TBW_SZ_SHIFT) |
			COMMON_IOMMU_CTRL_ENABLE |
			(pci_lock_tlb ? COMMON_IOMMU_CTRL_LCK_ENABLE : 0));
}

/*
 * iommu_get_dvma_pages
 *
 * The routines allocates npages of dvma space between the addresses
 * of addrlo and addrhi.  If no such space is available, the routine
 * will sleep based on the value on cansleep.
 *
 * used by: pci_dma_map(), pci_dma_bindhdl() and
 *		pci_dma_mctl() - DDI_DMA_RESERVE
 *
 * return value: dvma page frame number on success, zero on error
 */
dvma_addr_t
iommu_get_dvma_pages(iommu_t *iommu_p, size_t npages, dvma_addr_t addrlo,
	dvma_addr_t addrhi, dvma_addr_t align, uint32_t cntr_max, int cansleep)
{

	/*
	 * The minimum unit of allocation for dvma space is one page
	 * so make sure a devices dma counter restrictions have a minimum
	 * alignment of PAGESIZE
	 */
	cntr_max &= ~(IOMMU_PAGE_SIZE - 1);

	return (IOMMU_BTOP((dvma_addr_t)vmem_xalloc(iommu_p->iommu_dvma_map,
	    IOMMU_PTOB(npages), MAX(align, IOMMU_PAGE_SIZE), 0, cntr_max,
	    (void *)addrlo, (void *)(addrhi + 1),
	    cansleep ? VM_SLEEP : VM_NOSLEEP)));
}

void
iommu_free_dvma_pages(iommu_t *iommu_p, size_t npages, dvma_addr_t dvma_addr)
{
	iopfn_t dvma_pfn = IOMMU_BTOP(dvma_addr);
	iopfn_t index = (dvma_pfn - iommu_p->dvma_base_pg)
		/ pci_dvma_page_cache_clustsz;
	if (index >= pci_dvma_page_cache_entries) {
		vmem_free(iommu_p->iommu_dvma_map,
		    (void *)(dvma_addr & IOMMU_PAGE_MASK), IOMMU_PTOB(npages));
		pci_dvma_map_free_cnt++;
	} else {
		iommu_p->iommu_dvma_cache_locks[index] = 0;
		pci_dvma_cache_free_cnt++;
	}
}

void
iommu_map_pages(iommu_t *iommu_p, ddi_dma_impl_t *mp,
		dvma_addr_t dvma_pg, size_t npages, size_t pfn_index)
{
	pci_dmai_nexus_private_t *np = (pci_dmai_nexus_private_t *)
		mp->dmai_nexus_private;
	int i, red_zone = HAS_REDZONE(mp);
	dvma_addr_t pg_index = dvma_pg - iommu_p->dvma_base_pg;
	size_t pfn_last = pfn_index + npages;

	ASSERT(np->tte);
	DEBUG5(DBG_MAP_WIN, iommu_p->iommu_pci_p->pci_dip,
		"iommu_map_pages:%x+%x=%x npages=0x%x pfn_index=0x%x\n",
		(uint_t)pg_index, (uint_t)iommu_p->dvma_base_pg,
		(uint_t)(pg_index + iommu_p->dvma_base_pg),
		(uint_t)npages, (uint_t)pfn_index);

	for (i = pfn_index; i < pfn_last; i++, pg_index++) {
		iopfn_t pfn = PCI_GET_MP_PFN(np, i);
		volatile uint64_t cur_tte = IOMMU_PTOB(pfn) | np->tte;
		ASSERT(TTE_IS_INVALID(iommu_p->iommu_tsb_vaddr[pg_index]));
		iommu_p->iommu_tsb_vaddr[pg_index] = cur_tte;

#ifdef RIO_HW_BUGID_4175971
		/*
		 * XXX usb target abort workaround, disabled to see
		 * if prom can fix this
		 */
		IOMMU_PAGE_FLUSH(iommu_p, dvma_pg + i - pfn_index);
#endif
		DEBUG3(DBG_MAP_WIN, iommu_p->iommu_pci_p->pci_dip,
			"map_pages: mp=%x pg[%x]=%x\n", mp, i, (uint_t)pfn);
		DEBUG3(DBG_MAP_WIN, iommu_p->iommu_pci_p->pci_dip,
			"map_pages: tsb_base=%x tte=%08x.%08x\n",
			iommu_p->iommu_tsb_vaddr, (uint_t)(cur_tte >> 32),
			(uint_t)cur_tte);
	}

	/* Set up the red zone if requested */
	if (red_zone) {
		DEBUG1(DBG_MAP_WIN, iommu_p->iommu_pci_p->pci_dip,
			"iommu_map_pages: redzone pg=%x\n", pg_index);
		ASSERT(TTE_IS_INVALID(iommu_p->iommu_tsb_vaddr[pg_index]));
		iommu_p->iommu_tsb_vaddr[pg_index] = COMMON_IOMMU_INVALID_TTE;
		IOMMU_PAGE_FLUSH(iommu_p, dvma_pg + npages);
	}

	if (pci_dvma_debug_on)
		pci_dvma_alloc_debug((char *)mp->dmai_mapping,
			mp->dmai_size, mp);
}


/*
 * iommu_map_window
 *
 * This routine is called to program a dvma window into the iommu.
 * Non partial mappings are viewed as single window mapping.
 *
 * used by: pci_dma_map(), pci_dma_bindhdl(), pci_dma_win(),
 *	and pci_dma_mctl() - DDI_DMA_MOVWIN, DDI_DMA_NEXTWIN
 *
 * return value: none
 */
/*ARGSUSED*/
void
iommu_map_window(iommu_t *iommu_p, ddi_dma_impl_t *mp, window_t window)
{
	size_t npages, size;

	/*
	 * XXX this code does not deal with non-page aligned winsize and
	 * XXX assumes mmu and iommu has the same page size
	 */
	dvma_addr_t dvma_pg = IOMMU_BTOP(mp->dmai_mapping);
	size_t dma_offset = window * mp->dmai_winsize;
	size = MIN(mp->dmai_winsize, mp->dmai_object.dmao_size - dma_offset);
	npages = mmu_btopr(size + (mp->dmai_mapping & IOMMU_PAGE_OFFSET));

	DEBUG5(DBG_MAP_WIN, iommu_p->iommu_pci_p->pci_dip,
		"window=%x dma_offset=%x (%x pages) size=%x npages=%x\n",
		window, dma_offset, dma_offset / IOMMU_PAGE_SIZE, size, npages);

	iommu_map_pages(iommu_p, mp, dvma_pg, npages, IOMMU_BTOP(dma_offset));

	mp->dmai_ndvmapages = npages;
	mp->dmai_size = size;
	mp->dmai_offset = dma_offset;
}


#define	MAKE_BYPASS_ADDR(iommu_p, pfn, offset, bypass_prefix) \
	(iommu_p->iommu_dma_bypass_base + \
		((pfn << MMU_PAGESHIFT) + offset) | bypass_prefix)

/*ARGSUSED*/
int
iommu_create_bypass_cookies(iommu_t *iommu_p, dev_info_t *dip,
	ddi_dma_impl_t *mp, ddi_dma_cookie_t *cookiep)
{
	pci_dmai_nexus_private_t *np = (pci_dmai_nexus_private_t *)
	    mp->dmai_nexus_private;
	ddi_dma_cookie_t *cp;
	uint_t size = mp->dmai_size;
	uint_t cc;
	size_t balance;
	uintptr_t offset;
	size_t npages;
	iopfn_t pfn;
	uint64_t counter_max, bypass_prefix;
	dma_bypass_addr_t addr;
	uint_t sgllen;
	int i;

	/*
	 * Get the dma counter attribute.  We need to make sure that
	 * the size of any single cookie doesn't exceed this value.
	 */
	counter_max = mp->dmai_attr.dma_attr_count_max;
	if (counter_max == 0)
		counter_max = 0xffffffffffffffffull;

	/*
	 * Get the dma scatter/gather list length attribute.  This
	 * determines how many cookies we will be able to create.
	 */
	sgllen = (uint_t)mp->dmai_attr.dma_attr_sgllen;
	DEBUG2(DBG_BYPASS, dip, "counter_max=%x sgllen=%x\n",
	    counter_max, sgllen);
	if (sgllen == 0) {
		DEBUG1(DBG_BYPASS, dip, "sgllen==%x, too small\n", sgllen);
		return (DDI_DMA_TOOBIG);
	}

	switch (mp->dmai_object.dmao_type) {
	case DMA_OTYP_VADDR:
		offset = (uintptr_t)mp->dmai_object.dmao_obj.virt_obj.v_addr &
			IOMMU_PAGE_OFFSET;
		break;
	case DMA_OTYP_PAGES:
		offset = mp->dmai_object.dmao_obj.pp_obj.pp_offset;
		break;
	default:
		ASSERT(0);
	}

	/* check_dma_mode() made sure we are not mixing mem and io ops */
	pfn = PCI_GET_MP_PFN(np, 0);
	DEBUG3(DBG_BYPASS, dip,
		"PCI_GET_MP_PFN: np=%x page_no=0 pfn=%08x.%08x\n",
		np, (uint_t)(pfn >> 32), (uint_t)pfn);
	bypass_prefix = pf_is_memory(pfn) ? 0 : COMMON_IOMMU_BYPASS_NONCACHE;
	addr = MAKE_BYPASS_ADDR(iommu_p, pfn, offset, bypass_prefix);
	balance = IOMMU_PAGE_SIZE - offset;
	npages = np->npages - 1;

	cp = mp->dmai_cookie = np->cookie_list;
	for (i = 1, cc = 1; size > balance && npages; i++, npages--) {
		iopfn_t prev_pfn = pfn;
		pfn = PCI_GET_MP_PFN(np, i);
		DEBUG4(DBG_BYPASS, dip,
			"PCI_GET_MP_PFN: np=%x page_no=%x pfn=%08x.%08x\n",
			np, i, (uint_t)(pfn >> 32), (uint_t)pfn);
		if (pfn != prev_pfn+1 || balance + MMU_PAGESIZE > counter_max) {
			/* need a new cookie */
			if (cc++ > sgllen) {
				DEBUG1(DBG_BYPASS, dip,
				    "sgllen exceeded - %x\n", cc);
				return (DDI_DMA_TOOBIG);
			}

			MAKE_DMA_COOKIE(cp, addr, balance);
			size -= balance;
			balance = 0;
			DEBUG3(DBG_BYPASS, dip, "cookie (%x,%x,%x)\n",
				cp->dmac_notused, cp->dmac_address,
				cp->dmac_size);

			cp++; /* next cookie */

			/* non-first pages always has offset == 0 */
			addr = MAKE_BYPASS_ADDR(iommu_p, pfn, 0, bypass_prefix);
		}
		balance += MMU_PAGESIZE;
	}

	/*
	 * Finish up the last cookie.
	 */
	MAKE_DMA_COOKIE(cp, addr, size);
	PCI_SET_NP_NCOOKIES(np, cc);
	DEBUG4(DBG_BYPASS, dip, "last cookie (%x,%x,%x), count=%x\n",
	    cp->dmac_notused, cp->dmac_address, cp->dmac_size, cc);

	/*
	 * Fill in the caller's cookie with the first cookie, and set
	 * mp->dmai_cookie to point to the next cookie.
	 */
	*cookiep = *mp->dmai_cookie++;
	return (0);
}

int
iommu_tsb_alloc(iommu_t *iommu_p)
{
	uint8_t i;
	uint8_t largest_i;
	int largest;

	if (PCI_VALID_ID(iommu_p->iommu_spare_slot)) {
		return (iommu_tsb_alloc_size[iommu_p->iommu_spare_slot]);
	}

	for (i = 0, largest = 0, largest_i = 0; i < MAX_UPA; i++) {
		if (iommu_tsb_spare[i] == IOMMU_TSB_ISASPARE) {
			if (iommu_tsb_alloc_size[i] > largest) {
				largest_i = i;
				largest = iommu_tsb_alloc_size[largest_i];
			}
		}
	}
	if (largest) {
		iommu_tsb_spare[largest_i] |= IOMMU_TSB_ISUNAVAILABLE;
		iommu_p->iommu_spare_slot = largest_i;
	}
	return (largest);
}

static void
iommu_tsb_free(iommu_t *iommu_p)
{
	if (iommu_p->iommu_spare_slot != -1) {
		iommu_tsb_spare[iommu_p->iommu_spare_slot] &=
			~IOMMU_TSB_ISUNAVAILABLE;
		iommu_p->iommu_spare_slot = (uint8_t)-1;
	} else {
		iommu_tsb_spare[iommu_p->iommu_pci_p->pci_id] =
			IOMMU_TSB_ISASPARE;
	}
}
