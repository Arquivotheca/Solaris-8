/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci.c	1.47	99/10/11 SMI"

/*
 *	Host to PCI local bus driver
 */

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include "pci_nexus_internal.h"

#define	PCI_HOTPLUG

#if	defined(PCI_HOTPLUG)
#include <sys/hotplug/pci/pcihp.h>
#endif

/*
 * The "pci_autoconfig" module gets loaded and called early in system
 * startup, when it's still safe to call the BIOS and ask what configuration
 * mechanism to use.  Pci_autoconfig leaves the answer in pci_bios_cfg_type.
 */

#if	defined(PCI_HOTPLUG)
/*
 * For PCI Hotplug support, the misc/pcihp module provides devctl control
 * device and cb_ops functions to support hotplug operations.
 */
char _depends_on[] = "misc/pci_autoconfig misc/pcihp";
#else
char _depends_on[] = "misc/pci_autoconfig";
#endif
extern int pci_bios_cfg_type;

/*
 * These two variables can be used to force a configuration mechanism or
 * to force which function is used to probe for the presence of the PCI bus.
 */
int	PCI_CFG_TYPE = 0;
int	PCI_PROBE_TYPE = 0;

/*
 * This mutex protects all PCI configuration space accesses.  Note that
 * it is shared across all instances of this driver, since multiple PCI
 * host bridges act a lot like a single device - they share registers, for
 * instance.
 */
static	kmutex_t pci_mutex;

/*
 * These function pointers lead to the actual implementation routines
 * for configuration space access.  Normally they lead to either the
 * pci_mech1_* or pci_mech2_* routines, but they can also lead to
 * routines that work around chipset bugs.
 */
static	uchar_t (*pci_getb_func)(int bus, int dev, int func, int reg);
static	ushort_t (*pci_getw_func)(int bus, int dev, int func, int reg);
static	ulong_t (*pci_getl_func)(int bus, int dev, int func, int reg);
static	void (*pci_putb_func)(int bus, int dev, int func, int reg,
				uchar_t val);
static	void (*pci_putw_func)(int bus, int dev, int func, int reg,
				ushort_t val);
static	void (*pci_putl_func)(int bus, int dev, int func, int reg,
				ulong_t val);


/*
 * Bus Operation functions
 */
static int pci_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *,
	off_t, off_t, caddr_t *);
static int pci_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
	void *, void *);
static ddi_intrspec_t pci_get_intrspec(dev_info_t *, dev_info_t *, uint_t);

struct bus_ops pci_bus_ops = {
	BUSO_REV,
	pci_bus_map,
	pci_get_intrspec,
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
	pci_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

/*
 * Device Node Operation functions
 */
static int pci_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int pci_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int pci_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
	void *arg, void **result);

struct dev_ops pci_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	pci_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	pci_attach,		/* attach */
	pci_detach,		/* detach */
	nulldev,		/* reset */
#if	defined(PCI_HOTPLUG)
	&pcihp_cb_ops,		/* driver operations */
#else
	(struct cb_ops *)0,	/* driver operations */
#endif
	&pci_bus_ops		/* bus operations */

};

/*
 * Internal routines in support of particular pci_ctlops.
 */
static int pci_removechild(dev_info_t *child);
static int pci_initchild(dev_info_t *child);

/*
 * Probing
 */
static int pci_probe_direct(void);
static int pci_get_cfg_type(void);

/*
 * Miscellaneous internal functions
 */
static int pci_get_devid(dev_info_t *child, uint_t *bus, uint_t *devnum,
	uint_t *funcnum);
static int pci_devclass_to_ipl(int class);
static int pci_get_reg_prop(dev_info_t *dip, pci_regspec_t *pci_rp);

/*
 * These are the access routines.  The pci_bus_map sets the handle
 * to point to these.
 */
static uint8_t pci_config_rd8(ddi_acc_impl_t *hdlp, uint8_t *addr);
static uint16_t pci_config_rd16(ddi_acc_impl_t *hdlp, uint16_t *addr);
static uint32_t pci_config_rd32(ddi_acc_impl_t *hdlp, uint32_t *addr);
static uint64_t pci_config_rd64(ddi_acc_impl_t *hdlp, uint64_t *addr);

static void pci_config_wr8(ddi_acc_impl_t *hdlp, uint8_t *addr,
				uint8_t value);
static void pci_config_wr16(ddi_acc_impl_t *hdlp, uint16_t *addr,
				uint16_t value);
static void pci_config_wr32(ddi_acc_impl_t *hdlp, uint32_t *addr,
				uint32_t value);
