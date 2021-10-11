/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)le.c	1.136	99/10/22 SMI"

/*
 *  SunOS 5.0 Multithreaded STREAMS DLPI LANCE (Am7990) Ethernet Driver
 *
 *  Refer to document:
 *	"SunOS 5.0 STREAMS LE Driver", 800-7663-01.
 */

#include	<sys/types.h>
#include	<sys/errno.h>
#include	<sys/debug.h>
#include	<sys/time.h>
#include	<sys/sysmacros.h>
#include	<sys/systm.h>
#include	<sys/user.h>
#include	<sys/stropts.h>
#include	<sys/stream.h>
#include	<sys/strlog.h>
#include	<sys/strsubr.h>
#include	<sys/cmn_err.h>
#include	<sys/cpu.h>
#include	<sys/kmem.h>
#include	<sys/conf.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<sys/ksynch.h>
#include	<sys/stat.h>
#include	<sys/kstat.h>
#include	<sys/vtrace.h>
#include	<sys/strsun.h>
#include	<sys/dlpi.h>
#include	<sys/ethernet.h>
#include	<sys/lance.h>
#include	<sys/varargs.h>
#include	<sys/le.h>
#ifdef NETDEBUGGER
#include	<netinet/in.h>
#include	<netinet/in_systm.h>
#include	<netinet/ip.h>
#include	<netinet/udp.h>
unsigned int dle_virt_addr;
unsigned int dle_dma_addr;
unsigned int dle_regs;
unsigned int dle_k_ib;

/* there is a better way to do this */
#define	DLE_MEM_SIZE 0x910
#define	DEBUG_PORT_NUM 1080
#endif /* NETDEBUGGER */

#define	KIOIP	KSTAT_INTR_PTR(lep->le_intrstats)

/*
 * Function prototypes.
 */
static	int leidentify(dev_info_t *);
static	int leprobe(dev_info_t *);
static	int leattach(dev_info_t *, ddi_attach_cmd_t);
static	int ledetach(dev_info_t *, ddi_detach_cmd_t);
static	void lestatinit(struct le *);
static	int leinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static	void leallocthings(struct le *);
static	int leopen(queue_t *, dev_t *, int, int, cred_t *);
static	int leclose(queue_t *rq, int flag, int otyp, cred_t *credp);
static	int lewput(queue_t *, mblk_t *);
static	int lewsrv(queue_t *);
static	void leproto(queue_t *, mblk_t *);
static	void leioctl(queue_t *, mblk_t *);
static	void le_dl_ioc_hdr_info(queue_t *, mblk_t *);
static	void leareq(queue_t *, mblk_t *);
static	void ledreq(queue_t *, mblk_t *);
static	void ledodetach(struct lestr *);
static	void lebreq(queue_t *, mblk_t *);
static	void leubreq(queue_t *, mblk_t *);
static	void leireq(queue_t *, mblk_t *);
static	void leponreq(queue_t *, mblk_t *);
static	void lepoffreq(queue_t *, mblk_t *);
static	void leemreq(queue_t *, mblk_t *);
static	void ledmreq(queue_t *, mblk_t *);
static	void lepareq(queue_t *, mblk_t *);
static	void lespareq(queue_t *, mblk_t *);
static	void leudreq(queue_t *, mblk_t *);
static	int lestart(queue_t *, mblk_t *, struct le *);
static	uint_t leintr(caddr_t arg);
static	void lereclaim(struct le *);
static	void lewenable(struct le *);
static	struct lestr *leaccept(struct lestr *, struct le *, int,
	struct ether_addr *);
static	struct lestr *lepaccept(struct lestr *, struct le *, int,
	struct	ether_addr *);
static	void	lesetipq(struct le *);
static	void leread(struct le *, volatile struct lmd *);
static	void lesendup(struct le *, mblk_t *, struct lestr *(*)());
static	mblk_t *leaddudind(struct le *, mblk_t *, struct ether_addr *,
	struct ether_addr *, t_uscalar_t, t_uscalar_t);
static	int lemcmatch(struct lestr *, struct ether_addr *);
static	int leinit(struct le *);
static	void leuninit(struct le *lep);
static	struct lebuf *legetbuf(struct le *, int);
static	void lefreebuf(struct lebuf *);
static	void lermdinit(struct le *, volatile struct lmd *, struct lebuf *);
static	void le_rcv_error(struct le *, struct lmd *);
static	int le_xmit_error(struct le *, struct lmd *);
static	int le_chip_error(struct le *);
static	void lerror(dev_info_t *dip, char *fmt, ...);
static	void leopsadd(struct leops *);
static	struct leops *leopsfind(dev_info_t *);
static	int le_check_ledma(struct le *);
static	void le_watchdog(void *);

static	struct	module_info	leminfo = {
	LEIDNUM,	/* mi_idnum */
	LENAME,		/* mi_idname */
	LEMINPSZ,	/* mi_minpsz */
	LEMAXPSZ,	/* mi_maxpsz */
	LEHIWAT,	/* mi_hiwat */
	LELOWAT		/* mi_lowat */
};

static	struct	qinit	lerinit = {
	NULL,		/* qi_putp */
	NULL,		/* qi_srvp */
	leopen,		/* qi_qopen */
	leclose,	/* qi_qclose */
	NULL,		/* qi_qadmin */
	&leminfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static	struct	qinit	lewinit = {
	lewput,		/* qi_putp */
	lewsrv,		/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&leminfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct	streamtab	le_info = {
	&lerinit,	/* st_rdinit */
	&lewinit,	/* st_wrinit */
	NULL,		/* st_muxrinit */
	NULL		/* st_muxwrinit */
};

static	struct	cb_ops	cb_le_ops = {
	nulldev,			/* cb_open */
	nulldev,			/* cb_close */
	nodev,				/* cb_strategy */
	nodev,				/* cb_print */
	nodev,				/* cb_dump */
	nodev,				/* cb_read */
	nodev,				/* cb_write */
	nodev,				/* cb_ioctl */
	nodev,				/* cb_devmap */
	nodev,				/* cb_mmap */
	nodev,				/* cb_segmap */
	nochpoll,			/* cb_chpoll */
	ddi_prop_op,			/* cb_prop_op */
	&le_info,			/* cb_stream */
	D_MP | D_HOTPLUG,		/* cb_flag */
	CB_REV,				/* rev */
	nodev,				/* int (*cb_aread)() */
	nodev				/* int (*cb_awrite)() */
};

static	struct	dev_ops	le_ops = {
	DEVO_REV,	/* devo_rev */
	0,		/* devo_refcnt */
	leinfo,		/* devo_getinfo */
	leidentify,	/* devo_identify */
	leprobe,	/* devo_probe */
	leattach,	/* devo_attach */
	ledetach,	/* devo_detach */
	nodev,		/* devo_reset */
	&cb_le_ops,	/* devo_cb_ops */
	(struct bus_ops *)NULL,	/* devo_bus_ops */
	ddi_power	/* devo_power */
};

/*
 * The lance chip's dma capabilities are for 24 bits of
 * address. For the sun4c, the top byte is forced to 0xff
 * by the DMA chip so the lance can address 0xff000000-0xffffffff.
 *
 * Note 1:
 *	The lance has a 16-bit data port, so the wordsize
 *	is 16 bits. The initialization block for the lance
 *	has to be aligned on a word boundary. The message
 *	descriptors must be aligned on a quadword boundary
 *	(8 byte). The actual data buffers can be aligned
 *	on a byte boundary.
 */
#define	LEDLIMADDRLO	(0xff000000)
#define	LEDLIMADDRHI	(0xffffffff)
static ddi_dma_lim_t le_dma_limits = {
	(uint_t)LEDLIMADDRLO,	/* dlim_addr_lo */
	(uint_t)LEDLIMADDRHI,	/* dlim_addr_hi */
	(uint_t)((1<<24)-1),	/* dlim_cntr_max */
	(uint_t)0x3f,		/* dlim_burstsizes (1, 2, 8) */
	0x1,			/* dlim_minxfer */
	1024			/* dlim_speed */
};


/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern	struct	mod_ops	mod_driverops;

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module.  This one is a driver */
	"Lance Ethernet Driver v1.136",
	&le_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

/*
 * XXX Autoconfiguration lock:  We want to initialize all the global
 * locks at _init().  However, we do not have the cookie required which
 * is returned in ddi_add_intr(), which in turn is usually called at attach
 * time.
 */
static  kmutex_t	leautolock;
static int created_global_mutexes = 0;

/*
 * Linked list of "le" structures - one per device.
 */
static struct le *ledev = NULL;

/*
 * Linked list of active (inuse) driver Streams.
 */
static	struct	lestr	*lestrup = NULL;
static	krwlock_t	lestruplock;

/*
 * Linked list of device "opsvec" structures.
 */
static	struct	leops	*leops = NULL;

#define	LE_SYNC_INIT
#ifdef	LE_SYNC_INIT
static int	le_sync_init = 1;
#endif /* LE_SYNC_INIT */

/*
 * Single private "global" lock for the few rare conditions
 * we want single-threaded.
 */
static	kmutex_t	lelock;

/*
 * Watchdog timer variables
 */
#define	LEWD_FLAG_TX_TIMEOUT	0x1	/* reinit on tx timeout */
#define	LEWD_FLAG_RX_TIMEOUT	0x2	/* reinit on rx timeout */
#define	E_DRAIN			(1 << 10)
#define	E_DIRTY			0x3000000
#define	E_ADDR_MASK		0xffffff

static uint_t	lewdflag	= 3;	/* WD timeout enabled by LEWD_FLAG */
static clock_t	lewdinterval	= 1000;	/* WD routine frequency in msec */
static clock_t	lewdrx_timeout	= 1000;		/* rx timeout in msec */
static clock_t	lewdtx_timeout	= 1000;		/* tx timeout in msec */

int
_init(void)
{
	int 	status;

	mutex_init(&leautolock, NULL, MUTEX_DRIVER, NULL);
	status = mod_install(&modlinkage);
	if (status != 0) {
		mutex_destroy(&leautolock);
		return (status);
	}
	return (0);
}

int
_fini(void)
{
	int	status;

	status = mod_remove(&modlinkage);
	if (status != 0)
		return (status);
	if (created_global_mutexes) {
		mutex_destroy(&lelock);
		rw_destroy(&lestruplock);
	}
	mutex_destroy(&leautolock);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Patchable debug flag.
 * Set this to nonzero to enable *all* error messages.
 */
static	int	ledebug = 0;

/*
 * Allocate and zero-out "number" structures
 * each of type "structure" in kernel memory.
 */
#define	GETSTRUCT(structure, number)   \
	((structure *)kmem_zalloc(\
		(size_t)(sizeof (structure) * (number)), KM_SLEEP))

/*
 * Translate a kernel virtual address to i/o address.
 */
#define	LEBUFIOADDR(lep, a) \
	((uint32_t)((lep)->le_bufiobase + \
		((uintptr_t)(a) - (uintptr_t)(lep)->le_bufbase)))
#define	LEIOPBIOADDR(lep, a) \
	((uint32_t)((lep)->le_iopbiobase + \
		((uintptr_t)(a) - (uintptr_t)(lep)->le_iopbkbase)))

/*
 * ddi_dma_sync() a buffer.
 */
#define	LESYNCBUF(lep, a, size, who) \
	if (((lep)->le_flags & LESLAVE) == 0) \
		(void) ddi_dma_sync((lep)->le_bufhandle, \
			((uintptr_t)(a) - (uintptr_t)(lep)->le_bufbase), \
			(size), \
			(who))

/*
 * XXX
 * Define LESYNCIOPB to nothing for now.
 * If/when we have PSO-mode kernels running which really need
 * to sync something during a ddi_dma_sync() of iopb-allocated memory,
 * then this can go back in, but for now we take it out
 * to save some microseconds.
 */
#define	LESYNCIOPB(lep, a, size, who)

#define	LESAPMATCH(sap, type, flags) ((sap == type)? 1 : \
	((flags & SLALLSAP)? 1 : \
	((sap <= ETHERMTU) && (sap > 0) && (type <= ETHERMTU))? 1 : 0))

#define	SAMEMMUPAGE(a, b) \
	(((uintptr_t)(a) & MMU_PAGEMASK) == ((uintptr_t)(b) & MMU_PAGEMASK))

/*
 * Ethernet broadcast address definition.
 */
static	struct ether_addr	etherbroadcastaddr = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/*
 * MIB II broadcast/multicast packets
 */
#define	IS_BROADCAST(ehp) \
		(ether_cmp(&ehp->ether_dhost, &etherbroadcastaddr) == 0)
#define	IS_MULTICAST(ehp) \
		((ehp->ether_dhost.ether_addr_octet[0] & 01) == 1)
#define	BUMP_InNUcast(lep, ehp) \
		if (IS_BROADCAST(ehp)) { \
			lep->le_brdcstrcv++; \
		} else if (IS_MULTICAST(ehp)) { \
			lep->le_multircv++; \
		}
#define	BUMP_OutNUcast(lep, ehp) \
		if (IS_BROADCAST(ehp)) { \
			lep->le_brdcstxmt++; \
		} else if (IS_MULTICAST(ehp)) { \
			lep->le_multixmt++; \
		}

/*
 * Resource amounts.
 *	For now, at least, these never change, and resources
 *	are allocated once and for all at leattach() time.
 */
static int	le_ntmdp2 = 7;	/* power of 2 Transmit Ring Descriptors */
static int	le_nrmdp2 = 5;	/* power of 2 Receive Ring Descriptors */
static int	le_nbufs = 64;	/* # buffers allocated for xmit/recv pool */

/*
 * Our DL_INFO_ACK template.
 */
static	dl_info_ack_t leinfoack = {
	DL_INFO_ACK,			/* dl_primitive */
	ETHERMTU,			/* dl_max_sdu */
	0,				/* dl_min_sdu */
	LEADDRL,			/* dl_addr_length */
	DL_ETHER,			/* dl_mac_type */
	0,				/* dl_reserved */
	0,				/* dl_current_state */
	-2,				/* dl_sap_length */
	DL_CLDLS,			/* dl_service_mode */
	0,				/* dl_qos_length */
	0,				/* dl_qos_offset */
	0,				/* dl_range_length */
	0,				/* dl_range_offset */
	DL_STYLE2,			/* dl_provider_style */
	sizeof (dl_info_ack_t),		/* dl_addr_offset */
	DL_VERSION_2,			/* dl_version */
	ETHERADDRL,			/* dl_brdcst_addr_length */
	sizeof (dl_info_ack_t) + LEADDRL,	/* dl_brdcst_addr_offset */
	0				/* dl_growth */
};

/*
 * Identify device.
 */
static int
leidentify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "le") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/*
 * Probe for device.
 */
static int
leprobe(dev_info_t *dip)
{
	struct lanceregs *regsp;
	int	i;

	if (ddi_dev_is_sid(dip) == DDI_FAILURE) {
		/* XXX - need better test */
		if (ddi_map_regs(dip, 0, (caddr_t *)&regsp, 0, 0)
			== DDI_SUCCESS) {
			i = ddi_poke16(dip, (short *)&regsp->lance_rdp, 0);
			ddi_unmap_regs(dip, 0, (caddr_t *)&regsp, 0, 0);
			if (i)
				return (DDI_PROBE_FAILURE);
		} else
			return (DDI_PROBE_FAILURE);
	}
	return (DDI_PROBE_SUCCESS);
}

static char tlt_disabled[] =
	"?NOTICE: le%d: 'tpe-link-test?' is false.  "
	"TPE/AUI autoselection disabled.\n";
static char tlt_setprop[] =
	"?NOTICE: le%d: Set 'tpe-link-test?' to true if using AUI connector\n";


/*
 * Calculate the bit in the multicast address filter that selects the given
 * address.
 */

static uint32_t
leladrf_bit(struct ether_addr *addr)
{
	uchar_t *cp;
	uint32_t crc;
	uint_t c;
	int len;
	int	j;

	cp = (unsigned char *) addr;
	c = *cp;
	crc = (uint32_t)0xffffffff;
	len = 6;
	while (len-- > 0) {
		c = *cp++;
		for (j = 0; j < 8; j++) {
			if ((c & 0x01) ^ (crc & 0x01)) {
				crc >>= 1;
				/* polynomial */
				crc = crc ^ 0xedb88320;
			} else
				crc >>= 1;
			c >>= 1;
		}
	}
	/* Just want the 6 most significant bits. */
	crc = crc >> 26;
	return (crc);
}


/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
static int
leattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct lestr *lsp;
	struct le *lep;
	struct lanceregs *regsp;
	ddi_iblock_cookie_t c;
	struct leops *lop;
	int rval, plen;
	int doleinit = 0;

