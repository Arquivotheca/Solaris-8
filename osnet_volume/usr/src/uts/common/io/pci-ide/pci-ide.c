/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)pci-ide.c	1.7	99/08/17 SMI"

/*
 *	PCI-IDE bus nexus driver
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>
#include <sys/dma_engine.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/kmem.h>
#include <sys/pci.h>
#include <sys/promif.h>

int	pciide_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);

#define	PCIIDE_NATIVE_MODE(dip)						\
	(!ddi_prop_exists(DDI_DEV_T_ANY, (dip), DDI_PROP_DONTPASS, 	\
			  "compatibility-mode"))

#define PCIIDE_PRE26(dip)	\
	ddi_prop_exists(DDI_DEV_T_ANY, (dip), 0, "ignore-hardware-nodes")

#ifdef DEBUG
static int pci_ide_debug = 0;
#define	PDBG(fmt)				\
		if (pci_ide_debug) {		\
			prom_printf fmt;	\
		}
#else
#define	PDBG(fmt)
#endif

#ifndef	TRUE
#define	TRUE	1
#endif
#ifndef	FALSE
#define	FALSE	0
#endif

/*
 * bus_ops functions
 */

static int		pciide_bus_map(dev_info_t *dip, dev_info_t *rdip,
				ddi_map_req_t *mp, off_t offset, off_t len,
				caddr_t *vaddrp);

static	ddi_intrspec_t	pciide_get_intrspec(dev_info_t *dip, dev_info_t *rdip,
				u_int inumber);

static	int		pciide_ddi_ctlops(dev_info_t *dip, dev_info_t *rdip,
				ddi_ctl_enum_t ctlop, void *arg,
				void *result);

static	int		pciide_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
				ddi_intrspec_t intrspec,
				ddi_iblock_cookie_t *iblock_cookiep,
				ddi_idevice_cookie_t *idevice_cookiep,
				u_int (*int_handler)(caddr_t int_handler_arg),
				caddr_t int_handler_arg, int kind );

static	void		pciide_remove_intrspec(dev_info_t *dip,
				dev_info_t *rdip, ddi_intrspec_t intrspec,
				ddi_iblock_cookie_t iblock_cookie);


/*
 * Local Functions
 */
static	int	pciide_initchild(dev_info_t *mydip, dev_info_t *cdip);

static	void	pciide_compat_setup(dev_info_t *mydip, dev_info_t *cdip,
				    int dev);
static	int	pciide_pre26_rnumber_map(dev_info_t *mydip, int rnumber);
static	int	pciide_map_rnumber(int canonical_rnumber, int pri_native,
				   int sec_native);

/*
 * External functions
 */


extern ddi_intrspec_t i_ddi_get_intrspec(dev_info_t *dip, dev_info_t *rdip, 
					 uint_t inumber);


/*
 * Config information
 */

struct bus_ops pciide_bus_ops = {
	BUSO_REV,
	pciide_bus_map,
	pciide_get_intrspec,
	pciide_add_intrspec,
	pciide_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	pciide_ddi_ctlops,
	ddi_bus_prop_op,
#if (BUSO_REV >= 3)
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
#endif
};

struct dev_ops pciide_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	pciide_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&pciide_bus_ops	/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This is PCI-IDE bus driver */
	"pciide nexus driver for 'PCI-IDE'",
	&pciide_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};


int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
pciide_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	uint16_t cmdreg;
	ddi_acc_handle_t conf_hdl = NULL;
	caddr_t	mp_addr;
	ddi_device_acc_attr_t dev_attr;
	int rc;


	if (cmd == DDI_ATTACH) {

		/*
		 * Make sure bus-mastering is enabled, even if
		 * BIOS didn't.
		 */

		dev_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
		dev_attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
		dev_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

		rc = ddi_regs_map_setup(dip, 0, &mp_addr, 0, 0,
			&dev_attr, &conf_hdl);

		/*
		 * In case of error, return SUCCESS. This is because
		 * bus-mastering could be already enabled by BIOS.
		 */
		if (rc != DDI_SUCCESS)
			return (DDI_SUCCESS);

		cmdreg = ddi_get16(conf_hdl, (uint16_t *)PCI_CONF_COMM);
		if ((cmdreg & PCI_COMM_ME) == 0) {
			ddi_put16(conf_hdl, (uint16_t *)PCI_CONF_COMM,
			    cmdreg | PCI_COMM_ME);
		}
		ddi_regs_map_free(&conf_hdl);

		return (DDI_SUCCESS);
	} else {
		return (DDI_FAILURE);
	}
}


