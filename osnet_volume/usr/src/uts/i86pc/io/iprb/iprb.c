/*
 * Copyright (c) 1995 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * iprb -- Intel Pro100/B (82557/82558) Fast Ethernet Driver Depends on the
 * Generic LAN Driver utility functions in /kernel/misc/gld
 */

#pragma ident	"@(#)iprb.c	1.33	99/07/22 SMI"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/devops.h>
#include <sys/sunddi.h>
#ifndef	SPARC
#include <sys/eisarom.h>
#endif
#include <sys/ksynch.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/strsun.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/pci.h>
#include <sys/gld.h>
#include <sys/mii.h>


#include "iprb.h"
#include "rcvbundl.h"

/*
 * Declarations and Module Linkage
 */

static	char	ident[] = "Intel 82558/82559 Ver 0.010.1 Driver";

#ifdef IPRBDEBUG
/* used for debugging */
int		iprbdebug = 0;
#endif

/* Required system entry points */
static		iprbidentify(dev_info_t *);
static		iprbprobe(dev_info_t *);
static		iprbattach(dev_info_t *, ddi_attach_cmd_t);
static		iprbdetach(dev_info_t *, ddi_detach_cmd_t);

/* Required driver entry points for GLD */
int		iprb_reset(gld_mac_info_t *);
int		iprb_start_board(gld_mac_info_t *);
int		iprb_stop_board(gld_mac_info_t *);
int		iprb_set_mac_addr(gld_mac_info_t *, unsigned char *);
int		iprb_set_multicast(gld_mac_info_t *, unsigned char *, int);
int		iprb_set_promiscuous(gld_mac_info_t *, int);
int		iprb_get_stats(gld_mac_info_t *, struct gld_stats *);
int		iprb_send(gld_mac_info_t *, mblk_t *);
uint_t		iprb_intr(gld_mac_info_t *);

/* Functions to handle board command queue */
static	void	iprb_configure(gld_mac_info_t *);
static	void	iprb_init_board(gld_mac_info_t *);
static	void	iprb_add_command(gld_mac_info_t *);
static	void	iprb_reap_commands(gld_mac_info_t *macinfo, int reap_mode);
static	int	iprb_load_microcode(gld_mac_info_t *macinfo,
					uchar_t revision_id);
static	void	iprb_set_autopolarity(struct iprbinstance *iprbp);
static	ushort_t	iprb_diag_test(gld_mac_info_t *macinfo);
static	int	iprb_self_test(gld_mac_info_t *macinfo);
static	void	iprb_force_speed_and_duplex(struct iprbinstance *iprbp);
static	void	iprb_getprop(struct iprbinstance *iprbp);

/* Functions to R/W to PROM */
static	void
iprb_readia(struct iprbinstance *iprbp, uint16_t *addr,
		uint8_t offset);
static	void
iprb_shiftout(struct iprbinstance *iprbp, uint16_t data,
		uint8_t count);
static	void	iprb_raiseclock(struct iprbinstance *iprbp, uint8_t *eex);
static	void	iprb_lowerclock(struct iprbinstance *iprbp, uint8_t *eex);
static	uint16_t	iprb_shiftin(struct iprbinstance *iprbp);
static	void	iprb_eeclean(struct iprbinstance *iprbp);

/* Functions to handle DMA memory resources */
static	int	iprb_alloc_dma_resources(struct iprbinstance *iprbp);
static	void	iprb_free_dma_resources(struct iprbinstance *iprbp);
#ifdef	DMA_MEMORY_SCARCE
static	int
iprb_dma_mem_alloc(ddi_dma_handle_t, uint_t, ddi_device_acc_attr_t *,
			ulong_t, int (*) (caddr_t), caddr_t, caddr_t *,
			uint_t *, ddi_acc_handle_t *);
	static	void	iprb_dma_mem_free(ddi_acc_handle_t *);
#endif				/* DMA_MEMORY_SCARCE */

/* Functions to handle Rx operation and 'Free List' manipulation */
	static	void	iprb_process_recv(gld_mac_info_t *macinfo);
	static	struct	iprb_rfd_info *iprb_get_buffer(
			struct iprbinstance *iprbp);
	static	struct	iprb_rfd_info *iprb_alloc_buffer(
			struct iprbinstance *iprbp);
	static	void	iprb_remove_buffer(struct iprbinstance *iprbp);
	static	void	iprb_rcv_complete(struct iprb_rfd_info *);
	static	void	iprb_release_mblks(struct iprbinstance *);

/* Misc. functions */
	static	void	iprb_RU_wdog(void *arg);
	void	iprb_hard_reset(gld_mac_info_t *);
	void	iprb_get_ethaddr(gld_mac_info_t *);
	void	iprb_phy_errata(struct iprbinstance *iprbp);
	int	iprb_prepare_xmit_buff(gld_mac_info_t *, mblk_t *, size_t);
	int	iprb_prepare_ext_xmit_buff(gld_mac_info_t *, mblk_t *, size_t);
	ushort_t	iprb_get_phyid(struct iprbinstance *, int);

/* VA edited on 2/6/99 */
	static	ushort_t iprb_get_eeprom_size(struct iprbinstance *iprbp);

#ifdef IPRBDEBUG
	void	iprb_print_board_state(gld_mac_info_t *macinfo);
#endif

/* MII interface functions */
	static	int	get_active_mii(gld_mac_info_t *macinfo);
	static	void	iprb_phyinit(gld_mac_info_t *macinfo);
	static	ushort_t	iprb_mii_readreg(dev_info_t *dip, int phy_addr,
						int reg_addr);
	static	void	iprb_mii_writereg(dev_info_t *dip, int phy_addr,
						int reg_addr, int data);
	static	void	iprb_mii_linknotify_cb(dev_info_t *dip, int,
						enum mii_phy_state);

#ifdef	MII_IS_MODULE
#define	MII_DEPEND " misc/mii"
#else
#define	MII_DEPEND
#endif

	char	_depends_on[] = "misc/gld" MII_DEPEND;

/* DMA attributes for a control command block */
	static ddi_dma_attr_t control_cmd_dma_attr = {
		DMA_ATTR_V0,	/* version of this structure */
		0,		/* lowest usable address */
		0xffffffffU,	/* highest usable address */
		0x7fffffff,	/* maximum DMAable byte count */
		4,		/* alignment in bytes */
		0x100,		/* burst sizes (any?) */
		1,		/* minimum transfer */
		0xffffffffU,	/* maximum transfer */
		0xffffffffU,	/* maximum segment length */
		1,		/* maximum number of segments */
		1,		/* granularity */
		0,		/* flags (reserved) */
	};

/*
 * for Dynamic TBDs, TBD buffer must be DWORD aligned. This is due to the
 * possibility of interruptions to the PCI bus operations (e.g. disconnect)
 * that may result in the device reading only half of the TX buffer pointer.
 * If this half was zero and the pointer is then updated by the driver before
 * the latter word is read by the device, device may use the invalid pointer.
 * VA edited on APR 8, 99
 */

/* DMA attributes for a transmit buffer descriptor array */
	static ddi_dma_attr_t tx_buffer_desc_dma_attr = {
		DMA_ATTR_V0,	/* version of this structure */
		0,		/* lowest usable address */
		0xffffffffU,	/* highest usable address */
		0x7fffffff,	/* maximum DMAable byte count */
		4,		/* alignment in bytes */
		0x100,		/* burst sizes (any?) */
		1,		/* minimum transfer */
		0xffffffffU,	/* maximum transfer */
		0xffffffffU,	/* maximum segment length */
		1,		/* maximum number of segments */
		1,		/* granularity */
		0,		/* flags (reserved) */
	};

/* DMA attributes for a transmit buffer */
	static ddi_dma_attr_t tx_buffer_dma_attr = {
		DMA_ATTR_V0,	/* version of this structure */
		0,		/* lowest usable address */
		0xffffffffU,	/* highest usable address */
		0x7fffffff,	/* maximum DMAable byte count */
		1,		/* alignment in bytes */
		0x100,		/* burst sizes (any?) */
		1,		/* minimum transfer */
		0xffffffffU,	/* maximum transfer */
		0xffffffffU,	/* maximum segment length */
		2,		/* maximum number of segments */
		1,		/* granularity */
		0,		/* flags (reserved) */
	};


/* DMA attributes for a receive frame descriptor */
	static ddi_dma_attr_t rfd_dma_attr = {
		DMA_ATTR_V0,	/* version of this structure */
		0,		/* lowest usable address */
		0xffffffffU,	/* highest usable address */
		0x7fffffff,	/* maximum DMAable byte count */
		4,		/* alignment in bytes */
		0x100,		/* burst sizes (any?) */
		1,		/* minimum transfer */
		0xffffffffU,	/* maximum transfer */
		0xffffffffU,	/* maximum segment length */
		1,		/* maximum number of segments */
		1,		/* granularity */
		0,		/* flags (reserved) */
	};

/* DMA attributes for a statistics buffer */
	static ddi_dma_attr_t stats_buffer_dma_attr = {
		DMA_ATTR_V0,	/* version of this structure */
		0,		/* lowest usable address */
		0xffffffffU,	/* highest usable address */
		0x7fffffff,	/* maximum DMAable byte count */
		16,		/* alignment in bytes */
		0x100,		/* burst sizes (any?) */
		1,		/* minimum transfer */
		0xffffffffU,	/* maximum transfer */
		0xffffffffU,	/* maximum segment length */
		1,		/* maximum number of segments */
		1,		/* granularity */
		0,		/* flags (reserved) */
	};
/* DMA attributes for a self test buffer */
	static ddi_dma_attr_t selftest_buffer_dma_attr = {
		DMA_ATTR_V0,	/* version of this structure */
		0,		/* lowest usable address */
		0xffffffffU,	/* highest usable address */
		0x7fffffff,	/* maximum DMAable byte count */
		16,		/* alignment in bytes */
		0x100,		/* burst sizes (any?) */
		1,		/* minimum transfer */
		0xffffffffU,	/* maximum transfer */
		0xffffffffU,	/* maximum segment length */
		1,		/* maximum number of segments */
		1,		/* granularity */
		0,		/* flags (reserved) */
	};

/* DMA access attributes  <Little Endian Card> */
	static ddi_device_acc_attr_t accattr = {
		DDI_DEVICE_ATTR_V0,
		DDI_STRUCTURE_LE_ACC,
		DDI_STRICTORDER_ACC,
	};

unsigned	char	iprb_broadcastaddr[] =
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* Standard Streams initialization */

	static struct module_info minfo = {
		IPRBIDNUM, "iprb", 0, INFPSZ, IPRBHIWAT, IPRBLOWAT
	};

	static struct qinit rinit = {	/* read queues */
		NULL, gld_rsrv, gld_open, gld_close, NULL, &minfo, NULL
	};

	static struct qinit winit = {	/* write queues */
		gld_wput, gld_wsrv, NULL, NULL, NULL, &minfo, NULL
	};

	struct streamtab iprbinfo = {&rinit, &winit, NULL, NULL};

/* Standard Module linkage initialization for a Streams driver */

	extern struct mod_ops mod_driverops;

	static struct cb_ops cb_iprbops = {
		nulldev,	/* cb_open */
		nulldev,	/* cb_close */
		nodev,		/* cb_strategy */
		nodev,		/* cb_print */
		nodev,		/* cb_dump */
		nodev,		/* cb_read */
		nodev,		/* cb_write */
		nodev,		/* cb_ioctl */
		nodev,		/* cb_devmap */
		nodev,		/* cb_mmap */
		nodev,		/* cb_segmap */
		nochpoll,	/* cb_chpoll */
		ddi_prop_op,	/* cb_prop_op */
		&iprbinfo,	/* cb_stream */
		(int)(D_MP)	/* cb_flag */
	};

	struct dev_ops  iprbops = {
		DEVO_REV,	/* devo_rev */
		0,		/* devo_refcnt */
		gld_getinfo,	/* devo_getinfo */
		iprbidentify,	/* devo_identify */
		iprbprobe,	/* devo_probe */
		iprbattach,	/* devo_attach */
		iprbdetach,	/* devo_detach */
		nodev,		/* devo_reset */
		&cb_iprbops,	/* devo_cb_ops */
		(struct bus_ops *)NULL	/* devo_bus_ops */
	};

	static struct modldrv modldrv = {
		&mod_driverops,	/* Type of module.  This one is a driver */
		ident,		/* short description */
		&iprbops	/* driver specific ops */
	};

	static struct modlinkage modlinkage = {
		MODREV_1, (void *) &modldrv, NULL
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
 * DDI Entry Points
 */

/* identify(9E) -- See if we know about this device */
/* ARGSUSED */
iprbidentify(dev_info_t *devinfo)
{
	return (DDI_IDENTIFIED);
}

iprbprobe(dev_info_t *devinfo)
{
	ddi_acc_handle_t	handle;
	uint8_t			iline;
	uint16_t		cmdreg;
	unsigned char		iprb_revision_id;
	ushort_t		iprb_mwi_enable;

	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (DDI_PROBE_FAILURE);

	cmdreg = pci_config_get16(handle, PCI_CONF_COMM);
	iline = pci_config_get8(handle, PCI_CONF_ILINE);
	iprb_revision_id = pci_config_get8(handle, PCI_CONF_REVID);
#if defined(i86pc)
	if ((iline == 0) || (iline > 15)) {
		cmn_err(CE_WARN, "iprb: iline value out of range: %d", iline);
		pci_config_teardown(&handle);
		return (DDI_PROBE_FAILURE);
	}
#endif
	/* This code is needed to workaround a bug in the framework */
	iprb_mwi_enable = ddi_getprop(DDI_DEV_T_NONE, devinfo,
					DDI_PROP_DONTPASS, "MWIEnable",
					IPRB_DEFAULT_MWI_ENABLE);
	/*
	 *MWI should only be enabled for D101A4 and above i.e. 82558 and
	 *above
	 */
	if (iprb_mwi_enable == 1 && iprb_revision_id >= D101A4_REV_ID)
		cmdreg |= PCI_COMM_MAE | PCI_COMM_ME |
				PCI_COMM_MEMWR_INVAL | PCI_COMM_IO;
	else
		cmdreg |= PCI_COMM_MAE | PCI_COMM_ME | PCI_COMM_IO;

	/* Write it back to pci config register */
	pci_config_put16(handle, PCI_CONF_COMM, cmdreg);

	pci_config_teardown(&handle);
	return (DDI_PROBE_SUCCESS);
}

/*
 * attach(9E) -- Attach a device to the system
 *
 * Called once for each board successfully probed.
 */
iprbattach(dev_info_t *devinfo, ddi_attach_cmd_t cmd)
{
	gld_mac_info_t *macinfo;	/* GLD structure */
	struct iprbinstance *iprbp;	/* Our private device info */
	uint16_t	i;
	ddi_acc_handle_t handle;

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBDDI)
		cmn_err(CE_CONT, "iprbattach(0x%x)", (int)devinfo);
#endif
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	/*
	 *Allocate gld_mac_info_t and iprbinstance structures
	 */
	if ((macinfo = gld_mac_alloc(devinfo)) == NULL)
		return (DDI_FAILURE);

	if ((iprbp = (struct iprbinstance *)kmem_zalloc(
		sizeof (struct iprbinstance), KM_SLEEP)) == NULL) {
		gld_mac_free(macinfo);
		return (DDI_FAILURE);
	}

	/*
	 *This is to access pci config space as probe and attach may be
	 *called by different threads for different instances of the NICs VA
	 *edited on 20 Jun, 1999
	 */
	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/* Initialize our private fields in macinfo and iprbinstance */
	macinfo->gldm_devinfo = iprbp->iprb_dip = devinfo;
	macinfo->gldm_private = (caddr_t)iprbp;
	/* Now we need to extract revision id from pci config space... */
	iprbp->iprb_revision_id = pci_config_get8(handle, PCI_CONF_REVID);
	iprbp->phy_id = 0;
	/* No access to pci config space beyond this point */
	pci_config_teardown(&handle);
	/* get all the parameters */
	iprb_getprop(iprbp);

	/*
	 *Initialize pointers to device specific functions which will be
	 *used by the generic layer.
	 */
	macinfo->gldm_reset = iprb_reset;
	macinfo->gldm_start = iprb_start_board;
	macinfo->gldm_stop = iprb_stop_board;
	macinfo->gldm_set_mac_addr = iprb_set_mac_addr;
	macinfo->gldm_set_multicast = iprb_set_multicast;
	macinfo->gldm_set_promiscuous = iprb_set_promiscuous;
	macinfo->gldm_get_stats = iprb_get_stats;
	macinfo->gldm_send = iprb_send;
	macinfo->gldm_intr = iprb_intr;
	macinfo->gldm_ioctl = NULL;

	/*
	 *Initialize board characteristics needed by the generic layer.
	 */
	macinfo->gldm_ident = ident;
	macinfo->gldm_type = DL_ETHER;
	macinfo->gldm_minpkt = 0;	/* assumes we pad ourselves */
	macinfo->gldm_maxpkt = IPRBMAXPKT;
	macinfo->gldm_addrlen = ETHERADDRL;
	macinfo->gldm_saplen = -2;
	macinfo->gldm_ppa = ddi_get_instance(devinfo);

	if (ddi_regs_map_setup(devinfo, 1, (caddr_t *)&iprbp->port, 0, 0,
				&accattr, &iprbp->iohandle) != DDI_SUCCESS) {
		kmem_free(iprbp, sizeof (struct iprbinstance));
		gld_mac_free(macinfo);
		return (DDI_FAILURE);
	}
	/* Self Test */
	if ((ushort_t)iprbp->do_self_test == 1) {
		if (iprb_self_test(macinfo) != GLD_SUCCESS) {
			kmem_free(iprbp, sizeof (struct iprbinstance));
			gld_mac_free(macinfo);
			return (DDI_FAILURE);
		}
	}
	/* Reset the hardware */
	(void) iprb_hard_reset(macinfo);

	/* Disable Interrupts */
	DisableInterrupts(iprbp);

	/* VA edited on 2/8/99 */
	/* Get the board's eeprom size */
	(void) iprb_get_eeprom_size(iprbp);
	/* VA edited on 2/10/99 */
	/* Get the board's Sub vendor Id and device id */
	iprb_readia(iprbp, &(iprbp->iprb_sub_system_id), IPRB_SUB_SYSTEM_ID);
	iprb_readia(iprbp, &(iprbp->iprb_sub_vendor_id), IPRB_SUB_VENDOR_ID);

	/* Get the board's vendor-assigned hardware network address */
	(void) iprb_get_ethaddr(macinfo);
	macinfo->gldm_broadcast_addr = iprb_broadcastaddr;
	macinfo->gldm_vendor_addr = iprbp->macaddr;

	/* Get the board's Compatabilty information from the EEPROM image */
	iprb_readia(iprbp, &i, IPRB_EEPROM_COMP_WORD);
	/* RU lockup affects this card ? */
	if ((i & 0x03) != 3)
		iprbp->RU_needed = 1;

	/*
	 *Do anything necessary to prepare the board for operation short of
	 *actually starting the board.
	 */
	if (ddi_get_iblock_cookie(devinfo,
		0, &macinfo->gldm_cookie) != DDI_SUCCESS)
		goto afail;

	mutex_init(&iprbp->freelist_mutex,
		NULL, MUTEX_DRIVER, macinfo->gldm_cookie);
	mutex_init(&iprbp->cmdlock,
		NULL, MUTEX_DRIVER, macinfo->gldm_cookie);
	mutex_init(&iprbp->intrlock,
		NULL, MUTEX_DRIVER, macinfo->gldm_cookie);

	if (iprb_alloc_dma_resources(iprbp) == DDI_FAILURE)
		goto late_afail;

	/*
	 *This will prevent receiving interrupts before device is ready, as
	 *we are initializing device after setting the interrupts. So we
	 *will not get our interrupt handler invoked by OS while our device
	 *is still coming up or timer routines will not start till we are
	 *all set to process... VA May 19, 1999
	 */

	/* Add the interrupt handler */
	(void) ddi_add_intr(devinfo, 0, NULL, NULL, gld_intr, (caddr_t)macinfo);

	/* We need to do this for the MII code */
	ddi_set_driver_private(devinfo, (caddr_t)macinfo);
	if (iprbp->max_rxbcopy > ETHERMAX)
		iprbp->max_rxbcopy = ETHERMAX;
	if (iprbp->iprb_threshold > IPRB_MAX_THRESHOLD)
		iprbp->iprb_threshold = IPRB_MAX_THRESHOLD;
	if (iprbp->iprb_threshold < 0)
		iprbp->iprb_threshold = 0;

	/*
	 *Register ourselves with the GLD interface
	 *
	 *gld_register will: link us with the GLD system; set our
	 *ddi_set_driver_private(9F) data to the macinfo ptr; create the
	 *minor node.
	 */
	if (gld_register(devinfo, "iprb", macinfo) != DDI_SUCCESS) {
		ddi_remove_intr(devinfo, 0, macinfo->gldm_cookie);
		goto late_afail;
	}
	/*
	 *As described above, we will set the board up only when everything
	 *else is properly configured and registered. If I don't do this and
	 *accept CNA interrupts, I will run into potential hang situation,
	 *as my interrupt handler is ready to handle the interrupts but
	 *gld_register is not done, so GLD has no information about how to
	 *deal with it..., by moving these 2 calls here, this condition can
	 *be avoided... VA edited on May 19,1999
	 */
	iprb_init_board(macinfo);

	/* Configure physical layer */
	iprb_phyinit(macinfo);
	/* Do a Diag Test */
	if ((ushort_t)iprbp->do_diag_test == 1)
		if (iprb_diag_test(macinfo) != GLD_SUCCESS) {
			ddi_remove_intr(devinfo, 0, macinfo->gldm_cookie);
			goto late_afail;
		}
	/* Set auto polarity */
	iprb_set_autopolarity(iprbp);
	/* Change for 82558 enhancement */
	/*
	 *If D101 and if the user has enabled flow control, set up the Flow
	 *Control Reg. in the CSR VA edited on 20 Jun, Sunday, 1999
	 */
	if ((iprbp->iprb_revision_id >= D101A4_REV_ID) &&
		(iprbp->flow_control == 1)) {
		ddi_io_put8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_FC_THLD_REG),
			IPRB_DEFAULT_FC_THLD);
		ddi_io_put8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_FC_CMD_REG),
			IPRB_DEFAULT_FC_CMD);
	}
	EnableInterrupts(iprbp);
	return (DDI_SUCCESS);

