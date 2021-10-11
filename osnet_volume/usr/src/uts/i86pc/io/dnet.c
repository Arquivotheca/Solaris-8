/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dnet.c	1.39	99/08/20 SMI"

/*
 * dnet -- DEC 21x4x
 *
 * Currently supports:
 *	21040, 21041, 21140, 21142, 21143
 *	SROM versions 1, 3, 3.03
 *	TP, AUI, BNC, 100BASETX, 100BASET4
 *
 * XXX NEEDSWORK
 *	All media SHOULD work, FX is untested
 *
 * Depends on the Generic LAN Driver utility functions in /kernel/misc/gld
 */

#define	BUG_4010796	/* See 4007871, 4010796 */

#ifndef	REALMODE

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
#include <sys/debug.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/gld.h>
#include <sys/pci.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/mii.h>
#include <sys/dnet.h>

#ifdef MII_IS_MODULE
#define	MII_DEPEND " misc/mii"
#else
#define	MII_DEPEND
#endif

char _depends_on[] = "misc/gld" MII_DEPEND;

/*
 *	Declarations and Module Linkage
 */

static char ident[] = "DNET 21x4x";

#endif	/* REALMODE */

/*
#define	DNET_NOISY
#define	SROMDEBUG
#define	SROMDUMPSTRUCTURES
*/

#ifdef DNETDEBUG
#ifdef DNET_NOISY
int	dnetdebug = -1;
#else
int	dnetdebug = 0;
#endif
#endif

#ifndef REALMODE
/* used for message allocated using desballoc() */
struct free_ptr {
	struct free_rtn	free_rtn;
	uchar_t *buf;
};

/* Required system entry points */
static int dnetidentify(dev_info_t *);
static int dnetdevinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int dnetprobe(dev_info_t *);
static int dnetattach(dev_info_t *, ddi_attach_cmd_t);
static int dnetdetach(dev_info_t *, ddi_detach_cmd_t);

/* Required driver entry points for GLD */
static int dnet_reset(gld_mac_info_t *);
#endif	/* REALMODE */
static int dnet_start_board(gld_mac_info_t *);
static int dnet_stop_board(gld_mac_info_t *);
static int dnet_saddr(gld_mac_info_t *);
#ifndef REALMODE
static int dnet_dlsdmult(gld_mac_info_t *, struct ether_addr *, int);
static int dnet_prom(gld_mac_info_t *, int);
static int dnet_gstat(gld_mac_info_t *);
static int dnet_send(gld_mac_info_t *, mblk_t *);
static uint_t dnetintr(gld_mac_info_t *);
#endif	/* REALMODE */

/* Internal functions used by the above entry points */
static void write_gpr(struct dnetinstance *dnetp, ulong_t val);
static void dnet_reset_board(gld_mac_info_t *);
static void dnet_init_board(gld_mac_info_t *);
static void dnet_chip_init(gld_mac_info_t *);
static unsigned int hashindex(uchar_t *);

#ifndef REALMODE
static void dnet_getp(gld_mac_info_t *);
static void update_rx_stats(gld_mac_info_t *, int);
static void update_tx_stats(gld_mac_info_t *, int);
#endif	/* REALMODE */

/* Media Selection Setup Routines */
static void set_gpr(gld_mac_info_t *macinfo);
static void set_opr(gld_mac_info_t *macinfo);
static void set_sia(gld_mac_info_t *macinfo);

#ifndef REALMODE
/* Buffer Management Routines */
static int dnet_alloc_bufs(gld_mac_info_t *);
static void dnet_free_bufs(gld_mac_info_t *);
static void dnet_init_txrx_bufs(gld_mac_info_t *);
static int alloc_descriptor(gld_mac_info_t *);
static void dnet_reclaim_Tx_desc(gld_mac_info_t *);
static int dnet_rbuf_init(dev_info_t *, int);
static int dnet_rbuf_destroy();
static caddr_t dnet_rbuf_alloc(int);
static void dnet_rbuf_free(caddr_t);
static void dnet_freemsg_buf(struct free_ptr *);
#endif /* REALMODE */

static void setup_block(gld_mac_info_t *macinfo);

/* SROM read functions */
static int dnet_read_srom(dev_info_t, int, ddi_acc_handle_t, int, uchar_t *,
    int);
static void dnet_read21040addr(dev_info_t *, ddi_acc_handle_t, int, uchar_t *,
    int *);
static void dnet_read21140srom(ddi_acc_handle_t, int, uchar_t *, int);
static int get_alternative_srom_image(dev_info_t, uchar_t *, int);
static void dnet_print_srom(SROM_FORMAT *sr);
static void dnet_dump_leaf(LEAF_FORMAT *leaf);
static void dnet_dump_block(media_block_t *block);
#ifdef BUG_4010796
static void set_alternative_srom_image(dev_info_t, uchar_t *, int);
static int dnethack(dev_info_t *);
#endif

static int dnet_hack_interrupts(gld_mac_info_t *, int);
static int dnet_detach_hacked_interrupt(dev_info_t *devinfo);
extern uint_t gld_intr(gld_mac_info_t *macinfo);
static void enable_interrupts(struct dnetinstance *dnetp, int enable_xmit);

/* SROM parsing functions */
static void dnet_parse_srom(struct dnetinstance *dnetp, SROM_FORMAT *sr,
    uchar_t *vi);
static void parse_controller_leaf(struct dnetinstance *dnetp, LEAF_FORMAT *leaf,
    uchar_t *vi);
static uchar_t *parse_media_block(struct dnetinstance *dnetp,
    media_block_t *block, uchar_t *vi);
static int check_srom_valid(uchar_t *);
static void dnet_dumpbin(char *msg, uchar_t *, int size, int len);
static void setup_legacy_blocks();
/* Active Media Determination Routines */
static void find_active_media(gld_mac_info_t *);
static int send_test_packet(gld_mac_info_t *);
static int dnet_link_sense(gld_mac_info_t *macinfo);

/* PHY MII Routines */
static ushort_t dnet_mii_read(dev_info_t *dip, int phy_addr, int reg_num);
static void dnet_mii_write(dev_info_t *dip, int phy_addr, int reg_num,
			int reg_dat);
static void write_mii(struct dnetinstance *, ulong_t, int);
static void mii_tristate(struct dnetinstance *);
static void do_phy(gld_mac_info_t *);
static void dnet_mii_link_cb(dev_info_t *, int, enum mii_phy_state);
static void set_leaf(SROM_FORMAT *sr, LEAF_FORMAT *leaf);

#ifdef DNETDEBUG
ulong_t dnet_usecelapsed(struct dnetinstance *dnetp);
void dnet_timestamp(struct dnetinstance *, char *);
void dnet_usectimeout(struct dnetinstance *, ulong_t, int, timercb_t);
#endif
static char *media_str[] = {
	"10BaseT",
	"10Base2",
	"10Base5",
	"100BaseTX",
	"10BaseT FD",
	"100BaseTX FD",
	"100BaseT4",
	"100BaseFX",
	"100BaseFX FD",
	"MII"
};

#ifdef REALMODE
static ushort_t *realmode_gprseq = NULL;
static int realmode_gprseq_len = 0;
#endif

/* default SROM info for cards with no SROMs */
static LEAF_FORMAT leaf_default_100;
static LEAF_FORMAT leaf_asante;
static LEAF_FORMAT leaf_phylegacy;
static LEAF_FORMAT leaf_cogent_100;
static LEAF_FORMAT leaf_21041;
static LEAF_FORMAT leaf_21040;
#ifdef REALMODE
static LEAF_FORMAT realmode_leaf[7];
#endif

#ifndef	REALMODE

int rx_buf_size = (ETHERMAX + ETHERFCSL + 3) & ~3;	/* roundup to 4 */

int max_rx_desc_21040 = MAX_RX_DESC_21040;
int max_rx_desc_21140 = MAX_RX_DESC_21140;
int max_tx_desc = MAX_TX_DESC;
int dnet_xmit_threshold = MAX_TX_DESC >> 2;	/* XXX need tuning? */

static kmutex_t dnet_rbuf_lock;		/* mutex to protect rbuf_list data */

/* used for buffers allocated by ddi_dma_mem_alloc() */
static ddi_dma_attr_t dma_attr = {
	DMA_ATTR_V0,		/* dma_attr version */
	0,			/* dma_attr_addr_lo */
	(uint_t)0xFFFFFFFF,	/* dma_attr_addr_hi */
	0x7FFFFFFF,		/* dma_attr_count_max */
	4,			/* dma_attr_align */
	0x3F,			/* dma_attr_burstsizes */
	1,			/* dma_attr_minxfer */
	(uint_t)0xFFFFFFFF,	/* dma_attr_maxxfer */
	(uint_t)0xFFFFFFFF,	/* dma_attr_seg */
	1,			/* dma_attr_sgllen */
	1,			/* dma_attr_granular */
	0,			/* dma_attr_flags */
};

/* used for buffers which are NOT from ddi_dma_mem_alloc() - xmit side */
static ddi_dma_attr_t dma_attr_tx = {
	DMA_ATTR_V0,		/* dma_attr version */
	0,			/* dma_attr_addr_lo */
	(uint_t)0xFFFFFFFF,	/* dma_attr_addr_hi */
	0x7FFFFFFF,		/* dma_attr_count_max */
	1,			/* dma_attr_align */
	0x3F,			/* dma_attr_burstsizes */
	1,			/* dma_attr_minxfer */
	(uint_t)0xFFFFFFFF,	/* dma_attr_maxxfer */
	(uint_t)0xFFFFFFFF,	/* dma_attr_seg */
	0x7FFF,			/* dma_attr_sgllen */
	1,			/* dma_attr_granular */
	0,			/* dma_attr_flags */
};

static ddi_device_acc_attr_t accattr = {
	DDI_DEVICE_ATTR_V0,
	DDI_NEVERSWAP_ACC,
	DDI_STRICTORDER_ACC,
};

/* Standard Streams initialization */
static struct module_info minfo = {
	DNETIDNUM, "dnet", 0, INFPSZ, DNETHIWAT, DNETLOWAT
};

static struct qinit rinit = {	/* read queues */
	NULL, gld_rsrv, gld_open, gld_close, NULL, &minfo, NULL
};

static struct qinit winit = {	/* write queues */
	gld_wput, gld_wsrv, NULL, NULL, NULL, &minfo, NULL
};

static struct streamtab dnetinfo = {&rinit, &winit, NULL, NULL};

/* Standard Module linkage initialization for a Streams driver */
extern struct mod_ops mod_driverops;

static struct cb_ops cb_dnetops = {
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
	&dnetinfo,		/* cb_stream */
	(int)(D_MP)		/* cb_flag */
};

static struct dev_ops dnetops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	dnetdevinfo,		/* devo_getinfo */
	dnetidentify,		/* devo_identify */
	dnetprobe,		/* devo_probe */
	dnetattach,		/* devo_attach */
	dnetdetach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_dnetops,		/* devo_cb_ops */
	(struct bus_ops *)NULL	/* devo_bus_ops */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	ident,			/* short description */
	&dnetops		/* driver specific ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};


/*
 * Passed to the hacked interrupt for multiport Cogent and ZNYX cards with
 * dodgy interrupt routing
 */
#define	MAX_INST 8 /* Maximum instances on a multiport adapter. */
struct hackintr_inf
{
	gld_mac_info_t *macinfos[MAX_INST]; /* macinfos for each port */
	dev_info_t *devinfo;		    /* Devinfo of the primary device */
	kmutex_t lock;
		/* Ensures the interrupt doesn't get called while detaching */
};
static char hackintr_propname[] = "InterruptData";
static char macoffset_propname[] = "MAC_offset";
static char speed_propname[] = "speed";
static char ofloprob_propname[] = "dmaworkaround";
static char duplex_propname[] = "full-duplex"; /* Must agree with MII */
static char printsrom_propname[] = "print-srom";

static uint_t dnet_hack_intr(struct hackintr_inf *);

/*
 *	========== Module Loading Entry Points ==========
 */

int
_init(void)
{
	int i;

	/* Configure fake sroms for legacy cards */
	mutex_init(&dnet_rbuf_lock, NULL, MUTEX_DRIVER, NULL);
	setup_legacy_blocks();
	if ((i = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&dnet_rbuf_lock);
	}
	return (i);
}