/*ARGSUSED*/
static int
pciide_ddi_ctlops(	dev_info_t	*dip,
			dev_info_t	*rdip,
			ddi_ctl_enum_t	 ctlop,
			void		*arg,
			void		*result )
{
	PDBG(("pciide_bus_ctl\n"));

	switch (ctlop) {
	case DDI_CTLOPS_INITCHILD:

		return (pciide_initchild(dip, (dev_info_t *)arg));
 
	case DDI_CTLOPS_NREGS:
		*(int *)result = 3;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	{
		int	controller;
		int	rnumber;
		int	rc;
		int	tmp;

		/*
		 * Adjust the rnumbers based on which controller instance
		 * is requested; adjust for the 2 tuples per controller.
		 */
		if (strcmp("0", ddi_get_name_addr(rdip)) == 0)
			controller = 0;
		else
			controller = 1;


		switch (rnumber = *(int *)arg) {
		case 0:
		case 1:
			rnumber += (2 * controller);
			break;
		case 2:
			rnumber = 4;
			break;
		default:
			PDBG(("pciide_ctlops invalid rnumber\n"));
			return (DDI_FAILURE);
		}


		if (PCIIDE_PRE26(dip)) {
			int	old_rnumber;
			int	new_rnumber;

			old_rnumber = rnumber;
			new_rnumber
				= pciide_pre26_rnumber_map(dip, old_rnumber);
			PDBG(("pciide rnumber old %d new %d\n",
				old_rnumber, new_rnumber));
			rnumber = new_rnumber;
		}

		/*
		 * Add 1 to skip over the PCI config space tuple 
		 */
		rnumber++;

		/*
		 * If it's not tuple #2 pass the adjusted request to my parent
		 */
		if (*(int *)arg != 2) {
			return (ddi_ctlops(dip, dip, ctlop, &rnumber, result));
		}

		/*
		 * Handle my child's reg-tuple #2 here by splitting my 16 byte
		 * reg-tuple #4 into two 8 byte ranges based on the
		 * the child's controller #.
		 */

		tmp = 8;
		rc = ddi_ctlops(dip, dip, ctlop, &rnumber, &tmp);

		/*
		 * Allow for the possibility of less than 16 bytes by
		 * by checking what's actually returned for my reg-tuple #4.
		 */
		if (controller == 1) {
			if (tmp < 8)
				tmp = 0;
			else
				tmp -= 8;
		}
		if (tmp > 8)
			tmp = 8;
		*(int *)result = tmp;

		return (rc);
	}

	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}
}

/*
 * IEEE 1275 Working Group Proposal #414 says that the Primary
 * controller is "ata@0" and the Secondary controller "ata@1". 
 *
 * By the time we get here, boot Bootconf (2.6+) has created devinfo
 * nodes with the appropriate "reg", "assigned-addresses" and "interrupts"
 * properites on the pci-ide node and both ide child nodes.
 *
 * In compatibility mode the "reg" and "assigned-addresses" properties
 * of the pci-ide node are set up like this:
 *
 *   1. PCI-IDE Nexus
 *
 *	interrupts=0
 *				(addr-hi addr-mid addr-low size-hi  size-low)
 *	reg= assigned-addresses=00000000.00000000.00000000.00000000.00000000
 *				81000000.00000000.000001f0.00000000.00000008
 *				81000000.00000000.000003f4.00000000.00000004
 *				81000000.00000000,00000170.00000000.00000008
 *				81000000.00000000,00000374.00000000.00000004
 *				01000020.00000000,-[BAR4]-.00000000.00000010
 *
 * In native PCI mode the "reg" and "assigned-addresses" properties
 * would be set up like this:
 *
 *   2. PCI-IDE Nexus
 *
 *	interrupts=0
 *	reg= assigned-addresses=00000000.00000000.00000000.00000000.00000000
 *				01000010.00000000.-[BAR0]-.00000000.00000008
 *				01000014,00000000.-[BAR1]-.00000000.00000004
 *				01000018.00000000.-[BAR2]-.00000000.00000008
 *				0100001c.00000000.-[BAR3]-.00000000.00000004
 *				01000020.00000000.-[BAR4]-.00000000.00000010
 *
 *
 * In both modes the child nodes simply have the following:
 *
 *   2. primary controller (compatibility mode)
 *
 *	interrupts=14
 *	reg=00000000
 *
 *   3. secondary controller 
 *
 *	interrupts=15
 *	reg=00000001
 *
 * The pciide_bus_map() function is responsible for turning requests
 * to map primary or secondary controller rnumbers into mapping requests
 * of the appropriate regspec on the pci-ide node.
 *
 */

