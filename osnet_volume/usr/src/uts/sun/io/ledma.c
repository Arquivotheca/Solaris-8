/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ledma.c	1.27	99/04/21 SMI"

/*
 * DMA2 "ledma" driver.
 *
 * This driver identifies "ledma", maps in the DMA2 E-CSR register,
 * attaches the child node "le", and calls the le driver routine
 * attaches the child node "le", and exports the "learg" property
 * to pass info to the child le driver.
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/stream.h>
#include <sys/ethernet.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/le.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_subrdefs.h>

static	int	ledmaidentify(dev_info_t *);
static	int	ledmaattach(dev_info_t *, ddi_attach_cmd_t cmd);
static	int	ledmadetach(dev_info_t *, ddi_detach_cmd_t cmd);
static	int	ledmainit(dev_info_t *, int, ddi_dma_handle_t *);
static	int	ledmaintr(dev_info_t *);

/*
 * DMA2 E-CSR bit definitions.
 * Refer to the Campus II DMA2 Chip Specification for details.
 */
#define	E_INT_PEND	(1 << 0)	/* e_irq_ is active */
#define	E_ERR_PEND	(1 << 1)	/* memory time-out/protection/parity */
#define	E_DRAINING	(3 << 2)	/* e-cache draining */
#define	E_INT_EN	(1 << 4)	/* enable interrupts */
#define	E_INVALIDATE	(1 << 5)	/* invalidate e-cache */
#define	E_SLAVE_ERR	(1 << 6)	/* slave access size error */
#define	E_RESET		(1 << 7)	/* reset */
#define	E_DRAIN		(1 << 10)	/* drain e-cache */
#define	E_DSBL_WR_DRN	(1 << 11)	/* disable e-cache wr desc drain */
#define	E_DSBL_RD_DRN	(1 << 12)	/* disable e-cache slave rd drain */
#define	E_ILACC		(1 << 15)	/* ILACC mode (untested) */
#define	E_DSBL_BUF_WR	(1 << 16)	/* disable slave write buffer */
#define	E_DSBL_WR_INVAL	(1 << 17)	/* disable slave write e-cache inval. */
#define	E_BURST_SIZE	(3 << 18)	/* sbus burst sizes mask */
#define	E_ALE_AS	(1 << 20)	/* define pin 35 active high/low */
#define	E_LOOP_TEST	(1 << 21)	/* enable external ethernet loopback */
#define	E_TPE		(1 << 22)	/* select TP or AUI */
#define	E_DEV_ID	(0xf0000000)	/* device id mask */

#define	E_DMA2_ID	0xa0000000	/* DMA2 E-CSR device id */

/*
 * DMA2 E-CSR SBus burst sizes
 */
#define	E_BURST16	(0 << 18)	/* 16 byte SBus bursts */
#define	E_BURST32	(1 << 18)	/* 32 byte SBus bursts */

static struct bus_ops ledma_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	i_ddi_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	ddi_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0,	/* (*bus_post_event)();		*/
	i_ddi_intr_ctlops
};

/*
 * Device ops - copied from dmaga.c .
 */
static struct dev_ops ledma_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	ddi_no_info,		/* devo_info */
	ledmaidentify,		/* devo_identify */
	nulldev,		/* devo_probe */
	ledmaattach,		/* devo_attach */
	ledmadetach,		/* devo_detach */
	nodev,			/* devo_reset */
	(struct cb_ops *)0,	/* driver operations */
	&ledma_bus_ops,		/* bus operations */
	nulldev			/* power */
};

/*
 * Patchable delay variable for AT&T 7213.
 */
static	clock_t	ledmadelay = 300000;

/*
 * This is the loadable module wrapper.
 */

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module. This one is a driver */
	"le dma driver",	/* Name of the module. */
	&ledma_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, 0
};