int
_fini(void)
{
	int i;
	if ((i = mod_remove(&modlinkage)) == 0) { /* module can be unloaded */
		/* loop until all the receive buffers are freed */
		while (dnet_rbuf_destroy() != 0) {
			delay(drv_usectohz(100000));
#ifdef DNETDEBUG
			if (dnetdebug & DNETDDI)
				cmn_err(CE_WARN, "dnet _fini delay");
#endif
		}
		mutex_destroy(&dnet_rbuf_lock);
	}
	return (i);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 *	========== DDI Entry Points ==========
 */

/*
 * identify(9E) -- See if we know about this device
 */
/*ARGSUSED*/
static int
dnetidentify(dev_info_t *devinfo)
{
	return (DDI_IDENTIFIED);
}

/*
 * getinfo(9E) -- Get device driver information
 */
/*ARGSUSED*/
static int
dnetdevinfo(dev_info_t *devinfo, ddi_info_cmd_t cmd, void *arg, void **result)
{
	register int 	error;

	/*
	 * This code is not DDI compliant: the correct semantics
	 * for CLONE devices is not well-defined yet.
	 */
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (devinfo == NULL) {
			error = DDI_FAILURE;	/* Unfortunate */
		} else {
			*result = (void *) devinfo;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *) 0;	/* This CLONEDEV always returns zero */
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
static int
dnetprobe(dev_info_t *devinfo)
{
	ddi_acc_handle_t handle;
	short		vendorid;
	short		deviceid;

#ifdef DNETDEBUG
	if (dnetdebug & DNETDDI)
		cmn_err(CE_NOTE, "dnetprobe(0x%p)", (void *) devinfo);
#endif

	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (DDI_PROBE_FAILURE);

	vendorid = pci_config_getw(handle, PCI_CONF_VENID);

	if (vendorid != DEC_VENDOR_ID) {
		pci_config_teardown(&handle);
		return (DDI_PROBE_FAILURE);
	}

	deviceid = pci_config_getw(handle, PCI_CONF_DEVID);
	switch (deviceid) {
	case DEVICE_ID_21040:
	case DEVICE_ID_21041:
	case DEVICE_ID_21140:
	case DEVICE_ID_21143: /* And 142 */
		break;
	default:
		pci_config_teardown(&handle);
		return (DDI_PROBE_FAILURE);
	}

	pci_config_teardown(&handle);
#ifndef BUG_4010796
	return (DDI_PROBE_SUCCESS);
#else
	return (dnethack(devinfo));
#endif
}

#ifdef BUG_4010796
/*
 * If we have a device, but we cannot presently access its SROM data,
 * then we return DDI_PROBE_PARTIAL and hope that sometime later we
 * will be able to get at the SROM data.  This can only happen if we
 * are a secondary port with no SROM, and the bootstrap failed to set
 * our DNET_SROM property, and our primary sibling has not yet probed.
 */
static int
dnethack(dev_info_t *devinfo)
{
	uchar_t 		vendor_info[SROM_SIZE];
	ulong_t		csr;
	short		deviceid;
	ddi_acc_handle_t handle;
	int		retval;
	int		secondary;
	ddi_acc_handle_t io_handle;
	int		io_reg;

#define	DNET_PCI_RNUMBER	1

#ifdef DNETDEBUG
	if (dnetdebug & DNETDDI)
		cmn_err(CE_NOTE, "dnethack(0x%p)", (void *) devinfo);
#endif

	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (DDI_PROBE_FAILURE);

	deviceid = pci_config_getw(handle, PCI_CONF_DEVID);

	/*
	 * Turn on Master Enable and IO Enable bits.
	 */
	csr = pci_config_getl(handle, PCI_CONF_COMM);
	pci_config_putl(handle, PCI_CONF_COMM, (csr | PCI_COMM_ME|PCI_COMM_IO));

	pci_config_teardown(&handle);

	/* Now map I/O register */
	if (ddi_regs_map_setup(devinfo, DNET_PCI_RNUMBER,
	    (caddr_t *)&io_reg, (offset_t)0, (offset_t)0, &accattr,
	    &io_handle) != DDI_SUCCESS) {
		return (DDI_PROBE_FAILURE);
	}

	/*
	 * Reset the chip
	 */
	ddi_io_putl(io_handle, REG32(io_reg, BUS_MODE_REG), SW_RESET);
	drv_usecwait(3);
	ddi_io_putl(io_handle, REG32(io_reg, BUS_MODE_REG), 0);
	drv_usecwait(8);

	secondary = dnet_read_srom(devinfo, deviceid, io_handle,
	    io_reg, vendor_info, sizeof (vendor_info));

	switch (secondary) {
	case -1:
		/* We can't access our SROM data! */
		retval = DDI_PROBE_PARTIAL;
		break;
	case 0:
		retval = DDI_PROBE_SUCCESS;
		break;
	default:
		retval = DDI_PROBE_SUCCESS;
	}

	ddi_regs_map_free(&io_handle);
	return (retval);
}
#endif /* BUG_4010796 */

/*
 * attach(9E) -- Attach a device to the system
 *
 * Called once for each board successfully probed.
 */
static int
dnetattach(dev_info_t *devinfo, ddi_attach_cmd_t cmd)
{
	int revid;
	struct dnetinstance *dnetp;		/* Our private device info */
	gld_mac_info_t	*macinfo;		/* GLD structure */
	uchar_t 		vendor_info[SROM_SIZE];
	ulong_t		csr;
	short		deviceid;
	ddi_acc_handle_t handle;
	int		secondary;

#define	DNET_PCI_RNUMBER	1

#ifdef DNETDEBUG
	if (dnetdebug & DNETDDI)
		cmn_err(CE_NOTE, "dnetattach(0x%p)", (void *) devinfo);
#endif

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	if (pci_config_setup(devinfo, &handle) != DDI_SUCCESS)
		return (DDI_FAILURE);

	deviceid = pci_config_getw(handle, PCI_CONF_DEVID);
	switch (deviceid) {
	case DEVICE_ID_21040:
	case DEVICE_ID_21041:
	case DEVICE_ID_21140:
	case DEVICE_ID_21143: /* And 142 */
		break;
	default:
		pci_config_teardown(&handle);
		return (DDI_FAILURE);
	}

	/*
	 * Turn on Master Enable and IO Enable bits.
	 */
	csr = pci_config_getl(handle, PCI_CONF_COMM);
	pci_config_putl(handle, PCI_CONF_COMM, (csr | PCI_COMM_ME|PCI_COMM_IO));

	/* Make sure the device is not asleep */
	csr = pci_config_getl(handle, PCI_DNET_CONF_CFDD);
	pci_config_putl(handle, PCI_DNET_CONF_CFDD,
	    csr &  ~(CFDD_SLEEP|CFDD_SNOOZE));

	revid = pci_config_getb(handle, PCI_CONF_REVID);
	pci_config_teardown(&handle);

	/*
	 *	Allocate gld_mac_info_t and dnetinstance structures
	 */
	macinfo = (gld_mac_info_t *)kmem_zalloc(
			sizeof (gld_mac_info_t)+sizeof (struct dnetinstance),
			KM_SLEEP);

	dnetp = (struct dnetinstance *)(macinfo+1);

	/* Now map I/O register */
	if (ddi_regs_map_setup(devinfo, DNET_PCI_RNUMBER,
	    (caddr_t *)&dnetp->io_reg, (offset_t)0, (offset_t)0, &accattr,
	    &dnetp->io_handle) != DDI_SUCCESS) {
		kmem_free((caddr_t)macinfo,
			sizeof (gld_mac_info_t) + sizeof (struct dnetinstance));
		return (DDI_FAILURE);
	}

	dnetp->devinfo = devinfo;
	dnetp->board_type = deviceid;

	/*
	 * Initialize our private fields in macinfo and dnetinstance
	 */
	macinfo->gldm_private = (caddr_t)dnetp;
	macinfo->gldm_state = DNET_IDLE;
	macinfo->gldm_flags = 0;

	/*
	 * Initialize pointers to device specific functions which will be
	 * used by the generic layer.
	 */
	macinfo->gldm_reset	= dnet_reset;
	macinfo->gldm_start	= dnet_start_board;
	macinfo->gldm_stop	= dnet_stop_board;
	macinfo->gldm_saddr	= dnet_saddr;
	macinfo->gldm_sdmulti 	= dnet_dlsdmult;
	macinfo->gldm_prom	= dnet_prom;
	macinfo->gldm_gstat	= dnet_gstat;
	macinfo->gldm_send	= dnet_send;
	macinfo->gldm_intr	= dnetintr;
	macinfo->gldm_ioctl	= NULL;

	/*
	 *	Initialize board characteristics needed by the generic layer.
	 */
	macinfo->gldm_ident = ident;
	macinfo->gldm_type = DL_ETHER;
	macinfo->gldm_minpkt = 0;		/* assumes we pad ourselves */
	macinfo->gldm_maxpkt = DNETMAXPKT;
	macinfo->gldm_addrlen = ETHERADDRL;
	macinfo->gldm_saplen = -2;
	macinfo->gldm_media = GLDM_UNKNOWN;
	macinfo->gldm_options = GLDOPT_DONTFREE; /* don't do freemsg() */

	/*
	 * Get the BNC/TP indicator from the conf file for 21040
	 */
	dnetp->bnc_indicator =
		ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
				"bncaui", -1);

	/*
	 * For 21140 check the data rate set in the conf file. Default is
	 * 100Mb/s. Disallow connections at settings that would conflict
	 * with what's in the conf file
	 */
	dnetp->speed =
		ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
				speed_propname, 0);
	dnetp->full_duplex =
		ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
				duplex_propname, -1);

	if (dnetp->speed == 100) {
		dnetp->disallowed_media |= (1UL<<MEDIA_TP) | (1UL<<MEDIA_TP_FD);
	} else if (dnetp->speed == 10) {
		dnetp->disallowed_media |=
		    (1UL<<MEDIA_SYM_SCR) | (1UL<<MEDIA_SYM_SCR_FD);
	}

	if (dnetp->full_duplex == 1) {
		dnetp->disallowed_media |=
		    (1UL<<MEDIA_TP) | (1UL<<MEDIA_SYM_SCR);
	} else if (dnetp->full_duplex == 0) {
		dnetp->disallowed_media |=
		    (1UL<<MEDIA_TP_FD) | (1UL<<MEDIA_SYM_SCR_FD);
	}

	if (dnetp->bnc_indicator == 0) /* Disable BNC and AUI media */
		dnetp->disallowed_media |= (1UL<<MEDIA_BNC) | (1UL<<MEDIA_AUI);
	else if (dnetp->bnc_indicator == 1) /* Force BNC only */
		dnetp->disallowed_media =  ~(1UL<<MEDIA_BNC);
	else if (dnetp->bnc_indicator == 2) /* Force AUI only */
		dnetp->disallowed_media = ~(1UL<<MEDIA_AUI);

	dnet_reset_board(macinfo);

	secondary = dnet_read_srom(devinfo, dnetp->board_type, dnetp->io_handle,
	    dnetp->io_reg, vendor_info, sizeof (vendor_info));

	if (secondary == -1) /* ASSERT (vendor_info not big enough) */
		goto fail;

	dnet_parse_srom(dnetp, &dnetp->sr, vendor_info);

	if (ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
	    printsrom_propname, 0))
		dnet_print_srom(&dnetp->sr);

	dnetp->sr.netaddr[ETHERADDRL-1] += secondary;	/* unique ether addr */

	BCOPY((caddr_t)dnetp->sr.netaddr,
		(caddr_t)macinfo->gldm_vendor, ETHERADDRL);

	BCOPY((caddr_t)gldbroadcastaddr,
		(caddr_t)macinfo->gldm_broadcast, ETHERADDRL);

	BCOPY((caddr_t)dnetp->sr.netaddr,
		(caddr_t)macinfo->gldm_macaddr, ETHERADDRL);


	/*
	 * determine whether to implement workaround from DEC
	 * for DMA overrun errata.
	 */
	dnetp->overrun_workaround =
	    ((dnetp->board_type == DEVICE_ID_21140 && revid >= 0x20) ||
	    (dnetp->board_type == DEVICE_ID_21143 && revid <= 0x30)) ? 1 : 0;

	dnetp->overrun_workaround =
		ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
				ofloprob_propname, dnetp->overrun_workaround);

	/*
	 * Set reg index to -1 so gld doesn't map_reg anything for us.
	 */
	macinfo->gldm_reg_index = -1;
	macinfo->gldm_irq_index = dnet_hack_interrupts(macinfo, secondary);

	dnetp->max_tx_desc = max_tx_desc;
	dnetp->max_rx_desc = max_rx_desc_21040;
	if (dnetp->board_type != DEVICE_ID_21040 &&
	    dnetp->board_type != DEVICE_ID_21041 &&
	    dnetp->speed != 10)
		dnetp->max_rx_desc = max_rx_desc_21140;

	/* Allocate the TX and RX descriptors/buffers. */
	if (dnet_alloc_bufs(macinfo) == FAILURE) {
		cmn_err(CE_WARN, "DNET: Not enough DMA memory for buffers."
			" Add lomempages through /etc/system file");
		goto fail1;
	}

	/*
	 *	Register ourselves with the GLD interface
	 *
	 *	gld_register will:
	 *	link us with the GLD system;
	 *	set our ddi_set_driver_private(9F) data to the macinfo pointer;
	 *	save the devinfo pointer in macinfo->gldm_devinfo;
	 *	map the registers, putting the kvaddr into macinfo->gldm_memp;
	 *	add the interrupt, putting the cookie in gldm_cookie;
	 *	init the gldm_intrlock mutex which will block that interrupt;
	 *	create the minor node.
	 */

	if (gld_register(devinfo, "dnet", macinfo) == DDI_SUCCESS) {
		mutex_enter(&macinfo->gldm_maclock);
		dnetp->phyaddr = -1;
		if (dnetp->board_type == DEVICE_ID_21140 ||
		    dnetp->board_type == DEVICE_ID_21143)
			do_phy(macinfo);	/* Initialize the PHY, if any */
		find_active_media(macinfo);

		/* if the chosen media is non-MII, stop the port monitor */
		if (dnetp->selected_media_block->media_code != MEDIA_MII &&
		    dnetp->mii != NULL) {
			mii_destroy(dnetp->mii);
			dnetp->mii = NULL;
			dnetp->phyaddr = -1;
		}

#ifdef DNETDEBUG
		if (dnetdebug & DNETSENSE)
			cmn_err(CE_NOTE, "dnet: link configured : %s",
			    media_str[dnetp->selected_media_block->media_code]);
#endif
		bzero((char *)dnetp->setup_buf_vaddr, SETUPBUF_SIZE);
		dnet_reset_board(macinfo);
		dnet_init_board(macinfo);
		/* XXX function return value ignored */
		(void) dnet_stop_board(macinfo);
		mutex_exit(&macinfo->gldm_maclock);
		return (DDI_SUCCESS);
	}
fail1:
	/* XXX function return value ignored */
	(void) dnet_detach_hacked_interrupt(devinfo);
	dnet_free_bufs(macinfo);
fail:
	ddi_regs_map_free(&dnetp->io_handle);
	kmem_free((caddr_t)macinfo,
		sizeof (gld_mac_info_t)+sizeof (struct dnetinstance));
	return (DDI_FAILURE);
}

/*
 * detach(9E) -- Detach a device from the system
 */
static int
dnetdetach(dev_info_t *devinfo, ddi_detach_cmd_t cmd)
{
	int rc;
	gld_mac_info_t	*macinfo;		/* GLD structure */
	struct dnetinstance *dnetp;		/* Our private device info */
	int		proplen;

#ifdef DNETDEBUG
	if (dnetdebug & DNETDDI)
		cmn_err(CE_NOTE, "dnetdetach(0x%p)", (void *) devinfo);
#endif

	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	/* Get the driver private (gld_mac_info_t) structure */
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(devinfo);
	dnetp = (struct dnetinstance *)(macinfo->gldm_private);

	/* stop the board if it is running */
	dnet_reset_board(macinfo);

	if ((rc = dnet_detach_hacked_interrupt(devinfo)) != DDI_SUCCESS)
		return (rc);

	/*
	 *	Unregister ourselves from the GLD interface
	 *
	 *	gld_unregister will:
	 *	remove the minor node;
	 *	unmap the registers;
	 *	remove the interrupt;
	 *	destroy the gldm_intrlock mutex;
	 *	unlink us from the GLD system.
	 */
	if (gld_unregister(macinfo) != DDI_SUCCESS)
		return (DDI_FAILURE);

	if (dnetp->mii != NULL)
		mii_destroy(dnetp->mii);

	/* Free leaf information */
	set_leaf(&dnetp->sr, NULL);

	ddi_regs_map_free(&dnetp->io_handle);
	dnet_free_bufs(macinfo);
	kmem_free((caddr_t)macinfo,
		sizeof (gld_mac_info_t)+sizeof (struct dnetinstance));

#ifdef BUG_4010796
	if (ddi_getproplen(DDI_DEV_T_ANY, devinfo, 0,
	    "DNET_HACK", &proplen) != DDI_PROP_SUCCESS)
		return (DDI_SUCCESS);

	/*
	 * We must remove the properties we added, because if we leave
	 * them in the devinfo nodes and the driver is unloaded, when
	 * the driver is reloaded the info will still be there, causing
	 * nodes which had returned PROBE_PARTIAL the first time to
	 * instead return PROBE_SUCCESS, in turn causing the nodes to be
	 * attached in a different order, causing their PPA numbers to
	 * be different the second time around, which is undesirable.
	 */
	(void) ddi_prop_remove(DDI_DEV_T_NONE, devinfo, "DNET_HACK");
	(void) ddi_prop_remove(DDI_DEV_T_NONE, ddi_get_parent(devinfo),
		"DNET_SROM");
	(void) ddi_prop_remove(DDI_DEV_T_NONE, ddi_get_parent(devinfo),
		"DNET_DEVNUM");
#endif

	return (DDI_SUCCESS);
}

/*
 *	========== GLD Entry Points ==========
 */

/*
 *	dnet_reset() -- reset the board to initial state;
 */
static int
dnet_reset(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp =		/* Our private device info */
				(struct dnetinstance *)macinfo->gldm_private;
#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_reset(0x%p)", (void *) macinfo);
#endif

	/*
	 * Initialize internal data structures
	 */
	bzero((char *)dnetp->setup_buf_vaddr, SETUPBUF_SIZE);
	bzero((char *)dnetp->multicast_cnt, MCASTBUF_SIZE);
	dnetp->promisc = 0;
	dnetp->need_saddr = 0;

	dnet_reset_board(macinfo);

	if (ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo, DDI_PROP_DONTPASS,
	    "reset_do_find_active_media", 0)) {
		find_active_media(macinfo);	/* Redetermine active media */
		/* Go back to a good state */
		bzero((char *)dnetp->setup_buf_vaddr, SETUPBUF_SIZE);
		dnet_reset_board(macinfo);
	}

	dnet_init_board(macinfo);
	return (0);
}

#endif	/* REALMODE */

static void
dnet_reset_board(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp =		/* Our private device info */
				(struct dnetinstance *)macinfo->gldm_private;

	/*
	 * before initializing the dnet should be in STOP state
	 */
	/* XXX function return value ignored */
	(void) dnet_stop_board(macinfo);

	/*
	 * Reset the chip
	 */
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, INT_MASK_REG), 0);
	ddi_io_putl(dnetp->io_handle,
		REG32(dnetp->io_reg, BUS_MODE_REG), SW_RESET);
	drv_usecwait(5);
}

/*
 * dnet_init_board() -- initialize the specified network board short of
 * actually starting the board.  Call after dnet_reset_board().
 */
static void
dnet_init_board(gld_mac_info_t *macinfo)
{
#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_init_board(0x%p)", (void *) macinfo);
#endif
	set_opr(macinfo);
	set_gpr(macinfo);
	set_sia(macinfo);
	dnet_chip_init(macinfo);
}

static void
dnet_chip_init(gld_mac_info_t *macinfo)
{
	struct dnetinstance	*dnetp = (struct dnetinstance *)
					(macinfo->gldm_private);

	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, BUS_MODE_REG),
		    CACHE_ALIGN | BURST_SIZE);		/* CSR0 */

	/*
	 * Initialize the TX and RX descriptors/buffers
	 */
	dnet_init_txrx_bufs(macinfo);

	/*
	 * Set the base address of the Rx descriptor list in CSR3
	 */
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, RX_BASE_ADDR_REG),
		    (ulong_t)DNET_KVTOP(dnetp->rx_desc));

	/*
	 * Set the base address of the Tx descrptor list in CSR4
	 */
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, TX_BASE_ADDR_REG),
		    (ulong_t)DNET_KVTOP(dnetp->tx_desc));

	dnetp->tx_current_desc = dnetp->rx_current_desc = 0;
	dnetp->transmitted_desc = 0;
	dnetp->free_desc = dnetp->max_tx_desc;
	enable_interrupts(dnetp, 1);
}

/*
 *	dnet_start_board() -- start the board receiving and allow transmits.
 */
static int
dnet_start_board(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp =		/* Our private device info */
				(struct dnetinstance *)macinfo->gldm_private;
	ulong_t 		val;

#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_start_board(0x%p)", (void *) macinfo);
#endif

	/*
	 * start the board and enable receiving
	 */
	val = ddi_io_getl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG));
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG),
		    val | START_TRANSMIT);
	(void) dnet_saddr(macinfo);
	val = ddi_io_getl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG));
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG),
		    val | START_RECEIVE);
	enable_interrupts(dnetp, 1);
	return (0);
}

/*
 *	dnet_stop_board() -- stop board receiving
 */
static int
dnet_stop_board(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp =		/* Our private device info */
				(struct dnetinstance *)macinfo->gldm_private;
	ulong_t 		val;

#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_stop_board(0x%p)", (void *) macinfo);
#endif
	/*
	 * stop the board and disable transmit/receive
	 */
	val = ddi_io_getl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG));
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG),
		    val & ~(START_TRANSMIT | START_RECEIVE));
	return (0);
}

/*
 *	dnet_saddr() -- set the physical network address on the board
 */
static int
dnet_saddr(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp =		/* Our private device info */
		(struct dnetinstance *)macinfo->gldm_private;
	struct tx_desc_type *desc;
	register 	current_desc;
	unsigned int	index;
	ulong_t		*hashp;
	ulong_t 		val;

#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_saddr(0x%p)", (void *) macinfo);
#endif
	val = ddi_io_getl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG));
	if (!(val & START_TRANSMIT))
		return (0);

	current_desc = dnetp->tx_current_desc;
	desc = &dnetp->tx_desc[current_desc];

	dnetp->need_saddr = 0;

	if ((alloc_descriptor(macinfo)) == FAILURE) {
		dnetp->need_saddr = 1;
#ifdef DNETDEBUG
		if (dnetdebug & DNETTRACE)
			cmn_err(CE_WARN, "DNET saddr:alloc descriptor failure");
#endif
		return (0);
	}

	desc->buffer1 = (paddr_t)DNET_KVTOP(dnetp->setup_buf_vaddr);
	desc->buffer2			= (paddr_t)(0);
	desc->desc1.buffer_size1 	= SETUPBUF_SIZE;
	desc->desc1.buffer_size2 	= 0;
	desc->desc1.setup_packet	= 1;
	desc->desc1.first_desc		= 0;
	desc->desc1.last_desc 		= 0;
	desc->desc1.filter_type0 	= 1;
	desc->desc1.filter_type1 	= 1;
	desc->desc1.int_on_comp		= 1;

	/*
	 * As we are using Imperfect filtering, the broadcast address has to
	 * be set explicitly in the 512 bit hash table.  Hence the index into
	 * the hash table is calculated and the bit set to enable reception
	 * of broadcast packets.
	 *
	 * We also use HASH_ONLY mode, without using the perfect filter for
	 * our station address, because there appears to be a bug in the
	 * 21140 where it fails to receive the specified perfect filter
	 * address.
	 *
	 * Since dlsdmult comes through here, it doesn't matter that the count
	 * is wrong for the two bits that correspond to the cases below. The
	 * worst that could happen is that we'd leave on a bit for an old
	 * macaddr, in the case where the macaddr gets changed, which is rare.
	 * Since filtering is imperfect, it is OK if that happens.
	 */
	hashp = (ulong_t *)dnetp->setup_buf_vaddr;
	index = hashindex((uchar_t *)macinfo->gldm_broadcast);
	hashp[ index / 16 ] |= 1 << (index % 16);

	index = hashindex((uchar_t *)macinfo->gldm_macaddr);
	hashp[ index / 16 ] |= 1 << (index % 16);

	desc->desc0.own = 1;
	ddi_io_putb(dnetp->io_handle, REG8(dnetp->io_reg, TX_POLL_REG),
		    TX_POLL_DEMAND);
	return (0);
}

#ifndef	REALMODE

/*
 *	dnet_dlsdmult() -- set (enable) or disable a multicast address
 *
 *	Program the hardware to enable/disable the multicast address
 *	in "mcast".  Enable if "op" is non-zero, disable if zero.
 */
static int
dnet_dlsdmult(gld_mac_info_t *macinfo, struct ether_addr *mcast, int op)
{
	struct dnetinstance *dnetp =		/* Our private device info */
		(struct dnetinstance *)macinfo->gldm_private;
	unsigned int	index;
	ulong_t		*hashp;

#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_dlsdmult(0x%p, %s)", (void *) macinfo,
				op ? "ON" : "OFF");
#endif
	index = hashindex((uchar_t *)mcast->ether_addr_octet);
	hashp = (ulong_t *)dnetp->setup_buf_vaddr;
	if (op) {
		if (dnetp->multicast_cnt[index]++)
			return (0);
		hashp[ index / 16 ] |= 1 << (index % 16);
	} else {
		if (--dnetp->multicast_cnt[index])
			return (0);
		hashp[ index / 16 ] &= ~ (1 << (index % 16));
	}
	return (dnet_saddr(macinfo));
}

#endif	/* REALMODE */

/*
 * A hashing function used for setting the
 * node address or a multicast address
 */
