/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcmem.c	1.26	99/03/11 SMI"

/* #define	PCMEM_DEBUG */

/*
 *  PCMCIA Memory Nexus Driver
 *
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/ksynch.h>
#include <sys/conf.h>

/*
 * PCMCIA and DDI related header files
 */
#include <sys/pccard.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>


/*
 * Device Operations (dev_ops) Structure
 */
static int pcmem_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int pcmem_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

dev_info_t *pcmem_first_found;
struct par_list *impl_make_parlist(major_t);

/*
 *      bus nexus operations.
 */

static int
pcmem_ctlops(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t o,
						void *a, void *v);


static struct bus_ops pcmem__bus_ops = {
#if defined(BUSO_REV) && BUSO_REV >= 2
	BUSO_REV,		/* XXX */
	nullbusmap,
	0,			/* ddi_intrspec_t (*bus_get_intrspec)(); */
	0,			/* int (*bus_add_intrspec)(); */
	0,			/* void  (*bus_remove_intrspec)(); */
	i_ddi_map_fault,
	ddi_no_dma_map,
	ddi_no_dma_allochdl,
	ddi_no_dma_freehdl,
	ddi_no_dma_bindhdl,
	ddi_no_dma_unbindhdl,
	ddi_no_dma_flush,
	ddi_no_dma_win,
	ddi_no_dma_mctl,
	pcmem_ctlops,		/* ddi_ctlops   */
	ddi_bus_prop_op
#else
	nullbusmap,
	0,			/* ddi_intrspec_t (*bus_get_intrspec)(); */
	0,			/* int (*bus_add_intrspec)(); */
	0,			/* void  (*bus_remove_intrspec)(); */
	i_ddi_map_fault,
	ddi_no_dma_map,
	ddi_no_dma_mctl,
	pcmem_ctlops,		/* ddi_ctlops   */
	ddi_bus_prop_op
#endif
};



static struct dev_ops pcmem_ops = {
	DEVO_REV,		/* devo_rev	*/
	0,			/* refcnt	*/
	ddi_no_info,		/* info		*/
	nulldev,		/* identify	*/
	nulldev,		/* probe	*/
	pcmem_attach,		/* attach	*/
	pcmem_detach,		/* detach	*/
	nulldev,		/* reset (currently not supported) */
	(struct cb_ops *)NULL,	/* cb_ops pointer for leaf driver */
	&pcmem__bus_ops		/* bus_ops pointer for nexus driver */
};



/*
 * Module linkage information for the kernel
 */
extern struct mod_ops mod_driverops;

static struct modldrv md = {
	&mod_driverops,			/* Type of module */
	"PCMCIA Memory Nexus V2.0",	/* Name of the module */
	&pcmem_ops,			/* Device Operation Structure */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&md,
	NULL
};


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

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}



/*
 * pcmem_attach()
 *
 */
static int
pcmem_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	char		adapter [MODMAXNAMELEN+1];
	static void	pcmem_clone_and_load_proto(dev_info_t *);

	/* resume from a checkpoint */
	if (cmd == DDI_RESUME) {
		return (DDI_SUCCESS);
	}

	if (pcmem_first_found == NULL) {
		pcmem_first_found = dip;
	}

	(void) pcmem_clone_and_load_proto(dip);

	(void) strcpy(adapter, "pcram");
	(void) modload("drv", adapter);

	ddi_report_dev(dip);

#ifdef  PCMEM_DEBUG
	cmn_err(CE_CONT, "pcmem_attach - exit\n");
#endif

	return (DDI_SUCCESS);
}



static int
pcmem_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{

	/* suspend */
	if (cmd == DDI_SUSPEND) {
		return (DDI_SUCCESS);
	}

	if (cmd != DDI_DETACH) {
		if (dip == pcmem_first_found)
			pcmem_first_found = NULL;

		cmn_err(CE_NOTE, "pcmem_detach: cmd != DDI_DETACH\n");
		return (DDI_FAILURE);
	}

#ifdef  PCMEM_DEBUG
	cmn_err(CE_CONT, "pcmem_detach - exit\n");
#endif
	/* Do not need to do ddi_prop_remove_all */
	return (DDI_SUCCESS);
}


/*ARGSUSED*/
static int
pcmem_ctlops(dev_info_t *dip, dev_info_t *rdip,
		ddi_ctl_enum_t ctlop, void *arg, void *result)
{

	char    name[MAXNAMELEN];
	int	techreg, cissp;

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0) {
			return (DDI_FAILURE);
		}
#ifdef  PCMEM_DEBUG
		cmn_err(CE_CONT,
			"?pcmem_ctlops: %s%d at %s in socket %d\n",
			ddi_get_name(rdip), ddi_get_instance(rdip),
			ddi_get_name(dip),
			ddi_getprop(DDI_DEV_T_NONE, rdip,
					DDI_PROP_DONTPASS,
					"socket", -1));
