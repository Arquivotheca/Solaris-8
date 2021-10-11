/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_ecc.c	1.15	99/11/15 SMI"

/*
 * PCI ECC support
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/sunddi.h>
#include <sys/ivintr.h>
#include <sys/async.h>		/* struct async_flt */
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/

static void ecc_err_callback(ecc_t *ecc_p);
static uint_t ecc_intr(caddr_t a);
static void ecc_disable(ecc_t *ecc_p, int wait);

void
ecc_create(pci_t *pci_p)
{
#ifdef DEBUG
	dev_info_t *dip = pci_p->pci_dip;
#endif
	ecc_t *ecc_p;
	uintptr_t a;

	ecc_p = (ecc_t *)kmem_zalloc(sizeof (ecc_t), KM_SLEEP);
	ecc_p->ecc_pci_p = pci_p;
	pci_p->pci_ecc_p = ecc_p;

	ecc_p->ecc_ue.ecc_p = ecc_p;
	ecc_p->ecc_ue.ecc_type = PCI_ECC_UE;
	ecc_p->ecc_ce.ecc_p = ecc_p;
	ecc_p->ecc_ce.ecc_type = PCI_ECC_CE;

	a = pci_ecc_setup(ecc_p);

	/*
	 * Determine the virtual addresses of the streaming cache
	 * control/status and flush registers.
	 */
	ecc_p->ecc_ctrl_reg = (uint64_t *)(a + COMMON_ECC_CNTRL_REG_OFFSET);
	ecc_p->ecc_ue.ecc_async_flt_status_reg =
		(uint64_t *)(a + COMMON_UE_ASYNC_FLT_STATUS_REG_OFFSET);
	ecc_p->ecc_ue.ecc_async_flt_addr_reg =
		(uint64_t *)(a + COMMON_UE_ASYNC_FLT_ADDR_REG_OFFSET);
	ecc_p->ecc_ce.ecc_async_flt_status_reg =
		(uint64_t *)(a + COMMON_CE_ASYNC_FLT_STATUS_REG_OFFSET);
	ecc_p->ecc_ce.ecc_async_flt_addr_reg =
		(uint64_t *)(a + COMMON_CE_ASYNC_FLT_ADDR_REG_OFFSET);

	DEBUG1(DBG_ATTACH, dip, "ecc_create: ctrl=%x\n", ecc_p->ecc_ctrl_reg);
	DEBUG2(DBG_ATTACH, dip, "ecc_create: ue_afsr=%x, ue_afar=%x\n",
		ecc_p->ecc_ue.ecc_async_flt_status_reg,
		ecc_p->ecc_ue.ecc_async_flt_addr_reg);
	DEBUG2(DBG_ATTACH, dip, "ecc_create: ce_afsr=%x, ce_afar=%x\n",
		ecc_p->ecc_ce.ecc_async_flt_status_reg,
		ecc_p->ecc_ce.ecc_async_flt_addr_reg);
	DEBUG2(DBG_ATTACH, dip, "ecc_create, ue_mondo=%x, ce_mondo=%x\n",
		ecc_p->ecc_ue.ecc_mondo, ecc_p->ecc_ce.ecc_mondo);

	ecc_configure(ecc_p);

	/*
	 * Register routines to be called from system error handling
	 * code.
	 */
	register_bus_func(DIS_ERR_FTYPE, (afunc)ecc_disable_nowait,
		(caddr_t)ecc_p);
	register_bus_func(UE_ECC_FTYPE, (afunc)ecc_err_callback,
		(caddr_t)ecc_p);
}

void
ecc_register_intr(ecc_t *ecc_p)
{
	pci_t *pci_p = ecc_p->ecc_pci_p;
	ib_t *ib_p = ecc_p->ecc_pci_p->pci_ib_p;
	dev_info_t *dip = pci_p->pci_dip;
	ib_ino_t ino;

	/*
	 * Determine the mondo numbers and pil of the ECC interrupts.
	 */
	ino = IB_MONDO_TO_INO(pci_p->pci_interrupts[1]);
	ecc_p->ecc_ue.ecc_mondo = IB_MAKE_MONDO(ib_p, ino);
	IB_INO_INTR_CLEAR(ib_clear_intr_reg_addr(ib_p, ino));

	ino = IB_MONDO_TO_INO(pci_p->pci_interrupts[2]);
	ecc_p->ecc_ce.ecc_mondo = IB_MAKE_MONDO(ib_p, ino);
	IB_INO_INTR_CLEAR(ib_clear_intr_reg_addr(ib_p, ino));

	/*
	 * Install the UE and CE error interrupt handlers.
	 */
	(void) ddi_add_intr(dip, 1, NULL, NULL, ecc_intr,
		(caddr_t)&ecc_p->ecc_ue);
	(void) ddi_add_intr(dip, 2, NULL, NULL, ecc_intr,
		(caddr_t)&ecc_p->ecc_ce);
}

