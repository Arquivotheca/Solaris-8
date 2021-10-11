/*
 * pcn -- PC-Net Generic
 * Depends on the Generic LAN Driver utility functions in /kernel/misc/gld
 */

/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify this file without charge, but are not authorized to
 * license or distribute it to anyone else except as part of a product
 * or program developed by the user.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * This file is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS FILE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even
 * if Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#pragma ident	"@(#)pcn.c	1.38	99/08/18 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/ksynch.h>
#include <sys/stat.h>
#include <sys/modctl.h>

#ifdef _DDICT
#include "sys/dlpi.h"
#include "sys/ethernet.h"
#include "sys/gld.h"
#else
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/gld.h>
#endif

#include <sys/ddi.h>
#include <sys/pci.h>
#include <sys/debug.h>

#if defined(PCI_DDI_EMULATION) || defined(COMMON_IO_EMULATION)
#include <sys/xpci/sunddi_2.5.h>
#else
#include <sys/sunddi.h>
#endif
#include <sys/mii.h>
#include "pcn.h"

/*
 *  Declarations and Module Linkage
 */

static char ident[] = "PC-Net (Generic)";

#ifdef PCNDEBUG
/* used for debugging */
int	pcndebug = 0;
#endif

/* When this drive is ddi compliant, this can go away */
#if defined(prep)
#define	DMA_ADDR(a)	(0x80000000 | (a))
#elif defined(i386)
#define	DMA_ADDR(a)	(a)
#else
#error Unsupported architecture!
#endif

/* Required system entry points */
static	pcnidentify(dev_info_t *);
static	pcndevinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static	pcnprobe(dev_info_t *);
static	pcnattach(dev_info_t *, ddi_attach_cmd_t);
static	pcndetach(dev_info_t *, ddi_detach_cmd_t);
static	pcnreset(dev_info_t *, ddi_reset_cmd_t);

/* Required driver entry points for GLD */
int	pcn_reset(gld_mac_info_t *);
int	pcn_start_board(gld_mac_info_t *);
int	pcn_stop_board(gld_mac_info_t *);
int	pcn_saddr(gld_mac_info_t *);
int	pcn_dlsdmult(gld_mac_info_t *, struct ether_addr *, int);
int	pcn_prom(gld_mac_info_t *, int);
int	pcn_gstat(gld_mac_info_t *);
int	pcn_send(gld_mac_info_t *, mblk_t *);
uint_t	pcnintr(caddr_t);

/*
 * Forced dependencies: on GLD, XPCI and MII.
 */
#if defined(PCI_DDI_EMULATION) || defined(COMMON_IO_EMULATION)
#define	XPCI_DEPEND " misc/xpci"
#else
#define	XPCI_DEPEND
#endif

#ifdef MII_IS_MODULE
#define	MII_DEPEND " misc/mii"
#else
#define	MII_DEPEND
#endif

char _depends_on[] = "misc/gld" XPCI_DEPEND MII_DEPEND;

/*
 * Internal functions
 */

static int pcn_ProbePCI(dev_info_t *devinfo);
static int pcn_ProbeISA(dev_info_t *devinfo);
static int  pcn_ScanISA_IRQ(gld_mac_info_t *macinfo, int *irq_index);
static int pcn_ScanISA_DMA(gld_mac_info_t *macinfo, int *dma);
static int pcn_MapIO(dev_info_t *devinfo,
    ddi_acc_handle_t *phandle, int *preg);
static int pcn_GetIRQ(dev_info_t *devinfo, gld_mac_info_t *macinfo);
static int pcn_GetIRQ_PCI(dev_info_t *devinfo, gld_mac_info_t *macinfo);
static int pcn_GetIRQ_ISA(dev_info_t *devinfo, gld_mac_info_t *macinfo);
static int pcn_CheckSignature(ddi_acc_handle_t handle, int reg);
static void pcn_OutCSR(struct pcninstance *pcnp, int reg, ushort_t value);
static void pcn_OutBCR(struct pcninstance *pcnp, int reg, ushort_t value);
static ushort_t pcn_InCSR(struct pcninstance *pcnp, int reg);
static ushort_t pcn_InBCR(struct pcninstance *pcnp, int reg);
static void pcn_StopLANCE(struct pcninstance *pcnp);
static int pcn_InitData(struct pcninstance *pcnp, uchar_t *macaddr);
static int pcn_GetMem(struct pcninstance *pcnp, int size, void **vaddr,
    ulong_t *paddr);
static void pcn_ShredMem(struct pcninstance *pcnp);
static struct PCN_IOmem *pcn_GetPage(struct pcninstance *pcnp);
static void pcn_MakeRecvDesc(union PCN_RecvMsgDesc *ptr, ulong_t addr,
    ushort_t size, int owned);
static void pcn_MakeXmitDesc(union PCN_XmitMsgDesc *ptr, ulong_t addr,
    ushort_t size, int owned);
static int pcn_ProcessReceive(gld_mac_info_t *macinfo,
    struct pcninstance *pcnp);
static void pcn_ProcessTransmit(gld_mac_info_t *macinfo,
    struct pcninstance *pcnp);
static void pcn_ResetRings(struct pcninstance *pcnp);
static int pcn_LADR_index(uchar_t *addr);

static int pcn_GetBusType(dev_info_t *devinfo);
static void pcn_EnablePCI(struct pcninstance *pcnp, int on_or_off);
static int pcn_init_board(gld_mac_info_t *);
static void pcn_hard_init(gld_mac_info_t *);

/* PHY functions */
static ushort_t pcn_miiread(dev_info_t *, int, int);
static void pcn_miiwrite(dev_info_t *, int, int, int);
static void pcn_miilinkstate(dev_info_t *dip, int phy,
			    enum mii_phy_state state);
static void pcn_process_phy(gld_mac_info_t  *);
static void pcn_select_port(struct pcninstance *, unsigned long port);

static void pcn_suspend(struct pcninstance *pcnp);
static void pcn_resume(struct pcninstance *pcnp);
#define	PROP_ARGS	(DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP)

/*
 * Appropriate ddi_regs_map_setup arguments for each bus type
 */
#define	PCN_ISA_RNUMBER	0
#define	PCN_PCI_RNUMBER	1

/*
 * Standard Streams initialization
 */

static struct module_info minfo = {
	PCNIDNUM, "pcn", 0, INFPSZ, PCNHIWAT, PCNLOWAT
};

static struct qinit rinit = {	/* read queues */
	NULL, gld_rsrv, gld_open, gld_close, NULL, &minfo, NULL
};

static struct qinit winit = {	/* write queues */
	gld_wput, gld_wsrv, NULL, NULL, NULL, &minfo, NULL
};

struct streamtab pcninfo = {&rinit, &winit, NULL, NULL};

/* Standard Module linkage initialization for a Streams driver */

extern struct mod_ops mod_driverops;

static 	struct cb_ops cb_pcnops = {
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
	&pcninfo,		/* cb_stream */
	(int)(D_MP)		/* cb_flag */
};

struct dev_ops pcnops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	pcndevinfo,		/* devo_getinfo */
	pcnidentify,		/* devo_identify */
	pcnprobe,		/* devo_probe */
	pcnattach,		/* devo_attach */
	pcndetach,		/* devo_detach */
	pcnreset,		/* devo_reset */
	&cb_pcnops,		/* devo_cb_ops */
	(struct bus_ops *)NULL	/* devo_bus_ops */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	ident,			/* short description */
	&pcnops			/* driver specific ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
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

/*
 *  DDI Entry Points
 */

/* identify(9E) -- See if we know about this device */