static void pci_config_wr64(ddi_acc_impl_t *hdlp, uint64_t *addr,
				uint64_t value);

static void pci_config_rep_rd8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_rd16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_rd32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_rd64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

static void pci_config_rep_wr8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_wr16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_wr32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_wr64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);


extern impl_ddi_merge_child(dev_info_t *child);


/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module */
	"host to PCI nexus driver",
	&pci_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	mutex_init(&pci_mutex, NULL, MUTEX_DRIVER, NULL);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int rc;

	rc = mod_remove(&modlinkage);
	if (rc == 0)
		mutex_destroy(&pci_mutex);

	return (rc);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
pci_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
pci_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	static boolean_t pci_initialized = B_FALSE;

	/*
	 * Systems with multiple PCI host bridges will have multiple
	 * instances of this driver.  Generally we ignore this - we use
	 * global data for dispatching requests, a global mutex, etc.  This
	 * is an appropriate strategy for current x86 host bridges where
	 * the configuration registers are shared between the bridges.  It
	 * is exactly the wrong strategy for a system with multiple host
	 * bridges that use independent registers.
	 *
	 * The only practical impact of multiple instances of this driver
	 * is that "attach" can be called more than once.  We use the global
	 * mutex to protect ourselves, and we ensure that we only do the
	 * initialization once.
	 */
	mutex_enter(&pci_mutex);

	if (!pci_initialized) {

		switch (pci_get_cfg_type()) {
		case PCI_MECHANISM_1:
			if (pci_is_broken_orion()) {
				pci_getb_func = pci_orion_getb;
				pci_getw_func = pci_orion_getw;
				pci_getl_func = pci_orion_getl;
				pci_putb_func = pci_orion_putb;
				pci_putw_func = pci_orion_putw;
				pci_putl_func = pci_orion_putl;
			} else {
				pci_getb_func = pci_mech1_getb;
				pci_getw_func = pci_mech1_getw;
				pci_getl_func = pci_mech1_getl;
				pci_putb_func = pci_mech1_putb;
				pci_putw_func = pci_mech1_putw;
				pci_putl_func = pci_mech1_putl;
			}
			break;

		case PCI_MECHANISM_2:
			if (pci_check_neptune()) {
				/*
				 * The BIOS for some systems with the Intel
				 * Neptune chipset seem to default to #2 even
				 * though the chipset can do #1.  Override
				 * the BIOS so that MP systems will work
				 * correctly.
				 */

				pci_getb_func = pci_neptune_getb;
				pci_getw_func = pci_neptune_getw;
				pci_getl_func = pci_neptune_getl;
				pci_putb_func = pci_neptune_putb;
				pci_putw_func = pci_neptune_putw;
				pci_putl_func = pci_neptune_putl;
			} else {
				pci_getb_func = pci_mech2_getb;
				pci_getw_func = pci_mech2_getw;
				pci_getl_func = pci_mech2_getl;
				pci_putb_func = pci_mech2_putb;
				pci_putw_func = pci_mech2_putw;
				pci_putl_func = pci_mech2_putl;
			}
			break;

		default:
			/* Not sure what to do here. */
			cmn_err(CE_WARN, "pci:  Unknown configuration type");
			mutex_exit(&pci_mutex);
			return (DDI_FAILURE);
		}

		pci_initialized = B_TRUE;
	}

	mutex_exit(&pci_mutex);

	if (ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    "device_type", (caddr_t)"pci", 4) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "pci:  'device_type' prop create failed");
	}

#if	defined(PCI_HOTPLUG)
	/*
	 * Initialize hotplug support on this bus. At minimum
	 * (for non hotplug bus) this would create ":devctl" minor
	 * node to support DEVCTL_DEVICE_* and DEVCTL_BUS_* ioctls
	 * to this bus.
	 */
	if (pcihp_init(devi) != DDI_SUCCESS)
		cmn_err(CE_WARN, "pci: Failed to setup hotplug framework");
#endif

	ddi_report_dev(devi);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
pci_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
#if	defined(PCI_HOTPLUG)
	/*
	 * Uninitialize hotplug support on this bus.
	 */
	(void) pcihp_uninit(devi);
#endif

	return (DDI_SUCCESS);
}