late_afail:
	if (iprbp->mii)
		mii_destroy(iprbp->mii);
	iprb_free_dma_resources(iprbp);

	mutex_destroy(&iprbp->freelist_mutex);
	mutex_destroy(&iprbp->cmdlock);
	mutex_destroy(&iprbp->intrlock);

afail:
	ddi_regs_map_free(&iprbp->iohandle);
	kmem_free(iprbp, sizeof (struct iprbinstance));
	gld_mac_free(macinfo);
	return (DDI_FAILURE);
}

void
iprb_hard_reset(gld_mac_info_t *macinfo)
{
	struct	iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	int	i = 10;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_hard_reset...");
#endif
	/* As per 82557/8 spec. , issue selective reset first */
	ddi_io_put32(iprbp->iohandle,
		REG32(iprbp->port, IPRB_SCB_PORT), IPRB_PORT_SEL_RESET);
	/* Wait for PORT register to clear */
	do {
		drv_usecwait(10);
		if (ddi_io_get32(iprbp->iohandle,
			REG32(iprbp->port, IPRB_SCB_PORT)) == 0)
			break;
	} while (--i > 0);
#ifdef IPRBDEBUG
	if (i == 0)
		cmn_err(CE_WARN, "iprb : Selective reset failed");
#endif
	/* Issue software reset */
	ddi_io_put32(iprbp->iohandle,
		REG32(iprbp->port, IPRB_SCB_PORT), IPRB_PORT_SW_RESET);
	drv_usecwait(10);	/* As per specs - RSS */
}

/* detach(9E) -- Detach a device from the system */

iprbdetach(dev_info_t *devinfo, ddi_detach_cmd_t cmd)
{
	gld_mac_info_t *macinfo;	/* GLD structure */
	struct	iprbinstance *iprbp;	/* Our private device info */
	int	i;
#define	WAITTIME 10000			/* usecs to wait */
#define	MAX_TIMEOUT_RETRY_CNT	(30 *(1000000 / WAITTIME))	/* 30 seconds */

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBDDI)
		cmn_err(CE_CONT, "iprbdetach(0x%x)\n", (int)devinfo);
#endif

	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}
	/* Get the driver private (gld_mac_info_t) structure */
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(devinfo);
	iprbp = (struct iprbinstance *)(macinfo->gldm_private);

	/*
	 *We call stop_board before gld_unregister as the 82258 card will
	 *issue an RNR interrupt when we issue the RU_ABORT. However since
	 *gld resets its macinfo interrupt flag, when gld_intr is called it
	 *returns without claiming the interrupt for the 82258 card we're
	 *detaching from.
	 */
	/* Stop the receiver */
	(void) iprb_stop_board(macinfo);

	mutex_enter(&iprbp->cmdlock);
	/* Stop the board if it is running */
	iprb_hard_reset(macinfo);
	mutex_exit(&iprbp->cmdlock);

	/* Wait for all receive buffers to be returned */
	i = MAX_TIMEOUT_RETRY_CNT;

	mutex_enter(&iprbp->freelist_mutex);
	while (iprbp->rfds_outstanding > 0) {
		mutex_exit(&iprbp->freelist_mutex);
		delay(drv_usectohz(WAITTIME));

		if (--i == 0) {
			cmn_err(CE_WARN, "iprb: never reclaimed all the "
				"receive buffers.  Still have %d "
				"buffers outstanding.",
				iprbp->rfds_outstanding);
			return (DDI_FAILURE);
		}
		mutex_enter(&iprbp->freelist_mutex);
	}

	/*
	 *Unregister ourselves from the GLD interface
	 *
	 *gld_unregister will: remove the minor node; unlink us from the GLD
	 *module.
	 */
	if (gld_unregister(macinfo) != DDI_SUCCESS)
		return (DDI_FAILURE);

	mutex_exit(&iprbp->freelist_mutex);

	ddi_remove_intr(devinfo, 0, macinfo->gldm_cookie);

	/* Release any pending xmit mblks */
	iprb_release_mblks(iprbp);

	/* Free up MII resources */
	if (iprbp->mii)
		mii_destroy(iprbp->mii);

	/* Release all DMA resources */
	iprb_free_dma_resources(iprbp);

	mutex_destroy(&iprbp->freelist_mutex);
	mutex_destroy(&iprbp->cmdlock);
	mutex_destroy(&iprbp->intrlock);

	/* Unmap our register set */
	ddi_regs_map_free(&iprbp->iohandle);

	kmem_free(iprbp, sizeof (struct iprbinstance));
	gld_mac_free(macinfo);

	return (DDI_SUCCESS);
}

/*
 * GLD Entry Points
 */

/*
 * iprb_reset() -- reset the board to initial state; restore the machine
 * address afterwards.
 */

int
iprb_reset(gld_mac_info_t *macinfo)
{
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_CONT, "iprb_reset(0x%x)", (int)macinfo);
#endif

	return (iprb_stop_board(macinfo));
}

/*
 * iprb_init_board() -- initialize the specified network board.
 */
static void
iprb_init_board(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_init_board...");
#endif
	mutex_enter(&iprbp->intrlock);
	mutex_enter(&iprbp->cmdlock);

	iprbp->iprb_first_rfd = 0;	/* First RFD index in list */
	iprbp->iprb_last_rfd = iprbp->iprb_nrecvs - 1;	/* Last RFD index */
	iprbp->iprb_current_rfd = 0;	/* Next RFD index to be filled */

	/*
	 *iprb_first_cmd is the first command used but not yet processed by
	 *the D100.  -1 means we don't have any commands pending.
	 *
	 *iprb_last_cmd is the last command used but not yet processed by the
	 *D100.
	 *
	 *iprb_current_cmd is the one we can next use.
	 */
	iprbp->iprb_first_cmd = -1;
	iprbp->iprb_last_cmd = 0;
	iprbp->iprb_current_cmd = 0;

	/* Note that the next command will be the first command */
	iprbp->iprb_initial_cmd = 1;
	/* Set general pointer to 0 */
	/* LANCEWOOD Fix */
	/* Could not Reclaim all command buffers msg fix... */
	/*
	 *Always reinitialize gen ptr. and load CUbase and RUbase VA edited
	 *on SUNDAY JUN 20, 1999
	 */
	ddi_io_put32(iprbp->iohandle,
			REG32(iprbp->port, IPRB_SCB_PTR),
			(uint32_t)0);
	ddi_io_put8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_CMD), IPRB_LOAD_CUBASE);
	IPRB_SCBWAIT(iprbp);
	ddi_io_put32(iprbp->iohandle,
			REG32(iprbp->port, IPRB_SCB_PTR),
			(uint32_t)0);
	ddi_io_put8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_CMD), IPRB_LOAD_RUBASE);
	IPRB_SCBWAIT(iprbp);
	/* End of LANCEWOOD Fix */
	mutex_exit(&iprbp->cmdlock);
	mutex_exit(&iprbp->intrlock);
}

/*
 * Read the EEPROM (one bit at a time!!!) to get the ethernet address
 */

void
iprb_get_ethaddr(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	register int    i;

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_get_ethaddr...");
#endif
	for (i = 0; i < (ETHERADDRL / sizeof (uint16_t)); i++) {
		iprb_readia(iprbp, (uint16_t *)
			    & iprbp->macaddr[i *sizeof (uint16_t)], i);
	}
}

/*
 * iprb_set_mac_addr() -- set the physical network address on the board
 */

int
iprb_set_mac_addr(gld_mac_info_t *macinfo, unsigned char *macaddr)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;

	struct iprb_ias_cmd *ias;
	int	i;

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_CONT, "iprb_set_mac_addr(0x%x)", (int)macinfo);
#endif

	mutex_enter(&iprbp->cmdlock);

	/* Make available any command buffers already processed */
	iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);

	/* Any command buffers left? */
	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
		mutex_exit(&iprbp->cmdlock);
		return (GLD_NORESOURCES);
	}
	/*
	 *Make an individual address setup command
	 */

	ias = &iprbp->cmd_blk[iprbp->iprb_current_cmd]->ias_cmd;
	ias->ias_cmd = IPRB_IAS_CMD;

	/* Get the ethernet address from the macaddr */
	for (i = 0; i < ETHERADDRL; i++)
		ias->addr[i] = macaddr[i];

	iprb_add_command(macinfo);
	IPRB_SCBWAIT(iprbp);
	mutex_exit(&iprbp->cmdlock);
	return (GLD_SUCCESS);
}


/*
 * Set up the configuration of the D100.  If promiscuous mode is to be on, we
 * change certain things.
 */
static
void
iprb_configure(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;

	struct iprb_cfg_cmd *cfg;
	int	phy = 0;	/* Will let us know about MII Phy0 */
				/* or Phy1 and will be -1 in case  */
				/* of 503 */
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_configure...");
#endif
	/* XXX Handle Promisc multicast etc */
	iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);

	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
		cmn_err(CE_WARN, "IPRB: config failed, out of resources.");
		return;
	}
	cfg = &iprbp->cmd_blk[iprbp->iprb_current_cmd]->cfg_cmd;
	cfg->cfg_cmd = IPRB_CFG_CMD;
	cfg->cfg_byte0 = IPRB_CFG_B0;
	cfg->cfg_byte1 = IPRB_CFG_B1;

	if (iprbp->Aifs >= 0) {
		cfg->cfg_byte2 = (unsigned char) iprbp->Aifs;
	} else
		cfg->cfg_byte2 = IPRB_CFG_B2;

	cfg->cfg_byte3 = IPRB_CFG_B3;

	/*
	 *MWI should only be enabled for D101A4 and above i.e. 82558 and
	 *above
	 */
	/* Read Align should only be available for 82558 and above */
	/*
	 *MWIEnable is already read in iprbprobe routine for a completly
	 *different purpose, here it is being reread only because I don't
	 *want to make use of probe routine values as probe and attach and
	 *configure can be called in any order by different threads
	 */
	if (iprbp->iprb_revision_id >= D101A4_REV_ID) {
		if (ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
				DDI_PROP_DONTPASS,
				"MWIEnable",
				IPRB_DEFAULT_MWI_ENABLE) == 1)
			cfg->cfg_byte3 |= IPRB_CFG_MWI_ENABLE;
		if (iprbp->read_align == 1)
			cfg->cfg_byte3 |= IPRB_CFG_READAL_ENABLE |
						IPRB_CFG_TERMWCL_ENABLE;
	}
	cfg->cfg_byte4 = IPRB_CFG_B4;	/* Rx DMA Max. Byte Count */
	cfg->cfg_byte5 = IPRB_CFG_B5;	/* Tx DMA Max. Byte Count */
	if ((iprbp->promisc != GLD_MAC_PROMISC_NONE) &&
	    (iprbp->promisc != GLD_MAC_PROMISC_MULTI)) {
		cfg->cfg_byte6 = IPRB_CFG_B6 | IPRB_CFG_B6PROM;
		cfg->cfg_byte7 = IPRB_CFG_B7PROM;
	} else {
		cfg->cfg_byte6 = IPRB_CFG_B6;
		cfg->cfg_byte7 = IPRB_CFG_B7NOPROM;
	}

	/* Use Extended TCB if conf file directs so (only 82558 & 82559) */
	/*
	 *This bit is reserved in 82557 and should be set to 1 In 82558 and
	 *82559, if it set to 1, device will read standard 4 DWORDs
	 *TxCB(82557 compatible). When this bit is set to 0, the device will
	 *read 8 DWORDs for all CB's and process th TxCB as Extended
	 *TxCB(only for 82558/9). Here we are detecting the card and if
	 *appropriate, setting the bit to 0 (i.e. enable Extended TxCB VA
	 *edited on March 22, 99
	 */
	if ((iprbp->iprb_revision_id >= D101A4_REV_ID) &&
		(iprbp->extended_txcb == 0)) {
		cfg->cfg_byte6 &= ~IPRB_CFG_EXTXCB_DISABLE;	/* Enable it */
		iprbp->iprb_extended_txcb_enabled = IPRB_TRUE;
	} else
		iprbp->iprb_extended_txcb_enabled = IPRB_FALSE;
	/* Setup number of retries after under run */
	/*
	 *Bit 2-1 of Byte 7 represent no. of retries... VA edited on March
	 *22, 99
	 */
	cfg->cfg_byte7 = ((cfg->cfg_byte7 & (~IPRB_CFG_URUN_RETRY)) |
				((iprbp->tx_ur_retry) << 1));

	/* Dynamic TBD. Only available for 82558 and 82559 drivers */
	/*
	 *VA edited on March 22, 1999
	 */
	if ((iprbp->iprb_revision_id >= D101A4_REV_ID) &&
		(iprbp->enhanced_tx_enable == 1)) {
		cfg->cfg_byte7 |= IPRB_CFG_DYNTBD_ENABLE;
		iprbp->iprb_enhanced_tx_enable = IPRB_TRUE;
	} else
		iprbp->iprb_enhanced_tx_enable = IPRB_FALSE;

	phy = get_active_mii(macinfo);	/* Returns -1 in case of 503 */

	cfg->cfg_byte8 =
		IS_503(phy) ? IPRB_CFG_B8_503 : IPRB_CFG_B8_MII;

	cfg->cfg_byte9 = IPRB_CFG_B9;
	cfg->cfg_byte10 = IPRB_CFG_B10;
	cfg->cfg_byte11 = IPRB_CFG_B11;

	if (iprbp->ifs >= 0) {
		cfg->cfg_byte12 = (unsigned char) (iprbp->ifs << 4);
	} else
		cfg->cfg_byte12 = IPRB_CFG_B12;

	cfg->cfg_byte13 = IPRB_CFG_B13;
	cfg->cfg_byte14 = IPRB_CFG_B14;

	if ((iprbp->promisc != GLD_MAC_PROMISC_NONE) &&
	    (iprbp->promisc != GLD_MAC_PROMISC_MULTI))
		cfg->cfg_byte15 = IPRB_CFG_B15 | IPRB_CFG_B15_PROM;
	else
		cfg->cfg_byte15 = IPRB_CFG_B15;

	/* Broadcast Enable/Disable */
	if (iprbp->disable_broadcast == 1) {
		cfg->cfg_byte15 |= IPRB_CFG_BROADCAST_DISABLE;
	}
	/* WAW enable/disable  */
	if ((iprbp->iprb_revision_id >= D101A4_REV_ID) &&
		(iprbp->coll_backoff == 1)) {
		cfg->cfg_byte15 |= IPRB_CFG_WAW_ENABLE;
	}
	/*
	 *The CRS+CDT bit should only be set when NIC is operating in 503
	 *mode (Configuration Byte 8) Recommendation: This should be 1 for
	 *503 based 82557 designs 0 for MII based 82557 designs 0 for 82558
	 *or 82559 designs. VA edited on March 22, 99
	 */

	/* phy == -1 only in case of 503 , see get_active_mii */
	if (IS_503(phy)) {
		cfg->cfg_byte15 |= IPRB_CFG_CRS_OR_CDT;
	}
	/* Enabling Flow Control only if 82558 or 82559 */
	/*
	 *Flow control for PHY 1 (Please see the note below) VA edited on
	 *March 23, 1999
	 */
	if (iprbp->iprb_revision_id >= D101A4_REV_ID) {
	/*
	 *There is a fix added to disable flow control.
	 *VA edited on 07/02/1999
	 */
		if (iprbp->flow_control == 1) {
			cfg->cfg_byte16 = IPRB_CFG_FC_DELAY_LSB;
			cfg->cfg_byte17 = IPRB_CFG_FC_DELAY_MSB;
			cfg->cfg_byte18 = IPRB_CFG_B18;
			cfg->cfg_byte19 = (IPRB_CFG_B19 |
						IPRB_CFG_FC_RESTOP |
						IPRB_CFG_FC_RESTART |
						IPRB_CFG_REJECT_FC);
		}
		if (iprbp->flow_control == 0) {
			cfg->cfg_byte16 = IPRB_CFG_B16;
			cfg->cfg_byte17 = IPRB_CFG_B17;
			cfg->cfg_byte18 = IPRB_CFG_B18;
			cfg->cfg_byte19 = (IPRB_CFG_B19 |
						IPRB_CFG_TX_FC_DISABLE |
						IPRB_CFG_REJECT_FC);
		}
	} else {
		cfg->cfg_byte16 = IPRB_CFG_B16;
		cfg->cfg_byte17 = IPRB_CFG_B17;
		cfg->cfg_byte18 = IPRB_CFG_B18;
		if (iprbp->flow_control == 0) {
			cfg->cfg_byte19 = (IPRB_CFG_B19 |
						IPRB_CFG_TX_FC_DISABLE);
		}
		if (iprbp->flow_control == 1) {
			cfg->cfg_byte19 = IPRB_CFG_B19;
		}
	}

	/*
	 *We must force full duplex, if we are using PHY 0 and we are
	 *supposed to run in FDX mode. We will have to do it as IPRB has
	 *only one FDX# input pin. This pin is connected to PHY 1 Some
	 *testing is required to see the SPLASH full duplex operation. VA
	 *edited on March 23, 1999
	 */
	/*
	 *if (iprbp->full_duplex) cfg->cfg_byte19 |= IPRB_CFG_FORCE_FDX;
	 */

	cfg->cfg_byte20 = IPRB_CFG_B20;

	/* Enabling Multicast */
	if (iprbp->promisc == GLD_MAC_PROMISC_MULTI)
		cfg->cfg_byte21 = IPRB_CFG_B21 | IPRB_CFG_B21_MCPROM;
	else
		cfg->cfg_byte21 = IPRB_CFG_B21;
	cfg->cfg_byte22 = IPRB_CFG_B22;
	cfg->cfg_byte23 = IPRB_CFG_B23;

	iprb_add_command(macinfo);
	/* Set up the phy state and delay also */
	iprbp->phy_state = 0;
	iprbp->phy_delay = 0;
}

/*
 * iprb_set_multicast() -- set (enable) or disable a multicast address
 *
 * Program the hardware to enable/disable the multicast address in "mcast".
 * Enable if "op" is non-zero, disable if zero.
 */

/*
 * We keep a list of multicast addresses, because the D100 requires that the
 * entire list be uploaded every time a change is made.
 */