static int
pciide_initchild( dev_info_t *mydip, dev_info_t *cdip )
{
	struct ddi_parent_private_data *pdptr;
	struct intrspec	*ispecp;
	int	vec;
	int	*rp;
	int	proplen;
	char	name[80];
	int	dev;

	PDBG(("pciide_initchild\n"));

	/*
	 * Set the address portion of the node name based on
	 * the controller number (0 or 1) from the 'reg' property.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, cdip,
			DDI_PROP_DONTPASS, "reg", &rp,
			(u_int *)&proplen) != DDI_PROP_SUCCESS) {
		PDBG(("pciide_intchild prop error\n"));
		return (DDI_NOT_WELL_FORMED);
	}

	/*
	 * copy the controller number and
	 * free the memory allocated by ddi_prop_lookup_int_array
	 */
	dev = *rp;
	ddi_prop_free(rp);

	/*
	 * I only support two controllers per device, determine
	 * which this one is and set its unit address.
	 */
	if (dev > 1) {
		PDBG(("pciide_initchild bad dev\n"));
		return (DDI_NOT_WELL_FORMED);
	}
	(void) sprintf(name, "%d", dev);
	ddi_set_name_addr(cdip, name);

	/*
	 * determine if this instance is running in native or compat mode
	 */
	pciide_compat_setup(mydip, cdip, dev);

	/* interrupts property is required */
	if (PCIIDE_NATIVE_MODE(cdip)) {
		vec = 1;
	} else {
		/*
		 * In compatibility mode, dev 0 should always be
		 * IRQ 14 and dev 1 is IRQ 15. If for some reason
		 * this needs to be changed, do it via the interrupts
		 * property in the ata.conf file.
		 */
		vec = ddi_prop_get_int(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
				"interrupts", -1);
		if (vec == -1) {
			/* setup compatibility mode interrupts */
			if (dev == 0) {
				vec = 14;
			} else if (dev == 1) {
				vec = 15;
			} else {
				PDBG(("pciide_initchild bad intr\n"));
				return (DDI_NOT_WELL_FORMED);
			}
		}
	}

	pdptr = (struct ddi_parent_private_data *)
		kmem_zalloc((sizeof (struct ddi_parent_private_data) +
		sizeof (struct intrspec)), KM_SLEEP);
	ispecp = (struct intrspec *)(pdptr + 1);
	pdptr->par_nintr = 1;
	pdptr->par_intr = ispecp;
	ispecp->intrspec_vec = vec;
	ddi_set_parent_data(cdip, (caddr_t)pdptr);

	PDBG(("pciide_initchild okay\n"));
	return (DDI_SUCCESS);
}