pcnidentify(dev_info_t *devinfo)
{
	if (strcmp(ddi_get_name(devinfo), "pcn") == 0)
		return (DDI_IDENTIFIED);
	else if (strcmp(ddi_get_name(devinfo), "pci1022,2000") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/* getinfo(9E) -- Get device driver information */
/*ARGSUSED*/
pcndevinfo(dev_info_t *devinfo, ddi_info_cmd_t cmd, void *arg, void **result)
{
	register int error;

	/* This code is not DDI compliant: the correct semantics */
	/* for CLONE devices is not well-defined yet. */
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (devinfo == NULL) {
			error = DDI_FAILURE;	/* Unfortunate */
		} else {
			*result = (void *)devinfo;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;	/* This CLONEDEV always returns zero */
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * probe(9E) -- Determine if a device is present
 */

int
pcnprobe(dev_info_t *devinfo)
{
#ifdef PCNDEBUG
	if (pcndebug & PCNDDI)
		cmn_err(CE_CONT, "pcnprobe(0x%p)", (void *) devinfo);
#endif

	switch (pcn_GetBusType(devinfo)) {
	case PCN_BUS_ISA:
	case PCN_BUS_EISA:
		return (pcn_ProbeISA(devinfo));
	case PCN_BUS_PCI:
		return (pcn_ProbePCI(devinfo));
	default:
		return (DDI_PROBE_FAILURE);
	}
}


static int
pcn_ProbePCI(dev_info_t *devinfo)
{
	ushort_t	vendorid;
	ushort_t	deviceid;
	ushort_t	cmdreg;
	ushort_t	iline;
	int	adapter_ok;
	ddi_acc_handle_t	handle;

	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (DDI_PROBE_FAILURE);

	/*
	 * Sanity check the device configuration
	 */
	vendorid = pci_config_getw(handle, PCI_CONF_VENID);
	deviceid = pci_config_getw(handle, PCI_CONF_DEVID);
	adapter_ok = DDI_PROBE_FAILURE;

	if ((vendorid == PCI_AMD_VENDOR_ID) && (deviceid == PCI_PCNET_ID)) {
		cmdreg = pci_config_getw(handle, PCI_CONF_COMM);
		iline = pci_config_getb(handle, PCI_CONF_ILINE);
		adapter_ok = DDI_PROBE_SUCCESS;
		if (((iline > 15) || (iline == 0)) ||
		    (!(cmdreg & PCI_COMM_IO)))
			adapter_ok = DDI_PROBE_FAILURE;
	}

	pci_config_teardown(&handle);

	return (adapter_ok);
}


static int
pcn_ProbeISA(dev_info_t *devinfo)
{
	int	retval;
	ddi_acc_handle_t	handle;
	int			reg;
	int	IsPnP = 0;

	if (pcn_MapIO(devinfo, &handle, &reg) != DDI_SUCCESS)
		return (DDI_PROBE_FAILURE);

	/*
	 * Check if it is a PnP card (2.6), if it is don't do the
	 * check for the WW signature or checksum.
	 */
	if (!(ddi_getprop(DDI_DEV_T_ANY, devinfo, 0,
	    "ignore-hardware-nodes", 0))) {
		int nregs, reglen, i;
		struct {
			int bustype;
			int base;
			int size;
		} *reglist;

		if (ddi_getlongprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
		    "reg", (caddr_t)&reglist, &reglen) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
			    "pcn: reg property not found in dev property list");
			ddi_regs_map_free(&handle);
			return (DDI_FAILURE);
		}
		nregs = reglen / sizeof (*reglist);

		for (i = 0; i < nregs; i++) {
			if (reglist[i].bustype & 0x80000000)
				break;
		}
		kmem_free(reglist, reglen);
		IsPnP = (i >= nregs) ? 0 : 1;
	}

	if (IsPnP) {
		/*
		 * Pulse reset
		 */
#if defined(_WIO)
		(void) ddi_io_getw(handle, REG16(reg, PCN_IO_RESET));
		drv_usecwait(1);
		ddi_io_putw(handle, REG16(reg, PCN_IO_RESET), 0);
#elif defined(_DWIO)
		ddi_io_getl(handle, REG32(reg, PCN_IO_RESET));
		drv_usecwait(1);
		ddi_io_putl(handle, REG32(reg, PCN_IO_RESET), 0);
#else
#error Neither _WIO nor _DWIO defined!
#endif

		/* Add a 10msec delay for some adapters to settle down */
		drv_usecwait(10000);

	} else {
		/*
		 * Look for the WW first (accept no imitations).
		 */
		retval = pcn_CheckSignature(handle, reg);
	}
	ddi_regs_map_free(&handle);
	return (((IsPnP) ? DDI_PROBE_SUCCESS : retval));
}

static int
pcn_GetBusType(dev_info_t *devinfo)
{
	char	parent_type[16];
	int	parentlen;

	parentlen = sizeof (parent_type);

	if (ddi_prop_op(DDI_DEV_T_ANY, devinfo, PROP_LEN_AND_VAL_BUF, 0,
	    "device_type", (caddr_t)parent_type, &parentlen)
	    != DDI_PROP_SUCCESS && ddi_prop_op(DDI_DEV_T_ANY, devinfo,
	    PROP_LEN_AND_VAL_BUF, 0, "bus-type", (caddr_t)parent_type,
	    &parentlen) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
			    "pcn : can't figure out device type for"
			    " parent \"%s\"",
			    ddi_get_name(ddi_get_parent(devinfo)));
			return (PCN_BUS_UNKNOWN);
	}
	if (strcmp(parent_type, "eisa") == 0)
		return (PCN_BUS_EISA);
	else if (strcmp(parent_type, "isa") == 0)
		return (PCN_BUS_ISA);
	else if (strcmp(parent_type, "pci") == 0)
		return (PCN_BUS_PCI);
	else
		return (PCN_BUS_UNKNOWN);
}


static int
pcn_MapIO(dev_info_t *devinfo, ddi_acc_handle_t *phandle, int *preg)
{
	int rnumber, nregs, reglen, i;
	struct {
		int bustype;
		int base;
		int size;
	} *reglist;
	static ddi_device_acc_attr_t accattr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,	/* not portable */
		DDI_STRICTORDER_ACC,
	};

	switch (pcn_GetBusType(devinfo)) {
	case PCN_BUS_ISA:
	case PCN_BUS_EISA:
		if (ddi_getlongprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
		    "reg", (caddr_t)&reglist, &reglen) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
			    "pcn: reg property not found in dev property list");
			return (DDI_FAILURE);
		}
		if (!(ddi_getprop(DDI_DEV_T_ANY, devinfo, 0,
		    "ignore-hardware-nodes", 0))) {
			nregs = reglen / sizeof (*reglist);
			for (i = 0; i < nregs; i++) {
				if (reglist[i].bustype == 1)
					break;
			}
			rnumber = i;
		} else
			rnumber = PCN_ISA_RNUMBER;
		kmem_free(reglist, reglen);
		break;

	case PCN_BUS_PCI:
		rnumber = PCN_PCI_RNUMBER;
		break;

	default:
		return (DDI_FAILURE);
	}

	if (ddi_regs_map_setup(devinfo, rnumber, (caddr_t *)preg,
		(offset_t)0, (offset_t)0, &accattr, phandle) != DDI_SUCCESS) {
#ifdef	PCNDEBUG
		cmn_err(CE_NOTE, "pcn_MapIO: failed to map registers");
#endif	/* PCNDEBUG */
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

static int
pcn_GetIRQ(dev_info_t *devinfo, gld_mac_info_t *macinfo)
{
	switch (pcn_GetBusType(devinfo)) {
	case PCN_BUS_ISA:
	case PCN_BUS_EISA:
		return (pcn_GetIRQ_ISA(devinfo, macinfo));

	case PCN_BUS_PCI:
		return (pcn_GetIRQ_PCI(devinfo, macinfo));

	default:
		break;
	}

	return (-1);
}

/*ARGSUSED*/
static int
pcn_GetIRQ_PCI(dev_info_t *devinfo, gld_mac_info_t *macinfo)
{
#if defined(PCI_DDI_EMULATION) || defined(COMMON_IO_EMULATION)

	ddi_acc_handle_t	handle;
	int	iline;
	int	intarr[3];
	int 	i;
	int 	len;
	struct intrprop {
		int spl;
		int irq;
	} *intrprop;

	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (-1);

	iline = pci_config_getb(handle, PCI_CONF_ILINE);
	pci_config_teardown(&handle);

	if ((i = ddi_getlongprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
			"interrupts", (caddr_t)&intrprop,
			&len)) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN,
		    "pcn: Could not locate interrupts property.\n");
		return (DDI_PROBE_FAILURE);
	}

	len /= sizeof (struct intrprop);
	for (i = 0; i < len; i++)
		if (iline == intrprop[i].irq)
			break;
	kmem_free(intrprop, len * sizeof (struct intrprop));

	if (i >= len) {
		cmn_err(CE_WARN,
			"pcn: irq in conf file does not match PCI config.\n");
		return (-1);
	}

	return (i);
#else
	return (0);	/* always index 0 for PCI in 2.5 */
#endif
}

/*ARGSUSED*/
static int
pcn_GetIRQ_ISA(dev_info_t *devinfo, gld_mac_info_t *macinfo)
{
	int	index = -1;

	if (pcn_ScanISA_IRQ(macinfo, &index) != 0)
		return (-1);
	else
		return (index);
}

/*
 *  attach(9E) -- Attach a device to the system
 *
 *  Called once for each board successfully probed.
 */