static unsigned
hashindex(uchar_t *address)
{
	unsigned long	crc = (unsigned long)HASH_CRC;
	unsigned long	const POLY = HASH_POLY;
	unsigned long	msb;
	int 		byteslength;
	unsigned char 	currentbyte;
	unsigned 	index;
	int 		bit;
	int 		shift;

	for (byteslength = 0; byteslength < ETHERADDRL; byteslength++) {
		currentbyte = address[byteslength];
		for (bit = 0; bit < 8; bit++) {
			msb = crc >> 31;
			crc <<= 1;
			if (msb ^ (currentbyte & 1)) {
				crc ^= POLY;
				crc |= 0x00000001;
			}
			currentbyte >>= 1;
		}
	}

	for (index = 0, bit = 23, shift = 8;
		shift >= 0;
		bit++, shift--) {
			index |= (((crc >> bit) & 1) << shift);
	}
	return (index);
}

#ifndef	REALMODE

/*
 * dnet_prom() -- set or reset promiscuous mode on the board
 *
 *	Program the hardware to enable/disable promiscuous mode.
 *	Enable if "on" is non-zero, disable if zero.
 */

static int
dnet_prom(gld_mac_info_t *macinfo, int on)
{
	struct dnetinstance *dnetp;
	ulong_t 		val;

#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_prom(0x%p, %s)", (void *) macinfo,
				on ? "ON" : "OFF");
#endif

	dnetp = (struct dnetinstance *)macinfo->gldm_private;
	if (dnetp->promisc == on)
		return (SUCCESS);
	dnetp->promisc = on;

	val = ddi_io_getl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG));
	if (on)
		ddi_io_putl(dnetp->io_handle,
			REG32(dnetp->io_reg, OPN_MODE_REG),
			val | PROM_MODE);
	else
		ddi_io_putl(dnetp->io_handle,
			REG32(dnetp->io_reg, OPN_MODE_REG),
			val & (~PROM_MODE));
	return (DDI_SUCCESS);
}

/*
 * dnet_gstat() -- update statistics
 *
 *	GLD calls this routine just before it reads the driver's statistics
 *	structure.  If your board maintains statistics, this is the time to
 *	read them in and update the values in the structure.  If the driver
 *	maintains statistics continuously, this routine need do nothing.
 */

static int
dnet_gstat(gld_mac_info_t *macinfo)
{
#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_gstat(0x%p)", (void *) macinfo);
#endif
	return (DDI_SUCCESS);
}

/*
 *	dnet_send() -- send a packet
 *
 *	Called when a packet is ready to be transmitted. A pointer to an
 *	M_DATA message that contains the packet is passed to this routine.
 *	The complete LLC header is contained in the message's first message
 *	block, and the remainder of the packet is contained within
 *	additional M_DATA message blocks linked to the first message block.
 */

#define	NextTXIndex(index) (((index)+1) % dnetp->max_tx_desc)
#define	PrevTXIndex(index) (((index)-1) < 0 ? dnetp->max_tx_desc - 1: (index)-1)

static int
dnet_send(gld_mac_info_t *macinfo, mblk_t *mp)
{
	struct dnetinstance *dnetp =		/* Our private device info */
			(struct dnetinstance *)macinfo->gldm_private;
	register struct tx_desc_type	*ring = dnetp->tx_desc;
	int	mblen, totlen;
	int	index, end_index, start_index;
	int	avail;
	int	error;
	int	bufn;
	int	bp_count;
	int	retval;
	mblk_t	*bp;
	ulong_t	tx_interrupt_mask;

#ifdef DNETDEBUG
	if (dnetdebug & DNETSEND)
		cmn_err(CE_NOTE, "dnet_send(0x%p, 0x%p)",
			(void *) macinfo, (void *) mp);
#endif
	if (dnetp->need_saddr) {
		/* XXX function return value ignored */
		(void) dnet_saddr(macinfo);
	}

	/* reclaim any xmit descriptors completed */
	dnet_reclaim_Tx_desc(macinfo);

	/*
	 * Use the data buffers from the message and construct the
	 * scatter/gather list by calling ddi_dma_addr_bind_handle().
	 */
	error = bp_count = 0;
	totlen = 0;
	bp = mp;
	bufn = 0;
	index = start_index = dnetp->tx_current_desc;
	avail = dnetp->free_desc;
	while (bp != NULL) {
		uint_t ncookies;
		ddi_dma_cookie_t dma_cookie;

		if (++bp_count > DNET_MAX_FRAG) {
#ifndef DNET_NOISY
			(void) pullupmsg(bp, -1);
#else
			if (pullupmsg(bp, -1))
			    cmn_err(CE_NOTE, "DNET: pulled up send msg");
			else
			    cmn_err(CE_NOTE, "DNET: couldn't pullup send msg");
#endif
		}

		mblen = (int)(bp->b_wptr - bp->b_rptr);

		if (!mblen) {	/* skip zero-length message blocks */
			bp = bp->b_cont;
			continue;
		}

		retval = ddi_dma_addr_bind_handle(dnetp->dma_handle_tx, NULL,
		    (caddr_t)bp->b_rptr, mblen,
		    DDI_DMA_WRITE | DDI_DMA_STREAMING, DDI_DMA_SLEEP, 0,
		    &dma_cookie, &ncookies);

		switch (retval) {
		case DDI_DMA_MAPPED:
			break;		/* everything's fine */

		case DDI_DMA_NORESOURCES:
			error = 1;	/* allow retry by gld */
			break;

		case DDI_DMA_NOMAPPING:
		case DDI_DMA_INUSE:
		case DDI_DMA_TOOBIG:
		default:
			error = 2;	/* error, no retry */
			break;
		}

		/*
		 * we can use two cookies per descriptor (i.e buffer1 and
		 * buffer2) so we need at least (ncookies+1)/2 descriptors.
		 */
		if (((ncookies + 1) >> 1) > dnetp->free_desc) {
			(void) ddi_dma_unbind_handle(dnetp->dma_handle_tx);
			error = 1;
			break;
		}

		/* setup the descriptors for this data buffer */
		while (ncookies) {
			end_index = index;
			if (bufn % 2) {
			    ring[index].buffer2 =
				(paddr_t)dma_cookie.dmac_address;
			    ring[index].desc1.buffer_size2 =
				dma_cookie.dmac_size;
			    index = NextTXIndex(index); /* goto next desc */
			} else {
			    /* initialize the descriptor */
			    ASSERT(ring[index].desc0.own == 0);
			    *(ulong_t *)&ring[index].desc0 = 0;
			    *(ulong_t *)&ring[index].desc1 &= DNET_END_OF_RING;
			    ring[index].buffer1 =
				(paddr_t)dma_cookie.dmac_address;
			    ring[index].desc1.buffer_size1 =
				dma_cookie.dmac_size;
			    ring[index].buffer2 = (paddr_t)(0);
			    dnetp->free_desc--;
			    ASSERT(dnetp->free_desc >= 0);
			}
			totlen += dma_cookie.dmac_size;
			bufn++;
			if (--ncookies)
			    ddi_dma_nextcookie(dnetp->dma_handle_tx,
				&dma_cookie);
		}
		(void) ddi_dma_unbind_handle(dnetp->dma_handle_tx);
		bp = bp->b_cont;
	}

	if (error == 1) {
		macinfo->gldm_stats.glds_defer++;
		dnetp->free_desc = avail;
		return (GLD_TX_RESEND);
	} else if (error) {
		dnetp->free_desc = avail;
		freemsg(mp);
		return (0);	/* Drop packet, don't retry */
	}

	if (totlen > ETHERMAX) {
		cmn_err(CE_WARN, "DNET: tried to send large %d packet", totlen);
		dnetp->free_desc = avail;
		freemsg(mp);
		return (0);	/* We don't want to repeat this attempt */
	}

	/*
	 * Remeber the message buffer pointer to do freemsg() at xmit
	 * interrupt time.
	 */
	dnetp->tx_msgbufp[end_index] = mp;

	/*
	 * Now set the first/last buffer and own bits
	 * Since the 21040 looks for these bits set in the
	 * first buffer, work backwards in multiple buffers.
	 */
	ring[end_index].desc1.last_desc = 1;
	ring[end_index].desc1.int_on_comp = 1;
	for (index = end_index; index != start_index;
	    index = PrevTXIndex(index))
		ring[index].desc0.own = 1;
	ring[start_index].desc1.first_desc = 1;
	ring[start_index].desc0.own = 1;

	dnetp->tx_current_desc = NextTXIndex(end_index);

	/*
	 * Safety check: make sure end-of-ring is set in last desc.
	 */
	ASSERT(ring[dnetp->max_tx_desc-1].desc1.end_of_ring != 0);

	/*
	 * Enable xmit interrupt if we are running out of xmit descriptors
	 * or there are more packets on the queue waiting to be transmitted.
	 */
#ifdef GLD_INTR_WAIT	/* XXX This relies on new GLD changes */
	if (dnetp->free_desc <= dnet_xmit_threshold)
		tx_interrupt_mask = TX_INTERRUPT_MASK;
	else
		tx_interrupt_mask = (macinfo->gldm_GLD_flags & GLD_INTR_WAIT) ?
					TX_INTERRUPT_MASK : 0;
#else
	tx_interrupt_mask = TX_INTERRUPT_MASK;
#endif

	enable_interrupts(dnetp, tx_interrupt_mask);

	/*
	 * Kick the transmitter
	 */
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, TX_POLL_REG),
	    TX_POLL_DEMAND);
	return (GLD_TX_OK);		/* successful transmit attempt */
}

/*
 *	dnetintr() -- interrupt from board to inform us that a receive or
 *	transmit has completed.
 */


static uint_t
dnetintr(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp =	/* Our private device info */
				(struct dnetinstance *)macinfo->gldm_private;
	register ulong_t		int_status;
	ulong_t			tx_interrupt_mask;

#ifdef DNETDEBUG
	if (dnetdebug & DNETINT)
		cmn_err(CE_NOTE, "dnetintr(0x%p)", (void *)macinfo);
#endif
	int_status = ddi_io_getl(dnetp->io_handle, REG32(dnetp->io_reg,
			STATUS_REG));

	/*
	 * If interrupt was not from this board
	 */
	if (!(int_status & (NORMAL_INTR_SUMM | ABNORMAL_INTR_SUMM))) {
		return (DDI_INTR_UNCLAIMED);
	}

	macinfo->gldm_stats.glds_intr++;

	if (int_status & GPTIMER_INTR) {
		ddi_io_putl(dnetp->io_handle,
			    REG32(dnetp->io_reg, STATUS_REG), GPTIMER_INTR);
		if (dnetp->timer.cb)
			dnetp->timer.cb(dnetp);
		else
			cmn_err(CE_WARN, "dnet: unhandled timer interrupt");
	}

	if (int_status & TX_INTR) {
		ddi_io_putl(dnetp->io_handle,
			    REG32(dnetp->io_reg, STATUS_REG), TX_INTR);
	}

	/* reclaim any xmit descriptors that are completed */
	dnet_reclaim_Tx_desc(macinfo);

	/*
	 * Check if receive interrupt bit is set
	 */
	if (int_status & RX_INTR) {
		ddi_io_putl(dnetp->io_handle,
			    REG32(dnetp->io_reg, STATUS_REG), RX_INTR);
		dnet_getp(macinfo);
	}

	if (int_status & ABNORMAL_INTR_SUMM) {
		/*
		 * Check for system error
		 */
		if (int_status & SYS_ERR) {
			if ((int_status & SYS_ERR_BITS) == MASTER_ABORT)
				cmn_err(CE_WARN, "DNET: Bus Master Abort");
			if ((int_status & SYS_ERR_BITS) == TARGET_ABORT)
				cmn_err(CE_WARN, "DNET: Bus Target Abort");
			if ((int_status & SYS_ERR_BITS) == PARITY_ERROR)
				cmn_err(CE_WARN, "DNET: Parity error");
		}

		/*
		 * If the jabber has timed out then reset the chip
		 */
		if (int_status & TX_JABBER_TIMEOUT)
			cmn_err(CE_WARN, "DNET: Jabber timeout.");

		/*
		 * If an underflow has occurred, reset the chip
		 */
		if (int_status & TX_UNDERFLOW)
			cmn_err(CE_WARN, "DNET: Tx Underflow.");

#ifdef DNETDEBUG
		if (dnetdebug & DNETINT)
			cmn_err(CE_NOTE, "Trying to reset...");
#endif
		dnet_reset_board(macinfo);
		dnet_init_board(macinfo);
		/* XXX function return value ignored */
		(void) dnet_start_board(macinfo);
	}

	/*
	 * Enable the interrupts. Enable xmit interrupt only if we are
	 * running out of free descriptors or if there are packets
	 * in the queue waiting to be transmitted.
	 */
#ifdef GLD_INTR_WAIT	/* XXX This relies on new GLD changes */
	if (dnetp->free_desc <= dnet_xmit_threshold)
		tx_interrupt_mask = TX_INTERRUPT_MASK;
	else
		tx_interrupt_mask = (macinfo->gldm_GLD_flags & GLD_INTR_WAIT) ?
					TX_INTERRUPT_MASK : 0;
#else
	tx_interrupt_mask = TX_INTERRUPT_MASK;
#endif
	enable_interrupts(dnetp, tx_interrupt_mask);
	return (DDI_INTR_CLAIMED);	/* Indicate it was our interrupt */
}

static void
dnet_getp(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp =
			(struct dnetinstance *)macinfo->gldm_private;
	register int packet_length, index;
	mblk_t	*mp;
	uchar_t 	*virtual_address;
	struct	rx_desc_type *desc = dnetp->rx_desc;
	extern mblk_t *desballoc(unsigned char *, size_t, uint_t, frtn_t *);
	int marker = dnetp->rx_current_desc;
	int misses;

#ifdef DNETDEBUG
	if (dnetdebug & DNETRECV)
		cmn_err(CE_NOTE, "dnet_getp(0x%p)", (void *)macinfo);
#endif

	if (!dnetp->overrun_workaround) {
		/*
		 * If the workaround is not in place, we must still update
		 * the missed frame statistic from the on-chip counter.
		 */
		misses = ddi_io_getl(dnetp->io_handle,
		    REG32(dnetp->io_reg, MISSED_FRAME_REG));
		macinfo->gldm_stats.glds_missed += (misses & MISSED_FRAME_MASK);
	}

	/* While host owns the current descriptor */
	while (!(desc[dnetp->rx_current_desc].desc0.own)) {
		struct free_ptr *frp;
		uchar_t *newbuf;

		index = dnetp->rx_current_desc;
		ASSERT(desc[index].desc0.first_desc != 0);

#ifndef REALMODE
		/*
		 * DMA overrun errata from DEC: avoid possible bus hangs
		 * and data corruption
		 */
		if (dnetp->overrun_workaround &&
		    marker == dnetp->rx_current_desc) {
			int opn;
			do {
				marker = (marker+1) % dnetp->max_rx_desc;
			} while (!(dnetp->rx_desc[marker].desc0.own) &&
			    marker != index);

			misses = ddi_io_getl(dnetp->io_handle,
			    REG32(dnetp->io_reg, MISSED_FRAME_REG));
			macinfo->gldm_stats.glds_missed +=
			    (misses & MISSED_FRAME_MASK);
			if (misses & OVERFLOW_COUNTER_MASK) {
				/*
				 * Overflow(s) have occurred : stop receiver,
				 * and wait until in stopped state
				 */
				opn = ddi_io_getl(dnetp->io_handle,
				    REG32(dnetp->io_reg, OPN_MODE_REG));
				ddi_io_putl(dnetp->io_handle,
				    REG32(dnetp->io_reg, OPN_MODE_REG),
				    opn & ~(START_RECEIVE));

				do {
					drv_usecwait(10);
				} while ((ddi_io_getl(dnetp->io_handle,
				    REG32(dnetp->io_reg, STATUS_REG)) &
				    RECEIVE_PROCESS_STATE) != 0);
#ifdef DNETDEBUG
				if (dnetdebug & DNETRECV)
					cmn_err(CE_CONT, "^*");
#endif
				/* Discard probably corrupt frames */
				while (!(dnetp->rx_desc[index].desc0.own)) {
					dnetp->rx_desc[index].desc0.own = 1;
					index = (index+1) % dnetp->max_rx_desc;
					macinfo->gldm_stats.glds_missed++;
				}

				/* restart the receiver */
				opn = ddi_io_getl(dnetp->io_handle,
				    REG32(dnetp->io_reg, OPN_MODE_REG));
				ddi_io_putl(dnetp->io_handle,
				    REG32(dnetp->io_reg, OPN_MODE_REG),
				    opn | START_RECEIVE);
				marker = dnetp->rx_current_desc = index;
				continue;
			}
			/*
			 * At this point, we know that all packets before
			 * "marker" were received before a dma overrun occurred
			 */
		}
#endif

		/*
		 * If we get an oversized packet it could span multiple
		 * descriptors.  If this happens an error bit should be set.
		 */
		while (desc[index].desc0.last_desc == 0) {
			index = (index + 1) % dnetp->max_rx_desc;
			if (desc[index].desc0.own)
				return;	/* not done receiving large packet */
		}
		while (dnetp->rx_current_desc != index) {
			desc[dnetp->rx_current_desc].desc0.own = 1;
			dnetp->rx_current_desc =
			    (dnetp->rx_current_desc + 1) % dnetp->max_rx_desc;
#ifdef DNETDEBUG
			if (dnetdebug & DNETRECV)
				cmn_err(CE_WARN, "dnet: received large packet");
#endif
		}

		packet_length = desc[index].desc0.frame_len;
#ifdef DNETDEBUG
		if (dnetdebug & DNETRECV) {
			if (packet_length > ETHERMAX + ETHERFCSL)
				cmn_err(CE_WARN, "dnet: large packet size %d",
				    packet_length);
		}
#endif

		/* get the virtual address of the packet received */
		virtual_address =
		    dnetp->rx_buf_vaddr[index];

		/*
		 * If no packet errors then do:
		 * 	1. Allocate a new receive buffer so that we can
		 *	   use the current buffer as streams buffer to
		 *	   avoid bcopy.
		 *	2. If we got a new receive buffer then allocate
		 *	   an mblk using desballoc().
		 *	3. Otherwise use the mblk from allocb() and do
		 *	   the bcopy.
		 * XXX_NOTE: desballoc() is not a DDI function?
		 */
		frp = NULL;
		newbuf = NULL;
		mp = NULL;
		if (!desc[index].desc0.err_summary) {
			ASSERT(packet_length < rx_buf_size);
			/*
			 * Allocate another receive buffer for this descriptor.
			 * If we fail to allocate then we do the normal bcopy.
			 */
			newbuf = (uchar_t *)dnet_rbuf_alloc(0);
			if (newbuf != NULL) {
				frp = (struct free_ptr *)
					kmem_zalloc(sizeof (*frp), KM_NOSLEEP);
				if (frp != NULL) {
				    frp->free_rtn.free_func = dnet_freemsg_buf;
				    frp->free_rtn.free_arg = (char *)frp;
				    frp->buf = virtual_address;
				    mp = desballoc(virtual_address,
					packet_length, 0, &frp->free_rtn);
				    if (mp == NULL) {
					kmem_free(frp, sizeof (*frp));
					dnet_rbuf_free((caddr_t)newbuf);
					frp = NULL;
					newbuf = NULL;
				    }
				}
			}
			if (mp == NULL) {
				if (newbuf != NULL)
					dnet_rbuf_free((caddr_t)newbuf);
				mp = allocb(packet_length, 0);
			}
		}

		if (desc[index].desc0.err_summary || (mp == NULL)) {

			/* Update gld statistics */
			if (desc[index].desc0.err_summary)
				update_rx_stats(macinfo, index);
			else
				macinfo->gldm_stats.glds_norcvbuf++;

			/*
			 * Reset ownership of the descriptor.
			 */
			desc[index].desc0.own = 1;
			dnetp->rx_current_desc =
			    (dnetp->rx_current_desc+1) % dnetp->max_rx_desc;

			/* Demand receive polling by the chip */
			ddi_io_putl(dnetp->io_handle,
			    REG32(dnetp->io_reg, RX_POLL_REG), RX_POLL_DEMAND);

			continue;
		}

		if (newbuf != NULL) {
#ifndef CONTIG_RX_BUFS
			paddr_t end_paddr;
#endif
			/* attach the new buffer to the rx descriptor */
			dnetp->rx_buf_vaddr[index] = newbuf;
			desc[index].buffer1 = (paddr_t)DNET_KVTOP(newbuf);
#ifndef CONTIG_RX_BUFS
			desc[index].desc1.buffer_size1 = rx_buf_size;
			desc[index].desc1.buffer_size2 = 0;
			end_paddr = (paddr_t)DNET_KVTOP(newbuf+rx_buf_size-1);
			if ((desc[index].buffer1 & ~dnetp->pgmask) !=
			    (end_paddr & ~dnetp->pgmask)) {
				/* discontiguous */
				desc[index].buffer2 = end_paddr&~dnetp->pgmask;
				desc[index].desc1.buffer_size2 =
				    (end_paddr & dnetp->pgmask) + 1;
				desc[index].desc1.buffer_size1 =
				    rx_buf_size-desc[index].desc1.buffer_size2;
			}
#endif
		} else {
		    /* couldn't allocate another buffer; copy the data */
		    BCOPY((caddr_t)virtual_address, (caddr_t)mp->b_wptr,
			packet_length);
		}

		mp->b_wptr += packet_length;

		desc[dnetp->rx_current_desc].desc0.own = 1;

		/*
		 * Increment receive desc index. This is for the scan of
		 * next packet
		 */
		dnetp->rx_current_desc =
			(dnetp->rx_current_desc+1) % dnetp->max_rx_desc;

		/* Demand polling by chip */
		ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, RX_POLL_REG),
			    RX_POLL_DEMAND);

		/* send the packet upstream */
		gld_recv(macinfo, mp);
	}
}
/*
 * Function to update receive statistics
 */
