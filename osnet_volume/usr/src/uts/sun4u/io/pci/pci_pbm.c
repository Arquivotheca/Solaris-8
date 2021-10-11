/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_pbm.c	1.10	99/11/15 SMI"

/*
 * PCI PBM implementation:
 *	initialization
 *	Bus error interrupt handler
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/spl.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/ivintr.h>
#include <sys/async.h>		/* register_bus_func() */
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/

static uint_t pbm_error_intr(caddr_t a);

void
pbm_create(pci_t *pci_p)
{
	pbm_t *pbm_p;
	int i;
	int nrange = pci_p->pci_ranges_length / sizeof (pci_ranges_t);
	dev_info_t *dip = pci_p->pci_dip;
	pci_ranges_t *rangep = pci_p->pci_ranges;
	uint64_t base_addr, last_addr;
	ib_ino_t ino = IB_MONDO_TO_INO(pci_p->pci_interrupts[0]);
	ib_t *ib_p = pci_p->pci_ib_p;

#ifdef lint
	dip = dip;
#endif

	/*
	 * Allocate a state structure for the PBM and cross-link it
	 * to its per pci node state structure.
	 */
	pbm_p = (pbm_t *)kmem_zalloc(sizeof (pbm_t), KM_SLEEP);
	pci_p->pci_pbm_p = pbm_p;
	pbm_p->pbm_pci_p = pci_p;

	pci_pbm_setup(pbm_p);

	/* XXX sabre specific XXX */
	pbm_p->pbm_dma_sync_reg = (uint64_t *)(((caddr_t)pbm_p->pbm_ctrl_reg) +
		DMA_WRITE_SYNC_REG);
	/*
	 * Get this pbm's mem32 and mem64 segments to determine whether
	 * a dma object originates from ths pbm. i.e. dev to dev dma
	 */
	/* Init all of our boundaries */
	base_addr = 0xffffffffffffffffull;
	last_addr = 0ull;

	for (i = 0; i < nrange; i++, rangep++) {
		uint32_t rng_type = rangep->child_high & PCI_ADDR_MASK;
		if (rng_type == PCI_ADDR_MEM32 || rng_type == PCI_ADDR_MEM64) {
			uint64_t rng_addr, rng_size;

			rng_addr = (uint64_t)rangep->parent_high << 32;
			rng_addr |= (uint64_t)rangep->parent_low;
			rng_size = (uint64_t)rangep->size_high << 32;
			rng_size |= (uint64_t)rangep->size_low;
			base_addr = MIN(rng_addr, base_addr);
			last_addr = MAX(rng_addr + rng_size, last_addr);
		}
	}
	pbm_p->pbm_base_pfn = mmu_btop(base_addr);
	pbm_p->pbm_last_pfn = mmu_btop(last_addr);

	DEBUG4(DBG_ATTACH, dip,
		"pbm_create: ctrl=%x, afsr=%x, afar=%x, diag=%x\n",
		pbm_p->pbm_ctrl_reg, pbm_p->pbm_async_flt_status_reg,
		pbm_p->pbm_async_flt_addr_reg, pbm_p->pbm_diag_reg);
	DEBUG1(DBG_ATTACH, dip, "pbm_create: conf=%x\n",
		pbm_p->pbm_config_header);

	/*
	 * Get the mondo and pil for PCI bus error interrupts.
	 */
	pbm_p->pbm_bus_error_mondo = IB_MAKE_MONDO(ib_p, ino);

	/*
	 * Register a function to disable pbm error interrupts during a panic.
	 */
	register_bus_func(DIS_ERR_FTYPE, (afunc)pbm_disable_pci_errors,
		(caddr_t)pbm_p);

	pbm_configure(pbm_p);
}

void
pbm_register_intr(pbm_t *pbm_p)
{
	pci_t *pci_p = pbm_p->pbm_pci_p;
	dev_info_t *dip = pci_p->pci_dip;

	DEBUG2(DBG_ATTACH, dip,
		"pbm_register_intr: bus err mondo=%x pil=%x\n",
		pbm_p->pbm_bus_error_mondo, pbm_p->pbm_iblock_cookie);

	IB_INO_INTR_CLEAR(ib_clear_intr_reg_addr(pbm_p->pbm_pci_p->pci_ib_p,
		IB_MONDO_TO_INO(pbm_p->pbm_bus_error_mondo)));

	/*
	 * Install the PCI error interrupt handler.
	 */
	(void) ddi_add_intr(dip, 0, &pbm_p->pbm_iblock_cookie, NULL,
		pbm_error_intr, (caddr_t)pci_p);

	/*
	 * Create the pokefault mutext and flag.
	 */
	mutex_init(&pbm_p->pbm_pokefault_mutex, NULL, MUTEX_DRIVER,
		pbm_p->pbm_iblock_cookie);
}

void
pbm_destroy(pci_t *pci_p)
{
	dev_info_t *dip = pci_p->pci_dip;
	pbm_t *pbm_p = pci_p->pci_pbm_p;
	ib_t *ib_p = pci_p->pci_ib_p;
	ib_ino_t ino;

	unregister_bus_func((caddr_t)pbm_p);

	/*
	 * Free the pokefault mutex.
	 */
	DEBUG0(DBG_DETACH, dip, "pbm_destroy:\n");
	mutex_destroy(&pbm_p->pbm_pokefault_mutex);

	/*
	 * Remove the pci error interrupt handler.
	 */
	ino = IB_MONDO_TO_INO(pbm_p->pbm_bus_error_mondo);
	ib_intr_disable(ib_p, ino, IB_INTR_WAIT);
	ddi_remove_intr(dip, 0, NULL);

	/*
	 * Free the pbm state structure.
	 */
	kmem_free(pbm_p, sizeof (pbm_t));
	pci_p->pci_pbm_p = NULL;
}

