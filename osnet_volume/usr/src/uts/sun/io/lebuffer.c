/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ident	"@(#)lebuffer.c	1.16	99/04/22 SMI"

/*
 * ESC "lebuffer" driver.
 *
 * This driver identifies "lebuffer", maps in the memory associated
 * with the device and exports the property "learg"
 * describing the lebuffer-specifics.
 */

#include	<sys/types.h>
#include	<sys/kmem.h>
#include	<sys/stream.h>
#include	<sys/ethernet.h>
#include	<sys/cmn_err.h>
#include	<sys/conf.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<sys/le.h>
#include	<sys/errno.h>
#include	<sys/modctl.h>
#include	<sys/debug.h>

static int lebufidentify(dev_info_t *dip);
static int lebufattach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int lebufdetach(dev_info_t *dip, ddi_detach_cmd_t cmd);


#define	LEBUFSZ		0x20000	/* XXX use ddi_get_regsize() soon */

static void *lebuf_softc_state;   /* opaque ptr holds softc state instances */

struct lebuf_softc {
	caddr_t lebuf_base;	/* address to map in the device memory */
	caddr_t lebuf_save;	/* address to save device memory contents */
	int	lebuf_size;	/* size of the lebuffer */
	int	lebuf_suspended; /* suspend/resume flag */
};


struct bus_ops lebuf_bus_ops = {
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

static	struct	cb_ops	cb_lebuffer_ops = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* cb_stream */
	D_MP | D_HOTPLUG,	/* cb_flag */
	CB_REV,			/* rev */
	nodev,			/* int (*cb_aread)() */
	nodev			/* int (*cb_awrite)() */
};

/*
 * Device ops - copied from dmaga.c .
 */
struct	dev_ops lebuf_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	ddi_no_info,		/* devo_info */
	lebufidentify,		/* devo_identify */
	nulldev,		/* devo_probe */
	lebufattach,		/* devo_attach */
	lebufdetach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_lebuffer_ops,	/* driver operations */
	&lebuf_bus_ops		/* bus operations */
};

/*
 * This is the loadable module wrapper.
 */

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module. This one is a driver */
	"le local buffer driver",	/* Name of the module. */
	&lebuf_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, 0
};

int
_init()
{
	int error;

	if ((error = ddi_soft_state_init(&lebuf_softc_state,
		sizeof (struct lebuf_softc), 0)) != 0)
		return (error);

	if ((error = mod_install(&modlinkage)) != 0)
		ddi_soft_state_fini(&lebuf_softc_state);

	return (error);
}

int
_fini()
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	ddi_soft_state_fini(&lebuf_softc_state);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
lebufidentify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "lebuffer") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/* ARGSUSED1 */
static int
lebufdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int	unit = ddi_get_instance(dip);
	struct	lebuf_softc *softc;
	struct	leops *lop;

	softc = (struct lebuf_softc *)
		ddi_get_soft_state(lebuf_softc_state, unit);
	ASSERT(softc != NULL);

	switch (cmd) {
	case DDI_DETACH:
		ddi_unmap_regs(dip, 0, &softc->lebuf_base, 0, 0);

		lop = (struct leops *)ddi_getprop(DDI_DEV_T_NONE, dip, 0,
			"learg", 0);
		if (lop != (struct leops *)NULL) {
			kmem_free((caddr_t)lop, sizeof (*lop));
		}
		/*
		 * remove all driver properties
		 */
		ddi_prop_remove_all(dip);

		ddi_soft_state_free(lebuf_softc_state, unit);
		ddi_remove_minor_node(dip, NULL);
		ddi_set_driver_private(dip, NULL);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		if (softc->lebuf_suspended)
			return (DDI_SUCCESS);

		softc->lebuf_save = kmem_alloc(LEBUFSZ, KM_SLEEP);
		bcopy(softc->lebuf_base, softc->lebuf_save, softc->lebuf_size);
		softc->lebuf_suspended = B_TRUE;
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/* ARGSUSED1 */
static int
lebufattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int	unit = ddi_get_instance(dip);
	struct	lebuf_softc *softc;
	struct	leops *lop;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		softc = (struct lebuf_softc *)
			ddi_get_soft_state(lebuf_softc_state, unit);
		ASSERT(softc != NULL);

		if (!softc->lebuf_suspended)
			return (DDI_SUCCESS);

		bcopy(softc->lebuf_save, softc->lebuf_base, softc->lebuf_size);
		kmem_free((caddr_t)softc->lebuf_save, softc->lebuf_size);
		softc->lebuf_suspended = B_FALSE;
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(lebuf_softc_state, unit) != DDI_SUCCESS)
		return (DDI_FAILURE);

	softc = (struct lebuf_softc *)
		ddi_get_soft_state(lebuf_softc_state, unit);

	softc->lebuf_size = LEBUFSZ; /* XXX use ddi_get_regsize() soon */

	/* map in the buffer */
	if (ddi_map_regs(dip, 0, &softc->lebuf_base, 0, 0)) {
		cmn_err(CE_NOTE, "%s%d:  unable to map registers!",
			ddi_get_name(dip), unit);
		return (DDI_FAILURE);
	}

	ddi_report_dev(dip);

	/*
	 * Create "learg" property to pass info to child le driver.
	 */
	lop = kmem_alloc(sizeof (*lop), KM_SLEEP);
	lop->lo_dip = ddi_get_child(dip);
	lop->lo_flags = LOSLAVE;
	lop->lo_base = (u_long) softc->lebuf_base;
	lop->lo_size = softc->lebuf_size;
	lop->lo_init = NULL;
	lop->lo_intr = NULL;
	lop->lo_arg = NULL;

	if (ddi_prop_create(DDI_DEV_T_NONE, dip, 0, "learg",
		(caddr_t)&lop, sizeof (struct leops *)) != DDI_PROP_SUCCESS) {
		cmn_err(CE_NOTE, "lebuffer:  cannot create learg property");
		ddi_unmap_regs(dip, 0, &softc->lebuf_base, 0, 0);
		kmem_free((caddr_t)lop, sizeof (*lop));
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}