	lep = NULL;
	regsp = NULL;

	if (cmd == DDI_ATTACH) {
		/*
		 * Allocate soft data structure
		 */
		lep = GETSTRUCT(struct le, 1);

		/*
		 * Map in the device registers.
		 */
		if (ddi_map_regs(dip, 0, (caddr_t *)&regsp, 0, 0)) {
			kmem_free(lep, sizeof (struct le));
			lerror(dip, "ddi_map_regs failed");
			return (DDI_FAILURE);
		}

		/*
		 * Stop the chip.
		 */
		regsp->lance_rap = LANCE_CSR0;
		regsp->lance_csr = LANCE_STOP;

		if (ddi_get_iblock_cookie(dip, 0, &c) != DDI_SUCCESS) {
			ddi_unmap_regs(dip, 0, (caddr_t *)&regsp, 0, 0);
			kmem_free(lep, sizeof (struct le));
			lerror(dip, "ddi_get_iblock_cookie failed");
			return (DDI_FAILURE);
		}

		/*
		 * Initialize mutex's for this device.
		 */
		mutex_init(&lep->le_xmitlock, NULL, MUTEX_DRIVER, (void *)c);
		mutex_init(&lep->le_intrlock, NULL, MUTEX_DRIVER, (void *)c);
		mutex_init(&lep->le_buflock, NULL, MUTEX_DRIVER, (void *)c);

		/*
		 * One time only driver initializations.
		 */
		mutex_enter(&leautolock);
		if (created_global_mutexes == 0) {
			created_global_mutexes = 1;
			rw_init(&lestruplock, NULL, RW_DRIVER, (void *)c);
			mutex_init(&lelock, NULL, MUTEX_DRIVER, (void *)c);
		}
		mutex_exit(&leautolock);

		ddi_set_driver_private(dip, (caddr_t)lep);
		lep->le_dip = dip;
		lep->le_regsp = regsp;
		lep->le_oopkts = -1;
		lep->le_autosel = 1;

		/*
		 * Add interrupt to system.
		 */
		if (ddi_add_intr(dip, 0, &c, 0, leintr, (caddr_t)lep)) {
			mutex_destroy(&lep->le_xmitlock);
			mutex_destroy(&lep->le_intrlock);
			mutex_destroy(&lep->le_buflock);
			ddi_unmap_regs(dip, 0, (caddr_t *)&regsp, 0, 0);
			kmem_free(lep, sizeof (struct le));
			lerror(dip, "ddi_add_intr failed");
			return (DDI_FAILURE);
		}

		/*
		 * Get the local ethernet address.
		 */
		(void) localetheraddr((struct ether_addr *)NULL,
			&lep->le_ouraddr);

		/*
		 * Look for "learg" property and call leopsadd()
		 * with the info
		 * from our parent node if we find it.
		 */
		plen = sizeof (struct leops *);
		rval = ddi_prop_op(DDI_DEV_T_NONE, ddi_get_parent(dip),
			PROP_LEN_AND_VAL_BUF, 0, "learg", (caddr_t)&lop, &plen);
		if (rval == DDI_PROP_SUCCESS) {
			lop->lo_dip = dip;
			leopsadd(lop);
		}

		/*
		 * Create the filesystem device node.
		 */
		if (ddi_create_minor_node(dip, "le", S_IFCHR,
			ddi_get_instance(dip), DDI_NT_NET,
				CLONE_DEV) == DDI_FAILURE) {
			mutex_destroy(&lep->le_xmitlock);
			mutex_destroy(&lep->le_intrlock);
			mutex_destroy(&lep->le_buflock);
			ddi_remove_intr(dip, 0, c);
			ddi_unmap_regs(dip, 0, (caddr_t *)&regsp, 0, 0);
			kmem_free(lep, sizeof (struct le));
			lerror(dip, "ddi_create_minor_node failed");
			return (DDI_FAILURE);
		}


		/*
		 * Initialize power management bookkeeping; components are
		 * created idle.
		 */
		if (pm_create_components(dip, 1) == DDI_SUCCESS) {
			pm_set_normal_power(dip, 0, 1);
		} else {
			lerror(dip, "leattach:  pm_create_components error");
		}

		/*
		 * Link this per-device structure in with the rest.
		 */
		mutex_enter(&lelock);
		lep->le_nextp = ledev;
		ledev = lep;
		mutex_exit(&lelock);

		if (ddi_get_instance(dip) == 0 &&
		    strcmp(ddi_get_name(ddi_get_parent(dip)), "ledma") == 0) {

			int cablelen;
			char *cable_select = NULL;
			int proplen;
			char *prop;

			/*
			 * Always honour cable-selection, if set.
			 */
			if (ddi_getlongprop(DDI_DEV_T_ANY,
			    ddi_get_parent(lep->le_dip), DDI_PROP_CANSLEEP,
			    "cable-selection", (caddr_t)&cable_select,
			    &cablelen) == DDI_PROP_SUCCESS) {
				/*
				 * It's set, so disable auto-selection
				 */
				lep->le_autosel = 0;
				if (strncmp(cable_select, "tpe", cablelen) == 0)
					lep->le_tpe = 1;
				else
					lep->le_tpe = 0;
				kmem_free(cable_select, cablelen);
			} else {
				lep->le_tpe = 0;
				lep->le_autosel = 1;
			}

			/*
			 * If auto-selection is disabled, check
			 * to see if tpe-link-test? property is
			 * set to false.  If it is set to false, driver will
			 * not be interrupted with Loss of Carrier
			 * Interrupt and will cause the auto selection
			 * algorithm to break.
			 *
			 * Warn the user of the cable-selection property
			 * not being set, and tell them how to get out
			 * of it.
			 */
			if (ddi_getlongprop(DDI_DEV_T_ANY, dip,
			    DDI_PROP_CANSLEEP, "tpe-link-test?",
			    (caddr_t)&prop, &proplen) == DDI_PROP_SUCCESS) {
				if (strncmp("false", prop, proplen) == 0) {
					if (lep->le_autosel == 1 ||
					    lep->le_tpe == 0) {
						cmn_err(CE_CONT, tlt_disabled,
						    ddi_get_instance(dip));
						cmn_err(CE_CONT, tlt_setprop,
						    ddi_get_instance(dip));
					}
				}
				kmem_free(prop, proplen);
			}

			if (ledebug) {
				lerror(lep->le_dip, lep->le_tpe ?
					"uses twisted-pair" : "uses aui");
				lerror(lep->le_dip, lep->le_autosel ?
					"auto-select" : "no auto-select");
			}
		}

		lestatinit(lep);
		ddi_report_dev(dip);
		return (DDI_SUCCESS);
	} else if (cmd == DDI_RESUME || cmd == DDI_PM_RESUME) {
		if ((lep = (struct le *)ddi_get_driver_private(dip)) == NULL)
			return (DDI_FAILURE);
		/*
		 * This flag is used only to force ddi_dev_is_needed(),
		 * which is only appropriate for DDI_PM_SUSPEND/DDI_PM_RESUME
		 */
		if (cmd == DDI_PM_RESUME)
			lep->le_flags &= ~LESUSPENDED;

		/* Do leinit() only for interface that is active */
		rw_enter(&lestruplock, RW_READER);
		for (lsp = lestrup; lsp; lsp = lsp->sl_nextp) {
			if (lsp->sl_lep == lep) {
				doleinit = 1;
				break;
			}
		}
		rw_exit(&lestruplock);
		if (doleinit)
			(void) leinit(lep);
		return (DDI_SUCCESS);
	} else
		return (DDI_FAILURE);
}

static int
ledetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct le	*lep;
	struct le	**lepp;
	struct le	*letmp;
	struct leops	*lop;
	struct leops	**plop;

	lep = (struct le *)ddi_get_driver_private(dip);

	/* Handle the DDI_SUSPEND command */
	if ((cmd == DDI_SUSPEND) || (cmd == DDI_PM_SUSPEND)) {
		if (!lep)			/* No resources allocated */
			return (DDI_FAILURE);
		/*
		 * This flag is used only to force ddi_dev_is_needed(),
		 * which is only appropriate for DDI_PM_SUSPEND
		 */
		if (cmd == DDI_PM_SUSPEND)
			lep->le_flags |= LESUSPENDED;

		/*
		 * Reset any timeout that may have started
		 */
		if (lep->le_init && lep->le_timeout_id) {
			(void) untimeout(lep->le_timeout_id);
			lep->le_timeout_id = 0;
		}

		leuninit(lep);
		return (DDI_SUCCESS);
	}

	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	ASSERT(leinfoack.dl_provider_style == DL_STYLE2);

	if (lep == (struct le *)NULL) {		/* No resources allocated */
		return (DDI_SUCCESS);
	}

	/*
	 * Reset any timeout that may have started
	 */
	if (lep->le_init && lep->le_timeout_id) {
		(void) untimeout(lep->le_timeout_id);
		lep->le_timeout_id = 0;
	}

	if (lep->le_flags & (LERUNNING | LESUSPENDED)) {
		cmn_err(CE_NOTE, "%s is BUSY(0x%x)", ddi_get_name(dip),
		    lep->le_flags);
		return (DDI_FAILURE);
	}

	/*
	 * CHK: Need to get leclose(), leuninit(), eqiv. fn. done
	 */
	(void) ddi_remove_intr(dip, 0, lep->le_cookie);

	/*
	 * Stop the chip.
	 */
	if (lep->le_regsp == NULL) {
		cmn_err(CE_WARN, "ledetach: dip %p, lep %p(dip (%p))",
		    (void *)dip, (void *)lep, (void *)lep->le_dip);
		cmn_err(CE_WARN, "lep->le_regsp == NULL");
	} else {
		leuninit(lep);
		ddi_remove_minor_node(dip, NULL);
		(void) ddi_unmap_regs(dip, 0, (caddr_t *)&lep->le_regsp,
		    0, 0);
	}

	/*
	 * Remove lep from the linked list of device structures
	 */
	mutex_enter(&lelock);
	for (lepp = &ledev; (letmp = *lepp) != NULL;
	    lepp = &letmp->le_nextp)
		if (letmp == lep) {
			*lepp = letmp->le_nextp;
			break;
		}

	/* leopssub(dip) */
	for (plop = &leops; (lop = *plop) != NULL; plop = &lop->lo_next)
		if (lop->lo_dip == dip) {
			*plop = lop->lo_next;
			break;
		}

	mutex_exit(&lelock);

	/*
	 * remove all driver properties
	 * Note: Also done by ddi_uninitchild() i.e., can skip if necessary
	 */
	ddi_prop_remove_all(dip);

	if (lep->le_ksp) {
		kstat_delete(lep->le_ksp);
	}
	if (lep->le_intrstats) {
		kstat_delete(lep->le_intrstats);
	}
	mutex_destroy(&lep->le_xmitlock);
	mutex_destroy(&lep->le_intrlock);
	mutex_destroy(&lep->le_buflock);
	if (!(lep->le_flags & LESLAVE) && lep->le_iopbhandle) {
		(void) ddi_dma_free(lep->le_iopbhandle);
		(void) ddi_dma_free(lep->le_bufhandle);
		ddi_iopb_free((caddr_t)lep->le_iopbkbase);
		kmem_free(lep->le_bufbase,
		    P2ROUNDUP(lep->le_nbufs * sizeof (struct lebuf), PAGESIZE));
		kmem_free((caddr_t)lep->le_buftab,
		(lep->le_nbufs * sizeof (struct lebuf *)));
	}

	kmem_free((caddr_t)lep, sizeof (struct le));

	pm_destroy_components(dip);

	ddi_set_driver_private(dip, NULL);

	return (DDI_SUCCESS);
}

static int
lestat_kstat_update(kstat_t *ksp, int rw)
{
	struct le *lep;
	struct lestat *lesp;

	lep = (struct le *)ksp->ks_private;
	lesp = (struct lestat *)ksp->ks_data;

	if (rw == KSTAT_WRITE) {
		lep->le_ipackets	= lesp->les_ipackets.value.ul;
		lep->le_ierrors		= lesp->les_ierrors.value.ul;
		lep->le_opackets	= lesp->les_opackets.value.ul;
		lep->le_oerrors		= lesp->les_oerrors.value.ul;
		lep->le_collisions	= lesp->les_collisions.value.ul;

		/*
		 * MIB II kstat variables
		 */
		lep->le_rcvbytes	= lesp->les_rcvbytes.value.ul;
		lep->le_xmtbytes	= lesp->les_xmtbytes.value.ul;
		lep->le_multircv	= lesp->les_multircv.value.ul;
		lep->le_multixmt	= lesp->les_multixmt.value.ul;
		lep->le_brdcstrcv	= lesp->les_brdcstrcv.value.ul;
		lep->le_brdcstxmt	= lesp->les_brdcstxmt.value.ul;
		lep->le_norcvbuf	= lesp->les_norcvbuf.value.ul;
		lep->le_noxmtbuf	= lesp->les_noxmtbuf.value.ul;

#ifdef	kstat
		lep->le_defer		= lesp->les_defer.value.ul;
		lep->le_fram		= lesp->les_fram.value.ul;
		lep->le_crc		= lesp->les_crc.value.ul;
		lep->le_oflo		= lesp->les_oflo.value.ul;
		lep->le_uflo		= lesp->les_uflo.value.ul;
		lep->le_missed		= lesp->les_missed.value.ul;
		lep->le_tlcol		= lesp->les_tlcol.value.ul;
		lep->le_trtry		= lesp->les_trtry.value.ul;
		lep->le_tnocar		= lesp->les_tnocar.value.ul;
		lep->le_inits		= lesp->les_inits.value.ul;
		lep->le_notmds		= lesp->les_notmds.value.ul;
		lep->le_notbufs		= lesp->les_notbufs.value.ul;
		lep->le_norbufs		= lesp->les_norbufs.value.ul;
		lep->le_nocanput	= lesp->les_nocanput.value.ul;
		lep->le_allocbfail	= lesp->les_allocbfail.value.ul;

#endif	kstat
		return (0);
	} else {
		lesp->les_ipackets.value.ul	= lep->le_ipackets;
		lesp->les_ierrors.value.ul	= lep->le_ierrors;
		lesp->les_opackets.value.ul	= lep->le_opackets;
		lesp->les_oerrors.value.ul	= lep->le_oerrors;
		lesp->les_collisions.value.ul	= lep->le_collisions;
		lesp->les_defer.value.ul	= lep->le_defer;
		lesp->les_fram.value.ul		= lep->le_fram;
		lesp->les_crc.value.ul		= lep->le_crc;
		lesp->les_oflo.value.ul		= lep->le_oflo;
		lesp->les_uflo.value.ul		= lep->le_uflo;
		lesp->les_missed.value.ul	= lep->le_missed;
		lesp->les_tlcol.value.ul	= lep->le_tlcol;
		lesp->les_trtry.value.ul	= lep->le_trtry;
		lesp->les_tnocar.value.ul	= lep->le_tnocar;
		lesp->les_inits.value.ul	= lep->le_inits;
		lesp->les_notmds.value.ul	= lep->le_notmds;
		lesp->les_notbufs.value.ul	= lep->le_notbufs;
		lesp->les_norbufs.value.ul	= lep->le_norbufs;
		lesp->les_nocanput.value.ul	= lep->le_nocanput;
		lesp->les_allocbfail.value.ul	= lep->le_allocbfail;

		/*
		 * MIB II kstat variables
		 */
		lesp->les_rcvbytes.value.ul	= lep->le_rcvbytes;
		lesp->les_xmtbytes.value.ul	= lep->le_xmtbytes;
		lesp->les_multircv.value.ul	= lep->le_multircv;
		lesp->les_multixmt.value.ul	= lep->le_multixmt;
		lesp->les_brdcstrcv.value.ul	= lep->le_brdcstrcv;
		lesp->les_brdcstxmt.value.ul	= lep->le_brdcstxmt;
		lesp->les_norcvbuf.value.ul	= lep->le_norcvbuf;
		lesp->les_noxmtbuf.value.ul	= lep->le_noxmtbuf;

	}
	return (0);
}