static void
update_rx_stats(register gld_mac_info_t *macinfo, int index)
{
	struct	dnetinstance	*dnetp =
			(struct dnetinstance *)macinfo->gldm_private;
	register struct rx_desc_type *descp = &(dnetp->rx_desc[index]);

	/*
	 * Update gld statistics
	 */
	macinfo->gldm_stats.glds_errrcv++;

	if (descp->desc0.overflow)	{
		/* FIFO Overrun */
		macinfo->gldm_stats.glds_overflow++;
	}

	if (descp->desc0.collision) {
		/*EMPTY*/
		/* Late Colllision on receive */
		/* no appropriate counter */
	}

	if (descp->desc0.crc) {
		/* CRC Error */
		macinfo->gldm_stats.glds_crc++;
	}

	if (descp->desc0.runt_frame) {
		/* Runt Error */
		macinfo->gldm_stats.glds_short++;
	}

	if (descp->desc0.desc_err) {
		/*EMPTY*/
		/* Not enough receive descriptors */
		/* This condition is accounted in dnetintr() */
		/* macinfo->gldm_stats.glds_missed++; */
	}

	if (descp->desc0.frame2long) {
		macinfo->gldm_stats.glds_frame++;
	}
}

/*
 * Function to update transmit statistics
 */
static void
update_tx_stats(gld_mac_info_t *macinfo, int index)
{
	struct	dnetinstance	*dnetp =
			(struct dnetinstance *)macinfo->gldm_private;
	register struct tx_desc_type *descp = &(dnetp->tx_desc[index]);


	/* Update gld statistics */
	macinfo->gldm_stats.glds_errxmt++;

	if (descp->desc0.collision_count) {
		macinfo->gldm_stats.glds_collisions +=
			descp->desc0.collision_count;
	}

	if (descp->desc0.late_collision) {
		macinfo->gldm_stats.glds_xmtlatecoll++;
	}

	if (descp->desc0.excess_collision) {
		macinfo->gldm_stats.glds_excoll++;
	}

	if (descp->desc0.underflow) {
		macinfo->gldm_stats.glds_underflow++;
	}

#if 0
	if (descp->desc0.tx_jabber_to) {
		/* no appropriate counter */
	}
#endif

	if (descp->desc0.carrier_loss) {
		macinfo->gldm_stats.glds_nocarrier++;
	}

	if (descp->desc0.no_carrier) {
		macinfo->gldm_stats.glds_nocarrier++;
	}
}

#endif	/* REALMODE */

/*
 *	========== Media Selection Setup Routines ==========
 */


static void
write_gpr(struct dnetinstance *dnetp, ulong_t val)
{
#ifdef DEBUG
	if (dnetdebug & DNETREGCFG)
		cmn_err(CE_NOTE, "GPR: %lx", val);
#endif
	switch (dnetp->board_type) {
	case DEVICE_ID_21143:
		/* Set the correct bit for a control write */
		if (val & GPR_CONTROL_WRITE)
			val |= CWE_21143, val &= ~GPR_CONTROL_WRITE;
		/* Write to upper half of CSR15 */
		dnetp->gprsia = (dnetp->gprsia & 0xffff) | (val << 16);
		ddi_io_putl(dnetp->io_handle,
			REG32(dnetp->io_reg, SIA_GENERAL_REG),
			dnetp->gprsia);
		break;
	default:
		/* Set the correct bit for a control write */
		if (val & GPR_CONTROL_WRITE)
			val |= CWE_21140, val &= ~GPR_CONTROL_WRITE;
		ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, GP_REG),
			val);
		break;
	}
}

static ulong_t
read_gpr(struct dnetinstance *dnetp)
{
	switch (dnetp->board_type) {
	case DEVICE_ID_21143:
		/* Read upper half of CSR15 */
		return (ddi_io_getl(dnetp->io_handle,
			REG32(dnetp->io_reg, SIA_GENERAL_REG)) >> 16);
	default:
		return (ddi_io_getl(dnetp->io_handle,
				REG32(dnetp->io_reg, GP_REG)));
	}
}

static void
set_gpr(gld_mac_info_t *macinfo)
{
	ulong_t *sequence;
	int len;
	struct dnetinstance *dnetp = (struct dnetinstance *)
					macinfo->gldm_private;
	LEAF_FORMAT *leaf = &dnetp->sr.leaf[dnetp->leaf];
	media_block_t *block = dnetp->selected_media_block;
	int i;

#ifdef REALMODE
	if (realmode_gprseq) {
		for (i = 0; i < realmode_gprseq_len; i++)
			write_gpr(dnetp, realmode_gprseq[i]);
	}
#else
	if (ddi_getlongprop(DDI_DEV_T_ANY, dnetp->devinfo,
	    DDI_PROP_DONTPASS, "gpr-sequence", (caddr_t)&sequence,
	    &len) == DDI_PROP_SUCCESS) {
		for (i = 0; i < len / sizeof (ulong_t); i++)
			write_gpr(dnetp, sequence[i]);
		kmem_free(sequence, len);
	}
#endif
	else {
		/*
		 * Write the reset sequence if this is the first time this
		 * block has been selected.
		 */
		if (block->rstseqlen) {
			for (i = 0; i < block->rstseqlen; i++)
				write_gpr(dnetp, block->rstseq[i]);
			/*
			 * XXX Legacy blocks do not have reset sequences, so the
			 * static blocks will never be modified by this
			 */
			block->rstseqlen = 0;
		}
		if (leaf->gpr)
			write_gpr(dnetp, leaf->gpr | GPR_CONTROL_WRITE);

		/* write GPR sequence each time */
		for (i = 0; i < block->gprseqlen; i++)
			write_gpr(dnetp, block->gprseq[i]);
	}

	/* This has possibly caused a PHY to reset.  Let MII know */
	if (dnetp->phyaddr != -1)
		/* XXX function return value ignored */
		(void) mii_sync(dnetp->mii, dnetp->phyaddr);
	drv_usecwait(5);
}

static void
set_opr(gld_mac_info_t *macinfo)
{
	ulong_t fd, mb1, sf;

	struct dnetinstance *dnetp =
		(struct dnetinstance *)macinfo->gldm_private;
	int 		opnmode_len;
	ulong_t val;
	media_block_t *block = dnetp->selected_media_block;

	ASSERT(block);

	/* Check for custom "opnmode_reg" property */
	opnmode_len = sizeof (val);
	if (ddi_prop_op(DDI_DEV_T_ANY, dnetp->devinfo,
	    PROP_LEN_AND_VAL_BUF, DDI_PROP_DONTPASS, "opnmode_reg",
	    (caddr_t)&val, &opnmode_len) != DDI_PROP_SUCCESS)
		opnmode_len = 0;

	/* Some bits exist only on 21140 and greater */
	if (dnetp->board_type != DEVICE_ID_21040 &&
	    dnetp->board_type != DEVICE_ID_21041) {
		mb1 = OPN_REG_MB1;
		sf = STORE_AND_FORWARD;
	} else {
		mb1 = sf = 0;
		mb1 = OPN_REG_MB1; /* Needed for 21040? */
	}

	if (opnmode_len) {
		ddi_io_putl(dnetp->io_handle,
			REG32(dnetp->io_reg, OPN_MODE_REG), val);
		dnet_reset_board(macinfo);
		ddi_io_putl(dnetp->io_handle,
			REG32(dnetp->io_reg, OPN_MODE_REG), val);
		return;
	}

	/*
	 * Set each bit in CSR6 that we want
	 */

	/* Always want these bits set */
	val = HASH_FILTERING | HASH_ONLY | TX_THRESHOLD_160 | mb1 | sf;

	/* Promiscuous mode */
	val |= dnetp->promisc ? PROM_MODE : 0;

	/* Scrambler for SYM style media */
	val |= ((block->command & CMD_SCR) && !dnetp->disable_scrambler) ?
		SCRAMBLER_MODE : 0;

	/* Full duplex */
	if (dnetp->mii_up) {
		fd = dnetp->mii_duplex;
	} else {
		/* Rely on media code */
		fd =
		    block->media_code == MEDIA_TP_FD ||
		    block->media_code == MEDIA_SYM_SCR_FD;
	}

	/* Port select (and therefore, heartbeat disable) */
	val |= block->command & CMD_PS ? (PORT_SELECT | HEARTBEAT_DISABLE) : 0;

	/* PCS function */
	val |= (block->command) & CMD_PCS ? PCS_FUNCTION : 0;
	val |= fd ? FULL_DUPLEX : 0;

#ifdef DNETDEBUG
	if (dnetdebug & DNETREGCFG)
		cmn_err(CE_NOTE, "OPN: %lx", val);
#endif
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG), val);
	dnet_reset_board(macinfo);
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, OPN_MODE_REG), val);
}

static void
set_sia(gld_mac_info_t *macinfo)
{
	struct dnetinstance *dnetp = (struct dnetinstance *)
	    (macinfo->gldm_private);
	media_block_t *block = dnetp->selected_media_block;

	if (block->type == 2) {
		int sia_delay;
#ifdef DNETDEBUG
		if (dnetdebug & DNETREGCFG)
			cmn_err(CE_NOTE,
			    "SIA: CSR13: %lx, CSR14: %lx, CSR15: %lx",
			    block->un.sia.csr13,
			    block->un.sia.csr14,
			    block->un.sia.csr15);
#endif
		sia_delay = ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
		    DDI_PROP_DONTPASS, "sia-delay", 10000);

		ddi_io_putl(dnetp->io_handle,
			REG32(dnetp->io_reg, SIA_CONNECT_REG), 0);

		ddi_io_putl(dnetp->io_handle,
			REG32(dnetp->io_reg, SIA_TXRX_REG),
		    block->un.sia.csr14);

		/*
		 * For '143, we need to write through a copy of the register
		 * to keep the GP half intact
		 */
		dnetp->gprsia = (dnetp->gprsia&0xffff0000)|block->un.sia.csr15;
		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, SIA_GENERAL_REG),
		    dnetp->gprsia);

		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, SIA_CONNECT_REG),
		    block->un.sia.csr13);

		drv_usecwait(sia_delay);

	} else if (dnetp->board_type != DEVICE_ID_21140) {
		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, SIA_CONNECT_REG), 0);
		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, SIA_TXRX_REG), 0);
	}
}

#ifndef	REALMODE

/*
 *	========== Buffer Management Routines ==========
 */

/*
 * This function (re)allocates the receive and transmit buffers and
 * descriptors.  It can be called more than once per instance, though
 * currently it is only called from attach.  It should only be called
 * while the device is reset.
 */
static int
dnet_alloc_bufs(gld_mac_info_t *macinfo)
{
	struct dnetinstance	*dnetp = (struct dnetinstance *)
						(macinfo->gldm_private);
	int i;
	size_t len;
	int page_size;
	int realloc = 0;
	int nrecv_desc_old = 0;

	/*
	 * check if we are trying to reallocate with different xmit/recv
	 * descriptor ring sizes.
	 */
	if ((dnetp->tx_desc != NULL) &&
	    (dnetp->nxmit_desc != dnetp->max_tx_desc))
		realloc = 1;

	if ((dnetp->rx_desc != NULL) &&
	    (dnetp->nrecv_desc != dnetp->max_rx_desc))
		realloc = 1;

	/* free up the old buffers if we are reallocating them */
	if (realloc) {
		nrecv_desc_old = dnetp->nrecv_desc;
		dnet_free_bufs(macinfo); /* free the old buffers */
	}

	if (dnetp->dma_handle == NULL)
		/* XXX function return value ignored */
		(void) ddi_dma_alloc_handle(dnetp->devinfo, &dma_attr,
		    DDI_DMA_SLEEP, 0, &dnetp->dma_handle);

	if (dnetp->dma_handle_tx == NULL)
		/* XXX function return value ignored */
		(void) ddi_dma_alloc_handle(dnetp->devinfo, &dma_attr_tx,
		    DDI_DMA_SLEEP, 0, &dnetp->dma_handle_tx);

	page_size = ddi_ptob(dnetp->devinfo, 1);
	for (i = page_size, len = 0; i > 1; len++)
		i >>= 1;
	dnetp->pgmask = page_size - 1;
	dnetp->pgshft = len;

	/* allocate setup buffer if necessary */
	if (dnetp->setup_buf_vaddr == NULL) {
		if (ddi_dma_mem_alloc(dnetp->dma_handle,
		    SETUPBUF_SIZE, &accattr, DDI_DMA_STREAMING,
		    DDI_DMA_DONTWAIT, 0, (caddr_t *)&dnetp->setup_buf_vaddr,
		    &len, &dnetp->setup_buf_acchdl) != DDI_SUCCESS)
			return (FAILURE);
		bzero((char *)dnetp->setup_buf_vaddr, len);
	}

	/* allocate xmit descriptor array of size dnetp->max_tx_desc */
	if (dnetp->tx_desc == NULL) {
		if (ddi_dma_mem_alloc(dnetp->dma_handle,
		    sizeof (struct tx_desc_type) * dnetp->max_tx_desc,
		    &accattr, DDI_DMA_STREAMING, DDI_DMA_DONTWAIT, 0,
		    (caddr_t *)&dnetp->tx_desc, &len,
		    &dnetp->tx_desc_acchdl) != DDI_SUCCESS)
			return (FAILURE);
		bzero((char *)dnetp->tx_desc, len);
		dnetp->nxmit_desc = dnetp->max_tx_desc;

		dnetp->tx_msgbufp =
			(mblk_t **)kmem_zalloc(dnetp->max_tx_desc *
					sizeof (mblk_t **), KM_SLEEP);
	}

	/* allocate receive descriptor array of size dnetp->max_rx_desc */
	if (dnetp->rx_desc == NULL) {
		int ndesc;

		if (ddi_dma_mem_alloc(dnetp->dma_handle,
		    sizeof (struct rx_desc_type) * dnetp->max_rx_desc,
		    &accattr, DDI_DMA_STREAMING, DDI_DMA_DONTWAIT, 0,
		    (caddr_t *)&dnetp->rx_desc, &len,
		    &dnetp->rx_desc_acchdl) != DDI_SUCCESS)
			return (FAILURE);
		bzero((char *)dnetp->rx_desc, len);
		dnetp->nrecv_desc = dnetp->max_rx_desc;

		dnetp->rx_buf_vaddr =
			(uchar_t **)kmem_zalloc(dnetp->max_rx_desc *
					sizeof (uchar_t **), KM_SLEEP);
		/*
		 * Allocate or add to the pool of receive buffers.  The pool
		 * is shared among all instances of dnet.
		 *
		 * XXX NEEDSWORK
		 *
		 * We arbitrarily allocate twice as many receive buffers as
		 * receive descriptors because we use the buffers for streams
		 * messages to pass the packets up the stream.  We should
		 * instead have initialized constants reflecting
		 * MAX_RX_BUF_2104x and MAX_RX_BUF_2114x, and we should also
		 * probably have a total maximum for the free pool, so that we
		 * don't get out of hand when someone puts in an 8-port board.
		 * The maximum for the entire pool should be the total number
		 * of descriptors for all attached instances together, plus the
		 * total maximum for the free pool.  This maximum would only be
		 * reached after some number of instances allocate buffers:
		 * each instance would add (max_rx_buf-max_rx_desc) to the free
		 * pool.
		 */
		ndesc = dnetp->max_rx_desc - nrecv_desc_old;
		if ((ndesc > 0) &&
		    (dnet_rbuf_init(dnetp->devinfo, ndesc * 2) != 0))
			return (FAILURE);

		for (i = 0; i < dnetp->max_rx_desc; i++) {
			uchar_t *rbuf;

			rbuf = (uchar_t *)dnet_rbuf_alloc(1);
			if (rbuf == NULL)
				return (FAILURE);
			dnetp->rx_buf_vaddr[i] = rbuf;
		}
	}

	return (SUCCESS);
}

/*
 * free descriptors/buffers allocated for this device instance.  This routine
 * should only be called while the device is reset.
 */