void
ecc_destroy(pci_t *pci_p)
{
	dev_info_t *dip = pci_p->pci_dip;
	ecc_t *ecc_p = pci_p->pci_ecc_p;

	DEBUG0(DBG_DETACH, dip, "ecc_destroy:\n");

	/*
	 * Disable UE and CE ECC error interrupts.
	 */
	ecc_disable_wait(ecc_p);

	/* Only need one unregister as this is done by 'arg' value. */
	unregister_bus_func((caddr_t)ecc_p);

	/*
	 * Remove the ECC interrupt handlers.
	 */
	ddi_remove_intr(dip, 1, NULL);
	ddi_remove_intr(dip, 2, NULL);

	/*
	 * Free the streaming cache state structure.
	 */
	kmem_free(ecc_p, sizeof (ecc_t));
	pci_p->pci_ecc_p = NULL;
}

void
ecc_configure(ecc_t *ecc_p)
{
	dev_info_t *dip = ecc_p->ecc_pci_p->pci_dip;
	uint64_t l = 0;

	/*
	 * Clear any pending ECC errors.
	 */
	DEBUG0(DBG_ATTACH, dip, "ecc_configure: clearing UE and CE errors\n");
	*ecc_p->ecc_ue.ecc_async_flt_status_reg =
		(COMMON_ECC_UE_AFSR_E_MASK << COMMON_ECC_UE_AFSR_PE_SHIFT) |
		(COMMON_ECC_UE_AFSR_E_MASK << COMMON_ECC_UE_AFSR_SE_SHIFT);
	*ecc_p->ecc_ce.ecc_async_flt_status_reg =
		(COMMON_ECC_CE_AFSR_E_MASK << COMMON_ECC_CE_AFSR_PE_SHIFT) |
		(COMMON_ECC_CE_AFSR_E_MASK << COMMON_ECC_CE_AFSR_SE_SHIFT);

	/*
	 * Enable ECC error detections via the control register.
	 */
	DEBUG0(DBG_ATTACH, dip, "ecc_configure: enabling UE CE detection\n");
	if (ecc_error_intr_enable)
		l = (COMMON_ECC_CTRL_ECC_EN | COMMON_ECC_CTRL_UE_INTEN |
			COMMON_ECC_CTRL_CE_INTEN);
	else
		cmn_err(CE_WARN, "%s%d: PCI error interrupts disabled\n",
			ddi_driver_name(dip), ddi_get_instance(dip));
	*ecc_p->ecc_ctrl_reg = l;
}

void
ecc_enable_intr(ecc_t *ecc_p)
{
	dev_info_t *dip = ecc_p->ecc_pci_p->pci_dip;
	ib_t *ib_p = ecc_p->ecc_pci_p->pci_ib_p;
	ib_ino_t ino;

	DEBUG0(DBG_ATTACH, dip, "ecc_enabe_intr: enabling UE and CE intr\n");
	ino = IB_MONDO_TO_INO(ecc_p->ecc_ue.ecc_mondo);
	ib_intr_enable(ib_p, dip, ino);
	ino = IB_MONDO_TO_INO(ecc_p->ecc_ce.ecc_mondo);
	ib_intr_enable(ib_p, dip, ino);

}

void
ecc_disable_wait(ecc_t *ecc_p)
{
	ecc_disable(ecc_p, IB_INTR_WAIT);
}

void
ecc_disable_nowait(ecc_t *ecc_p)
{
	ecc_disable(ecc_p, IB_INTR_NOWAIT);
}

static void
ecc_disable(ecc_t *ecc_p, int wait)
{
	ib_t *ib_p = ecc_p->ecc_pci_p->pci_ib_p;
	ib_ino_t ino;

	*ecc_p->ecc_ctrl_reg &=
		~(COMMON_ECC_CTRL_ECC_EN | COMMON_ECC_CTRL_UE_INTEN |
		COMMON_ECC_CTRL_CE_INTEN);

	ino = IB_MONDO_TO_INO(ecc_p->ecc_ue.ecc_mondo);
	ib_intr_disable(ib_p, ino, wait);
	ino = IB_MONDO_TO_INO(ecc_p->ecc_ce.ecc_mondo);
	ib_intr_disable(ib_p, ino, wait);
}

