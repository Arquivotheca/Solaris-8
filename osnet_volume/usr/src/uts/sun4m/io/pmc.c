/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pmc.c	1.18	99/07/02 SMI"

/*
 * Power Management Chip Driver
 */

#include	<sys/stat.h>
#include	<sys/modctl.h>
#include	<sys/conf.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<sys/errno.h>

#include	<sys/pmcreg.h>
#include	<sys/pmcvar.h>
#include	<sys/pmcio.h>
#include	<sys/ddi_impldefs.h>

/*
 * Function prototypes.
 */
static int	pmc_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int	pmc_chpoll(dev_t, short, int, short *, struct pollhead **);

static int	pmc_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	pmc_identify(dev_info_t *);
static int	pmc_attach(dev_info_t *, ddi_attach_cmd_t);
static int	pmc_detach(dev_info_t *, ddi_detach_cmd_t);
static int	pmc_power(dev_info_t *, int, int);

static u_int	pmc_intr(caddr_t);

static int	pmc_mapfb(pmc_unit *);

static int	platform_power(power_req_t *);

static struct cb_ops pmc_cb_ops = {
	nulldev,		/* open */
	nulldev,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	pmc_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	pmc_chpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	NULL,			/* streamtab */
	D_NEW | D_MP		/* driver compatibility flag */
};

static struct dev_ops pmc_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	pmc_getinfo,		/* info */
	pmc_identify,		/* identify */
	nulldev,		/* probe */
	pmc_attach,		/* attach */
	pmc_detach,		/* detach */
	nodev,			/* reset */
	&pmc_cb_ops,		/* driver operations */
	(struct bus_ops *)NULL,	/* bus operations */
	pmc_power		/* power */
};

/*
 * loadable module wrapper ...
 */

static void *statep;
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* type of module - pseudo */
	"Power Management prototype v1.17",
	&pmc_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

/* Module Configuration routines */

int
_init(void)
{
	int e;

	(void) ddi_soft_state_init(&statep, sizeof (pmc_unit), 1);
	if ((e = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&statep);
	}
	return (e);
}

int
_fini(void)
{
	/*
	 * return ebusy so the framework can't unload the pmc driver
	 */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
pmc_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t		dev = (dev_t)arg;
	int		instance;
	pmc_unit	*unitp;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		instance = getminor(dev);
		if ((unitp = ddi_get_soft_state(statep, instance)) == NULL)
			return (DDI_FAILURE);
		*result = (void *)unitp->dip;
		return (DDI_SUCCESS);

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor(dev);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
pmc_identify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "SUNW,pmc") == 0 ||
	    strcmp(ddi_get_name(dip), "power-management") == 0)
		return (DDI_IDENTIFIED);

	return (DDI_NOT_IDENTIFIED);
}