static int
pci_get_cfg_type()
{
	/* Check to see if the config mechanism has been set in /etc/system */
	switch (PCI_CFG_TYPE) {
	default:
	case 0:
		break;
	case 1:
		return (PCI_MECHANISM_1);
	case 2:
		return (PCI_MECHANISM_2);
	case -1:
		return (PCI_MECHANISM_NONE);
	}

	/* call one of the PCI detection algorithms */
	switch (PCI_PROBE_TYPE) {
	default:
	case 0:
		/* This is determined by pci_autoconfig early in startup. */
		return (pci_bios_cfg_type);
	case 1:
		return (pci_probe_direct());
	case -1:
		return (PCI_MECHANISM_NONE);
	}
}


/*
 * CAUTION:  This code (pci_probe_direct) has not been maintained or exercised
 * in a long time, and probably has bugs.  02 July 1996.
 */
static int
pci_probe_direct(void)
{
	unchar	old;
	unchar	new;
	unchar	tmp;

	/*
	 * The Intel Neptune chipset defines this extra register
	 * to enable Config Mechanism #1.
	 */
	/* XXX: Reserved bits? */
	outb(PCI_PMC, 1);

	/* try writing and reading zeros to the PCI port */
	outl(PCI_CONFADD, 0);
	if (inl(PCI_CONFADD) != 0)
		goto try_mech2;

	/* try turning on the configuration space enable bit */
	outl(PCI_CONFADD, PCI_CADDR1(0, 0, 0, 0));
	if (inl(PCI_CONFADD) != PCI_CADDR1(0, 0, 0, 0))
		goto try_mech2;

	/* check the base class of the Host to PCI bridge */
	/* XXX:  Why dev 0 here and dev 5 below? */
	outl(PCI_CONFADD, PCI_CADDR1(0, 0, 0, PCI_CONF_BASCLASS));
	tmp = inb(PCI_CONFDATA | (PCI_CONF_BASCLASS & 0x3));

	if (tmp == PCI_CLASS_BRIDGE || tmp == 0) {
		/* leave configuration space enabled ??? */
		return (PCI_MECHANISM_1);
	}


	/* check the vendor and device id's of the Host to PCI bridge */
	outl(PCI_CONFADD, PCI_CADDR1(0, 5, 0, PCI_CONF_VENID));
	tmp = inl(PCI_CONFDATA);

	/* Aries chipset doesn't implement baseclass register */
	if (tmp == ((0x0486 << 16) | 0x8086)) {
		/* leave configuration space enabled ??? */
		return (PCI_MECHANISM_1);
	}


try_mech2:
	old =  inb(PCI_CSE_PORT);
	tmp = (old & 0xf0) | 0x10;
	outb(PCI_CSE_PORT, tmp);
	new = inb(PCI_CSE_PORT);
	if (new == tmp) {
		outb(PCI_CSE_PORT, old);
		return (PCI_MECHANISM_2);
	}

	/* don't try to restore the register if didn't change */
	if (old != new)
		outb(PCI_CSE_PORT, old);
	return (PCI_MECHANISM_NONE);
}

static int
pci_get_reg_prop(dev_info_t *dip, pci_regspec_t *pci_rp)
{
	pci_regspec_t *assigned_addr;
	int	assigned_addr_len;
	u_int 	phys_hi;
	int	i;
	int 	rc;
	int 	number;

	phys_hi = pci_rp->pci_phys_hi;
	if (((phys_hi & PCI_REG_ADDR_M) == PCI_ADDR_CONFIG) ||
		(phys_hi & PCI_RELOCAT_B))
		return (DDI_SUCCESS);

	/*
	 * the "reg" property specifies relocatable, get and interpret the
	 * "assigned-addresses" property.
	 */
	rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "assigned-addresses",
		(int **)&assigned_addr, (u_int *)&assigned_addr_len);
	if (rc != DDI_PROP_SUCCESS)
		return (DDI_FAILURE);

	/*
	 * Scan the "assigned-addresses" for one that matches the specified
	 * "reg" property entry.
	 */
	phys_hi &= PCI_CONF_ADDR_MASK;
	number = assigned_addr_len / (sizeof (pci_regspec_t) / sizeof (int));
	for (i = 0; i < number; i++) {
		if ((assigned_addr[i].pci_phys_hi & PCI_CONF_ADDR_MASK) ==
				phys_hi) {
			pci_rp->pci_phys_mid = assigned_addr[i].pci_phys_mid;
			pci_rp->pci_phys_low = assigned_addr[i].pci_phys_low;
			ddi_prop_free(assigned_addr);
			return (DDI_SUCCESS);
		}
	}

	ddi_prop_free(assigned_addr);
	return (DDI_FAILURE);
}

