/*
 * Copyright (c) 1991,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootbus.c	1.39	97/10/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/autoconf.h>
#include <sys/cpu.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/avintr.h>
#include <sys/debug.h>
#include <sys/spl.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>

/*
 * External Routines:
 */
extern int nexus_note_bbus(u_int cpu_id, struct autovec *handler);

#define	ZALLOC_SLEEP_N(type, num)				\
	(type *)kmem_zalloc((num) * sizeof (type), KM_SLEEP)

#define	MAX_BOOTBUS	(2 * 10)
#define	MAX_PRI		16

/*
 * bootbus-nexi private data
 */
typedef struct {
	struct autovec *av_table;
	dev_info_t *dip;
	u_int cpu_id;
} bbus_private_t;

/*
 * In the device tree, each bootbus hangs off the CPU node that owns
 * its semaphore.  So bbus_private[] is indexed by cpu_id, and entries
 * exist only for CPU's which own a bootbus semaphore.
 */
bbus_private_t bbus_private[MAX_BOOTBUS];

#define	get_private_data(dip)	\
	((bbus_private_t *)ddi_get_driver_private(dip))


static void
set_private_data(dev_info_t *dip, int cpu_id, struct autovec *vec)
{
	bbus_private_t *prv = bbus_private + cpu_id;	/* ptr arithmetic! */

	ddi_set_driver_private(dip, (caddr_t)prv);
	prv->dip = dip;
	prv->av_table = vec;
	prv->cpu_id = cpu_id;
	(void) nexus_note_bbus(cpu_id, vec);
}

/*
 * determine_attached_cpu - this should be a generic routine
 */
static int
determine_attached_cpu(dev_info_t *devi)
{
	dev_info_t *pdevi = ddi_get_parent(devi);
	int node_id = ddi_get_nodeid(devi);
	int parent_node_id = ddi_get_nodeid(pdevi);
	int device_id = get_deviceid(node_id, parent_node_id);
	int cpu_id = xdb_cpu_unit(device_id);

	return (cpu_id);
}

/*
 * bbus_parent_cpu - hide details of the av_table
 */
u_int
bbus_parent_cpu(dev_info_t *dip)
{
	bbus_private_t *prv = get_private_data(dip);
	u_int cpu_id = prv->cpu_id;

	return (cpu_id);
}

/*
 * bbus_find_vector - hide details of the av_table
 */
static struct autovec *
bbus_find_vector(dev_info_t *dip, int pri)
{
	struct autovec *base = get_private_data(dip)->av_table;
	struct autovec *av = base + pri;	/* ptr arithmetic! */

	ASSERT(pri < MAX_PRI);
	ASSERT(base != 0);
	return (av);
}

static int
bbus_patch_intrspec_pri(int intrspec_pri)
{
	int pri = intrspec_pri & 0xf;

	if (intrspec_pri != pri) {
		printf("bootbus nexus: intrspec_pri=%d, expected=%d\n",
			intrspec_pri, pri);
	}
	return (pri);
}

/*
 * bbus_add_hard - add an interrupt service routine
 */
/* ARGSUSED */
static int
bbus_add_hard(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg)
{
	struct intrspec *ispec = (struct intrspec *)intrspec;
	u_int pri = bbus_patch_intrspec_pri(ispec->intrspec_pri);
	struct autovec *av = bbus_find_vector(dip, pri);

	if (av == NULL)
		return (DDI_FAILURE);

	if (av->av_vector != NULL) {
		/*
		 * This means we rely on 1 zs intr for the whole system.
		 * I.e. zs driver cannot register more than one zsintr
		 * with different args. Bug?
		 */
		if ((av->av_vector != int_handler) ||
		    (av->av_intarg != int_handler_arg)) {
			cmn_err(CE_WARN, "bootbus_add_hard: handler exists!\n");
			return (DDI_FAILURE);
		}
	}

	av->av_devi = dip;
	av->av_intarg = int_handler_arg;
	av->av_vector = int_handler;

	ispec->intrspec_func = int_handler;
	ispec->intrspec_pri = pri;

	if (iblock_cookiep) {
		*iblock_cookiep = (ddi_iblock_cookie_t)ipltospl(pri);
	}

	if (idevice_cookiep) {
		idevice_cookiep->idev_vector = 0;
		idevice_cookiep->idev_priority = pri;
	}

	return (DDI_SUCCESS);
}

/*
 * bbus_rem_hard - take away an interrupt service routine
 */
static void
bbus_rem_hard(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec)
{
	char *name = ddi_get_name(rdip);
	struct intrspec *ispec = (struct intrspec *)intrspec;
	u_int pri = bbus_patch_intrspec_pri(ispec->intrspec_pri);
	struct autovec *av = bbus_find_vector(dip, pri);

	extern void wait_till_seen(int ipl);

#ifdef	DEBUG
	printf("bbus_add_hard: name=%s, pri=%d, av=0x%p\n", name, pri,
								(void *)av);
#endif	/* DEBUG */

	if (ispec->intrspec_func == (u_int (*)()) 0) {
		return;
	}

	if (av) {
		av->av_vector = 0;
		wait_till_seen(INT_IPL(ispec->intrspec_pri));
	}

	ispec->intrspec_func = (u_int (*)()) 0;
}


