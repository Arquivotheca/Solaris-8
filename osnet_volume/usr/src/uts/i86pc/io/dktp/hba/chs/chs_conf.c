/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)chs_conf.c	1.5	99/08/18 SMI"

/*
 * CHS-specific configuration routines
 */

#include "chs.h"

nops_t	*chs_conf[] = {
	&viper_nops,
	NULL
};

int	chs_disks_scsi;		/* allow RDY drives to be accessed via scsi */

nops_t *
chs_hbatype(dev_info_t	 *dip,
		int		**rpp,
		int		 *lp,
		bool_t		  probing)
{
	bus_t	  bus_type = 0;
	int	  *regp = NULL;
	int	  reglen;
	int	  *pidp = NULL;		/* ptr to the product id property */
	int	  pidlen = 0;		/* length of the array */
	char	  *parent_type = NULL;
	int	  parentlen = 0;
	nops_t	  **nopspp = NULL;


	/*
	 * The parent-type property is a hack for 2.4 compatibility, since
	 * if you're on an EISA+PCI machine under Solaris 2.4 the PCI devices
	 * end up under EISA and there's no way to tell which one you're
	 * probing.  Getting the parent's device_type is the real right way.
	 */
	if ((ddi_getlongprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "parent-type", (caddr_t)&parent_type,
	    &parentlen) != DDI_PROP_SUCCESS)) {

		if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
		    0, "device_type", (caddr_t)&parent_type,
		    &parentlen) != DDI_PROP_SUCCESS) {

			if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
			    0, "bus-type", (caddr_t)&parent_type,
			    &parentlen) != DDI_PROP_SUCCESS) {
				parent_type = NULL;	/* Don't know. */
			}
		}
	}


	/* If we don't know, it's PCI */
	if (parent_type && strcmp(parent_type, "eisa") == 0)
		bus_type = BUS_TYPE_EISA;
	else
		bus_type = BUS_TYPE_PCI;


	/* get the hba's address */
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
	    "reg", (caddr_t)&regp, &reglen) != DDI_PROP_SUCCESS) {
		if (parent_type != NULL)
			kmem_free(parent_type, parentlen);
		MDBG4(("chs_hbatype: reg property not found\n"));
		return (NULL);
	}

	/* pass the reg property back to chs_cfg_init() */
	if (rpp != NULL) {
		*rpp = regp;
		*lp = reglen;
	}

	/* the product id property is optional, if it's not specified */
	/* then the chip specific modules will use default values */
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
	    "product-id", (caddr_t)&pidp, &pidlen) != DDI_PROP_SUCCESS) {
		MDBG4(("chs_hbatype: product-id property not found\n"));
		pidp = NULL;
		pidlen = 0;
	}

	/* determine which chsops vector table to use */
	for (nopspp = chs_conf; *nopspp != NULL; nopspp++) {
		if ((*nopspp)->chs_probe(dip, regp, reglen, pidp, pidlen,
		    bus_type, probing)) {
			break;
		}
	}

	if (parent_type != NULL)
		kmem_free(parent_type, parentlen);
	if (pidp != NULL)
		kmem_free(pidp, pidlen);
	if (!nopspp || !rpp)
		kmem_free(regp, reglen);

	return (*nopspp);
}


/*
 * Determine the interrupt request line for this HBA
 */
/*ARGSUSED*/
bool_t
chs_get_irq_pci(chs_t		*chsp,
			int		*regp,
			int		 reglen)
{
	ddi_acc_handle_t handle;

	if (pci_config_setup(chsp->dip, &handle) != DDI_SUCCESS)
		return (FALSE);
	chsp->irq = pci_config_getb(handle, PCI_CONF_ILINE);
	pci_config_teardown(&handle);

	cmn_err(CE_CONT, "?chs: pci slot=%d,%d reg=0x%x\n",
		*regp, *(regp + 1), chsp->reg);
	return (TRUE);
}

bool_t
chs_cfg_init(chs_t		*chsp)
{
	int	*regp;		/* ptr to the reg property */
	int	reglen; 	/* length of the array */

	regp = chsp->regp;
	reglen = chsp->reglen;

	if ((chsp->rnum = CHS_RNUMBER(chsp, regp, reglen)) < 0) {
		MDBG4(("chs_cfg_init: no reg\n"));
		return (FALSE);
	}

	if (!CHS_GET_IRQ(chsp, regp, reglen)) {
		MDBG4(("chs_cfg_init: no irq\n"));
		return (FALSE);
	}

	return (TRUE);
}

/*
 * Determine the i/o register base address for this HBA
 * PCI, read reg from the config register
 */
/*ARGSUSED*/
int
chs_get_reg_pci(chs_t	*chsp,
			int	*regp,
			int	 reglen)
{
	ddi_acc_handle_t handle;

	if (pci_config_setup(chsp->dip, &handle) != DDI_SUCCESS)
		return (-1);
	chsp->reg =
	    pci_config_getl(handle, PCI_CONF_BASE0) & PCI_BASE_IO_ADDR_M;
	pci_config_teardown(&handle);

	return (CHS_PCI_RNUMBER);
}