static void
lestatinit(struct le *lep)
{
	kstat_t	*ksp;
	struct lestat *lesp;
	int instance;
	char buf[16];

	instance = ddi_get_instance(lep->le_dip);

	(void) sprintf(buf, "lec%d", instance);
	/*
	 * I'd rather call it leN but the name is already taken by the
	 * old kstat code. This way we are in sync with fd.c driver.
	 */
	lep->le_intrstats = kstat_create("le", instance, buf, "controller",
		KSTAT_TYPE_INTR, 1, KSTAT_FLAG_PERSISTENT);
	if (lep->le_intrstats) {
		kstat_install(lep->le_intrstats);
	}

#ifdef	kstat
	if ((ksp = kstat_create("le", instance,
	    NULL, "net", KSTAT_TYPE_NAMED,
	    sizeof (struct lestat) / sizeof (kstat_named_t),
		KSTAT_FLAG_PERSISTENT)) == NULL) {
#else
	if ((ksp = kstat_create("le", instance,
	    NULL, "net", KSTAT_TYPE_NAMED,
	    sizeof (struct lestat) / sizeof (kstat_named_t), 0)) == NULL) {
#endif	kstat
		lerror(lep->le_dip, "kstat_create failed");
		return;
	}
	lep->le_ksp = ksp;
	lesp = (struct lestat *)(ksp->ks_data);
	kstat_named_init(&lesp->les_ipackets,		"ipackets",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_ierrors,		"ierrors",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_opackets,		"opackets",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_oerrors,		"oerrors",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_collisions,		"collisions",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_defer,		"defer",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_fram,		"framing",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_crc,		"crc",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_oflo,		"oflo",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_uflo,		"uflo",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_missed,		"missed",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_tlcol,		"late_collisions",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_trtry,		"retry_error",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_tnocar,		"nocarrier",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_inits,		"inits",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_notmds,		"notmds",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_notbufs,		"notbufs",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_norbufs,		"norbufs",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_nocanput,		"nocanput",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_allocbfail,		"allocbfail",
		KSTAT_DATA_ULONG);

	/*
	 * MIB II kstat variables
	 */
	kstat_named_init(&lesp->les_rcvbytes,		"rbytes",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_xmtbytes,		"obytes",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_multircv,		"multircv",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_multixmt,		"multixmt",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_brdcstrcv,		"brdcstrcv",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_brdcstxmt,		"brdcstxmt",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_norcvbuf,		"norcvbuf",
		KSTAT_DATA_ULONG);
	kstat_named_init(&lesp->les_noxmtbuf,		"noxmtbuf",
		KSTAT_DATA_ULONG);

	ksp->ks_update = lestat_kstat_update;
	ksp->ks_private = (void *) lep;
	kstat_install(ksp);
}

/*
 * Translate "dev_t" to a pointer to the associated "dev_info_t".
 */
/* ARGSUSED */
static int
leinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	dev_t dev = (dev_t)arg;
	minor_t instance;
	struct lestr *slp;
	int rc;

	instance = getminor(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		rw_enter(&lestruplock, RW_READER);
		dip = NULL;
		for (slp = lestrup; slp; slp = slp->sl_nextp)
			if (slp->sl_minor == instance)
				break;
		if (slp && slp->sl_lep)
			dip = slp->sl_lep->le_dip;
		rw_exit(&lestruplock);

		if (dip) {
			*result = (void *) dip;
			rc = DDI_SUCCESS;
		} else
			rc = DDI_FAILURE;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		rc = DDI_SUCCESS;
		break;

	default:
		rc = DDI_FAILURE;
		break;
	}
	return (rc);
}

/*
 * Allocate memory for:
 *   - LANCE initialization block
 *   - LANCE transmit descriptor ring
 *   - LANCE receive descriptor ring
 *   - pool of xmit/recv buffers
 *
 * For SLAVE devices, allocate out of device local memory.
 * For DVMA devices, allocate out of main memory.
 *
 * The init block and descriptors are allocated contiguously
 * and the buffers are allocated contiguously.  Each
 * of the iopb and buf areas are described by a DDI DMA handle,
 * (null for slave devices), the base kernel virtual address
 * for driver usage, and the base io virtual address
 * for the chip usage.
 */
static void
leallocthings(struct le *lep)
{
	struct lebuf *lbp;
	uintptr_t a;
	uint_t size;
	int i;
	ddi_dma_cookie_t c;

	/*
	 * Return if resources are already allocated.
	 */
	if (lep->le_ibp)
		return;

	lep->le_nrmdp2 = le_nrmdp2;
	lep->le_nrmds = 1 << lep->le_nrmdp2;
	lep->le_ntmdp2 = le_ntmdp2;
	lep->le_ntmds = 1 << lep->le_ntmdp2;
	lep->le_nbufs = le_nbufs;

	/*
	 * Allocate Init block, RMDs, TMDs, and Buffers.
	 */
	if (lep->le_flags & LESLAVE) {	/* Slave (non-DMA) interface */

		/*
		 * Allocate data structures from device local memory
		 * starting at 'membase' which is assumed to be
		 * LANCEALIGNed.
		 */
		ASSERT((lep->le_membase & 0x3) == 0);
		a = lep->le_membase;

		/* Allocate the chip initialization block */
		lep->le_ibp = (struct lance_init_block *)a;
		a += sizeof (struct lance_init_block);
		a = LEROUNDUP(a, LANCEALIGN);

		/* Allocate the message descriptor rings */
		lep->le_rmdp = (struct lmd *)a;
		a += lep->le_nrmds * sizeof (struct lmd);
		a = LEROUNDUP(a, LANCEALIGN);
		lep->le_tmdp = (struct lmd *)a;
		a += lep->le_ntmds * sizeof (struct lmd);
		a = LEROUNDUP(a, LEBURSTSIZE);

		/* Allocate the buffer pool burst-aligned */
		lep->le_nbufs = (lep->le_memsize -
			OFFSET(lep->le_membase, a)) /
			sizeof (struct lebuf);
		lep->le_bufbase = (caddr_t)a;

		/* Initialize private io address handles */
		lep->le_iopbhandle = NULL;
		lep->le_iopbkbase = (uintptr_t)lep->le_membase;
		lep->le_iopbiobase = 0;
		lep->le_bufhandle = NULL;
		lep->le_bufkbase = (uintptr_t)lep->le_bufbase;
		lep->le_bufiobase =
			(uintptr_t)lep->le_bufkbase - lep->le_membase;
	} else {	/* Master (DMA) interface */

		/*
		 * Allocate all data structures from main memory.
		 */

		/*
		 * Allocate the chip init block and descriptors
		 * all at one time remembering to allocate extra for
		 * alignments.
		 */
		size = sizeof (struct lance_init_block) +
			(lep->le_nrmds * sizeof (struct lmd)) +
			(lep->le_ntmds * sizeof (struct lmd)) +
			(4 * LANCEALIGN);	/* fudge */
		if (ddi_iopb_alloc(lep->le_dip, &le_dma_limits,
			(uint_t)size,
			(caddr_t *)&lep->le_iopbkbase)) {
			panic("leallocthings:  out of iopb space");
			/*NOTREACHED*/
		}
		a = lep->le_iopbkbase;
		a = LEROUNDUP(a, LANCEALIGN);
		lep->le_ibp = (struct lance_init_block *)a;
		a += sizeof (struct lance_init_block);
		a = LEROUNDUP(a, LANCEALIGN);
		lep->le_rmdp = (struct lmd *)a;
		a += lep->le_nrmds * sizeof (struct lmd);
		a = LEROUNDUP(a, LANCEALIGN);
		lep->le_tmdp = (struct lmd *)a;

#ifdef NETDEBUGGER
		size = (DLE_MEM_SIZE > size) ? DLE_MEM_SIZE : size;
#endif /* NETDEBUGGER */

		/*
		 * IO map this and get an "iopb" dma handle.
		 */
		if (ddi_dma_addr_setup(lep->le_dip, (struct as *)0,
			(caddr_t)lep->le_iopbkbase, size,
			DDI_DMA_RDWR|DDI_DMA_CONSISTENT,
			DDI_DMA_DONTWAIT, 0, &le_dma_limits,
			&lep->le_iopbhandle))
			panic("le:  ddi_dma_addr_setup iopb failed");

		/*
		 * Initialize iopb io virtual address.
		 */
		if (ddi_dma_htoc(lep->le_iopbhandle, 0, &c))
			panic("le:  ddi_dma_htoc iopb failed");
		lep->le_iopbiobase = c.dmac_address;

		/*
		 * Allocate the buffers page-aligned, which ensures that
		 * they're burst-aligned as well.
		 */
		size = P2ROUNDUP(lep->le_nbufs * sizeof (struct lebuf),
		    PAGESIZE);

		lep->le_bufbase = kmem_zalloc(size, KM_SLEEP);
		lep->le_bufkbase = (uintptr_t)lep->le_bufbase;

		/*
		 * IO map the buffers and get a "buffer" dma handle.
		 */
		if (ddi_dma_addr_setup(lep->le_dip, (struct as *)0,
			(caddr_t)lep->le_bufbase,
			(lep->le_nbufs * sizeof (struct lebuf)),
			DDI_DMA_RDWR, DDI_DMA_DONTWAIT, 0, &le_dma_limits,
			&lep->le_bufhandle))
			panic("le: ddi_dma_addr_setup of bufs failed");

		/*
		 * Initialize buf io virtual address.
		 */
		if (ddi_dma_htoc(lep->le_bufhandle, 0, &c))
			panic("le:  ddi_dma_htoc buf failed");
		lep->le_bufiobase = c.dmac_address;

	}

#ifdef NETDEBUGGER

	/* DLE virt addr for iopb */
	dle_virt_addr = (uintptr_t)lep->le_iopbkbase;

	dle_dma_addr = lep->le_iopbiobase;	/* dle IO addr for chip */
	dle_regs = lep->le_regsp;	/* addr of regs */
	dle_k_ib = lep->le_iopbiobase;	/* addr of iobp */
#endif /* NETDEBUGGER */

	/*
	 * Keep handy limit values for RMD, TMD, and Buffers.
	 */
	lep->le_rmdlimp = &((lep->le_rmdp)[lep->le_nrmds]);
	lep->le_tmdlimp = &((lep->le_tmdp)[lep->le_ntmds]);

	/*
	 * Allocate buffer pointer stack (fifo).
	 */
	size = lep->le_nbufs * sizeof (struct lebuf *);
	lep->le_buftab = kmem_alloc(size, KM_SLEEP);

	/*
	 * Zero out the buffers.
	 */
	bzero(lep->le_bufbase, lep->le_nbufs * sizeof (struct lebuf));

	/*
	 * Zero out xmit and rcv holders.
	 */
	bzero((caddr_t)lep->le_tbufp, sizeof (lep->le_tbufp));
	bzero((caddr_t)lep->le_tmblkp, sizeof (lep->le_tmblkp));
	bzero((caddr_t)lep->le_rbufp, sizeof (lep->le_rbufp));

	/*
	 * Initialize buffer pointer stack.
	 */
	lep->le_bufi = 0;
	for (i = 0; i < lep->le_nbufs; i++) {
		lbp = &((struct lebuf *)lep->le_bufbase)[i];
		lbp->lb_lep = lep;
		lbp->lb_frtn.free_func = lefreebuf;
		lbp->lb_frtn.free_arg = (char *)lbp;
		lefreebuf(lbp);
	}
	lep->le_buflim = lep->le_bufbase +
		(lep->le_nbufs * sizeof (struct lebuf));
}

/*ARGSUSED*/
static
leopen(queue_t *rq, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	struct	lestr	*slp;
	struct	lestr	**prevslp;
	minor_t minordev;
	int	rc = 0;

	ASSERT(rq);
	ASSERT(sflag != MODOPEN);
	TRACE_1(TR_FAC_LE, TR_LE_OPEN, "leopen:  rq %p", rq);

	/*
	 * Serialize all driver open and closes.
	 */
	rw_enter(&lestruplock, RW_WRITER);

	/*
	 * Determine minor device number.
	 */
	prevslp = &lestrup;
	if (sflag == CLONEOPEN) {
		minordev = 0;
		for (; (slp = *prevslp) != NULL; prevslp = &slp->sl_nextp) {
			if (minordev < slp->sl_minor)
				break;
			minordev++;
		}
		*devp = makedevice(getmajor(*devp), minordev);
	} else
		minordev = getminor(*devp);

	if (rq->q_ptr)
		goto done;

	slp = GETSTRUCT(struct lestr, 1);
	slp->sl_minor = minordev;
	slp->sl_rq = rq;
	slp->sl_state = DL_UNATTACHED;
	slp->sl_sap = 0;
	slp->sl_flags = 0;
	slp->sl_lep = NULL;

	mutex_init(&slp->sl_lock, NULL, MUTEX_DRIVER, NULL);

	/*
	 * Link new entry into the list of active entries.
	 */
	slp->sl_nextp = *prevslp;
	*prevslp = slp;

	rq->q_ptr = WR(rq)->q_ptr = (char *)slp;

	/*
	 * Disable automatic enabling of our write service procedure.
	 * We control this explicitly.
	 */
	noenable(WR(rq));

done:
	rw_exit(&lestruplock);
	qprocson(rq);
	return (rc);
}

/*ARGSUSED1*/
static int
leclose(queue_t *rq, int flag, int otyp, cred_t *credp)
{
	struct	lestr	*slp;
	struct	lestr	**prevslp;

	TRACE_1(TR_FAC_LE, TR_LE_CLOSE, "leclose:  rq %p", rq);
	ASSERT(rq);
	ASSERT(rq->q_ptr);

	qprocsoff(rq);

	slp = (struct lestr *)rq->q_ptr;

	/*
	 * Implicit detach Stream from interface.
	 */
	if (slp->sl_lep)
		ledodetach(slp);

	rw_enter(&lestruplock, RW_WRITER);

	/*
	 * Unlink the per-Stream entry from the active list and free it.
	 */
	for (prevslp = &lestrup; (slp = *prevslp) != NULL;
		prevslp = &slp->sl_nextp)
		if (slp == (struct lestr *)rq->q_ptr)
			break;
	ASSERT(slp);
	*prevslp = slp->sl_nextp;

	mutex_destroy(&slp->sl_lock);
	kmem_free(slp, sizeof (struct lestr));

	rq->q_ptr = WR(rq)->q_ptr = NULL;

	rw_exit(&lestruplock);
	return (0);
}

static int
lewput(queue_t *wq, mblk_t *mp)
{
	struct lestr *slp = (struct lestr *)wq->q_ptr;
	struct le *lep;

	TRACE_2(TR_FAC_LE, TR_LE_WPUT_START,
		"lewput start:  wq %p db_type %o", wq, DB_TYPE(mp));

	switch (DB_TYPE(mp)) {
		case M_DATA:		/* "fastpath" */
			lep = slp->sl_lep;
			if (((slp->sl_flags & (SLFAST|SLRAW)) == 0) ||
				(slp->sl_state != DL_IDLE) ||
				(lep == NULL)) {
				merror(wq, mp, EPROTO);
				break;
			}

			/*
			 * If any msgs already enqueued or the interface will
			 * loop back up the message (due to LEPROMISC), then
			 * enqueue the msg.  Otherwise just xmit it directly.
			 */
			if (wq->q_first) {
				(void) putq(wq, mp);
				lep->le_wantw = 1;
				qenable(wq);
			} else if (lep->le_flags & LEPROMISC) {
				(void) putq(wq, mp);
				qenable(wq);
			} else
				(void) lestart(wq, mp, lep);

			break;

		case M_PROTO:
		case M_PCPROTO:
			/*
			 * Break the association between the current thread and
			 * the thread that calls leproto() to resolve the
			 * problem of leintr() threads which loop back around
			 * to call leproto and try to recursively acquire
			 * internal locks.
			 */
			(void) putq(wq, mp);
			qenable(wq);
			break;

		case M_IOCTL:
			leioctl(wq, mp);
			break;

		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW) {
				flushq(wq, FLUSHALL);
				*mp->b_rptr &= ~FLUSHW;
			}
			if (*mp->b_rptr & FLUSHR)
				qreply(wq, mp);
			else
				freemsg(mp);
			break;

		default:
			freemsg(mp);
			break;
	}
	TRACE_1(TR_FAC_LE, TR_LE_WPUT_END, "lewput end:  wq %p", wq);
	return (0);
}

/*
 * Enqueue M_PROTO/M_PCPROTO (always) and M_DATA (sometimes) on the wq.
 *
 * Processing of some of the M_PROTO/M_PCPROTO msgs involves acquiring
 * internal locks that are held across upstream putnext calls.
 * Specifically there's the problem of leintr() holding le_intrlock
 * and lestruplock when it calls putnext() and that thread looping
 * back around to call lewput and, eventually, leinit() to create a
 * recursive lock panic.  There are two obvious ways of solving this
 * problem: (1) have leintr() do putq instead of putnext which provides
 * the loopback "cutout" right at the rq, or (2) allow leintr() to putnext
 * and put the loopback "cutout" around leproto().  We choose the latter
 * for performance reasons.
 *
 * M_DATA messages are enqueued on the wq *only* when the xmit side
 * is out of tbufs or tmds.  Once the xmit resource is available again,
 * wsrv() is enabled and tries to xmit all the messages on the wq.
 */