int
iprb_set_multicast(gld_mac_info_t *macinfo, unsigned char *mcast, int op)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;

	int	i, s;
	int	found = 0;
	int	free_index = -1;
	struct	iprb_mcs_cmd *mcmd;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_CONT, "iprb_set_multicast(0x%x, %s)", (int)macinfo,
			op == GLD_MULTI_ENABLE ? "ON" : "OFF");
#endif
	mutex_enter(&iprbp->cmdlock);

	iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);
	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
		mutex_exit(&iprbp->cmdlock);
		return (GLD_NORESOURCES);
	}
	for (s = 0; s < IPRB_MAXMCSN; s++) {
		if (iprbp->iprb_mcs_addrval[s] == 0) {
			if (free_index == -1)
				free_index = s;
		} else {
			if (bcmp((caddr_t)&(iprbp->iprb_mcs_addrs[s]),
				(caddr_t)mcast, ETHERADDRL) == 0) {
				found = 1;
				break;
			}
		}
	}

	if (op == GLD_MULTI_DISABLE) {
		/* We want to disable the multicast address. */
		if (found) {
			iprbp->iprb_mcs_addrval[s] = 0;
		} else {
			/* Trying to remove non-existant mcast addr */
			mutex_exit(&iprbp->cmdlock);
			return (GLD_SUCCESS);
		}
	} else if (op == GLD_MULTI_ENABLE) {
		/* Enable a mcast addr */
		if (!found) {
			if (free_index == -1) {
				mutex_exit(&iprbp->cmdlock);
				return (GLD_NORESOURCES);
			}
			bcopy((caddr_t)mcast,
				(caddr_t)&(iprbp->iprb_mcs_addrs[free_index]),
				ETHERADDRL);
			iprbp->iprb_mcs_addrval[free_index] = 1;
		} else {
			/* already enabled */
			mutex_exit(&iprbp->cmdlock);
			return (GLD_SUCCESS);
		}
	} else {
		mutex_exit(&iprbp->cmdlock);
		return (GLD_BADARG);
	}

	mcmd = &iprbp->cmd_blk[iprbp->iprb_current_cmd]->mcs_cmd;

	for (i = 0, s = 0; s < IPRB_MAXMCSN; s++) {
		if (iprbp->iprb_mcs_addrval[s] != 0) {
			bcopy((caddr_t)&iprbp->iprb_mcs_addrs[s],
				(caddr_t)&mcmd->mcs_bytes[i *ETHERADDRL],
				ETHERADDRL);
			i++;
		}
	}

	mcmd->mcs_cmd = IPRB_MCS_CMD;
	mcmd->mcs_count = i *ETHERADDRL;

	iprb_add_command(macinfo);

	mutex_exit(&iprbp->cmdlock);
	return (GLD_SUCCESS);
}

/*
 * iprb_set_promiscuous() -- set or reset promiscuous mode on the board
 *
 * Program the hardware to enable/disable promiscuous mode. Enable if "on" is
 * non-zero, disable if zero.
 */

int
iprb_set_promiscuous(gld_mac_info_t *macinfo, int level)
{
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_CONT, "iprb_set_promiscuous(0x%x, %x)",
			(int)macinfo, level);
#endif
	mutex_enter(&iprbp->cmdlock);
	/* Free up any processed command buffers */
	iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);
	/* If there are no control blocks available, return */
	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
		mutex_exit(&iprbp->cmdlock);
		return (GLD_NORESOURCES);
	}
	iprbp->promisc = level;
	iprb_configure(macinfo);
	mutex_exit(&iprbp->cmdlock);
	return (GLD_SUCCESS);

}

/*
 * iprb_get_stats() -- update statistics
 */
int
iprb_get_stats(gld_mac_info_t *macinfo, struct gld_stats *g_stats)
{
	dev_info_t	*devinfo;
	struct iprbinstance *iprbp;
	volatile struct iprb_stats *stats;
	ddi_dma_handle_t dma_handle_stats;
	ddi_acc_handle_t stats_dma_acchdl;
	ddi_dma_cookie_t dma_cookie;
	size_t	len;
	uint_t	ncookies;
	int	warning = 1000;

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_CONT, "iprb_get_stats(0x%x)", (int)macinfo);
#endif

	iprbp = (struct iprbinstance *)macinfo->gldm_private;
	devinfo = iprbp->iprb_dip;

	/* Allocate a DMA handle for the statistics buffer */
	if (ddi_dma_alloc_handle(devinfo, &stats_buffer_dma_attr,
		DDI_DMA_SLEEP, 0,
		&dma_handle_stats) != DDI_SUCCESS) {
		cmn_err(CE_WARN,
			"iprb: could not allocate statistics dma handle");
		return (GLD_NORESOURCES);
	}
	/* Now allocate memory for the statistics buffer */
	if (ddi_dma_mem_alloc(dma_handle_stats, sizeof (struct iprb_stats),
		&accattr, 0, DDI_DMA_SLEEP, 0, (caddr_t *)&stats, &len,
		&stats_dma_acchdl) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "iprb: could not allocate memory "
			"for statistics buffer");
		ddi_dma_free_handle(&dma_handle_stats);
		return (GLD_NORESOURCES);
	}
	bzero((caddr_t)stats, sizeof (struct iprb_stats));
	/* and finally get the DMA address associated with the buffer */
	if (ddi_dma_addr_bind_handle(dma_handle_stats, NULL, (caddr_t)stats,
		sizeof (struct iprb_stats), DDI_DMA_READ, DDI_DMA_SLEEP, 0,
		&dma_cookie, &ncookies) != DDI_DMA_MAPPED) {
		cmn_err(CE_WARN, "iprb: could not get dma address for "
			"statistics buffer");
		ddi_dma_mem_free(&stats_dma_acchdl);
		ddi_dma_free_handle(&dma_handle_stats);
		return (GLD_NORESOURCES);
	}
	ASSERT(ncookies == 1);

	mutex_enter(&iprbp->cmdlock);
	IPRB_SCBWAIT(iprbp);
	ddi_io_put32(iprbp->iohandle,
		REG32(iprbp->port, IPRB_SCB_PTR), dma_cookie.dmac_address);
	/*
	 *VVV ddi_io_put16(iprbp->iohandle, iprbp->port + IPRB_SCB_CMD,
	 *IPRB_CU_LOAD_DUMP_ADDR);
	 */
	ddi_io_put8(iprbp->iohandle,
		REG8(iprbp->port, IPRB_SCB_CMD), IPRB_CU_LOAD_DUMP_ADDR);

	IPRB_SCBWAIT(iprbp);
	/*
	 *VVV ddi_io_put16(iprbp->iohandle, iprbp->port + IPRB_SCB_CMD,
	 *IPRB_CU_DUMPSTAT);
	 */
	ddi_io_put8(iprbp->iohandle,
		REG8(iprbp->port, IPRB_SCB_CMD), IPRB_CU_DUMPSTAT);

	do {
		if (stats->iprb_stat_chkword == IPRB_STAT_COMPLETE)
			break;
		drv_usecwait(10);
	} while (--warning > 0);
	mutex_exit(&iprbp->cmdlock);

	(void) ddi_dma_unbind_handle(dma_handle_stats);

	if (warning == 0) {
		cmn_err(CE_WARN, "IPRB: Statistics generation failed.");
		ddi_dma_mem_free(&stats_dma_acchdl);
		ddi_dma_free_handle(&dma_handle_stats);
		return (GLD_FAILURE);
	}
	g_stats->glds_excoll = stats->iprb_stat_maxcol;
	g_stats->glds_xmtlatecoll = stats->iprb_stat_latecol;
	g_stats->glds_underflow = stats->iprb_stat_xunderrun;
	g_stats->glds_nocarrier = stats->iprb_stat_crs;
	g_stats->glds_defer = stats->iprb_stat_defer;
	g_stats->glds_dot3_first_coll = stats->iprb_stat_onecoll;
	g_stats->glds_dot3_multi_coll = stats->iprb_stat_multicoll;
	g_stats->glds_collisions = stats->iprb_stat_multicoll;
	g_stats->glds_crc = stats->iprb_stat_crc;
	g_stats->glds_frame = stats->iprb_stat_align;
	g_stats->glds_missed = stats->iprb_stat_resource;
	g_stats->glds_overflow = stats->iprb_stat_roverrun;
	g_stats->glds_short = stats->iprb_stat_short;

	/* Stats from instance structure, which we update */
	g_stats->glds_speed = iprbp->speed *1000000;
	g_stats->glds_media = GLDM_PHYMII;
	g_stats->glds_duplex =
		iprbp->full_duplex ? GLD_DUPLEX_FULL : GLD_DUPLEX_HALF;
	g_stats->glds_dot3_mac_rcv_error = iprbp->RU_stat_count;
	g_stats->glds_intr = iprbp->iprb_stat_intr;
	g_stats->glds_norcvbuf = iprbp->iprb_stat_norcvbuf;
	g_stats->glds_dot3_frame_too_long = iprbp->iprb_stat_frame_toolong;

	/* Total rcv/tx errors */
	g_stats->glds_errrcv = g_stats->glds_crc
		+ g_stats->glds_frame
		+ g_stats->glds_missed
		+ g_stats->glds_dot3_mac_rcv_error
		+ g_stats->glds_overflow
		+ g_stats->glds_short;
	g_stats->glds_errxmt = g_stats->glds_excoll
		+ g_stats->glds_xmtlatecoll
		+ g_stats->glds_underflow
		+ g_stats->glds_nocarrier;
	ddi_dma_mem_free(&stats_dma_acchdl);
	ddi_dma_free_handle(&dma_handle_stats);
	return (GLD_SUCCESS);
}

/*
 * iprb_reap_commands() - reap commands already processed
 */
static
void
iprb_reap_commands(gld_mac_info_t *macinfo, int mode)
{
	int		reaper, last_reaped, i;
	volatile	struct iprb_gen_cmd *gcmd;
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;

	/* Any commands to be processed ? */
	if (iprbp->iprb_first_cmd == -1)	/* no */
		return;
	reaper = iprbp->iprb_first_cmd;
	last_reaped = -1;

	do {
		/* Get the command to be reaped */
		gcmd = &iprbp->cmd_blk[reaper]->gen_cmd;

		/* If we aren't reaping all cmds */
		if (mode != IPRB_REAP_ALL_CMDS)
			/* Is it done? */
			if (!(gcmd->gen_status & IPRB_CMD_COMPLETE))
				break;	/* No */

		/* If this was a Tx command, free all the resources. */
		if (iprbp->Txbuf_info[reaper].mp != NULL) {
			freemsg(iprbp->Txbuf_info[reaper].mp);
			iprbp->Txbuf_info[reaper].mp = NULL;

			for (i = 0;
				i < iprbp->Txbuf_info[reaper].frag_nhandles;
				i++)
				(void) ddi_dma_unbind_handle(
						iprbp->Txbuf_info[reaper].
						frag_dma_handle[i]);
		}
		/*
		 *Increment to next command to be processed, looping around
		 *if necessary.
		 */

		last_reaped = reaper++;
		if (reaper == iprbp->iprb_nxmits)
			reaper = 0;
	} while (last_reaped != iprbp->iprb_last_cmd);

	/* Did we get them all? */
	if (last_reaped == iprbp->iprb_last_cmd)
		iprbp->iprb_first_cmd = -1;	/* Yes */
	else
		iprbp->iprb_first_cmd = reaper;	/* No */

}

/*
 * iprb_add_command() - Modify the command chain so that the current command
 * is known to be ready.
 */
void
iprb_add_command(gld_mac_info_t *macinfo)
{
	register volatile struct iprb_gen_cmd *current_cmd, *last_cmd;
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;

	current_cmd
		= &iprbp->cmd_blk[iprbp->iprb_current_cmd]->gen_cmd;
	last_cmd = &iprbp->cmd_blk[iprbp->iprb_last_cmd]->gen_cmd;

	current_cmd->gen_status = 0;
	/* Make it so the D100 will suspend upon completion of this cmd */
	current_cmd->gen_cmd |= IPRB_SUSPEND;

	/* This one is the new last command */
	iprbp->iprb_last_cmd = iprbp->iprb_current_cmd;

	/* If there were previously no commands, this one is first */
	if (iprbp->iprb_first_cmd == -1)
		iprbp->iprb_first_cmd = iprbp->iprb_current_cmd;

	/* Make a new current command, looping around if necessary */
	++iprbp->iprb_current_cmd;

	if (iprbp->iprb_current_cmd == iprbp->iprb_nxmits)
		iprbp->iprb_current_cmd = 0;

	/*
	 *If we think we are about to run out of resources, have this
	 *command generate a software interrupt on completion, so we will
	 *ping gld to try again.
	 */
	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd)
		current_cmd->gen_cmd |= IPRB_INTR;

	/* Make it so that the D100 will no longer suspend on last command */
	if (current_cmd != last_cmd)
		last_cmd->gen_cmd &= ~IPRB_SUSPEND;

	IPRB_SCBWAIT(iprbp);

	/*
	 *RESUME the D100.  RESUME will be ignored if the CU is either IDLE
	 *or ACTIVE, leaving the state IDLE or Active, respectively; after
	 *the first command, the CU will be either ACTIVE or SUSPENDED.
	 */

	if (iprbp->iprb_initial_cmd) {
		iprbp->iprb_initial_cmd = 0;
		ddi_io_put32(iprbp->iohandle,
				REG32(iprbp->port, IPRB_SCB_PTR),
				(uint32_t)
		    iprbp->cmd_blk_info[iprbp->iprb_last_cmd].cmd_physaddr);
		/*
		 *VVV ddi_io_put16(iprbp->iohandle, iprbp->port +
		 *IPRB_SCB_CMD, IPRB_CU_START);
		 */
		ddi_io_put8(iprbp->iohandle,
			    REG8(iprbp->port, IPRB_SCB_CMD), IPRB_CU_START);
	} else
		/*
		 *VVV ddi_io_put16(iprbp->iohandle, iprbp->port +
		 *IPRB_SCB_CMD, IPRB_CU_RESUME);
		 */
		ddi_io_put8(iprbp->iohandle,
			    REG8(iprbp->port, IPRB_SCB_CMD), IPRB_CU_RESUME);
}

/*
 * iprb_send() -- send a packet
 *
 * Called when a packet is ready to be transmitted. A pointer to an M_DATA
 * message that contains the packet is passed to this routine. The complete
 * LLC header is contained in the message's first message block, and the
 * remainder of the packet is contained within additional M_DATA message
 * blocks linked to the first message block.
 *
 * This routine may NOT free the packet.
 */
int
iprb_send(gld_mac_info_t *macinfo, mblk_t *mp)
{
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	int		i;
	size_t		pktlen = 0;
	mblk_t		*lmp;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBSEND)
		cmn_err(CE_CONT, "iprb_send(0x%x, 0x%x)",
			(int)macinfo, (int)mp);
#endif

	ASSERT(mp != NULL);

	mutex_enter(&iprbp->cmdlock);

	/* Free up any processed command buffers */
	iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);

	/* If there are no xmit control blocks available, return */
	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
		mutex_exit(&iprbp->cmdlock);
		return (GLD_NORESOURCES);
	}
	/* Count the number of mblks in list */
	for (i = 0, lmp = mp; lmp != NULL; lmp = lmp->b_cont, ++i)
		pktlen += lmp->b_wptr - lmp->b_rptr;

	/* Make sure packet isn't too large */
	if (pktlen > ETHERMAX) {
		mutex_exit(&iprbp->cmdlock);
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBSEND)
		cmn_err(CE_WARN, "iprb%d: Large Xmit Packet : %d bytes.",
				macinfo->gldm_ppa, (int)pktlen);
#endif
		iprbp->iprb_stat_frame_toolong++;
		return (GLD_BADARG);
	}
	/* Now assume worst case, that each mblk crosses (one) page boundry */
	i <<= 1;
	if (i > IPRB_MAX_FRAGS) {
#ifdef IPRBDEBUG
		if (iprbdebug & IPRBSEND)
			cmn_err(CE_WARN, "iprb: no. of fragments"
				" in packet is %d",
				i);
#endif
		if (pullupmsg(mp, -1) == 0) {
#ifdef IPRBDEBUG
			if (iprbdebug & IPRBSEND)
				cmn_err(CE_WARN,
					"iprb: pullup message of %d frags "
					"failed", i >> 1);
#endif
			mutex_exit(&iprbp->cmdlock);
			return (GLD_NORESOURCES);	/* Tell GLD to resend */
							/* the packet */
		}
	}
	/* Prepare message for transmission */
	/* Changed for 82558 enhancements */
/*
 * If this is D101 and extended TCBs are enabled use
 * iprb_prepare_ext_xmit_buff otherwise use iprb_prepare_xmit_buff.
 * Extended TCBs and Dynamic TBDs would be enabled all the time if
 * extended TCB property is enabled VA edited on APR 06, 1999
 */
/*
 * I can call mutex exit iprbp->cmdlock here and then
 * following functions iprb_prepare_xmit_buff or
 * iprb_prepare_ext_xmit_buff will again ask for
 * mutex_enter iprbp->cmdlock etc... extra mutex
 * release and regain will impact performance
 */
	if ((iprbp->iprb_revision_id >= D101A4_REV_ID) &&
		((iprbp->iprb_extended_txcb_enabled == IPRB_TRUE) ||
		(iprbp->iprb_enhanced_tx_enable == IPRB_TRUE))) {
		if (iprb_prepare_ext_xmit_buff(macinfo, mp, pktlen) ==
			GLD_NORESOURCES)
			return (GLD_NORESOURCES);
	} else if (iprb_prepare_xmit_buff(macinfo, mp, pktlen) ==
		GLD_NORESOURCES) {
		return (GLD_NORESOURCES);
		/* mutex lock is already removed by the callee */
	}
	return (GLD_SUCCESS);	/* successful transmit attempt */
}
/*
 * Mutex iprbp->cmdlock must be held before calling this function
 * Mutex will be released by this function.
 * VA edited on Jun 20, 1999
 */