pcnattach(dev_info_t *devinfo, ddi_attach_cmd_t cmd)
{
	gld_mac_info_t *macinfo;		/* GLD structure */
	struct pcninstance *pcnp;		/* Our private device info */
	int	i;
	ddi_acc_handle_t	io_handle;
	int			io_reg;

#ifdef PCNDEBUG
	if (pcndebug & PCNDDI)
		cmn_err(CE_CONT, "pcnattach(0x%p)", (void *) devinfo);
#endif

	/* NEEDSWORK: APM */
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	/*
	 * If we cannot map the registers, fail to attach
	 */
	if (pcn_MapIO(devinfo, &io_handle, &io_reg) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/*
	 *  Allocate gld_mac_info_t and pcninstance structures
	 */
	macinfo = (gld_mac_info_t *)kmem_zalloc(
			sizeof (gld_mac_info_t)+sizeof (struct pcninstance),
			KM_NOSLEEP);
	if (macinfo == NULL) {
		ddi_regs_map_free(&io_handle);
		return (DDI_FAILURE);
	}
	pcnp = (struct pcninstance *)(macinfo+1);

	/*  Initialize our private fields in macinfo and pcninstance */
	macinfo->gldm_private = (caddr_t)pcnp;
	macinfo->gldm_state = PCN_IDLE;
	macinfo->gldm_flags = 0;

	/*
	 * Initialize pcninstance
	 */
	pcnp->devinfo = devinfo;
	pcnp->io_handle = io_handle;
	pcnp->io_reg = io_reg;
	pcnp->dma_attached = -1;	/* not attached */
	pcnp->irq_scan = pcnp->dma_scan = 0;
	/* reset the multi-cast reference list */
	for (i = 0; i < LADRF_LEN; i++)
		pcnp->mcref[i] = 0;

	/*
	 * set up DMA limits.  Currently, the limits enforce the
	 * ISA style 24bit addressing supported by all PCnet family
	 * chips ("16 bit mode").  For PCnet-32 and PCnet-PCI, we
	 * may wish to relax the limits and make sure that the
	 * "32 bit mode" is used.  Since each instance of pcn
	 * has an individual dmalim, each PCnet can be different
	 */
	pcnp->page_size = ddi_ptob(devinfo, 1);

	pcnp->dmalim.dlim_addr_lo = 0x00000000;
	pcnp->dmalim.dlim_addr_hi = 0x00ffffff;	/* NEEDSWORK: ISA limit */
	pcnp->dmalim.dlim_cntr_max = 0;
	pcnp->dmalim.dlim_burstsizes = 1;
	pcnp->dmalim.dlim_minxfer = DMA_UNIT_8;
	pcnp->dmalim.dlim_dmaspeed = 0;
	pcnp->dmalim.dlim_version = (uint_t)DMALIM_VER0;
	pcnp->dmalim.dlim_adreg_max = pcnp->page_size - 1;
	pcnp->dmalim.dlim_ctreg_max = pcnp->page_size - 1;
	pcnp->dmalim.dlim_granular = pcnp->page_size;
	pcnp->dmalim.dlim_sgllen = 1;
	pcnp->dmalim.dlim_reqsize = pcnp->page_size;
	/*
	 *  Initialize pointers to device specific functions which will be
	 *  used by the generic layer.
	 */

	macinfo->gldm_reset   = pcn_reset;
	macinfo->gldm_start   = pcn_start_board;
	macinfo->gldm_stop    = pcn_stop_board;
	macinfo->gldm_saddr   = pcn_saddr;
	macinfo->gldm_sdmulti = pcn_dlsdmult;
	macinfo->gldm_prom    = pcn_prom;
	macinfo->gldm_gstat   = pcn_gstat;
	macinfo->gldm_send    = pcn_send;
	macinfo->gldm_intr    = pcnintr;
	macinfo->gldm_ioctl   = NULL;    /* if you have one, NULL otherwise */

	/*
	 *  Initialize board characteristics needed by the generic layer.
	 */

	macinfo->gldm_ident = ident;
	macinfo->gldm_type = DL_ETHER;
	macinfo->gldm_minpkt = 0;		/* assumes we pad ourselves */
	macinfo->gldm_maxpkt = PCNMAXPKT;
	macinfo->gldm_addrlen = ETHERADDRL;
	macinfo->gldm_saplen = -2;

	for (i = 0; i < ETHERADDRL; i++)
		macinfo->gldm_vendor[i] =
			ddi_io_getb(pcnp->io_handle, REG8(pcnp->io_reg, i));

	/* NEEDSWORK: may be able to tell */
	macinfo->gldm_media = GLDM_UNKNOWN;

	bcopy((caddr_t)gldbroadcastaddr,
		(caddr_t)macinfo->gldm_broadcast, ETHERADDRL);
	bcopy((caddr_t)macinfo->gldm_vendor,
		(caddr_t)macinfo->gldm_macaddr, ETHERADDRL);

	/*
	 * Set reg index to -1 so gld doesn't map_reg anything for us.
	 * NEEDSWORK
	 */
	macinfo->gldm_reg_index = -1;

	/*
	 * If we are driving a PCI device, make sure I/O enable and
	 * Master Enable are set.  The assuption is that probe will
	 * fail for inadequately initialized PCI devices, so this
	 * should always be valid.
	 */
	if (pcn_GetBusType(devinfo) == PCN_BUS_PCI)
		pcn_EnablePCI(pcnp, 1);

	/*
	 * Allocate and initialize the shared memory data
	 */
	/* XXX function return value is ignored */
	(void) pcn_InitData(pcnp, macinfo->gldm_vendor);

	/*
	 * Determine IRQ index
	 */
	macinfo->gldm_irq_index = pcn_GetIRQ(devinfo, macinfo);

	if (macinfo->gldm_irq_index < 0)
		goto attach_fail;

	/*
	 * If this is ISA and doesn't yet know DMA, scan and attach it.
	 */
	switch (pcn_GetBusType(devinfo)) {
	case PCN_BUS_ISA:
	case PCN_BUS_EISA:
		if (pcn_ScanISA_DMA(macinfo, &i) < 0) {
			/* NEEDSWORK: cmn_err warning for DMA failure */
			goto attach_fail;
		}

		if (ddi_dmae_alloc(pcnp->devinfo, i, NULL, NULL) != DDI_SUCCESS)
			goto attach_fail;

		if (ddi_dmae_1stparty(pcnp->devinfo, i) != DDI_SUCCESS) {
			(void) ddi_dmae_release(pcnp->devinfo, i);
			goto attach_fail;
		}

		if (ddi_dmae_enable(pcnp->devinfo, i) != DDI_SUCCESS) {
			(void) ddi_dmae_release(pcnp->devinfo, i);
			goto attach_fail;
		}

		pcnp->dma_attached = i;
		break;
	default:
		/* non-ISA doesn't need DMA */
		break;
	}

	/*
	 * Ensure board is not running before gld_register; otherwise
	 * board may generate early interrupts (bug 1207743)
	 */
	/* XXX function return value is ignored */
	(void) pcn_stop_board(macinfo);

	/*
	 *  Register ourselves with the GLD interface
	 *
	 *  gld_register will:
	 *	link us with the GLD system;
	 *	set our ddi_set_driver_private(9F) data to the macinfo pointer;
	 *	save the devinfo pointer in macinfo->gldm_devinfo;
	 *	map the registers, putting the kvaddr into macinfo->gldm_memp;
	 *	add the interrupt, putting the cookie in gldm_cookie;
	 *	init the gldm_intrlock mutex which will block that interrupt;
	 *	create the minor node.
	 */

	if (gld_register(devinfo, "pcn", macinfo) == DDI_SUCCESS) {
		/*
		 *  Do anything necessary to prepare the board for operation
		 *  short of actually starting the board.  We call
		 *  gld_register() first in case hardware initialization
		 *  requires memory mapping or is interrupt-driven.
		 *  In this case we need to check for
		 *  the return code of initialization.  If initialization fails,
		 *  gld_unregister() should be called, data structures be freed,
		 *  and DDI_FAILURE returned.
		 */

		pcn_hard_init(macinfo);
		if (pcn_init_board(macinfo)) {
			gld_unregister(macinfo);
			goto attach_fail;
		}

		/* Make sure we have our address set */
		/* XXX function return value is ignored */
		(void) pcn_saddr(macinfo);

		return (DDI_SUCCESS);
	}

attach_fail:
	pcn_ShredMem(pcnp);
	ddi_regs_map_free(&pcnp->io_handle);
	if (pcnp->mii)
		mii_destroy(pcnp->mii);
	kmem_free((caddr_t)macinfo,
		sizeof (gld_mac_info_t)+sizeof (struct pcninstance));
	return (DDI_FAILURE);
}

/*
 *  detach(9E) -- Detach a device from the system
 */

pcndetach(dev_info_t *devinfo, ddi_detach_cmd_t cmd)
{
	gld_mac_info_t *macinfo;		/* GLD structure */
	struct pcninstance *pcnp;		/* Our private device info */

#ifdef PCNDEBUG
	if (pcndebug & PCNDDI)
		cmn_err(CE_CONT, "pcndetach(0x%p)", (void *) devinfo);
#endif

	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	/* Get the driver private (gld_mac_info_t) structure */
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(devinfo);
	pcnp = (struct pcninstance *)(macinfo->gldm_private);

	/* stop the board if it is running */
	(void) pcn_stop_board(macinfo);

	if (pcnp->dma_attached >= 0) {
		(void) ddi_dmae_disable(pcnp->devinfo, pcnp->dma_attached);
		(void) ddi_dmae_release(pcnp->devinfo, pcnp->dma_attached);
	}
	pcn_ShredMem(pcnp);

	if (pcnp->mii)
		mii_destroy(pcnp->mii);
	ddi_regs_map_free(&pcnp->io_handle);


	/*
	 *  Unregister ourselves from the GLD interface
	 *
	 *  gld_unregister will:
	 *	remove the minor node;
	 *	unmap the registers;
	 *	remove the interrupt;
	 *	destroy the gldm_intrlock mutex;
	 *	unlink us from the GLD system.
	 */
	if (gld_unregister(macinfo) == DDI_SUCCESS) {
		kmem_free((caddr_t)macinfo,
			sizeof (gld_mac_info_t)+sizeof (struct pcninstance));
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*
 * XXX
 * This function gets called during system reboot operation. It is not
 * documented on what this function should do!  Implementation of this
 * function was needed (at least) on PowerPC machines to disable
 * the device interrupts before returning to the firmware for system
 * reset operation (see bug id 1218533).
 */
pcnreset(dev_info_t *devinfo, ddi_reset_cmd_t cmd)
{
	gld_mac_info_t *macinfo;		/* GLD structure */

#ifdef PCNDEBUG
	if (pcndebug & PCNDDI)
		cmn_err(CE_CONT, "pcnreset(0x%p)", (void *) devinfo);
#endif

	if (cmd != DDI_RESET_FORCE)
		return (DDI_FAILURE);

	/* Get the driver private (gld_mac_info_t) structure */
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(devinfo);

	/* stop the board if it is running */
	(void) pcn_stop_board(macinfo);

	return (DDI_SUCCESS);
}

/*
 *  GLD Entry Points
 */

/*
 *  pcn_reset() -- reset the board to initial state; restore the machine
 *  address afterwards.
 */

int
pcn_reset(gld_mac_info_t *macinfo)
{
#ifdef PCNDEBUG
	if (pcndebug & PCNTRACE)
		cmn_err(CE_CONT, "pcn_reset(0x%p)", (void *) macinfo);
#endif

	(void) pcn_stop_board(macinfo);
	(void) pcn_init_board(macinfo);
	(void) pcn_saddr(macinfo);
	return (0);
}

/*
 *  pcn_init_board() -- initialize the specified network board.
 */

static int
pcn_init_board(gld_mac_info_t *macinfo)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;
	int	j;

	pcn_StopLANCE(pcnp);
	pcn_ResetRings(pcnp);

	if (pcn_GetBusType(pcnp->devinfo) == PCN_BUS_PCI)
		pcn_EnablePCI(pcnp, 1);

#if defined(SSIZE32)
	/* Kick board into software size 32 and PCnet PCI mode */
	pcn_OutBCR(pcnp, BCR_SWS, BCR_SWS_PCNET_PCI);
#endif

	/* Select the port that we wish to use */
	if (pcnp->phy_supported && pcnp->soft_phy)
		pcn_select_port(pcnp, pcnp->curport);

	pcn_OutCSR(pcnp, CSR1, (ushort_t)pcnp->phys_initp);
	pcn_OutCSR(pcnp, CSR2, (ushort_t)(pcnp->phys_initp>>16));
	pcn_OutCSR(pcnp, CSR0, CSR0_INIT);
	/*
	 * Spin waiting for the LANCE to initialize, 1mS max.
	 */
	for (j = 0; j < 1000; j++) {
		if (pcn_InCSR(pcnp, CSR0) & (CSR0_IDON|CSR0_MERR))
			break;
		drv_usecwait(1);
	}

	return (!(pcn_InCSR(pcnp, CSR0) & CSR0_IDON));
}

/*
 *  pcn_start_board() -- start the board receiving and allow transmits.
 */

pcn_start_board(gld_mac_info_t *macinfo)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;
	int	j;

#ifdef PCNDEBUG
	if (pcndebug & PCNTRACE)
		cmn_err(CE_CONT, "pcn_start_board(0x%p)", (void *) macinfo);
#endif

	if (!(pcn_InCSR(pcnp, CSR0) & CSR0_INIT))
		return (1);	/* failed; adapter not initialized */

	/*
	 * Set the start bit and enable interrupts and wait 1mS for it to take
	 */
	pcn_OutCSR(pcnp, CSR0, CSR0_INEA | CSR0_STRT);
	for (j = 0; j < 1000; j++) {
		if (pcn_InCSR(pcnp, CSR0) & CSR0_STRT)
			break;
		drv_usecwait(1);
	}
	if (j == 1000)
		return (-1);
	else
		return (0);
}

/*
 *  pcn_stop_board() -- stop board receiving
 */

pcn_stop_board(gld_mac_info_t *macinfo)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;

#ifdef PCNDEBUG
	if (pcndebug & PCNTRACE)
		cmn_err(CE_CONT, "pcn_stop_board(0x%p)", (void *) macinfo);
#endif

	pcn_StopLANCE(pcnp);
	return (0);
}

/*
 *  pcn_saddr() -- set the physical network address on the board
 */

int
pcn_saddr(gld_mac_info_t *macinfo)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;

#ifdef PCNDEBUG
	if (pcndebug & PCNTRACE)
		cmn_err(CE_CONT, "pcn_saddr(0x%p)", (void *) macinfo);
#endif

	/*
	 * Copy in the Ethernet address
	 */
	pcn_StopLANCE(pcnp);
	bcopy((caddr_t)macinfo->gldm_macaddr,
		(caddr_t)pcnp->initp->PADR, ETHERADDRL);

	if (pcn_init_board(macinfo) || pcn_start_board(macinfo))
		return (1);

	return (0);
}

/*
 *  pcn_dlsdmult() -- set (enable) or disable a multicast address
 *
 *  Program the hardware to enable/disable the multicast address
 *  in "mcast".  Enable if "op" is non-zero, disable if zero.
 */

int
pcn_dlsdmult(gld_mac_info_t *macinfo, struct ether_addr *mcast, int op)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;
	int	index, bitindex, bitoffset, reset;

#ifdef PCNDEBUG
	if (pcndebug & PCNTRACE)
		cmn_err(CE_CONT, "pcn_dlsdmult(0x%p, %s)", (void *) macinfo,
				op ? "ON" : "OFF");