void
pbm_enable_intr(pbm_t *pbm_p)
{
	dev_info_t *dip = pbm_p->pbm_pci_p->pci_dip;
	ib_ino_t ino = IB_MONDO_TO_INO(pbm_p->pbm_bus_error_mondo);
	ib_intr_enable(pbm_p->pbm_pci_p->pci_ib_p, dip, ino);
}

static uint_t
pbm_error_intr(caddr_t a)
{
	pci_t *pci_p = (pci_t *)a;
	pbm_t *pbm_p = pci_p->pci_pbm_p;
	struct pci_fault_handle *fhp;
	int nerr = 0;

	ddi_nofault_data_t *nofault_data = pbm_p->nofault_data;
	if (nofault_data && (nofault_data->op_type == POKE_START)) {
		int pf_cnt = 0;
		for (fhp = pci_p->pci_fh_lst; fhp; fhp = fhp->fh_next) {
			DEBUG2(DBG_ERR_INTR, pci_p->pci_dip,
				"examing pokefault on %s-%d\n",
				ddi_driver_name(fhp->fh_dip),
				ddi_get_instance(fhp->fh_dip));
			if ((*fhp->fh_f)(FAULT_POKEFLT, fhp->fh_arg)) {
#ifdef lint
				nerr = nerr;
#endif
				DEBUG2(DBG_ERR_INTR, pci_p->pci_dip,
					"pokefault: non pokefault on %s-%d\n",
					ddi_driver_name(fhp->fh_dip),
					ddi_get_instance(fhp->fh_dip));
			} else {
				DEBUG2(DBG_ERR_INTR, pci_p->pci_dip,
					"pokefault: logged on %s-%d\n",
					ddi_driver_name(fhp->fh_dip),
					ddi_get_instance(fhp->fh_dip));
				pf_cnt++;
			}
		}

		DEBUG1(DBG_ERR_INTR, pci_p->pci_dip,
			"pokefault: %d devices reported pokefault\n", pf_cnt);

		if (pf_cnt == 0) /* nobody reported poke fault */
			goto real_fault;

		for (fhp = pci_p->pci_fh_lst; fhp; fhp = fhp->fh_next) {
			DEBUG2(DBG_ERR_INTR, pci_p->pci_dip,
				"cleaning up pokefault on %s-%d\n",
				ddi_driver_name(fhp->fh_dip),
				ddi_get_instance(fhp->fh_dip));
			(void) (*fhp->fh_f)(FAULT_POKEFINI, fhp->fh_arg);
		}

		DEBUG0(DBG_ERR_INTR, pci_p->pci_dip, "all pokefault cleared\n");

		/* inform the poke framework the poke has faulted */
		nofault_data->op_type = POKE_FAULT;

		DEBUG0(DBG_ERR_INTR, pci_p->pci_dip, "pokefault handled\n");
		goto done;
	}
real_fault:
	for (fhp = pci_p->pci_fh_lst; fhp; fhp = fhp->fh_next) {
		DEBUG2(DBG_ERR_INTR, pci_p->pci_dip,
			"examing real fault on %s-%d\n",
			ddi_driver_name(fhp->fh_dip),
			ddi_get_instance(fhp->fh_dip));
		nerr += (*fhp->fh_f)(FAULT_LOG, fhp->fh_arg);
	}

	if (nerr) {
		dev_info_t *dip = pci_p->pci_dip;
		cmn_err(pci_panic_on_fatal_errors ? CE_PANIC : CE_WARN,
			"%s-%d: PCI bus %d error(s)!\n",
			ddi_driver_name(dip), ddi_get_instance(dip), nerr);
	}

	for (fhp = pci_p->pci_fh_lst; fhp; fhp = fhp->fh_next) {
		DEBUG2(DBG_ERR_INTR, pci_p->pci_dip,
			"clearing real fault on %s-%d\n",
			ddi_driver_name(fhp->fh_dip),
			ddi_get_instance(fhp->fh_dip));
		(void) (*fhp->fh_f)(FAULT_RESET, fhp->fh_arg);
	}

	cmn_err(CE_CONT, "No fatal PCI bus error(s)\n");
done:
	ib_intr_clear(pci_p->pci_ib_p,
		IB_MONDO_TO_INO(pbm_p->pbm_bus_error_mondo));

	return (DDI_INTR_CLAIMED);
}

/*
 * pbm_has_pass_1_cheerio
 *
 *
 * Given a PBM soft state pointer, this routine scans it child nodes
 * to see if one is a pass 1 cheerio.
 *
 * return value: 1 if pass 1 cheerio is found, 0 otherwise
 */
int
pbm_has_pass_1_cheerio(pci_t *pci_p)
{
	dev_info_t *cdip;
	int found = 0;
	char *s;
	int rev;

	cdip = ddi_get_child(pci_p->pci_dip);
	while (cdip != NULL && found == 0) {
		s = ddi_get_name(cdip);
		if (strcmp(s, "ebus") == 0 || strcmp(s, "pci108e,1000") == 0) {
			rev =
			    ddi_getprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
				"revision-id", 0);
			if (rev == 0)
				found = 1;
		}
		cdip = ddi_get_next_sibling(cdip);
	}
	return (found);
}