static int
pci_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	struct regspec reg;
	ddi_map_req_t mr;
	ddi_acc_hdl_t *hp;
	ddi_acc_impl_t *ap;
	pci_regspec_t pci_reg;
	pci_regspec_t *pci_rp;
	int 	rnumber;
	int	length;
	int	rc;
	pci_acc_cfblk_t *cfp;
	int	space;


	mr = *mp; /* Get private copy of request */
	mp = &mr;

	/*
	 * check for register number
	 */
	switch (mp->map_type) {
	case DDI_MT_REGSPEC:
		pci_reg = *(pci_regspec_t *)(mp->map_obj.rp);
		pci_rp = &pci_reg;
		if (pci_get_reg_prop(rdip, pci_rp) != DDI_SUCCESS)
			return (DDI_FAILURE);
		break;
	case DDI_MT_RNUMBER:
		rnumber = mp->map_obj.rnumber;
		/*
		 * get ALL "reg" properties for dip, select the one of
		 * of interest. In x86, "assigned-addresses" property
		 * is identical to the "reg" property, so there is no
		 * need to cross check the two to determine the physical
		 * address of the registers.
		 * This routine still performs some validity checks to
		 * make sure that everything is okay.
		 */
		rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
			DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
			(u_int *)&length);
		if (rc != DDI_PROP_SUCCESS) {
			return (DDI_FAILURE);
		}

		/*
		 * validate the register number.
		 */
		length /= (sizeof (pci_regspec_t) / sizeof (int));
		if (rnumber >= length) {
			ddi_prop_free(pci_rp);
			return (DDI_FAILURE);
		}

		/*
		 * copy the required entry.
		 */
		pci_reg = pci_rp[rnumber];

		/*
		 * free the memory allocated by ddi_prop_lookup_int_array
		 */
		ddi_prop_free(pci_rp);

		pci_rp = &pci_reg;
		if (pci_get_reg_prop(rdip, pci_rp) != DDI_SUCCESS)
			return (DDI_FAILURE);
		mp->map_type = DDI_MT_REGSPEC;
		break;
	default:
		return (DDI_ME_INVAL);
	}

	space = pci_rp->pci_phys_hi & PCI_REG_ADDR_M;

	/*
	 * check for unmap and unlock of address space
	 */
	if ((mp->map_op == DDI_MO_UNMAP) || (mp->map_op == DDI_MO_UNLOCK)) {
		/*
		 * Adjust offset and length
		 * A non-zero length means override the one in the regspec.
		 */
		pci_rp->pci_phys_low += (uint_t)offset;
		if (len != 0)
			pci_rp->pci_size_low = len;

		switch (space) {
		case PCI_ADDR_CONFIG:
			/* No work required on unmap of Config space */
			return (DDI_SUCCESS);

		case PCI_ADDR_IO:
			reg.regspec_bustype = 1;
			break;

		case PCI_ADDR_MEM64:
			/*
			 * MEM64 requires special treatment on map, to check
			 * that the device is below 4G.  On unmap, however,
			 * we can assume that everything is OK... the map
			 * must have succeeded.
			 */
			/* FALLTHROUGH */
		case PCI_ADDR_MEM32:
			reg.regspec_bustype = 0;
			break;

		default:
			return (DDI_FAILURE);
		}
		reg.regspec_addr = pci_rp->pci_phys_low;
		reg.regspec_size = pci_rp->pci_size_low;

		mp->map_obj.rp = &reg;
		return (ddi_map(dip, mp, (off_t)0, (off_t)0, vaddrp));

	}

	/* check for user mapping request - not legal for Config */
	if (mp->map_op == DDI_MO_MAP_HANDLE && space == PCI_ADDR_CONFIG) {
		return (DDI_FAILURE);
	}

	/*
	 * check for config space
	 * On x86, CONFIG is not mapped via MMU and there is
	 * no endian-ness issues. Set the attr field in the handle to
	 * indicate that the common routines to call the nexus driver.
	 */
	if (space == PCI_ADDR_CONFIG) {
		hp = (ddi_acc_hdl_t *)mp->map_handlep;

		if (hp == NULL) {
			/* Can't map config space without a handle */
			return (DDI_FAILURE);
		}

		ap = (ddi_acc_impl_t *)hp->ah_platform_private;

		/* endian-ness check */
		if (hp->ah_acc.devacc_attr_endian_flags == DDI_STRUCTURE_BE_ACC)
			return (DDI_FAILURE);

		/*
		 * range check
		 */
		if ((offset >= 256) || (len > 256) || (offset + len > 256))
			return (DDI_FAILURE);
		*vaddrp = (caddr_t)offset;

		ap->ahi_acc_attr |= DDI_ACCATTR_CONFIG_SPACE;
		ap->ahi_put8 = pci_config_wr8;
		ap->ahi_get8 = pci_config_rd8;
		ap->ahi_put64 = pci_config_wr64;
		ap->ahi_get64 = pci_config_rd64;
		ap->ahi_rep_put8 = pci_config_rep_wr8;
		ap->ahi_rep_get8 = pci_config_rep_rd8;
		ap->ahi_rep_put64 = pci_config_rep_wr64;
		ap->ahi_rep_get64 = pci_config_rep_rd64;
		ap->ahi_get16 = pci_config_rd16;
		ap->ahi_get32 = pci_config_rd32;
		ap->ahi_put16 = pci_config_wr16;
		ap->ahi_put32 = pci_config_wr32;
		ap->ahi_rep_get16 = pci_config_rep_rd16;
		ap->ahi_rep_get32 = pci_config_rep_rd32;
		ap->ahi_rep_put16 = pci_config_rep_wr16;
		ap->ahi_rep_put32 = pci_config_rep_wr32;


		/* record the device address for future reference */
		cfp = (pci_acc_cfblk_t *)&hp->ah_bus_private;
		cfp->c_busnum = PCI_REG_BUS_G(pci_rp->pci_phys_hi);
		cfp->c_devnum = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
		cfp->c_funcnum = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

		return (DDI_SUCCESS);
	}

	/*
	 * range check
	 */
	if ((offset >= pci_rp->pci_size_low) ||
	    (len > pci_rp->pci_size_low) ||
	    (offset + len > pci_rp->pci_size_low)) {
		return (DDI_FAILURE);
	}

	/*
	 * Adjust offset and length
	 * A non-zero length means override the one in the regspec.
	 */
	pci_rp->pci_phys_low += (uint_t)offset;
	if (len != 0)
		pci_rp->pci_size_low = len;

	/*
	 * convert the pci regsec into the generic regspec used by the
	 * parent root nexus driver.
	 */
	switch (space) {
	case PCI_ADDR_IO:
		reg.regspec_bustype = 1;
		break;
	case PCI_ADDR_MEM64:
		/*
		 * We can't handle 64-bit devices that are mapped above
		 * 4G or that are larger than 4G.
		 */
		if (pci_rp->pci_phys_mid != 0 ||
		    pci_rp->pci_size_hi != 0)
			return (DDI_FAILURE);
		/*
		 * Other than that, we can treat them as 32-bit mappings
		 */
		/* FALLTHROUGH */
	case PCI_ADDR_MEM32:
		reg.regspec_bustype = 0;
		break;
	default:
		return (DDI_FAILURE);
	}
	reg.regspec_addr = pci_rp->pci_phys_low;
	reg.regspec_size = pci_rp->pci_size_low;

	mp->map_obj.rp = &reg;
	return (ddi_map(dip, mp, (off_t)0, (off_t)0, vaddrp));
}