#endif

	index = pcn_LADR_index(mcast->ether_addr_octet);
	bitindex = index / (sizeof (pcnp->initp->LADRF[0]) * NBBY);
	bitoffset  = index % (sizeof (pcnp->initp->LADRF[0]) * NBBY);
	reset = 0;
	if (op) {
		if (pcnp->mcref[index] == 0) {
			pcnp->initp->LADRF[bitindex] |= (1<<bitoffset);
			reset = 1;
		}
		pcnp->mcref[index]++;
	} else {
		if (pcnp->mcref[index] > 0)
			if (--pcnp->mcref[index] == 0) {
				pcnp->initp->LADRF[bitindex] &= ~(1<<bitoffset);
				reset = 1;
			}
	}

	if (reset) {
		pcn_StopLANCE(pcnp);
		if (pcn_init_board(macinfo) || pcn_start_board(macinfo)) {
			return (1);
		}
	}

	return (0);
}

/*
 * pcn_prom() -- set or reset promiscuous mode on the board
 *
 *  Program the hardware to enable/disable promiscuous mode.
 *  Enable if "on" is non-zero, disable if zero.
 */

int
pcn_prom(gld_mac_info_t *macinfo, int on)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;
	int	reset;

#ifdef PCNDEBUG
	if (pcndebug & PCNTRACE)
		cmn_err(CE_CONT, "pcn_prom(0x%p, %s)", (void *) macinfo,
				on ? "ON" : "OFF");
#endif

	reset = 0;
	if (on) {
		if (!(pcnp->initp->MODE & MODE_PROM)) {
			pcnp->initp->MODE |= MODE_PROM;
			reset = 1;
		}
	} else {
		if (pcnp->initp->MODE & MODE_PROM) {
			pcnp->initp->MODE &= ~MODE_PROM;
			reset = 1;
		}
	}

	if (reset) {
		pcn_StopLANCE(pcnp);
		if (pcn_init_board(macinfo) || pcn_start_board(macinfo))
			return (1);
	}

	return (0);
}

/*
 * pcn_gstat() -- update statistics
 *
 *  GLD calls this routine just before it reads the driver's statistics
 *  structure.  If your board maintains statistics, this is the time to
 *  read them in and update the values in the structure.  If the driver
 *  maintains statistics continuously, this routine need do nothing.
 */

int
pcn_gstat(gld_mac_info_t *macinfo)
{
#ifdef PCNDEBUG
	if (pcndebug & PCNTRACE)
		cmn_err(CE_CONT, "pcn_gstat(0x%p)", (void *) macinfo);
#endif

	/* nothing to do */
	return (0);
}

/*
 *  pcn_send() -- send a packet
 *
 *  Called when a packet is ready to be transmitted. A pointer to an
 *  M_DATA message that contains the packet is passed to this routine.
 *  The complete LLC header is contained in the message's first message
 *  block, and the remainder of the packet is contained within
 *  additional M_DATA message blocks linked to the first message block.
 *
 *  This routine may NOT free the packet.
 */

#if defined(PCN_TXMEM_STATS)
static int single_frag = 0;
static int same_page = 0;
static int addr24bit = 0;
static int total_tx = 0;
#endif	/* defined(PCN_TXMEM_STATS) */

int
pcn_send(gld_mac_info_t *macinfo, mblk_t *mp)
{
	struct pcninstance *pcnp =		/* Our private device info */
			(struct pcninstance *)macinfo->gldm_private;
	int	mblen, seglen, bclen, totlen;
	int	index, end_index, start_index;
	int	avail;
	char	*segptr;
	union	PCN_XmitMsgDesc *ring;
	ushort_t	csr0;

#ifdef PCNDEBUG
	if (pcndebug & PCNSEND)
		cmn_err(CE_CONT, "pcn_send(0x%p, 0x%p)",
			(void *) macinfo, (void *) mp);
#endif

	/*
	 * Check to be sure transmitter hasn't shut down without
	 * telling us.  If it has, restart it.  This is a workaround
	 * for a purported hardware bug.
	 * NOTE:The transmitter WILL shutdown when there is a transmit underflow
	 * or a transmit memory error. This is documented behaviour. The FAST
	 * chips allow control of wheather or not the transmitter stops on a
	 * transmit underrun.
	 */

	csr0 = pcn_InCSR(pcnp, CSR0);
	if ((csr0 & CSR0_STOP) || (!(csr0 & CSR0_TXON))) {
#ifdef PCNDEBUG
		if (pcndebug & PCNTRACE)
			cmn_err(CE_NOTE,
			    "!pcn: transmitter/device shutdown.  Restarting");
#endif
		if (pcn_stop_board(macinfo) ||
		    pcn_init_board(macinfo) ||
		    pcn_start_board(macinfo)) {
			cmn_err(CE_NOTE, "!pcn: tx restart failed(?)");
			return (1);	/* ask GLD to try again later */
		}
	}

	/*
	 * Make sure we have room
	 */
	ring = (union PCN_XmitMsgDesc *)pcnp->tx_ringp;
	avail = pcnp->tx_avail;
	ASSERT(avail >= 0);
	start_index = pcnp->tx_index;
	if ((msgdsize(mp) > (avail*PCN_TX_BUF_SIZE)) ||
	    ring[start_index].tmd_bits.own) {
		macinfo->gldm_stats.glds_defer++;
		return (1);
	}

#if defined(PCN_TXMEM_STATS)
	total_tx++;

	/* EXPERIMENTAL: tally up number of single frame packets */
	if (mp->b_cont == 0) {
		char	*ptr;
		short	page;

		/* single mbuf frag */
		single_frag++;
		ptr = (char *)mp->b_rptr;
		page = hat_getkpfnum(ptr);

		if (page == hat_getkpfnum(ptr+msgdsize(mp)-1)) {
		/* same MMU page */
			same_page++;
			if (page < 0x1000) {
			/* bottom 16M */
				addr24bit++;
			}
		}
	}
#endif	/* defined(PCN_TXMEM_STATS) */

	/*
	 * Copy the mbufs into txbufs
	 */
	totlen = seglen = 0;
	do {
		if (seglen <= 0) {
			pcnp->tx_avail--;
			ASSERT(pcnp->tx_avail >= 0);
			end_index = pcnp->tx_index;
			/*
			 * Check for TX out of space. This shouldn't
			 * ever happen. NEEDSWORK: completely drop packet?
			 */
			if (ring[end_index].tmd_bits.own) {
				pcnp->tx_index = start_index;
				pcnp->tx_avail = avail;
				return (0);
			}
			segptr = (char *)pcnp->tx_buf[end_index];
			seglen = PCN_TX_BUF_SIZE;

			/* The stuff below wipes the error/status bits clean */
#if defined(SSIZE32)
			ring[end_index].tmd[1] = 0;
			ring[end_index].tmd[2] = 0;
			/* tmd[3] is all reserved bits */
#else
			{
				unsigned short tbadru;

				tbadru = ring[end_index].tmd_bits.tbadru;
				ring[end_index].tmd[1] = 0;
				ring[end_index].tmd[3] = 0;
				ring[end_index].tmd_bits.tbadru = tbadru;
				/* tmd[2] is bcnt & ones which are set below */
			}
#endif
			ring[end_index].tmd_bits.ones = (ushort_t)~0;
			pcnp->tx_index = NextTXIndex(end_index);
		}
		mblen = (int)(mp->b_wptr - mp->b_rptr);
		bclen = (mblen > seglen) ? seglen : mblen;
		bcopy((char *)mp->b_rptr, segptr, bclen);
		seglen -= bclen;
		segptr += bclen;
		totlen += bclen;
		mp->b_rptr += bclen;
		ring[end_index].tmd_bits.bcnt = -(PCN_TX_BUF_SIZE - seglen);
		if (mblen <= bclen)
			mp = mp->b_cont;
	} while (mp != NULL);

	/*
	 * Now set the STP, ENP and OWN bits as required.
	 * Since LANCE looks for the STP/OWN bits set in the
	 * first buffer, work backwards in multiple buffers.
	 */
	if (start_index == end_index) {
		if (totlen < PCNMINPKT) {
			ring[end_index].tmd_bits.bcnt = (ushort_t)-PCNMINPKT;
		}
		ring[start_index].tmd_bits.stp = B_TRUE;
		ring[start_index].tmd_bits.enp = B_TRUE;
		ring[start_index].tmd_bits.own = B_TRUE;
	} else {
		index = end_index;
		while (index != start_index) {
			if (index == end_index) {
				ring[index].tmd_bits.enp = B_TRUE;
				ring[index].tmd_bits.own = B_TRUE;
			} else {
				ring[index].tmd_bits.own = B_TRUE;
			}
			index = PrevTXIndex(index);
		}
		ring[index].tmd_bits.stp = B_TRUE;
		ring[index].tmd_bits.own = B_TRUE;
	}

	/*
	 * Kick the transmitter
	 */
	pcn_OutCSR(pcnp, CSR0, CSR0_INEA | CSR0_TDMD);

	return (0);		/* successful transmit attempt */
}

/*
 *  pcnintr() -- interrupt from board to inform us that a receive or
 *  transmit has completed.
 */

uint_t
pcnintr(caddr_t ptr)
{
	gld_mac_info_t *macinfo;
	struct pcninstance *pcnp;
	int	reset_required = 0;
	ushort_t	csr0;

	macinfo = (gld_mac_info_t *)ptr;
	pcnp = (struct pcninstance *)macinfo->gldm_private;

	/*
	 * Verify our adapter is interrupting
	 */
	if (!(pcn_InCSR(pcnp, CSR0) & CSR0_INTR))
		return (DDI_INTR_UNCLAIMED);	/* not this adapter */

#ifdef PCNDEBUG
	if (pcndebug & PCNINT)
		cmn_err(CE_CONT, "pcnintr(0x%p)", (void *) macinfo);
#endif

	macinfo->gldm_stats.glds_intr++;

	/*
	 * Loop on the adapter servicing interrupts until he shuts up
	 */
	while ((csr0 = pcn_InCSR(pcnp, CSR0)) & CSR0_INTR) {
		if (csr0 & CSR0_RINT) {
			if (reset_required =
			    pcn_ProcessReceive(macinfo, pcnp))
				break;
		}

		if (csr0 & CSR0_TINT) {
			pcn_ProcessTransmit(macinfo, pcnp);
		}

		if (csr0 & CSR0_MISS) {
			macinfo->gldm_stats.glds_missed++;
		}

		if (csr0 & CSR0_IDON) {
			pcnp->init_intr++;
		}

		if (csr0 & CSR0_MERR) {
			if (pcnp->irq_scan || pcnp->dma_scan)
				pcnp->init_intr++;
			else {
				cmn_err(CE_NOTE,
				    "!pcn: memory error (attempting reset)");
				reset_required = 1;
				break;
			}
		}

		if (csr0 & CSR0_BABL) {
			/* NEEDSWORK: need we do anything here? */
			cmn_err(CE_NOTE, "!pcn: babbling (attempting reset)");
			reset_required = 1;
			break;
		}

		/*
		 * Reset the interrupt bits
		 */
		pcn_OutCSR(pcnp, CSR0, csr0);
	}

	if (reset_required) {
		if (pcn_stop_board(macinfo) ||
		    pcn_init_board(macinfo) ||
		    pcn_start_board(macinfo)) {
			cmn_err(CE_CONT, "!pcn: failed to reset");
		}
	}

	return (DDI_INTR_CLAIMED);	/* Indicate it was our interrupt */
}