int
iprb_prepare_xmit_buff(gld_mac_info_t *macinfo, mblk_t *mp, size_t pktlen)
{

	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	mblk_t		*lmp;
	int		len, i, j;
	int		used_len = 0;
	int		offset = 0;
	int		num_frags_in_stash = 0;
	uint_t		ncookies;
	ddi_dma_cookie_t	dma_cookie;

	struct	iprb_cmd_info xcmd_info;
	struct	iprb_xmit_cmd *xcmd;
	struct	iprb_Txbuf_info *new_info;
	struct	iprb_Txbuf_desc *new_txbuf_desc;

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_prepare_xmit_buff...");
#endif
	ASSERT(mutex_owned(&iprbp->cmdlock));
	xcmd_info = iprbp->cmd_blk_info[iprbp->iprb_current_cmd];
	xcmd = &iprbp->cmd_blk[iprbp->iprb_current_cmd]->xmit_cmd;
	new_txbuf_desc = (struct iprb_Txbuf_desc *)
		iprbp->Txbuf_desc[iprbp->iprb_current_cmd];
	new_info = &iprbp->Txbuf_info[iprbp->iprb_current_cmd];

	/* Set up transmit buffers */
	i = j = 0;
	lmp = mp;
	do {
		len = lmp->b_wptr - lmp->b_rptr;

		ASSERT(len > 0);
		/* skip over any zero length mblks */
		if (len == 0)
			continue;

		/* space left in stash? */
		if ((used_len + len) <= IPRB_STASH_SIZE) {
			/*
			 *Compute offset from end - allows for structure
			 *padding)
			 */
			offset = (sizeof (struct iprb_xmit_cmd))
				- (IPRB_STASH_SIZE - used_len);
			bcopy(lmp->b_rptr,
				((xcmd->data).gen_tcb.data_stash +
				used_len), len);
			new_txbuf_desc->frag[j].data = xcmd_info.cmd_physaddr
				+ offset;
			new_txbuf_desc->frag[j].frag_len = len;
			used_len += len;
			j++;
			/*
			 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd%d_buf_addr
			 *= %d\n", j, xcmd_info.cmd_physaddr+offset);
			 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd%d_buf_cnt
			 *= %d\n", j, len);
			 */
			num_frags_in_stash++;
			continue;
		}
		if (ddi_dma_addr_bind_handle(new_info->frag_dma_handle[i],
			NULL, (caddr_t)lmp->b_rptr, len,
			DDI_DMA_WRITE | DDI_DMA_STREAMING, DDI_DMA_DONTWAIT,
			0, &dma_cookie, &ncookies) != DDI_DMA_MAPPED) {
			for (j = 0; j < i; j++)
				(void) ddi_dma_unbind_handle(
					new_info->frag_dma_handle[j]);
			mutex_exit(&iprbp->cmdlock);
			return (GLD_NORESOURCES);
		}
		/* Worst case scenario is crossing 1 page boundary */
		ASSERT(ncookies == 1 || ncookies == 2);
		for (;;) {
			new_txbuf_desc->frag[j].data =
				dma_cookie.dmac_address;
			new_txbuf_desc->frag[j].frag_len =
				dma_cookie.dmac_size;
			j++;
			/*
			 *cmn_err(CE_CONT,"PF
			 *xcmd->data.ext_tcb.tbd%d_buf_addr = %d\n", j,
			 *dma_cookie.dmac_address); cmn_err(CE_CONT,"PF
			 *xcmd->data.ext_tcb.tbd%d_buf_cnt = %d\n", j,
			 *dma_cookie.dmac_size);
			 */
			if (--ncookies == 0)
				break;
			ddi_dma_nextcookie(new_info->frag_dma_handle[i],
				&dma_cookie);
		}
		i++;


	} while ((lmp = lmp->b_cont) != NULL);

	ASSERT(j <= IPRB_MAX_FRAGS);

	if (j == num_frags_in_stash) {
		/* We bcopied whole packet */
		freemsg(mp);
		new_info->mp = NULL;
	} else
		new_info->mp = mp;	/* mblk to free when complete */

	/* No. of DMA handles to unbind */
	new_info->frag_nhandles = (uint8_t)i;

	/*
	 *cmn_err(CE_CONT,"xcmd->xmit_tbd %d", xcmd->xmit_tbd);
	 *cmn_err(CE_CONT,"\nxcmd->xmit_count  = %d len = %d\n", pktlen,
	 *len); cmn_err(CE_CONT,"xcmd->xmit_threshold = %d\n",  pktlen >>
	 *iprbp->iprb_threshold); cmn_err(CE_CONT,"xcmd->xmit_tbdnum =
	 *%d\n", j); cmn_err(CE_CONT,"xcmd->xmit_next = %d\n",
	 *xcmd->xmit_next); cmn_err(CE_CONT,"xcmd->xmit_bits = %d\n",
	 *xcmd->xmit_bits); if (j > 2) cmn_err(CE_CONT,"Crossed it ");
	 */
	xcmd->xmit_tbd = new_info->tbd_physaddr;
	xcmd->xmit_count = 0;
	xcmd->xmit_threshold = pktlen >> iprbp->iprb_threshold;
	xcmd->xmit_tbdnum = (uint8_t)j;	/* No. of fragments to process */
	xcmd->xmit_cmd = IPRB_XMIT_CMD | IPRB_SF;
	iprb_add_command(macinfo);
	mutex_exit(&iprbp->cmdlock);
	return (GLD_SUCCESS);
}
/*
 * Mutex iprbp->cmdlock must be held before calling this function
 * Mutex will be released by this function.
 * VA edited on Jun 20, 1999
 */
int
iprb_prepare_ext_xmit_buff(gld_mac_info_t *macinfo, mblk_t *mp, size_t pktlen)
{
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	mblk_t		*lmp;
	int		len, i, j;
	int		used_len = 0;
	int		offset = 0;
	int		num_frags_in_stash = 0;
	uint_t		ncookies;
	ddi_dma_cookie_t	dma_cookie;

	struct	iprb_cmd_info xcmd_info;
	struct	iprb_xmit_cmd *xcmd;
	struct	iprb_Txbuf_info *new_info;
	struct	iprb_Txbuf_desc *new_txbuf_desc;

	/* To bind first 2 TBDs into the extended TCBs */
	int	tbd_cnt = 0;

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_prepare_ext_xmit_buff...");
#endif
	ASSERT(mutex_owned(&iprbp->cmdlock));
	xcmd_info = iprbp->cmd_blk_info[iprbp->iprb_current_cmd];
	xcmd = &iprbp->cmd_blk[iprbp->iprb_current_cmd]->xmit_cmd;
	new_txbuf_desc = (struct iprb_Txbuf_desc *)
		iprbp->Txbuf_desc[iprbp->iprb_current_cmd];
	new_info = &iprbp->Txbuf_info[iprbp->iprb_current_cmd];

	/* If this 82558 then enable CNA Interrupts delay */
	/*
	 *xcmd->xmit_cmd |= iprbp->curr_cna_backoff << 8;
	 */
	/* Set up transmit buffers */
	/* Pictorial View  Extended TCB */
/*
 * ---------                ---------
 * | TCB #1 | ------------>|  TCB #2 |
 * ---------                ---------
 *     |                        |
 *     V                        V
 * ---------                 --------
 * |  TBD   |               |  TBD   |
 * |  Array |--Data Buffer  |  Array |---Data Buffer  (3 data buffers asso.
 * ---------                 -------------		with TBD Array)
 *                |___ Data Buffer   |    |___Data Buffer
 *                                   |_________Data Buffer
 *
 * TCB
 * ----------------------------------------------------------------------------
 * | EL | S | I | CID | 000 | NC | SF | 100 | C |   X | OK | U |  xxxxxxxxxxxx|
 * ----------------------------------------------------------------------------
 * |         LINK ADDRESS                                                     |
 * ----------------------------------------------------------------------------
 * |         TBD ARRAY ADDRESS                                                |
 * ----------------------------------------------------------------------------
 * |  TBD NUMBER | Tx Threshold | EOF | 0 | TxCB Byte Count                   |
 * ----------------------------------------------------------------------------
 *
 * Extended TCB
 * ----------------------------------------------------------------------------
 * | EL | S | I | CID | 000 | NC | SF | 100 | C |   X | OK | U |  000000000000|
 * ----------------------------------------------------------------------------
 * |         LINK ADDRESS                                                     |
 * ----------------------------------------------------------------------------
 * |         TBD ARRAY ADDRESS    Will point to TBD Array which has first     |
 * |          TBD which will be having an address of 3rd data buffer          |
 * ----------------------------------------------------------------------------
 * |  TBD NUMBER | Tx Threshold | EOF | 0 | TxCB Byte Count                   |
 * ----------------------------------------------------------------------------
 * |  Transmit Buffer #0 Address   Address of first data Buffer               |
 * ----------------------------------------------------------------------------
 * |                                 | EL | 0 | Size (Act Count)              |
 * ----------------------------------------------------------------------------
 * |  Transmit Buffer #1 Address   Address of Second data Buffer              |
 * ----------------------------------------------------------------------------
 * |                                 | EL | 0 | Size (Act Count)              |
 * ----------------------------------------------------------------------------
 */

	i = j = 0;
	lmp = mp;
	do {
		/*
		 *Here we will dynamically chain TBDs. 82557s should first
		 *create all the TBDs and then call CU_START or CU_RESUME.
		 *In environments where virtual address must be translated
		 *to physical addresses, TBD setup can be a very time
		 *consuming process. 82558 and 82559 support a new
		 *configuration mode called "Dynamic TBD" mode, which
		 *activates two new features in the Transmit Buffer
		 *Descriptor structure NV - Not Valid pointer Device checks
		 *this bit to verify valid TBDs so that it can use or
		 *discard  Tx buffers accordingly EL - End of List bit
		 *Device knows this is the last associated TBD with this
		 *transmit frame. This bit does not need to be set on last
		 *TBD as TBD number field will indicate the same. If this
		 *bit is set for a valid TBD then device will terminate the
		 *transfer irrespective of TBD number value. So device
		 *simply waits to check TBD number or EL bit and what ever
		 *condition occurs first, it terminates the transmission.
		 *Dynamic TBD configurations will set TBD number field to
		 *0xFFh. and use EL bit to terminate the transmission. VA
		 *APR 7,99
		 */

		len = lmp->b_wptr - lmp->b_rptr;

		ASSERT(len > 0);
		/* skip over any zero length mblks */
		if (len == 0)
			continue;
		/*
		 *If it is the first or second fragment, store the size in
		 *the extended TCB fields. Once they are full, start storing
		 *the sizes in the TBD array. VA, APR 7,99
		 */

		/* space left in stash? */
		if ((used_len + len) <= IPRB_STASH_SIZE) {
			/*
			 *Compute offset from end - allows for structure
			 *padding
			 */
			offset = (sizeof (struct iprb_xmit_cmd))
				- (IPRB_STASH_SIZE - used_len);
			bcopy(lmp->b_rptr,
				((xcmd->data).ext_tcb.data_stash +
				used_len), len);

			if (tbd_cnt == 0) {
				(xcmd->data).ext_tcb.tbd0_buf_addr =
				xcmd_info.cmd_physaddr + offset;
				(xcmd->data).ext_tcb.tbd0_buf_cnt = len;
				/*
				 *
				 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd0_buf
				 *_cnt = %d\n",
				 *(xcmd->data).ext_tcb.tbd0_buf_cnt);
				 */
			}
			if (tbd_cnt == 1) {
				(xcmd->data).ext_tcb.tbd1_buf_addr =
				xcmd_info.cmd_physaddr + offset;
				(xcmd->data).ext_tcb.tbd1_buf_cnt = len;
				/*
				 *
				 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd1_buf
				 *_cnt = %d\n",
				 *(xcmd->data).ext_tcb.tbd1_buf_cnt);
				 */
			}
			if (tbd_cnt > 1) {
				new_txbuf_desc->frag[j].data =
				xcmd_info.cmd_physaddr + offset;
				new_txbuf_desc->frag[j].frag_len = len;
				j++;
			}
			used_len += len;
			num_frags_in_stash++;
			tbd_cnt++;
		} else {
			if (ddi_dma_addr_bind_handle(
					new_info->frag_dma_handle[i],
					NULL, (caddr_t)lmp->b_rptr, len,
					DDI_DMA_WRITE | DDI_DMA_STREAMING,
					DDI_DMA_DONTWAIT,
					0, &dma_cookie, &ncookies) !=
					DDI_DMA_MAPPED) {
				for (j = 0; j < i; j++)
					(void) ddi_dma_unbind_handle(
						new_info->frag_dma_handle[j]);
				mutex_exit(&iprbp->cmdlock);
				return (GLD_NORESOURCES);
			}
			/* Worst case scenario is crossing 1 page boundary */
			ASSERT(ncookies == 1 || ncookies == 2);
			for (;;) {

				if (tbd_cnt == 0) {
					(xcmd->data).ext_tcb.tbd0_buf_addr =
						dma_cookie.dmac_address;
					(xcmd->data).ext_tcb.tbd0_buf_cnt =
						dma_cookie.dmac_size;
					/*
					 *cmn_err(CE_CONT,"PF
					 *xcmd->data.ext_tcb.tbd0_buf_cnt =
					 *%d\n",
					 *(xcmd->data).ext_tcb.tbd0_buf_cnt);
					 **/
				}
				if (tbd_cnt == 1) {
					(xcmd->data).ext_tcb.tbd1_buf_addr =
						dma_cookie.dmac_address;
					(xcmd->data).ext_tcb.tbd1_buf_cnt =
						dma_cookie.dmac_size;
					/*
					 *cmn_err(CE_CONT,"PF
					 *xcmd->data.ext_tcb.tbd1_buf_cnt =
					 *%d\n",
					 *(xcmd->data).ext_tcb.tbd1_buf_cnt);
					 **/
				}
				if (tbd_cnt > 1) {
					new_txbuf_desc->frag[j].data =
						dma_cookie.dmac_address;
					new_txbuf_desc->frag[j].frag_len =
						dma_cookie.dmac_size;
					j++;
				}
				tbd_cnt++;
				/*
				 *cmn_err(CE_CONT,"TBD Count = %d ncookies =
				 *%d\n", tbd_cnt, ncookies);
				 *cmn_err(CE_CONT,"PF
				 *xcmd->data.ext_tcb.tbd%d_buf_cnt = %d
				 *tbd_cnt = %d\n", j, dma_cookie.dmac_size,
				 *tbd_cnt);
				 */

				if (--ncookies == 0)
					break;
				ddi_dma_nextcookie(new_info->frag_dma_handle[i],
						&dma_cookie);

			}
		}		/* End of else */

		/*
		 *82558 and 82559 has an advanced transmit command block
		 *When enabled, device reads an 8 DWORD TxCB from host
		 *memory into its internal registers instead of the standard
		 *4 Dword TxCB. This new TxCB is composed of the 4 standard
		 *TxCB DWORDs followed by 2 TBDs, 2 DWORDs each. VA edited
		 *in APR 7, 99
		 */
		/*
		 *if (tbd_cnt == 0 ) (xcmd->data).ext_tcb.tbd0_buf_cnt &=
		 *~IPRB_EL_BIT; if (tbd_cnt == 1)
		 *(xcmd->data).ext_tcb.tbd1_buf_cnt &= ~IPRB_EL_BIT; if
		 *(tbd_cnt > 1) xcmd->xmit_count &= ~IPRB_EL_BIT;
		 */

		/*
		 *If this is the first or second fragment, store address in
		 *the extended  TCB fields. Once these are full, start
		 *storing the addresses in the TBD array. VA edited on APR
		 *7, 99
		 */
		/* Try using TCB field 0 TBD */

		/*
		 *If extended TCB fields are set up, then start the control
		 *unit. I will continue setting up the remaining TBDs
		 *dynamically
		 */
		/*
		 *I would have used the extended TCB fields as our first two
		 *TBDs. Starting from the third fragment, I would be using
		 *the TBD array. So, when tbd_cnt is 3, we have already
		 *filled up the first 2 TBDs in the TBD array.  VA edited on
		 *APR 8, 99
		 */

		if (iprbp->iprb_enhanced_tx_enable == IPRB_TRUE) {

			if ((lmp->b_cont == NULL) && (tbd_cnt == 1)) {
				(xcmd->data).ext_tcb.tbd1_buf_cnt |=
				IPRB_EL_BIT;
			}
			if ((lmp->b_cont == NULL) && (tbd_cnt == 2)) {
				new_txbuf_desc->frag[0].frag_len |= IPRB_EL_BIT;
			}
			if ((lmp->b_cont == NULL) && (tbd_cnt > 2)) {
				new_txbuf_desc->frag[j].frag_len |= IPRB_EL_BIT;
			}
			if (tbd_cnt == 3) {
				/* Set up tcb fields */
				xcmd->xmit_count = 0;
				/*
				 *In Flexible Mode, transmit all data
				 *pointed by TBD Array
				 */
				xcmd->xmit_threshold =
					pktlen >> iprbp->iprb_threshold;
				/*
				 *This value needs optimization as most of
				 *the traffic should cross the limit.
				 *Minimum value is 6 (48 bytes) and maximum
				 *value is 190 (1520 bytes) VA edited on APR
				 *8, 99
				 */
				if (j == 0)
					xcmd->xmit_tbd = 0;
				else
					xcmd->xmit_tbd = new_info->tbd_physaddr;
				/* Dynamic TBDs rely on validity of pointer */
				/* marked by NV flag */
				xcmd->xmit_tbdnum = (uint8_t)tbd_cnt;
				xcmd->xmit_cmd = IPRB_XMIT_CMD | IPRB_SF;
				iprb_add_command(macinfo);
			}
		}
		i++;

	} while ((lmp = lmp->b_cont) != NULL);

	ASSERT(j <= IPRB_MAX_FRAGS);

	if (tbd_cnt == num_frags_in_stash) {
		/* We bcopied whole packet */
		freemsg(mp);
		new_info->mp = NULL;
	} else
		new_info->mp = mp;	/* mblk to free when complete */

	/* No. of DMA handles to unbind */
	new_info->frag_nhandles = (uint8_t)i;

	/*
	 *cmn_err(CE_CONT,"xcmd->xmit_tbd %d", xcmd->xmit_tbd);
	 *cmn_err(CE_CONT,"\nxcmd->xmit_count  = %d len = %d\n", pktlen,
	 *len); cmn_err(CE_CONT,"xcmd->xmit_threshold = %d\n",  pktlen >>
	 *iprbp->iprb_threshold); cmn_err(CE_CONT,"xcmd->xmit_tbdnum = %d j
	 *value = %d\n", tbd_cnt, j); cmn_err(CE_CONT,"xcmd->xmit_next =
	 *%d\n", xcmd->xmit_next); cmn_err(CE_CONT,"xcmd->xmit_bits = %d\n",
	 *xcmd->xmit_bits);
	 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd0_buf_addr = %d\n",
	 *(xcmd->data).ext_tcb.tbd0_buf_addr);
	 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd0_buf_cnt = %d\n",
	 *(xcmd->data).ext_tcb.tbd0_buf_cnt);
	 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd1_buf_addr = %d\n",
	 *(xcmd->data).ext_tcb.tbd1_buf_addr);
	 *cmn_err(CE_CONT,"xcmd->data.ext_tcb.tbd1_buf_cnt = %d\n",
	 *(xcmd->data).ext_tcb.tbd1_buf_cnt);
	 */

	/*
	 *If there were less than 3 frags, then the control unit is still
	 *waiting to start
	 */
	if (iprbp->iprb_enhanced_tx_enable == IPRB_TRUE) {
		if ((tbd_cnt) && (tbd_cnt < 3)) {
			/* Set up tcb fields */
			xcmd->xmit_count = 0;
			/*
			 *In Flexible Mode, transmit
			 *all data pointed by TBD
			 *Array
			 */
			xcmd->xmit_threshold = pktlen >> iprbp->iprb_threshold;
			if (j == 0)
				xcmd->xmit_tbd = 0;
			else
				xcmd->xmit_tbd = new_info->tbd_physaddr;
/*
 * This value needs optimization as most of the
 * traffic should cross the limit. Minimum value is 6
 * (48 bytes) and maximum value is 190 (1520 bytes)
 * VA edited on APR 8, 99
 */
	/* Dynamic TBDs rely on validity of pointer marked by NV flag */
			xcmd->xmit_tbdnum = (uint8_t)tbd_cnt;
			xcmd->xmit_cmd = IPRB_XMIT_CMD | IPRB_SF;
			iprb_add_command(macinfo);
		}
	} else {
		/* Set up tcb fields */
		xcmd->xmit_count = 0;	/* In Flexible Mode, transmit all */
					/* data pointed by TBD Array  */
		xcmd->xmit_threshold = pktlen >> iprbp->iprb_threshold;
		if (j == 0)
			xcmd->xmit_tbd = 0;
		else
			xcmd->xmit_tbd = new_info->tbd_physaddr;
/*
 * This value needs optimization as most of the traffic
 * should cross the limit. Minimum value is 6 (48 bytes) and
 * maximum value is 190 (1520 bytes) VA edited on APR 8, 99
 */
	/* Dynamic TBDs rely on validity of pointer marked by NV flag */
		xcmd->xmit_tbdnum = (uint8_t)tbd_cnt;
		xcmd->xmit_cmd = IPRB_XMIT_CMD | IPRB_SF;
		iprb_add_command(macinfo);
	}
	mutex_exit(&iprbp->cmdlock);
	return (GLD_SUCCESS);
}
/*
 * iprb_intr() -- interrupt from board to inform us that a receive or
 * transmit has completed.
 */

/*
 * IMPORTANT NOTE - Due to a bug in the D100 controller, the suspend/resume
 * functionality for receive is not reliable.  We are therefore using the EL
 * bit, along with RU_START.  In order to do this properly, it is very
 * important that the order of interrupt servicing remains intact; more
 * specifically, it is important that the FR interrupt bit is serviced before
 * the RNR interrupt bit is serviced, because both bits will be set when an
 * RNR condition occurs, and we must process the received frames before we
 * restart the receive unit. */
uint_t
iprb_intr(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;

	int	intr_status;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBINT)
		cmn_err(CE_CONT, "iprb_intr(0x%x)", (int)macinfo);