/*
 * pci_get_intrspec: construct the interrupt specification from properties
 *		and the interrupt line and class-code configuration registers.
 */
/*ARGSUSED*/
ddi_intrspec_t
pci_get_intrspec(dev_info_t *dip, dev_info_t *rdip, uint_t inumber)
{
	struct ddi_parent_private_data *pdptr;
	struct intrspec *ispec;
	uint_t	bus;
	uint_t	devnum;
	uint_t	funcnum;
	int 	class;
	int	rc;
	int	*intpriorities;
	u_int	num_intpriorities;

	pdptr = (struct ddi_parent_private_data *)ddi_get_parent_data(rdip);
	if (!pdptr)
		return (NULL);

	ispec = pdptr->par_intr;
	ASSERT(ispec);

	/* check if the intrspec has been initialized */
	if (ispec->intrspec_pri != 0)
		return (ispec);

	/*
	 * If there's an interrupt-priorities property, use it to get
	 * the interrupt priority.  This is a little more complex than
	 * it really needs to be, because each PCI device can have only
	 * one interrupt.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
	    "interrupt-priorities", &intpriorities, &num_intpriorities) ==
		DDI_PROP_SUCCESS) {

		if (inumber < num_intpriorities)
			ispec->intrspec_pri = intpriorities[inumber];

		ddi_prop_free(intpriorities);

	}

	/*
	 * If we still haven't gotten a priority, guess one based on the
	 * class code.
	 */
	if (ispec->intrspec_pri == 0) {

		/* get the 'class' property to derive the intr priority */
		class = ddi_prop_get_int(DDI_DEV_T_ANY, rdip,
			DDI_PROP_DONTPASS, "class-code", -1);
		if (class == -1)
			ispec->intrspec_pri = 1;
		else
			ispec->intrspec_pri = pci_devclass_to_ipl(class);

	}

	/*
	 * Get interrupt line value.  This is uglier and more complex
	 * than it should be, but it is the *ONLY* place in this driver
	 * that actually touches config space on its own (!!!), and so
	 * it doesn't make sense to try to put together a more generic
	 * internal config space access mechanism.  We can't use
	 * pci_config_rdb because we don't have a handle.
	 */
	rc = pci_get_devid(rdip, &bus, &devnum, &funcnum);
	if (rc != DDI_SUCCESS)
		return (NULL);

	mutex_enter(&pci_mutex);

	ispec->intrspec_vec =
		(*pci_getb_func)(bus, devnum, funcnum, PCI_CONF_ILINE);

	mutex_exit(&pci_mutex);

	return (ispec);
}

