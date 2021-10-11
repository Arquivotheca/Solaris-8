/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)obio.c	1.20	99/08/28 SMI"	/* SVr4 5.0 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/avintr.h>
#include <sys/ddi_impldefs.h>
#include <sys/kmem.h>
#include <sys/modctl.h>

static dev_info_t *obio_devi;

static int obio_bus_ctl(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
    void *, void *);
static int obio_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int obio_identify(dev_info_t *);
static int obio_attach(dev_info_t *, ddi_attach_cmd_t);
static int obio_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

/*
 * bus_ops
 */
static struct bus_ops obio_bus_ops = {
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
	obio_bus_ctl,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static struct dev_ops obio_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	obio_info,		/* info */
	obio_identify,		/* identify */
	nulldev,		/* probe */
	obio_attach,		/* attach */
	obio_detach,		/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&obio_bus_ops,		/* bus operations */
	nulldev			/* power */
};

static struct modldrv obio_modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"obio nexus driver 1.20",	/* Name of the module. */
	&obio_ops		/* Driver ops */
};

static struct modlinkage obio_modlinkage = {
	MODREV_1, (void *)&obio_modldrv, NULL
};

/*
 * This is the driver initialization routine.
 */
int
_init(void)
{
	return (mod_install(&obio_modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&obio_modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&obio_modlinkage, modinfop));
}

/*ARGSUSED*/
static int
obio_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (obio_devi == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *) obio_devi;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

static int
obio_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "obio") == 0) {
		return (DDI_IDENTIFIED);
	}
	return (DDI_NOT_IDENTIFIED);
}

/*ARGSUSED1*/
static int
obio_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		/*
		 * We should only have one obio nexus!
		 */
		obio_devi = devi;
		ddi_report_dev(devi);
		return (DDI_SUCCESS);

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/* ARGSUSED */
static int
obio_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	case DDI_DETACH:
	default:
		return (DDI_FAILURE);
	}
}

#define	REPORTDEV_BUFSIZE	1024

static int
obio_ctl_reportdev(dev_info_t *dip, dev_info_t *rdip)
{
	int i, n, len, f_len = 0;
	dev_info_t *pdev;
	char *buf;

#ifdef	lint
	dip = dip;
#endif

	buf = kmem_alloc(REPORTDEV_BUFSIZE, KM_SLEEP);

	pdev = (dev_info_t *)DEVI(rdip)->devi_parent;
	f_len += snprintf(buf, REPORTDEV_BUFSIZE, "%s%d at %s%d",
		ddi_driver_name(dip), ddi_get_instance(rdip),
		ddi_driver_name(pdev), ddi_get_instance(pdev));
	len = strlen(buf);

	for (i = 0, n = sparc_pd_getnreg(rdip); i < n; i++) {

		struct regspec *rp = sparc_pd_getreg(rdip, i);

		if (i == 0) {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    ": ");
		} else {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    " and ");
		}
		len = strlen(buf);

		f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
		    "obio 0x%x", rp->regspec_addr);
		len = strlen(buf);
	}

	for (i = 0, n = sparc_pd_getnintr(rdip); i < n; i++) {

		if (i != 0) {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    " ");
		} else {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    ",");
		}
		len = strlen(buf);

		f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
		    " sparc ipl %d",
		    INT_IPL(sparc_pd_getintr(rdip, i)->intrspec_pri));
		len = strlen(buf);
	}
#ifdef DEBUG
	if (f_len + 1 >= REPORTDEV_BUFSIZE) {
		cmn_err(CE_NOTE, "next message is truncated: "
		    "printed length 1024, real length %d", f_len);
	}
#endif DEBUG
	cmn_err(CE_CONT, "?%s\n", buf);
	kmem_free(buf, REPORTDEV_BUFSIZE);
	return (DDI_SUCCESS);
}

/*
 * We're prepared to claim that the interrupt string is in the
 * form of a list of <ipl> specifications - so just 'translate' it.
 */
static int
obio_ctl_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, int *in,
	struct ddi_parent_private_data *pdptr)
{
	register size_t size;
	register int n;
	register struct intrspec *new;

	static char bad_obiointr_fmt[] =
	    "obio%d: bad interrupt spec for %s%d - sparc ipl %d\n";

	/*
	 * The list consists of either <ipl> elements
	 */
	if ((n = *in++) < 1)
		return (DDI_FAILURE);

	pdptr->par_nintr = n;
	size = n * sizeof (struct intrspec);
	new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

	while (n--) {
		register int level = *in++;

		if (level < 1 || level > 15) {
			cmn_err(CE_CONT, bad_obiointr_fmt,
			    DEVI(dip)->devi_instance, DEVI(rdip)->devi_name,
			    DEVI(rdip)->devi_instance, level);
			goto broken;
			/*NOTREACHED*/
		}
		new->intrspec_pri = level | INTLEVEL_ONBOARD;
		new++;
	}

	return (DDI_SUCCESS);
	/*NOTREACHED*/

broken:
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = (void *)0;
	pdptr->par_nintr = 0;
	return (DDI_FAILURE);
}

static int
obio_bus_ctl(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *a, void *v)
{
	switch (op) {

	case DDI_CTLOPS_REPORTDEV:
		return (obio_ctl_reportdev(dip, rdip));

	case DDI_CTLOPS_XLATE_INTRS:
		return (obio_ctl_xlate_intrs(dip, rdip, a, v));

	default:
		return (ddi_ctlops(dip, rdip, op, a, v));
	}
}