#endif

	mutex_enter(&iprbp->intrlock);

	/* Get interrupt bits */
	intr_status =
		ddi_io_get16(iprbp->iohandle,
			REG16(iprbp->port, IPRB_SCB_STATUS))
		& IPRB_SCB_INTR_MASK;

	if (intr_status == 0) {
		mutex_exit(&iprbp->intrlock);
		return (DDI_INTR_UNCLAIMED);	/* was NOT our interrupt */
	}
	/* Acknowledge all interrupts */
	ddi_io_put16(iprbp->iohandle,
		REG16(iprbp->port, IPRB_SCB_STATUS), intr_status);

	/* Frame received */
	if (intr_status & IPRB_INTR_FR) {
		(void) drv_getparm(LBOLT, &iprbp->RUwdog_lbolt);
		iprb_process_recv(macinfo);
	}
	/* Out of resources on receive condition */
	if (intr_status & IPRB_INTR_RNR) {
		(void) drv_getparm(LBOLT, &iprbp->RUwdog_lbolt);

		/* Reset End-of-List */
		iprbp->rfd[iprbp->iprb_last_rfd]->rfd_control &=
			~IPRB_RFD_EL;
		iprbp->rfd[iprbp->iprb_nrecvs - 1]->rfd_control |=
			IPRB_RFD_EL;

		/* Reset driver's pointers */
		iprbp->iprb_first_rfd = 0;
		iprbp->iprb_last_rfd = iprbp->iprb_nrecvs - 1;
		iprbp->iprb_current_rfd = 0;

		/* and start at first RFD again */
		mutex_enter(&iprbp->cmdlock);
		IPRB_SCBWAIT(iprbp);
		ddi_io_put32(iprbp->iohandle,
				REG32(iprbp->port, IPRB_SCB_PTR),
				iprbp->rfd_info[
				iprbp->iprb_current_rfd]->rfd_physaddr);
		IPRB_SCBWAIT(iprbp);
/*
 * VVV ddi_io_put16(iprbp->iohandle, iprbp->port +
 * IPRB_SCB_CMD, IPRB_RU_START);
 */
		ddi_io_put8(iprbp->iohandle,
			    REG8(iprbp->port, IPRB_SCB_CMD), IPRB_RU_START);
		mutex_exit(&iprbp->cmdlock);
	}
	if (intr_status & IPRB_INTR_CXTNO)
		gld_sched(macinfo);

/*
 * VA still testing. if (intr_status & IPRB_INTR_CNACI) {
 * cmn_err(CE_CONT, "Got CNA Interrupt..."); }
 */

	/* Should never get this interrupt */
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBINT)
		if (intr_status & IPRB_INTR_MDI)
			cmn_err(CE_WARN, "IPRB: Received MDI interrupt.");
#endif
	iprbp->iprb_stat_intr++;
	mutex_exit(&iprbp->intrlock);
	return (DDI_INTR_CLAIMED);	/* Indicate it WAS our interrupt */
}


/*
 * iprb_start_board() -- start the board receiving.
 */

int
iprb_start_board(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_start_board...");
#endif
	mutex_enter(&iprbp->intrlock);

	/* Disable Interrupts */
	DisableInterrupts(iprbp);

	/* Tell the board what its address is (from macaddr) */
	(void) iprb_set_mac_addr(macinfo, iprbp->macaddr);

	iprb_configure(macinfo);

	/* Loading appropriate microcode... */
	(void) iprb_load_microcode(macinfo, iprbp->iprb_revision_id);

	iprbp->iprb_receive_enabled = 1;

	/* Reset End-of-List */
	iprbp->rfd[iprbp->iprb_last_rfd]->rfd_control &= ~IPRB_RFD_EL;
	iprbp->rfd[iprbp->iprb_nrecvs - 1]->rfd_control |= IPRB_RFD_EL;

	/* Reset driver's pointers */
	iprbp->iprb_first_rfd = 0;
	iprbp->iprb_last_rfd = iprbp->iprb_nrecvs - 1;
	iprbp->iprb_current_rfd = 0;
	/*
	 *Initialise the Watch Dog timer to be used to fix hardware RU
	 *lockup. This lockup will not notify S/W. Intel recommends a 2
	 *second watch dog to free the RU lockup.
	 */
	if (iprbp->RU_needed) {
		iprbp->RUwdog_lbolt = 0;
		iprbp->RUwdogID =
		(int)timeout(iprb_RU_wdog,
		(void *)macinfo, (clock_t)RUWDOGTICKS);
	}
	/* and start at first RFD again */
	mutex_enter(&iprbp->cmdlock);
	IPRB_SCBWAIT(iprbp);
	ddi_io_put32(iprbp->iohandle,
			REG32(iprbp->port, IPRB_SCB_PTR),
			iprbp->rfd_info[iprbp->iprb_first_rfd]->rfd_physaddr);
	IPRB_SCBWAIT(iprbp);
/*
 * VVV ddi_io_put16(iprbp->iohandle, iprbp->port + IPRB_SCB_CMD,
 * IPRB_RU_START);
 */
	ddi_io_put8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_CMD), IPRB_RU_START);
	mutex_exit(&iprbp->cmdlock);
	EnableInterrupts(iprbp);

	mutex_exit(&iprbp->intrlock);
	(void) mii_start_portmon(iprbp->mii, iprb_mii_linknotify_cb,
			&iprbp->cmdlock);
	return (GLD_SUCCESS);
}


/* iprb_stop_board() - disable receiving */
int
iprb_stop_board(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;
	clock_t	microsec = IPRB_DELAY_TIME;
	int	i = 10;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_stop_board...");
#endif
	mutex_enter(&iprbp->intrlock);
	mutex_enter(&iprbp->cmdlock);
/*
 * We don't want to do this as we may loose link in between and no
 * one will inform OS... VA edited on Jun 1, 1999
 */
	(void) mii_stop_portmon(iprbp->mii);
	iprbp->iprb_receive_enabled = 0;
	if (iprbp->RU_needed)
		(void) untimeout((timeout_id_t)(iprbp->RUwdogID));
	DisableInterrupts(iprbp);
	/* Turn off receiving unit */
	IPRB_SCBWAIT(iprbp);
	/*
	 *VVV ddi_io_put16(iprbp->iohandle, iprbp->port + IPRB_SCB_CMD,
	 *IPRB_RU_ABORT);
	 */
	ddi_io_put8(iprbp->iohandle,
		    REG8(iprbp->port, IPRB_SCB_CMD), IPRB_RU_ABORT);

	/* Try to reap all commands which are complete */
	iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);
	/* Give enough time to clear full command queue */
	while (iprbp->iprb_first_cmd != -1) {
		delay(drv_usectohz(microsec));
		iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);
		i--;
		if (i == 0) {
			cmn_err(CE_WARN, "iprb : could not reclaim all"
				" command buffers");
			break;
		}
	}
	mutex_exit(&iprbp->cmdlock);
	mutex_exit(&iprbp->intrlock);
	return (GLD_SUCCESS);
}


/* Code to process all of the receive packets */
static void
iprb_process_recv(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;

	int		current;
	mblk_t		*mp;
	unsigned short	rcv_len;
	struct iprb_rfd	*rfd, *rfd_end;
	struct iprb_rfd_info	*newrx, *currx;

	/* XXX desballoc is not in DDI XXX */
	extern	mblk_t  *desballoc(unsigned char *, size_t, uint_t, frtn_t *);

#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_process_recv...");
#endif
	/* while we don't come across command not complete */
	for (;;) {
		/* Start with the current one */
		current = iprbp->iprb_current_rfd;
		rfd = iprbp->rfd[current];

		/* Is it complete? */
		if (!(rfd->rfd_status & IPRB_RFD_COMPLETE))
			break;

		if (!(rfd->rfd_status & IPRB_RFD_OK)) {
#ifdef IPRBDEBUG
			int	found_reason = 0;

			if (rfd->rfd_status & IPRB_RFD_CRC_ERR) {
				cmn_err(CE_WARN, "iprb: CRC error in received "
					"frame");
				found_reason = 1;
			}
			if (rfd->rfd_status & IPRB_RFD_ALIGN_ERR) {
				cmn_err(CE_WARN, "iprb: alignment error in "
					"received frame");
				found_reason = 1;
			}
			if (rfd->rfd_status & IPRB_RFD_NO_BUF_ERR) {
				cmn_err(CE_WARN, "iprb: no buffer space while"
					" receiving frame");
				found_reason = 1;
				/* N.N.B There used to be a 'break' here ! */
			}
			if (rfd->rfd_status & IPRB_RFD_DMA_OVERRUN) {
				cmn_err(CE_WARN, "iprb: DMA overrun while "
					"receiving frame");
				found_reason = 1;
			}
			if (rfd->rfd_status & IPRB_RFD_SHORT_ERR) {
				cmn_err(CE_WARN, "iprb: received short "
					"frame");
				found_reason = 1;
			}
			if (rfd->rfd_status & IPRB_RFD_PHY_ERR) {
				cmn_err(CE_WARN, "iprb: physical media error "
					"while receiving frame");
				found_reason = 1;
			}
			if (rfd->rfd_status & IPRB_RFD_COLLISION) {
				cmn_err(CE_WARN, "iprb: collision occured "
					"while receiving frame");
				found_reason = 1;
			}
			if (!found_reason) {
				cmn_err(CE_WARN, "iprb: received frame is not "
					"ok, but for an unknown "
					"reason");
			}
#endif
			goto failed_receive;
		}
		/* Get the length from the RFD */
		rcv_len = rfd->rfd_count & IPRB_RFD_CNTMSK;

		if (!iprbp->iprb_receive_enabled)
			goto failed_receive;

		currx = iprbp->rfd_info[current];
		/*
		 *Packet Rx'd is smaller than iprbp->max_rxbcopy then we
		 *bcopy the packet.
		 */
		if ((rcv_len < iprbp->max_rxbcopy) &&
		    ((mp = allocb(rcv_len, BPRI_MED)) != NULL)) {
			bcopy((caddr_t)(currx->rfd_virtaddr +
				IPRB_RFD_OFFSET), (caddr_t)mp->b_wptr,
				rcv_len);
		} else {
			mutex_enter(&iprbp->freelist_mutex);
			newrx = iprb_get_buffer(iprbp);
			if (newrx != NULL)
				iprbp->rfds_outstanding++;
			mutex_exit(&iprbp->freelist_mutex);

			if (newrx == NULL) {	/* NEWRX == NULL */
				/*
				 *No buffers available on free list, bcopy
				 *the data from the buffer and keep the
				 *original buffer.
				 */
				if ((mp = allocb(rcv_len, BPRI_MED)) == NULL)
					goto failed_receive;

				bcopy((caddr_t)currx->rfd_virtaddr +
					IPRB_RFD_OFFSET, (caddr_t)mp->b_wptr,
					rcv_len);
			} else { /* NEWRX != NULL */
				if ((mp = desballoc(
					(unsigned char *) currx->rfd_virtaddr +
					IPRB_RFD_OFFSET,
					rcv_len, 0,
					(frtn_t *)currx)) == NULL) {
					iprbp->iprb_stat_norcvbuf++;

					mutex_enter(&iprbp->freelist_mutex);
					newrx->next = iprbp->free_list;
					iprbp->free_list = newrx;
					iprbp->free_buf_cnt++;
					iprbp->rfds_outstanding--;
					mutex_exit(&iprbp->freelist_mutex);

					goto failed_receive;
				}
				/* Link in the new rfd into the chain */
				iprbp->rfd_info[current] = newrx;
				rfd = (struct iprb_rfd *)newrx->rfd_virtaddr;

				iprbp->rfd[iprbp->iprb_last_rfd]->rfd_next =
					newrx->rfd_physaddr;
				rfd->rfd_next = iprbp->rfd[current]->rfd_next;
				iprbp->rfd[current] = rfd;
			}
		}
		mp->b_wptr += rcv_len;
		gld_recv(macinfo, mp);

failed_receive:
		rfd_end = iprbp->rfd[iprbp->iprb_last_rfd];

		/* This one becomes the new end-of-list */
		rfd->rfd_control |= IPRB_RFD_EL;
		rfd->rfd_count = 0;
		rfd->rfd_status = 0;

		iprbp->iprb_last_rfd = current;

		/* Turn off EL bit on old end-of-list (if not same) */
		if (rfd_end != rfd)
			rfd_end->rfd_control &= ~IPRB_RFD_EL;

		/* Current one moves up, looping around if necessary */
		iprbp->iprb_current_rfd++;
		if (iprbp->iprb_current_rfd == iprbp->iprb_nrecvs)
			iprbp->iprb_current_rfd = 0;

	}
}

/*
 * Code directly from Intel spec to read EEPROM data for the ethernet
 * address, one bit at a time. The read can only be a 16-bit read.
 */

static void
iprb_readia(struct iprbinstance *iprbp, uint16_t *addr, uint8_t offset)
{

	uint8_t		eex;
	/*
	 *VA edited on 2/8/99 Calculate address length
	 */
	ushort_t	address_length = 6;
	address_length = IPRB_EEPROM_ADDRESS_SIZE(iprbp->iprb_eeprom_size);
	eex = ddi_io_get8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL));
	eex &= ~(IPRB_EEDI | IPRB_EEDO | IPRB_EESK);
	eex |= IPRB_EECS;
	ddi_io_put8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL), eex);
	iprb_shiftout(iprbp, IPRB_EEPROM_READ, 3);
	/*
	 *VA edited on 2/8/99 I will now use address_length instead of 6
	 */
	iprb_shiftout(iprbp, offset, address_length);
	*addr = iprb_shiftin(iprbp);
	iprb_eeclean(iprbp);

}

static void
iprb_shiftout(struct iprbinstance *iprbp, uint16_t data, uint8_t count)
{
	uint8_t		eex;
	uint16_t	mask;

	mask = 0x01 << (count - 1);
	eex = ddi_io_get8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL));
	eex &= ~(IPRB_EEDO | IPRB_EEDI);

	do {
		eex &= ~IPRB_EEDI;
		if (data & mask)
			eex |= IPRB_EEDI;

		ddi_io_put8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_EECTL), eex);
		drv_usecwait(100);
		iprb_raiseclock(iprbp, (uint8_t *)&eex);
		iprb_lowerclock(iprbp, (uint8_t *)&eex);
		mask = mask >> 1;
	} while (mask);

	eex &= ~IPRB_EEDI;
	ddi_io_put8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL), eex);
}

static void
iprb_raiseclock(struct iprbinstance *iprbp, uint8_t *eex)
{
	*eex = *eex | IPRB_EESK;
	ddi_io_put8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL), *eex);
	drv_usecwait(100);
}

static void
iprb_lowerclock(struct iprbinstance *iprbp, uint8_t *eex)
{
	*eex = *eex & ~IPRB_EESK;
	ddi_io_put8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL), *eex);
	drv_usecwait(100);
}

uint16_t
iprb_shiftin(struct iprbinstance *iprbp)
{
	uint16_t	d;
	uint8_t		x, i;

	x = ddi_io_get8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL));
	x &= ~(IPRB_EEDO | IPRB_EEDI);
	d = 0;

	for (i = 0; i < 16; i++) {
		d = d << 1;
		iprb_raiseclock(iprbp, &x);
		x = ddi_io_get8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_EECTL));
		x &= ~(IPRB_EEDI);
		if (x & IPRB_EEDO)
			d |= 1;

		iprb_lowerclock(iprbp, &x);
	}

	return (d);
}

static void
iprb_eeclean(struct iprbinstance *iprbp)
{
	uint8_t		eex;

	eex = ddi_io_get8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL));
	eex &= ~(IPRB_EECS | IPRB_EEDI);
	ddi_io_put8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL), eex);

	iprb_raiseclock(iprbp, &eex);
	iprb_lowerclock(iprbp, &eex);
}

/*
 * Acquire all DMA resources for driver operation.
 */
static int
iprb_alloc_dma_resources(struct iprbinstance *iprbp)
{
	dev_info_t	*devinfo = iprbp->iprb_dip;
	register int	i, j;
	ddi_dma_cookie_t	dma_cookie;
	size_t		len;
	uint_t		ncookies;
	uint32_t	last_dma_addr;
	struct	iprb_rfd_info *rfd_info_free;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_alloc_dma_resources...");
#endif


	/*
	 *How many Rx descriptors do I need ?
	 *
	 *Every Rx Descriptor equates to a Rx buffer.
	 */

	/*
	 *Allocate all the DMA structures that need to be freed when the
	 *driver is detached using iprb_free_dma_resources().
	 */

	/*
	 *Command Blocks (Tx) Memory First Command Blocks are used to tell
	 *the adapter what to do, which is mostly to transmit packets. So
	 *the amount of Command blocks is equivalent to the amount of Tx
	 *descriptors (they are one and the same).
	 */

	last_dma_addr = 0;
	for (i = iprbp->iprb_nxmits - 1; i >= 0; i--) {

		/* Allocate a DMA handle for all Command Blocks (Tx) */
		if (ddi_dma_alloc_handle(devinfo, &control_cmd_dma_attr,
			DDI_DMA_SLEEP, 0,
			&iprbp->cmd_blk_info[i].cmd_dma_handle) !=
			DDI_SUCCESS)
			goto error;

		/* Allocate the Command Block itself */
		if (ddi_dma_mem_alloc(iprbp->cmd_blk_info[i].cmd_dma_handle,
			sizeof (union iprb_generic_cmd), &accattr,
			DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0,
			(caddr_t *)&iprbp->cmd_blk_info[i].cmd_virtaddr,
			&len,
			&iprbp->cmd_blk_info[i].cmd_acc_handle) !=
			DDI_SUCCESS) {
			ddi_dma_free_handle(
				    &iprbp->cmd_blk_info[i].cmd_dma_handle);
			iprbp->cmd_blk_info[i].cmd_dma_handle = NULL;
			goto error;
		}
		/* Bind the Handle and the memory together */
		if (ddi_dma_addr_bind_handle(
			iprbp->cmd_blk_info[i].cmd_dma_handle,
			NULL, (caddr_t)iprbp->cmd_blk_info[i].cmd_virtaddr,
			sizeof (union iprb_generic_cmd), DDI_DMA_RDWR |
			DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0, &dma_cookie,
			&ncookies) != DDI_DMA_MAPPED) {
			ddi_dma_mem_free(
				&iprbp->cmd_blk_info[i].cmd_acc_handle);
			ddi_dma_free_handle(
				&iprbp->cmd_blk_info[i].cmd_dma_handle);
			iprbp->cmd_blk_info[i].cmd_dma_handle =
				iprbp->cmd_blk_info[i].cmd_acc_handle = NULL;
			goto error;
		}
		ASSERT(ncookies == 1);
		/* Zero out the Command Blocks (Tx) for use */
		bzero((caddr_t)iprbp->cmd_blk_info[i].cmd_virtaddr,
			sizeof (union iprb_generic_cmd));
		iprbp->cmd_blk_info[i].cmd_physaddr = dma_cookie.dmac_address;
		iprbp->cmd_blk[i] = (union iprb_generic_cmd *)
			iprbp->cmd_blk_info[i].cmd_virtaddr;

		/* Initialise the Command Blocks for operation */
		iprbp->cmd_blk[i]->xmit_cmd.xmit_next = last_dma_addr;
		iprbp->cmd_blk[i]->xmit_cmd.xmit_cmd = IPRB_XMIT_CMD | IPRB_SF;
		last_dma_addr = dma_cookie.dmac_address;
	}
	iprbp->cmd_blk[iprbp->iprb_nxmits - 1]->xmit_cmd.xmit_next =
		last_dma_addr;

	/* PreAllocate Tx Buffer descriptors. (One per Command Block) */
	for (i = iprbp->iprb_nxmits - 1; i >= 0; i--) {
		/* Allocate a handle for the Tx Buffer Descriptor */
		if (ddi_dma_alloc_handle(devinfo, &tx_buffer_desc_dma_attr,
			DDI_DMA_SLEEP, 0,
			&iprbp->Txbuf_info[i].tbd_dma_handle) !=
			DDI_SUCCESS)
			goto error;

		/* Allocate the Tx Buffer Descriptor itself */
		if (ddi_dma_mem_alloc(iprbp->Txbuf_info[i].tbd_dma_handle,
			sizeof (struct iprb_Txbuf_desc), &accattr,
			DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0,
			(caddr_t *)&iprbp->Txbuf_info[i].tbd_virtaddr, &len,
			&iprbp->Txbuf_info[i].tbd_acc_handle) !=
			DDI_SUCCESS) {
			ddi_dma_free_handle(
				&iprbp->Txbuf_info[i].tbd_dma_handle);
			iprbp->Txbuf_info[i].tbd_dma_handle = NULL;
			goto error;
		}
		/* Bind the Handle and the memory together */
		if (ddi_dma_addr_bind_handle(
			iprbp->Txbuf_info[i].tbd_dma_handle,
			NULL, (caddr_t)iprbp->Txbuf_info[i].tbd_virtaddr,
			sizeof (struct iprb_Txbuf_desc), DDI_DMA_RDWR |
			DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0, &dma_cookie,
			&ncookies) != DDI_DMA_MAPPED) {
			ddi_dma_mem_free(
				&iprbp->Txbuf_info[i].tbd_acc_handle);
			ddi_dma_free_handle(
				&iprbp->Txbuf_info[i].tbd_dma_handle);
			iprbp->Txbuf_info[i].tbd_acc_handle = NULL;
			iprbp->Txbuf_info[i].tbd_dma_handle = NULL;
			goto error;
		}
		ASSERT(ncookies == 1);

		/* Zero out the Tx Buffer descriptors for use */
		bzero((caddr_t)iprbp->Txbuf_info[i].tbd_virtaddr,
			sizeof (struct iprb_Txbuf_desc));
		iprbp->Txbuf_info[i].tbd_physaddr =
			dma_cookie.dmac_address;
		iprbp->Txbuf_desc[i] = (struct iprb_Txbuf_desc *)
			iprbp->Txbuf_info[i].tbd_virtaddr;
	}

	/* PreAllocate handles for all the Tx Fragments held by TxBuf desc. */
	for (i = 0; i < iprbp->iprb_nxmits; i++)
		for (j = 0; j < IPRB_MAX_FRAGS; j++)
			if (ddi_dma_alloc_handle(devinfo,
				&tx_buffer_dma_attr, DDI_DMA_SLEEP, 0,
				&iprbp->Txbuf_info[i].frag_dma_handle[j]) !=
				DDI_SUCCESS)
				goto error;

	/*
	 *PRE-Allocate all the required Rx descriptors/buffers.
	 */

	/* Create the "free list" of Rx buffers. */
	for (i = 0; i < iprbp->min_rxbuf; i++) {
		rfd_info_free = iprb_alloc_buffer(iprbp);
		if (rfd_info_free == NULL)
			goto error;

		rfd_info_free->next = iprbp->free_list;
		iprbp->free_list = rfd_info_free;
		iprbp->free_buf_cnt++;
	}

	/* Take a buffer for each descriptor */
	ASSERT(iprbp->iprb_nrecvs <= IPRB_MAX_RECVS);
	last_dma_addr = IPRB_NULL_PTR;
	for (i = iprbp->iprb_nrecvs - 1; i >= 0; i--) {
		mutex_enter(&iprbp->freelist_mutex);
		rfd_info_free = iprb_get_buffer(iprbp);
		mutex_exit(&iprbp->freelist_mutex);

		/* Initialise the Rx descriptors into a list */
		iprbp->rfd_info[i] = rfd_info_free;
		iprbp->rfd[i] = (struct iprb_rfd *)rfd_info_free->rfd_virtaddr;
		iprbp->rfd[i]->rfd_next = last_dma_addr;
		iprbp->rfd[i]->rfd_rbd = IPRB_NULL_PTR;
		iprbp->rfd[i]->rfd_size = ETHERMAX;
		last_dma_addr = rfd_info_free->rfd_physaddr;
	}
	/* This is the end of Rx descriptor list */
	iprbp->rfd[iprbp->iprb_nrecvs - 1]->rfd_next = last_dma_addr;
	iprbp->rfd[iprbp->iprb_nrecvs - 1]->rfd_control |= (IPRB_RFD_EL);

	return (DDI_SUCCESS);

error:
	cmn_err(CE_WARN, "iprb: could not allocate enough DMA memory,"
		"Add lomempages through /etc/system file");
	iprb_free_dma_resources(iprbp);
	return (DDI_FAILURE);
}
/*
 * Release all DMA resources. In the opposite order from
 * iprb_alloc_dma_resources().
 */