/*
 * translate from device class to ipl
 */
static int
pci_devclass_to_ipl(int class)
{
	int	base_cl;
	int	ipl;

	base_cl = (class & 0xff0000) >> 16;
	/* sub_cl = (class & 0xff00) >> 8; */
	/*
	 * Use the class code values to construct an ipl for the device.
	 */
	switch (base_cl) {
	default:
	case PCI_CLASS_NONE:
		ipl = 1;
		break;

	case PCI_CLASS_MASS:
		ipl = 0x5;
		break;

	case PCI_CLASS_NET:
		ipl = 0x6;
		break;

	case PCI_CLASS_DISPLAY:
		ipl = 0x9;
		break;

	/*
	 * for high priority interrupt handlers, use level 12
	 * as the highest for device drivers
	 */
	case PCI_CLASS_MM:
		ipl = 0xc;
		break;

	case PCI_CLASS_MEM:
		ipl = 0xc;
		break;

	case PCI_CLASS_BRIDGE:
		ipl = 0xc;
		break;
	}
	return (ipl);
}

/*ARGSUSED*/
static int
pci_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	pci_regspec_t *drv_regp;
	u_int	reglen;
	int	rn;
	int	totreg;

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?PCI-device: %s@%s, %s%d\n",
		    ddi_node_name(rdip), ddi_get_name_addr(rdip),
		    ddi_driver_name(rdip),
		    ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (pci_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		return (pci_removechild((dev_info_t *)arg));

	case DDI_CTLOPS_NINTRS:
		if (ddi_get_parent_data(rdip))
			*(int *)result = 1;
		else
			*(int *)result = 0;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_XLATE_INTRS:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_SIDDEV:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);

		*(int *)result = 0;
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
				DDI_PROP_DONTPASS, "reg", (int **)&drv_regp,
				&reglen) != DDI_PROP_SUCCESS) {
			return (DDI_FAILURE);
		}

		totreg = (reglen * sizeof (int)) / sizeof (pci_regspec_t);
		if (ctlop == DDI_CTLOPS_NREGS)
			*(int *)result = totreg;
		else if (ctlop == DDI_CTLOPS_REGSIZE) {
			rn = *(int *)arg;
			if (rn >= totreg) {
				ddi_prop_free(drv_regp);
				return (DDI_FAILURE);
			}
			*(off_t *)result = drv_regp[rn].pci_size_low;
		}
		ddi_prop_free(drv_regp);

		return (DDI_SUCCESS);

	case DDI_CTLOPS_POWER: {
		power_req_t	*reqp = (power_req_t *)arg;
		/*
		 * We currently understand reporting of PCI_PM_IDLESPEED
		 * capability. Everything else is passed up.
		 */
		if ((reqp->request_type == PMR_REPORT_PMCAP) &&
		    (reqp->req.report_pmcap_req.cap ==  PCI_PM_IDLESPEED)) {
			int	idlespd = (int)reqp->req.report_pmcap_req.arg;
			/*
			 * Nothing to do here at this time, so just return
			 * success. We don't accept obviously bad values
			 * so that a leaf driver gets a consistent view
			 * on this platform and the platform doing real
			 * processing.
			 */
			if ((idlespd >= 0 && idlespd <= PCI_CLK_33MHZ / 1024) ||
			    (idlespd == (int)PCI_PM_IDLESPEED_ANY) ||
			    (idlespd == (int)PCI_PM_IDLESPEED_NONE)) {
				return (DDI_SUCCESS);
			} else {
				return (DDI_FAILURE);
			}
		}
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}

	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}

	/* NOTREACHED */

}

