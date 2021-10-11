/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)pcs.c	1.4	96/05/09 SMI"

#include <sys/types.h>
#include <sys/byteorder.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
int pcs_open(dev_t *, int, int, cred_t *);
int pcs_close(dev_t, int, int, cred_t *);
*/
int pcs_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
int pcs_identify(dev_info_t *);
int pcs_attach(dev_info_t *, ddi_attach_cmd_t);
int pcs_detach(dev_info_t *, ddi_detach_cmd_t);
dev_info_t *pcs_dip;

#if 0
static struct cb_ops pcs_ops = {
	pcs_open,
	pcs_close
};
#endif

static struct dev_ops pcs_devops = {
	DEVO_REV,
	0,
	pcs_getinfo,
	pcs_identify,
	nulldev,
	pcs_attach,
	pcs_detach,
	nulldev,
#if 0
	&pcs_ops,
#else
	NULL,
#endif
	NULL
};
/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"PCMCIA Socket Driver",	/* Name of the module. */
	&pcs_devops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

struct pcs_inst {
	dev_info_t *dip;
} *pcs_instances;

int
_init()
{
	int ret;
	if ((ret = ddi_soft_state_init((void **)&pcs_instances,
					sizeof (struct pcs_inst), 1)) != 0)
		return (ret);
	if ((ret = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini((void **)&pcs_instances);
	}
	return (ret);
}

int
_fini()
{
	int ret;
	ret = mod_remove(&modlinkage);
	if (ret == 0) {
		ddi_soft_state_fini((void **)&pcs_instances);
	}
	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
pcs_identify(dev_info_t *dip)
{
#ifdef lint
	dip = dip;
#endif
	return (DDI_IDENTIFIED);
}

int
pcs_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	int error = DDI_SUCCESS;
	int inum;
	struct pcs_inst *inst;
#ifdef lint
	dip = dip;
#endif

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		inum = getminor((dev_t)arg);
		inst = (struct pcs_inst *)ddi_get_soft_state(pcs_instances,
								inum);
		if (inst == NULL)
			error = DDI_FAILURE;
		else
			*result = inst->dip;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		inum = getminor((dev_t)arg);
		inst = (struct pcs_inst *)ddi_get_soft_state(pcs_instances,
								inum);
		if (inst == NULL)
			error = DDI_FAILURE;
		else
			*result = (void *)inum;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

int
pcs_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int ret = DDI_SUCCESS;
	int inum;
	struct pcs_inst *inst;
#ifdef lint
	dip = dip;
	cmd = cmd;
#endif

	inum = ddi_get_instance(dip);

	if (ddi_soft_state_zalloc(pcs_instances, inum) == DDI_SUCCESS) {
		inst = (struct pcs_inst *)ddi_get_soft_state(pcs_instances,
								inum);
		if (inst == NULL)
			ret = DDI_FAILURE;
		else
			inst->dip = dip;
	}

	return (ret);
}

int
pcs_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd == DDI_DETACH) {
		ddi_soft_state_free(pcs_instances, ddi_get_instance(dip));
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}