#endif
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		/*
		 * XXXX - Read card CIS to determine technology
		 *	region(tn) and CIS space(dn).
		 *	Refer to Bugid 1179336.
		 */

		/*
		 * see cis_handler.h for CISTPL_DEVICE
		 *	and CISTPL_DEVICE_A
		 *
		 * CISTPL_DEVICE_DTYPE_NULL	0x00	NULL device
		 * CISTPL_DEVICE_DTYPE_ROM	0x01	ROM
		 * CISTPL_DEVICE_DTYPE_OTPROM	0x02	OTPROM
		 * CISTPL_DEVICE_DTYPE_EPROM	0x03    EPROM
		 * CISTPL_DEVICE_DTYPE_EEPROM	0x04	EEPROM
		 * CISTPL_DEVICE_DTYPE_FLASH	0x05	FLASH
		 * CISTPL_DEVICE_DTYPE_SRAM	0x06	SRAM
		 * CISTPL_DEVICE_DTYPE_DRAM	0x07	DRAM
		 *
		 */
		/*
		 * XXXX - For now set to default SRAM device
		 */
		techreg = CISTPL_DEVICE_DTYPE_SRAM;
		cissp = 0;
		(void) sprintf(name, "%d,%d", techreg, cissp);
		ddi_set_name_addr((dev_info_t *)arg, name);
#ifdef  PCMEM_DEBUG
		cmn_err(CE_CONT,
			"pcmem_ctlops - DDI_CTLOPS_INITCHILD\n");
#endif
		return (DDI_SUCCESS);

	case DDI_CTLOPS_UNINITCHILD:
		ddi_set_name_addr((dev_info_t *)arg, NULL);
#ifdef  PCMEM_DEBUG
		cmn_err(CE_CONT,
			"pcmem_ctlops - DDI_CTLOPS_UNINITCHILD\n");
#endif
		return (DDI_SUCCESS);

	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}
}


struct pcmem_drv_instances {
	struct pcmem_drv_instances *next;
	char name[MAXNAMELEN];
	int instance;
} *pcmem_drv_instances;


struct pcmem_drv_instances *
pcmem_find_instance(char *driver)
{
	struct pcmem_drv_instances *inst = pcmem_drv_instances;

	while (inst != NULL) {
		if (strcpy(inst->name, driver) == 0)
			break;
		inst = inst->next;
	}
	return (inst);
}


static void
pcmem_enumerate_instances(dev_info_t *dip)
{
	char *driver;
	dev_info_t *child, *sibling;
	struct pcmem_drv_instances *inst;

	for (child = (dev_info_t *)DEVI(dip)->devi_child; child != NULL;
	    child = (dev_info_t *)DEVI(child)->devi_child) {
		driver = ddi_get_name(child);
		inst = pcmem_find_instance(ddi_get_name(child));
		for (sibling = (dev_info_t *)DEVI(child)->devi_sibling;
			sibling != NULL;
			sibling = (dev_info_t *)DEVI(sibling)->devi_sibling) {
			if (inst == NULL) {
				inst = kmem_zalloc(
					sizeof (struct pcmem_drv_instances),
					KM_NOSLEEP);
				(void) strcpy(inst->name, driver);
				inst->next = pcmem_drv_instances;
				pcmem_drv_instances = inst;
			}
			if (ddi_get_instance(sibling) < inst->instance)
				inst->instance = ddi_get_instance(sibling);
		}
		if (inst != NULL)
			inst->instance++;
	}
}


pcmem_next_instance(char *driver)
{
	struct pcmem_drv_instances *inst;

	inst = pcmem_find_instance(driver);
	if (inst != NULL) {
		return (inst->instance++);
	}
	return (0);
}


static void
pcmem_clone_and_load_proto(dev_info_t *dip)
{
	extern int	impl_proto_to_cf2(dev_info_t *);
	dev_info_t	*child, *proto;


	if (dip != pcmem_first_found && DEVI(dip)->devi_child == NULL) {
		/* we clone the first one here */
		/* CSTYLED */
		for (proto = (dev_info_t *)DEVI(DEVI(pcmem_first_found)->devi_child);
		    proto != NULL;
		    proto = (dev_info_t *)DEVI(proto)->devi_sibling) {
			child = ddi_add_child(dip, ddi_get_name(proto),
					DEVI_PSEUDO_NODEID,
					pcmem_next_instance(
						ddi_get_name(proto)));
			/*
			 * XXX: note that copy_prop of hardware nodes
			 * should be done here to make a true clone
			 */
			if (child) {
				/* XXX function return ignored */
				(void) impl_proto_to_cf2(child);
			}
		}
	} else if (dip == pcmem_first_found) {
		(void) pcmem_enumerate_instances(dip);
	}
}