static int
pciide_bus_map(	dev_info_t	*dip,
		dev_info_t	*rdip,
		ddi_map_req_t	*mp,
		off_t		 offset,
		off_t		 len,
		caddr_t		*vaddrp )
{
	dev_info_t *pdip;
	int	    rnumber = mp->map_obj.rnumber;
	int	    controller;
	int	    rc;

	PDBG(("pciide_bus_map\n"));

	if (strcmp("0", ddi_get_name_addr(rdip)) == 0)
		controller = 0;
	else
		controller = 1;

	/*
	 * Adjust the rnumbers based on which controller instance
	 * is being mapped; adjust for the 2 tuples per controller.
	 */

	switch (rnumber) {
	case 0:
	case 1:
		mp->map_obj.rnumber += (controller * 2);
		break;
	case 2:
		/*
		 * split the 16 I/O ports into two 8 port ranges
		 */
		mp->map_obj.rnumber = 4;
		if (offset + len > 8) {
			PDBG(("pciide_bus_map offset\n"));
			return (DDI_FAILURE);
		}
		if (len == 0)
			len = 8 - offset;
		offset += 8 * controller;
		break;
	default:
		PDBG(("pciide_bus_map default\n"));
		return (DDI_FAILURE);
	}

	if (PCIIDE_PRE26(dip)) {
		int	old_rnumber;
		int	new_rnumber;

		old_rnumber = mp->map_obj.rnumber;
		new_rnumber = pciide_pre26_rnumber_map(dip, old_rnumber);
		PDBG(("pciide rnumber old %d new %d\n",
			old_rnumber, new_rnumber));
		mp->map_obj.rnumber = new_rnumber;
	}

	 /*
	  * Add 1 to skip over the PCI config space tuple 
	  */
	mp->map_obj.rnumber++;


	/*
	 * pass the adjusted request to my parent
	 */
	pdip = ddi_get_parent(dip);
	rc = ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_map))
			(pdip, dip, mp, offset, len, vaddrp));

	PDBG(("pciide_bus_map %s\n", rc == DDI_SUCCESS ? "okay" : "!ok"));

	return (rc);
}


static ddi_intrspec_t
pciide_get_intrspec( dev_info_t *dip, dev_info_t *rdip, u_int inumber )
{
	struct ddi_parent_private_data *ppdptr;
	struct intrspec	*ispecp;
	int		*intpriorities;
	u_int		 num_intpriorities;
	int		 rc;

	PDBG(("pciide_get_intrspec\n"));

	/*
	 * Native mode PCI-IDE controllers share the parent's
	 * PCI interrupt line.
	 *
	 * Compatibility mode PCI-IDE controllers have their
	 * own intrspec which specifies ISA IRQ 14 or 15.
	 *
	 */
	if (PCIIDE_NATIVE_MODE(rdip)) {
		/*
		 * use the PCI INT assigned to the PCI-IDE node
		 */
		ispecp = i_ddi_get_intrspec(dip, dip, inumber);
		PDBG(("pciide_get_intr okay\n"));
		return (ispecp);
	}

	/*
	 * Else compatibility mode, use the ISA IRQ
	 */

	ppdptr = (struct ddi_parent_private_data *)ddi_get_parent_data(rdip);
	if (!ppdptr) {
		PDBG(("pciide_get_intr null\n"));
		return (NULL);
	}

	/*
	 * validate the interrupt number.
	 */
	if (inumber >= ppdptr->par_nintr) {
		PDBG(("pciide_get_inum\n"));
		return (NULL);
	}

	ispecp = &ppdptr->par_intr[inumber];
	ASSERT(ispecp);

	/* check if the intrspec has been initialized */
	if (ispecp->intrspec_pri != 0) {
		PDBG(("pciide_get_init\n"));
		return (ispecp);
	}

	/* Use a default of level 5  */
	ispecp->intrspec_pri = 5;

	/*
	 * If there's an interrupt-priorities property, use it to 
	 * over-ride the default interrupt priority.  
	 */
	rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
				      "interrupt-priorities", &intpriorities,
				      &num_intpriorities);
	if (rc == DDI_PROP_SUCCESS) {
		if (inumber < num_intpriorities)
			ispecp->intrspec_pri = intpriorities[inumber];
		ddi_prop_free(intpriorities);
	}

	PDBG(("pciide_get_interspec %s\n",
		rc == DDI_PROP_SUCCESS ? "okay" : "!ok"));

	return (ispecp);
}


static int
pciide_add_intrspec(	dev_info_t	*dip,
			dev_info_t	*rdip,
			ddi_intrspec_t	 intrspec,
			ddi_iblock_cookie_t *iblock_cookiep,
			ddi_idevice_cookie_t *idevice_cookiep,
			u_int		(*int_handler)(caddr_t int_handler_arg),
			caddr_t		int_handler_arg,
			int kind )
{
	int	rc;

	if (PCIIDE_NATIVE_MODE(rdip)) {
		dip = ddi_get_parent(dip);
		rdip = dip;
	} else {
		/* get ptr to the root node */
		dip = ddi_root_node();
	}

	rc = (*(DEVI(dip)->devi_ops->devo_bus_ops->bus_add_intrspec))
			(dip, rdip, intrspec, iblock_cookiep, idevice_cookiep,
			 int_handler, int_handler_arg, kind);
	PDBG(("pciide_add_intrspec rc=%d", rc));
	return (rc);
}