/*
 * Process a receive interrupt
 */
#define	ResetRXBuf(index)	{ \
	ring[(index)].rmd_bits.enp = \
	ring[(index)].rmd_bits.stp = \
	ring[(index)].rmd_bits.buff = \
	ring[(index)].rmd_bits.crc = \
	ring[(index)].rmd_bits.oflo = \
	ring[(index)].rmd_bits.fram = \
	ring[(index)].rmd_bits.err = (ushort_t)0; \
	ring[(index)].rmd[2] = 0; \
	ring[(index)].rmd[3] = 0; \
	ring[(index)].rmd_bits.bcnt = (ushort_t)-PCN_RX_BUF_SIZE; \
	ring[(index)].rmd_bits.ones = (ushort_t)~0; \
	ring[(index)].rmd_bits.own = (ushort_t)B_TRUE; \
}

static int
pcn_ProcessReceive(gld_mac_info_t *macinfo, struct pcninstance *pcnp)
{
	int	start_index, end_index, index, count;
	int	msg_size, bclen, done;
	union	PCN_RecvMsgDesc *ring;
	mblk_t	*mp;
	char	*rp;

#ifdef PCNDEBUG
	if (pcndebug & PCNINT)
		cmn_err(CE_CONT, "pcn_ProcessReceive called");
#endif
	/*
	 * start looking at rx_index for a descriptor we own
	 */
	ring = (union PCN_RecvMsgDesc *)pcnp->rx_ringp;

pr_retry:
	index = pcnp->rx_index;
	count = PCN_RX_RING_SIZE;
	while (count > 0) {
		if (! (ring[index].rmd_bits.own)) {
			if (ring[index].rmd_bits.stp)
				break;
			else {
				/* reset the buffer and give it back to LANCE */
				cmn_err(CE_NOTE, "!pcn: rx ring broken?");
				ResetRXBuf(index);
			}
		} else {
			/* encountered ring desc we don't own */
			return (0);
		}
		pcnp->rx_index = index = NextRXIndex(index);
		count--;
	}

	/*
	 * If we orbited the entire ring and found nothing, return.
	 */
	if (count <= 0) {
		return (0);
	}

	/*
	 * Determine the size of the packet, also check for errors.
	 * When looking for the end of a frame, treat OFLO the same as ENP.
	 */
	start_index = index;
	done = 0;
	while (! (ring[index].rmd_bits.own)) {
		if (ring[index].rmd_bits.enp || ring[index].rmd_bits.err) {
			done = 1;
			break;
		}
		index = NextRXIndex(index);
	}
	/*
	 * If we exit the loop without setting done, then we have a
	 * partial frame and will get another interrupt when it is
	 * complete.  Return for now.
	 */
	if (! done)
		return (0);

	end_index = index;

	/*
	 * ERR indicates some error has occurred; if so, note the
	 * error and discard the frame.
	 */
	if (ring[end_index].rmd_bits.err) {
		int enp, oflo;

		/*
		 * If BUFF is set, ignore ENP entirely
		 */
		if (ring[end_index].rmd_bits.buff) {
			macinfo->gldm_stats.glds_overflow++;
			goto rx_discard;
		}

		/*
		 * If ENP is not set, and OFLO is,
		 * we have an overflow error
		 */
		enp  = ring[end_index].rmd_bits.enp;
		oflo = ring[end_index].rmd_bits.oflo;

		if (! enp && oflo) {
			macinfo->gldm_stats.glds_overflow++;
			goto rx_discard;
		} else if (enp && ! oflo) {
			if (ring[end_index].rmd_bits.crc)
				macinfo->gldm_stats.glds_crc++;

			if (ring[end_index].rmd_bits.fram)
				macinfo->gldm_stats.glds_frame++;
		}

rx_discard:
		index = start_index;
		done = 0;
		do {
			if (index == end_index)
				done = 1;
			ResetRXBuf(index);
			index = NextRXIndex(index);
		} while (!done);
		pcnp->rx_index = NextRXIndex(end_index);
		goto pr_retry;	/* yeah, I know I used a goto */
	}


	/*
	 * Bug workaround: if MISS bit is set in CSR0, drop this frame.
	 */
	if (pcn_InCSR(pcnp, CSR0) & CSR0_MISS) {
		cmn_err(CE_NOTE, "!pcn: possible RX frame corruption");
		/*
		 * return an error so the ISR resets the adapter
		 */
		return (1);
	}

	/*
	 * Got a good packet, now copy it out
	 */
	msg_size = ring[end_index].rmd_bits.mcnt - LANCE_FCS_SIZE;

	/*
	 * Get an mbuf to hold this lovely message
	 */
	if ((mp = allocb(msg_size, BPRI_MED)) != NULL) {
		rp = (char *)mp->b_wptr;
		mp->b_wptr = mp->b_rptr + msg_size;
		index = start_index;
		done = 0;
		do {
			bclen = (msg_size < PCN_RX_BUF_SIZE) ?
			    msg_size : PCN_RX_BUF_SIZE;
			bcopy((char *)pcnp->rx_buf[index], rp, bclen);
			rp += bclen;
			msg_size -= bclen;
			if (index == end_index)
				done = 1;
			ResetRXBuf(index);
			index = NextRXIndex(index);
		} while (!done);
		gld_recv(macinfo, mp);
	} else {
		macinfo->gldm_stats.glds_norcvbuf++;
		/*
		 * Drop the frame since we have nowhere to put it
		 */
		goto rx_discard;
	}

	pcnp->rx_index = NextRXIndex(end_index);
	goto pr_retry;	/* yeah, I know I used a goto */
}

#undef	ResetRXBuf

/*
 * Process a transmit interrupt
 */
static void
pcn_ProcessTransmit(gld_mac_info_t *macinfo, struct pcninstance *pcnp)
{
	int	index;
	static int total_underflows = 0;
	static int underflow_reported = 0;
	static char pcn_underflow_notice[] =
		"pcn: PCnet driver is experiencing too many underflows. "
		"PCI chipset may be at fault";

	union	PCN_XmitMsgDesc *ring;

#ifdef PCNDEBUG
	if (pcndebug & PCNINT)
		cmn_err(CE_CONT, "pcn_ProcessTransmit called");
#endif
	ring = (union PCN_XmitMsgDesc *)pcnp->tx_ringp;
	index = pcnp->tx_index_save;
	/*
	 * Since tx_index_save can be the same as tx_index
	 * when the ring is full, always check the ring
	 * when tx_avail is 0, even if tx_index_save and tx_index
	 * are equal (which normally indicates an empty ring)
	 */
	while ((pcnp->tx_avail == 0) || (index != pcnp->tx_index) &&
	    ! (ring[index].tmd_bits.own)) {

		if (total_underflows > 0) /* decay underflow counter */
		    total_underflows--;

		if (ring[index].tmd_bits.err)
			macinfo->gldm_stats.glds_errxmt++;

		if (ring[index].tmd_bits.def)
			macinfo->gldm_stats.glds_defer++;

		if (ring[index].tmd_bits.uflo) {
			if (!underflow_reported) {
				/*
				 * If we get 100 underflows within 100000
				 * packets, report PCI problem to the user.
				 */
				total_underflows += 1000;
				if (total_underflows > 100000) {
					cmn_err(CE_NOTE, pcn_underflow_notice);
					underflow_reported = 1;
				}
			}
			macinfo->gldm_stats.glds_underflow++;
		}

		if (ring[index].tmd_bits.lcar)
			macinfo->gldm_stats.glds_nocarrier++;

		if (ring[index].tmd_bits.lcol)
			macinfo->gldm_stats.glds_xmtlatecoll++;
		else if (ring[index].tmd_bits.one)
			macinfo->gldm_stats.glds_collisions++;

		if (ring[index].tmd_bits.rtry)
			macinfo->gldm_stats.glds_excoll++;

		pcnp->tx_avail++;
		index = NextTXIndex(index);
	}
	pcnp->tx_index_save = index;
	ASSERT(pcnp->tx_avail > 0);
}

/* NEEDSWORK */

/*
 * Called at attach time to determine which interrupt is in use.
 * Return 0 if IRQ identified and pcnp->irq filled in, or -1 otherwise.
 */
static int
pcn_ScanISA_IRQ(gld_mac_info_t *macinfo, int *irq_index)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;
	int	*irqarr, arrlen, arrsize;
	int	i, j;
	ddi_iblock_cookie_t ib_cookie;
	ddi_idevice_cookie_t id_cookie;

	/*
	 * Get the "interrupts" property
	 */
	if (ddi_prop_op(DDI_DEV_T_NONE, pcnp->devinfo, PROP_LEN_AND_VAL_ALLOC,
			    PROP_ARGS, "interrupts", (caddr_t)&irqarr, &arrsize)
			    != DDI_PROP_SUCCESS) {
#ifdef	PCNDEBUG
		cmn_err(CE_NOTE, "pcn: failed to find interrupts property");
#endif	/* PCNDEBUG */
		return (-1);
	}

	if (!(ddi_getprop(DDI_DEV_T_ANY, pcnp->devinfo, 0,
	    "ignore-hardware-nodes", 0))) {
		kmem_free(irqarr, arrsize);
		*irq_index = 0;
		return (0);
	}

	/*
	 *
	 */
	pcnp->irq_scan = 1;
	arrlen = (arrsize/sizeof (int))/2;
	for (i = 0; i < arrlen; i++) {
		/*
		 * Stop LANCE before attaching interrupt
		 */
		pcn_StopLANCE(pcnp);

		/*
		 * Attach our ISR
		 */
		if (ddi_add_intr(pcnp->devinfo, i, &ib_cookie, &id_cookie,
		    pcnintr, (caddr_t)macinfo) != DDI_SUCCESS) {
			continue;
		}
		pcnp->init_intr = 0;

		/*
		 * Prod the LANCE to generate an interrupt
		 */
		pcn_OutCSR(pcnp, CSR1, (ushort_t)pcnp->phys_initp);
		pcn_OutCSR(pcnp, CSR2, (ushort_t)(pcnp->phys_initp>>16));
		pcn_OutCSR(pcnp, CSR0, CSR0_INIT | CSR0_INEA);
		/*
		 * Spin on the 'done' var for 10mS max
		 */
		for (j = 0; j < 1000; j++) {
			if (pcnp->init_intr > 0)
				break;
			drv_usecwait(10);
		}
		/*
		 * Detach the ISR and then look to see what happened
		 */
		ddi_remove_intr(pcnp->devinfo, i, &ib_cookie);
		if (pcnp->init_intr > 0) {
			/* NEEDSWORK */
			*irq_index = i;
			break;
		}
	}

	pcnp->irq_scan = 0;
	kmem_free(irqarr, arrsize);
	return (i >= arrlen);	/* return != 0 if interrupt not found */
}