static void
dnet_free_bufs(gld_mac_info_t *macinfo)
{
	int i;
	struct dnetinstance	*dnetp = (struct dnetinstance *)
						(macinfo->gldm_private);
	/* free up any xmit descriptors/buffers */
	if (dnetp->tx_desc != NULL) {
		ddi_dma_mem_free(&dnetp->tx_desc_acchdl);
		dnetp->tx_desc = NULL;
		/* we use streams buffers for DMA in xmit process */
		if (dnetp->tx_msgbufp != NULL) {
		    /* free up any streams message buffers unclaimed */
		    for (i = 0; i < dnetp->nxmit_desc; i++) {
			if (dnetp->tx_msgbufp[i] != NULL) {
				freemsg(dnetp->tx_msgbufp[i]);
			}
		    }
		    kmem_free(dnetp->tx_msgbufp, dnetp->nxmit_desc *
			sizeof (mblk_t **));
		    dnetp->tx_msgbufp = NULL;
		}
		dnetp->nxmit_desc = 0;
	}

	/* free up any receive descriptors/buffers */
	if (dnetp->rx_desc != NULL) {
		ddi_dma_mem_free(&dnetp->rx_desc_acchdl);
		dnetp->rx_desc = NULL;
		if (dnetp->rx_buf_vaddr != NULL) {
			/* free up the attached rbufs if any */
			for (i = 0; i < dnetp->nrecv_desc; i++) {
			    if (dnetp->rx_buf_vaddr[i])
				dnet_rbuf_free((caddr_t)dnetp->rx_buf_vaddr[i]);
			}
		    kmem_free(dnetp->rx_buf_vaddr, dnetp->nrecv_desc *
			sizeof (uchar_t **));
		    dnetp->rx_buf_vaddr = NULL;
		}
		dnetp->nrecv_desc = 0;
	}

	if (dnetp->setup_buf_vaddr != NULL) {
		ddi_dma_mem_free(&dnetp->setup_buf_acchdl);
		dnetp->setup_buf_vaddr = NULL;
	}

	if (dnetp->dma_handle != NULL) {
		ddi_dma_free_handle(&dnetp->dma_handle);
		dnetp->dma_handle = NULL;
	}

	if (dnetp->dma_handle_tx != NULL) {
		ddi_dma_free_handle(&dnetp->dma_handle_tx);
		dnetp->dma_handle_tx = NULL;
	}
}

/*
 * Initialize transmit and receive descriptors.
 */
static void
dnet_init_txrx_bufs(gld_mac_info_t *macinfo)
{
	int		i;
	struct dnetinstance	*dnetp = (struct dnetinstance *)
						(macinfo->gldm_private);

	/*
	 * Initilize all the Tx descriptors
	 */
	for (i = 0; i < dnetp->nxmit_desc; i++) {
		/*
		 * We may be resetting the device due to errors,
		 * so free up any streams message buffer unclaimed.
		 */
		if (dnetp->tx_msgbufp[i] != NULL) {
			freemsg(dnetp->tx_msgbufp[i]);
			dnetp->tx_msgbufp[i] = NULL;
		}
		*(ulong_t *)&dnetp->tx_desc[i].desc0 = 0;
		*(ulong_t *)&dnetp->tx_desc[i].desc1 = 0;
		dnetp->tx_desc[i].buffer1 = (paddr_t)(0);
		dnetp->tx_desc[i].buffer2 = (paddr_t)(0);
	}
	dnetp->tx_desc[i - 1].desc1.end_of_ring = 1;

	/*
	 * Initialize the Rx descriptors
	 */
	for (i = 0; i < dnetp->nrecv_desc; i++) {
#ifndef CONTIG_RX_BUFS
		paddr_t end_paddr;
#endif
		*(ulong_t *)&dnetp->rx_desc[i].desc0 = 0;
		*(ulong_t *)&dnetp->rx_desc[i].desc1 = 0;
		dnetp->rx_desc[i].desc0.own = 1;
		dnetp->rx_desc[i].desc1.buffer_size1 = rx_buf_size;
		dnetp->rx_desc[i].buffer1 =
		    (paddr_t)DNET_KVTOP(dnetp->rx_buf_vaddr[i]);
		dnetp->rx_desc[i].buffer2 = (paddr_t)(0);
#ifndef CONTIG_RX_BUFS
		end_paddr =
		    (paddr_t)DNET_KVTOP(dnetp->rx_buf_vaddr[i]+rx_buf_size-1);
		if ((dnetp->rx_desc[i].buffer1 & ~dnetp->pgmask) !=
		    (end_paddr & ~dnetp->pgmask)) {
			/* discontiguous */
			dnetp->rx_desc[i].buffer2 = end_paddr&~dnetp->pgmask;
			dnetp->rx_desc[i].desc1.buffer_size2 =
			    (end_paddr & dnetp->pgmask) + 1;
			dnetp->rx_desc[i].desc1.buffer_size1 =
			    rx_buf_size-dnetp->rx_desc[i].desc1.buffer_size2;
		}
#endif
	}
	dnetp->rx_desc[i - 1].desc1.end_of_ring = 1;
}

static int
alloc_descriptor(gld_mac_info_t *macinfo)
{
	int index;
	struct dnetinstance *dnetp =	/* Our private device info */
		(struct dnetinstance *)macinfo->gldm_private;
	register struct tx_desc_type    *ring = dnetp->tx_desc;
alloctop:
	index = dnetp->tx_current_desc;

	dnet_reclaim_Tx_desc(macinfo);

	/* we do have free descriptors, right? */
	if (dnetp->free_desc <= 0) {
#ifdef DNETDEBUG
		if (dnetdebug & DNETRECV)
			cmn_err(CE_NOTE, "dnet: Ring buffer is full");
#endif
		return (FAILURE);
	}

	/* sanity, make sure the next descriptor is free for use (should be) */
	if (ring[index].desc0.own) {
#ifdef DNETDEBUG
		if (dnetdebug & DNETRECV)
			cmn_err(CE_WARN,
				"dnet: next descriptor is not free for use");
#endif
		return (FAILURE);
	}
	if (dnetp->need_saddr) {
		/* XXX function return value ignored */
		(void) dnet_saddr(macinfo);
		goto alloctop;
	}

	*(ulong_t *)&ring[index].desc0 = 0;  /* init descs */
	*(ulong_t *)&ring[index].desc1 &= DNET_END_OF_RING;

	/* hardware will own this descriptor when poll activated */
	dnetp->free_desc--;

	/* point to next free descriptor to be used */
	dnetp->tx_current_desc = NextTXIndex(index);

#ifdef DNET_NOISY
	cmn_err(CE_WARN, "sfree 0x%x, transmitted 0x%x, tx_current 0x%x",
	    dnetp->free_desc, dnetp->transmitted_desc, dnetp->tx_current_desc);
#endif
	return (SUCCESS);
}

/*
 *
 */
static void
dnet_reclaim_Tx_desc(gld_mac_info_t *macinfo)
{
	struct dnetinstance	*dnetp = (struct dnetinstance *)
						(macinfo->gldm_private);
	struct tx_desc_type	*desc = dnetp->tx_desc;
	int index;

#ifdef DNETDEBUG
	if (dnetdebug & DNETTRACE)
		cmn_err(CE_NOTE, "dnet_reclaim_Tx_desc(0x%p)",
			(void *) macinfo);
#endif

	index = dnetp->transmitted_desc;
	while (((dnetp->free_desc == 0) || (index != dnetp->tx_current_desc)) &&
	    !(desc[index].desc0.own)) {
		/*
		 * Check for Tx Error that gets set
		 * in the last desc.
		 */
		if (desc[index].desc1.setup_packet == 0 &&
		    desc[index].desc1.last_desc &&
		    desc[index].desc0.err_summary)
			update_tx_stats(macinfo, index);

		/*
		 * If we have used the streams message buffer for this
		 * descriptor then free up the message now.
		 */
		if (dnetp->tx_msgbufp[index] != NULL) {
			freemsg(dnetp->tx_msgbufp[index]);
			dnetp->tx_msgbufp[index] = NULL;
		}
		dnetp->free_desc++;
		index = (index+1) % dnetp->max_tx_desc;
	}

	dnetp->transmitted_desc = index;
}

/*
 * Receive buffer allocation/freeing routines.
 *
 * There is a common pool of receive buffers shared by all dnet instances.
 *
 * XXX NEEDSWORK
 *
 * We arbitrarily allocate twice as many receive buffers as
 * receive descriptors because we use the buffers for streams
 * messages to pass the packets up the stream.  We should
 * instead have initialized constants reflecting
 * MAX_RX_BUF_2104x and MAX_RX_BUF_2114x, and we should also
 * probably have a total maximum for the free pool, so that we
 * don't get out of hand when someone puts in an 8-port board.
 * The maximum for the entire pool should be the total number
 * of descriptors for all attached instances together, plus the
 * total maximum for the free pool.  This maximum would only be
 * reached after some number of instances allocate buffers:
 * each instance would add (max_rx_buf-max_rx_desc) to the free
 * pool.
 */

struct rbuf_list {
	struct rbuf_list	*rbuf_next;	/* next in the list */
	caddr_t			rbuf_vaddr;	/* virual addr of the buf */
	ddi_acc_handle_t	rbuf_acchdl;	/* handle for DDI functions */
};

static struct rbuf_list *rbuf_usedlist_head;
static struct rbuf_list *rbuf_freelist_head;
static struct rbuf_list *rbuf_usedlist_end;	/* last buffer allocated */

static int rbuf_freebufs;	/* no. of free buffers in the pool */
static int rbuf_pool_size;	/* total no. of buffers in the pool */
static ddi_dma_handle_t dma_handle;

/* initialize/add 'nbufs' buffers to the rbuf pool */
static int
dnet_rbuf_init(dev_info_t *dip, int nbufs)
{
	int i;

	mutex_enter(&dnet_rbuf_lock);

	if (dma_handle == NULL) {
		/* XXX function return value ignored */
		(void) ddi_dma_alloc_handle(dip, &dma_attr, DDI_DMA_SLEEP, 0,
					&dma_handle);
	}

	/* allocate buffers and add them to the pool */
	for (i = 0; i < nbufs; i++) {
		struct rbuf_list *rp;

		/* allocate rbuf_list element */
		rp = (struct rbuf_list *)kmem_zalloc(sizeof (struct rbuf_list),
						KM_SLEEP);
		/* allocate iopb/dma memory for the buffer */
#ifdef CONTIG_RX_BUFS
		if (ddi_dma_mem_alloc(dma_handle, rx_buf_size, &accattr,
		    DDI_DMA_STREAMING, DDI_DMA_DONTWAIT, 0,
		    &rp->rbuf_vaddr, &len,
		    &rp->rbuf_acchdl) != DDI_SUCCESS) {
			mutex_exit(&dnet_rbuf_lock);
			kmem_free(rp, sizeof (struct rbuf_list));
			return (-1);
		}
#else
		/* We assume kmem_alloc() longword aligns */
		if ((rp->rbuf_vaddr = (caddr_t)kmem_alloc(rx_buf_size,
		    KM_NOSLEEP)) == NULL) {
			mutex_exit(&dnet_rbuf_lock);
			kmem_free(rp, sizeof (struct rbuf_list));
			return (-1);
		}
#endif
		rp->rbuf_next = rbuf_freelist_head;
		rbuf_freelist_head = rp;
		rbuf_pool_size++;
		rbuf_freebufs++;
	}

	mutex_exit(&dnet_rbuf_lock);
	return (0);
}

/*
 * Try to free up all the rbufs in the pool. Returns 0 if it frees up all
 * buffers. The buffers in the used list are considered busy so these
 * buffers are not freed.
 */
static int
dnet_rbuf_destroy()
{
	struct rbuf_list *rp, *next;

	mutex_enter(&dnet_rbuf_lock);

	for (rp = rbuf_freelist_head; rp; rp = next) {
		next = rp->rbuf_next;
#ifdef CONTIG_RX_BUFS
		ddi_dma_mem_free(&rp->rbuf_acchdl);
#else
		kmem_free(rp->rbuf_vaddr, rx_buf_size);
#endif
		kmem_free(rp, sizeof (struct rbuf_list));
		rbuf_pool_size--;
		rbuf_freebufs--;
	}
	rbuf_freelist_head = NULL;

	if (rbuf_pool_size) { /* pool is still not empty */
		mutex_exit(&dnet_rbuf_lock);
		return (-1);
	}

	if (dma_handle != NULL) {
		ddi_dma_free_handle(&dma_handle);
		dma_handle = NULL;
	}

	mutex_exit(&dnet_rbuf_lock);
	return (0);
}

static caddr_t
dnet_rbuf_alloc(int cansleep)
{
	struct rbuf_list *rp;

	mutex_enter(&dnet_rbuf_lock);

	if (rbuf_freelist_head == NULL) {

		if (!cansleep) {
			mutex_exit(&dnet_rbuf_lock);
			return (NULL);
		}

		/* allocate rbuf_list element */
		rp = (struct rbuf_list *)kmem_zalloc(sizeof (struct rbuf_list),
							KM_SLEEP);
		/* allocate iopb/dma memory for the buffer */
#ifdef CONTIG_RX_BUFS
		if (ddi_dma_mem_alloc(dma_handle, rx_buf_size, &accattr,
		    DDI_DMA_STREAMING, DDI_DMA_DONTWAIT, 0,
		    &rp->rbuf_vaddr, &len,
		    &rp->rbuf_acchdl) != DDI_SUCCESS) {
			kmem_free(rp, sizeof (struct rbuf_list));
			mutex_exit(&dnet_rbuf_lock);
			return (NULL);
		}
#else
		/* We assume kmem_alloc() longword aligns */
		if ((rp->rbuf_vaddr = (caddr_t)kmem_alloc(rx_buf_size,
		    KM_NOSLEEP)) == NULL) {
			kmem_free(rp, sizeof (struct rbuf_list));
			mutex_exit(&dnet_rbuf_lock);
			return (NULL);
		}
#endif
		rbuf_freelist_head = rp;
		rbuf_pool_size++;
		rbuf_freebufs++;
	}

	/* take the buffer from the head of the free list */
	rp = rbuf_freelist_head;
	rbuf_freelist_head = rbuf_freelist_head->rbuf_next;

	/* update the used list; put the entry at the end */
	if (rbuf_usedlist_head == NULL)
		rbuf_usedlist_head = rp;
	else
		rbuf_usedlist_end->rbuf_next = rp;
	rp->rbuf_next = NULL;
	rbuf_usedlist_end = rp;
	rbuf_freebufs--;

	mutex_exit(&dnet_rbuf_lock);

	return (rp->rbuf_vaddr);
}

static void
dnet_rbuf_free(caddr_t vaddr)
{
	struct rbuf_list *rp, *prev;

	ASSERT(vaddr != NULL);
	ASSERT(rbuf_usedlist_head != NULL);

	mutex_enter(&dnet_rbuf_lock);

	/* find the entry in the used list */
	for (prev = rp = rbuf_usedlist_head; rp; rp = rp->rbuf_next) {
		if (rp->rbuf_vaddr == vaddr)
			break;
		prev = rp;
	}

	if (rp == NULL) {
		cmn_err(CE_WARN, "DNET: rbuf_free: bad addr 0x%p",
			(void *)vaddr);
		mutex_exit(&dnet_rbuf_lock);
		return;
	}

	/* update the used list and put the buffer back in the free list */
	if (rbuf_usedlist_head != rp) {
		prev->rbuf_next = rp->rbuf_next;
		if (rbuf_usedlist_end == rp)
			rbuf_usedlist_end = prev;
	} else {
		rbuf_usedlist_head = rp->rbuf_next;
		if (rbuf_usedlist_end == rp)
			rbuf_usedlist_end = NULL;
	}
	rp->rbuf_next = rbuf_freelist_head;
	rbuf_freelist_head = rp;
	rbuf_freebufs++;

	mutex_exit(&dnet_rbuf_lock);
}

/*
 * Free the receive buffer used in a stream's message block allocated
 * thru desballoc().
 */
static void
dnet_freemsg_buf(struct free_ptr *frp)
{
	dnet_rbuf_free((caddr_t)frp->buf); /* buffer goes back to the pool */
	kmem_free(frp, sizeof (*frp)); /* free up the free_rtn structure */
}

#endif	/* REALMODE */

/*
 *	========== SROM Read Routines ==========
 */

/*
 * The following code gets the SROM information, either by reading it
 * from the device or, failing that, by reading a property.
 */
static int
dnet_read_srom(dev_info_t devinfo, int board_type, ddi_acc_handle_t io_handle,
    int io_reg, uchar_t *vi, int maxlen)
{
	int all_ones, zerocheck, i;

	/*
	 * Load SROM into vendor_info
	 */
	if (board_type == DEVICE_ID_21040)
		dnet_read21040addr(devinfo, io_handle, io_reg, vi, &maxlen);
	else
		/* 21041/21140 serial rom */
		dnet_read21140srom(io_handle, io_reg, vi, maxlen);
	/*
	 * If the dumpsrom property is present in the conf file, print
	 * the contents of the SROM to the console
	 */
	if (ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
				"dumpsrom", 0))
		dnet_dumpbin("SROM", vi, 1, maxlen);

	for (zerocheck = i = 0, all_ones = 0xff; i < maxlen; i++) {
		zerocheck |= vi[i];
		all_ones &= vi[i];
	}
	if (zerocheck == 0 || all_ones == 0xff) {
		return (get_alternative_srom_image(devinfo, vi, maxlen));
	} else {
#ifdef BUG_4010796
		set_alternative_srom_image(devinfo, vi, maxlen);
#endif
		return (0);	/* Primary */
	}
}

/*
 * The function reads the ethernet address of the 21040 adapter
 */
static void
dnet_read21040addr(dev_info_t *dip, ddi_acc_handle_t io_handle, int io_reg,
    uchar_t *addr, int *len)
{
	ulong_t		val;
	int		i;

	/* No point reading more than the ethernet address */
	*len = ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, macoffset_propname, 0) + ETHERADDRL;

	/* Reset ROM pointer */
	ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG), 0);
	for (i = 0; i < *len; i++) {
		do {
			val = ddi_io_getl(io_handle,
				REG32(io_reg, ETHER_ROM_REG));
		} while (val & 0x80000000);
		addr[i] = val & 0xFF;
	}
}

#define	drv_nsecwait(x)	drv_usecwait(((x)+999)/1000) /* XXX */

/*
 * The function reads the SROM	of the 21140 adapter
 */
static void
dnet_read21140srom(ddi_acc_handle_t io_handle, int io_reg, uchar_t *addr,
    int maxlen)
{
	uint_t 		i, j;
	unsigned char	rom_addr;
	unsigned char	bit;
	unsigned long	dout;
	unsigned short	word;

	rom_addr = 0;
	for (i = 0; i <	maxlen; i += 2) {
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM);
		drv_nsecwait(30);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP);
		drv_nsecwait(50);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | SEL_CLK);
		drv_nsecwait(250);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP);
		drv_nsecwait(100);

		/* command */
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | DATA_IN);
		drv_nsecwait(150);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | DATA_IN | SEL_CLK);
		drv_nsecwait(250);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | DATA_IN);
		drv_nsecwait(250);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | DATA_IN | SEL_CLK);
		drv_nsecwait(250);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | DATA_IN);
		drv_nsecwait(100);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP);
		drv_nsecwait(150);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | SEL_CLK);
		drv_nsecwait(250);
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP);
		drv_nsecwait(100);

		/* Address */
		for (j = HIGH_ADDRESS_BIT; j >= 1; j >>= 1) {
			bit = (rom_addr & j) ? DATA_IN : 0;
			ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | bit);
			drv_nsecwait(150);
			ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | bit | SEL_CLK);
			drv_nsecwait(250);
			ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM | SEL_CHIP | bit);
			drv_nsecwait(100);
		}
		drv_nsecwait(150);

		/* Data */
		word = 0;
		for (j = 0x8000; j >= 1; j >>= 1) {
			ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
				    READ_OP | SEL_ROM | SEL_CHIP | SEL_CLK);
			drv_nsecwait(100);
			dout = ddi_io_getl(io_handle,
					    REG32(io_reg, ETHER_ROM_REG));
			drv_nsecwait(150);
			if (dout & DATA_OUT)
				word |= j;
			ddi_io_putl(io_handle,
				    REG32(io_reg, ETHER_ROM_REG),
				    READ_OP | SEL_ROM | SEL_CHIP);
			drv_nsecwait(250);
		}
		addr[i] = (word & 0x0000FF);
		addr[i + 1] = (word >> 8);
		rom_addr++;
		ddi_io_putl(io_handle, REG32(io_reg, ETHER_ROM_REG),
			    READ_OP | SEL_ROM);
		drv_nsecwait(100);
	}
}


