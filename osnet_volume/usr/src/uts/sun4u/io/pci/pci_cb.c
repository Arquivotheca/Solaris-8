/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_cb.c	1.9	99/11/15 SMI"

/*
 * PCI Control Block object
 */
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/systm.h>		/* timeout() */
#include <sys/async.h>
#include <sys/ivintr.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>
#include <sys/machsystm.h>

/*LINTLIBRARY*/

static uint_t cb_thermal_intr(caddr_t a);

void
cb_create(pci_t *pci_p)
{
	cb_t *cb_p = (cb_t *)kmem_zalloc(sizeof (cb_t), KM_SLEEP);
	pci_p->pci_cb_p = cb_p;
	cb_p->cb_pci_p = pci_p;

	pci_cb_setup(cb_p);
}

void
cb_register_intr(cb_t *cb_p)
{
	pci_t *pci_p = cb_p->cb_pci_p;
	ib_t *ib_p = pci_p->pci_ib_p;
	ib_ino_t ino;
	ih_t *ih_p;

	/*
	 * If the thermal-interrupt property is in place, register the
	 * interrupt handler for it.
	 */
	if (cb_p->cb_pci_p->pci_thermal_interrupt != -1) {
		cb_p->cb_thermal_mondo = pci_p->pci_interrupts[4];
		cb_p->cb_thermal_pil = pci_pil[4];
		DEBUG2(DBG_ATTACH, pci_p->pci_dip,
		    "installing thermal interrupt - mondo=%x, pil=%x\n",
		    cb_p->cb_thermal_mondo, cb_p->cb_thermal_pil);

		ino = IB_MONDO_TO_INO(cb_p->cb_thermal_mondo);
		ih_p = ib_alloc_ih(pci_p->pci_dip, 4, cb_thermal_intr,
		    (caddr_t)cb_p);
		(void) ib_new_ino(ib_p, ino, ih_p);

		IB_INO_INTR_CLEAR(ib_clear_intr_reg_addr(ib_p, ino));
		(void) ddi_add_intr(pci_p->pci_dip, 4, NULL, NULL,
		    cb_thermal_intr, (caddr_t)cb_p);
	}
}


void
cb_destroy(pci_t *pci_p)
{
	cb_t *cb_p = pci_p->pci_cb_p;
	ib_t *ib_p = pci_p->pci_ib_p;
	dev_info_t *dip = pci_p->pci_dip;
	ib_ino_t ino;

	if (cb_p->cb_pci_p->pci_thermal_interrupt != -1) {

		/*
		 * Disable thermal warning interrupts and remove the handler.
		 */
		ino = IB_MONDO_TO_INO(cb_p->cb_thermal_mondo);
		ib_intr_disable(ib_p, ino, IB_INTR_WAIT);
		ddi_remove_intr(dip, 4, NULL);
	}
}


void
cb_enable_intr(cb_t *cb_p)
{
	dev_info_t *dip = cb_p->cb_pci_p->pci_dip;
	ib_t *ib_p = cb_p->cb_pci_p->pci_ib_p;
	ib_ino_t ino;

	if (cb_p->cb_pci_p->pci_thermal_interrupt != -1) {
		DEBUG0(DBG_ATTACH, dip,
			"cb_enable_intr: enabling thermal interrupt\n");
		ino = IB_MONDO_TO_INO(cb_p->cb_thermal_mondo);
		ib_intr_enable(ib_p, dip, ino);
	}
}


/*
 * cb_thermal_intr
 *
 * This function is the interrupt handler for thermal warning interrupts.
 *
 * return value: DDI_INTR_CLAIMED
 */
static uint_t
cb_thermal_intr(caddr_t a)
{
	cb_t *cb_p = (cb_t *)a;
	ib_t *ib_p = cb_p->cb_pci_p->pci_ib_p;
	ib_ino_t ino;

	/*
	 * Disable any further instances of the thermal warning interrupt and
	 * clear this occurrunce.
	 */
	ino = IB_MONDO_TO_INO(cb_p->cb_thermal_mondo);
	ib_intr_disable(ib_p, ino, IB_INTR_NOWAIT);
	ib_intr_clear(ib_p, ino);

	/*
	 * Halt the OS.
	 */
	cmn_err(CE_WARN, "Thermal warning detected!\n");
	if (pci_thermal_intr_fatal) {
		do_shutdown();

		/*
		 * In case do_shutdown() fails to halt the system.
		 */
		(void) timeout((void(*)(void *))power_down, NULL,
		    thermal_powerdown_delay * hz);
	}
	return (DDI_INTR_CLAIMED);
}