/*
 *
 */
static int
pcn_ScanISA_DMA(gld_mac_info_t *macinfo, int *dma)
{
	struct pcninstance *pcnp =		/* Our private device info */
		(struct pcninstance *)macinfo->gldm_private;
	int	*dmaarr, arrlen, arrsize;
	int	i, j;

	/*
	 * Get the DMA "dma-channels" property
	 */
	if (ddi_prop_op(DDI_DEV_T_NONE, pcnp->devinfo, PROP_LEN_AND_VAL_ALLOC,
	    PROP_ARGS, "dma-channels", (caddr_t)&dmaarr, &arrsize)
			    != DDI_PROP_SUCCESS) {
#ifdef	PCNDEBUG
		cmn_err(CE_NOTE, "pcn: failed to find dma-channels property");
#endif	/* PCNDEBUG */
		return (-1);
	}

	if (!(ddi_getprop(DDI_DEV_T_ANY, pcnp->devinfo, 0,
	    "ignore-hardware-nodes", 0))) {
		*dma = dmaarr[0];
		kmem_free(dmaarr, arrsize);
		return (0);
	}

	/*
	 *
	 */
	pcnp->dma_scan = 1;
	arrlen = arrsize/sizeof (int);
	for (i = 0; i < arrlen; i++) {
		/*
		 * Allocate and set the candidate DMA channel to 1stparty
		 */
		if (ddi_dmae_alloc(pcnp->devinfo, dmaarr[i],
			NULL, NULL) != DDI_SUCCESS) {
			continue;	/* NEEDSWORK? */
		}
		if (ddi_dmae_1stparty(pcnp->devinfo, dmaarr[i])
		    != DDI_SUCCESS) {
			(void) ddi_dmae_disable(pcnp->devinfo, dmaarr[i]);
			(void) ddi_dmae_release(pcnp->devinfo, dmaarr[i]);
			continue;
		}
		/*
		 * Now, attempt to initialize the LANCE and check for success.
		 */
		j = pcn_init_board(macinfo);

		/*
		 * Release the channel.
		 */
		(void) ddi_dmae_disable(pcnp->devinfo, dmaarr[i]);
		(void) ddi_dmae_release(pcnp->devinfo, dmaarr[i]);

		if (!j) {
			/* NEEDSWORK */
			*dma = dmaarr[i];
			break;
		}
	}

	pcnp->dma_scan = 0;
	kmem_free(dmaarr, arrsize);
	return (i >= arrlen);	/* return 0 if DMA identified */
}

/*
 * Check for PC-Net signature
 * Return DDI_PROBE_FAILURE for no adapter present,
 *        DDI_PROBE_SUCCESS for adapter present.
 */
static int
pcn_CheckSignature(ddi_acc_handle_t handle, int reg)
{
	int	i, csum;

	/*
	 * Pulse reset
	 */
#if defined(_WIO)
	(void) ddi_io_getw(handle, REG16(reg, PCN_IO_RESET));
	drv_usecwait(1);
	ddi_io_putw(handle, REG16(reg, PCN_IO_RESET), 0);
#elif defined(_DWIO)
	ddi_io_getl(handle, REG32(reg, PCN_IO_RESET));
	drv_usecwait(1);
	ddi_io_putl(handle, REG32(reg, PCN_IO_RESET), 0);
#else
#error Neither _WIO nor _DWIO defined!
#endif

	/* Add a 10msec delay for some adapters to settle down */
	drv_usecwait(10000);

	/*
	 * Look for the WW first (accept no imitations).
	 */
	if (!((ddi_io_getb(handle, REG8(reg, 14)) == 'W') &&
		(ddi_io_getb(handle, REG8(reg, 15)) == 'W')))
		return (DDI_PROBE_FAILURE);

	/*
	 * Checksum the first sixteen byte ports
	 */
	csum = 0;
	for (i = 0; i < 12; i++)
		csum += ddi_io_getb(handle, REG8(reg, i));

	csum += ddi_io_getb(handle, REG8(reg, 14));
	csum += ddi_io_getb(handle, REG8(reg, 15));

	if (csum != ddi_io_getw(handle, REG16(reg, 12)))
		return (DDI_PROBE_FAILURE);

	return (DDI_PROBE_SUCCESS);
}


/*
 *
 */
static void
pcn_EnablePCI(struct pcninstance *pcnp, int on_or_off)
{
	ddi_acc_handle_t handle;
	ushort_t	tmp;

	if (pci_config_setup(pcnp->devinfo, &handle) != DDI_SUCCESS)  {
		cmn_err(CE_WARN, "pcn: PCI error enabling adapter");
		return;
	}

	tmp = pci_config_getw(handle, PCI_CONF_COMM);

	if (on_or_off)
		tmp |= PCI_COMM_ME;
	else
		tmp &= ~PCI_COMM_ME;

	pci_config_putw(handle, PCI_CONF_COMM, tmp);
	pci_config_teardown(&handle);
}


#if defined(_WIO)
/*
 *
 */
static void
pcn_OutCSR(struct pcninstance *pcnp, int reg, ushort_t value)
{
	ddi_io_putw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RAP), reg);
	ddi_io_putw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RDP), value);
}

/*
 *
 */
static void
pcn_OutBCR(struct pcninstance *pcnp, int reg, ushort_t value)
{
	ddi_io_putw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RAP), reg);
	ddi_io_putw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_BDP), value);
}


/*
 *
 */
static ushort_t
pcn_InCSR(struct pcninstance *pcnp, int reg)
{
	ddi_io_putw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RAP), reg);
	return (ddi_io_getw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RDP)));
}

/*
 *
 */
static ushort_t
pcn_InBCR(struct pcninstance *pcnp, int reg)
{
	ddi_io_putw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RAP), reg);
	return (ddi_io_getw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_BDP)));
}

#elif defined(_DWIO)
/*
 *
 */
static void
pcn_OutCSR(struct pcninstance *pcnp, int reg, ushort_t value)
{
	ddi_io_putl(pcnp->io_handle,
		REG32(pcnp->io_reg, PCN_IO_RAP), (ulong_t)reg);
	ddi_io_putl(pcnp->io_handle,
		REG32(pcnp->io_reg, PCN_IO_RDP), (ulong_t)value);
}

/*
 *
 */
static void
pcn_OutBCR(struct pcninstance *pcnp, int reg, ushort_t value)
{
	ddi_io_putl(pcnp->io_handle,
		REG32(pcnp->io_reg, PCN_IO_RAP), (ulong_t)reg);
	ddi_io_putl(pcnp->io_handle, REG32(pcnp->io_reg, PCN_IO_BDP),
		(ulong_t)value);
}

/*
 *
 */
static ushort_t
pcn_InCSR(struct pcninstance *pcnp, int reg)
{
	ddi_io_putl(pcnp->io_handle, REG32(pcnp->io_reg, PCN_IO_RAP),
		(ulong_t)reg);
	return (ddi_io_getw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RDP)));
}

/*
 *
 */
static ushort_t
pcn_InBCR(struct pcninstance *pcnp, int reg)
{
	ddi_io_putl(pcnp->io_handle,
		REG32(pcnp->io_reg, PCN_IO_RAP), (ulong_t)reg);
	return (ddi_io_getw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_BDP)));
}

#else
#error Neither _WIO nor _DWIO defined!
#endif

/*
 *
 */
static void
pcn_StopLANCE(struct pcninstance *pcnp)
{

	register int i;

	if (pcn_GetBusType(pcnp->devinfo) == PCN_BUS_PCI)
		pcn_EnablePCI(pcnp, 0);

	pcn_OutCSR(pcnp, CSR0, CSR0_STOP);	/* also resets CSR0_INEA */
	for (i = 0; i < 100000; ++i) {
		if (pcn_InCSR(pcnp, CSR0) & CSR0_STOP)
			break;
		drv_usecwait(20);	/* wait for the STOP to take */
	}
#ifdef PCNDEBUG
	if (! (pcn_InCSR(pcnp, CSR0) & CSR0_STOP))
		printf("pcn_StopLANCE: failed to stop!\n");
#endif

#if defined(_WIO)
	(void) ddi_io_getw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RESET));
	drv_usecwait(1);
	ddi_io_putw(pcnp->io_handle, REG16(pcnp->io_reg, PCN_IO_RESET), 0);
#elif defined(_DWIO)
	ddi_io_getl(pcnp->io_handle, REG32(pcnp->io_reg, PCN_IO_RESET));
	drv_usecwait(1);
	ddi_io_putl(pcnp->io_handle, REG32(pcnp->io_reg, PCN_IO_RESET), 0);
#else
#error Neither _WIO nor _DWIO defined!
#endif

	pcn_OutCSR(pcnp, CSR0, CSR0_STOP);	/* also resets CSR0_INEA */
	for (i = 0; i < 100000; ++i) {
		if (pcn_InCSR(pcnp, CSR0) & CSR0_STOP)
			break;
		drv_usecwait(1);	/* wait for the STOP to take */
	}
	if (! (pcn_InCSR(pcnp, CSR0) & CSR0_STOP))
		printf("pcn_StopLANCE: failed to stop after reset!\n");

	pcnp->tx_index = 0;
	pcnp->tx_index_save = 0;
	pcnp->rx_index = 0;
}

/*
 * Initialize all data structures used to communicate with the LANCE.
 * The LANCE must be stopped before calling this function.
 */