#ifdef REALMODE
static int
get_alternative_srom_image(dev_info_t devinfo, uchar_t *vi, int len)
{
}

static void
set_alternative_srom_image(dev_info_t devinfo, uchar_t *vi, int len)
{
}
#else

/*
 * XXX NEEDSWORK
 *
 * Some lame multiport cards have only one SROM, which can be accessed
 * only from the "first" 21x4x chip, whichever that one is.  If we can't
 * get at our SROM, we look for its contents in a property instead, which
 * we rely on the bootstrap to have properly set.
#ifdef BUG_4010796
 * We also have a hack to try to set it ourselves, when the "first" port
 * attaches, if it has not already been properly set.  However, this method
 * is not reliable, since it makes the unwarrented assumption that the
 * "first" port will attach first.
#endif
 */

static int
get_alternative_srom_image(dev_info_t devinfo, uchar_t *vi, int len)
{
	int	l = len;

	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
	    "DNET_SROM", (caddr_t)vi, &len) != DDI_PROP_SUCCESS &&
	    (len = l) && ddi_getlongprop_buf(DDI_DEV_T_ANY,
	    ddi_get_parent(devinfo), DDI_PROP_DONTPASS, "DNET_SROM",
	    (caddr_t)vi, &len) != DDI_PROP_SUCCESS)
		return (-1);	/* Can't find it! */

	/*
	 * The return value from this routine specifies which port number
	 * we are.  The primary port is denoted port 0.  On a QUAD card we
	 * should return 1, 2, and 3 from this routine.  The return value
	 * is used to modify the ethernet address from the SROM data.
	 */

#ifdef BUG_4010796
	{
	/*
	 * For the present, we remember the device number of our primary
	 * sibling and hope we and our other siblings are consecutively
	 * numbered up from there.  In the future perhaps the bootstrap
	 * will pass us the necessary information telling us which physical
	 * port we really are.
	 */
	pci_regspec_t	*assignp;
	int		assign_len;
	int 		devnum;
	int		primary_devnum;

	primary_devnum = ddi_getprop(DDI_DEV_T_ANY, devinfo, 0,
	    "DNET_DEVNUM", -1);
	if (primary_devnum == -1)
		return (1);	/* XXX NEEDSWORK -- We have no better idea */

	if ((ddi_getlongprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
	    "assigned-addresses", (caddr_t)&assignp,
	    &assign_len)) != DDI_PROP_SUCCESS)
		return (1);	/* XXX NEEDSWORK -- We have no better idea */

	devnum = PCI_REG_DEV_G(assignp->pci_phys_hi);
	kmem_free(assignp, assign_len);
	return (devnum - primary_devnum);
	}
#else
	return (1);	/* XXX NEEDSWORK -- We have no better idea */
#endif
}


#ifdef BUG_4010796
static void
set_alternative_srom_image(dev_info_t devinfo, uchar_t *vi, int len)
{
	int 		proplen;
	pci_regspec_t	*assignp;
	int		assign_len;
	int 		devnum;

	if (ddi_getproplen(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
	    "DNET_SROM", &proplen) == DDI_PROP_SUCCESS ||
	    ddi_getproplen(DDI_DEV_T_ANY, ddi_get_parent(devinfo),
	    DDI_PROP_DONTPASS, "DNET_SROM", &proplen) == DDI_PROP_SUCCESS)
		return;		/* Already done! */

	/* XXX function return value ignored */
	(void) ddi_prop_create(DDI_DEV_T_NONE, ddi_get_parent(devinfo),
	    DDI_PROP_CANSLEEP, "DNET_SROM", (caddr_t)vi, len);
	/* XXX function return value ignored */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devinfo,
	    DDI_PROP_CANSLEEP, "DNET_HACK", "hack", 4);

	if ((ddi_getlongprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
	    "assigned-addresses", (caddr_t)&assignp,
	    &assign_len)) == DDI_PROP_SUCCESS) {
		devnum = PCI_REG_DEV_G(assignp->pci_phys_hi);
		kmem_free(assignp, assign_len);
		/* XXX function return value ignored */
		(void) ddi_prop_create(DDI_DEV_T_NONE, ddi_get_parent(devinfo),
		    DDI_PROP_CANSLEEP, "DNET_DEVNUM", (caddr_t)&devnum,
		    sizeof (devnum));
	}
}
#endif

#endif /* REALMODE */

/*
 *	========== SROM Parsing Routines ==========
 */

static int
check_srom_valid(uchar_t *vi)
{
	int valid;

	/*
	 * XXX NEEDSWORK
	 *
	 * Should probably check the checksum, but if you change this you
	 * must retest all supported cards.
	 */
	valid = ((vi[SROM_VERSION] == 1 || vi[SROM_VERSION] == 3) &&
	    vi[SROM_MBZ] == 0 && /* must be zero */
	    vi[SROM_MBZ2] == 0 && /* must be zero */
	    vi[SROM_MBZ3] == 0 && /* must be zero */
	    vi[SROM_ADAPTER_CNT] >= 1 &&
	    vi[SROM_ADAPTER_CNT] <= MAX_ADAPTERS);
	return (valid);
}


/*
 *	========== Active Media Determination Routines ==========
 */

/* This routine is also called for V3 Compact and extended type 0 SROMs */
static int is_fdmedia(media)
{
	if (media == MEDIA_TP_FD || media == MEDIA_SYM_SCR_FD)
		return (1);
	else
		return (0);
}

/*
 * "Linkset" is used to merge media that use the same link test check. So,
 * if the TP link is added to the linkset, so is the TP Full duplex link.
 * Used to avoid checking the same link status twice.
 */
static void
linkset_add(ulong_t *set, int media)
{
	if (media == MEDIA_TP_FD || media == MEDIA_TP)
		*set |= (1UL<<MEDIA_TP_FD) | (1UL<<MEDIA_TP);
	else if (media == MEDIA_SYM_SCR_FD || media == MEDIA_SYM_SCR)
		*set |= (1UL<<MEDIA_SYM_SCR_FD) | (1UL<<MEDIA_SYM_SCR);
	else *set |= 1UL<<media;
}
static int
linkset_isset(ulong_t linkset, int media)
{
	return (((1UL<<media)  & linkset) ? 1:0);
}

/*
 * The following code detects which Media is connected for 21041/21140
 * Expect to change this code to support new 21140 variants.
 */
static void
find_active_media(gld_mac_info_t *macinfo)
{
	int i;
	media_block_t *block;
	media_block_t *best_allowed = NULL;
	media_block_t *fd_found = NULL; /* A fulldup medium that has a link */
	struct dnetinstance	*dnetp = (struct dnetinstance *)
					(macinfo->gldm_private);
	LEAF_FORMAT *leaf = &dnetp->sr.leaf[dnetp->leaf];
	ulong_t checked = 0, links_up = 0;

#ifdef SROMDEBUG
	cmn_err(CE_NOTE, "find_active_media 0x%x,0x%x", sr->version, macinfo);
#endif
	dnetp->selected_media_block = leaf->default_block;

#ifndef REALMODE
	if (dnetp->phyaddr != -1) {
		dnetp->selected_media_block = leaf->mii_block;
		setup_block(macinfo);

		if (ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
		    DDI_PROP_DONTPASS, "portmon", 1)) {
			/* XXX return value ignored */
			(void) mii_start_portmon(dnetp->mii, dnet_mii_link_cb,
			    &macinfo->gldm_maclock);
			/*
			 * If the port monitor detects the link is already
			 * up, there is no point going through the rest of the
			 * link sense
			 */
			if (dnetp->mii_up) {
				return;
			}
		}
	}
#endif


	/* Search for an active medium, and select it */
	for (block = leaf->block + leaf->block_count  - 1;
	    block >= leaf->block; block--) {
		int media = block->media_code;
		char *name = media_str[media];

		/* User settings disallow selection of this block */
		if (dnetp->disallowed_media & (1UL<<media))
			continue;

		/* We may not be able to pick the default */
		if (best_allowed == NULL || block == leaf->default_block)
			best_allowed = block;
#ifdef DEBUG
		if (dnetdebug & DNETSENSE)
		    cmn_err(CE_NOTE, "Testing %s medium (block type %d)",
			name, block->type);
#endif

		dnetp->selected_media_block = block;
		switch (block->type) {

		case 2: /* SIA Media block: Best we can do is send a packet */
			setup_block(macinfo);
			if (send_test_packet(macinfo)) {
				if (!is_fdmedia(media))
					return;
				if (!fd_found)
					fd_found = block;
			}
			break;

		/* SYM/SCR or TP block: Use the link-sense bits */
		case 0:
			if (!linkset_isset(checked, media)) {
				linkset_add(&checked, media);
				if (((media == MEDIA_BNC ||
				    media == MEDIA_AUI) &&
				    send_test_packet(macinfo)) ||
				    dnet_link_sense(macinfo))
					linkset_add(&links_up, media);
			}

			if (linkset_isset(links_up, media)) {
				if (!is_fdmedia(media) &&
				    dnetp->selected_media_block ==
				    leaf->default_block) {
					setup_block(macinfo);
					return;
				} else if (fd_found == NULL) {
					fd_found = block;
				}
			}
			break;

		/*
		 * MII block: May take up to a second or so to settle if
		 * setup causes a PHY reset
		 */
		case 1: case 3:
			setup_block(macinfo);
			for (i = 0; ; i++) {
				if (mii_linkup(dnetp->mii, dnetp->phyaddr)) {
					/* XXX function return value ignored */
					(void) mii_getspeed(dnetp->mii,
						dnetp->phyaddr,
						&dnetp->mii_speed,
						&dnetp->mii_duplex);
					dnetp->mii_up = 1;
					leaf->mii_block = block;
					return;
				}
				if (i == 10)
					break;
				delay(drv_usectohz(150000));
			}
			dnetp->mii_up = 0;
			break;
		}
	}
	if (fd_found) {
		dnetp->selected_media_block = fd_found;
	} else {
		if (best_allowed == NULL)
			best_allowed = leaf->default_block;
		dnetp->selected_media_block = best_allowed;
		cmn_err(CE_WARN, "!dnet: Default media selected\n");
	}
	setup_block(macinfo);
}

/* Do anything neccessary to select the selected_media_block */
static void
setup_block(gld_mac_info_t *macinfo)
{
	dnet_reset_board(macinfo);
	dnet_init_board(macinfo);
	/* XXX function return value ignored */
	(void) dnet_start_board(macinfo);
}

static int
dnet_link_sense(gld_mac_info_t *macinfo)
{
	/*
	 * This routine makes use of the command word from the srom config.
	 * Details of the auto-sensing information contained in this can
	 * be found in the "Digital Semiconductor 21X4 Serial ROM Format v3.03"
	 * spec. Section 4.3.2.1, and 4.5.2.1.3
	 */
	struct dnetinstance *dnetp = (struct dnetinstance *)
					(macinfo->gldm_private);
	media_block_t *block = dnetp->selected_media_block;
	ulong_t link, status, mask, polarity;
	int settletime, stabletime, waittime, upsamples;
	int delay_100, delay_10;


	/* Don't autosense if the medium does not support it */
	if (block->command & (1 << 15)) {
		/* This should be the default block */
		if (block->command & (1UL<<14))
			dnetp->sr.leaf[dnetp->leaf].default_block = block;
		return (0);
	}

	delay_100 = ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
	    DDI_PROP_DONTPASS, "autosense-delay-100", 2000);

	delay_10 = ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
	    DDI_PROP_DONTPASS, "autosense-delay-10", 400);

	/*
	 * Scrambler may need to be disabled for link sensing
	 * to work
	 */
	dnetp->disable_scrambler = 1;
	setup_block(macinfo);
	dnetp->disable_scrambler = 0;

	if (block->media_code == MEDIA_TP || block->media_code == MEDIA_TP_FD)
		settletime = delay_10;
	else
		settletime = delay_100;
	stabletime = settletime / 4;

	mask = 1 << ((block->command & CMD_MEDIABIT_MASK) >> 1);
	polarity = block->command & CMD_POL ? 0xffffffff : 0;

	for (waittime = 0, upsamples = 0;
	    waittime <= settletime + stabletime && upsamples < 8;
	    waittime += stabletime/8) {
#ifdef REALMODE
		milliseconds(stabletime/8);
#else
		delay(drv_usectohz(stabletime*1000 / 8));
#endif
		status = read_gpr(dnetp);
		link = (status^polarity) & mask;
		if (link)
			upsamples++;
		else
			upsamples = 0;
	}
#ifdef DNETDEBUG
	if (dnetdebug & DNETSENSE)
		cmn_err(CE_NOTE, "%s upsamples:%d stat:%lx polarity:%lx "
		    "mask:%lx link:%lx",
		    upsamples == 8 ? "UP":"DOWN",
		    upsamples, status, polarity, mask, link);
#endif
	if (upsamples == 8)
		return (1);
	return (0);
}

static int
send_test_packet(gld_mac_info_t *macinfo)
{
	int packet_delay;
	struct dnetinstance	*dnetp = (struct dnetinstance *)
					(macinfo->gldm_private);
	struct tx_desc_type *desc;
	int bufindex;
	int media_code = dnetp->selected_media_block->media_code;
	ulong_t del;

	/*
	 * For a successful test packet, the card must have settled into
	 * its current setting.  Almost all cards we've tested manage to
	 * do this with all media within 50ms.  However, the SMC 8432
	 * requires 300ms to settle into BNC mode.  We now only do this
	 * from attach, and we do sleeping delay() instead of drv_usecwait()
	 * so we hope this .2 second delay won't cause too much suffering.
	 * ALSO: with an autonegotiating hub, an aditional 1 second delay is
	 * required. This is done if the media type is TP
	 */
	if (media_code == MEDIA_TP || media_code == MEDIA_TP_FD) {
		packet_delay = ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
		    DDI_PROP_DONTPASS, "test_packet_delay_tp", 1300000);
	} else {
		packet_delay = ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
		    DDI_PROP_DONTPASS, "test_packet_delay", 300000);
	}
	delay(drv_usectohz(packet_delay));

	desc = dnetp->tx_desc;

	bufindex = dnetp->tx_current_desc;
	if (alloc_descriptor(macinfo) == FAILURE) {
		cmn_err(CE_WARN, "DNET: send_test_packet: alloc_descriptor"
		    "failed");
		return (0);
	}

	/*
	 * use setup buffer as the buffer for the test packet
	 * instead of allocating one.
	 */

	ASSERT(dnetp->setup_buf_vaddr != NULL);
	/* Put something decent in dest address so we don't annoy other cards */
	BCOPY((caddr_t)macinfo->gldm_macaddr,
		(caddr_t)dnetp->setup_buf_vaddr, ETHERADDRL);
	BCOPY((caddr_t)macinfo->gldm_macaddr,
		(caddr_t)dnetp->setup_buf_vaddr+ETHERADDRL, ETHERADDRL);

	desc[bufindex].buffer1 = (paddr_t)DNET_KVTOP(dnetp->setup_buf_vaddr);
	desc[bufindex].desc1.buffer_size1 = SETUPBUF_SIZE;
	desc[bufindex].buffer2 = (paddr_t)(0);
	desc[bufindex].desc1.first_desc = 1;
	desc[bufindex].desc1.last_desc = 1;
	desc[bufindex].desc1.int_on_comp = 1;
	desc[bufindex].desc0.own = 1;

	ddi_io_putb(dnetp->io_handle, REG8(dnetp->io_reg, TX_POLL_REG),
		    TX_POLL_DEMAND);

	/*
	 * Give enough time for the chip to transmit the packet
	 */
#if 1
	del = 1000;
	while (desc[bufindex].desc0.own && --del)
		drv_usecwait(10);	/* quickly wait up to 10ms */
	if (desc[bufindex].desc0.own)
		delay(drv_usectohz(200000));	/* nicely wait a longer time */
#else
	del = 0x10000;
	while (desc[bufindex].desc0.own && --del)
		drv_usecwait(10);
#endif

#ifdef DNETDEBUG
	if (dnetdebug & DNETSENSE)
		cmn_err(CE_NOTE, "desc0 bits = %lu, %lu, %lu, %lu, %lu, %lu",
			desc[bufindex].desc0.own,
			desc[bufindex].desc0.err_summary,
			desc[bufindex].desc0.carrier_loss,
			desc[bufindex].desc0.no_carrier,
			desc[bufindex].desc0.late_collision,
			desc[bufindex].desc0.link_fail);
#endif
	if (desc[bufindex].desc0.own) /* it shouldn't take this long, error */
	    return (0);

	return (!desc[bufindex].desc0.err_summary);
}

static void
enable_interrupts(struct dnetinstance *dnetp, int enable_xmit)
{
	/* Don't enable interrupts if they have been forced off */
	if (dnetp->interrupts_disabled)
		return;
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, INT_MASK_REG),
	    NORMAL_INTR_MASK | ABNORMAL_INTR_MASK | TX_UNDERFLOW_MASK |
	    (enable_xmit ? TX_INTERRUPT_MASK : 0) |
	    (dnetp->timer.cb ? GPTIMER_INTR : 0) |
	    RX_INTERRUPT_MASK | SYSTEM_ERROR_MASK | TX_JABBER_MASK);

}


#ifndef REALMODE
/*
 * Some older multiport cards are non-PCI compliant in their interrupt routing.
 * Second and subsequent devices are incorrectly configured by the BIOS
 * (either in their ILINE configuration or the MP Configuration Table for PC+MP
 * systems).
 * The hack stops gldregister() registering the interrupt routine for the
 * FIRST device on the adapter, and registers its own. It builds up a table
 * of macinfo structures for each device, and the new interrupt routine
 * calls gldintr for each of them.
 * Known cards that suffer from this problem are:
 *	All Cogent multiport cards;
 * 	Znyx 314;
 *	Znyx 315.
 *
 * XXX NEEDSWORK -- see comments above get_alternative_srom_image(). This
 * hack relies on the fact that the offending cards will have only one SROM.
 * It uses this fact to identify devices that are on the same multiport
 * adapter, as opposed to multiple devices from the same vendor (as
 * indicated by "secondary"
 */