static void
iprb_free_dma_resources(struct iprbinstance *iprbp)
{
	register int	i, j;

	/*
	 *Free all Rx resources and the Rx Free list.
	 */
	ASSERT(iprbp->iprb_receive_enabled == 0);
	ASSERT(iprbp->rfds_outstanding == 0);
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_free_dma_resources...");
#endif
	/*
	 *Free all the Rx Desc/buffers currently in use, place them on the
	 *'free list' to be freed up.
	 */
	for (i = 0; i < iprbp->iprb_nrecvs; i++) {
		if (iprbp->rfd[i] == NULL)
			continue;
		ASSERT(iprbp->rfd_info[i] != NULL);
		(iprbp->rfd_info[i])->next = iprbp->free_list;
		iprbp->free_list = iprbp->rfd_info[i];
		iprbp->free_buf_cnt++;
		iprbp->rfd[i] = NULL;
		iprbp->rfd_info[i] = NULL;
	}

	/* Free Rx buffers on the "free list" */
	for (; iprbp->free_list; )
		iprb_remove_buffer(iprbp);
	ASSERT(iprbp->free_buf_cnt == 0);
	ASSERT(iprbp->rfd_cnt == 0);
	/*
	 *Free all Tx resources and all Command Blocks used for the adapter.
	 */

	/* Free the Tx Fragment Handles */
	for (i = 0; i < iprbp->iprb_nxmits; i++)
		for (j = 0; j < IPRB_MAX_FRAGS; j++)
			if (iprbp->Txbuf_info[i].frag_dma_handle[j] != NULL) {
				ddi_dma_free_handle(
				&iprbp->Txbuf_info[i].frag_dma_handle[j]);
				iprbp->Txbuf_info[i].frag_dma_handle[j] = NULL;
			}
	/* Free the Tx Buffer Descriptors */
	for (i = 0; i < iprbp->iprb_nxmits; i++)
		if (iprbp->Txbuf_info[i].tbd_acc_handle != NULL) {
			(void) ddi_dma_unbind_handle(
				iprbp->Txbuf_info[i].tbd_dma_handle);
			ddi_dma_mem_free(&iprbp->Txbuf_info[i].tbd_acc_handle);
			ddi_dma_free_handle(
				&iprbp->Txbuf_info[i].tbd_dma_handle);

			iprbp->Txbuf_info[i].tbd_dma_handle =
				iprbp->Txbuf_info[i].tbd_acc_handle =
				iprbp->Txbuf_desc[i] = NULL;
		}
	/* Free the Command Blocks (Tx) */
	for (i = 0; i < iprbp->iprb_nxmits; i++)
		if (iprbp->cmd_blk_info[i].cmd_acc_handle != NULL) {
			(void) ddi_dma_unbind_handle(
				iprbp->cmd_blk_info[i].cmd_dma_handle);
			ddi_dma_mem_free(
				&iprbp->cmd_blk_info[i].cmd_acc_handle);
			ddi_dma_free_handle(
				&iprbp->cmd_blk_info[i].cmd_dma_handle);
			iprbp->cmd_blk_info[i].cmd_acc_handle =
				iprbp->cmd_blk_info[i].cmd_dma_handle =
				iprbp->cmd_blk[i] = NULL;
		}
}

/*
 * Alloc a DMA-able data buffer and the decriptor which allow the driver to
 * track it.
 */
static struct iprb_rfd_info *
iprb_alloc_buffer(struct iprbinstance *iprbp)
{
	dev_info_t	*devinfo = iprbp->iprb_dip;
	struct iprb_rfd_info *rfd_info;
	struct iprb_rfd *rfd;
	uint_t		ncookies, how_far = 0;
	ddi_dma_cookie_t dma_cookie;
	size_t		len;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_alloc_buffer...");
#endif
	if (iprbp->rfd_cnt >= iprbp->max_rxbuf)
		return (NULL);

	if ((rfd_info = kmem_zalloc(sizeof (struct iprb_rfd_info),
				KM_NOSLEEP)) == NULL)
		return (NULL);

	if (ddi_dma_alloc_handle(devinfo, &rfd_dma_attr,
		DDI_DMA_SLEEP, 0, &rfd_info->rfd_dma_handle) != DDI_SUCCESS)
		goto fail;

	how_far++;

	/* Now Attempt to allocate the data buffer itself!! */
#ifndef	DMA_MEMORY_SCARCE
	if (ddi_dma_mem_alloc(rfd_info->rfd_dma_handle,
#else				/* DMA_MEMORY_SCARCE */
	if (iprb_dma_mem_alloc(rfd_info->rfd_dma_handle,
#endif				/* DMA_MEMORY_SCARCE */
			sizeof (struct iprb_rfd),
			&accattr, DDI_DMA_STREAMING, 0, 0,
			(caddr_t *)&rfd_info->rfd_virtaddr, &len,
			&rfd_info->rfd_acc_handle) !=
	    DDI_SUCCESS)
		goto fail;

	how_far++;

	/* Bind the address of the data buffer to a handle */
	if (ddi_dma_addr_bind_handle(rfd_info->rfd_dma_handle, NULL,
		(caddr_t)rfd_info->rfd_virtaddr, sizeof (struct iprb_rfd),
		DDI_DMA_RDWR | DDI_DMA_STREAMING, DDI_DMA_SLEEP, 0, &dma_cookie,
		&ncookies) != DDI_DMA_MAPPED)
		goto fail;

	/* Initialise RFD for Operation */
	rfd = (struct iprb_rfd *)rfd_info->rfd_virtaddr;
	bzero((caddr_t)rfd, sizeof (struct iprb_rfd));
	rfd->rfd_size = ETHERMAX;
	rfd->rfd_rbd = IPRB_NULL_PTR;

	rfd_info->rfd_physaddr = dma_cookie.dmac_address;
	rfd_info->iprbp = iprbp;
	rfd_info->free_rtn.free_func = iprb_rcv_complete;
	rfd_info->free_rtn.free_arg = (char *)rfd_info;
	iprbp->rfd_cnt++;
	return (rfd_info);

fail:
	switch (how_far) {
	case 2:		/* FALLTHROUGH */
#ifndef	DMA_MEMORY_SCARCE
		ddi_dma_mem_free(&rfd_info->rfd_acc_handle);
#else				/* DMA_MEMORY_SCARCE */
		iprb_dma_mem_free(&rfd_info->rfd_acc_handle);
#endif				/* DMA_MEMORY_SCARCE */
	case 1:		/* FALLTHROUGH */
		ddi_dma_free_handle(&rfd_info->rfd_dma_handle);
	case 0:		/* FALLTHROUGH */
		kmem_free(rfd_info, sizeof (struct iprb_rfd_info));
	default:
		return (NULL);
	}
}

/*
 * Shrink the Rx "Free List".
 */
static void
iprb_remove_buffer(struct iprbinstance *iprbp)
{
	struct iprb_rfd_info *rfd_info;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_remove_buffer...");
#endif
	if (iprbp->free_list == NULL)
		return;

	ASSERT(iprbp->free_buf_cnt > 0);
	/* Unlink buffer to be freed from free list */
	rfd_info = iprbp->free_list;
	iprbp->free_list = iprbp->free_list->next;
	iprbp->rfd_cnt--;
	iprbp->free_buf_cnt--;

	/* Kick that buffer out of here */
	(void) ddi_dma_unbind_handle(rfd_info->rfd_dma_handle);

#ifndef DMA_MEMORY_SCARCE
	ddi_dma_mem_free(&rfd_info->rfd_acc_handle);
#else				/* DMA_MEMORY_SCARCE */
	iprb_dma_mem_free(&rfd_info->rfd_acc_handle);
#endif
	ddi_dma_free_handle(&rfd_info->rfd_dma_handle);

	kmem_free(rfd_info, sizeof (struct iprb_rfd_info));
}

static struct iprb_rfd_info *
iprb_get_buffer(struct iprbinstance *iprbp)
{
	struct iprb_rfd_info *rfd_info;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_get_buffer...");
#endif
	rfd_info = iprbp->free_list;
	if (rfd_info != NULL) {
		iprbp->free_list = iprbp->free_list->next;
		iprbp->free_buf_cnt--;
	} else
		rfd_info = iprb_alloc_buffer(iprbp);

	return (rfd_info);
}

static void
iprb_rcv_complete(struct iprb_rfd_info *rfd_info)
{
	struct iprbinstance *iprbp = rfd_info->iprbp;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_rcv_complete...");
#endif
	/* One less outstanding receive buffer */
	mutex_enter(&iprbp->freelist_mutex);
	ASSERT(iprbp->rfds_outstanding > 0);
	iprbp->rfds_outstanding--;
	/* Return buffer to free list */
	rfd_info->next = iprbp->free_list;
	iprbp->free_list = rfd_info;
	iprbp->free_buf_cnt++;
	mutex_exit(&iprbp->freelist_mutex);
}

static void
iprb_release_mblks(register struct iprbinstance *iprbp)
{
	register	int	i, j;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_release_mblk...");
#endif
	for (i = 0; i < iprbp->iprb_nxmits; ++i) {
		if (iprbp->Txbuf_info[i].mp != NULL) {
			freemsg(iprbp->Txbuf_info[i].mp);
			iprbp->Txbuf_info[i].mp = NULL;
			for (j = 0; j < iprbp->Txbuf_info[i].frag_nhandles; j++)
				(void) ddi_dma_unbind_handle(
				iprbp->Txbuf_info[i].frag_dma_handle[j]);
		}
	}
}

#ifdef IPRBDEBUG
void
iprb_print_board_state(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;
	unsigned	long	scb_csr;
	unsigned	long	scb_ptr;

	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_print_board_state...");

	scb_csr = ddi_io_get32(iprbp->iohandle,
			REG32(iprbp->port, IPRB_SCB_STATUS));
	scb_ptr = ddi_io_get32(iprbp->iohandle,
			REG32(iprbp->port, IPRB_SCB_PTR));

	cmn_err(CE_WARN, "iprb : scb csr 0x%x ... scb ptr 0x%x",
		(int)scb_csr, (int)scb_ptr);
}
#endif

/*
 * Wdog Timer : Called every 2 seconds. If a receive operation has not been
 * completed within the two seconds this timer will break the RU lock
 * condition by sending a multicast setup command, which resets the RU and
 * releases it from the lock condition [ See Intel Confidential : Technical
 * Reference Manual PRO/100B 82557 ] [ 6.2 82557 B-step Errata, 6.2.2 Receive
 * Lockup ]
 */
static
void
iprb_RU_wdog(void *arg)
{
	gld_mac_info_t *macinfo = (gld_mac_info_t *)arg;
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	unsigned long	val;
	volatile struct iprb_mcs_cmd *mcmd;
	int	i, s;

	mutex_enter(&iprbp->cmdlock);
	(void) drv_getparm(LBOLT, &val);
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_RU_wdog...");
#endif
	if (val - iprbp->RUwdog_lbolt > RUTIMEOUT) {
		/* RU in Locked State , try to free it up */
		iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);

		if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
			/* command queue is full */
			iprb_reap_commands(macinfo, IPRB_REAP_ALL_CMDS);
		}
		/* Now Phy Errata implementation */
		if (iprbp->implement_errata) {
			iprb_phy_errata(iprbp);
			iprbp->implement_errata = IPRB_FALSE;
		} else {
			if ((iprbp->phy_errata_freq != 0) &&
				(iprbp->implement_errata_counter ==
				iprbp->phy_errata_freq)) {
				iprbp->implement_errata = IPRB_TRUE;
				iprbp->implement_errata_counter = 0;
			} else
				iprbp->implement_errata_counter++;
		}


		/* construct Multicast Setup command */
		/*
		 *Receive Lockup only happens at 10 Mbps. This fix is only
		 *required if card is operating at 10 Mbps, ignore this fix
		 *for 100 Mbps networks. VA edited it on March 26, 1999
		 */
		if (iprbp->speed == 10) {
			mcmd =
			&iprbp->cmd_blk[iprbp->iprb_current_cmd]->mcs_cmd;
			mcmd->mcs_cmd = IPRB_MCS_CMD;
			for (s = 0, i = 0; s < IPRB_MAXMCSN; s++) {
				if (iprbp->iprb_mcs_addrval[s] != 0) {
					bcopy((caddr_t)
					& (iprbp->iprb_mcs_addrs[s]),
					(caddr_t)
					& (mcmd->mcs_bytes[i *ETHERADDRL]),
					ETHERADDRL);
					i++;
				}
			}
			mcmd->mcs_count = i *ETHERADDRL;

			/* Send the command to the 82557 */
			iprb_add_command(macinfo);
			(void) drv_getparm(LBOLT, &iprbp->RUwdog_lbolt);
			iprbp->RU_stat_count++;
		}
	}
	if (iprbp->iprb_receive_enabled)
		iprbp->RUwdogID =
#ifdef IA64
		timeout(iprb_RU_wdog, (void *)macinfo, (clock_t)RUWDOGTICKS);
#else
		(int)(timeout(iprb_RU_wdog, (caddr_t)macinfo, RUWDOGTICKS));
#endif

	mutex_exit(&iprbp->cmdlock);
}

#ifdef DMA_MEMORY_SCARCE
/*
 * Functions to temporarily replace ddi_dma_mem_xxx() for receive buffer
 * memory as the x86 versions of ddi_dma_mem_xxx() rely on a scarce resource.
 * Remove when the x86 ddi_dma_mem functions are fixed. N.B.: This is not a
 * general purpose DMA memory allocator.  It assumes that DMA addresses are
 * physical address and that any given allocation can span only one page
 * boundry.
 */
struct dma_mem {
	caddr_t		vaddr;
	unsigned	int	length;
};

/* Maximum number of retries for contiguous memory */
#define	MAX_RETRIES 20
/* Statistics counter for number of times first try was not contiguous */
static unsigned long phys_contig_miss;
/* Statistics counters for number of times retries were not contiguous */
static unsigned long phys_contig_retry_miss[MAX_RETRIES];