static int
pcn_InitData(struct pcninstance *pcnp, uchar_t *macaddr)
{
	void	*ptr;
	ulong_t	pptr;
	int	i;

	/*
	 * Allocate the initblock
	 */
	if (pcn_GetMem(pcnp, sizeof (*pcnp->initp), (void *) &pcnp->initp,
	    &pcnp->phys_initp))
		return (1);


	/*
	 * Default LANCE MODE setting is 0.
	 */
	pcnp->initp->MODE = 0;		/* NEEDSWORK: constant */

	/*
	 * Copy in the Ethernet address
	 */
	bcopy((caddr_t)macaddr,
		(caddr_t)pcnp->initp->PADR, ETHERADDRL);

	/*
	 * Fill the Multicast array with 0s
	 */
	bzero((caddr_t)pcnp->initp->LADRF, sizeof (pcnp->initp->LADRF));

	/*
	 * Create the RX Ring Ptr
	 */
	if (pcn_GetMem(pcnp, PCN_RX_RING_SIZE * sizeof (union PCN_RecvMsgDesc),
	    (void *) &pcnp->rx_ringp, &pptr))
		return (1);

#if defined(SSIZE32)
	pcnp->initp->RDRA = pptr;
	pcnp->initp->RLEN = PCN_RX_RING_VAL;
#else
	pcnp->initp->RDRAL = (ushort_t)pptr;
	pcnp->initp->RDRAU = ((ushort_t)(pptr >> 16));
	pcnp->initp->RLEN = PCN_RX_RING_VAL;
#endif


	/*
	 * Create the TX Ring Ptr
	 */
	if (pcn_GetMem(pcnp, PCN_TX_RING_SIZE * sizeof (union PCN_XmitMsgDesc),
	    (void *) &pcnp->tx_ringp, &pptr))
		return (1);

#if defined(SSIZE32)
	pcnp->initp->TDRA = pptr;
	pcnp->initp->TLEN = PCN_TX_RING_VAL;
#else
	pcnp->initp->TDRAL = (ushort_t)pptr;
	pcnp->initp->TDRAU = ((ushort_t)(pptr >> 16));
	pcnp->initp->TLEN = PCN_TX_RING_VAL;
#endif

	/*
	 * populate the buffer arrays
	 */
	for (i = 0; i < PCN_RX_RING_SIZE; i++) {
		if (pcn_GetMem(pcnp, PCN_RX_BUF_SIZE, &ptr, &pptr))
			return (1);
		pcnp->rx_buf[i] = ptr;
		pcn_MakeRecvDesc(((union PCN_RecvMsgDesc *)pcnp->rx_ringp) + i,
					pptr, -PCN_RX_BUF_SIZE, B_TRUE);
	}

	for (i = 0; i < PCN_TX_RING_SIZE; i++) {
		if (pcn_GetMem(pcnp, PCN_TX_BUF_SIZE, &ptr, &pptr))
			return (1);
		pcnp->tx_buf[i] = ptr;
		pcn_MakeXmitDesc(((union PCN_XmitMsgDesc *)pcnp->tx_ringp) + i,
		    pptr, 0, B_FALSE);
	}
	pcnp->tx_avail = PCN_TX_RING_SIZE;

	return (0);
}

/*
 *
 */
static void
pcn_MakeXmitDesc(
	union PCN_XmitMsgDesc *ptr,
	ulong_t addr,
	ushort_t size,
	int owned)
{
	bzero((caddr_t)ptr, sizeof (*ptr));
#if defined(SSIZE32)
	ptr->tmd_bits.tbadr = (ushort_t)addr;
#else
	ptr->tmd_bits.tbadrl = (ushort_t)addr;
	ptr->tmd_bits.tbadru = ((ushort_t)(addr >> 16));
#endif
	ptr->tmd_bits.ones = (ushort_t)~0;
	ptr->tmd_bits.bcnt = (ushort_t)size;
	ptr->tmd_bits.own = (ushort_t)owned;
}

/*
 *
 */
/* ARGSUSED */
static void
pcn_MakeRecvDesc(
	union PCN_RecvMsgDesc *ptr,
	ulong_t addr,
	ushort_t size,
	int owned)
{
	bzero((caddr_t)ptr, sizeof (*ptr));
#if defined(SSIZE32)
	ptr->rmd_bits.rbadr = addr;
#else
	ptr->rmd_bits.rbadrl = (ushort_t)addr;
	ptr->rmd_bits.rbadru = ((ushort_t)(addr >> 16));
#endif
	ptr->rmd_bits.own = (ushort_t)owned;
}


static void
pcn_ResetRings(struct pcninstance *pcnp)
{
	union PCN_XmitMsgDesc *xring;
	union PCN_RecvMsgDesc *rring;
	int	index;

	xring = (union PCN_XmitMsgDesc *)pcnp->tx_ringp;
	for (index = 0; index < PCN_TX_RING_SIZE; index++) {
		xring[index].tmd_bits.enp   = (ushort_t)B_FALSE;
		xring[index].tmd_bits.stp   = (ushort_t)B_FALSE;
		xring[index].tmd_bits.def   = (ushort_t)B_FALSE;
		xring[index].tmd_bits.one   = (ushort_t)B_FALSE;
		xring[index].tmd_bits.more  = (ushort_t)B_FALSE;
		xring[index].tmd_bits.add_fcs = (ushort_t)B_FALSE;
		xring[index].tmd_bits.err   = (ushort_t)B_FALSE;
		xring[index].tmd_bits.own   = (ushort_t)B_FALSE;
		xring[index].tmd_bits.bcnt  = (ushort_t)0;
		xring[index].tmd_bits.ones  = (ushort_t)~0;
		xring[index].tmd_bits.tdr   = (ushort_t)0;
		xring[index].tmd_bits.rtry  = (ushort_t)B_FALSE;
		xring[index].tmd_bits.lcar  = (ushort_t)B_FALSE;
		xring[index].tmd_bits.lcol  = (ushort_t)B_FALSE;
		xring[index].tmd_bits.exdef = (ushort_t)B_FALSE;
		xring[index].tmd_bits.uflo  = (ushort_t)B_FALSE;
		xring[index].tmd_bits.buff  = (ushort_t)B_FALSE;
#if defined(SSIZE32)
		xring[index].tmd_bits.trc   = (ushort_t)0;
#endif
	}

	rring = (union PCN_RecvMsgDesc *)pcnp->rx_ringp;
	for (index = 0; index < PCN_RX_RING_SIZE; index++) {
		rring[index].rmd_bits.own   = (ushort_t)B_TRUE;
		rring[index].rmd_bits.err   = (ushort_t)B_FALSE;
		rring[index].rmd_bits.fram  = (ushort_t)B_FALSE;
		rring[index].rmd_bits.oflo  = (ushort_t)B_FALSE;
		rring[index].rmd_bits.crc   = (ushort_t)B_FALSE;
		rring[index].rmd_bits.buff  = (ushort_t)B_FALSE;
		rring[index].rmd_bits.stp   = (ushort_t)B_FALSE;
		rring[index].rmd_bits.enp   = (ushort_t)B_FALSE;
		rring[index].rmd_bits.ones  = (ushort_t)~0;
		rring[index].rmd_bits.bcnt  = (ushort_t)-PCN_RX_BUF_SIZE;
		rring[index].rmd_bits.mcnt  = (ushort_t)0;
		rring[index].rmd_bits.zeros = 0;
#if defined(SSIZE32)
		rring[index].rmd_bits.rpc   = (ushort_t)0;
		rring[index].rmd_bits.rcc   = (ushort_t)0;
#endif
	}
	pcnp->tx_avail = PCN_TX_RING_SIZE;
}

/*
 * Calculate the LADRF index for a given address
 */
int
pcn_LADR_index(uchar_t *cp)
{
	register ulong_t crc;
	register ulong_t c;
	register int len;
	int	j;

	c = *cp;
	crc = (ulong_t)0xffffffff;
	len = 6;
	while (len-- > 0) {
		c = *cp;
		for (j = 0; j < 8; j++) {
			if ((c & 0x01) ^ (crc & 0x01)) {
				crc >>= 1;
				/* polynomial */
				crc = crc ^ 0xedb88320;
			} else
				crc >>= 1;
			c >>= 1;
		}
		cp++;
	}
	/* Just want the 6 most significant bits. */
	crc = crc >> 26;

	return (crc);
}


/*
 * Allocate and free IO memory
 */


/*
 * Return the address of a PCN_IOmem struct or NULL
 */
static struct PCN_IOmem *
pcn_GetPage(struct pcninstance *pcnp)
{
	struct PCN_IOmem *pmp;
	caddr_t	pp;
	uint_t	pfnum;

	/*
	 * First get a management structure
	 */
	if ((pmp = (struct PCN_IOmem *)kmem_alloc(sizeof (*pmp), KM_NOSLEEP))
	    == NULL)
		return (NULL);

	if (ddi_iopb_alloc(pcnp->devinfo, &pcnp->dmalim, pcnp->page_size, &pp)
		!= DDI_SUCCESS) {
		kmem_free(pmp, sizeof (*pmp));
		return (NULL);
	}
	/*
	 * NOTE NOTE NOTE
	 * Since we get a page aligned chunk-o-ram, we assume alignment is
	 * at least on a 16 byte boundary
	 */
	pmp->vbase = pmp->vptr = (void *) pp;
	pmp->avail = pcnp->page_size;
	/*
	 * get physical base address
	 */
	ASSERT(((ulong_t)pp & 0xfff) == 0);
	pfnum = hat_getkpfnum(pp);
	pmp->pbase = pmp->pptr = pfnum * pcnp->page_size;
	pmp->next = NULL;

	return (pmp);
}

/*
 * Allocate I/O memory
 *
 * If successful, allocate size bytes and return the virtual and physical
 * addresses of the RAM in the provided variables; function returns 0,
 * else no RAM is allocated and function returns -1.
 */

static int
pcn_GetMem(struct pcninstance *pcnp, int size, void **vaddr, ulong_t *paddr)
{
	struct PCN_IOmem *pmp, *pmp1;
	int	have_room;
	int	min_diff, diff;

	/*
	 * Reject any requests we have no chance of satisfying
	 */
	size = (size+15)&(~15);	/* round up to 16 byte granule */
	if (size > pcnp->page_size)
		return (-1);

	/*
	 * Hatch the chicken if need be.
	 */
	if (pcnp->iomemp == NULL)
		if ((pcnp->iomemp = pcn_GetPage(pcnp)) == NULL)
			return (-1);

	/*
	 * Scan the list looking for the smallest avail that is large enough
	 * to satisfy request.
	 */
	pmp = pcnp->iomemp;
	min_diff = pcnp->page_size+1;
	have_room = 0;
	while (pmp) {
		diff = pmp->avail - size;
		if (diff >= 0) {
			have_room = 1;
			if (diff < min_diff) {
				min_diff = diff;
				pmp1 = pmp;
			}
		}
		pmp = pmp->next;
	}
	pmp = pmp1;

	/*
	 * If we didn't find enough, get a new page and stuff it
	 * on the front of the list.
	 */
	if (!have_room) {
		if ((pmp = pcn_GetPage(pcnp)) == NULL)
			return (-1);
		pmp->next = pcnp->iomemp;
		pcnp->iomemp = pmp;
	}
	/*
	 * Now satisfy the request
	 */
	*vaddr = pmp->vptr;
	pmp->vptr = (uchar_t *)pmp->vptr + size;
	*paddr = DMA_ADDR(pmp->pptr);
	pmp->pptr += size;
	pmp->avail -= size;

	return (0);
}

/*
 *
 */
static void
pcn_ShredMem(struct pcninstance *pcnp)
{
	struct PCN_IOmem *pmp;

	pmp = pcnp->iomemp;
	pcnp->iomemp = NULL;
	while (pmp) {
		ddi_iopb_free((caddr_t)pmp->vbase);
		pmp = pmp->next;
	}
}

/*
 * Suspend /resume the transmitter and receiver to allow access to all CSRs/BCRs
 * This ONLY works on the '71, (and probably later chips), and is used for
 * some of the MII operations. (also only supported on the '71
 */