static
lewsrv(queue_t *wq)
{
	mblk_t	*mp;
	struct	lestr	*slp;
	struct	le	*lep;

	TRACE_1(TR_FAC_LE, TR_LE_WSRV_START, "lewsrv start:  wq %p", wq);

	slp = (struct lestr *)wq->q_ptr;
	lep = slp->sl_lep;

	while (mp = getq(wq))
		switch (DB_TYPE(mp)) {
			case	M_DATA:
				if (lep) {
					if (lestart(wq, mp, lep))
						goto done;
				} else
					freemsg(mp);
				break;

			case	M_PROTO:
			case	M_PCPROTO:
				leproto(wq, mp);
				break;

			default:
				ASSERT(0);
				break;
		}

done:
	TRACE_1(TR_FAC_LE, TR_LE_WSRV_END, "lewsrv end:  wq %p", wq);
	return (0);
}

static void
leproto(queue_t *wq, mblk_t *mp)
{
	union	DL_primitives	*dlp;
	struct	lestr	*slp;
	t_uscalar_t	prim;

	slp = (struct lestr *)wq->q_ptr;
	dlp = (union DL_primitives *)mp->b_rptr;
	prim = dlp->dl_primitive;

	TRACE_2(TR_FAC_LE, TR_LE_PROTO_START,
		"leproto start:  wq %p dlprim %X", wq, prim);

	mutex_enter(&slp->sl_lock);

	switch (prim) {
		case	DL_UNITDATA_REQ:
			leudreq(wq, mp);
			break;

		case	DL_ATTACH_REQ:
			leareq(wq, mp);
			break;

		case	DL_DETACH_REQ:
			ledreq(wq, mp);
			break;

		case	DL_BIND_REQ:
			lebreq(wq, mp);
			break;

		case	DL_UNBIND_REQ:
			leubreq(wq, mp);
			break;

		case	DL_INFO_REQ:
			leireq(wq, mp);
			break;

		case	DL_PROMISCON_REQ:
			leponreq(wq, mp);
			break;

		case	DL_PROMISCOFF_REQ:
			lepoffreq(wq, mp);
			break;

		case	DL_ENABMULTI_REQ:
			leemreq(wq, mp);
			break;

		case	DL_DISABMULTI_REQ:
			ledmreq(wq, mp);
			break;

		case	DL_PHYS_ADDR_REQ:
			lepareq(wq, mp);
			break;

		case	DL_SET_PHYS_ADDR_REQ:
			lespareq(wq, mp);
			break;

		default:
			dlerrorack(wq, mp, prim, DL_UNSUPPORTED, 0);
			break;
	}

	TRACE_2(TR_FAC_LE, TR_LE_PROTO_END,
		"leproto end:  wq %p dlprim %X", wq, prim);

	mutex_exit(&slp->sl_lock);
}

static void
leioctl(queue_t *wq, mblk_t *mp)
{
	struct	iocblk	*iocp = (struct iocblk *)mp->b_rptr;
	struct	lestr	*slp = (struct lestr *)wq->q_ptr;

	switch (iocp->ioc_cmd) {
	case DLIOCRAW:		/* raw M_DATA mode */
		slp->sl_flags |= SLRAW;
		miocack(wq, mp, 0, 0);
		break;

	case DL_IOC_HDR_INFO:	/* M_DATA "fastpath" info request */
		le_dl_ioc_hdr_info(wq, mp);
		break;

	default:
		miocnak(wq, mp, 0, EINVAL);
		break;
	}
}

/*
 * M_DATA "fastpath" info request.
 * Following the M_IOCTL mblk should come a DL_UNITDATA_REQ mblk.
 * We ack with an M_IOCACK pointing to the original DL_UNITDATA_REQ mblk
 * followed by an mblk containing the raw ethernet header corresponding
 * to the destination address.  Subsequently, we may receive M_DATA
 * msgs which start with this header and may send up
 * up M_DATA msgs with b_rptr pointing to a (ulong) group address
 * indicator followed by the network-layer data (IP packet header).
 * This is all selectable on a per-Stream basis.
 */
static void
le_dl_ioc_hdr_info(queue_t *wq, mblk_t *mp)
{
	mblk_t	*nmp;
	struct	lestr	*slp;
	struct	ledladdr	*dlap;
	dl_unitdata_req_t	*dludp;
	struct	ether_header	*headerp;
	struct	le	*lep;
	t_uscalar_t off, len;
	int	minsize;

	slp = (struct lestr *)wq->q_ptr;
	minsize = sizeof (dl_unitdata_req_t) + LEADDRL;

	/*
	 * Sanity check the request.
	 */
	if ((mp->b_cont == NULL) ||
		(MBLKL(mp->b_cont) < minsize) ||
		(*((t_uscalar_t *)mp->b_cont->b_rptr) != DL_UNITDATA_REQ) ||
		((lep = slp->sl_lep) == NULL)) {
		miocnak(wq, mp, 0, EINVAL);
		return;
	}

	/*
	 * Sanity check the DL_UNITDATA_REQ destination address
	 * offset and length values.
	 */
	dludp = (dl_unitdata_req_t *)mp->b_cont->b_rptr;
	off = dludp->dl_dest_addr_offset;
	len = dludp->dl_dest_addr_length;
	if (!MBLKIN(mp->b_cont, off, len) || (len != LEADDRL)) {
		miocnak(wq, mp, 0, EINVAL);
		return;
	}

	dlap = (struct ledladdr *)(mp->b_cont->b_rptr + off);

	/*
	 * Allocate a new mblk to hold the ether header.
	 */
	if ((nmp = allocb(sizeof (struct ether_header), BPRI_MED)) == NULL) {
		miocnak(wq, mp, 0, ENOMEM);
		return;
	}
	nmp->b_wptr += sizeof (struct ether_header);

	/*
	 * Fill in the ether header.
	 */
	headerp = (struct ether_header *)nmp->b_rptr;
	ether_copy(&dlap->dl_phys, &headerp->ether_dhost);
	ether_copy(&lep->le_ouraddr, &headerp->ether_shost);
	headerp->ether_type = dlap->dl_sap;

	/*
	 * Link new mblk in after the "request" mblks.
	 */
	linkb(mp, nmp);

	slp->sl_flags |= SLFAST;

	/*
	 * XXX Don't bother calling lesetipq() here.
	 */

	miocack(wq, mp, msgsize(mp->b_cont), 0);
}

static void
leareq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	union	DL_primitives	*dlp;
	struct	le	*lep;
	int	ppa;

	slp = (struct lestr *)wq->q_ptr;
	dlp = (union DL_primitives *)mp->b_rptr;

	if (MBLKL(mp) < DL_ATTACH_REQ_SIZE) {
		dlerrorack(wq, mp, DL_ATTACH_REQ, DL_BADPRIM, 0);
		return;
	}

	if (slp->sl_state != DL_UNATTACHED) {
		dlerrorack(wq, mp, DL_ATTACH_REQ, DL_OUTSTATE, 0);
		return;
	}

	ppa = dlp->attach_req.dl_ppa;

	/*
	 * Valid ppa?
	 */
	mutex_enter(&lelock);
	for (lep = ledev; lep; lep = lep->le_nextp)
		if (ppa == ddi_get_instance(lep->le_dip))
			break;
	mutex_exit(&lelock);

	if (lep == NULL) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_BADPPA, 0);
		return;
	}

	/*
	 * Set link to device and update our state.
	 */
	slp->sl_lep = lep;
	slp->sl_state = DL_UNBOUND;

	/*
	 * Has device been initialized?  Do so if necessary.
	 * Also check if promiscous mode is set via the ALLPHYS and
	 * ALLMULTI flags, for the stream.  If so initialize the
	 * interface.
	 * Also update power management property.
	 */

	if (((lep->le_flags & LERUNNING) == 0) ||
		((lep->le_flags & LERUNNING) &&
		(slp->sl_flags & (SLALLPHYS | SLALLMULTI)))) {
			if (leinit(lep)) {
				dlerrorack(wq, mp, dlp->dl_primitive,
					DL_INITFAILED, 0);
				slp->sl_lep = NULL;
				slp->sl_state = DL_UNATTACHED;
				return;
			}

		if (pm_busy_component(lep->le_dip, 0) != DDI_SUCCESS)
			lerror(lep->le_dip, "pm_busy_component failed");
	}


	dlokack(wq, mp, DL_ATTACH_REQ);
}

static void
ledreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_DETACH_REQ_SIZE) {
		dlerrorack(wq, mp, DL_DETACH_REQ, DL_BADPRIM, 0);
		return;
	}

	if (slp->sl_state != DL_UNBOUND) {
		dlerrorack(wq, mp, DL_DETACH_REQ, DL_OUTSTATE, 0);
		return;
	}

	ledodetach(slp);
	dlokack(wq, mp, DL_DETACH_REQ);
}

/*
 * Detach a Stream from an interface.
 */
static void
ledodetach(struct lestr *slp)
{
	struct	lestr	*tslp;
	struct	le	*lep;
	int	reinit = 0;
	int i;

	ASSERT(slp->sl_lep);

	lep = slp->sl_lep;
	slp->sl_lep = NULL;

	/*
	 * Disable promiscuous mode if on.
	 */
	if (slp->sl_flags & SLALLPHYS) {
		slp->sl_flags &= ~SLALLPHYS;
		reinit = 1;
	}

	/*
	 * Disable ALLSAP mode if on.
	 */
	if (slp->sl_flags & SLALLSAP) {
		slp->sl_flags &= ~SLALLSAP;
	}

	/*
	 * Disable ALLMULTI mode if on.
	 */
	if (slp->sl_flags & SLALLMULTI) {
		slp->sl_flags &= ~SLALLMULTI;
		reinit = 1;
	}

	/*
	 * Disable any Multicast Addresses.
	 */

	for (i = 0; i < NMCHASH; i++) {
		if (slp->sl_mctab[i]) {
			reinit = 1;
			kmem_free(slp->sl_mctab[i], slp->sl_mcsize[i] *
			    sizeof (struct ether_addr));
			slp->sl_mctab[i] = NULL;
		}
		slp->sl_mccount[i] = slp->sl_mcsize[i] = 0;
	}

	for (i = 0; i < 4; i++)
		slp->sl_ladrf[i] = 0;

	for (i = 0; i < 64; i++)
		slp->sl_ladrf_refcnt[i] = 0;

	/*
	 * Detach from device structure.
	 * Uninit the device and update power management property
	 * when no other streams are attached to it.
	 */
	for (tslp = lestrup; tslp; tslp = tslp->sl_nextp)
		if (tslp->sl_lep == lep)
			break;
	if (tslp == NULL) {
		leuninit(lep);
		if (pm_idle_component(lep->le_dip, 0) != DDI_SUCCESS)
			lerror(lep->le_dip, "pm_idle_component failed");
	} else if (reinit)
		(void) leinit(lep);

	slp->sl_state = DL_UNATTACHED;

	lesetipq(lep);
}

static void
lebreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	union	DL_primitives	*dlp;
	struct	le	*lep;
	struct	ledladdr	leaddr;
	t_uscalar_t	sap;
	int	xidtest;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_BIND_REQ_SIZE) {
		dlerrorack(wq, mp, DL_BIND_REQ, DL_BADPRIM, 0);
		return;
	}

	if (slp->sl_state != DL_UNBOUND) {
		dlerrorack(wq, mp, DL_BIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	dlp = (union DL_primitives *)mp->b_rptr;
	lep = slp->sl_lep;
	sap = dlp->bind_req.dl_sap;
	xidtest = dlp->bind_req.dl_xidtest_flg;

	ASSERT(lep);

	if (xidtest) {
		dlerrorack(wq, mp, DL_BIND_REQ, DL_NOAUTO, 0);
		return;
	}

	if (sap > ETHERTYPE_MAX) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_BADSAP, 0);
		return;
	}

	/*
	 * Save SAP value for this Stream and change state.
	 */
	slp->sl_sap = sap;
	slp->sl_state = DL_IDLE;

	leaddr.dl_sap = sap;
	ether_copy(&lep->le_ouraddr, &leaddr.dl_phys);
	dlbindack(wq, mp, sap, &leaddr, LEADDRL, 0, 0);

	lesetipq(lep);
}

static void
leubreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_UNBIND_REQ_SIZE) {
		dlerrorack(wq, mp, DL_UNBIND_REQ, DL_BADPRIM, 0);
		return;
	}

	if (slp->sl_state != DL_IDLE) {
		dlerrorack(wq, mp, DL_UNBIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	slp->sl_state = DL_UNBOUND;
	slp->sl_sap = 0;

	(void) putnextctl1(RD(wq), M_FLUSH, FLUSHRW);
	dlokack(wq, mp, DL_UNBIND_REQ);

	lesetipq(slp->sl_lep);
}

static void
leireq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	dl_info_ack_t	*dlip;
	struct	ledladdr	*dlap;
	struct	ether_addr	*ep;
	int	size;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_INFO_REQ_SIZE) {
		dlerrorack(wq, mp, DL_INFO_REQ, DL_BADPRIM, 0);
		return;
	}

	/*
	 * Exchange current msg for a DL_INFO_ACK.
	 */
	size = sizeof (dl_info_ack_t) + LEADDRL + ETHERADDRL;
	if ((mp = mexchange(wq, mp, size, M_PCPROTO, DL_INFO_ACK)) == NULL)
		return;

	/*
	 * Fill in the DL_INFO_ACK fields and reply.
	 */
	dlip = (dl_info_ack_t *)mp->b_rptr;
	*dlip = leinfoack;
	dlip->dl_current_state = slp->sl_state;
	dlap = (struct ledladdr *)(mp->b_rptr + dlip->dl_addr_offset);
	dlap->dl_sap = slp->sl_sap;
	if (slp->sl_lep) {
		ether_copy(&slp->sl_lep->le_ouraddr, &dlap->dl_phys);
	} else {
		bzero((caddr_t)&dlap->dl_phys, ETHERADDRL);
	}
	ep = (struct ether_addr *)(mp->b_rptr + dlip->dl_brdcst_addr_offset);
	ether_copy(&etherbroadcastaddr, ep);

	qreply(wq, mp);
}

static void
leponreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_PROMISCON_REQ_SIZE) {
		dlerrorack(wq, mp, DL_PROMISCON_REQ, DL_BADPRIM, 0);
		return;
	}

	switch (((dl_promiscon_req_t *)mp->b_rptr)->dl_level) {
		case DL_PROMISC_PHYS:
			slp->sl_flags |= SLALLPHYS;
			break;

		case DL_PROMISC_SAP:
			slp->sl_flags |= SLALLSAP;
			break;

		case DL_PROMISC_MULTI:
			slp->sl_flags |= SLALLMULTI;
			break;

		default:
			dlerrorack(wq, mp, DL_PROMISCON_REQ,
				DL_NOTSUPPORTED, 0);
			return;
	}

	if (slp->sl_lep)
		(void) leinit(slp->sl_lep);

	if (slp->sl_lep)
		lesetipq(slp->sl_lep);

	dlokack(wq, mp, DL_PROMISCON_REQ);
}

static void
lepoffreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	int	flag;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_PROMISCOFF_REQ_SIZE) {
		dlerrorack(wq, mp, DL_PROMISCOFF_REQ, DL_BADPRIM, 0);
		return;
	}

	switch (((dl_promiscoff_req_t *)mp->b_rptr)->dl_level) {
		case DL_PROMISC_PHYS:
			flag = SLALLPHYS;
			break;

		case DL_PROMISC_SAP:
			flag = SLALLSAP;
			break;

		case DL_PROMISC_MULTI:
			flag = SLALLMULTI;
			break;

		default:
			dlerrorack(wq, mp, DL_PROMISCOFF_REQ,
				DL_NOTSUPPORTED, 0);
			return;
	}

	if ((slp->sl_flags & flag) == 0) {
		dlerrorack(wq, mp, DL_PROMISCOFF_REQ, DL_NOTENAB, 0);
		return;
	}

	slp->sl_flags &= ~flag;
	if (slp->sl_lep)
		(void) leinit(slp->sl_lep);

	if (slp->sl_lep)
		lesetipq(slp->sl_lep);

	dlokack(wq, mp, DL_PROMISCOFF_REQ);
}


	/* This is to support unlimited number of members in MC */