static int
pci_initchild(dev_info_t *child)
{
	struct ddi_parent_private_data *pdptr;
	char name[80];
	pci_regspec_t *pci_rp;
	char **unit_addr;
	int dev;
	int func;
	int length;
	uint_t n;

	/*
	 * Pseudo nodes indicate a prototype node with per-instance
	 * properties to be merged into the real h/w device node.
	 * The interpretation of the unit-address is DD[,F]
	 * where DD is the device id and F is the function.
	 */
	if (ndi_dev_is_persistent_node(child) == 0) {
		if (ddi_getlongprop(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "reg", (caddr_t)&pci_rp,
		    (int *)&n) == DDI_SUCCESS) {
			cmn_err(CE_WARN,
			    "cannot merge prototype from %s.conf",
			    ddi_get_name(child));
			kmem_free((caddr_t)pci_rp, n);
			return (DDI_NOT_WELL_FORMED);
		}
		if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "unit-address", &unit_addr, &n) !=
		    DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
			    "cannot merge prototype from %s.conf",
			    ddi_get_name(child));
			return (DDI_NOT_WELL_FORMED);
		}
		if (n != 1 || *unit_addr == NULL || **unit_addr == 0) {
			cmn_err(CE_WARN, "unit-address property in %s.conf"
			    " not well-formed", ddi_get_name(child));
			ddi_prop_free(unit_addr);
			return (DDI_NOT_WELL_FORMED);
		}
		ddi_set_name_addr(child, *unit_addr);
		ddi_set_parent_data(child, NULL);
		ddi_prop_free(unit_addr);

		/*
		 * Try to merge the properties from this prototype
		 * node into real h/w nodes.
		 */
		if (impl_ddi_merge_child(child) != DDI_SUCCESS) {
			/*
			 * Merged ok - return failure to remove the node.
			 */
			return (DDI_FAILURE);
		}
		/*
		 * The child was not merged into a h/w node,
		 * but there's not much we can do with it other
		 * than return failure to cause the node to be removed.
		 */
		cmn_err(CE_WARN, "!%s@%s: %s.conf properties not merged",
		    ddi_get_name(child), ddi_get_name_addr(child),
		    ddi_get_name(child));
		return (DDI_NOT_WELL_FORMED);
	}

	/*
	 * Set the address portion of the node name based on
	 * the function and device number from the 'reg' property.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, child,
			DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
			(u_int *)&length) != DDI_PROP_SUCCESS) {
		return (DDI_NOT_WELL_FORMED);
	}

	/* copy the device identifications */
	dev = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	func = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array
	 */
	ddi_prop_free(pci_rp);

	if (func != 0) {
		(void) sprintf(name, "%x,%x", dev, func);
	} else {
		(void) sprintf(name, "%x", dev);
	}

	ddi_set_name_addr(child, name);

	if (ddi_prop_get_int(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
			"interrupts", -1) != -1) {
		pdptr = (struct ddi_parent_private_data *)
			kmem_zalloc((sizeof (struct ddi_parent_private_data) +
			sizeof (struct intrspec)), KM_SLEEP);
		pdptr->par_intr = (struct intrspec *)(pdptr + 1);
		pdptr->par_nintr = 1;
		ddi_set_parent_data(child, (caddr_t)pdptr);
	} else
		ddi_set_parent_data(child, NULL);

	return (DDI_SUCCESS);
}

static int
pci_removechild(dev_info_t *dip)
{
	register struct ddi_parent_private_data *pdptr;

	pdptr = (struct ddi_parent_private_data *)ddi_get_parent_data(dip);
	if (pdptr != NULL) {
		kmem_free(pdptr, (sizeof (*pdptr) + sizeof (struct intrspec)));
		ddi_set_parent_data(dip, NULL);
	}
	ddi_set_name_addr(dip, NULL);

	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);

	impl_rem_dev_props(dip);

	return (DDI_SUCCESS);
}



static int
pci_get_devid(dev_info_t *child, uint_t *bus, uint_t *devnum, uint_t *funcnum)
{
	pci_regspec_t *pci_rp;
	int	length;
	int	rc;

	/* get child "reg" property */
	rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, child,
		DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
		(u_int *)&length);
	if ((rc != DDI_SUCCESS) || (length <
			(sizeof (pci_regspec_t) / sizeof (int)))) {
		return (DDI_FAILURE);
	}

	/* copy the device identifications */
	*bus = PCI_REG_BUS_G(pci_rp->pci_phys_hi);
	*devnum = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	*funcnum = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array().
	 */
	ddi_prop_free(pci_rp);
	return (DDI_SUCCESS);
}