static void
pcn_suspend(struct pcninstance *pcnp)
{
	int cnt;
	ulong_t csr;
	/* No need to suspend adapter if it is stopped */
	if (pcn_InCSR(pcnp, CSR0) & (CSR0_STOP)) {
		cmn_err(CE_CONT, "!pcn: suspend req. when stopped ignored");
		return;
	}
	csr = pcn_InCSR(pcnp, 7) | (1<<15);
	pcn_OutCSR(pcnp, 7, csr);
	csr = pcn_InCSR(pcnp, CSR5) | CSR5_SPND;
	pcn_OutCSR(pcnp, CSR5, csr);
	for (cnt = 100; cnt > 0; cnt--) {
		if ((csr = pcn_InCSR(pcnp, CSR5)) & CSR5_SPND)
			break;
		drv_usecwait(500);
	}
	if (cnt == 0)
		cmn_err(CE_CONT, "!pcn: Could not suspend adapter");
}

static void pcn_resume(struct pcninstance *pcnp)
{
	ulong_t csr5 = pcn_InCSR(pcnp, CSR5);
	csr5 &= ~CSR5_SPND;
	pcn_OutCSR(pcnp, CSR5, csr5);
}

/* General PHY support routines */
static ushort_t
pcn_miiread(dev_info_t *dip, int phy, int reg)
{
	struct pcninstance *pcnp;
	gld_mac_info_t *macinfo;
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	pcnp = (struct pcninstance *)(macinfo->gldm_private);
	pcn_OutBCR(pcnp, BCR_MIIAR, phy << 5 | reg);
	return (pcn_InBCR(pcnp, BCR_MIIDR));
}

static void
pcn_miiwrite(dev_info_t *dip, int phy, int reg, int val)
{
	struct pcninstance *pcnp;
	gld_mac_info_t *macinfo;
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	pcnp = (struct pcninstance *)(macinfo->gldm_private);
	pcn_OutBCR(pcnp, BCR_MIIAR, phy << 5 | reg);
	pcn_OutBCR(pcnp, BCR_MIIDR, val);
}

/*
 * Make the driver interface to the PHY, taking over the functions of the
 * AMD chip's automatic setup and port selection
 */
static int
do_phy(gld_mac_info_t *macinfo)
{
	struct pcninstance *pcnp =
	    (struct pcninstance *)(macinfo->gldm_private);
	int phy;
	ulong_t miics;


	/* Disable autonegotiation so we can do it ourselves */
	miics = pcn_InBCR(pcnp, BCR_MIICS);
	miics |= BCR_MIICS_DANAS;
	pcn_OutBCR(pcnp, BCR_MIICS, miics);
	pcn_OutBCR(pcnp, BCR_MC, 0);

	/* Only initialise PHYs once */
	if (pcnp->mii)
		return (0);
	/*
	 * NEEDSWORK: The chip can actually support multiple PHYs, but it is
	 * unlikely that a board will ever have another
	 */
	if (mii_create(pcnp->devinfo, pcn_miiwrite, pcn_miiread, &pcnp->mii) !=
	    MII_SUCCESS)
		return (-1);

	pcnp->pcn_exphy_addr = -1; /* so we can tell there is no external PHY */
	if (pcn_InBCR(pcnp, BCR_MIICS) & BCR_MIICS_MIIPD) {
		/*
		 * External PHY defaults to address 0, but at least one card
		 * has it at address 1 (HP's Tweety prototype)
		 */
		for (phy = 0; phy < 31; phy++)
			if (mii_probe_phy(pcnp->mii, phy) == MII_SUCCESS) {
				pcnp->pcn_exphy_addr = phy;

				/* XXX function return value is ignored */
				(void) mii_init_phy(pcnp->mii, phy);
			}
	}

	/* Initialize the internal PHY */
	/* XXX function return value is ignored */
	(void) mii_init_phy(pcnp->mii, 31);

	/* XXX function return value is ignored */
	(void) mii_start_portmon(pcnp->mii, pcn_miilinkstate,
		&macinfo->gldm_maclock);
	return (0);
}

static void
pcn_process_phy(gld_mac_info_t *macinfo) /* Handle PHY work if required */
{
	struct pcninstance *pcnp =
	    (struct pcninstance *)(macinfo->gldm_private);

	/*
	 * phy_supported can be used for other (newer) chips that
	 * support PHYs through the same register set as the '71 but report a
	 * different part ID. (Default is supported on AM79C971 only)
	 */

	pcnp->phy_supported = ddi_getprop(DDI_DEV_T_ANY, pcnp->devinfo, 0,
				"phy-supported", pcnp->devtype == pcn_fast);

	pcnp->soft_phy = ddi_getprop(DDI_DEV_T_ANY, pcnp->devinfo, 0,
								"soft-phy", 0);

	/*
	 * The AMD chip is pretty clever, and doesn't really need us to
	 * interfere at all. But, this eliminates the possibility of fixing
	 * speed, and other low level control of the PHY, which may be required
	 */

	if (pcnp->phy_supported) {
		/* Device support PHYs in the same manner as the AM79C971 */
		if (pcnp->soft_phy) {
			/* Let the driver handle the PHY */
			/* XXX function return value is ignored */
			(void) do_phy(macinfo);
		} else {
			/*
			 * User specified that we allow the AM79C971 to handle
			 * all PHY programming. We may have previously disabled
			 * autonegotiation and auto port selection.
			 * Make sure they are enabled.
			 */
			int mc, miics = pcn_InBCR(pcnp, BCR_MIICS);
			pcn_OutBCR(pcnp, BCR_MIICS, miics & ~BCR_MIICS_DANAS);
			mc = pcn_InBCR(pcnp, BCR_MC);
			pcn_OutBCR(pcnp, BCR_MC, mc | BCR_MC_AUTO_SELECT);
		}
	}
}

static void
pcn_select_port(struct pcninstance *pcnp, ulong_t port)
{
	/* Select the PHY as the output device, not the AUI system */
	unsigned long csr;
	pcnp->curport = port; /* So we know what to select after a reset */

	pcn_suspend(pcnp);
	/* Change PORTSEL */
	csr = pcn_InCSR(pcnp, CSR15);
	csr &= ~PORTSEL_MASK;
	csr |= port;
	pcn_OutCSR(pcnp, CSR15, csr);
	pcn_resume(pcnp);

#ifdef PCNDEBUG
	if (pcndebug & PCNPHY)
		cmn_err(CE_NOTE, "!pcn: Switching to port %s",
					port == PORTSEL_AUI ? "AUI" :
					port == PORTSEL_10BASET ? "10BASET" :
					port == PORTSEL_MII ? "MII":
					"Unknown");
#endif
}

static void
pcn_miilinkstate(dev_info_t *dip, int phy, enum mii_phy_state state)
{
	struct pcninstance *pcnp;
	gld_mac_info_t *macinfo;
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	pcnp = (struct pcninstance *)(macinfo->gldm_private);
#ifdef PCNDEBUG
	if (pcndebug & PCNPHY)
		cmn_err(CE_CONT, "!pcn: Link state change to %d, PHY %d",
					state, phy);
#endif

	if (phy == 31)
		pcnp->inphy_state = state;
	else if (phy >= 0 && phy < 31)
		pcnp->exphy_state = state;

	pcn_suspend(pcnp);
	if (pcnp->exphy_state == phy_state_linkup)
		pcn_select_port(pcnp, PORTSEL_MII);
	else if (pcnp->inphy_state == phy_state_linkup)
		pcn_select_port(pcnp, PORTSEL_10BASET);
	else
		pcn_select_port(pcnp, PORTSEL_AUI);
	pcn_resume(pcnp);
}


/*
 * Detect hardware, and do initialisation that won't change without a hard
 * reset.
 */
static void pcn_hard_init(gld_mac_info_t *macinfo)
{
	struct pcninstance *pcnp = (struct pcninstance *)macinfo->gldm_private;
	ushort_t csr;
	unsigned int chipid;

	chipid = pcn_InCSR(pcnp, CSR88) | (pcn_InCSR(pcnp, CSR89)<<16);
	chipid &= 0xfffffff; /* Get rid of silicon revision */
	switch (chipid) {
	case 0x2623003:
		pcnp->devtype = pcn_fast;
		/*
		 * Play with CSR3 so the transmitter doesn't stop when
		 * a transmit underflow occurs (we'll simply drop a
		 * packet and restart anyway)
		 */
		csr = pcn_InCSR(pcnp, CSR3) | CSR3_DXSUFLO;
		pcn_OutCSR(pcnp, CSR3, csr & CSR3_WRITEMASK);
		break;

	case 0x2621003:
		pcnp->devtype = pcn_pciII;
		break;

	case 0x2430003:
		pcnp->devtype = pcn_pci;
		break;

	case 0x2260003:
	case 0x2261003:
		pcnp->devtype = pcn_isaplus;
		break;

	default:
		/*
		 * Old ISA chip didn't have a CSR89, so we don't know
		 * how it behaves
		 */
		if ((chipid & 0xffff) == 0x3003) {
		    pcnp->devtype = pcn_isa;
		} else {
		    pcnp->devtype = pcn_unknown;
		}
		break;
	}

#ifdef PCNDEBUG
	if (pcndebug)
		cmn_err(CE_CONT, "!pcn: Device type:%d id:%x",
				pcnp->devtype, chipid);
#endif

	/* Set PCI bus features as requested */
	if (pcnp->devtype == pcn_pciII ||
		pcnp->devtype == pcn_pci ||
		pcnp->devtype == pcn_fast) {
		ushort_t burst = 0, old, csr, xmitstart;

		/* Recommended settings from PCnet-PCI manual */
		int linbc = ddi_getprop
				(DDI_DEV_T_ANY, pcnp->devinfo, 0, "linbc", 2);

		/*
		 * Burst mode NOT enabled by default. Seems to crash some
		 * machines
		 */

		if (ddi_getprop
			(DDI_DEV_T_ANY, pcnp->devinfo, 0, "burst-read", 0))
			burst |= BCR_BSBC_BREADE;

		if (ddi_getprop
			(DDI_DEV_T_ANY, pcnp->devinfo, 0, "burst-write", 0))
			burst |= BCR_BSBC_BWRITE;

		if (pcnp->devtype == pcn_fast)
			burst |= (BCR_BSBC_BREADE | BCR_BSBC_BWRITE);

		old = pcn_InBCR(pcnp, BCR_BSBC)
			& ~(BCR_BSBC_BWRITE|BCR_BSBC_BREADE|3);

		pcn_OutBCR(pcnp, BCR_BSBC, old|burst|linbc);

		xmitstart = ddi_getprop(DDI_DEV_T_ANY, pcnp->devinfo, 0,
					"transmit-start", 2);

		csr = pcn_InCSR(pcnp, CSR80) & (~3<<10);
		csr |= xmitstart << 10;
		pcn_OutCSR(pcnp, CSR80, csr);

		csr = pcn_InCSR(pcnp, CSR4);
		if (ddi_getprop(DDI_DEV_T_ANY, pcnp->devinfo, 0, "dmaplus", 1))
			csr |= CSR4_DMAPLUS;
		else
			csr &= ~CSR4_DMAPLUS;

		pcn_OutCSR(pcnp, CSR4, csr);

	}
	pcn_process_phy(macinfo);
}