static void
leemreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	union	DL_primitives	*dlp;
	struct	ether_addr	*addrp, *mcbucket;
	t_uscalar_t off, len;
	uint32_t ladrf_bit, mchash;
	uint16_t ladrf;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_ENABMULTI_REQ_SIZE) {
		dlerrorack(wq, mp, DL_ENABMULTI_REQ, DL_BADPRIM, 0);
		return;
	}

	if (slp->sl_state == DL_UNATTACHED) {
		dlerrorack(wq, mp, DL_ENABMULTI_REQ, DL_OUTSTATE, 0);
		return;
	}

	dlp = (union DL_primitives *)mp->b_rptr;
	len = dlp->enabmulti_req.dl_addr_length;
	off = dlp->enabmulti_req.dl_addr_offset;
	addrp = (struct ether_addr *)(mp->b_rptr + off);

	if ((len != ETHERADDRL) ||
		!MBLKIN(mp, off, len) ||
		((addrp->ether_addr_octet[0] & 01) == 0)) {
		dlerrorack(wq, mp, DL_ENABMULTI_REQ, DL_BADADDR, 0);
		return;
	}

	/*
	 * Calculate hash value and bucket.
	 */

	mchash = MCHASH(addrp);
	mcbucket = slp->sl_mctab[mchash];

	/*
	 * Allocate hash bucket if it's not there.
	 */

	if (mcbucket == NULL) {
		slp->sl_mctab[mchash] = mcbucket =
		    kmem_alloc(INIT_BUCKET_SIZE *
			    sizeof (struct ether_addr), KM_SLEEP);
		slp->sl_mcsize[mchash] = INIT_BUCKET_SIZE;
	}

	/*
	 * We no longer bother checking to see if the address is already
	 * in the table (bugid 1209733).  We won't reinitialize the
	 * hardware, since we'll find the mc bit is already set.
	 */

	/*
	 * Expand table if necessary.
	 */
	if (slp->sl_mccount[mchash] >= slp->sl_mcsize[mchash]) {
		struct	ether_addr	*newbucket;
		uint32_t		newsize;

		newsize = slp->sl_mcsize[mchash] * 2;

		newbucket = kmem_alloc(newsize * sizeof (struct ether_addr),
			KM_SLEEP);
		bcopy((caddr_t)mcbucket, (caddr_t)newbucket,
		    slp->sl_mcsize[mchash] * sizeof (struct ether_addr));
		kmem_free(mcbucket, slp->sl_mcsize[mchash] *
		    sizeof (struct ether_addr));

		slp->sl_mctab[mchash] = mcbucket = newbucket;
		slp->sl_mcsize[mchash] = newsize;
	}

	/*
	 * Add address to the table.
	 */
	mcbucket[slp->sl_mccount[mchash]++] = *addrp;

	/*
	 * If this address's bit was not already set in the local address
	 * filter, add it and re-initialize the LANCE.
	 */
	ladrf_bit = leladrf_bit(addrp);

	if (slp->sl_ladrf_refcnt[ladrf_bit] == 0) {
		ladrf = slp->sl_ladrf[ladrf_bit >> 4];
		slp->sl_ladrf[ladrf_bit >> 4] =
		    ladrf | (1 << (ladrf_bit & 0xf));
		(void) leinit(slp->sl_lep);
	}
	slp->sl_ladrf_refcnt[ladrf_bit]++;

	dlokack(wq, mp, DL_ENABMULTI_REQ);
}

static void
ledmreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	union	DL_primitives	*dlp;
	struct	ether_addr	*addrp, *mcbucket;
	t_uscalar_t off;
	int	i;
	uint_t	mchash, len;



	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_DISABMULTI_REQ_SIZE) {
		dlerrorack(wq, mp, DL_DISABMULTI_REQ, DL_BADPRIM, 0);
		return;
	}

	if (slp->sl_state == DL_UNATTACHED) {
		dlerrorack(wq, mp, DL_DISABMULTI_REQ, DL_OUTSTATE, 0);
		return;
	}

	dlp = (union DL_primitives *)mp->b_rptr;
	len = dlp->disabmulti_req.dl_addr_length;
	off = dlp->disabmulti_req.dl_addr_offset;
	addrp = (struct ether_addr *)(mp->b_rptr + off);

	if ((len != ETHERADDRL) || !MBLKIN(mp, off, len)) {
		dlerrorack(wq, mp, DL_DISABMULTI_REQ, DL_BADADDR, 0);
		return;
	}

	/*
	 * Calculate hash value, get pointer to hash bucket for this address.
	 */

	mchash = MCHASH(addrp);
	mcbucket = slp->sl_mctab[mchash];

	/*
	 * Try and delete the address if we can find it.
	 */
	if (mcbucket) {
		for (i = 0; i < slp->sl_mccount[mchash]; i++) {
			if (ether_cmp(addrp, &mcbucket[i]) == 0) {
				uint32_t ladrf_bit;
				uint16_t adrf_bit;

				/*
				 * If there's more than one address in this
				 * bucket, delete the unwanted one by moving
				 * the last one in the list over top of it;
				 * otherwise, just free the bucket.
				 */
				if (slp->sl_mccount[mchash] > 1) {
					mcbucket[i] =
					    mcbucket[slp->sl_mccount[mchash]-1];
				} else {
					kmem_free(mcbucket,
					    slp->sl_mcsize[mchash] *
					    sizeof (struct ether_addr));
					slp->sl_mctab[mchash] = NULL;
				}
				slp->sl_mccount[mchash]--;

				/*
				 * If this address's bit should no longer be
				 * set in the local address filter, clear it and
				 * re-initialize the LANCE.
				 */

				ladrf_bit = leladrf_bit(addrp);
				slp->sl_ladrf_refcnt[ladrf_bit]--;

				if (slp->sl_ladrf_refcnt[ladrf_bit] == 0) {
					adrf_bit =
					    slp->sl_ladrf[ladrf_bit >> 4];
					slp->sl_ladrf[ladrf_bit >> 4] =
					    adrf_bit &
					    ~(1 << (ladrf_bit & 0xf));
					(void) leinit(slp->sl_lep);
				}

				dlokack(wq, mp, DL_DISABMULTI_REQ);
				return;
			}
		}
	}
	dlerrorack(wq, mp, DL_DISABMULTI_REQ, DL_NOTENAB, 0);
}



static void
lepareq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	union	DL_primitives	*dlp;
	int	type;
	struct	le	*lep;
	struct	ether_addr	addr;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_PHYS_ADDR_REQ_SIZE) {
		dlerrorack(wq, mp, DL_PHYS_ADDR_REQ, DL_BADPRIM, 0);
		return;
	}

	dlp = (union DL_primitives *)mp->b_rptr;
	type = dlp->physaddr_req.dl_addr_type;
	lep = slp->sl_lep;

	if (lep == NULL) {
		dlerrorack(wq, mp, DL_PHYS_ADDR_REQ, DL_OUTSTATE, 0);
		return;
	}

	switch (type) {
		case	DL_FACT_PHYS_ADDR:
			(void) localetheraddr((struct ether_addr *)NULL, &addr);
			break;

		case	DL_CURR_PHYS_ADDR:
			ether_copy(&lep->le_ouraddr, &addr);
			break;

		default:
			dlerrorack(wq, mp, DL_PHYS_ADDR_REQ,
				DL_NOTSUPPORTED, 0);
			return;
	}

	dlphysaddrack(wq, mp, &addr, ETHERADDRL);
}

static void
lespareq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	union	DL_primitives	*dlp;
	t_uscalar_t off, len;
	struct	ether_addr	*addrp;
	struct	le	*lep;

	slp = (struct lestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_SET_PHYS_ADDR_REQ_SIZE) {
		dlerrorack(wq, mp, DL_SET_PHYS_ADDR_REQ, DL_BADPRIM, 0);
		return;
	}

	dlp = (union DL_primitives *)mp->b_rptr;
	len = dlp->set_physaddr_req.dl_addr_length;
	off = dlp->set_physaddr_req.dl_addr_offset;

	if (!MBLKIN(mp, off, len)) {
		dlerrorack(wq, mp, DL_SET_PHYS_ADDR_REQ, DL_BADPRIM, 0);
		return;
	}

	addrp = (struct ether_addr *)(mp->b_rptr + off);

	/*
	 * Error if length of address isn't right or the address
	 * specified is a multicast or broadcast address.
	 */
	if ((len != ETHERADDRL) ||
		((addrp->ether_addr_octet[0] & 01) == 1) ||
		(ether_cmp(addrp, &etherbroadcastaddr) == 0)) {
		dlerrorack(wq, mp, DL_SET_PHYS_ADDR_REQ, DL_BADADDR, 0);
		return;
	}

	/*
	 * Error if this stream is not attached to a device.
	 */
	if ((lep = slp->sl_lep) == NULL) {
		dlerrorack(wq, mp, DL_SET_PHYS_ADDR_REQ, DL_OUTSTATE, 0);
		return;
	}

	/*
	 * Set new interface local address and re-init device.
	 * This is destructive to any other streams attached
	 * to this device.
	 */
	ether_copy(addrp, &lep->le_ouraddr);
	(void) leinit(slp->sl_lep);

	dlokack(wq, mp, DL_SET_PHYS_ADDR_REQ);
}

static void
leudreq(queue_t *wq, mblk_t *mp)
{
	struct	lestr	*slp;
	struct	le	*lep;
	dl_unitdata_req_t	*dludp;
	mblk_t	*nmp;
	struct	ledladdr	*dlap;
	struct	ether_header	*headerp;
	t_uscalar_t off, len;
	t_uscalar_t sap;

	slp = (struct lestr *)wq->q_ptr;
	lep = slp->sl_lep;

	if (slp->sl_state != DL_IDLE) {
		dlerrorack(wq, mp, DL_UNITDATA_REQ, DL_OUTSTATE, 0);
		return;
	}

	dludp = (dl_unitdata_req_t *)mp->b_rptr;

	off = dludp->dl_dest_addr_offset;
	len = dludp->dl_dest_addr_length;

	/*
	 * Validate destination address format.
	 */
	if (!MBLKIN(mp, off, len) || (len != LEADDRL)) {
		dlerrorack(wq, mp, DL_UNITDATA_REQ, DL_BADADDR, 0);
		return;
	}

	/*
	 * Error if no M_DATA follows.
	 */
	nmp = mp->b_cont;
	if (nmp == NULL) {
		dluderrorind(wq, mp, mp->b_rptr + off, len, DL_BADDATA, 0);
		return;
	}

	dlap = (struct ledladdr *)(mp->b_rptr + off);

	/*
	 * Create ethernet header by either prepending it onto the
	 * next mblk if possible, or reusing the M_PROTO block if not.
	 */
	if ((DB_REF(nmp) == 1) &&
		(MBLKHEAD(nmp) >= sizeof (struct ether_header)) &&
		(((uintptr_t)nmp->b_rptr & 0x1) == 0)) {
		nmp->b_rptr -= sizeof (struct ether_header);
		headerp = (struct ether_header *)nmp->b_rptr;
		ether_copy(&dlap->dl_phys, &headerp->ether_dhost);
		ether_copy(&lep->le_ouraddr, &headerp->ether_shost);
		sap = dlap->dl_sap;
		freeb(mp);
		mp = nmp;
	} else {
		DB_TYPE(mp) = M_DATA;
		headerp = (struct ether_header *)mp->b_rptr;
		mp->b_wptr = mp->b_rptr + sizeof (struct ether_header);
		ether_copy(&dlap->dl_phys, &headerp->ether_dhost);
		ether_copy(&lep->le_ouraddr, &headerp->ether_shost);
		sap = dlap->dl_sap;
	}

	/*
	 * sap value in the range [0-1500] are treated equivalent
	 * and represent a desire to use 802.3 mode. Therefore compute
	 * the length and put it in the type field in header
	 */
	if ((sap <= ETHERMTU) || (slp->sl_sap == 0))
		headerp->ether_type =
			(msgsize(mp) - sizeof (struct ether_header));
	else
		headerp->ether_type = sap;

	(void) lestart(wq, mp, lep);

	/*
	 * Delay for 90 usecs on first time to enable the interrupt
	 * to switch from AUI to TPE on an LCAR xmit error. On faster
	 * machines, leemreq() can call leinit() and reset the chip
	 * before the interface can be switched.
	 */
	if (!lep->le_oerrors && !lep->le_opackets)
		drv_usecwait(90);
}

/*
 * Start transmission.
 * Return zero on success,
 * otherwise put msg on wq, set 'want' flag and return nonzero.
 */
static
lestart(queue_t *wq, mblk_t *mp, struct le *lep)
{
	volatile struct lmd *tmdp, *ntmdp;
	struct	lmd	t;
	struct	lebuf	*lbp;
	mblk_t	*nmp = NULL;
	int	len;
	uintptr_t kaddr;
	uintptr_t sbound, soff;
	uint32_t ioaddr;
	int	i;
	int	flags;
	struct  ether_header    *ehp;

	TRACE_1(TR_FAC_LE, TR_LE_START_START, "lestart start:  wq %p", wq);

	/*
	 * update MIB II statistics
	 */
	ehp = (struct ether_header *)mp->b_rptr;
	BUMP_OutNUcast(lep, ehp);

	flags = lep->le_flags;

	if ((flags & (LERUNNING|LEPROMISC)) != LERUNNING) {
		if (!(flags & LERUNNING)) {
			(void) putbq(wq, mp);
			return (1);
		}
		if (flags & LEPROMISC) {
			if ((nmp = copymsg(mp)) == NULL)
				lep->le_allocbfail++;
		}
	}

	len = msgsize(mp);
	if (len > ETHERMAX) {
		lerror(lep->le_dip, "msg too big:  %d", len);
		lep->le_oerrors++;
		freemsg(mp);
		if (nmp)
			freemsg(nmp);
		TRACE_1(TR_FAC_LE, TR_LE_START_END,
			"lestart end:  wq %p", wq);
		return (0);
	}

	kaddr = (uintptr_t)mp->b_rptr;

	/*
	 * Get a local tbuf.
	 */
	if ((lbp = legetbuf(lep, 1)) == NULL) {
		lep->le_notbufs++;
		(void) putbq(wq, mp);
		lep->le_wantw = 1;
		if (nmp)
			freemsg(nmp);
		TRACE_1(TR_FAC_LE, TR_LE_START_END,
			"lestart end:  wq %p", wq);
		return (1);
	}

	/*
	 * Copy msg into tbuf and free the original msg.
	 */
	kaddr = (uintptr_t)lbp->lb_buf;
	if (mp->b_cont == NULL) {
		sbound = (uintptr_t)mp->b_rptr & ~LEBURSTMASK;
		soff = (uintptr_t)mp->b_rptr - sbound;
		bcopy((caddr_t)sbound, (caddr_t)kaddr,
		    LEROUNDUP(len + soff, LEBURSTSIZE));
		kaddr += soff;
		freemsg(mp);
	} else if (mp->b_cont->b_cont == NULL) {
		sbound = (uintptr_t)mp->b_cont->b_rptr & ~LEBURSTMASK;
		soff = (uintptr_t)mp->b_cont->b_rptr - sbound;
		kaddr = LEROUNDUP(kaddr + MBLKL(mp), LEBURSTSIZE);
		bcopy((caddr_t)sbound, (caddr_t)kaddr,
		    LEROUNDUP(MBLKL(mp->b_cont) + soff, LEBURSTSIZE));
		kaddr = kaddr + soff - MBLKL(mp);
		bcopy((caddr_t)mp->b_rptr, (caddr_t)kaddr, MBLKL(mp));
		freemsg(mp);
	} else
		(void) mcopymsg(mp, (uchar_t *)kaddr);
	mp = NULL;

	/*
	 * Pad out to ETHERMIN.
	 */
	if (len < ETHERMIN)
		len = ETHERMIN;

	/*
	 * Translate buffer kernel virtual address into I/O address.
	 */
	ioaddr = LEBUFIOADDR(lep, kaddr);

	/*
	 * Craft a new TMD for this buffer.
	 */
	t.lmd_ladr = (uint16_t)ioaddr;
	t.lmd_hadr = (uint32_t)ioaddr >> 16;
	t.lmd_bcnt = -len;
	t.lmd_flags3 = 0;
	t.lmd_flags = LMD_STP | LMD_ENP | LMD_OWN;

	/*
	 * Acquire xmit lock now and hold it until done.
	 */
	mutex_enter(&lep->le_xmitlock);

	/*
	 * Allocate a tmd and increment pointer to next tmd.
	 * Never allow "next" pointer to point to an in-use tmd.
	 */
	tmdp = lep->le_tnextp;
	ntmdp = NEXTTMD(lep, tmdp);
	if (ntmdp == lep->le_tcurp) {	/* out of tmds */
		lep->le_notmds++;
		mutex_exit(&lep->le_xmitlock);
		if (nmp)
			freemsg(nmp);
		if (mp) {
			(void) putbq(wq, mp);
			lep->le_wantw = 1;
			TRACE_1(TR_FAC_LE, TR_LE_START_END,
				"lestart end:  wq %p", wq);
			return (1);
		} else {
			lefreebuf(lbp);
			lep->le_noxmtbuf++;
			TRACE_1(TR_FAC_LE, TR_LE_START_END,
				"lestart end:  wq %p", wq);
			return (0);
		}
	}

	/*
	 * Save msg or tbuf to free later.
	 */
	i = tmdp - lep->le_tmdp;
	if (mp)
		lep->le_tmblkp[i] = mp;
	else
		lep->le_tbufp[i] = lbp;

	/*
	 * Sync the buffer so the device sees it.
	 */
	LESYNCBUF(lep, kaddr, len, DDI_DMA_SYNC_FORDEV);

	/*
	 * Write out the new TMD.
	 */
	*((uint64_t *)tmdp) = *((uint64_t *)&t);

	/*
	 * Update our TMD ring pointer.
	 */
	lep->le_tnextp = ntmdp;

	/*
	 * Sync the TMD.
	 */
	LESYNCIOPB(lep, tmdp, sizeof (struct lmd), DDI_DMA_SYNC_FORDEV);

	/*
	 * Bang the chip.
	 */
	lep->le_regsp->lance_csr = LANCE_TDMD | LANCE_INEA;

	/*
	 * Now it's ok to release the lock.
	 */
	mutex_exit(&lep->le_xmitlock);

	/*
	 * Loopback a broadcast packet.
	 */
	if ((flags & LEPROMISC) && nmp)
		lesendup(lep, nmp, lepaccept);

	TRACE_1(TR_FAC_LE, TR_LE_START_END, "lestart end:  wq %p", wq);
	return (0);
}