int
_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
ledmaidentify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "ledma") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static int
ledmaattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	uint32_t *csr;
	struct leops *lop;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	/*
	 * Map in the E-CSR control register.
	 */
	if (ddi_map_regs(dip, 0, (caddr_t *)&csr, 0, 0)) {
		cmn_err(CE_NOTE, "ledma%d: unable to map registers",
		    ddi_get_instance(dip));
		return (DDI_FAILURE);
	}

	/*
	 * DMA2 sanity check.
	 */
	if ((*csr & E_DEV_ID) != E_DMA2_ID) {
		cmn_err(CE_NOTE, "ledma%d: invalid DMA2 E-CSR format: 0x%x",
			ddi_get_instance(dip), *csr);
		ddi_unmap_regs(dip, 0, (caddr_t *)&csr, 0, 0);
		return (DDI_FAILURE);
	}

	ddi_set_driver_private(dip, (caddr_t)csr);
	ddi_report_dev(dip);

	/*
	 * Export "learg" property to le driver.
	 */
	lop = kmem_alloc(sizeof (*lop), KM_SLEEP);
	lop->lo_dip = ddi_get_child(dip);
	lop->lo_flags = 0;
	lop->lo_base = NULL;
	lop->lo_size = 0;
	lop->lo_init = ledmainit;
	lop->lo_intr = ledmaintr;
	lop->lo_arg = (caddr_t)dip;

	if (ddi_prop_create(DDI_DEV_T_NONE, dip, 0, "learg",
		(caddr_t)&lop, sizeof (struct leops *)) != DDI_PROP_SUCCESS) {
		cmn_err(CE_NOTE, "ledma:  cannot create learg property");
		ddi_unmap_regs(dip, 0, (caddr_t *)&csr, 0, 0);
		kmem_free(lop, sizeof (*lop));
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
ledmadetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	case DDI_DETACH:
	default:
		return (DDI_FAILURE);
	}
}

/*
 * "Exported" by address and called by the le driver.
 *
 * SS10 uses AT&T 7213 which requires us to wait for 200ms during the
 * switch between TP and AUI.
 */
static int
ledmainit(dev_info_t *dip, int tpe, ddi_dma_handle_t *handlep)
{
	uint32_t *csr = (uint32_t *)ddi_get_driver_private(dip);
	uint32_t tcsr;
	int burst = -1;

	/*
	 * A nonzero tpe argument means Twisted Pair.
	 * Zero means AUI.
	 */
	if (tpe)
		tpe = E_TPE;
	tcsr = *csr;

	/*
	 * Hard reset only when required.
	 * These times are:
	 * At time of initialization (boot time)
	 * Whenver E_ERR_PEND is set.
	 * E_INT_EN will always be set after the first initialization
	 */
	if ((tcsr & E_ERR_PEND) || (!(tcsr & E_INT_EN))) {

		*csr = E_RESET;

		if (*csr)	/* readback */
			/*EMPTY*/;
		drv_usecwait(ledmadelay);

		/*
		 * Determine proper SBus burst size to use.
		 */
		burst = ddi_dma_burstsizes(*handlep);
		if (burst & 0x20)
			burst = E_BURST32;
		else
			burst = E_BURST16;

		/*
		 * Initialize DMA2 E_CSR.
		 */
		*csr = tpe | E_INT_EN | E_INVALIDATE | E_DSBL_RD_DRN
			| E_DSBL_WR_INVAL | burst;

		if (*csr)	/* readback */
			/*EMPTY*/;
		drv_usecwait(ledmadelay);

	} else if ((tcsr & E_TPE) != tpe) {

		/*
		 * We have already reset before.  Now we need to switch between
		 * TPE and AUI.
		 */
		tcsr &= ~E_TPE;
		tcsr |= tpe;
		*csr = tcsr;

		if (*csr)	/* readback */
			/*EMPTY*/;
		drv_usecwait(ledmadelay);
	}

	return (1);
}

/*
 * "Exported" by address and called by the le driver.
 * Return 1 if "serviced", 0 otherwise.
 */
static int
ledmaintr(dev_info_t *dip)
{
	uint32_t *csr = (uint32_t *)ddi_get_driver_private(dip);

	if (*csr & (E_ERR_PEND | E_SLAVE_ERR)) {
		if (*csr & E_ERR_PEND)
			cmn_err(CE_NOTE, "ledma%d:  E_ERR_PEND!",
			    ddi_get_instance(dip));
		if (*csr & E_SLAVE_ERR)
			cmn_err(CE_NOTE, "ledma%d:  E_SLAVE_ERR!",
			    ddi_get_instance(dip));
		return (1);
	}

	return (0);
}