/*
 * These are the get and put functions to be shared with drivers. They
 * acquire the mutex  enable the configuration space and then call the
 * internal function to do the appropriate access mechanism.
 */

static uint8_t
pci_config_rd8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint8_t	rval;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	mutex_enter(&pci_mutex);

	rval = (*pci_getb_func)(cfp->c_busnum, cfp->c_devnum, cfp->c_funcnum,
		(int)addr);

	mutex_exit(&pci_mutex);

	return (rval);
}

static void
pci_config_rep_rd8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = pci_config_rd8(hdlp, d++);
	else
		for (; repcount; repcount--)
			*h++ = pci_config_rd8(hdlp, d);
}

static uint16_t
pci_config_rd16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint16_t rval;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	mutex_enter(&pci_mutex);

	rval = (*pci_getw_func)(cfp->c_busnum, cfp->c_devnum, cfp->c_funcnum,
			    (int)addr);

	mutex_exit(&pci_mutex);
	return (rval);
}

static void
pci_config_rep_rd16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = pci_config_rd16(hdlp, d++);
	else
		for (; repcount; repcount--)
			*h++ = pci_config_rd16(hdlp, d);
}

static uint32_t
pci_config_rd32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint32_t rval;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	mutex_enter(&pci_mutex);

	rval = (*pci_getl_func)(cfp->c_busnum, cfp->c_devnum,
			    cfp->c_funcnum, (int)addr);

	mutex_exit(&pci_mutex);
	return (rval);
}

static void
pci_config_rep_rd32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = pci_config_rd32(hdlp, d++);
	else
		for (; repcount; repcount--)
			*h++ = pci_config_rd32(hdlp, d);
}


static void
pci_config_wr8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	pci_acc_cfblk_t *cfp;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	mutex_enter(&pci_mutex);

	(*pci_putb_func)(cfp->c_busnum, cfp->c_devnum,
		    cfp->c_funcnum, (int)addr, value);

	mutex_exit(&pci_mutex);
}

static void
pci_config_rep_wr8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			pci_config_wr8(hdlp, d++, *h++);
	else
		for (; repcount; repcount--)
			pci_config_wr8(hdlp, d, *h++);
}

static void
pci_config_wr16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	pci_acc_cfblk_t *cfp;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	mutex_enter(&pci_mutex);

	(*pci_putw_func)(cfp->c_busnum, cfp->c_devnum,
		    cfp->c_funcnum, (int)addr, value);

	mutex_exit(&pci_mutex);
}

static void
pci_config_rep_wr16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			pci_config_wr16(hdlp, d++, *h++);
	else
		for (; repcount; repcount--)
			pci_config_wr16(hdlp, d, *h++);
}

static void
pci_config_wr32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	pci_acc_cfblk_t *cfp;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	mutex_enter(&pci_mutex);

	(*pci_putl_func)(cfp->c_busnum, cfp->c_devnum,
		    cfp->c_funcnum, (int)addr, value);

	mutex_exit(&pci_mutex);
}

static void
pci_config_rep_wr32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			pci_config_wr32(hdlp, d++, *h++);
	else
		for (; repcount; repcount--)
			pci_config_wr32(hdlp, d, *h++);
}

static uint64_t
pci_config_rd64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	uint32_t lw_val;
	uint32_t hi_val;
	uint32_t *dp;
	uint64_t val;

	dp = (uint32_t *)addr;
	lw_val = pci_config_rd32(hdlp, dp);
	dp++;
	hi_val = pci_config_rd32(hdlp, dp);
	val = ((uint64_t)hi_val << 32) | lw_val;
	return (val);
}

static void
pci_config_wr64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	uint32_t lw_val;
	uint32_t hi_val;
	uint32_t *dp;

	dp = (uint32_t *)addr;
	lw_val = (uint32_t)(value & 0xffffffff);
	hi_val = (uint32_t)(value >> 32);
	pci_config_wr32(hdlp, dp, lw_val);
	dp++;
	pci_config_wr32(hdlp, dp, hi_val);
}

static void
pci_config_rep_rd64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--)
			*host_addr++ = pci_config_rd64(hdlp, dev_addr++);
	} else {
		for (; repcount; repcount--)
			*host_addr++ = pci_config_rd64(hdlp, dev_addr);
	}
}

static void
pci_config_rep_wr64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--)
			pci_config_wr64(hdlp, host_addr++, *dev_addr++);
	} else {
		for (; repcount; repcount--)
			pci_config_wr64(hdlp, host_addr++, *dev_addr);
	}
}