static uint_t
leintr(caddr_t arg)
{
	volatile	struct	lmd	*rmdp;
	volatile	ushort_t	csr0;
	int	serviced = DDI_INTR_UNCLAIMED;
	uint32_t le_inits;
	struct le *lep = (struct le *)arg;

	mutex_enter(&lep->le_intrlock);

	le_inits = lep->le_inits;

	csr0 = lep->le_regsp->lance_csr;

	TRACE_2(TR_FAC_LE, TR_LE_INTR_START,
		"leintr start:  lep %p csr %X", lep, csr0);

	if (lep->le_intr_flag) {
		serviced = DDI_INTR_CLAIMED;
		lep->le_intr_flag = 0;
	}
	if ((csr0 & LANCE_INTR) == 0) {
		if (lep->le_intr && (*lep->le_intr)(lep->le_arg)) {
			if (lep->le_intrstats)
				KIOIP->intrs[KSTAT_INTR_HARD]++;
			mutex_exit(&lep->le_intrlock);
			lep->loc_flag = 1;
			serviced = DDI_INTR_CLAIMED;
		} else {
			if (lep->le_intrstats) {
				if (serviced == DDI_INTR_UNCLAIMED)
					KIOIP->intrs[KSTAT_INTR_SPURIOUS]++;
				else
					KIOIP->intrs[KSTAT_INTR_HARD]++;
			}
			mutex_exit(&lep->le_intrlock);
		}
		TRACE_2(TR_FAC_LE, TR_LE_INTR_END,
			"leintr end:  lep %p serviced %d",
			lep, serviced);
		return (serviced);
	}

	/*
	 * Clear RINT/TINT .
	 */
	lep->le_regsp->lance_csr =
		(csr0 & (LANCE_RINT | LANCE_TINT)) | LANCE_INEA;
	serviced = DDI_INTR_CLAIMED;

	/*
	 * Check for receive activity.
	 * One packet per RMD.
	 */
	if (csr0 & LANCE_RINT) {
		rmdp = lep->le_rnextp;

		/*
		 * Sync RMD before looking at it.
		 */
		LESYNCIOPB(lep, rmdp, sizeof (struct lmd),
			DDI_DMA_SYNC_FORCPU);

		/*
		 * Loop through each RMD.
		 */
		while ((rmdp->lmd_flags & LMD_OWN) == 0) {
			leread(lep, rmdp);

			/*
			 * if the chip has been reinitialized
			 * then we break out and handle any
			 * new packets in the next interrupt
			 */
			if (le_inits < lep->le_inits)
				break;

			/*
			 * Give the descriptor and associated
			 * buffer back to the chip.
			 */
			rmdp->lmd_mcnt = 0;
			rmdp->lmd_flags = LMD_OWN;

			/*
			 * Sync the RMD after writing it.
			 */
			LESYNCIOPB(lep, rmdp, sizeof (struct lmd),
				DDI_DMA_SYNC_FORDEV);

			/*
			 * Increment to next RMD.
			 */
			lep->le_rnextp = rmdp = NEXTRMD(lep, rmdp);

			/*
			 * Sync the next RMD before looking at it.
			 */
			LESYNCIOPB(lep, rmdp, sizeof (struct lmd),
				DDI_DMA_SYNC_FORCPU);
		}
		lep->le_rx_lbolt = lbolt;
	}

	/*
	 * Check for transmit activity.
	 * One packet per TMD.
	 */
	if (csr0 & LANCE_TINT) {
		lereclaim(lep);
		lep->le_tx_lbolt = lbolt;
		if (lewdflag & LEWD_FLAG_TX_TIMEOUT) {
			lep->le_rx_lbolt = lbolt;
		}
	}

	/*
	 * Check for errors not specifically related
	 * to transmission or reception.
	 */
	if ((csr0 & (LANCE_BABL|LANCE_MERR|LANCE_MISS|LANCE_TXON|LANCE_RXON))
		!= (LANCE_TXON|LANCE_RXON)) {
		(void) le_chip_error(lep);
	}

done:

	/*
	 * Read back register to flush write buffers.
	 */
	csr0 = lep->le_regsp->lance_csr;

	if (lep->le_intrstats) {
		KIOIP->intrs[KSTAT_INTR_HARD]++;
	}
	mutex_exit(&lep->le_intrlock);
	TRACE_2(TR_FAC_LE, TR_LE_INTR_END,
		"leintr end:  lep %p serviced %d", lep, serviced);
	return (serviced);
}

/*
 * Transmit completion reclaiming.
 */
static void
lereclaim(struct le *lep)
{
	volatile	struct	lmd	*tmdp;
	int	flags;
	int	i;

	tmdp = lep->le_tcurp;

	/*
	 * Sync TMD before looking at it.
	 */
	LESYNCIOPB(lep, tmdp, sizeof (struct lmd), DDI_DMA_SYNC_FORCPU);

	/*
	 * Loop through each TMD.
	 */
	while ((((flags = tmdp->lmd_flags) &
		(LMD_OWN | TMD_INUSE)) == 0) &&
		(tmdp != lep->le_tnextp)) {

		/*
		 * Keep defer/retry statistics.
		 */
		if (flags & TMD_DEF)
			lep->le_defer++;
		if (flags & TMD_ONE)
			lep->le_collisions++;
		else if (flags & TMD_MORE)
			lep->le_collisions += 2;

		/*
		 * Check for transmit errors and keep output stats.
		 */
		if ((tmdp->lmd_flags3 & TMD_ANYERROR) == 0) {
			lep->le_opackets++;
			lep->le_xmtbytes += (ushort_t)-tmdp->lmd_bcnt;
		} else {
			lep->le_oerrors++;
			if (le_xmit_error(lep, (struct lmd *)tmdp))
				return;
		}

		/*
		 * Free msg or buffer.
		 */
		i = tmdp - lep->le_tmdp;
		if (lep->le_tmblkp[i]) {
			freemsg(lep->le_tmblkp[i]);
			lep->le_tmblkp[i] = NULL;
		} else if (lep->le_tbufp[i]) {
			lefreebuf(lep->le_tbufp[i]);
			lep->le_tbufp[i] = NULL;
		}

		tmdp = NEXTTMD(lep, tmdp);

		/*
		 * Sync TMD before looking at it.
		 */
		LESYNCIOPB(lep, tmdp, sizeof (struct lmd),
			DDI_DMA_SYNC_FORCPU);
	}

	lep->le_tcurp = tmdp;

	/*
	 * Check for any msgs that were queued
	 * due to out-of-tmd condition.
	 */
	if (lep->le_wantw)
		lewenable(lep);
}

/*
 * Start xmit on any msgs previously enqueued on any write queues.
 */
static void
lewenable(struct le *lep)
{
	struct	lestr	*slp;
	queue_t	*wq;

	/*
	 * Order of wantw accesses is important.
	 */
	do {
		lep->le_wantw = 0;
		for (slp = lestrup; slp; slp = slp->sl_nextp)
			if ((slp->sl_lep == lep) &&
				((wq = WR(slp->sl_rq))->q_first))
				qenable(wq);
	} while (lep->le_wantw);
}

/*
 * Test upstream destination sap and address match.
 */
static struct lestr *
leaccept(struct lestr *slp, struct le *lep,
    int type, struct ether_addr *addrp)
{
	int	sap;
	int	flags;

	for (; slp; slp = slp->sl_nextp) {
		sap = slp->sl_sap;
		flags = slp->sl_flags;

		if ((slp->sl_lep == lep) && LESAPMATCH(sap, type, flags))
			if ((ether_cmp(addrp, &lep->le_ouraddr) == 0) ||
				(ether_cmp(addrp, &etherbroadcastaddr) == 0) ||
				(flags & SLALLPHYS) ||
				lemcmatch(slp, addrp))
				return (slp);
	}

	return (NULL);
}

/*
 * Test upstream destination sap and address match for SLALLPHYS only.
 */
/* ARGSUSED3 */
static struct lestr *
lepaccept(struct lestr *slp, struct le *lep, int type, struct ether_addr *addrp)
{
	int	sap;
	int	flags;

	for (; slp; slp = slp->sl_nextp) {
		sap = slp->sl_sap;
		flags = slp->sl_flags;

		if ((slp->sl_lep == lep) &&
			LESAPMATCH(sap, type, flags) &&
			(flags & SLALLPHYS))
			return (slp);
	}

	return (NULL);
}

/*
 * Set or clear the device ipq pointer.
 * XXX Assumes IPv4 and IPv6 are SLFAST.
 */
static void
lesetipq(struct le *lep)
{
	struct	lestr	*slp;
	int	ok = 1;
	int	ok6 = 1;
	queue_t	*ipq = NULL;
	queue_t	*ip6q = NULL;

	rw_enter(&lestruplock, RW_READER);

	for (slp = lestrup; slp; slp = slp->sl_nextp) {
		if (slp->sl_lep == lep) {
			if (slp->sl_flags & (SLALLPHYS|SLALLSAP)) {
				ok = 0;
				ok6 = 0;
				break;
			}
			if (slp->sl_sap == ETHERTYPE_IP) {
				if (ipq == NULL)
					ipq = slp->sl_rq;
				else
					ok = 0;
			}
			if (slp->sl_sap == ETHERTYPE_IPV6) {
				if (ip6q == NULL)
					ip6q = slp->sl_rq;
				else
					ok6 = 0;
			}
		}
	}
	rw_exit(&lestruplock);

	if (ok)
		lep->le_ipq = ipq;
	else
		lep->le_ipq = NULL;

	if (ok6)
		lep->le_ip6q = ip6q;
	else
		lep->le_ip6q = NULL;
}

static void
leread(struct le *lep, volatile struct lmd *rmdp)
{
	mblk_t	*mp;
	struct	lebuf	*lbp, *nlbp;
	struct	ether_header	*ehp;
	t_uscalar_t type;
	queue_t	*ipq;
	queue_t	*ip6q;
	uintptr_t bufp;
	uint16_t len;
	uintptr_t sbound, soff;
	uintptr_t kaddr;
	uint32_t ioaddr;
	int	i;
#ifdef NETDEBUGGER
	struct ip *iphp;		/* ip header pointer */
	struct udphdr *udphp;		/* udp header pointer */
#endif /* NETDEBUGGER */

	TRACE_0(TR_FAC_LE, TR_LE_READ_START, "leread start");

	/*
	 * Check for packet errors.
	 */
	if ((rmdp->lmd_flags & ~RMD_OFLO) != (LMD_STP | LMD_ENP)) {
		le_rcv_error(lep, (struct lmd *)rmdp);
		lep->le_ierrors++;
		TRACE_0(TR_FAC_LE, TR_LE_READ_END, "leread end");
		return;
	}

	i = rmdp - lep->le_rmdp;
	lbp = lep->le_rbufp[i];

	bufp = (uintptr_t)lbp->lb_buf + LEHEADROOM;
	len = rmdp->lmd_mcnt - ETHERFCSL;

	if (len < ETHERMIN) {
		lep->le_ierrors++;
		TRACE_0(TR_FAC_LE, TR_LE_READ_END, "leread end");
		return;
	}

	/*
	 * Sync the received buffer before looking at it.
	 */

	LESYNCBUF(lep, bufp, len, DDI_DMA_SYNC_FORKERNEL);

	/*
	 * If a MASTER and buffers are available,
	 * then "loan up".  Otherwise allocb a new mblk and copy.
	 */
	if (!(lep->le_flags & LESLAVE) && (nlbp = legetbuf(lep, 0)) != NULL) {

		if ((mp = desballoc((unsigned char *)bufp, len, BPRI_LO,
			&lbp->lb_frtn)) == NULL) {
			lep->le_allocbfail++;
			lep->le_ierrors++;
			lep->le_norcvbuf++;
			if (ledebug)
				lerror(lep->le_dip, "desballoc failed");
			lefreebuf(nlbp);
			TRACE_0(TR_FAC_LE, TR_LE_READ_END, "leread end");
			return;
		}
		mp->b_wptr += len;

		/* leave the new rbuf with the current rmd. */
		kaddr = (uintptr_t)nlbp->lb_buf + LEHEADROOM;
		ioaddr = LEBUFIOADDR(lep, kaddr);
		rmdp->lmd_ladr = (uint16_t)ioaddr;
		rmdp->lmd_hadr = (uint32_t)ioaddr >> 16;
		lep->le_rbufp[i] = nlbp;

	} else {

		lep->le_norbufs++;

		/* allocate and aligned-copy */
		if ((mp = allocb(len + (3 * LEBURSTSIZE), BPRI_LO))
			== NULL) {
			lep->le_ierrors++;
			lep->le_allocbfail++;
			lep->le_norcvbuf++;
			if (ledebug)
				lerror(lep->le_dip, "allocb fail");
			TRACE_0(TR_FAC_LE, TR_LE_READ_END, "leread end");
			return;
		}

		mp->b_rptr =
			(void *)LEROUNDUP((uintptr_t)mp->b_rptr, LEBURSTSIZE);
		sbound = bufp & ~LEBURSTMASK;
		soff = bufp - sbound;
		bcopy((caddr_t)sbound, (caddr_t)mp->b_rptr,
			LEROUNDUP(len + soff, LEBURSTSIZE));
		mp->b_rptr += soff;
		mp->b_wptr = mp->b_rptr + len;
	}

	ehp = (struct ether_header *)bufp;
	ipq = lep->le_ipq;
	ip6q = lep->le_ip6q;
	type = ehp->ether_type;

	lep->le_ipackets++;

	/*
	 * update MIB II statistics
	 */
	BUMP_InNUcast(lep, ehp);
	lep->le_rcvbytes += len;

#ifdef NETDEBUGGER
	iphp = (struct ip *)(((char *)ehp) + sizeof (struct ether_header));
	udphp = (struct udphdr *)(((char *)iphp) + sizeof (struct ip));

	if ((type == ETHERTYPE_IP) &&
	    (iphp->ip_p == IPPROTO_UDP) &&
	    (udphp->uh_dport == DEBUG_PORT_NUM)) {
		debug_enter("Network request to enter debugger");
	} else {
#endif /* NETDEBUGGER */

	/*
	 * IP shortcut
	 */
	if ((type == ETHERTYPE_IP) &&
		((ehp->ether_dhost.ether_addr_octet[0] & 01) == 0) &&
		(ipq) && canputnext(ipq)) {
		mp->b_rptr += sizeof (struct ether_header);
		putnext(ipq, mp);
	} else if ((type == ETHERTYPE_IPV6) &&
		((ehp->ether_dhost.ether_addr_octet[0] & 01) == 0) &&
		(ip6q) && canputnext(ip6q)) {
		mp->b_rptr += sizeof (struct ether_header);
		putnext(ip6q, mp);
	} else {
		/* Strip the PADs for 802.3 */
		if (type + sizeof (struct ether_header) < ETHERMIN)
			mp->b_wptr = mp->b_rptr
					+ sizeof (struct ether_header)
					+ type;
		lesendup(lep, mp, leaccept);
	}
#ifdef NETDEBUGGER
	}
#endif /* NETDEBUGGER */

	TRACE_0(TR_FAC_LE, TR_LE_READ_END, "leread end");
}

/*
 * Send packet upstream.
 * Assume mp->b_rptr points to ether_header.
 */
