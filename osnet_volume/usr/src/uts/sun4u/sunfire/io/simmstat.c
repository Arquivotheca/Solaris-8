/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)simmstat.c 1.11	98/04/01 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/obpdefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ivintr.h>
#include <sys/intr.h>
#include <sys/intreg.h>
#include <sys/autoconf.h>
#include <sys/modctl.h>
#include <sys/spl.h>

#include <sys/fhc.h>
#include <sys/simmstat.h>

/* Useful debugging Stuff */
#include <sys/nexusdebug.h>

/*
 * Function prototypes
 */

static int simmstat_identify(dev_info_t *);

static int simmstat_attach(dev_info_t *, ddi_attach_cmd_t);

static int simmstat_detach(dev_info_t *, ddi_detach_cmd_t);

static void simmstat_add_kstats(struct simmstat_soft_state *);

static int simmstat_kstat_update(kstat_t *, int);

/*
 * Configuration data structures
 */
static struct cb_ops simmstat_cb_ops = {
	nulldev,			/* open */
	nulldev,			/* close */
	nulldev,			/* strategy */
	nulldev,			/* print */
	nodev,				/* dump */
	nulldev,			/* read */
	nulldev,			/* write */
	nulldev,			/* ioctl */
	nodev,				/* devmap */
	nodev,				/* mmap */
	nodev,				/* segmap */
	nochpoll,			/* poll */
	ddi_prop_op,			/* cb_prop_op */
	0,				/* streamtab */
	D_MP | D_NEW | D_HOTPLUG,	/* Driver compatibility flag */
	CB_REV,				/* rev */
	nodev,				/* cb_aread */
	nodev				/* cb_awrite */
};

static struct dev_ops simmstat_ops = {
	DEVO_REV,			/* rev */
	0,				/* refcnt  */
	ddi_no_info,			/* getinfo */
	simmstat_identify,		/* identify */
	nulldev,			/* probe */
	simmstat_attach,		/* attach */
	simmstat_detach,		/* detach */
	nulldev,			/* reset */
	&simmstat_cb_ops,		/* cb_ops */
	(struct bus_ops *)0,		/* bus_ops */
	nulldev				/* power */
};

static u_int simmstat_reg_read_delay_us = 10;

/*
 * Driver globals
 */
void *simmstatp;

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,			/* module type, this one is a driver */
	"SIMM-status Leaf v1.11",	/* module name */
	&simmstat_ops,			/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,		/* rev */
	(void *)&modldrv,
	NULL
};

#ifndef	lint
static char _depends_on[] = "drv/fhc";
#endif	/* lint */

/*
 * These are the module initialization routines.
 */

int
_init(void)
{
	int error;

	if ((error = ddi_soft_state_init(&simmstatp,
	    sizeof (struct simmstat_soft_state), 1)) != 0)
		return (error);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	ddi_soft_state_fini(&simmstatp);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
simmstat_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	if ((strcmp(name, "simm-status") == 0) ||
	    (strcmp(name, "simmstat") == 0)) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

static int
simmstat_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct simmstat_soft_state *softsp;
	int instance;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(devi);

	if (ddi_soft_state_zalloc(simmstatp, instance) != DDI_SUCCESS)
		return (DDI_FAILURE);

	softsp = ddi_get_soft_state(simmstatp, instance);

	/* Set the dip in the soft state */
	softsp->dip = devi;

	/* Get the board number from this nodes parent device. */
	softsp->pdip = ddi_get_parent(softsp->dip);
	if ((softsp->board = (int)ddi_getprop(DDI_DEV_T_ANY, softsp->pdip,
	    DDI_PROP_DONTPASS, OBP_BOARDNUM, -1)) == -1) {
		cmn_err(CE_WARN, "simmstat%d: unable to retrieve %s property",
			instance, OBP_BOARDNUM);
		goto bad;
	}

	DPRINTF(SIMMSTAT_ATTACH_DEBUG, ("simmstat%d: devi= 0x%x\n, "
		" softsp=0x%x\n", instance, devi, softsp));

	/* map in the registers for this device. */
	if (ddi_map_regs(softsp->dip, 0,
	    (caddr_t *)&softsp->simmstat_base, 0, 0)) {
		cmn_err(CE_WARN, "simmstat%d: unable to map registers",
			instance);
		goto bad;
	}

	/* nothing to suspend/resume here */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
		"pm-hardware-state", (caddr_t)"no-suspend-resume",
		strlen("no-suspend-resume") + 1);

	/* create the kstats for this device */
	simmstat_add_kstats(softsp);

	ddi_report_dev(devi);

	return (DDI_SUCCESS);

bad:
	ddi_soft_state_free(simmstatp, instance);
	return (DDI_FAILURE);
}

/* ARGSUSED */
static int
simmstat_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance;
	struct simmstat_soft_state *softsp;

	/* get the instance of this devi */
	instance = ddi_get_instance(devi);

	/* get the soft state pointer for this device node */
	softsp = ddi_get_soft_state(simmstatp, instance);

	switch (cmd) {
	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	case DDI_DETACH:
		(void) fhc_bdlist_lock(softsp->board);
		if (fhc_bd_detachable(softsp->board))
			break;
		else
			fhc_bdlist_unlock();
		/* FALLTHROUGH */

	default:
		return (DDI_FAILURE);
	}

	fhc_bdlist_unlock();

	/* remove the kstat for this board */
	kstat_delete(softsp->simmstat_ksp);

	/* unmap the registers */
	ddi_unmap_regs(softsp->dip, 0,
		(caddr_t *)&softsp->simmstat_base, 0, 0);

	/* free up the soft state */
	ddi_soft_state_free(simmstatp, instance);
	ddi_prop_remove_all(devi);

	return (DDI_SUCCESS);
}

static void
simmstat_add_kstats(struct simmstat_soft_state *softsp)
{
	struct kstat *simmstat_ksp;

	if ((simmstat_ksp = kstat_create("unix", softsp->board,
	    SIMMSTAT_KSTAT_NAME, "misc", KSTAT_TYPE_RAW,
	    SIMM_COUNT, KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "simmstat%d: kstat_create failed",
			ddi_get_instance(softsp->dip));
	}

	simmstat_ksp->ks_update = simmstat_kstat_update;
	simmstat_ksp->ks_private = (void *)softsp;
	softsp->simmstat_ksp = simmstat_ksp;
	kstat_install(simmstat_ksp);
}

/*
 * Kstats only need ks_update functions when they change dynamically
 * at run time.
 * In the case of the simmstat registers, they contain battery
 * information for NVSIMMs. These need to be updated whenever a
 * kstat_read asks for the data. There is currently no plan to
 * ship NVSIMMs on this platform, but this support must be present.
 */

static int
simmstat_kstat_update(kstat_t *ksp, int rw)
{
	struct simmstat_soft_state *softsp;
	volatile char *statp;	/* pointer to hardware register */
	char *kstatp;		/* pointer to kstat data buffer */
	int i;

	kstatp = (char *)ksp->ks_data;
	softsp = (struct simmstat_soft_state *)ksp->ks_private;

	statp = (char *)softsp->simmstat_base;

	/* this is a read-only kstat. Bail out on a write */
	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {

		/*
		 * copy current status of hardware into the kstat
		 * structure.
		 */
		for (i = 0; i < SIMM_COUNT; i++, statp++, kstatp++) {
			*kstatp = *statp;
			DELAY(simmstat_reg_read_delay_us);
		}
	}
	return (0);
}