static int
dnet_hack_interrupts(gld_mac_info_t *macinfo, int secondary)
{
	int i;
	struct hackintr_inf *hackintr_inf;
	struct dnetinstance *dnetp =
		(struct dnetinstance *)macinfo->gldm_private;
	dev_info_t devinfo = dnetp->devinfo;
	unsigned long oui = 0;	/* Organizationally Unique ID */

	if (ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
	    "no_INTA_workaround", 0) != 0)
		return (0);

	for (i = 0; i < 3; i++)
		oui = (oui << 8) | macinfo->gldm_vendor[i];

	/* Check wheather or not we need to implement the hack */

	switch (oui) {
	case ZNYX_ETHER:
		/* Znyx multiport 21040 cards <<==>> ZX314 or ZX315 */
		if (dnetp->board_type != DEVICE_ID_21040)
			return (0);
		break;

	case COGENT_ETHER:
		/* All known Cogent multiport cards */
		break;

	case ADAPTEC_ETHER:
		/* Adaptec multiport cards */
		break;

	default:
		/* Other cards work correctly */
		return (0);
	}

	/* card is (probably) non-PCI compliant in its interrupt routing */


	if (!secondary) {

		/*
		 * If we have already registered a hacked interrupt, and
		 * this is also a 'primary' adapter, then this is NOT part of
		 * a multiport card, but a second card on the same PCI bus.
		 * BUGID: 4057747
		 */
		if (ddi_getprop(DDI_DEV_T_ANY, ddi_get_parent(devinfo),
		    DDI_PROP_DONTPASS, hackintr_propname, 0) != 0)
			return (0);
				/* ... Primary not part of a multiport device */

#ifdef DNETDEBUG
		if (dnetdebug & DNETTRACE)
			cmn_err(CE_NOTE, "dnet: Implementing hardware "
				"interrupt flaw workaround");
#endif
		dnetp->hackintr_inf = hackintr_inf =
		    kmem_zalloc(sizeof (struct hackintr_inf), KM_SLEEP);
		if (hackintr_inf == NULL)
			goto fail;

		hackintr_inf->macinfos[0] = macinfo;
		hackintr_inf->devinfo = devinfo;

		/*
		 * Add a property to allow successive attaches to find the
		 * table
		 */

		if (ddi_prop_create(DDI_DEV_T_NONE, ddi_get_parent(devinfo),
		    DDI_PROP_CANSLEEP, hackintr_propname,
		    (caddr_t)&dnetp->hackintr_inf,
		    sizeof (void *)) != DDI_PROP_SUCCESS)
			goto fail;


		/* Register our hacked interrupt routine */
		if (ddi_add_intr(devinfo, 0, &macinfo->gldm_cookie, NULL,
		    (uint_t (*)(char *))dnet_hack_intr,
		    (caddr_t)hackintr_inf) != DDI_SUCCESS) {
			/* XXX function return value ignored */
			(void) ddi_prop_remove(DDI_DEV_T_NONE,
				ddi_get_parent(devinfo),
				hackintr_propname);
			goto fail;
		}

		/*
		 * Mutex required to ensure interrupt routine has completed
		 * when detaching devices
		 */
		mutex_init(&hackintr_inf->lock, NULL, MUTEX_DRIVER,
		    macinfo->gldm_cookie);

		/* Stop GLD registering an interrupt */
		return (-1);
	} else {

		/* Add the macinfo for this secondary device to the table */

		hackintr_inf = (struct hackintr_inf *)
		    ddi_getprop(DDI_DEV_T_ANY, ddi_get_parent(devinfo),
			DDI_PROP_DONTPASS, hackintr_propname, 0);

		if (hackintr_inf == NULL)
			goto fail;

		/* Find an empty slot */
		for (i = 0; i < MAX_INST; i++)
			if (hackintr_inf->macinfos[i] == NULL)
				break;

		/* More than 8 ports on adapter ?! */
		if (i == MAX_INST)
			goto fail;

		hackintr_inf->macinfos[i] = macinfo;

		/*
		 * Allow GLD to register a handler for this
		 * device. If the card is actually broken, as we suspect, this
		 * handler will never get called. However, by registering the
		 * interrupt handler, we can copy gracefully with new multiport
		 * Cogent cards that decide to fix the hardware problem
		 */
		return (0);
	}

fail:
	cmn_err(CE_WARN, "dnet: Could not work around hardware interrupt"
	    " routing problem");
	return (0);
}

/*
 * Call gld_intr for all adapters on a multiport card
 */

static uint_t
dnet_hack_intr(struct hackintr_inf *hackintr_inf)
{
	int i;
	int claimed = DDI_INTR_UNCLAIMED;

	/* Stop detaches while processing interrupts */
	mutex_enter(&hackintr_inf->lock);

	for (i = 0; i < MAX_INST; i++) {
		if (hackintr_inf->macinfos[i] &&
		    gld_intr(hackintr_inf->macinfos[i]) == DDI_INTR_CLAIMED)
			claimed = DDI_INTR_CLAIMED;
	}
	mutex_exit(&hackintr_inf->lock);
	return (claimed);
}

/*
 * This removes the detaching device from the table procesed by the hacked
 * interrupt routine. Because the interrupts from all devices come in to the
 * same interrupt handler, ALL devices must stop interrupting once the
 * primary device detaches. This isn't a problem at present, because all
 * instances of a device are detached when the driver is unloaded.
 */
static int
dnet_detach_hacked_interrupt(dev_info_t *devinfo)
{
	int i;
	struct hackintr_inf *hackintr_inf;
	gld_mac_info_t *mac, *macinfo =
	    (gld_mac_info_t *)ddi_get_driver_private(devinfo);

	hackintr_inf = (struct hackintr_inf *)
		    ddi_getprop(DDI_DEV_T_ANY, ddi_get_parent(devinfo),
			DDI_PROP_DONTPASS, hackintr_propname, 0);

	/*
	 * No hackintr_inf implies hack was not required or the primary has
	 * detached, and our interrupts are already disabled
	 */
	if (!hackintr_inf)
		return (DDI_SUCCESS);

	/* Remove this device from the handled table */
	mutex_enter(&hackintr_inf->lock);
	for (i = 0; i < MAX_INST; i++) {
		if (hackintr_inf->macinfos[i] == macinfo) {
			hackintr_inf->macinfos[i] = NULL;
			break;
		}
	}

	mutex_exit(&hackintr_inf->lock);

	/* Not the primary card, we are done */
	if (devinfo != hackintr_inf->devinfo)
		return (DDI_SUCCESS);

	/*
	 * This is the primary card. All remaining adapters on this device
	 * must have their interrupts disabled before we remove the handler
	 */
	for (i = 0; i < MAX_INST; i++) {
		if ((mac = hackintr_inf->macinfos[i]) != NULL) {
			struct dnetinstance *altdnetp =
				(struct dnetinstance *)mac->gldm_private;
			altdnetp->interrupts_disabled = 1;
			ddi_io_putl(altdnetp->io_handle,
				REG32(altdnetp->io_reg, INT_MASK_REG), 0);
		}
	}

	/* It should now be safe to remove the interrupt handler */

	ddi_remove_intr(devinfo, 0, &macinfo->gldm_cookie);
	mutex_destroy(&hackintr_inf->lock);
	/* XXX function return value ignored */
	(void) ddi_prop_remove(DDI_DEV_T_NONE, ddi_get_parent(devinfo),
	    hackintr_propname);
	kmem_free(hackintr_inf, sizeof (struct hackintr_inf));
	return (DDI_SUCCESS);
}
#endif
/*
 *	========== PHY MII Routines ==========
 */

static void
do_phy(gld_mac_info_t *macinfo)
{
	dev_info_t *dip;
	struct dnetinstance
	    *dnetp = (struct dnetinstance *)macinfo->gldm_private;
	LEAF_FORMAT *leaf = dnetp->sr.leaf + dnetp->leaf;
	media_block_t *block;
	int phy;

#ifdef REALMODE
	dip = (dev_info_t *)dnetp;
#else
	dip = dnetp->devinfo;
#endif

	/*
	 * Find and configure the PHY media block. If NO PHY blocks are
	 * found on the SROM, but a PHY device is present, we assume the card
	 * is a legacy device, and that there is ONLY a PHY interface on the
	 * card (ie, no BNC or AUI, and 10BaseT is implemented by the PHY
	 */

	for (block = leaf->block + leaf->block_count -1;
	    block >= leaf->block; block --) {
		if (block->type == 3 || block->type == 1) {
			leaf->mii_block = block;
			break;
		}
	}

	/*
	 * If no MII block, select default, and hope this configuration will
	 * allow the phy to be read/written if it is present
	 */
	dnetp->selected_media_block = leaf->mii_block ?
		leaf->mii_block : leaf->default_block;

	setup_block(macinfo);
	/* XXX function return value ignored */
	(void) mii_create(dip, dnet_mii_write, dnet_mii_read, &dnetp->mii);

	/*
	 * We try PHY 0 LAST because it is less likely to be connected
	 */
	for (phy = 1; phy < 33; phy++)
		if (mii_probe_phy(dnetp->mii, phy % 32) == MII_SUCCESS &&
			mii_init_phy(dnetp->mii, phy % 32) == MII_SUCCESS) {
#ifdef DNETDEBUG
			if (dnetdebug & DNETSENSE)
				cmn_err(CE_NOTE, "dnet: "
				"PHY at address %d", phy % 32);
#endif
			dnetp->phyaddr = phy % 32;
			if (!leaf->mii_block) {
				/* Legacy card, change the leaf node */
				set_leaf(&dnetp->sr, &leaf_phylegacy);
			}
			return;
		}
#ifdef DNETDEBUG
	if (dnetdebug & DNETSENSE)
		cmn_err(CE_NOTE, "dnet: No PHY found");
#endif
}

static ushort_t
dnet_mii_read(dev_info_t *dip, int phy_addr, int reg_num)
{
	gld_mac_info_t *macinfo;
	struct dnetinstance *dnetp;

	ulong_t command_word;
	ulong_t tmp;
	ulong_t data = 0;
	int i;
	int bits_in_ushort = ((sizeof (ushort_t))*8);
	int turned_around = 0;

#ifndef REALMODE
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	dnetp = (struct dnetinstance *)macinfo->gldm_private;
#else
	dnetp = (struct dnetinstance *)dip;
#endif

	/* Write Preamble */
	write_mii(dnetp, MII_PRE, 2*bits_in_ushort);

	/* Prepare command word */
	command_word = (ulong_t)phy_addr << MII_PHY_ADDR_ALIGN;
	command_word |= (ulong_t)reg_num << MII_REG_ADDR_ALIGN;
	command_word |= MII_READ_FRAME;

	write_mii(dnetp, command_word, bits_in_ushort-2);

	mii_tristate(dnetp);

	/* Check that the PHY generated a zero bit the 2nd clock */
	tmp = ddi_io_getl(dnetp->io_handle,
		REG32(dnetp->io_reg, ETHER_ROM_REG));

	turned_around = (tmp & MII_DATA_IN) ? 0 : 1;

	/* read data WORD */
	for (i = 0; i < bits_in_ushort; i++) {
		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, ETHER_ROM_REG), MII_READ);
		drv_usecwait(MII_DELAY);
		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, ETHER_ROM_REG), MII_READ | MII_CLOCK);
		drv_usecwait(MII_DELAY);
		tmp = ddi_io_getl(dnetp->io_handle,
		    REG32(dnetp->io_reg, ETHER_ROM_REG));
		drv_usecwait(MII_DELAY);
		data = (data << 1) | (tmp >> MII_DATA_IN_POSITION) & 0x0001;
	}

	mii_tristate(dnetp);
	return (turned_around ? data: -1);
}

static void
dnet_mii_write(dev_info_t *dip, int phy_addr, int reg_num, int reg_dat)
{
	gld_mac_info_t *macinfo;
	struct dnetinstance *dnetp;
	ulong_t command_word;
	int bits_in_ushort = ((sizeof (ushort_t))*8);
#ifdef REALMODE
	dnetp = (struct dnetinstance *)dip;
#else
	macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	dnetp = (struct dnetinstance *)macinfo->gldm_private;
#endif

	write_mii(dnetp, MII_PRE, 2*bits_in_ushort);

	/* Prepare command word */
	command_word = ((ulong_t)phy_addr << MII_PHY_ADDR_ALIGN);
	command_word |= ((ulong_t)reg_num << MII_REG_ADDR_ALIGN);
	command_word |= (MII_WRITE_FRAME | (ulong_t)reg_dat);

	write_mii(dnetp, command_word, 2*bits_in_ushort);
	mii_tristate(dnetp);
}

/*
 * Write data size bits from mii_data to the MII control lines.
 */
static void
write_mii(struct dnetinstance *dnetp, ulong_t mii_data, int data_size)
{
	int i;
	ulong_t dbit;

	for (i = data_size; i > 0; i--) {
		dbit = ((mii_data >>
		    (31 - MII_WRITE_DATA_POSITION)) & MII_WRITE_DATA);
		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, ETHER_ROM_REG),
		    MII_WRITE | dbit);
		drv_usecwait(MII_DELAY);
		ddi_io_putl(dnetp->io_handle,
		    REG32(dnetp->io_reg, ETHER_ROM_REG),
		    MII_WRITE | MII_CLOCK | dbit);
		drv_usecwait(MII_DELAY);
		mii_data <<= 1;
	}
}

/*
 * Put the MDIO port in tri-state for the turn around bits
 * in MII read and at end of MII management sequence.
 */
static void
mii_tristate(struct dnetinstance *dnetp)
{
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, ETHER_ROM_REG),
	    MII_WRITE_TS);
	drv_usecwait(MII_DELAY);
	ddi_io_putl(dnetp->io_handle, REG32(dnetp->io_reg, ETHER_ROM_REG),
	    MII_WRITE_TS | MII_CLOCK);
	drv_usecwait(MII_DELAY);
}


static void
set_leaf(SROM_FORMAT *sr, LEAF_FORMAT *leaf)
{
	if (sr->leaf && !sr->leaf->is_static)
		kmem_free(sr->leaf, sr->adapters * sizeof (LEAF_FORMAT));
	sr->leaf = leaf;
}

/*
 * Callback from MII module. Makes sure that the CSR registers are
 * configured properly if the PHY changes mode.
 */
/* ARGSUSED */
static void
dnet_mii_link_cb(dev_info_t *dip, int phy, enum mii_phy_state state)
{
	gld_mac_info_t *macinfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
	struct dnetinstance *dnetp =
	    (struct dnetinstance *)macinfo->gldm_private;
	LEAF_FORMAT *leaf = dnetp->sr.leaf + dnetp->leaf;

	if (state == phy_state_linkup) {
		dnetp->mii_up = 1;
		/* XXX function return value ignored */
		(void) mii_getspeed(dnetp->mii,
			dnetp->phyaddr, &dnetp->mii_speed,
			&dnetp->mii_duplex);
		dnetp->selected_media_block = leaf->mii_block;
		setup_block(macinfo);
	} else {
		/* NEEDSWORK: Probably can call find_active_media here */
		dnetp->mii_up = 0;
		if (leaf->default_block->media_code == MEDIA_MII)
			dnetp->selected_media_block = leaf->default_block;
		setup_block(macinfo);
	}
}

/*
 * SROM parsing routines.
 * Refer to the Digital 3.03 SROM spec while reading this! (references refer
 * to this document)
 * Where possible ALL vendor specific changes should be localised here. The
 * SROM data should be capable of describing any programmatic irregularities
 * of DNET cards (via SIA or GP registers, in particular), so vendor specific
 * code elsewhere should not be required
 */
static void
dnet_parse_srom(struct dnetinstance *dnetp, SROM_FORMAT *sr, uchar_t *vi)
{
	unsigned long ether_mfg = 0;
	int i;
	uchar_t *p;

	if (!ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
	    DDI_PROP_DONTPASS, "no_sromconfig", 0))
		dnetp->sr.init_from_srom = check_srom_valid(vi);

	if (dnetp->sr.init_from_srom && dnetp->board_type != DEVICE_ID_21040) {
		/* Section 2/3: General SROM Format/ ID Block */
		p = vi+18;
		sr->version = *p++;
		sr->adapters = *p++;
#ifdef REALMODE
		sr->leaf = realmode_leaf;
#else
		sr->leaf =
		    kmem_zalloc(sr->adapters * sizeof (LEAF_FORMAT), KM_SLEEP);
#endif
		for (i = 0; i < 6; i++)
			sr->netaddr[i] = *p++;

		for (i = 0; i < sr->adapters; i++) {
			uchar_t devno = *p++;
			ushort_t offset = *p++;
			offset |= *p++ << 8;
			sr->leaf[i].device_number = devno;
			parse_controller_leaf(dnetp, sr->leaf+i, vi+offset);
		}
		/*
		 * 'Orrible hack for cogent cards. The 6911A board seems to
		 * have an incorrect SROM. (From the OEMDEMO program
		 * supplied by cogent, it seems that the ROM matches a setup
		 * or a board with a QSI or ICS PHY.
		 */
		for (i = 0; i < 3; i++)
			ether_mfg = (ether_mfg << 8) | sr->netaddr[i];

		if (ether_mfg == ADAPTEC_ETHER) {
			static ushort_t cogent_gprseq[] = {0x821, 0};
			switch (vi[COGENT_SROM_ID]) {
			case COGENT_ANA6911A_C:
			case COGENT_ANA6911AC_C:
#ifdef DNETDEBUG
				if (dnetdebug & DNETTRACE)
					cmn_err(CE_WARN,
					    "Suspected bad GPR sequence."
					    " Making a guess (821,0)");
#endif

#ifdef REALMODE
				realmode_gprseq = cogent_gprseq;
				realmode_gprseq_len = 2;
#else
				/* XXX function return value ignored */
				(void) ddi_prop_create(DDI_DEV_T_NONE,
				    dnetp->devinfo,
				    DDI_PROP_CANSLEEP, "gpr-sequence",
				    (caddr_t)cogent_gprseq,
				    sizeof (cogent_gprseq));
#endif
				break;
			}
		}
	} else {
		/*
		 * Adhoc SROM, check for some cards which need special handling
		 * Assume vendor info contains ether address in first six bytes
		 */

		uchar_t *mac = vi + ddi_getprop(DDI_DEV_T_ANY, dnetp->devinfo,
		    DDI_PROP_DONTPASS, macoffset_propname, 0);

		for (i = 0; i < 6; i++)
			sr->netaddr[i] = mac[i];

		if (dnetp->board_type == DEVICE_ID_21140) {
			for (i = 0; i < 3; i++)
				ether_mfg = (ether_mfg << 8) | mac[i];

			switch (ether_mfg) {
			case ASANTE_ETHER:
				dnetp->vendor_21140 = ASANTE_TYPE;
				dnetp->vendor_revision = 0;
				set_leaf(sr, &leaf_asante);
				sr->adapters = 1;
				break;

			case COGENT_ETHER:
			case ADAPTEC_ETHER:
				dnetp->vendor_21140 = COGENT_EM_TYPE;
				dnetp->vendor_revision =
				    vi[VENDOR_REVISION_OFFSET];
				set_leaf(sr, &leaf_cogent_100);
				sr->adapters = 1;
				break;

			default:
				dnetp->vendor_21140 = DEFAULT_TYPE;
				dnetp->vendor_revision = 0;
				set_leaf(sr, &leaf_default_100);
				sr->adapters = 1;
				break;
			}
		} else if (dnetp->board_type == DEVICE_ID_21041) {
			set_leaf(sr, &leaf_21041);
		} else if (dnetp->board_type == DEVICE_ID_21040) {
			set_leaf(sr, &leaf_21040);
		}
	}
}

/* Section 4.2, 4.3, 4.4, 4.5 */
static void
parse_controller_leaf(struct dnetinstance *dnetp, LEAF_FORMAT *leaf,
	uchar_t *vi)
{
	int i;

	leaf->selected_contype = *vi++;
	leaf->selected_contype |= *vi++ << 8;

	if (dnetp->board_type == DEVICE_ID_21140) /* Sect. 4.3 */
		leaf->gpr = *vi++;

	leaf->block_count = *vi++;

	if (leaf->block_count > MAX_MEDIA) {
		cmn_err(CE_WARN, "dnet: Too many media in SROM!");
		leaf->block_count = 1;
	}
	for (i = 0; i <= leaf->block_count; i++) {
		vi = parse_media_block(dnetp, leaf->block + i, vi);
		if (leaf->block[i].command & CMD_DEFAULT_MEDIUM)
			leaf->default_block = leaf->block+i;
	}
	/* No explicit default block: use last in the ROM */
	if (leaf->default_block == NULL)
		leaf->default_block = leaf->block + leaf->block_count -1;

}