static uint_t
ecc_intr(caddr_t a)
{
	ecc_intr_info_t *ecc_ii_p = (ecc_intr_info_t *)a;
	ecc_t *ecc_p = ecc_ii_p->ecc_p;
	dev_info_t *dip = ecc_p->ecc_pci_p->pci_dip;
	ib_t *ib_p = ecc_p->ecc_pci_p->pci_ib_p;
	ib_ino_t ino;
	uint64_t afsr, afar;
	uint_t i;
	struct async_flt ecc;

	if (ecc_ii_p->ecc_type == PCI_ECC_UE) {
		/*
		 * Disable all further errors since the will be treated as a
		 * fatal error.
		 */
		ecc_disable_nowait(ecc_p);
	}

	/*
	 * Read the fault registers.
	 */
	if (ecc_ii_p->ecc_errpndg_mask) {
		for (i = 0; i < pci_ecc_afsr_retries; i++) {
			afsr = *ecc_ii_p->ecc_async_flt_status_reg;
			if ((afsr & ecc_ii_p->ecc_errpndg_mask) == 0)
				break;
			/*
			 * If we timeout, the logging routine will
			 * know because it will see the ERRPNDG bits
			 * set in the AFSR.
			 */
		}
	} else {
		afsr = *ecc_ii_p->ecc_async_flt_status_reg;
	}
	afar = *ecc_ii_p->ecc_async_flt_addr_reg;

	if (ecc_ii_p->ecc_type == PCI_ECC_CE) {
		/*
		 * Check for false alarms.
		 */
		if (((afsr >> COMMON_ECC_CE_AFSR_PE_SHIFT) &
		    COMMON_ECC_CE_AFSR_E_MASK) == 0) {
			DEBUG0(DBG_ERR_INTR, dip, "ce: false alarm\n");
			return (DDI_INTR_CLAIMED);
		}
	}

	/*
	 * Clear the errors.
	 */
	*ecc_ii_p->ecc_async_flt_status_reg = afsr;

	/*
	 * Clear the interrupt.
	 */
	ino = IB_MONDO_TO_INO(ecc_ii_p->ecc_mondo);
	ib_intr_clear(ib_p, ino);

	/*
	 * Call system ecc handling code.
	 */
	ecc.flt_stat = afsr;
	ecc.flt_addr = afar;
	ecc.flt_status = ECC_IOBUS;
	ecc.flt_synd = 0;
	ecc.flt_offset = ((afsr & ecc_ii_p->ecc_offset_mask)
	    >> ecc_ii_p->ecc_offset_shift) << ecc_ii_p->ecc_size_log2;
	ecc.flt_size = ecc_ii_p->ecc_size_log2;
	ecc.flt_bus_id = (uint32_t)ecc_p->ecc_pci_p->pci_id;
	ecc.flt_inst = ddi_get_instance(dip);
	ecc.flt_in_memory =
		(pf_is_memory(ecc.flt_addr >> MMU_PAGESHIFT)) ? 1: 0;

	switch (ecc_ii_p->ecc_type) {
	case PCI_ECC_UE:
		ecc.flt_func = (afunc)ecc_log_ue_error;
		ue_error(&ecc);
		break;

	case PCI_ECC_CE:
		ecc.flt_synd = pci_ecc_get_synd(afsr);
		ecc.flt_func = (afunc)ecc_log_ce_error;
		ce_error(&ecc);
		break;

	default:
		cmn_err(CE_PANIC, "Unexpected ecc intr type in pci ecc_intr");
		break;
	}

	return (DDI_INTR_CLAIMED);
}

static void
ecc_err_callback(ecc_t *ecc_p)
{
	uint64_t afsr, afar;
	struct async_flt ecc_flt;
	uint_t i;

	/*
	 * Read the fault registers.
	 */
	if (ecc_p->ecc_ue.ecc_errpndg_mask) {
		for (i = 0; i < pci_ecc_afsr_retries; i++) {
			afsr = *ecc_p->ecc_ue.ecc_async_flt_status_reg;
			if ((afsr & ecc_p->ecc_ue.ecc_errpndg_mask) == 0)
				break;
			/*
			 * If we timeout, the logging routine will
			 * know because it will see the ERRPNDG bits
			 * set in the AFSR.
			 */
		}
	} else {
		afsr = *ecc_p->ecc_ue.ecc_async_flt_status_reg;
	}
	afar = *ecc_p->ecc_ue.ecc_async_flt_addr_reg;

	/*
	 * If there are any errors, log them.
	 */
	if (((afsr >> COMMON_ECC_UE_AFSR_PE_SHIFT) &
	    COMMON_ECC_UE_AFSR_E_MASK) != 0) {
		ecc_flt.flt_stat = afsr;
		ecc_flt.flt_addr = afar;
		ecc_flt.flt_bus_id = ecc_p->ecc_pci_p->pci_id;
		ecc_flt.flt_inst = ddi_get_instance(ecc_p->ecc_pci_p->pci_dip);
		(void) ecc_log_ue_error(&ecc_flt, "No MemMod Decoding");
	}
}