static void
pciide_remove_intrspec(	dev_info_t	*dip,
			dev_info_t	*rdip,
			ddi_intrspec_t	 intrspec,
			ddi_iblock_cookie_t iblock_cookie )
{

	if (PCIIDE_NATIVE_MODE(rdip)) {
		dip = ddi_get_parent(dip);
		rdip = dip;
	} else {
		/* get ptr to the root node */
		dip = ddi_root_node();
	}

	/* request parent to remove an interrupt specification */
	(*(DEVI(dip)->devi_ops->devo_bus_ops->bus_remove_intrspec))
				(dip, rdip, intrspec, iblock_cookie);
	return;
}


static void
pciide_compat_setup( dev_info_t *mydip, dev_info_t *cdip, int dev )
{
	int	class_code;
	int	rc;

	class_code = ddi_prop_get_int(DDI_DEV_T_ANY, mydip, DDI_PROP_DONTPASS,
				"class-code", 0);

	if ((dev == 0 && !(class_code & PCI_IDE_IF_NATIVE_PRI))
	||  (dev == 1 && !(class_code & PCI_IDE_IF_NATIVE_SEC))) {
		rc = ddi_prop_update_int(DDI_DEV_T_NONE, cdip,
					 "compatibility-mode", 1);
		if (rc == DDI_PROP_SUCCESS)
			return;
		cmn_err(CE_WARN, "pciide prop error compat mode");
	}
	return;
}


static int
pciide_pre26_rnumber_map( dev_info_t *mydip, int rnumber )
{
	int	pri_native;
	int	sec_native;
	int	class_code;

	class_code = ddi_prop_get_int(DDI_DEV_T_ANY, mydip, DDI_PROP_DONTPASS,
				"class-code", 0);

	pri_native = (class_code & PCI_IDE_IF_NATIVE_PRI) ? TRUE : FALSE;
	sec_native = (class_code & PCI_IDE_IF_NATIVE_SEC) ? TRUE : FALSE;
	
	return (pciide_map_rnumber(rnumber, pri_native, sec_native));

}

/*
	The canonical order of the reg property tuples for the
	Base Address Registers is supposed to be:

	primary controller (BAR 0)
	primary controller (BAR 1)
	secondary controller (BAR 2)
	secondary controller (BAR 3)
	bus mastering regs (BAR 4)

	For 2.6, bootconf has been fixed to always generate the
	reg property (and assigned-addresses property) tuples
	in the above order.

	But in releases prior to 2.6 the order varies depending
	on whether compatibility or native mode is being used for
	each controller. There ends up being four possible
	orders:

	BM, P0, P1, S0, S1	primary compatible, secondary compatible
	S0, S1, BM, P0, P1	primary compatible, secondary native
	P0, P1, BM, S0, S1	primary native, secondary compatible
	P0, P1, S0, S1, BM	primary native, secondary native

	where: Px is the primary tuples, Sx the secondary tuples, and
	B the Bus Master tuple.

	Here's the results for each of the four states:

		0, 1, 2, 3, 4

	CC	1, 2, 3, 4, 0
	CN	3, 4, 0, 1, 2
	NC	0, 1, 3, 4, 2
	NN	0, 1, 2, 3, 4

	C=compatible (!native) == 0
	N=native == 1

	Here's the transformation matrix:
*/

static	int	pciide_transform[2][2][5] = {
/*  P  S  */
/* [C][C] */	+1, +1, +1, +1, -4,
/* [C][N] */	+3, +3, -2, -2, -2,
/* [N][C] */	+0, +0, +1, +1, -2,
/* [N][N] */	+0, +0, +0, +0, +0
};


static int
pciide_map_rnumber(	int	rnumber,
			int	pri_native,
			int	sec_native )
{

	/* transform flags into indexes */
	pri_native = pri_native ? 1 : 0;
	sec_native = sec_native ? 1 : 0;

	rnumber += pciide_transform[pri_native][sec_native][rnumber];
	return (rnumber);
}