static void
lesendup(struct le *lep, mblk_t *mp, struct lestr *(*acceptfunc)())
{
	struct ether_addr *dhostp, *shostp;
	struct lestr *slp, *nslp;
	t_uscalar_t isgroupaddr, type;
	mblk_t *nmp;

	TRACE_0(TR_FAC_LE, TR_LE_SENDUP_START, "lesendup start");

	dhostp = &((struct ether_header *)mp->b_rptr)->ether_dhost;
	shostp = &((struct ether_header *)mp->b_rptr)->ether_shost;
	type = ((struct ether_header *)mp->b_rptr)->ether_type;

	isgroupaddr = dhostp->ether_addr_octet[0] & 01;

	/*
	 * While holding a reader lock on the linked list of streams structures,
	 * attempt to match the address criteria for each stream
	 * and pass up the raw M_DATA ("fastpath") or a DL_UNITDATA_IND.
	 */

	rw_enter(&lestruplock, RW_READER);

	if ((slp = (*acceptfunc)(lestrup, lep, type, dhostp)) == NULL) {
		rw_exit(&lestruplock);

		/*
		 * On MACIO-based sun4m machines, network collisions in
		 * conjunction with back-to-back Inter-packet gap transmissions
		 * that violate the Ethernet/IEEE 802.3 specification may cause
		 * the NCR92C990 Lance internal fifo pointers to lose sync.
		 * This can result in one or more bytes of data prepended to
		 * the ethernet header from the previous packet that is written
		 * to memory.
		 * The upper layer protocol will disregard those packets and
		 * ethernet interface will have appeared to be hung.
		 * Check here for this condition since the leaccept routine
		 * did not find a match.
		 */
		if (lep->le_init && ((*acceptfunc) == &leaccept)) {
			unsigned char *dp;
			int eaddr_ok = 1;

			/*
			 * Ref: Bug Id# 4068119 :: patch 103903 causes le
			 * interfaces receive input errors
			 * Cannot count incoming broadcast packets with no
			 * listener as an input error. This should be done
			 * only after verifying that the broadcast ethernet
			 * address is not starting at an offset of 1-4 bytes
			 * as observed on MACIO-based sun4m machines: Sumanth
			 * lep->le_ierrors++; *** Removed this line 10/15/97***
			 */


			/*
			 * Verify this condition by checking to see
			 * if the broadcast ethernet address starts in
			 * byte positions 1 - 4 of the header
			 */
			dp = (unsigned char *)&dhostp->ether_addr_octet[0];

			if (!(bcmp((char *)&dp[1],
					(char *)&etherbroadcastaddr, 6)) ||
			    !(bcmp((char *)&dp[2],
					(char *)&etherbroadcastaddr, 6)) ||
			    !(bcmp((char *)&dp[3],
					(char *)&etherbroadcastaddr, 6)) ||
			    !(bcmp((char *)&dp[4],
					(char *)&etherbroadcastaddr, 6)))
				eaddr_ok = 0;

			/*
			 * If the dest ethernet address is misaligned then
			 * reset the MACIO Lance chip
			 */
			if (!eaddr_ok) {
				/*
				* Increment input_errors here only when we
				* verify that the destination address is
				* Invalid : Sumanth
				* * Following line has been added : 10/15/97 *
				*/
				lep->le_ierrors++;

				if (ledebug)
					lerror(lep->le_dip,
				"Invalid dest address: %x:%x:%x:%x:%x:%x\n",
						dp[0], dp[1], dp[2],
						dp[3], dp[4], dp[5]);
				lep->loc_flag = 1;
			}
		}
		freemsg(mp);
		TRACE_0(TR_FAC_LE, TR_LE_SENDUP_END, "lesendup end");
		return;
	}

	/*
	 * Loop on matching open streams until (*acceptfunc)() returns NULL.
	 */
	for (; nslp = (*acceptfunc)(slp->sl_nextp, lep, type, dhostp);
		slp = nslp)
		if (canputnext(slp->sl_rq))
			if (nmp = dupmsg(mp)) {
				if ((slp->sl_flags & SLFAST) && !isgroupaddr) {
					nmp->b_rptr +=
						sizeof (struct ether_header);
					putnext(slp->sl_rq, nmp);
				} else if (slp->sl_flags & SLRAW)
					putnext(slp->sl_rq, nmp);
				else if ((nmp = leaddudind(lep, nmp, shostp,
						dhostp, type, isgroupaddr)))
						putnext(slp->sl_rq, nmp);
			} else {
				lep->le_allocbfail++;
			}
		else
			lep->le_nocanput++;


	/*
	 * Do the last one.
	 */
	if (canputnext(slp->sl_rq)) {
		if ((slp->sl_flags & SLFAST) && !isgroupaddr) {
			mp->b_rptr += sizeof (struct ether_header);
			putnext(slp->sl_rq, mp);
		} else if (slp->sl_flags & SLRAW)
			putnext(slp->sl_rq, mp);
		else if ((mp = leaddudind(lep, mp, shostp, dhostp,
			type, isgroupaddr)))
			putnext(slp->sl_rq, mp);
	} else {
		freemsg(mp);
		lep->le_nocanput++;
		lep->le_norcvbuf++;
	}

	rw_exit(&lestruplock);
	TRACE_0(TR_FAC_LE, TR_LE_SENDUP_END, "lesendup end");
}

/*
 * Prefix msg with a DL_UNITDATA_IND mblk and return the new msg.
 */
static mblk_t *
leaddudind(struct le *lep, mblk_t *mp, struct ether_addr *shostp,
    struct ether_addr *dhostp, t_uscalar_t type, t_uscalar_t isgroupaddr)
{
	dl_unitdata_ind_t	*dludindp;
	struct	ledladdr	*dlap;
	mblk_t	*nmp;
	int	size;

	TRACE_0(TR_FAC_LE, TR_LE_ADDUDIND_START, "leaddudind start");

	mp->b_rptr += sizeof (struct ether_header);

	/*
	 * Allocate an M_PROTO mblk for the DL_UNITDATA_IND.
	 */
	size = sizeof (dl_unitdata_ind_t) + LEADDRL + LEADDRL;
	nmp = allocb(LEROUNDUP(LEHEADROOM + size, sizeof (double)), BPRI_LO);
	if (nmp == NULL) {
		lep->le_allocbfail++;
		lep->le_ierrors++;
		lep->le_norcvbuf++;
		if (ledebug)
			lerror(lep->le_dip, "allocb failed");
		freemsg(mp);
		TRACE_0(TR_FAC_LE, TR_LE_ADDUDIND_END, "leaddudind end");
		return (NULL);
	}
	DB_TYPE(nmp) = M_PROTO;
	nmp->b_wptr = nmp->b_datap->db_lim;
	nmp->b_rptr = nmp->b_wptr - size;

	/*
	 * Construct a DL_UNITDATA_IND primitive.
	 */
	dludindp = (dl_unitdata_ind_t *)nmp->b_rptr;
	dludindp->dl_primitive = DL_UNITDATA_IND;
	dludindp->dl_dest_addr_length = LEADDRL;
	dludindp->dl_dest_addr_offset = sizeof (dl_unitdata_ind_t);
	dludindp->dl_src_addr_length = LEADDRL;
	dludindp->dl_src_addr_offset = sizeof (dl_unitdata_ind_t) + LEADDRL;
	dludindp->dl_group_address = isgroupaddr;

	dlap = (struct ledladdr *)(nmp->b_rptr + sizeof (dl_unitdata_ind_t));
	ether_copy(dhostp, &dlap->dl_phys);
	dlap->dl_sap = type;

	dlap = (struct ledladdr *)(nmp->b_rptr + sizeof (dl_unitdata_ind_t)
		+ LEADDRL);
	ether_copy(shostp, &dlap->dl_phys);
	dlap->dl_sap = type;

	/*
	 * Link the M_PROTO and M_DATA together.
	 */
	nmp->b_cont = mp;
	TRACE_0(TR_FAC_LE, TR_LE_ADDUDIND_END, "leaddudind end");
	return (nmp);
}

/*
 * Return TRUE if the given multicast address is one
 * of those that this particular Stream is interested in.
 */
static
lemcmatch(struct lestr *slp, struct ether_addr *addrp)
{
	struct ether_addr *mcbucket;
	int i;
	uint32_t  mchash, mccount;

	/*
	 * Return FALSE if not a multicast address.
	 */
	if (!(addrp->ether_addr_octet[0] & 01))
		return (0);

	/*
	 * Check if all multicasts have been enabled for this Stream
	 */
	if (slp->sl_flags & SLALLMULTI)
		return (1);

	/*
	 * Compute the hash value for the address and
	 * grab the bucket and the number of entries in the
	 * bucket.
	 */

	mchash = MCHASH(addrp);
	mcbucket = slp->sl_mctab[mchash];
	mccount = slp->sl_mccount[mchash];

	/*
	 * Return FALSE if no multicast addresses enabled for this Stream.
	 */

	if (mccount == 0)
	    return (0);

	/*
	 * Otherwise, find it in the table.
	 */

	if (mcbucket)
		for (i = 0; i < mccount; i++)
			if (!ether_cmp(addrp, &mcbucket[i]))
				return (1);

	return (0);
}

/*
 * Initialize chip and driver.
 * Return 0 on success, nonzero on error.
 */
static int
leinit(struct le *lep)
{
	volatile	struct	lance_init_block	*ibp;
	volatile	struct	lanceregs	*regsp;
	struct	lebuf	*lbp;
	struct	lestr	*slp;
	struct	leops	*lop;
	ushort_t	ladrf[4];
	uint32_t ioaddr;
	int	i;
#ifdef LE_SYNC_INIT
	int	leflags  = lep->le_flags;
#endif /* LE_SYNC_INIT */

	TRACE_1(TR_FAC_LE, TR_LE_INIT_START,
		"leinit start:  lep %p", lep);

	if (lep->le_flags & LESUSPENDED)
		(void) ddi_dev_is_needed(lep->le_dip, 0, 1);

	mutex_enter(&lep->le_intrlock);
	rw_enter(&lestruplock, RW_WRITER);
	mutex_enter(&lep->le_xmitlock);

	/*
	 * Reset any timeout that may have started
	 */
	if (lep->le_init && lep->le_timeout_id) {
		(void) untimeout(lep->le_timeout_id);
		lep->le_timeout_id = 0;
	}

	lep->le_flags = 0;
	lep->le_wantw = 0;
	lep->le_inits++;
	lep->le_intr_flag = 1;		/* fix for race condition on fusion */
	lep->loc_flag = 0;
	/*
	 * Stop the chip.
	 */
	regsp = lep->le_regsp;

	/*
	 * Set device-specific information here
	 * before calling leallocthings().
	 */
	lop = (struct leops *)leopsfind(lep->le_dip);
	if (lop) {
		if (lop->lo_flags & LOSLAVE)
			lep->le_flags |= LESLAVE;
		lep->le_membase = lop->lo_base;
		lep->le_memsize = lop->lo_size;
		lep->le_init = lop->lo_init;
		lep->le_intr = lop->lo_intr;
		lep->le_arg = lop->lo_arg;
	}

	/*
	 * Reject this device if it's a Bus Master in a slave-only slot.
	 */
	if ((ddi_slaveonly(lep->le_dip) == DDI_SUCCESS) &&
		(!(lep->le_flags & LESLAVE))) {
		lerror(lep->le_dip,
			"this card won't work in a slave-only slot");
		goto done;
	}

#ifdef LE_SYNC_INIT
	if ((leflags & LERUNNING) && lep->le_init && le_sync_init) {
		volatile struct	lmd	*tmdp;
		/*
		 * if running and any pending writes
		 * wait for any potential dma to finish
		 */
		tmdp = lep->le_tcurp;

		while (tmdp != lep->le_tnextp) {
			LESYNCIOPB(lep, tmdp, sizeof (struct lmd),
				DDI_DMA_SYNC_FORCPU);
			CDELAY(((tmdp->lmd_flags & (LMD_OWN | TMD_INUSE)) == 0),
				1000);
			tmdp = NEXTTMD(lep, tmdp);
		}
	}
#endif /* LE_SYNC_INIT */

	/*
	 * Allocate data structures.
	 */
	leallocthings(lep);

	/*
	 * MACIO B.1 requires E_CSR reset before INIT.
	 */
	if (lep->le_init)
		(*lep->le_init)(lep->le_arg, lep->le_tpe, &lep->le_bufhandle);

	/* Access LANCE registers after DMA2 is initialized. ESC #9879  */

	regsp->lance_rap = LANCE_CSR0;
	regsp->lance_csr = LANCE_STOP;

	/*
	 * Free any pending buffers or mgs.
	 */
	for (i = 0; i < 128; i++) {
		if (lep->le_tbufp[i]) {
			lefreebuf(lep->le_tbufp[i]);
			lep->le_tbufp[i] = NULL;
		}
		if (lep->le_tmblkp[i]) {
			freemsg(lep->le_tmblkp[i]);
			lep->le_tmblkp[i] = NULL;
		}
		if (lep->le_rbufp[i]) {
			lefreebuf(lep->le_rbufp[i]);
			lep->le_rbufp[i] = NULL;
		}
	}


	/*
	 * Reset RMD and TMD 'walking' pointers.
	 */
	lep->le_rnextp = lep->le_rmdp;
	lep->le_tcurp = lep->le_tmdp;
	lep->le_tnextp = lep->le_tmdp;

	/*
	 * Construct the LANCE initialization block.
	 */

	ibp = lep->le_ibp;
	bzero((void *)ibp, sizeof (struct lance_init_block));

	/*
	 * Mode word 0 should be all zeros except
	 * possibly for the promiscuous mode bit.
	 */
	ibp->ib_prom = 0;
	for (slp = lestrup; slp; slp = slp->sl_nextp)
		if ((slp->sl_lep == lep) && (slp->sl_flags & SLALLPHYS)) {
			ibp->ib_prom = 1;
			lep->le_flags |= LEPROMISC;
			break;
		}

	/*
	 * Set our local individual ethernet address.
	 */
	ibp->ib_padr[0] = lep->le_ouraddr.ether_addr_octet[1];
	ibp->ib_padr[1] = lep->le_ouraddr.ether_addr_octet[0];
	ibp->ib_padr[2] = lep->le_ouraddr.ether_addr_octet[3];
	ibp->ib_padr[3] = lep->le_ouraddr.ether_addr_octet[2];
	ibp->ib_padr[4] = lep->le_ouraddr.ether_addr_octet[5];
	ibp->ib_padr[5] = lep->le_ouraddr.ether_addr_octet[4];

	/*
	 * Set up multicast address filter by passing all multicast
	 * addresses through a crc generator, and then using the
	 * high order 6 bits as a index into the 64 bit logical
	 * address filter. The high order two bits select the word,
	 * while the rest of the bits select the bit within the word.
	 */

	for (i = 0; i < 4; i++)
		ladrf[i] = 0;

	for (slp = lestrup; slp; slp = slp->sl_nextp)
		if (slp->sl_lep == lep) {
			if (slp->sl_flags & SLALLMULTI) {
				for (i = 0; i < 4; i++) {
					ladrf[i] = 0xffff;
				}
				break;	/* All bits are already on */
			}

			for (i = 0; i < 4; i++)
				ladrf[i] |= slp->sl_ladrf[i];
		}

	for (i = 0; i < 4; i++)
		ibp->ib_ladrf[i] = ladrf[i];


	ioaddr = LEIOPBIOADDR(lep, lep->le_rmdp);
	ibp->ib_rdrp.lr_laddr = ioaddr;
	ibp->ib_rdrp.lr_haddr = ioaddr >> 16;
	ibp->ib_rdrp.lr_len   = lep->le_nrmdp2;

	ioaddr = LEIOPBIOADDR(lep, lep->le_tmdp);
	ibp->ib_tdrp.lr_laddr = ioaddr;
	ibp->ib_tdrp.lr_haddr = ioaddr >> 16;
	ibp->ib_tdrp.lr_len   = lep->le_ntmdp2;

	/*
	 * Clear all descriptors.
	 */
	bzero(lep->le_rmdp, lep->le_nrmds * sizeof (struct lmd));
	bzero(lep->le_tmdp, lep->le_ntmds * sizeof (struct lmd));

	/*
	 * Hang out receive buffers.
	 */
	for (i = 0; i < lep->le_nrmds; i++) {
		if ((lbp = legetbuf(lep, 1)) == NULL) {
			lerror(lep->le_dip, "leinit failed:  out of buffers");
			goto done;
		}
		lermdinit(lep, &lep->le_rmdp[i], lbp);
		lep->le_rbufp[i] = lbp;	/* save for later use */
	}

	/*
	 * Set CSR1, CSR2, and CSR3.
	 */
	ioaddr = LEIOPBIOADDR(lep, lep->le_ibp);
	regsp->lance_rap = LANCE_CSR1;	/* select the low address register */
	regsp->lance_rdp = ioaddr & 0xffff;
	regsp->lance_rap = LANCE_CSR2;	/* select the high address register */
	regsp->lance_rdp = (ioaddr >> 16) & 0xff;
	regsp->lance_rap = LANCE_CSR3;	/* Bus Master control register */
	regsp->lance_rdp = ddi_getprop(DDI_DEV_T_ANY, lep->le_dip, 0,
		"busmaster-regval", LANCE_BSWP | LANCE_ACON | LANCE_BCON);

	/*
	 * Sync the init block and descriptors.
	 */
	LESYNCIOPB(lep, lep->le_iopbkbase,
		(sizeof (struct lance_init_block)
		+ (lep->le_nrmds * sizeof (struct lmd))
		+ (lep->le_ntmds * sizeof (struct lmd))
		+ (4 * LANCEALIGN)),
		DDI_DMA_SYNC_FORDEV);

	/*
	 * Chip init.
	 */
	regsp->lance_rap = LANCE_CSR0;
	regsp->lance_csr = LANCE_INIT;

	/*
	 * Allow 10 ms for the chip to complete initialization.
	 */
	CDELAY((regsp->lance_csr & LANCE_IDON), 10000);
	if (!(regsp->lance_csr & LANCE_IDON)) {
		lerror(lep->le_dip, "LANCE chip didn't initialize!");
		goto done;
	}
	regsp->lance_csr = LANCE_IDON;		/* Clear this bit */

	/*
	 * Chip start.
	 */
	regsp->lance_csr = LANCE_STRT | LANCE_INEA;

	lep->le_flags |= LERUNNING;

	/*
	 * Only do this for ledma devices
	 */
	if (lep->le_init && lewdflag) {

		/* initialize lbolts for rx/tx */
		lep->le_rx_lbolt = lbolt;
		lep->le_tx_lbolt = lbolt;

		/* save address of dma tst_csr */
		lep->le_dma2_tcsr = (uint32_t *)ddi_get_driver_private(
					ddi_get_parent(lep->le_dip)) + 4;

		/* initialize timeouts */
		lep->le_timeout_id = timeout(le_watchdog, lep,
			drv_usectohz(lewdinterval * 1000));
	}

	lewenable(lep);

done:
	mutex_exit(&lep->le_xmitlock);
	rw_exit(&lestruplock);
	mutex_exit(&lep->le_intrlock);

	TRACE_1(TR_FAC_LE, TR_LE_INIT_END,
		"leinit end:  lep %p", lep);

	return (!(lep->le_flags & LERUNNING));
}