/* ARGSUSED */
static
int
iprb_dma_mem_alloc(
		ddi_dma_handle_t handle,
		uint_t length,
		ddi_device_acc_attr_t *accattrp,
		ulong_t flags,
		int (*waitfp) (caddr_t),
		caddr_t arg,
		caddr_t *kaddrp,
		uint_t *real_length,
		ddi_acc_handle_t *handlep)
{
	int	cansleep;
	uint_t	start_dma_page, end_dma_page;
	int	retries;
	caddr_t	unusable_addrs[MAX_RETRIES];
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_dma_mem_alloc...");
#endif
	/* translate ddi_dma_mem_alloc sleep flag to kmem_alloc's */
	cansleep = (waitfp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP;
	/* get some memory to keep track of this allocation */
	*handlep = (ddi_acc_handle_t *)kmem_alloc(sizeof (struct dma_mem),
						cansleep);
	/* If we can't keep track, we cannot service the request */
	if (*handlep == NULL)
		return (DDI_FAILURE);

	/* Now try for the requested memory itself */
	*kaddrp = (caddr_t)kmem_alloc(length, cansleep);

	/* No? Free the tracking structure */
	if (*kaddrp == NULL) {
		kmem_free((void *)*handlep, sizeof (struct dma_mem));
		return (DDI_FAILURE);
	}
	/* If memory is physically contiguous great */
	start_dma_page = hat_getkpfnum(*kaddrp);
	end_dma_page = hat_getkpfnum(*kaddrp + length - 1);
	if ((start_dma_page == end_dma_page) ||
	    ((start_dma_page + 1) == end_dma_page)) {
		((struct dma_mem *)*handlep)->vaddr = *kaddrp;
		((struct dma_mem *)*handlep)->length = length;
		*real_length = length;
		return (DDI_SUCCESS);
	}
	++phys_contig_miss;

	/* Was not physically contiguous, try harder */
	for (retries = 0; retries < MAX_RETRIES; ++retries) {
		unusable_addrs[retries] = *kaddrp;
		*kaddrp = (caddr_t)kmem_alloc(length, cansleep);
		if (*kaddrp == NULL) {
			++retries;
			break;
		}
		/* If memory is physically contiguous great */
		start_dma_page = hat_getkpfnum(*kaddrp);
		end_dma_page = hat_getkpfnum(*kaddrp + length - 1);
		if ((start_dma_page == end_dma_page) ||
			((start_dma_page + 1) == end_dma_page)) {
			((struct dma_mem *)*handlep)->vaddr = *kaddrp;
			((struct dma_mem *)*handlep)->length = length;
			*real_length = length;
			while (retries >= 0) {
				kmem_free((void *) unusable_addrs[retries],
					length);
				--retries;
			}
			return (DDI_SUCCESS);
		}
		++phys_contig_retry_miss[retries];
	}
	while (--retries >= 0)
		kmem_free((void *) unusable_addrs[retries], length);

	if (*kaddrp != NULL)
		kmem_free((void *) *kaddrp, length);

	kmem_free((void *) *handlep, sizeof (struct dma_mem));

	return (DDI_FAILURE);
}

static
void
iprb_dma_mem_free(ddi_acc_handle_t *handlep)
{
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_dma_mem_free...");
#endif
	kmem_free((void *)((struct dma_mem *)*handlep)->vaddr,
		((struct dma_mem *)*handlep)->length);
	kmem_free((void *)*handlep, sizeof (struct dma_mem));
}
#endif				/* DMA_MEMORY_SCARCE */


/*
 * Based on the PHY dection routine in the PRO/100B tecnical reference manual
 * At startup we work out what phys are present, and when we attempt to
 * configure the device, we check what one to use
 */

static void
iprb_phyinit(gld_mac_info_t *macinfo)
{
	int	phy1addr = 0;

	struct	iprbinstance *iprbp =
	(struct	iprbinstance *)macinfo->gldm_private;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_phyinit...");
#endif
	if (mii_create(iprbp->iprb_dip, iprb_mii_writereg, iprb_mii_readreg,
		&iprbp->mii) != MII_SUCCESS) {
		cmn_err(CE_CONT, "!iprb: Cannot initialise MII instance");
		return;
	}
	/*
	 *We can have possibly two PHYs: o "logical" PHY1, which occurs at
	 *any address from 1-31 o "logical" PHY0, which occurs at address 0
	 */

	/* Find PHY1 */
	for (phy1addr = 1; phy1addr < 32; phy1addr++) {
		if (mii_probe_phy(iprbp->mii, phy1addr) == MII_SUCCESS) {
			(void) mii_init_phy(iprbp->mii, phy1addr);
			iprbp->iprb_phyaddr = phy1addr;
			break;
		}
	}

	/* ...and PHY 0 */
	if (mii_probe_phy(iprbp->mii, 0) == MII_SUCCESS) {
		(void) mii_init_phy(iprbp->mii, 0);
		iprbp->iprb_phy0exists = 1;
	} else {
		iprbp->iprb_phy0exists = 0;
	}
	/* Get info about speed and duplex settings */
	(void) mii_stop_portmon(iprbp->mii);
	iprb_force_speed_and_duplex(iprbp);
	drv_usecwait(10);
	/*
	 *I will start mii_start_portmon in iprb_start_board anyway and
	 *commenting it out also prevents one spurious mesg. which reports
	 *link lost... VA edited on Jun 20, Sunday, 1999
	 *mii_start_portmon(iprbp->mii, iprb_mii_linknotify_cb,
	 *&iprbp->cmdlock);
	 */
}

/*
 * return the address of the active MII port, or -1 if we should fall back to
 * the '503 interface
 */

static int
get_active_mii(gld_mac_info_t *macinfo)
{
	struct iprbinstance *iprbp =
	(struct iprbinstance *)(macinfo->gldm_private);
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering get_active_mii...");
#endif
	/*
	 *We choose in the following order PHY 1 if it has a link PHY 0 if
	 *it has a link PHY 1 if it exists PHY 0 if it exists The '503
	 *interface (non-mii)
	 */

	/* Check the link status on PHY 1 */
	if (iprbp->iprb_phyaddr != 0 &&
	    mii_linkup(iprbp->mii, iprbp->iprb_phyaddr))
		return (iprbp->iprb_phyaddr);

	/* PHY1 not present or no link. Try PHY0 */

	if (iprbp->iprb_phy0exists) {
		/* Is the link up? We must isolate PHY1 before checking */
		if (iprbp->iprb_phyaddr != 0) {
			(void) mii_isolate(iprbp->mii, iprbp->iprb_phyaddr);
			(void) mii_unisolate(iprbp->mii, 0);
		}
		if (mii_linkup(iprbp->mii, 0))
			return (0);
	}
	/* No PHYs had a link up. Use whatever exists */

	if (iprbp->iprb_phyaddr) {
		if (iprbp->iprb_phy0exists) {
			(void) mii_isolate(iprbp->mii, 0);
			(void) mii_unisolate(iprbp->mii, iprbp->iprb_phyaddr);
		}
		return (iprbp->iprb_phyaddr);
	}
	if (iprbp->iprb_phy0exists)
		return (0);

	/* Fallback to '503 interface */

	return (-1);
}


/* Callbacks for MII code */
static ushort_t
iprb_mii_readreg(dev_info_t *dip, int phy_addr, int reg_addr)
{
	gld_mac_info_t *macinfo =
	(gld_mac_info_t *)ddi_get_driver_private(dip);
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	ulong_t	phy_command = 0;
	ulong_t	out_data = 0;
	int	timeout = 4;

	phy_command = IPRB_MDI_READFRAME(phy_addr, reg_addr);

	ddi_io_put32(iprbp->iohandle, REG32(iprbp->port, IPRB_SCB_MDICTL),
		phy_command);

	/* max of 64 microseconds, as per PRO/100B HRM 3.3.6.3 */

	for (;;) {
		out_data = ddi_io_get32(iprbp->iohandle,
					REG32(iprbp->port, IPRB_SCB_MDICTL));
		if (out_data & IPRB_MDI_READY || timeout-- == 0)
			break;
		drv_usecwait(16);
	}
	return (out_data);
}

static void
iprb_mii_writereg(dev_info_t *dip, int phy_addr, int reg_addr, int data)
{
	gld_mac_info_t *macinfo =
	(gld_mac_info_t *)ddi_get_driver_private(dip);
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	uint32_t	command;
	int		timeout = 4;

	command = IPRB_MDI_WRITEFRAME(phy_addr, reg_addr, data);

	ddi_io_put32(iprbp->iohandle,
		REG32(iprbp->port, IPRB_SCB_MDICTL), command);

	/* Wait a max of 64 microseconds, as per note in secion 3.3.6.2 */
	while (!(ddi_io_get32(iprbp->iohandle,
		REG32(iprbp->port, IPRB_SCB_MDICTL)) &
		IPRB_MDI_READY) && timeout--)
		drv_usecwait(100);

	if (timeout == -1)
		cmn_err(CE_WARN, "!iprb: timeout writing MDI frame");
}

/* ARGSUSED */
static void
iprb_mii_linknotify_cb(dev_info_t *dip, int phy, enum mii_phy_state state)
{
	gld_mac_info_t *macinfo =
	(gld_mac_info_t *)ddi_get_driver_private(dip);
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_mii_linknotify_cb...");
#endif
	if (state != phy_state_linkup) {
		cmn_err(CE_WARN, "iprb%d: no MII link detected",
			macinfo->gldm_ppa);
		iprbp->link_status = IPRB_LINK_DOWN;
	}
	if (state == phy_state_linkup) {
		int	oldspeed = iprbp->speed;

		(void) mii_getspeed(iprbp->mii, phy, &iprbp->speed,
			&iprbp->full_duplex);
		if (oldspeed != 0)
			/* Not first link up since driver attached  */
			cmn_err(CE_NOTE, "iprb%d: %d Mbps %s-duplex link up",
				macinfo->gldm_ppa, iprbp->speed,
				iprbp->full_duplex ? "full" : "half");
		iprbp->link_status = IPRB_LINK_UP;

		/* iprb_configure works out which port to select now */
		iprb_configure(macinfo);
	}
}


/*
 * Procedure : iprb_get_eeprom_size
 *
 * Description :  This function will calculate the size of EEPROM as it can vary
 * from 82557 (64 registers) to 82558/82559 which supports 64 and 256
 * registers No of bits required to address EEPROM registers will also change
 * and it will be 6 bits for 64 registers, 7 bits for 128 registers and 8
 * bits for 256 registers.
 *
 * Arguments : iprbp 	- pointer to struct iprbinstance
 *
 * Returns : size 	- EEPROM size in bytes.
 *
 * Vinay edited on 2/2/99 to support 82558/82559 larger EEPROM support...
 */

static ushort_t
iprb_get_eeprom_size(struct iprbinstance *iprbp)
{
	ushort_t	eex;		/* This is the manipulation bit */
	ushort_t	size = 1;	/* Size must be initialized to 1 */
	/*
	 *The algorithm used to enable is dummy zero mechanism From the
	 *82558/82559 Technical Reference Manual 1.   First activate EEPROM
	 *by writing a '1' to the EECS bit 2.   Write the read opcode
	 *including the start bit (110b), one bit at a time, starting with
	 *the Msb('1'): 2.1. Write the opcode bit to the EEDI bit 2.2. Write
	 *a '1' to EESK bit and then wait for the minimum SK high time 2.3.
	 *Write a '0' to EESK bit and then wait for the minimum SK low time.
	 *2.4. Repeat steps 3 to 5 for next 2 opcode bits 3.   Write the
	 *address field, one bit at a time, keeping track of the number of
	 *bits shifted in, starting with MSB. 3.1  Write the address bit to
	 *the EEDI bit 3.2  Write a '1' to EESK bit and then wait for the
	 *minimum SK SK high time 3.3. Write a '0' to EESK bit and then wait
	 *for the minimum SK low time. 3.4  Read the EEDO bit and check for
	 *"dummy zero bit" 3.5  Repeat steps 3.1 to 3.4 unttk the EEDO bit
	 *is set to '0'. The number of loop operations performed will be
	 *equal to number of bits in the address field. 4.	  Read a 16
	 *bit word from the EEPROM one bit at a time, starting with the MSB,
	 *to complete the transaction. 4.1  Write a '1' to EESK bit and then
	 *wait for the minimum SK high time 4.2  Read data bit from the EEDO
	 *bit 4.3  Write a '0' to EESK bit and then wait for the minimum SK
	 *low time. 4.4  Repeat steps 4.1 to 4.3 for next 15 times 5.
	 *Deactivate the EEPROM by writing a 0 codeto EECS bit VINAY edited
	 *on 2/21/99 To support 82558/82559 related requirements...
	 */
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_get_eeprom_size...");
#endif
	eex = (ushort_t)ddi_io_get8(iprbp->iohandle,
		REG8(iprbp->port, IPRB_SCB_EECTL));

	eex &= ~(IPRB_EEDI | IPRB_EEDO | IPRB_EESK);
	eex |= IPRB_EECS;
	ddi_io_put8(iprbp->iohandle, REG8(iprbp->port, IPRB_SCB_EECTL), eex);
	/* Write the read opcode */
	iprb_shiftout(iprbp, IPRB_EEPROM_READ, 3);

	/*
	 *experiment to discover the size of the eeprom.  request register
	 *zero and wait for the eeprom to tell us it has accepted the entire
	 *address.
	 */

	eex = (ushort_t)ddi_io_get8(iprbp->iohandle,
		REG8(iprbp->port, IPRB_SCB_EECTL));
	do {
		size *= 2;	/* each bit of address doubles eeprom size */
		eex |= IPRB_EEDO;	/* set bit to detect "dummy zero" */
		eex &= ~IPRB_EEDI;	/* address consists of all zeros */
		ddi_io_put8(iprbp->iohandle,
			    REG8(iprbp->port, IPRB_SCB_EECTL), eex);
		drv_usecwait(100);
		iprb_raiseclock(iprbp, (uint8_t *)&eex);
		iprb_lowerclock(iprbp, (uint8_t *)&eex);

		/* check for "dummy zero" */
		eex = (ushort_t)ddi_io_get8(iprbp->iohandle,
			REG8(iprbp->port, IPRB_SCB_EECTL));
		if (size > 256) {
			size = 0;
			break;
		}
	}
	while (eex & IPRB_EEDO);

	/* read in the value requested */
	(void) iprb_shiftin(iprbp);
	iprb_eeclean(iprbp);
	iprbp->iprb_eeprom_size = size;
	return (size);
}



/*
 * Procedure: 	iprb_load_microcode
 *
 * Description: This routine downloads microcode on to the controller. This
 * microcode is available for the D101A, D101B0 and D101M. The microcode
 * reduces the number of receive interrupts by "bundling" them. The amount of
 * reduction in interrupts is configurable thru a iprb.conf file parameter
 * called CpuCycleSaver.
 *
 * Arguments: macinfo   - Pointer to gld_mac_info_t structure. revision_id
 * - Revision ID of the board.
 *
 * Returns: B_TRUE - Success B_FALSE - Failure
 *
 * Vinay: Feb 11, 1999
 */
int
iprb_load_microcode(gld_mac_info_t *macinfo, uchar_t revision_id)
{
	uint_t		i, microcode_length;
	ulong_t		d101a_microcode[] = D101_A_RCVBUNDLE_UCODE;
	/* Microcode for 82558 A Step */
	ulong_t		d101b0_microcode[] = D101_B0_RCVBUNDLE_UCODE;
	/* Microcode for 82558 B Step */
	ulong_t		d101ma_microcode[] = D101M_B_RCVBUNDLE_UCODE;
	/* Microcode for 82559 A Step */
	ulong_t		d101s_microcode[] = D101S_RCVBUNDLE_UCODE;
	/* Microcode for 82559 S */
	ulong_t		*mlong;
	ushort_t	*mshort;
	int		cpusaver_dword = 0;
	ulong_t		cpusaver_dword_val = 0;
	/*
	 *bddp = ( pbdd_t )bdp->bddp;
	 */
	struct iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;
	struct iprb_ldmc_cmd *ldmc;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_load_microcode...");
#endif

	if (iprbp->cpu_cycle_saver_dword_val == 0)
		return (GLD_SUCCESS);	/* User has disabled it */

	/* Decide which microcode to use by looking at the board's rev_id */
	if (revision_id == D101A4_REV_ID) {
		mlong = d101a_microcode;
		mshort = (ushort_t *)d101a_microcode;
		microcode_length = D101_MICROCODE_LENGTH;
		cpusaver_dword = D101_CPUSAVER_DWORD;
		cpusaver_dword_val = 0x00080600;
	} else if (revision_id == D101B0_REV_ID) {
		mlong = d101b0_microcode;
		mshort = (ushort_t *)d101b0_microcode;
		microcode_length = D101_MICROCODE_LENGTH;
		cpusaver_dword = D101_CPUSAVER_DWORD;
		cpusaver_dword_val = 0x00080600;
	} else if (revision_id == D101MA_REV_ID) {
		mlong = d101ma_microcode;
		mshort = (ushort_t *)d101ma_microcode;
		microcode_length = D101M_MICROCODE_LENGTH;
		cpusaver_dword = D101M_CPUSAVER_DWORD;
		cpusaver_dword_val = 0x00080800;
	} else if (revision_id == D101S_REV_ID) {
		mlong = d101s_microcode;
		mshort = (ushort_t *)d101s_microcode;
		microcode_length = D101S_MICROCODE_LENGTH;
		cpusaver_dword = D101S_CPUSAVER_DWORD;
		cpusaver_dword_val = 0x00080600;
	} else {
		return (GLD_FAILURE);
	/* we don't have microcode for this board */
	}

	/* Check micorcode */
	if ((cpusaver_dword != 0) &&
		(mlong[cpusaver_dword] != cpusaver_dword_val)) {
		cmn_err(CE_CONT, "iprb_load_microcode: Invalid microcode");
		cmn_err(CE_CONT, "mlong value %x and it should be %x",
			(int)mlong[cpusaver_dword], (int)cpusaver_dword_val);
		return (GLD_FAILURE);
	}
	/* Tune microcode for cpu saver */

	mshort[cpusaver_dword *2] = (iprbp->cpu_cycle_saver_dword_val < 1) ?
					0x0600 :
					((iprbp->cpu_cycle_saver_dword_val >
					0xc000) ? 0x0600 :
					iprbp->cpu_cycle_saver_dword_val);

	/* Tune microcode for rcvbundle cpu saver, only for 82559s */

	if (revision_id == D101MA_REV_ID)
		mshort[D101M_CPUSAVER_BUNDLE_MAX_DWORD *2] =
			(iprbp->cpu_saver_bundle_max_dword_val < 1) ?
			0x0006 : ((iprbp->cpu_saver_bundle_max_dword_val >
			0xffff) ?
			0x0006 : iprbp->cpu_saver_bundle_max_dword_val);

	if (revision_id == D101S_REV_ID)
		mshort[D101S_CPUSAVER_BUNDLE_MAX_DWORD *2] =
			(iprbp->cpu_saver_bundle_max_dword_val < 1) ? 0x0006 :
			((iprbp->cpu_saver_bundle_max_dword_val > 0xffff) ?
			0x0006 :
			iprbp->cpu_saver_bundle_max_dword_val);

	mutex_enter(&iprbp->cmdlock);

	/* Make available any command buffers already processed */
	iprb_reap_commands(macinfo, IPRB_REAP_COMPLETE_CMDS);

	/* Any command buffers left? */
	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
		mutex_exit(&iprbp->cmdlock);
		return (GLD_NORESOURCES);
	}
	ldmc = &iprbp->cmd_blk[iprbp->iprb_current_cmd]->ldmc_cmd;
	ldmc->ldmc_cmd = IPRB_LDMC_CMD;

	/* Copy in the microcode */
	for (i = 0; i < microcode_length; i++)
		ldmc->microcode[i] = mlong[i];

	/*
	 *Submit the Load microcode command to the chip, and wait for it to
	 *complete.
	 */
	iprb_add_command(macinfo);

	mutex_exit(&iprbp->cmdlock);
	return (GLD_SUCCESS);

}

/*
 * Procedure: 	iprb_phy_errata
 *
 * Description: This routine implements workarounds for all known Phy errata
 * (equalizer, squelch).
 *
 * Arguments: macinfo       - Pointer to the gld_mac_info_t.
 *
 * Returns: void
 *
 * Vinay: Mar 26, 1999
 */

void
iprb_phy_errata(struct iprbinstance *iprbp)
{
	/* Non Intel Phy does not need this work around */
	int	phy = -1;	/* 503 means -1 */
	/* Here phy will be passed to get phydata */
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_phy_errata...");
#endif
	if (iprbp->iprb_phyaddr)
		phy = iprbp->iprb_phyaddr;
	if (iprbp->iprb_phy0exists)
		phy = 0;

	/*
	 *If it is not an Intel phy, there is no need to implement errata,
	 *also we do force speed so it is now not okay to check just for
	 *intel's phy
	 */
	/*
	 *mii_init_phy has them burried deep in phydata structure and best
	 *way is to get it from there. There are 2 ways to get it. 1. Define
	 *one more interface function for mii and get phydata->id. 2. Expose
	 *phydata structure to iprb.c which inherently exposes the whole
	 *implementation of mii interface and will defeat the whole design
	 *purpose of havin an mii interface.
	 *
	 *Now second best option is just read it one time and store it in
	 *iprbinstance structure. This will not affect the preformance as it
	 *would be done just once for each occurance of the interface
	 *IPRB_INTEL_82555 is kept the same as INTEL_82555 i.e. 0x15 VA
	 *edited on Apr 06,99
	 */
	if (iprbp->phy_id == 0) {
		iprbp->phy_id = iprb_get_phyid(iprbp, phy);
	}
	if ((PHY_MODEL(iprbp->phy_id) != IPRB_INTEL_82555) ||
		(iprbp->speed_duplex_forced))
		return;
	/* Check the link status */
	if (iprbp->link_status == IPRB_LINK_UP) {
		switch (iprbp->phy_state) {	/* initial state is zero */
		case 0:	/* set normal state and clear delay */
			break;
		case 1:	/* clear squelch */
			iprb_mii_writereg(iprbp->iprb_dip, phy,
			IPRB_PHY_SPECIAL_CNTRL, 0x0000);
			break;
		case 2:	/* unforced EQ */
			iprb_mii_writereg(iprbp->iprb_dip, phy,
			MDI_EQUALIZER_CSR, 0x3000);
			break;
		}
		/* set normal state and clear delay */
		iprbp->phy_state = 0;
		iprbp->phy_delay = 0;

	} else if (!iprbp->phy_delay--) {	/* No Link phy_delay is 0 */
		switch (iprbp->phy_state) {	/* initial state is zero */
		case 0:	/* Set squelch bit */
			iprb_mii_writereg(iprbp->iprb_dip, phy,
			IPRB_PHY_SPECIAL_CNTRL, EXTENDED_SQUELCH_BIT);
			iprbp->phy_state = 1;
			break;
		case 1:	/* undo set squelch bit, force equalizer */
			iprb_mii_writereg(iprbp->iprb_dip, phy,
			IPRB_PHY_SPECIAL_CNTRL, 0x0000);
			iprb_mii_writereg(iprbp->iprb_dip, phy,
			MDI_EQUALIZER_CSR, 0x2010);
			iprbp->phy_state = 2;
			break;
		case 2:	/* unforced equalizer */
			iprb_mii_writereg(iprbp->iprb_dip, phy,
			MDI_EQUALIZER_CSR, 0x3000);
			iprbp->phy_state = 0;
			break;
		}
		iprb_mii_writereg(iprbp->iprb_dip, phy, MDI_CONTROL_REG,
			MDI_CR_AUTO_SELECT | MDI_CR_RESTART_AUTO_NEG);
		iprbp->phy_delay = 3;
	}
}