static int
pmc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	pmc_unit	*unitp;
	int		instance = ddi_get_instance(dip);
	struct pmc_reg	*pmc;
	int		(*prop_val)();
	int		nintrs;
	extern int	(*pm_platform_power)(power_req_t *);

	/* Check for number of interrupts. */
	if (ddi_dev_nintrs(dip, &nintrs) == DDI_FAILURE) {
		cmn_err(CE_WARN, "pmc: ddi_dev_nintrs failed.");
	}

	switch (cmd) {
	case DDI_ATTACH:
		if (instance != 0)
			/*
			 * Only one pmc chip and driver per system
			 * This is assumed in pmc_power()
			 */
			return (DDI_FAILURE);

		/*
		 * Allocate soft state structure
		 */
		if (ddi_soft_state_zalloc(statep, instance) != 0)
			return (DDI_FAILURE);
		unitp = (pmc_unit *)ddi_get_soft_state(statep, instance);

		/*
		 * Add in interrupt
		 */
		if (nintrs > 0 && ddi_add_intr(dip, 0, &unitp->pmcibc,
		    NULL, pmc_intr, (caddr_t)unitp) != DDI_SUCCESS) {
			ddi_soft_state_free(statep, instance);
			return (DDI_FAILURE);
		}

		/*
		 * Map in device registers
		 */
		if (ddi_map_regs(dip, 0, (caddr_t *)&unitp->pmcregs, 0,
		    sizeof (struct pmc_reg)) != DDI_SUCCESS) {
			ddi_soft_state_free(statep, instance);
			if (nintrs > 0) {
				ddi_remove_intr(dip, 0, unitp->pmcibc);
			}
			return (DDI_FAILURE);
		}

		if (ddi_create_minor_node(dip, "pmc", S_IFCHR,
		    instance, "ddi_other", 0) == DDI_FAILURE) {
			ddi_soft_state_free(statep, instance);
			if (nintrs > 0) {
				ddi_remove_intr(dip, 0, unitp->pmcibc);
			}
			ddi_unmap_regs(dip, 0, (caddr_t *)&unitp->pmcregs,
			    0, sizeof (struct pmc_reg));
			return (DDI_FAILURE);
		}
		ddi_report_dev(dip);

		mutex_init(&unitp->pmc_lock, NULL, MUTEX_DRIVER, unitp->pmcibc);
		cv_init(&unitp->a2d_cv, NULL, CV_DRIVER, NULL);

		unitp->dip = dip;

		mutex_enter(&unitp->pmc_lock);
		pmc = unitp->pmcregs;
		pmc->pmc_kb = PMC_KBD_INTEN;
		pmc->pmc_pwrkey = PMC_PWRKEY_INTEN;
		pmc->pmc_enet |= PMC_ENET_INTEN;
		pmc->pmc_isdn = PMC_ISDN_INTEN;
		pmc->pmc_a2dctrl = PMC_A2D_INTEN;
		pmc->pmc_d2a = 0xff;
		unitp->save_d2a = 0xff;

		/*
		 * add hook for power management
		 * we're committed now, we can't fail attach or
		 * unload after this, as this will be used from the
		 * idle loop
		 */
		pm_platform_power = platform_power;
		break;

	case DDI_RESUME:
		unitp = (pmc_unit *)ddi_get_soft_state(statep, instance);
		pmc = unitp->pmcregs;
		mutex_enter(&unitp->pmc_lock);
		pmc->pmc_kb = PMC_KBD_INTEN;
		pmc->pmc_pwrkey = PMC_PWRKEY_INTEN;
		pmc->pmc_enet |= PMC_ENET_INTEN;
		pmc->pmc_isdn = PMC_ISDN_INTEN;
		pmc->pmc_a2dctrl = PMC_A2D_INTEN;
		pmc->pmc_d2a = unitp->save_d2a;
		break;
	default:
		cmn_err(CE_WARN, "pmc: attach command unknown");
		return (DDI_FAILURE);
	}

	unitp->state = NORMAL;
	mutex_exit(&unitp->pmc_lock);
	return (DDI_SUCCESS);
}


static int
pmc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	pmc_unit	*unitp;
	int		instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_DETACH:			/* Not allowed */
		return (DDI_FAILURE);

	case DDI_SUSPEND:
		unitp = (pmc_unit *)ddi_get_soft_state(statep, instance);
		unitp->state = SUSPEND;
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*
 * (standard) UNIX entry points
 */

/*ARGSUSED2*/
static int
pmc_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *cred_p, int *rval_p)
{
	int		instance;
	pmc_unit	*unitp;
	volatile struct pmc_reg	*pmc;

	instance = getminor(dev);
	if ((unitp = (pmc_unit *)ddi_get_soft_state(statep, instance)) == NULL)
		return (ENXIO);
	pmc = unitp->pmcregs;

	mutex_enter(&unitp->pmc_lock);

	switch (cmd) {

	case PMC_GET_KBD:
		*rval_p = pmc->pmc_kb & PMC_KBD_STAT;
		break;

	case PMC_GET_ENET:
		*rval_p = pmc->pmc_enet & PMC_ENET_STAT;
		break;

	case PMC_GET_ISDN:
		*rval_p = pmc->pmc_isdn & (PMC_ISDN_ST0 | PMC_ISDN_ST1);
		break;

	case PMC_GET_A2D:
		pmc->pmc_a2dctrl &= 0xf8; /* clear addr pins */
		pmc->pmc_a2dctrl |= 0x01; /* set addr of dac readback */
		pmc->pmc_a2dctrl |= 0x10; /* start conversion */
		while (!cv_wait_sig(&unitp->a2d_cv, &unitp->pmc_lock))
			;
		*rval_p = pmc->pmc_a2d;
		break;

	case PMC_POWER_OFF:
		if (drv_priv(cred_p))
			return (EPERM);
		pmc->pmc_pwrkey |= PMC_PWRKEY_PWR;
		break;			/* We won't get here: no power */

	default:
		mutex_exit(&unitp->pmc_lock);
		return (ENOTTY);
	}
	mutex_exit(&unitp->pmc_lock);
	return (0);

}