/*
 * Un-initialize (STOP) LANCE
 */
static void
leuninit(struct le *lep)
{
	/*
	 * Allow up to 'ledraintime' for pending xmit's to complete.
	 */
	CDELAY((lep->le_tcurp == lep->le_tnextp), LEDRAINTIME);

	mutex_enter(&lep->le_intrlock);
	mutex_enter(&lep->le_xmitlock);
	mutex_enter(&lep->le_buflock);

	lep->le_flags &= ~LERUNNING;

	/*
	 * Stop the chip.
	 */
	lep->le_regsp->lance_rap = LANCE_CSR0;
	lep->le_regsp->lance_csr = LANCE_STOP;

	mutex_exit(&lep->le_buflock);
	mutex_exit(&lep->le_xmitlock);
	mutex_exit(&lep->le_intrlock);
}

#define	LEBUFSRESERVED	4

static struct lebuf *
legetbuf(struct le *lep, int pri)
{
	struct	lebuf	*lbp;
	int	i;

	TRACE_1(TR_FAC_LE, TR_LE_GETBUF_START,
		"legetbuf start:  lep %p", lep);

	mutex_enter(&lep->le_buflock);

	i = lep->le_bufi;

	if ((i == 0) || ((pri == 0) && (i < LEBUFSRESERVED))) {
		mutex_exit(&lep->le_buflock);
		TRACE_1(TR_FAC_LE, TR_LE_GETBUF_END,
			"legetbuf end:  lep %p", lep);
		return (NULL);
	}

	lbp = lep->le_buftab[--i];
	lep->le_bufi = i;

	mutex_exit(&lep->le_buflock);

	TRACE_1(TR_FAC_LE, TR_LE_GETBUF_END,
		"legetbuf end:  lep %p", lep);
	return (lbp);
}

static void
lefreebuf(struct lebuf *lbp)
{
	struct le *lep = lbp->lb_lep;

	TRACE_1(TR_FAC_LE, TR_LE_FREEBUF_START,
		"lefreebuf start:  lep %p", lep);

	mutex_enter(&lep->le_buflock);

	lep->le_buftab[lep->le_bufi++] = lbp;

	mutex_exit(&lep->le_buflock);

	if (lep->le_wantw)
		lewenable(lep);
	TRACE_1(TR_FAC_LE, TR_LE_FREEBUF_END,
		"lefreebuf end:  lep %p", lep);
}

/*
 * Initialize RMD.
 */
static void
lermdinit(struct le *lep, volatile struct lmd *rmdp, struct lebuf *lbp)
{
	uintptr_t kaddr;
	uint32_t ioaddr;

	kaddr = (uintptr_t)lbp->lb_buf + LEHEADROOM;
	ioaddr = LEBUFIOADDR(lep, kaddr);

	rmdp->lmd_ladr = (uint16_t)ioaddr;
	rmdp->lmd_hadr = (uint32_t)ioaddr >> 16;
	rmdp->lmd_bcnt = (uint16_t)-(LEBUFSIZE - LEHEADROOM);
	rmdp->lmd_mcnt = 0;
	rmdp->lmd_flags = LMD_OWN;
}

/*
 * Report Receive errors.
 */
static void
le_rcv_error(struct le *lep, struct lmd *rmdp)
{
	uint_t flags = rmdp->lmd_flags;
	static	ushort_t gp_count = 0;

	if ((flags & RMD_FRAM) && !(flags & RMD_OFLO)) {
		if (ledebug)
			lerror(lep->le_dip, "Receive: framing error");
		lep->le_fram++;
	}
	if ((flags & RMD_CRC) && !(flags & RMD_OFLO)) {
		if (ledebug)
			lerror(lep->le_dip, "Receive: crc error");
		lep->le_crc++;
	}
	if ((flags & RMD_OFLO) && !(flags & LMD_ENP)) {
		if (ledebug)
			lerror(lep->le_dip, "Receive: overflow error");
		lep->le_oflo++;
	}
	if (flags & RMD_BUFF)
		lerror(lep->le_dip, "Receive: BUFF set in rmd");
	/*
	 * If an OFLO error occurred, the chip may not set STP or ENP,
	 * so we ignore a missing ENP bit in these cases.
	 */
	if (!(flags & LMD_STP) && !(flags & RMD_OFLO)) {
		if (ledebug)
			lerror(lep->le_dip, "Receive: STP in rmd cleared");
		/*
		 * if using a macio ethernet chip - reset it
		 * as it may have gone into a hung state
		 */
		if ((flags & LMD_ENP) && (lep->le_init)) {
			/*
			 * only reset if the packet size was
			 * greater than 4096 bytes
			 */
			if (gp_count > rmdp->lmd_mcnt) {
				if (ledebug)
					lerror(lep->le_dip,
						"Receive: reset ethernet");
				lep->loc_flag = 1;
			}
			gp_count = 0;
		} else
			gp_count += LEBUFSIZE;
	} else if (!(flags & LMD_ENP) && !(flags & RMD_OFLO)) {
		if (ledebug)
			lerror(lep->le_dip,
				"Receive: giant packet");
		gp_count += LEBUFSIZE;
	}
}

static char *lenocar1 =
	"No carrier - transceiver cable problem?";
static char *lenocar2 =
	"No carrier - cable disconnected or hub link test disabled?";

/*
 * Report on transmission errors paying very close attention
 * to the Rev B (October 1986) Am7990 programming manual.
 * Return 1 if leinit() is called, 0 otherwise.
 */
static int
le_xmit_error(struct le *lep, struct lmd *tmdp)
{
	uint_t flags = tmdp->lmd_flags3;

	/*
	 * BUFF is not valid if either RTRY or LCOL is set.
	 * We assume here that BUFFs are always caused by UFLO's
	 * and not driver bugs.
	 */
	if ((flags & (TMD_BUFF | TMD_RTRY | TMD_LCOL)) == TMD_BUFF) {
		if (ledebug)
			lerror(lep->le_dip, "Transmit: BUFF set in tmd");
	}
	if (flags & TMD_UFLO) {
		if (ledebug)
			lerror(lep->le_dip, "Transmit underflow");
		lep->le_uflo++;
	}
	if (flags & TMD_LCOL) {
		if (ledebug)
			lerror(lep->le_dip,
				"Transmit late collision - net problem?");
		lep->le_tlcol++;
	}

	/*
	 * Early MACIO chips set both LCAR and RTRY on RTRY.
	 */
	if ((flags & (TMD_LCAR|TMD_RTRY)) == TMD_LCAR) {
		if (lep->le_init) {
			if (lep->le_oopkts == lep->le_opackets) {
				lerror(lep->le_dip, "%s", lenocar2);
				lep->le_oopkts = -1;
				lep->le_tnocar++;
			} else
				lep->le_oopkts = lep->le_opackets;
			if (lep->le_autosel)
				lep->le_tpe = 1 - lep->le_tpe;
			lep->loc_flag = 1;
			return (1);
		} else
			lerror(lep->le_dip, "%s", lenocar1);
		lep->le_tnocar++;
	}
	if (flags & TMD_RTRY) {
		if (ledebug)
			lerror(lep->le_dip,
		"Transmit retried more than 16 times - net  jammed");
		lep->le_collisions += 16;
		lep->le_trtry++;
	}
	return (0);
}

/*
 * Handle errors reported in LANCE CSR0.
 */
static
le_chip_error(struct le *lep)
{
	ushort_t	csr0 = lep->le_regsp->lance_csr;
	int restart = 0;

	if (csr0 & LANCE_MISS) {
		if (ledebug)
			lerror(lep->le_dip, "missed packet");
		lep->le_ierrors++;
		lep->le_missed++;
		lep->le_regsp->lance_csr = LANCE_MISS | LANCE_INEA;
	}

	if (csr0 & LANCE_BABL) {
	    lerror(lep->le_dip,
		"Babble error - packet longer than 1518 bytes");
		lep->le_oerrors++;
	    lep->le_regsp->lance_csr = LANCE_BABL | LANCE_INEA;
	}
	/*
	 * If a memory error has occurred, both the transmitter
	 * and the receiver will have shut down.
	 * Display the Reception Stopped message if it
	 * wasn't caused by MERR (should never happen) OR ledebug.
	 * Display the Transmission Stopped message if it
	 * wasn't caused by MERR (was caused by UFLO) AND ledebug
	 * since we will have already displayed a UFLO msg.
	 */
	if (csr0 & LANCE_MERR) {
		if (ledebug)
			lerror(lep->le_dip, "Memory error!");
		lep->le_ierrors++;
		lep->le_regsp->lance_csr = LANCE_MERR | LANCE_INEA;
		restart++;
	}
	if (!(csr0 & LANCE_RXON)) {
	    if (!(csr0 & LANCE_MERR) || ledebug)
		lerror(lep->le_dip, "Reception stopped");
	    restart++;
	}
	if (!(csr0 & LANCE_TXON)) {
		if (!(csr0 & LANCE_MERR) && ledebug)
			lerror(lep->le_dip, "Transmission stopped");
		restart++;
	}

	if (restart) {
		lep->loc_flag = 1;
	}
	return (restart);
}

/*VARARGS*/
static void
lerror(dev_info_t *dip, char *fmt, ...)
{
	static	long	last;
	static	char	*lastfmt;
	char		msg_buffer[255];
	va_list	ap;

	mutex_enter(&lelock);

	/*
	 * Don't print same error message too often.
	 */
	if ((last == (hrestime.tv_sec & ~1)) && (lastfmt == fmt)) {
		mutex_exit(&lelock);
		return;
	}
	last = hrestime.tv_sec & ~1;
	lastfmt = fmt;


	va_start(ap, fmt);
	(void) vsprintf(msg_buffer, fmt, ap);
	cmn_err(CE_CONT, "%s%d: %s\n", ddi_get_name(dip),
					ddi_get_instance(dip),
					msg_buffer);
	va_end(ap);

	mutex_exit(&lelock);
}

/*
 * Create and add an leops structure to the linked list.
 */
static void
leopsadd(struct leops *lop)
{
	mutex_enter(&lelock);
	lop->lo_next = leops;
	leops = lop;
	mutex_exit(&lelock);
}

struct leops *
leopsfind(dev_info_t *dip)
{
	struct	leops	*lop;

	for (lop = leops; lop; lop = lop->lo_next)
		if (lop->lo_dip == dip)
			return (lop);
	return (NULL);
}

static int
le_tx_pending(struct le *lep)
{
	if (lep->le_tcurp == lep->le_tnextp)
		return (0);
	else
		return (1);
}

static int
le_rx_timeout(struct le *lep)
{
	if (((lbolt - lep->le_rx_lbolt) >
	    drv_usectohz(lewdrx_timeout * 1000)) && lewdrx_timeout)
		return (1);
	else
		return (0);
}

static int
le_tx_timeout(struct le *lep)
{
	if (((lbolt - lep->le_tx_lbolt) >
	    drv_usectohz(lewdtx_timeout * 1000)) &&
	    lewdtx_timeout && le_tx_pending(lep))
		return (1);
	else
		return (0);
}

static void
le_watchdog(void *le_arg)
{
	struct le *lep = le_arg;
	static int le_count_ticks = 0;

	ASSERT(lep->le_init);

	/*
	 * reset the chip if TX timeout detected
	 */
	if (lewdflag & LEWD_FLAG_TX_TIMEOUT) {
		if (le_tx_timeout(lep) && le_check_ledma(lep)) {
			if (ledebug)
				lerror(lep->le_dip,
				    "Watchdog: transmit timeout");
			(void) leinit(lep);
			return;
		}
	}

	/*
	 * Check for RX timeouts
	 */
	if (lewdflag & LEWD_FLAG_RX_TIMEOUT) {
		if (le_rx_timeout(lep)) {
			/*
			 * If no pending xmits and haven't received a packet
			 * in lewdrx_timeout msecs, check dma2 tst_csr
			 */
			if (!le_tx_pending(lep) && le_check_ledma(lep)) {
				if (ledebug)
					lerror(lep->le_dip,
						"Watchdog: receive timeout");
				(void) leinit(lep);
				return;
			}
		}
	}
	if (lep->loc_flag && (le_count_ticks++ == 5)) {
		le_count_ticks = 0;
		(void) leinit(lep);
	}
	lep->le_timeout_id = timeout(le_watchdog, lep,
		drv_usectohz(lewdinterval * 1000));
}

static int
le_check_ledma(struct le *lep)
{
	uint32_t tst_csr, tst_csr2;
	uint32_t dma_addr;
	uint32_t last_dma_addr;
	uint32_t *dma2_csr;

	/*
	 * Read the TST_CSR register from DMA2 and extract DMA address
	 */
	tst_csr = *lep->le_dma2_tcsr;
	dma_addr = tst_csr & E_ADDR_MASK;

	/*
	 * get the receive dma address from descriptor ring
	 */
	last_dma_addr = lep->le_rnextp->lmd_ladr +
				(lep->le_rnextp->lmd_hadr << 16);

	/*
	 * When the hang occurs, the dma_addr in the test csr has one
	 * of 2 values;
	 *	1. address of last receive addr + offset < ETHERMIN and
	 *	   dma2 cache indicates modified data
	 *	2. address of byte count field in a receive descriptor
	 *	   ie., dma address has offset of 0x4 or 0xc
	 *
	 * Reset the interface on either of these conditions
	 */
	if ((((dma_addr - last_dma_addr) < ETHERMIN) && (tst_csr & E_DIRTY)) ||
	    (((dma_addr & 0x7) == 0x4) &&
	    ((dma_addr + LEDLIMADDRLO) < LEIOPBIOADDR(lep, lep->le_tmdlimp)))) {

		/*
		 * Set the drain bit in the DMA2 CSR, if modified data in
		 * the DMA2 internal buffer is not drained to memory,
		 * then the DMA2 is wedged, reset the interface
		 */
		dma2_csr = (uint32_t *)((uintptr_t)lep->le_dma2_tcsr - 4);
		*dma2_csr |= E_DRAIN;
		drv_usecwait(100);
		tst_csr2 = *lep->le_dma2_tcsr;

		if (tst_csr == tst_csr2) {
			return (1);
		}
	}
	return (0);
}