static uchar_t *
parse_media_block(struct dnetinstance *dnetp, media_block_t *block, uchar_t *vi)
{
	int i;

	/*
	 * There are three kinds of media block we need to worry about:
	 * The 21041 blocks.
	 * 21140 blocks from a version 1 SROM
	 * 2114[023] block from a version 3 SROM
	 */

	if (dnetp->board_type == DEVICE_ID_21041) {
		/* Section 4.2 */
		block->media_code = *vi & 0x3f;
		block->type = 2;
		if (*vi++ & 0x40) {
			block->un.sia.csr13 = *vi++;
			block->un.sia.csr13 |= *vi++ << 8;
			block->un.sia.csr14 = *vi++;
			block->un.sia.csr14 |= *vi++ << 8;
			block->un.sia.csr15 = *vi++;
			block->un.sia.csr15 |= *vi++ << 8;
		} else {
			/* No media data (csrs 13,14,15). Insert defaults */
			switch (block->media_code) {
			case MEDIA_TP:
				block->un.sia.csr13 = 0xef01;
				block->un.sia.csr14 = 0x7f3f;
				block->un.sia.csr15 = 0x0008;
				break;
			case MEDIA_TP_FD:
				block->un.sia.csr13 = 0xef01;
				block->un.sia.csr14 = 0x7f3d;
				block->un.sia.csr15 = 0x0008;
				break;
			case MEDIA_BNC:
				block->un.sia.csr13 = 0xef09;
				block->un.sia.csr14 = 0x0705;
				block->un.sia.csr15 = 0x0006;
				break;
			case MEDIA_AUI:
				block->un.sia.csr13 = 0xef09;
				block->un.sia.csr14 = 0x0705;
				block->un.sia.csr15 = 0x000e;
				break;
			}
		}
	} else  if (*vi & 0x80) {  /* Extended format: Section 4.3.2.2 */
		int blocklen = *vi++ & 0x7f;
		block->type = *vi++;
		switch (block->type) {
		case 0: /* "non-MII": Section 4.3.2.2.1 */
			block->media_code = (*vi++) & 0x3f;
			block->gprseqlen = 1;
			block->gprseq[0] = *vi++;
			block->command = *vi++;
			block->command |= *vi++ << 8;
			break;

		case 1: /* MII/PHY: Section 4.3.2.2.2 */
			block->command = CMD_PS;
			block->media_code = MEDIA_MII;
				/* This is whats needed in CSR6 */

			block->un.mii.phy_num = *vi++;
			block->gprseqlen = *vi++;

			for (i = 0; i < block->gprseqlen; i++)
				block->gprseq[i] = *vi++;
			block->rstseqlen = *vi++;
			for (i = 0; i < block->rstseqlen; i++)
				block->rstseq[i] = *vi++;

			block->un.mii.mediacaps = *vi++;
			block->un.mii.mediacaps |= *vi++ << 8;
			block->un.mii.nwayadvert = *vi++;
			block->un.mii.nwayadvert |= *vi++ << 8;
			block->un.mii.fdxmask = *vi++;
			block->un.mii.fdxmask |= *vi++ << 8;
			block->un.mii.ttmmask = *vi++;
			block->un.mii.ttmmask |= *vi++ << 8;
			break;

		case 2: /* SIA Media: Section 4.4.2.1.1 */
			block->media_code = *vi & 0x3f;
			if (*vi++ & 0x40) {
				block->un.sia.csr13 = *vi++;
				block->un.sia.csr13 |= *vi++ << 8;
				block->un.sia.csr14 = *vi++;
				block->un.sia.csr14 |= *vi++ << 8;
				block->un.sia.csr15 = *vi++;
				block->un.sia.csr15 |= *vi++ << 8;
			} else {
				/*
				 * SIA values not provided by SROM; provide
				 * defaults. See appendix D of 2114[23] manuals.
				 */
				switch (block->media_code) {
				case MEDIA_BNC:
					block->un.sia.csr13 = 0x0009;
					block->un.sia.csr14 = 0x0705;
					block->un.sia.csr15 = 0x0000;
					break;
				case MEDIA_AUI:
					block->un.sia.csr13 = 0x0009;
					block->un.sia.csr14 = 0x0705;
					block->un.sia.csr15 = 0x0008;
					break;
				case MEDIA_TP:
					block->un.sia.csr13 = 0x0001;
					block->un.sia.csr14 = 0x7f3f;
					block->un.sia.csr15 = 0x0000;
					break;
				case MEDIA_TP_FD:
					block->un.sia.csr13 = 0x0001;
					block->un.sia.csr14 = 0x7f3d;
					block->un.sia.csr15 = 0x0000;
					break;
				default:
					block->un.sia.csr13 = 0x0000;
					block->un.sia.csr14 = 0x0000;
					block->un.sia.csr15 = 0x0000;
				}
			}

			/* Treat GP control/data as a GPR sequence */
			block->gprseqlen = 2;
			block->gprseq[0] = *vi++;
			block->gprseq[0] |= *vi++ << 8;
			block->gprseq[0] |= GPR_CONTROL_WRITE;
			block->gprseq[1] = *vi++;
			block->gprseq[1] |= *vi++ << 8;
			break;

		case 3: /* MII/PHY : Section 4.4.2.1.2 */
			block->command = CMD_PS;
			block->media_code = MEDIA_MII;
			block->un.mii.phy_num = *vi++;

			block->gprseqlen = *vi++;
			for (i = 0; i < block->gprseqlen; i++) {
				block->gprseq[i] = *vi++;
				block->gprseq[i] |= *vi++ << 8;
			}

			block->rstseqlen = *vi++;
			for (i = 0; i < block->rstseqlen; i++) {
				block->rstseq[i] = *vi++;
				block->rstseq[i] |= *vi++ << 8;
			}
			block->un.mii.mediacaps = *vi++;
			block->un.mii.mediacaps |= *vi++ << 8;
			block->un.mii.nwayadvert = *vi++;
			block->un.mii.nwayadvert |= *vi++ << 8;
			block->un.mii.fdxmask = *vi++;
			block->un.mii.fdxmask |= *vi++ << 8;
			block->un.mii.ttmmask = *vi++;
			block->un.mii.ttmmask |= *vi++ << 8;
			block->un.mii.miiintr |= *vi++;
			break;

		case 4: /* SYM Media: 4.5.2.1.3 */
			block->media_code = *vi & 0x3f;
			/* Treat GP control and data as a GPR sequence */
			block->gprseqlen = 2;
			block->gprseq[0] = *vi++;
			block->gprseq[0] |= *vi++ << 8;
			block->gprseq[0] |= GPR_CONTROL_WRITE;
			block->gprseq[1]  = *vi++;
			block->gprseq[1] |= *vi++ << 8;
			block->command = *vi++;
			block->command = *vi++ << 8;
			break;

		case 5: /* GPR reset sequence:  Section 4.5.2.1.4 */
			block->rstseqlen = *vi++;
			for (i = 0; i < block->rstseqlen; i++)
				block->rstseq[i] = *vi++;
			break;

		default: /* Unknown media block. Skip it. */
			cmn_err(CE_WARN, "dnet: Unsupported SROM block.");
			vi += blocklen;
			break;
		}
	} else { /* Compact format (or V1 SROM): Section 4.3.2.1 */
		block->type = 0;
		block->media_code = *vi++ & 0x3f;
		block->gprseqlen = 1;
		block->gprseq[0] = *vi++;
		block->command = *vi++;
		block->command |= (*vi++) << 8;
	}
	return (vi);
}


/*
 * An alternative to doing this would be to store the legacy ROMs in binary
 * format in the conf file, and in read_srom, pick out the data. This would
 * then allow the parser to continue on as normal. This makes it a little
 * easier to read.
 */
static void
setup_legacy_blocks()
{
	LEAF_FORMAT *leaf;
	media_block_t *block;

	/* Default FAKE SROM */
	leaf = &leaf_default_100;
	leaf->is_static = 1;
	leaf->default_block = &leaf->block[3];
	leaf->block_count = 4; /* 100 cards are highly unlikely to have BNC */
	block = leaf->block;
	block->media_code = MEDIA_TP_FD;
	block->type = 0;
	block->command = 0x8e;  /* PCS, PS off, media sense: bit7, pol=1 */
	block++;
	block->media_code = MEDIA_TP;
	block->type = 0;
	block->command = 0x8e;  /* PCS, PS off, media sense: bit7, pol=1 */
	block++;
	block->media_code = MEDIA_SYM_SCR_FD;
	block->type = 0;
	block->command = 0x6d;  /* PCS, PS, SCR on, media sense: bit6, pol=0 */
	block++;
	block->media_code = MEDIA_SYM_SCR;
	block->type = 0;
	block->command = 0x406d; /* PCS, PS, SCR on, media sense: bit6, pol=0 */

	/* COGENT FAKE SROM */
	leaf = &leaf_cogent_100;
	leaf->is_static = 1;
	leaf->default_block = &leaf->block[4];
	leaf->block_count = 5; /* 100TX, 100TX-FD, 10T 10T-FD, BNC */
	block = leaf->block; /* BNC */
	block->media_code = MEDIA_BNC;
	block->type = 0;
	block->command =  0x8000; /* No media sense, PCS, SCR, PS all off */
	block->gprseqlen = 2;
	block->rstseqlen = 0;
	block->gprseq[0] = 0x13f;
	block->gprseq[1] = 1;

	block++;
	block->media_code = MEDIA_TP_FD;
	block->type = 0;
	block->command = 0x8e;  /* PCS, PS off, media sense: bit7, pol=1 */
	block->gprseqlen = 2;
	block->rstseqlen = 0;
	block->gprseq[0] = 0x13f;
	block->gprseq[1] = 0x26;

	block++; /* 10BaseT */
	block->media_code = MEDIA_TP;
	block->type = 0;
	block->command = 0x8e;  /* PCS, PS off, media sense: bit7, pol=1 */
	block->gprseqlen = 2;
	block->rstseqlen = 0;
	block->gprseq[0] = 0x13f;
	block->gprseq[1] = 0x3e;

	block++; /* 100BaseTX-FD */
	block->media_code = MEDIA_SYM_SCR_FD;
	block->type = 0;
	block->command = 0x6d;  /* PCS, PS, SCR on, media sense: bit6, pol=0 */
	block->gprseqlen = 2;
	block->rstseqlen = 0;
	block->gprseq[0] = 0x13f;
	block->gprseq[1] = 1;

	block++; /* 100BaseTX */
	block->media_code = MEDIA_SYM_SCR;
	block->type = 0;
	block->command = 0x406d; /* PCS, PS, SCR on, media sense: bit6, pol=0 */
	block->gprseqlen = 2;
	block->rstseqlen = 0;
	block->gprseq[0] = 0x13f;
	block->gprseq[1] = 1;

	/* Generic legacy card with a PHY. */
	leaf = &leaf_phylegacy;
	leaf->block_count = 1;
	leaf->mii_block = leaf->block;
	leaf->default_block = &leaf->block[0];
	leaf->is_static = 1;
	block = leaf->block;
	block->media_code = MEDIA_MII;
	block->type = 1; /* MII Block type 1 */
	block->command = 1; /* Port select */
	block->gprseqlen = 0;
	block->rstseqlen = 0;

	/* ASANTE FAKE SROM */
	leaf = &leaf_asante;
	leaf->is_static = 1;
	leaf->default_block = &leaf->block[0];
	leaf->block_count = 1;
	block = leaf->block;
	block->media_code = MEDIA_MII;
	block->type = 1; /* MII Block type 1 */
	block->command = 1; /* Port select */
	block->gprseqlen = 3;
	block->rstseqlen = 0;
	block->gprseq[0] = 0x180;
	block->gprseq[1] = 0x80;
	block->gprseq[2] = 0x0;

	/* LEGACY 21041 card FAKE SROM */
	leaf = &leaf_21041;
	leaf->is_static = 1;
	leaf->block_count = 4;  /* SIA Blocks for TP, TPfd, BNC, AUI */
	leaf->default_block = &leaf->block[3];

	block = leaf->block;
	block->media_code = MEDIA_AUI;
	block->type = 2;
	block->un.sia.csr13 = 0xef09;
	block->un.sia.csr14 = 0x0705;
	block->un.sia.csr15 = 0x000e;

	block++;
	block->media_code = MEDIA_TP_FD;
	block->type = 2;
	block->un.sia.csr13 = 0xef01;
	block->un.sia.csr14 = 0x7f3d;
	block->un.sia.csr15 = 0x0008;

	block++;
	block->media_code = MEDIA_BNC;
	block->type = 2;
	block->un.sia.csr13 = 0xef09;
	block->un.sia.csr14 = 0x0705;
	block->un.sia.csr15 = 0x0006;

	block++;
	block->media_code = MEDIA_TP;
	block->type = 2;
	block->un.sia.csr13 = 0xef01;
	block->un.sia.csr14 = 0x7f3f;
	block->un.sia.csr15 = 0x0008;

	/* LEGACY 21040 card FAKE SROM */
	leaf = &leaf_21040;
	leaf->is_static = 1;
	leaf->block_count = 4;  /* SIA Blocks for TP, TPfd, BNC, AUI */
	block = leaf->block;
	block->media_code = MEDIA_AUI;
	block->type = 2;
	block->un.sia.csr13 = 0x8f09;
	block->un.sia.csr14 = 0x0705;
	block->un.sia.csr15 = 0x000e;
	block++;
	block->media_code = MEDIA_TP_FD;
	block->type = 2;
	block->un.sia.csr13 = 0x0f01;
	block->un.sia.csr14 = 0x7f3d;
	block->un.sia.csr15 = 0x0008;
	block++;
	block->media_code = MEDIA_BNC;
	block->type = 2;
	block->un.sia.csr13 = 0xef09;
	block->un.sia.csr14 = 0x0705;
	block->un.sia.csr15 = 0x0006;
	block++;
	block->media_code = MEDIA_TP;
	block->type = 2;
	block->un.sia.csr13 = 0x8f01;
	block->un.sia.csr14 = 0x7f3f;
	block->un.sia.csr15 = 0x0008;
}

static void
dnet_print_srom(SROM_FORMAT *sr)
{
	int i;
	uchar_t *a = sr->netaddr;
	cmn_err(CE_NOTE, "SROM Dump: %d. ver %d, Num adapters %d,"
		"Addr:%x:%x:%x:%x:%x:%x",
		sr->init_from_srom, sr->version, sr->adapters,
		a[0], a[1], a[2], a[3], a[4], a[5]);

	for (i = 0; i < sr->adapters; i++)
		dnet_dump_leaf(sr->leaf+i);
}

static void
dnet_dump_leaf(LEAF_FORMAT *leaf)
{
	int i;
	cmn_err(CE_NOTE, "Leaf: Device %d, block_count %d, gpr: %x",
		leaf->device_number, leaf->block_count, leaf->gpr);
	for (i = 0; i < leaf->block_count; i++)
		dnet_dump_block(leaf->block+i);
}

static void
dnet_dump_block(media_block_t *block)
{
	cmn_err(CE_NOTE, "Block(%p): type %x, media %s, command: %x ",
	    (void *)block,
	    block->type, media_str[block->media_code], block->command);
	dnet_dumpbin("\tGPR Seq", (uchar_t *)block->gprseq, 2,
	    block->gprseqlen *2);
	dnet_dumpbin("\tGPR Reset", (uchar_t *)block->rstseq, 2,
	    block->rstseqlen *2);
	switch (block->type) {
	case 1: case 3:
		cmn_err(CE_NOTE, "\tMII Info: phy %d, nway %x, fdx"
			"%x, ttm %x, mediacap %x",
			block->un.mii.phy_num, block->un.mii.nwayadvert,
			block->un.mii.fdxmask, block->un.mii.ttmmask,
			block->un.mii.mediacaps);
		break;
	case 2:
		cmn_err(CE_NOTE, "\tSIA Regs: CSR13:%lx, CSR14:%lx, CSR15:%lx",
			block->un.sia.csr13, block->un.sia.csr14,
			block->un.sia.csr15);
		break;
	}
}


/* Utility to print out binary info dumps. Handy for SROMs, etc */

static int
hexcode(unsigned val)
{
	if (val <= 9)
		return (val +'0');
	if (val <= 15)
		return (val + 'a' - 10);
	return (-1);
}

static void
dnet_dumpbin(char *msg, unsigned char *data, int size, int len)
{
	char hex[128], *p = hex;
	char ascii[128], *q = ascii;
	int i, j;

	if (!len)
		return;

	for (i = 0; i < len; i += size) {
		for (j = size - 1; j >= 0; j--) { /* PORTABILITY: byte order */
			*p++ = hexcode(data[i+j] >> 4);
			*p++ = hexcode(data[i+j] & 0xf);
			*q++ = (data[i+j] < 32 || data[i+j] > 127) ?
			    '.' : data[i];
		}
		*p++ = ' ';
		if (q-ascii >= 8) {
			*p = *q = 0;
			cmn_err(CE_NOTE, "%s: %s\t%s", msg, hex, ascii);
			p = hex;
			q = ascii;
		}
	}
	if (p != hex) {
		while ((p - hex) < 8*3)
		    *p++ = ' ';
		*p = *q = 0;
		cmn_err(CE_NOTE, "%s: %s\t%s", msg, hex, ascii);
	}
}

#ifdef DNETDEBUG
void
dnet_usectimeout(struct dnetinstance *dnetp, ulong_t usecs, int contin,
    timercb_t cback)
{
	dnetp->timer.start_ticks = (usecs * 100) / 8192;
	dnetp->timer.cb = cback;
	outl(dnetp->io_reg + GP_TIMER_REG, dnetp->timer.start_ticks |
		(contin ? GPTIMER_CONT : 0));
	if (dnetp->timer.cb)
		enable_interrupts(dnetp, 1);
}

ulong_t
dnet_usecelapsed(struct dnetinstance *dnetp)
{
	ulong_t ticks = dnetp->timer.start_ticks -
	    (inl(dnetp->io_reg + GP_TIMER_REG) & 0xffff);
	return ((ticks * 8192) / 100);
}

/* ARGSUSED */
void
dnet_timestamp(struct dnetinstance *dnetp,  char *buf)
{
	unsigned long elapsed = dnet_usecelapsed(dnetp);
	char loc[32], *p = loc;
	int firstdigit = 1;
	unsigned long divisor;

	while (*p++ = *buf++)
		;
	p--;

	for (divisor = 1000000000; divisor /= 10; ) {
		int digit = (elapsed / divisor);
		elapsed -= digit * divisor;
		if (!firstdigit || digit) {
			*p++ = digit + '0';
			firstdigit = 0;
		}

	}

	/* Actual zero, output it */
	if (firstdigit)
		*p++ = '0';

	*p++ = '-';
	*p++ = '>';
	*p++ = 0;
#ifdef REALMODE
	putstr(loc);
#else
	printf(loc);
#endif
	dnet_usectimeout(dnetp, 1000000, 0, 0);
}

#endif