static int
pmc_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp)
{
	pmc_unit	*unitp;
	int		instance;

	instance = getminor(dev);
	if ((unitp = (pmc_unit *)ddi_get_soft_state(statep, instance)) == NULL)
		return (ENXIO);
	*reventsp = 0;
	mutex_enter(&unitp->pmc_lock);
	if (unitp->poll_event) {
		*reventsp = unitp->poll_event;
		unitp->poll_event = 0;
	} else if ((events & POLLIN) && !anyyet)
		*phpp = &unitp->poll;
	mutex_exit(&unitp->pmc_lock);
	return (0);
}

static u_int
pmc_intr(caddr_t arg)
{
	pmc_unit	*unitp = (pmc_unit *)arg;
	struct pmc_reg	*pmc;

	pmc = unitp->pmcregs;

	/*
	 * look at registers to see who/what interrupted.
	 */
	if (pmc->pmc_kb & PMC_KBD_INTR) {
		unitp->poll_event = POLLIN;
		pollwakeup(&unitp->poll, POLLIN);
		return (DDI_INTR_CLAIMED);
	}
	if (pmc->pmc_pwrkey & PMC_PWRKEY_INTR) {
		return (DDI_INTR_CLAIMED);
	}
	if (pmc->pmc_enet & PMC_ENET_INTR) {
		unitp->poll_event = POLLIN;
		pollwakeup(&unitp->poll, POLLIN);
		return (DDI_INTR_CLAIMED);
	}
	if (pmc->pmc_isdn & PMC_ISDN_INTR) {
		unitp->poll_event = POLLIN;
		pollwakeup(&unitp->poll, POLLIN);
		return (DDI_INTR_CLAIMED);
	}
	if (pmc->pmc_a2dctrl & PMC_A2D_INTR) {
		cv_broadcast(&unitp->a2d_cv);
		return (DDI_INTR_CLAIMED);
	}

	return (DDI_INTR_UNCLAIMED);

}

static int
pmc_power(dev_info_t *dip, int cmpt, int level)
{
	int		instance = 0; /* Hack to get own instance */
	pmc_unit	*unitp;
	u_char		*pmc, reg;
	pmc_device	*device;

	unitp = (pmc_unit *)ddi_get_soft_state(statep, instance);
	for (device = pmc_devices; device->name != NULL; device++) {
		if (strcmp(ddi_get_name(dip), device->name) ||
		    ddi_get_instance(dip) != device->instance ||
		    cmpt != device->cmpt)
			continue;

		pmc = ((u_char *)unitp->pmcregs) + device->reg;
		/*
		 * Framework uses power values: 0 = off; 1, 2, ... = on;
		 * but pmc chip uses 0 = on, 1 = off for everything but cgsix
		 * and bwtwo, so ...
		 */
		if (strcmp(device->name, "cgsix") != 0 &&
		    strcmp(device->name, "bwtwo") != 0) {
			level = level ? 0 : 0xff;
			reg = *pmc & ~device->mask;	/* Get non pwr bits */
			reg |= device->mask & level;	/* Set new pwr bits */
		} else {
			/* Frame buffer */
			if (!unitp->fbctl && !pmc_mapfb(unitp))
				return (DDI_FAILURE);
			reg = (u_char)level;		/* d2a is write only */
			if (level) {
				unitp->save_d2a = reg;
				*unitp->fbctl = 0x0c;
			} else if (*unitp->fbctl) {
				*unitp->fbctl = 0x00;
			}
		}
		*pmc = reg;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

static int
pmc_mapfb(pmc_unit *unitp)
{
	dev_info_t	*fbdip;

	/*
	 * Map in frame buffer flat panel register (lives in frame
	 * buffer address space). Set it to manual mode.
	 */
	if (((fbdip = ddi_find_devinfo("bwtwo", -1, 0)) == NULL ||
	    ddi_map_regs(fbdip, 0, (caddr_t *)&unitp->fbctl, BW2OFFSET,
	    sizeof (char)) != DDI_SUCCESS) &&
	    ((fbdip = ddi_find_devinfo("cgsix", -1, 0)) == NULL ||
	    ddi_map_regs(fbdip, 0, (caddr_t *)&unitp->fbctl, CG6OFFSET,
	    sizeof (char)) != DDI_SUCCESS)) {
		return (0);
	}
	*unitp->fbctl = 0x0c;
	return (1);
}

/*
 * This routine just decodes the struct and passes the reqeust on
 */
static int
platform_power(power_req_t *req)
{
	/*
	 * We only know how to do component power setting on sun4m
	 * We fail all other requests.
	 */
	if (req->request_type == PMR_SET_POWER)
		return (pmc_power(req->req.set_power_req.who,
		    req->req.set_power_req.cmpt, req->req.set_power_req.level));
	else
		return (DDI_FAILURE);
}