/*
 * add_intrspec - Add an interrupt specification.
 */
static int
bbus_add_intrspec(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg,
	int kind)
{
	ASSERT(intrspec != 0);
	ASSERT(rdip != 0);
	ASSERT(ddi_get_driver(rdip) != 0);

	switch (kind) {
		case IDDI_INTR_TYPE_NORMAL: {
			int rc;

			mutex_enter(&av_lock);
			rc = bbus_add_hard(dip, rdip, intrspec,
				iblock_cookiep, idevice_cookiep,
				int_handler, int_handler_arg);
			mutex_exit(&av_lock);
			return (rc);
		}
		default: {
			/*
			 * I can't do it, pass the buck to my parent
			 */
			int rc = i_ddi_add_intrspec(dip, rdip, intrspec,
				iblock_cookiep, idevice_cookiep,
				int_handler, int_handler_arg, kind);
			return (rc);
		}
	}
}

/*
 * remove_intrspec - Remove an interrupt specification.
 */
/*ARGSUSED3*/
static void
bbus_remove_intrspec(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t iblock_cookiep)
{
	ASSERT(intrspec != 0);
	ASSERT(rdip != 0);

	mutex_enter(&av_lock);
	bbus_rem_hard(dip, rdip, intrspec);
	mutex_exit(&av_lock);
}

/*
 * This is a short term OBP device tree workaround!
 */
int
bbus_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	int ptr;
	struct rangespec *rangep = sparc_pd_getrng(dip, 0);

	rangep->rng_size = 0x7fffff;
	ptr = i_ddi_bus_map(dip, rdip, mp, offset, len, vaddrp);
	return (ptr);
}

static struct bus_ops bbus_bus_ops = {
	BUSO_REV,
	bbus_bus_map,		/* map */
	i_ddi_get_intrspec,	/* get_intrspec */
	bbus_add_intrspec,	/* add_intrspec */
	bbus_remove_intrspec,	/* remove_intrspec */
	i_ddi_map_fault,	/* map_fault */
	ddi_dma_map,		/* dma_map */
	ddi_dma_allochdl,	/* dma_allochdl */
	ddi_dma_freehdl,	/* dma_freehdl */
	ddi_dma_bindhdl,	/* dma_bindhdl */
	ddi_dma_unbindhdl,	/* dma_unbindhdl */
	ddi_dma_flush,		/* dma_flush */
	ddi_dma_win,		/* dma_win */
	ddi_dma_mctl,		/* dma_ctl */
	ddi_ctlops,		/* ctl */
	ddi_bus_prop_op,	/* prop_op */
	0,			/* (*bus_get_eventcookie)();	*/
	0,			/* (*bus_add_eventcall)();	*/
	0,			/* (*bus_remove_eventcall)();	*/
	0			/* (*bus_post_event)();		*/
};

/*
 * bbus_getinfo - don't know what we're supposed to do!
 */
/*ARGSUSED*/
static int
bbus_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {

	case DDI_INFO_DEVT2DEVINFO: {
		error = DDI_FAILURE;
		break;
	}

	case DDI_INFO_DEVT2INSTANCE: {
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	}

	default:
		error = DDI_FAILURE;
	}

	return (error);
}


/*
 * bbus_identify
 */
static int
bbus_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	if (strcmp(name, "bootbus") == 0) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

/*
 * bbus_attach - hey, we're alive!
 */
/*ARGSUSED*/
static int
bbus_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int cpu_id = determine_attached_cpu(devi);
	struct autovec *table = ZALLOC_SLEEP_N(struct autovec, MAX_PRI);

	if (table == 0) {
		return (DDI_FAILURE);
	}

	set_private_data(devi, cpu_id, table);
	ddi_report_dev(devi);

	return (DDI_SUCCESS);
}

static struct dev_ops bbus_ops = {
	DEVO_REV,		/* rev */
	0,			/* refcnt */
	bbus_getinfo,		/* getinfo */
	bbus_identify,		/* identify */
	nulldev,		/* probe */
	bbus_attach,		/* attach */
	nulldev,		/* detach */
	nulldev,		/* reset */
	(struct cb_ops *)0,	/* cb_ops */
	&bbus_bus_ops		/* bus_ops */
};


/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* modops */
	"Boot-Bus Nexus",	/* linkinfo */
	&bbus_ops,		/* dev_ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,		/* rev */
	(void *)&modldrv,	/* linkage[4] */
	NULL
};

int
_fini(void)
{
	return (EBUSY);
}

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