/*
 * Procedure: 	iprb_set_autopolarity
 *
 * Description: This routine implements workarounds for polarity Phy errata.
 * Cable hole problem.
 *
 * Arguments: iprbp       - Pointer to the iprb instance structure.
 *
 * Returns: void
 *
 * Vinay: Mar 26, 1999
 */

void
iprb_set_autopolarity(struct iprbinstance *iprbp)
{
	int		AutoPolarity = 0;
	ushort_t	iprb_errors = 0;
	ushort_t	iprb_polarity = 0;
	int		phy = -1;	/* 503 means -1 */
	/* Here phy will be passed to get phydata */
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_set_auto_polarity...");
#endif
	if (iprbp->iprb_phyaddr)
		phy = iprbp->iprb_phyaddr;
	if (iprbp->iprb_phy0exists)
		phy = 0;

	/*
	 * This fix is for auto-polarity toggle problem. With a short cable
	 * connecting an 82555 with an 840A link partner, if the medium is
	 * noisy, the 82555 sometimes presumes that the polarity might be
	 * wrong and so it toggles polarity. This happens repeatedly and
	 * results in a high bit error rate. D101 does not have this problem.
	 * This problem only happens at 10 Mbps.
	 */

	/*
	 * User can set the auto polarity through conf file. If user disables
	 * it just do that and no fixes are required to be implemented.
	 * Otherwise here are the values for auto polarity. AutoPolarity=0
	 * means disable auto polarity. AutoPolarity=1 means enable auto
	 * polarity. AutoPoalrity=2 means we will do the workaround.
	 */

	if (iprbp->auto_polarity == 0) {
		iprb_mii_writereg(iprbp->iprb_dip, phy,
				IPRB_PHY_SPECIAL_CNTRL,
				IPRB_DISABLE_AUTO_POLARITY);
	} else if (AutoPolarity >= 2) {
		/* Only required in case of 82555 intel's phy */
		if (iprbp->phy_id == 0) {
			iprbp->phy_id = iprb_get_phyid(iprbp, phy);
		}
		if (PHY_MODEL(iprbp->phy_id) == IPRB_INTEL_82555) {
			/* only required if speed is 10 Mbps */
			if (iprbp->speed == 10) {
				/* See if we have any end of frame errors */
				iprb_errors =
					iprb_mii_readreg(iprbp->iprb_dip, phy,
					IPRB_PHY_EOF_COUNTER);
				if (iprb_errors) {
					/*
					 *If there are errors wait for 100
					 *microseconds before reading again
					 */
					drv_usecwait(100);
					iprb_errors =
						iprb_mii_readreg(
						iprbp->iprb_dip,
						phy,
						IPRB_PHY_EOF_COUNTER);
					if (iprb_errors) {
						/*
						 *Errors are still there,
						 *time to disable the
						 *polarity
						 */
						iprb_mii_writereg(
						iprbp->iprb_dip, phy,
						IPRB_PHY_SPECIAL_CNTRL,
						IPRB_DISABLE_AUTO_POLARITY);
					} else {
						/* Read Polarity Again */
						iprb_polarity =
						iprb_mii_readreg(
						iprbp->iprb_dip, phy,
						IPRB_PHY_CSR);
						/*
						 *If Polarity is normal,
						 *disable it
						 */
						if (!(iprb_polarity &
						IPRB_PHY_POLARITY_BIT)) {
						iprb_mii_writereg(
						iprbp->iprb_dip,
						phy,
						IPRB_PHY_SPECIAL_CNTRL,
						IPRB_DISABLE_AUTO_POLARITY);
						} /* If Polarity is normal */
					}	/* else of iprb_errors i.e. */
						/* no errors */
				}	/* end of first if iprb_errors */
			}	/* end of speed == 10 */
		}		/* end of Intel Phy check */
	}			/* end of Polarity Check >=2 */
}

ushort_t
iprb_diag_test(gld_mac_info_t *macinfo)
{
	ushort_t	status;
	struct	iprb_diag_cmd *diag;
	struct	iprbinstance *iprbp =	/* Our private device info */
	(struct iprbinstance *)macinfo->gldm_private;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_diag_test...");
#endif
	mutex_enter(&iprbp->cmdlock);

	/* Make available any command buffers already processed */
	iprb_reap_commands(macinfo, IPRB_REAP_ALL_CMDS);

	/* Any command buffers left? */
	if (iprbp->iprb_current_cmd == iprbp->iprb_first_cmd) {
		mutex_exit(&iprbp->cmdlock);
		return (GLD_NORESOURCES);
	}
	diag = &iprbp->cmd_blk[iprbp->iprb_current_cmd]->diag_cmd;
	diag->diag_cmd = IPRB_DIAG_CMD;

	iprb_add_command(macinfo);
	drv_usecwait(5000);
	IPRB_SCBWAIT(iprbp);
	mutex_exit(&iprbp->cmdlock);

	if (((~(diag->diag_bits)) & 0x800) == 0x800) {
#ifdef IPRBDEBUG
		if (iprbdebug & IPRBTEST)
			cmn_err(CE_CONT, "iprb%d: Diagnostics Successful...",
			macinfo->gldm_ppa);
#endif
		status = GLD_SUCCESS;
	} else {
		cmn_err(CE_WARN, "iprb%d: Diagnostics failed...",
		macinfo->gldm_ppa);
		status = (ushort_t)GLD_FAILURE;
	}
	return (status);
}

int
iprb_self_test(gld_mac_info_t *macinfo)
{
	ddi_dma_handle_t dma_handle_selftest;
	volatile struct iprb_self_test_cmd *selftest;
	ddi_acc_handle_t selftest_dma_acchdl;
	uint32_t	self_test_cmd;
	size_t		len;
	int		status;
	struct iprbinstance *iprbp =
	(struct iprbinstance *)macinfo->gldm_private;
	dev_info_t	*devinfo = iprbp->iprb_dip;
	ddi_dma_cookie_t dma_cookie;
	int		i = 50;
	uint_t		ncookies;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_self_test...");
#endif
	/* This command requires 16 byte aligned address */
	/* Allocate a DMA handle for the self test buffer */
	if (ddi_dma_alloc_handle(devinfo, &selftest_buffer_dma_attr,
			DDI_DMA_SLEEP, 0,
			&dma_handle_selftest) != DDI_SUCCESS) {
		cmn_err(CE_WARN,
			"iprb%d: could not allocate self test dma handle",
			macinfo->gldm_ppa);
		status = GLD_NORESOURCES;

	} else {
		/* Now allocate memory for the self test buffer */
		if (ddi_dma_mem_alloc(dma_handle_selftest,
			sizeof (struct iprb_self_test_cmd),
			&accattr, 0, DDI_DMA_SLEEP, 0,
			(caddr_t *)&selftest, &len,
			&selftest_dma_acchdl) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
				"iprb%d: could not allocate memory "
				"for self test buffer",
				macinfo->gldm_ppa);
			ddi_dma_free_handle(&dma_handle_selftest);
			status = GLD_NORESOURCES;
		} else {
			bzero((caddr_t)selftest,
				sizeof (struct iprb_self_test_cmd));
			/*
			 * and finally get the DMA address associated with
			 * the buffer
			 */
			if (ddi_dma_addr_bind_handle(dma_handle_selftest,
				NULL, (caddr_t)selftest,
				sizeof (struct iprb_self_test_cmd),
				DDI_DMA_READ, DDI_DMA_SLEEP, 0,
				&dma_cookie, &ncookies) != DDI_DMA_MAPPED) {
				cmn_err(CE_WARN,
				"iprb%d: could not get dma address for "
				"self test buffer", macinfo->gldm_ppa);
				ddi_dma_mem_free(&selftest_dma_acchdl);
				ddi_dma_free_handle(&dma_handle_selftest);
				status = GLD_NORESOURCES;
			} else {
				ASSERT(ncookies == 1);
				self_test_cmd = dma_cookie.dmac_address;
				self_test_cmd |= IPRB_PORT_SELF_TEST;
				selftest->st_sign = 0;
				selftest->st_result = 0xffffffff;

				ddi_io_put32(iprbp->iohandle,
					REG32(iprbp->port, IPRB_SCB_PORT),
					(uint32_t)self_test_cmd);
				drv_usecwait(5000);
				/* Wait for PORT register to clear */
				do {
					drv_usecwait(10);
					if (ddi_io_get32(iprbp->iohandle,
					REG32(iprbp->port, IPRB_SCB_PORT)) == 0)
						break;
				} while (--i > 0);

				if (ddi_io_get32(iprbp->iohandle,
					REG32(iprbp->port, IPRB_SCB_PORT))
						!= 0) {
					cmn_err(CE_WARN,
					"iprb%d: Port is not clear,"
					" Self test command is not "
					" completed yet...",
					macinfo->gldm_ppa);
				}
				if ((selftest->st_sign == 0) ||
				    (selftest->st_result != 0)) {
					cmn_err(CE_WARN,
						"iprb%d: Selftest failed "
						"Sig = %x Result = %x",
						macinfo->gldm_ppa,
						selftest->st_sign,
						selftest->st_result);
					status = GLD_FAILURE;
				} else
					status = GLD_SUCCESS;
			}	/* else of ddi_dma_addr_bind_handle */
		}		/* else of ddi_dma_mem_alloc */
	}			/* else of ddi_dma_alloc_handle */
	/* Issue software reset */

	/*
	 *Port reset command should not be used under normal operation when
	 *device is active. This will reset the device unconditionally. In
	 *some cases this will hang the pci bus. First issue selective
	 *reset, wait for port register to be cleared (completion of
	 *selective reset command) and then issue a port Reset command.
	 */
	/* Memory Cleanup */

	if (status == GLD_SUCCESS)
#ifdef IPRBDEBUG
		if (iprbdebug & IPRBTEST)
			cmn_err(CE_CONT,
			"iprb%d: Selftest Successful Sig = %x Result = %x",
			macinfo->gldm_ppa,
			selftest->st_sign, selftest->st_result);
#endif
	(void) ddi_dma_unbind_handle(dma_handle_selftest);
	ddi_dma_mem_free(&selftest_dma_acchdl);
	ddi_dma_free_handle(&dma_handle_selftest);
	/* Now let us setup the device unconditionally */
	ddi_io_put32(iprbp->iohandle,
		REG32(iprbp->port, IPRB_SCB_PORT), IPRB_PORT_SW_RESET);
	drv_usecwait(50);
	/* Wait for PORT register to clear */
	i = 10;
	do {
		drv_usecwait(10);
		if (ddi_io_get32(iprbp->iohandle,
				REG32(iprbp->port, IPRB_SCB_PORT)) == 0)
			break;
	} while (--i > 0);

	if (ddi_io_get32(iprbp->iohandle,
		REG32(iprbp->port, IPRB_SCB_PORT)) != 0) {
		cmn_err(CE_WARN, "iprb%d: Port is not clear,"
			" SW reset command is not completed yet...",
			macinfo->gldm_ppa);
	}
	return (status);
}

static void
iprb_force_speed_and_duplex(struct iprbinstance *iprbp)
{
	ushort_t	control;
	int		phy = -1;	/* 503 means -1 */
	/* Here phy will be passed to get phydata */
	int		*forced_speed_and_duplex;
	uint_t		num_forced_speed_and_duplex;
	static uint_t	inst_force_speed_and_duplex[MAX_DEVICES];
	int		i = 0;
	static		instance = 0;
#ifdef IPRBDEBUG
	if (iprbdebug & IPRBTRACE)
		cmn_err(CE_WARN, "Entering iprb_force_speed_and_duplex...");
#endif
	if (instance == -1)
	/* Did not find records for the first time */
		return;
	if (iprbp->iprb_phyaddr)
		phy = iprbp->iprb_phyaddr;
	if (iprbp->iprb_phy0exists)
		phy = 0;

	if (instance == 0) {
	/* Only Fetch records for the first time call */
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, iprbp->iprb_dip,
			DDI_PROP_DONTPASS,
			"ForceSpeedDuplex",
			&forced_speed_and_duplex,
			&num_forced_speed_and_duplex) == DDI_PROP_SUCCESS) {
			for (i = 0; i <= num_forced_speed_and_duplex; i++)
				inst_force_speed_and_duplex[i] =
				forced_speed_and_duplex[i];
			ddi_prop_free(forced_speed_and_duplex);
		} else {	/* No records so there is no need to force */
			iprbp->speed_duplex_forced = IPRB_FALSE;
			instance = -1;	/* No records at the first time call */
			return;
		}
	}
	control = iprb_mii_readreg(iprbp->iprb_dip, phy, MDI_CONTROL_REG);
	control &= ~MDI_CR_AUTO_SELECT;
	if (inst_force_speed_and_duplex[instance] > 4 ||
		inst_force_speed_and_duplex[instance] < 1) {
	/*
	 *It means it is not forced as value may be 5
	 *for autonegotiation or it may be invalid
	 *VA edited on 06/28/1999
	 */
		iprbp->speed_duplex_forced = IPRB_FALSE;
		return;		/* Either invalid value of unspacified value */
	}
	switch (inst_force_speed_and_duplex[instance]) {
		/* 10 half */
	case IPRB_10_HALF:
		control &= ~MDI_CR_10_100;
		control &= ~MDI_CR_FULL_HALF;
		iprbp->speed = 10;
		iprbp->full_duplex = 0;
		break;

		/* 10 full */
	case IPRB_10_FULL:
		control &= ~MDI_CR_10_100;
		control |= MDI_CR_FULL_HALF;
		iprbp->speed = 10;
		iprbp->full_duplex = 1;
		break;

		/* 100 half */
	case IPRB_100_HALF:
		control |= MDI_CR_10_100;
		control &= ~MDI_CR_FULL_HALF;
		iprbp->speed = 100;
		iprbp->full_duplex = 0;
		break;

		/* 100 full */
	case IPRB_100_FULL:
		control |= MDI_CR_10_100;
		control |= MDI_CR_FULL_HALF;
		iprbp->speed = 100;
		iprbp->full_duplex = 1;
		break;
	}
	iprbp->speed_duplex_forced = IPRB_TRUE;
	iprb_mii_writereg(iprbp->iprb_dip, phy, MDI_CONTROL_REG, control);
	instance++;
}

static void
iprb_getprop(struct iprbinstance *iprbp)
{

	iprbp->max_rxbcopy = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "max-rxbcopy", IPRB_MAX_RXBCOPY);
	iprbp->iprb_threshold = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "xmit-threshold", IPRB_DEFAULT_THRESHOLD);
	iprbp->phy_errata_freq = ddi_getprop(DDI_DEV_T_NONE,
		iprbp->iprb_dip, DDI_PROP_DONTPASS, "PhyErrataFrequency",
		IPRB_DEFAULT_PHYERRATA_FREQUENCY);
	iprbp->iprb_nxmits = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "num-xmit-bufs", IPRB_MAX_XMITS);
	if (iprbp->iprb_nxmits < 3)
		iprbp->iprb_nxmits = IPRB_DEFAULT_XMITS;
	if (iprbp->iprb_nxmits > IPRB_MAX_XMITS)
		iprbp->iprb_nxmits = IPRB_MAX_XMITS;

	iprbp->iprb_nrecvs = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "num-recv-bufs", IPRB_DEFAULT_RECVS);
	if (iprbp->iprb_nrecvs < 2)
		iprbp->iprb_nrecvs = IPRB_DEFAULT_RECVS;
	if (iprbp->iprb_nrecvs > IPRB_MAX_RECVS)
		iprbp->iprb_nrecvs = IPRB_MAX_RECVS;
	iprbp->min_rxbuf = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "min-recv-bufs", IPRB_FREELIST_SIZE);
	/* Must have at least 'nrecvs' */
	if (iprbp->min_rxbuf < iprbp->iprb_nrecvs)
		iprbp->min_rxbuf = iprbp->iprb_nrecvs;
	iprbp->max_rxbuf = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "max-recv-bufs", IPRB_MAX_RECVS);
	if (iprbp->max_rxbuf <= iprbp->min_rxbuf)
		iprbp->max_rxbuf = IPRB_MAX_RECVS;
	iprbp->do_self_test = ddi_getprop(DDI_DEV_T_NONE,
				iprbp->iprb_dip,
				DDI_PROP_DONTPASS,
				"DoSelfTest",
				IPRB_DEFAULT_SELF_TEST);
	iprbp->do_diag_test = ddi_getprop(DDI_DEV_T_NONE,
				iprbp->iprb_dip,
				DDI_PROP_DONTPASS,
				"DoDiagnosticsTest",
				IPRB_DEFAULT_DIAGNOSTICS_TEST);
	iprbp->Aifs = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
				DDI_PROP_DONTPASS, "Adaptive-IFS", -1);
	if (iprbp->Aifs > 255)
		iprbp->Aifs = 255;
	iprbp->read_align = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "ReadAlign", IPRB_DEFAULT_READAL_ENABLE);
	iprbp->extended_txcb = ddi_getprop(DDI_DEV_T_NONE,
			iprbp->iprb_dip, DDI_PROP_DONTPASS, "ExtendedTxCB",
			IPRB_DEFAULT_EXTXCB_ENABLE);
	iprbp->tx_ur_retry = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "TxURRetry", IPRB_DEFAULT_TXURRETRY);
	iprbp->enhanced_tx_enable = ddi_getprop(DDI_DEV_T_NONE,
		iprbp->iprb_dip, DDI_PROP_DONTPASS, "EnhancedTxEnable",
		IPRB_DEFAULT_ENHANCED_TX);
	iprbp->ifs = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
		DDI_PROP_DONTPASS, "inter-frame-spacing", -1);
	if (iprbp->ifs > 15)
		iprbp->ifs = 15;
	iprbp->disable_broadcast = ddi_getprop(DDI_DEV_T_NONE,
		iprbp->iprb_dip, DDI_PROP_DONTPASS, "DisableBroadCast",
		IPRB_DEFAULT_BROADCAST_DISABLE);
	iprbp->coll_backoff = ddi_getprop(DDI_DEV_T_NONE,
		iprbp->iprb_dip, DDI_PROP_DONTPASS,
		"CollisionBackOffModification",
		IPRB_DEFAULT_WAW);
	iprbp->flow_control = ddi_getprop(DDI_DEV_T_NONE,
		iprbp->iprb_dip, DDI_PROP_DONTPASS, "FlowControl",
		IPRB_DEFAULT_FLOW_CONTROL);
	iprbp->curr_cna_backoff = ddi_getprop(DDI_DEV_T_NONE,
					iprbp->iprb_dip,
					DDI_PROP_DONTPASS,
					"CurrentCNABackoff",
					IPRB_DEFAULT_CNA_BACKOFF);
	iprbp->cpu_cycle_saver_dword_val = ddi_getprop(DDI_DEV_T_NONE,
					iprbp->iprb_dip,
					DDI_PROP_DONTPASS,
					"CpuCycleSaver",
					IPRB_DEFAULT_CPU_CYCLE_SAVER);
	iprbp->cpu_saver_bundle_max_dword_val =
					ddi_getprop(DDI_DEV_T_NONE,
					iprbp->iprb_dip,
					DDI_PROP_DONTPASS,
					"CpuSaverBundleMax",
					IPRB_DEFAULT_CPU_SAVER_BUNDLE_MAX);
	iprbp->auto_polarity = ddi_getprop(DDI_DEV_T_NONE, iprbp->iprb_dip,
					DDI_PROP_DONTPASS,
					"AutoPolarity",
					IPRB_DEFAULT_AUTO_POLARITY);
	iprbp->implement_errata = 0;
	iprbp->implement_errata_counter = 0;
}


ushort_t
iprb_get_phyid(struct iprbinstance *iprbp, int phy) {
	ushort_t id;
	id = iprb_mii_readreg(iprbp->iprb_dip, phy,
		IPRB_PHYIDH) << 16;
	id |= iprb_mii_readreg(iprbp->iprb_dip, phy,
		IPRB_PHYIDL);
	return (id);
}
