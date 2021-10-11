/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)qe.c	1.80	99/10/22 SMI"

/*
 * SunOS MT STREAMS Quad-MACE Ethernet Device Driver
 */


#include	<sys/types.h>
#include	<sys/debug.h>
#include	<sys/stropts.h>
#include	<sys/stream.h>
#include	<sys/cmn_err.h>
#include	<sys/vtrace.h>
#include	<sys/kmem.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>
#include	<sys/strsun.h>
#include	<sys/stat.h>
#include	<sys/kstat.h>
#include	<sys/dlpi.h>
#include	<sys/ethernet.h>
#include	<sys/mace.h>
#include	<sys/qec.h>
#include	<sys/qe.h>
#include	<sys/time.h>
#include	<sys/modctl.h>


/*
 * Function prototypes.
 */
static	int qeattach(dev_info_t *, ddi_attach_cmd_t);
static	int qedetach(dev_info_t *, ddi_detach_cmd_t);
static	int qechanstop(struct qe *);
static	void qestatinit(struct qe *);
static	int qeinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static	void qeallocthings(struct qe *);
static	void qefreebufs(struct qe *);
static	int qeopen(queue_t *, dev_t *, int, int, cred_t *);
static	int qeclose(queue_t *);
static	int qewput(queue_t *, mblk_t *);
static	int qewsrv(queue_t *);
static	void qeproto(queue_t *, mblk_t *);
static	void qeioctl(queue_t *, mblk_t *);
static	void qe_dl_ioc_hdr_info(queue_t *, mblk_t *);
static	void qeareq(queue_t *, mblk_t *);
static	void qedreq(queue_t *, mblk_t *);
static	void qedodetach(struct qestr *);
static	void qebreq(queue_t *, mblk_t *);
static	void qeubreq(queue_t *, mblk_t *);
static	void qeireq(queue_t *, mblk_t *);
static	void qeponreq(queue_t *, mblk_t *);
static	void qepoffreq(queue_t *, mblk_t *);
static	void qeemreq(queue_t *, mblk_t *);
static	void qedmreq(queue_t *, mblk_t *);
static	void qepareq(queue_t *, mblk_t *);
static	void qespareq(queue_t *, mblk_t *);
static	void qeudreq(queue_t *, mblk_t *);
static	int qestart(queue_t *, mblk_t *, struct qe *);
static	int qestart_slow(queue_t *, mblk_t *, struct qe *);
static	void qeintr();
static	void qereclaim(struct qe *);
static	void qewenable(struct qe *);
static	int qeinit(struct qe *);
static	void qeuninit(struct qe *qep);
static	void qermdinit(volatile struct qmd *, ulong_t);
static	void qerror(dev_info_t *dip, char *fmt, ...);
static	void qeother(struct qe *, uint_t);
static	mblk_t *qeaddudind(struct qe *, mblk_t *, struct ether_addr *,
	struct ether_addr *, t_uscalar_t, t_uscalar_t);
static	struct qestr *qeaccept(struct qestr *, struct qe *, int,
	struct ether_addr *);
static	struct qestr *qepaccept(struct qestr *, struct qe *, int,
	struct	ether_addr *);
static	void	qesetipq(struct qe *);
static	int qemcmatch(struct qestr *, struct ether_addr *);
static	void qeread(struct qe *, volatile struct qmd *);
static	void qeread_slow(struct qe *, volatile struct qmd *);
static	void qesendup(struct qe *, mblk_t *, struct qestr *(*)());
static	void qesavecntrs(struct qe *);

static	void qe_stop_timer(struct qe *);
static	void qe_start_timer(struct qe *, void (*)(void *), int);
static	void qedog(void *);

extern kmutex_t	qeclock;

static	struct	module_info	qeminfo = {
	QEIDNUM,	/* mi_idnum */
	QENAME,		/* mi_idname */
	QEMINPSZ,	/* mi_minpsz */
	QEMAXPSZ,	/* mi_maxpsz */
	QEHIWAT,	/* mi_hiwat */
	QELOWAT		/* mi_lowat */
};

static	struct	qinit	qerinit = {
	NULL,		/* qi_putp */
	NULL,		/* qi_srvp */
	qeopen,		/* qi_qopen */
	qeclose,	/* qi_qclose */
	NULL,		/* qi_qadmin */
	&qeminfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static	struct	qinit	qewinit = {
	qewput,		/* qi_putp */
	qewsrv,		/* qi_srvp */
	NULL,		/* qi_qopen */
	NULL,		/* qi_qclose */
	NULL,		/* qi_qadmin */
	&qeminfo,	/* qi_minfo */
	NULL		/* qi_mstat */
};

static struct	streamtab	qe_info = {
	&qerinit,	/* st_rdinit */
	&qewinit,	/* st_wrinit */
	NULL,		/* st_muxrinit */
	NULL		/* st_muxwrinit */
};

static	struct	cb_ops	cb_qe_ops = {
	nodev,			/* cb_open */
	nodev,			/* cb_close */
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
	&qe_info,		/* cb_stream */
	D_MP|D_HOTPLUG,		/* cb_flag */
	CB_REV,			/* rev */
	nodev,			/* int (*cb_aread)() */
	nodev			/* int (*cb_awrite)() */
};

static	struct	dev_ops	qe_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	qeinfo,			/* devo_getinfo */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	qeattach,		/* devo_attach */
	qedetach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_qe_ops,		/* devo_cb_ops */
	(struct bus_ops *)NULL	/* devo_bus_ops */
};

/*
 * Claim the device is ultra-capable of burst in the beginning.  Use
 * the value returned by ddi_dma_burstsizes() to actually set the QEC
 * global control register later.
 */
#define	QEDLIMADDRLO	(0x00000000U)
#define	QEDLIMADDRHI	(0xffffffffU)

static ddi_dma_lim_t qe_dma_limits = {
	QEDLIMADDRLO,	/* dlim_addr_lo */
	QEDLIMADDRHI,	/* dlim_addr_hi */
	QEDLIMADDRHI,	/* dlim_cntr_max */
	0x3f,		/* dlim_burstsizes */
	0x1,		/* dlim_minxfer */
	1024		/* dlim_speed */
};

/*
 * this driver depends on qec.  Make sure that is loaded first
 */
char _depends_on[] = "drv/qec";

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module.  This one is a driver */
	"Quad-MACE Ethernet Driver v1.80",
	&qe_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

/*
 * XXX Autoconfiguration lock:  We want to initialize all the global
 * locks at _init().  However, we do not have the cookie required which
 * is returned in ddi_add_intr() which is usually called at the attach
 * time.
 */
static	kmutex_t	qeautolock;

/*
 * Linked list of active (inuse) driver Streams.
 */
static	struct	qestr	*qestrup = NULL;
static	krwlock_t	qestruplock;
static int qe_sanity_timer = 30;	/* Timer Interval to check qe status */
						/* Rest if hung */

/*
 * Single private "global" lock for the few rare conditions
 * we want single-threaded.
 */
static	kmutex_t	qelock;

int
_init(void)
{
	int	status;

	mutex_init(&qeautolock, NULL, MUTEX_DRIVER, NULL);
	status = mod_install(&modlinkage);
	if (status != 0) {
		mutex_destroy(&qeautolock);
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
	mutex_destroy(&qelock);
	rw_destroy(&qestruplock);
	mutex_destroy(&qeautolock);
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
static	int	qedebug = 0;

/*
 * Patchable flag for the MACE hang problem. When this flag is
 * set, the channel is reset on the following errors:
 *  RETRY
 *  LCOL
 *  UFLO
 */

static int mace_vers = 0;

/*
 * calc of rcv index for fast dma mappings.
 */
#define	QERINDEX(i) \
	((i) % QERPENDING)

#define	DONT_FLUSH -1

/*
 * Allocate and zero-out "number" structures
 * each of type "structure" in kernel memory.
 */
#define	GETSTRUCT(structure, number)   \
	((structure *)kmem_zalloc(\
		(size_t)(sizeof (structure) * (number)), KM_SLEEP))
#define	GETBUF(structure, size)   \
	((structure *)kmem_zalloc((uint_t)(size), KM_SLEEP))

/*
 * Translate a kernel virtual address to i/o address.
 */
#define	QEIOPBIOADDR(qep, a) \
	((uint32_t)((qep)->qe_iopbiobase + ((uintptr_t)(a) - \
		(qep)->qe_iopbkbase)))

/*
 * ddi_dma_sync() a buffer.
 */
#define	QESYNCBUF(qep, a, size, who) \
	(void) ddi_dma_sync((qep)->qe_bufhandle, \
		((uintptr_t)(a) - (qep)->qe_bufkbase), \
		(size), \
		(who))

/*
 * XXX
 * Define QESYNCIOPB to nothing for now.
 * If/when we have PSO-mode kernels running which really need
 * to sync something during a ddi_dma_sync() of iopb-allocated memory,
 * then this can go back in, but for now we take it out
 * to save some microseconds.
 */
#define	QESYNCIOPB(qep, a, size, who)

#define	QESAPMATCH(sap, type, flags) ((sap == type)? 1 : \
	((flags & QESALLSAP)? 1 : \
((sap <= ETHERMTU) && (sap > (t_uscalar_t)0) && (type <= ETHERMTU))? 1 : 0))

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
#define	BUMP_InNUcast(qep, ehp) \
	if (IS_BROADCAST(ehp)) { \
		qep->qe_brdcstrcv++; \
	} else if (IS_MULTICAST(ehp)) { \
		qep->qe_multircv++; \
	}
#define	BUMP_OutNUcast(qep, ehp) \
	if (IS_BROADCAST(ehp)) { \
		qep->qe_brdcstxmt++; \
	} else if (IS_MULTICAST(ehp)) { \
		qep->qe_multixmt++; \
	}



/*
 * Patchable variable for the programmable IFS.
 */
static	int	qeifs = 0x00;

/*
 * Linked list of "qe" structures.
 */
static struct qe *qeup = NULL;

/*
 * force the fallback to ddi_dma routines
 */
static int qe_force_dma = 0;

/*
 * Our DL_INFO_ACK template.
 */
static	dl_info_ack_t qeinfoack = {
	DL_INFO_ACK,				/* dl_primitive */
	ETHERMTU,				/* dl_max_sdu */
	0,					/* dl_min_sdu */
	QEADDRL,				/* dl_addr_length */
	DL_ETHER,				/* dl_mac_type */
	0,					/* dl_reserved */
	0,					/* dl_current_state */
	-2,					/* dl_sap_length */
	DL_CLDLS,				/* dl_service_mode */
	0,					/* dl_qos_length */
	0,					/* dl_qos_offset */
	0,					/* dl_range_length */
	0,					/* dl_range_offset */
	DL_STYLE2,				/* dl_provider_style */
	sizeof (dl_info_ack_t),			/* dl_addr_offset */
	DL_VERSION_2,				/* dl_version */
	ETHERADDRL,				/* dl_brdcst_addr_length */
	sizeof (dl_info_ack_t) + QEADDRL,	/* dl_brdcst_addr_offset */
	0					/* dl_growth */
};



/*
 * Calculate the bit in the multicast address filter that selects the given
 * address.
 */

static uint32_t
qeladrf_bit(struct ether_addr *addr)
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
qeattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct qestr	*sqp;
	int		doqeinit = 0;
	struct qe *qep;
	struct qec_soft *qsp;
	int regno;
	int chan;
	static int once = 1;

	/* Handle the DDI_RESUME command */
	if (cmd == DDI_RESUME) {
		if ((qep = (struct qe *)ddi_get_driver_private(dip))
		    == NULL) {
			return (DDI_FAILURE);
		}
		qep->qe_flags &= ~(QESUSPENDED|QESTOP);

		/* Do qeinit() only for interface that is active */
		rw_enter(&qestruplock, RW_READER);
		for (sqp = qestrup; sqp; sqp = sqp->sq_nextp) {
			if (sqp->sq_qep == qep) {
				doqeinit = 1;
				break;
			}
		}
		rw_exit(&qestruplock);
		if (doqeinit) {
			qep->qe_timerid = 0;
			(void) qeinit(qep);
		}
		return (DDI_SUCCESS);
	}

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	qep = (struct qe *)NULL;

	/*
	 * Allocate soft data structure
	 */
	qep = GETSTRUCT(struct qe, 1);

	/*
	 * Map in the device registers.
	 *
	 * Reg # 0 is the QEC per-channel register set.
	 * Reg # 1 is the MACE register set.
	 */
	if (ddi_dev_nregs(dip, &regno) != DDI_SUCCESS ||
		regno != 2) {
		qerror(dip, "ddi_dev_nregs failed, returned %d", regno);
		goto bad;
	}
	if (ddi_map_regs(dip, 0, (caddr_t *)&(qep->qe_chanregp), 0, 0)) {
		qerror(dip, "ddi_map_regs for qec per-channel reg failed");
		goto bad;
	}
	if (ddi_map_regs(dip, 1, (caddr_t *)&(qep->qe_maceregp), 0, 0)) {
		qerror(dip, "ddi_map_regs for mace reg failed");
		goto bad;
	}

	qep->qe_dip = dip;
	if (qechanstop(qep))
		goto bad;

	qep->qe_chan = chan =
		ddi_getprop(DDI_DEV_T_NONE, dip, 0, "channel#", 0);

	/*
	 * XXX
	 * This property has been defined to trigger S/W workarounds
	 * for H/W bugs. It is used in the following manner:
	 * (property not found is equivalent to value of 0)
	 * 1. value == 0  implies that the MACE hangs on retry,
	 *		  late collision or uflo. So must reset the
	 *		  the channel under these errors.
	 * This property can be used to workaround other H/W
	 * bugs by assigning it appropriate values.
	 */

	if (mace_vers == 0) {
		int prop_len  = sizeof (int);

		if (ddi_getlongprop_buf(DDI_DEV_T_ANY, dip, 0, "mace-version",
					(caddr_t)&mace_vers,
					&prop_len) != DDI_PROP_SUCCESS)
			mace_vers = 0;
	}

	qsp = (struct qec_soft *)ddi_get_driver_private(ddi_get_parent(dip));
	qsp->qs_intr_func[chan] = qeintr;
	qsp->qs_intr_arg[chan] = qep;

	/*
	 * At this point, we are *really* here.
	 */
	ddi_set_driver_private(dip, (caddr_t)qep);

	/*
	 * XXX
	 * Currently the xmit lock is overloaded. In addition
	 * to acquiring this lock at xmit time, it is also acquired
	 * whenever qereclaim is called.
	 */
	mutex_init(&qep->qe_xmitlock, NULL, MUTEX_DRIVER,
		(void *)qsp->qs_cookie);
	mutex_init(&qep->qe_intrlock, NULL, MUTEX_DRIVER,
		(void *)qsp->qs_cookie);

	/*
	 * Get the local ethernet address.
	 */
	(void) localetheraddr((struct ether_addr *)NULL, &qep->qe_ouraddr);

	/*
	 * Create the filesystem device node.
	 */
	if (ddi_create_minor_node(dip, "qe", S_IFCHR,
		ddi_get_instance(dip), DDI_NT_NET, CLONE_DEV) == DDI_FAILURE) {
		qerror(dip, "ddi_create_minor_node failed");
		mutex_destroy(&qep->qe_xmitlock);
		mutex_destroy(&qep->qe_intrlock);
		goto bad;
	}

	mutex_enter(&qeautolock);
	if (once) {
		once = 0;
		rw_init(&qestruplock, NULL, RW_DRIVER, (void *)qsp->qs_cookie);
		mutex_init(&qelock, NULL, MUTEX_DRIVER, (void *)qsp->qs_cookie);
	}
	mutex_exit(&qeautolock);

	mutex_enter(&qelock);
	qep->qe_nextp = qeup;
	qeup = qep;
	mutex_exit(&qelock);

	qestatinit(qep);
	ddi_report_dev(dip);
	return (DDI_SUCCESS);

bad:
	if (qep->qe_maceregp)
		ddi_unmap_regs(dip, 0, (caddr_t *)&(qep->qe_maceregp), 0, 0);
	if (qep->qe_chanregp)
		ddi_unmap_regs(dip, 1, (caddr_t *)&(qep->qe_chanregp), 0, 0);
	if (qep)
		kmem_free((caddr_t)qep, sizeof (*qep));

	return (DDI_FAILURE);
}

static int
qedetach(dip, cmd)
dev_info_t	*dip;
ddi_detach_cmd_t	cmd;
{
	struct qe 		*qep, *qetmp, **prevqep;
	volatile struct mace 		*macep;
	volatile struct qecm_chan 	*chanp;
	int 			chan;
	struct qec_soft	*qsp;

	qep = (struct qe *)ddi_get_driver_private(dip);

	/* Handle the DDI_SUSPEND command */
	if (cmd == DDI_SUSPEND) {
		if (!qep)			/* No resources allocated */
			return (DDI_FAILURE);
		qep->qe_flags |= QESUSPENDED;
		qe_stop_timer(qep);
		qeuninit(qep);
		return (DDI_SUCCESS);
	}

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	ASSERT(qeinfoack.dl_provider_style == DL_STYLE2);

	if (!qep) {			/* No resources allocated */
		return (DDI_SUCCESS);
	}

	if (qep->qe_flags & (QERUNNING | QESUSPENDED)) {
		cmn_err(CE_NOTE, "%s is BUSY(0x%x)", ddi_get_name(dip),
			qep->qe_flags);
		return (DDI_FAILURE);
	}

	macep = qep->qe_maceregp;
	chanp = qep->qe_chanregp;

	/*
	 * XXX Is there a way to make driver quiescent
	 *macep->lance_rap = LANCE_CSR0;
	 *macep->lance_csr = LANCE_STOP;
	 */

	ddi_remove_minor_node(dip, NULL);

	/*
	 * Remove instance of the intr from QEC structure
	 */
	chan = ddi_getprop(DDI_DEV_T_NONE, dip, 0, "channel#", 0);

	qsp = (struct qec_soft *)ddi_get_driver_private(ddi_get_parent(dip));
	qsp->qs_intr_func[chan] = NULL;
	qsp->qs_intr_arg[chan] = NULL;

	/*
	 * Destroy all mutexes and data structures allocated during
	 * attach time.
	 */
	if (macep)
		(void) ddi_unmap_regs(dip, 0, (caddr_t *)&macep, 0, 0);
	if (chanp)
		(void) ddi_unmap_regs(dip, 0, (caddr_t *)&chanp, 0, 0);

	/*
	 * Remove qep from the link list of device structures
	 */

	for (prevqep = &qeup; (qetmp = *prevqep) != NULL;
		prevqep = &qetmp->qe_nextp)
		if (qetmp == qep) {
			if (qetmp->qe_ksp)
				kstat_delete(qetmp->qe_ksp);
			*prevqep = qetmp->qe_nextp;
			qe_stop_timer(qetmp);
			mutex_destroy(&qetmp->qe_xmitlock);
			mutex_destroy(&qetmp->qe_intrlock);
			if (qetmp->qe_iopbhandle)
				(void) ddi_dma_free(qetmp->qe_iopbhandle);
			if (qetmp->qe_iopbkbase)
				ddi_iopb_free((caddr_t)qetmp->qe_iopbkbase);

			qefreebufs(qetmp);

			if (qetmp->qe_dvmarh) {
				(void) dvma_release(qetmp->qe_dvmarh);
				(void) dvma_release(qetmp->qe_dvmaxh);
				qetmp->qe_dvmarh = qetmp->qe_dvmaxh = NULL;
			}
			if (qetmp->qe_dmarh) {
				kmem_free((caddr_t)qetmp->qe_dmaxh,
					(QEC_QMDMAX + QERPENDING) *
					(sizeof (ddi_dma_handle_t)));
				qetmp->qe_dmarh = qetmp->qe_dmaxh = NULL;
			}

			ddi_set_driver_private(dip, NULL);
			kmem_free((caddr_t)qetmp, sizeof (struct qe));
			break;
		}
	return (DDI_SUCCESS);
}
/*
 * Return 0 upon success, 1 on failure.
 */
static int
qechanstop(qep)
struct	qe	*qep;
{
	volatile	uchar_t *biuccp = &(qep->qe_maceregp->biucc);
	volatile	uint_t *controlp = &(qep->qe_chanregp->control);

	*biuccp = MACE_BIU_SWSRT;
	QECDELAY((*biuccp & MACE_BIU_SWSRT) == 0, QECMAXRSTDELAY);
	if (*biuccp & MACE_BIU_SWSRT) {
		qerror(qep->qe_dip, "cannot stop mace register");
		return (1);
	}

	(void) qep->qe_maceregp->rcvfifo;	/* bug fix 1222358 */
	*controlp = QECM_CONTROL_RST;
	QECDELAY((*controlp & QECM_CONTROL_RST) == 0, QECMAXRSTDELAY);
	if (*controlp & QECM_CONTROL_RST) {
		qerror(qep->qe_dip, "cannot stop channel register");
		return (1);
	} else
		return (0);
}

static int
qestat_kstat_update(kstat_t *ksp, int rw)
{
	struct qe *qep;
	struct qekstat *qkp;

	qep = (struct qe *)ksp->ks_private;
	qkp = (struct qekstat *)ksp->ks_data;

	if (rw == KSTAT_WRITE) {
		qep->qe_ipackets	= qkp->qk_ipackets.value.ui32;
		qep->qe_ierrors		= qkp->qk_ierrors.value.ui32;
		qep->qe_opackets	= qkp->qk_opackets.value.ui32;
		qep->qe_oerrors		= qkp->qk_oerrors.value.ui32;
		qep->qe_txcoll		= qkp->qk_txcoll.value.ui32;

		/*
		 * MIB II kstat variables
		 */
		qep->qe_rcvbytes  = qkp->qk_rcvbytes.value.ui32;
		qep->qe_xmtbytes  = qkp->qk_xmtbytes.value.ui32;
		qep->qe_multircv  = qkp->qk_multircv.value.ui32;
		qep->qe_multixmt  = qkp->qk_multixmt.value.ui32;
		qep->qe_brdcstrcv = qkp->qk_brdcstrcv.value.ui32;
		qep->qe_brdcstxmt = qkp->qk_brdcstxmt.value.ui32;
		qep->qe_norcvbuf  = qkp->qk_norcvbuf.value.ui32;
		qep->qe_noxmtbuf  = qkp->qk_noxmtbuf.value.ui32;


#ifdef	kstat
		qep->qe_rxcoll		= qkp->qk_rxcoll.value.ui32;
		qep->qe_defer		= qkp->qk_defer.value.ui32;
		qep->qe_fram		= qkp->qk_fram.value.ui32;
		qep->qe_crc		= qkp->qk_crc.value.ui32;
		qep->qe_buff		= qkp->qk_buff.value.ui32;
		qep->qe_ifspeed		= qkp->qk_ifspeed.value.ui32;
		qep->qe_oflo		= qkp->qk_oflo.value.ui32;
		qep->qe_uflo		= qkp->qk_uflo.value.ui32;
		qep->qe_missed		= qkp->qk_missed.value.ui32;
		qep->qe_tlcol		= qkp->qk_tlcol.value.ui32;
		qep->qe_trtry		= qkp->qk_trtry.value.ui32;
		qep->qe_tnocar		= qkp->qk_tnocar.value.ui32;
		qep->qe_inits		= qkp->qk_inits.value.ui32;
		qep->qe_nocanput	= qkp->qk_nocanput.value.ui32;
		qep->qe_allocbfail	= qkp->qk_allocbfail.value.ui32;
		qep->qe_runt		= qkp->qk_runt.value.ui32;
		qep->qe_jab		= qkp->qk_jab.value.ui32;
		qep->qe_babl		= qkp->qk_babl.value.ui32;
		qep->qe_tmder 		= qkp->qk_tmder.value.ui32;
		qep->qe_laterr		= qkp->qk_laterr.value.ui32;
		qep->qe_parerr		= qkp->qk_parerr.value.ui32;
		qep->qe_errack		= qkp->qk_errack.value.ui32;
		qep->qe_notmds		= qkp->qk_notmds.value.ui32;
		qep->qe_notbufs		= qkp->qk_notbufs.value.ui32;
		qep->qe_norbufs		= qkp->qk_norbufs.value.ui32;
		qep->qe_clsn		= qkp->qk_clsn.value.ui32;
#endif	kstat
		return (0);
	} else {
		qkp->qk_ipackets.value.ui32	= qep->qe_ipackets;
		qkp->qk_ierrors.value.ui32	= qep->qe_ierrors;
		qkp->qk_opackets.value.ui32	= qep->qe_opackets;
		qkp->qk_oerrors.value.ui32	= qep->qe_oerrors;
		qkp->qk_txcoll.value.ui32	= qep->qe_txcoll;
		qkp->qk_rxcoll.value.ui32	= qep->qe_rxcoll;
		qkp->qk_defer.value.ui32	= qep->qe_defer;
		qkp->qk_fram.value.ui32		= qep->qe_fram;
		qkp->qk_crc.value.ui32		= qep->qe_crc;
		qkp->qk_ifspeed.value.ui32	= qep->qe_ifspeed;
		qkp->qk_buff.value.ui32		= qep->qe_buff;
		qkp->qk_oflo.value.ui32		= qep->qe_oflo;
		qkp->qk_uflo.value.ui32		= qep->qe_uflo;
		qkp->qk_missed.value.ui32	= qep->qe_missed;
		qkp->qk_tlcol.value.ui32	= qep->qe_tlcol;
		qkp->qk_trtry.value.ui32	= qep->qe_trtry;
		qkp->qk_tnocar.value.ui32	= qep->qe_tnocar;
		qkp->qk_inits.value.ui32	= qep->qe_inits;
		qkp->qk_nocanput.value.ui32	= qep->qe_nocanput;
		qkp->qk_allocbfail.value.ui32	= qep->qe_allocbfail;
		qkp->qk_runt.value.ui32		= qep->qe_runt;
		qkp->qk_jab.value.ui32		= qep->qe_jab;
		qkp->qk_babl.value.ui32		= qep->qe_babl;
		qkp->qk_tmder.value.ui32	= qep->qe_tmder;
		qkp->qk_laterr.value.ui32	= qep->qe_laterr;
		qkp->qk_parerr.value.ui32	= qep->qe_parerr;
		qkp->qk_errack.value.ui32	= qep->qe_errack;
		qkp->qk_notmds.value.ui32	= qep->qe_notmds;
		qkp->qk_notbufs.value.ui32	= qep->qe_notbufs;
		qkp->qk_norbufs.value.ui32	= qep->qe_norbufs;
		qkp->qk_clsn.value.ui32		= qep->qe_clsn;

		/*
		 * MIB II kstat variables
		 */
		qkp->qk_rcvbytes.value.ui32   = qep->qe_rcvbytes;
		qkp->qk_xmtbytes.value.ui32   = qep->qe_xmtbytes;
		qkp->qk_multircv.value.ui32   = qep->qe_multircv;
		qkp->qk_multixmt.value.ui32   = qep->qe_multixmt;
		qkp->qk_brdcstrcv.value.ui32  = qep->qe_brdcstrcv;
		qkp->qk_brdcstxmt.value.ui32  = qep->qe_brdcstxmt;
		qkp->qk_norcvbuf.value.ui32   = qep->qe_norcvbuf;
		qkp->qk_noxmtbuf.value.ui32   = qep->qe_noxmtbuf;


	}
	return (0);
}

static void
qestatinit(qep)
struct	qe	*qep;
{
	struct	kstat	*ksp;
	struct	qekstat	*qkp;

#ifdef	kstat
	if ((ksp = kstat_create("qe", ddi_get_instance(qep->qe_dip),
		NULL, "net", KSTAT_TYPE_NAMED,
		sizeof (struct qekstat) / sizeof (kstat_named_t),
		KSTAT_FLAG_PERSISTENT)) == NULL) {
#else
	if ((ksp = kstat_create("qe", ddi_get_instance(qep->qe_dip),
	    NULL, "net", KSTAT_TYPE_NAMED,
	    sizeof (struct qekstat) / sizeof (kstat_named_t), 0)) == NULL) {
#endif	kstat
		qerror(qep->qe_dip, "kstat_create failed");
		return;
	}

	qep->qe_ksp = ksp;
	qkp = (struct qekstat *)(ksp->ks_data);
	kstat_named_init(&qkp->qk_ipackets,		"ipackets",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_ierrors,		"ierrors",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_opackets,		"opackets",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_oerrors,		"oerrors",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_txcoll,		"collisions",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_rxcoll,		"rx_collisions",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_defer,		"excess_defer",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_fram,			"framming",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_crc,			"crc",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_ifspeed,		"ifspeed",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_buff,			"buff",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_oflo,			"oflo",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_uflo,			"uflo",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_missed,		"missed",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_tlcol,		"tx_late_collisions",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_trtry,		"retry_error",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_tnocar,		"nocarrier",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_inits,		"inits",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_nocanput,		"nocanput",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_allocbfail,		"allocbfail",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_runt,			"runt",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_jab,			"jabber",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_babl,			"babble",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_tmder,		"tmd_error",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_laterr,		"late_error",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_parerr,		"parity_error",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_errack,		"error_ack",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_notmds,		"no_tmds",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_notbufs,		"no_tbufs",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_norbufs,		"no_rbufs",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_clsn,			"rx_late_collisions",
		KSTAT_DATA_UINT32);


	/*
	 * MIB II kstat variables
	 */
	kstat_named_init(&qkp->qk_rcvbytes,		"rbytes",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_xmtbytes,		"obytes",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_multircv,		"multircv",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_multixmt,		"multixmt",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_brdcstrcv,		"brdcstrcv",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_brdcstxmt,		"brdcstxmt",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_norcvbuf,		"norcvbuf",
		KSTAT_DATA_UINT32);
	kstat_named_init(&qkp->qk_noxmtbuf,		"noxmtbuf",
		KSTAT_DATA_UINT32);



	ksp->ks_update = qestat_kstat_update;
	ksp->ks_private = (void *)qep;
	kstat_install(ksp);
}

/*
 * Translate "dev_t" to a pointer to the associated "dev_info_t".
 */
/* ARGSUSED */
static int
qeinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t	dev = (dev_t)arg;
	minor_t	instance;
	int	rc;
	struct	qestr	*sqp;

	instance = getminor(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		rw_enter(&qestruplock, RW_READER);
		dip = NULL;
		for (sqp = qestrup; sqp; sqp = sqp->sq_nextp)
			if (sqp->sq_minor == instance)
				break;
		if (sqp && sqp->sq_qep)
			dip = sqp->sq_qep->qe_dip;
		rw_exit(&qestruplock);

		if (dip) {
			*result = (void *)dip;
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
 * Assorted DLPI V2 routines.
 */
/*ARGSUSED*/
static
qeopen(rq, devp, flag, sflag, credp)
queue_t	*rq;
dev_t	*devp;
int	flag;
int	sflag;
cred_t	*credp;
{
	struct	qestr	*sqp;
	struct	qestr	**prevsqp;
	minor_t	minordev;
	int	rc = 0;

	/* XXX Do we need to assert on rq? */

	ASSERT(rq);
	ASSERT(sflag != MODOPEN);
	TRACE_1(TR_FAC_QE, TR_QE_OPEN, "qeopen:  rq %p", rq);

	/*
	 * Serialize all driver open and closes.
	 */
	rw_enter(&qestruplock, RW_WRITER);

	/*
	 * Determine minor device number.
	 */
	prevsqp = &qestrup;
	if (sflag == CLONEOPEN) {
		minordev = 0;
		for (; (sqp = *prevsqp) != NULL; prevsqp = &sqp->sq_nextp) {
			if (minordev < sqp->sq_minor)
				break;
			minordev++;
		}
		*devp = makedevice(getmajor(*devp), minordev);
	} else
		minordev = getminor(*devp);

	if (rq->q_ptr)
		goto done;

	sqp = GETSTRUCT(struct qestr, 1);
	sqp->sq_minor = minordev;
	sqp->sq_rq = rq;
	sqp->sq_state = DL_UNATTACHED;
	sqp->sq_sap = 0;
	sqp->sq_flags = 0;
	sqp->sq_qep = NULL;

	mutex_init(&sqp->sq_lock, NULL, MUTEX_DRIVER, NULL);

	/*
	 * Link new entry into the list of active entries.
	 */
	sqp->sq_nextp = *prevsqp;
	*prevsqp = sqp;

	rq->q_ptr = WR(rq)->q_ptr = sqp;

	/*
	 * Disable automatic enabling of our write service procedure.
	 * We control this explicitly.
	 */
	noenable(WR(rq));

done:
	rw_exit(&qestruplock);
	qprocson(rq);
	return (rc);
}

static
qeclose(rq)
queue_t	*rq;
{
	struct	qestr	*sqp;
	struct	qestr	**prevsqp;

	TRACE_1(TR_FAC_QE, TR_QE_CLOSE, "qeclose:  rq %p", rq);
	ASSERT(rq);
	ASSERT(rq->q_ptr);

	qprocsoff(rq);

	sqp = (struct qestr *)rq->q_ptr;

	/*
	 * Implicit detach Stream from interface.
	 */
	if (sqp->sq_qep)
		qedodetach(sqp);

	rw_enter(&qestruplock, RW_WRITER);

	/*
	 * Unlink the per-Stream entry from the active list and free it.
	 */
	for (prevsqp = &qestrup; (sqp = *prevsqp) != NULL;
		prevsqp = &sqp->sq_nextp)
		if (sqp == (struct qestr *)rq->q_ptr)
			break;
	ASSERT(sqp);
	*prevsqp = sqp->sq_nextp;

	mutex_destroy(&sqp->sq_lock);
	kmem_free((char *)sqp, sizeof (struct qestr));

	rq->q_ptr = WR(rq)->q_ptr = NULL;

	rw_exit(&qestruplock);
	return (0);
}

static
qewput(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp = (struct qestr *)wq->q_ptr;
	struct	qe	*qep;

	TRACE_2(TR_FAC_QE, TR_QE_WPUT_START,
		"qewput start:  wq %p db_type %o", wq, DB_TYPE(mp));
	ASSERT(wq);
	ASSERT(mp);

	switch (DB_TYPE(mp)) {
		case M_DATA:		/* "fastpath" */
			qep = sqp->sq_qep;
			if (((sqp->sq_flags & (QESFAST|QESRAW)) == 0) ||
				(sqp->sq_state != DL_IDLE) ||
				(qep == NULL)) {
				merror(wq, mp, EPROTO);
				break;
			}

			/*
			 * If any msgs already enqueued or the interface will
			 * loop back up the message (due to QEPROMISC), then
			 * enqueue the msg.  Otherwise just xmit it directly.
			 */
			if (wq->q_first) {
				(void) putq(wq, mp);
				qep->qe_wantw = 1;
				qenable(wq);
			} else if (qep->qe_flags & QEPROMISC) {
				(void) putq(wq, mp);
				qenable(wq);
			} else
				(void) qestart(wq, mp, qep);

			break;

		case M_PROTO:
		case M_PCPROTO:
			/*
			 * Break the association between the current thread and
			 * the thread that calls qeproto() to resolve the
			 * problem of qeintr() threads which loop back around
			 * to call qeproto and try to recursively acquire
			 * internal locks.
			 */
			(void) putq(wq, mp);
			qenable(wq);
			break;

		case M_IOCTL:
			qeioctl(wq, mp);
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
	TRACE_1(TR_FAC_QE, TR_QE_WPUT_END, "qewput end:  wq %p", wq);
	return (0);
}

/*
 * Enqueue M_PROTO/M_PCPROTO (always) and M_DATA (sometimes) on the wq.
 *
 * Processing of some of the M_PROTO/M_PCPROTO msgs involves acquiring
 * internal locks that are held across upstream putnext calls.
 * Specifically there's the problem of qeintr() holding qe_intrlock
 * and qestruplock when it calls putnext() and that thread looping
 * back around to call qewput and, eventually, qeinit() to create a
 * recursive lock panic.  There are two obvious ways of solving this
 * problem: (1) have qeintr() do putq instead of putnext which provides
 * the loopback "cutout" right at the rq, or (2) allow qeintr() to putnext
 * and put the loopback "cutout" around qeproto().  We choose the latter
 * for performance reasons.
 *
 * M_DATA messages are enqueued on the wq *only* when the xmit side
 * is out of tbufs or tmds.  Once the xmit resource is available again,
 * wsrv() is enabled and tries to xmit all the messages on the wq.
 */
static
qewsrv(wq)
queue_t	*wq;
{
	mblk_t	*mp;
	struct	qestr	*sqp;
	struct	qe	*qep;

	TRACE_1(TR_FAC_QE, TR_QE_WSRV_START, "qewsrv start:  wq %p", wq);

	sqp = (struct qestr *)wq->q_ptr;
	qep = sqp->sq_qep;

	while (mp = getq(wq))
		switch (DB_TYPE(mp)) {
			case	M_DATA:
				if (qep) {
					if (qestart(wq, mp, qep))
						goto done;
				} else
					freemsg(mp);
				break;

			case	M_PROTO:
			case	M_PCPROTO:
				qeproto(wq, mp);
				break;

			default:
				ASSERT(0);
				break;
		}

done:
	TRACE_1(TR_FAC_QE, TR_QE_WSRV_END, "qewsrv end:  wq %p", wq);
	return (0);
}

static void
qeproto(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	union	DL_primitives	*dlp;
	struct	qestr	*sqp;
	t_uscalar_t	prim;

	sqp = (struct qestr *)wq->q_ptr;
	dlp = (union DL_primitives *)mp->b_rptr;
	prim = dlp->dl_primitive;

	TRACE_2(TR_FAC_QE, TR_QE_PROTO_START,
		"qeproto start:  wq %p dlprim %X", wq, prim);

	mutex_enter(&sqp->sq_lock);

	switch (prim) {
		case	DL_UNITDATA_REQ:
			qeudreq(wq, mp);
			break;

		case	DL_ATTACH_REQ:
			qeareq(wq, mp);
			break;

		case	DL_DETACH_REQ:
			qedreq(wq, mp);
			break;

		case	DL_BIND_REQ:
			qebreq(wq, mp);
			break;

		case	DL_UNBIND_REQ:
			qeubreq(wq, mp);
			break;

		case	DL_INFO_REQ:
			qeireq(wq, mp);
			break;

		case	DL_PROMISCON_REQ:
			qeponreq(wq, mp);
			break;

		case	DL_PROMISCOFF_REQ:
			qepoffreq(wq, mp);
			break;

		case	DL_ENABMULTI_REQ:
			qeemreq(wq, mp);
			break;

		case	DL_DISABMULTI_REQ:
			qedmreq(wq, mp);
			break;

		case	DL_PHYS_ADDR_REQ:
			qepareq(wq, mp);
			break;

		case	DL_SET_PHYS_ADDR_REQ:
			qespareq(wq, mp);
			break;

		default:
			dlerrorack(wq, mp, prim, DL_UNSUPPORTED, 0);
			break;
	}

	TRACE_2(TR_FAC_QE, TR_QE_PROTO_END,
		"qeproto end:  wq %p dlprim %X", wq, prim);

	mutex_exit(&sqp->sq_lock);
}

static void
qeioctl(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	iocblk	*iocp = (struct iocblk *)mp->b_rptr;
	struct	qestr	*sqp = (struct qestr *)wq->q_ptr;

	switch (iocp->ioc_cmd) {
	case DLIOCRAW:		/* raw M_DATA mode */
		sqp->sq_flags |= QESRAW;
		miocack(wq, mp, 0, 0);
		break;

/* XXX Remove this line in mars */
#define	DL_IOC_HDR_INFO	(DLIOC|10)	/* XXX reserved */

	case DL_IOC_HDR_INFO:	/* M_DATA "fastpath" info request */
		qe_dl_ioc_hdr_info(wq, mp);
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
qe_dl_ioc_hdr_info(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	mblk_t	*nmp;
	struct	qestr	*sqp;
	struct	qedladdr	*dlap;
	dl_unitdata_req_t	*dludp;
	struct	ether_header	*headerp;
	struct	qe	*qep;
	t_uscalar_t	off, len;
	int	minsize;

	sqp = (struct qestr *)wq->q_ptr;
	minsize = sizeof (dl_unitdata_req_t) + QEADDRL;

	/*
	 * Sanity check the request.
	 */
	if ((mp->b_cont == NULL) ||
		(MBLKL(mp->b_cont) < minsize) ||
		(*((t_uscalar_t *)mp->b_cont->b_rptr) != DL_UNITDATA_REQ) ||
		((qep = sqp->sq_qep) == NULL)) {
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
	if (!MBLKIN(mp->b_cont, off, len) || (len != QEADDRL)) {
		miocnak(wq, mp, 0, EINVAL);
		return;
	}

	dlap = (struct qedladdr *)(mp->b_cont->b_rptr + off);

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
	ether_copy(&qep->qe_ouraddr, &headerp->ether_shost);
	headerp->ether_type = dlap->dl_sap;

	/*
	 * Link new mblk in after the "request" mblks.
	 */
	linkb(mp, nmp);

	sqp->sq_flags |= QESFAST;

	/*
	 * XXX Don't bother calling qesetipq() here.
	 */

	miocack(wq, mp, msgsize(mp->b_cont), 0);
}

static void
qeareq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	union	DL_primitives	*dlp;
	struct	qe	*qep;
	t_uscalar_t	ppa;

	sqp = (struct qestr *)wq->q_ptr;
	dlp = (union DL_primitives *)mp->b_rptr;

	if (MBLKL(mp) < DL_ATTACH_REQ_SIZE) {
		dlerrorack(wq, mp, DL_ATTACH_REQ, DL_BADPRIM, 0);
		return;
	}

	if (sqp->sq_state != DL_UNATTACHED) {
		dlerrorack(wq, mp, DL_ATTACH_REQ, DL_OUTSTATE, 0);
		return;
	}

	ppa = dlp->attach_req.dl_ppa;

	/*
	 * Valid ppa?
	 */
	mutex_enter(&qelock);
	for (qep = qeup; qep; qep = qep->qe_nextp)
		if (ppa == ddi_get_instance(qep->qe_dip))
			break;
	mutex_exit(&qelock);

	if (qep == NULL) {
		dlerrorack(wq, mp, dlp->dl_primitive, DL_BADPPA, 0);
		return;
	}

	/*
	 * Set link to device and update our state.
	 */
	sqp->sq_qep = qep;
	sqp->sq_state = DL_UNBOUND;

	/*
	 * Has device been initialized?  Do so if necessary.
	 * Also check if promiscous mode is set via the ALLPHYS and
	 * ALLMULTI flags, for the stream.  If so initialize the
	 * interface.
	 */

	if (((qep->qe_flags & QERUNNING) == 0) ||
		((qep->qe_flags & QERUNNING) &&
		(sqp->sq_flags & (QESALLPHYS | QESALLMULTI)))) {
			if (qeinit(qep)) {
				dlerrorack(wq, mp, dlp->dl_primitive,
						DL_INITFAILED, 0);
				sqp->sq_qep = NULL;
				sqp->sq_state = DL_UNATTACHED;
				return;
			}
	}


	dlokack(wq, mp, DL_ATTACH_REQ);
}

static void
qedreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_DETACH_REQ_SIZE) {
		dlerrorack(wq, mp, DL_DETACH_REQ, DL_BADPRIM, 0);
		return;
	}

	if (sqp->sq_state != DL_UNBOUND) {
		dlerrorack(wq, mp, DL_DETACH_REQ, DL_OUTSTATE, 0);
		return;
	}

	qedodetach(sqp);
	dlokack(wq, mp, DL_DETACH_REQ);
}

/*
 * Detach a Stream from an interface.
 */
static void
qedodetach(sqp)
struct	qestr	*sqp;
{
	struct	qestr	*tsqp;
	struct	qe	*qep;
	int	i, reinit = 0;

	ASSERT(sqp->sq_qep);

	qep = sqp->sq_qep;
	sqp->sq_qep = NULL;

	/*
	 * Disable promiscuous mode if on.
	 */
	if (sqp->sq_flags & QESALLPHYS) {
		sqp->sq_flags &= ~QESALLPHYS;
		reinit = 1;
	}

	/*
	 * Disable ALLSAP mode if on.
	 */
	if (sqp->sq_flags & QESALLSAP) {
		sqp->sq_flags &= ~QESALLSAP;
	}

	/*
	 * Disable ALLMULTI mode if on.
	 */
	if (sqp->sq_flags & QESALLMULTI) {
		sqp->sq_flags &= ~QESALLMULTI;
		reinit = 1;
	}

	/*
	 * Disable any Multicast Addresses.
	 */

	for (i = 0; i < NMCHASH; i++) {
		if (sqp->sq_mctab[i]) {
			reinit = 1;
			kmem_free(sqp->sq_mctab[i], sqp->sq_mcsize[i] *
			    sizeof (struct ether_addr));
			sqp->sq_mctab[i] = NULL;
		}
		sqp->sq_mccount[i] = sqp->sq_mcsize[i] = 0;
	}

	for (i = 0; i < 4; i++)
		sqp->sq_ladrf[i] = 0;

	for (i = 0; i < 64; i++)
		sqp->sq_ladrf_refcnt[i] = 0;

	/*
	 * Detach from device structure.
	 * Uninit the device when no other streams are attached to it.
	 */
	for (tsqp = qestrup; tsqp; tsqp = tsqp->sq_nextp)
		if (tsqp->sq_qep == qep)
			break;

	if (tsqp == NULL)
		qeuninit(qep);
	else if (reinit)
		(void) qeinit(qep);

	sqp->sq_state = DL_UNATTACHED;

	qesetipq(qep);
}

static void
qebreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	union	DL_primitives	*dlp;
	struct	qe	*qep;
	struct	qedladdr	qeaddr;
	t_uscalar_t	sap;
	t_uscalar_t	xidtest;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_BIND_REQ_SIZE) {
		dlerrorack(wq, mp, DL_BIND_REQ, DL_BADPRIM, 0);
		return;
	}

	if (sqp->sq_state != DL_UNBOUND) {
		dlerrorack(wq, mp, DL_BIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	dlp = (union DL_primitives *)mp->b_rptr;
	qep = sqp->sq_qep;
	sap = dlp->bind_req.dl_sap;
	xidtest = dlp->bind_req.dl_xidtest_flg;

	ASSERT(qep);

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
	sqp->sq_sap = sap;
	sqp->sq_state = DL_IDLE;

	qeaddr.dl_sap = sap;
	ether_copy(&qep->qe_ouraddr, &qeaddr.dl_phys);
	dlbindack(wq, mp, sap, &qeaddr, QEADDRL, 0, 0);

	qesetipq(qep);
}

static void
qeubreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_UNBIND_REQ_SIZE) {
		dlerrorack(wq, mp, DL_UNBIND_REQ, DL_BADPRIM, 0);
		return;
	}

	if (sqp->sq_state != DL_IDLE) {
		dlerrorack(wq, mp, DL_UNBIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	sqp->sq_state = DL_UNBOUND;
	/* XXX Should we set the sap value to 0 here? */
	sqp->sq_sap = 0;

	dlokack(wq, mp, DL_UNBIND_REQ);

	qesetipq(sqp->sq_qep);
}

static void
qeireq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	dl_info_ack_t	*dlip;
	struct	qedladdr	*dlap;
	struct	ether_addr	*ep;
	int	size;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_INFO_REQ_SIZE) {
		dlerrorack(wq, mp, DL_INFO_REQ, DL_BADPRIM, 0);
		return;
	}

	/*
	 * Exchange current msg for a DL_INFO_ACK.
	 */
	size = sizeof (dl_info_ack_t) + QEADDRL + ETHERADDRL;
	if ((mp = mexchange(wq, mp, size, M_PCPROTO, DL_INFO_ACK)) == NULL)
		return;

	/*
	 * Fill in the DL_INFO_ACK fields and reply.
	 */
	dlip = (dl_info_ack_t *)mp->b_rptr;
	*dlip = qeinfoack;
	dlip->dl_current_state = sqp->sq_state;
	dlap = (struct qedladdr *)(mp->b_rptr + dlip->dl_addr_offset);
	dlap->dl_sap = sqp->sq_sap;
	if (sqp->sq_qep) {
		ether_copy(&sqp->sq_qep->qe_ouraddr, &dlap->dl_phys);
	} else {
		bzero((caddr_t)&dlap->dl_phys, ETHERADDRL);
	}
	ep = (struct ether_addr *)(mp->b_rptr + dlip->dl_brdcst_addr_offset);
	ether_copy(&etherbroadcastaddr, ep);

	qreply(wq, mp);
}

static void
qeponreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_PROMISCON_REQ_SIZE) {
		dlerrorack(wq, mp, DL_PROMISCON_REQ, DL_BADPRIM, 0);
		return;
	}

	switch (((dl_promiscon_req_t *)mp->b_rptr)->dl_level) {
		case DL_PROMISC_PHYS:
			sqp->sq_flags |= QESALLPHYS;
			break;

		case DL_PROMISC_SAP:
			sqp->sq_flags |= QESALLSAP;
			break;

		case DL_PROMISC_MULTI:
			sqp->sq_flags |= QESALLMULTI;
			break;

		default:
			dlerrorack(wq, mp, DL_PROMISCON_REQ,
				DL_NOTSUPPORTED, 0);
			return;
	}

	if (sqp->sq_qep)
		(void) qeinit(sqp->sq_qep);

	if (sqp->sq_qep)
		qesetipq(sqp->sq_qep);

	dlokack(wq, mp, DL_PROMISCON_REQ);
}

static void
qepoffreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	int	flag;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_PROMISCOFF_REQ_SIZE) {
		dlerrorack(wq, mp, DL_PROMISCOFF_REQ, DL_BADPRIM, 0);
		return;
	}

	switch (((dl_promiscoff_req_t *)mp->b_rptr)->dl_level) {
		case DL_PROMISC_PHYS:
			flag = QESALLPHYS;
			break;

		case DL_PROMISC_SAP:
			flag = QESALLSAP;
			break;

		case DL_PROMISC_MULTI:
			flag = QESALLMULTI;
			break;

		default:
			dlerrorack(wq, mp, DL_PROMISCOFF_REQ,
				DL_NOTSUPPORTED, 0);
			return;
	}

	if ((sqp->sq_flags & flag) == 0) {
		dlerrorack(wq, mp, DL_PROMISCOFF_REQ, DL_NOTENAB, 0);
		return;
	}

	sqp->sq_flags &= ~flag;
	if (sqp->sq_qep)
		(void) qeinit(sqp->sq_qep);

	if (sqp->sq_qep)
		qesetipq(sqp->sq_qep);

	dlokack(wq, mp, DL_PROMISCOFF_REQ);
}

static void
qeemreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	union	DL_primitives	*dlp;
	struct	ether_addr	*addrp, *mcbucket;
	t_uscalar_t	off;
	t_uscalar_t	len;
	uint_t	ladrf_bit;
	uint_t	mchash;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_ENABMULTI_REQ_SIZE) {
		dlerrorack(wq, mp, DL_ENABMULTI_REQ, DL_BADPRIM, 0);
		return;
	}

	if (sqp->sq_state == DL_UNATTACHED) {
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
	mcbucket = sqp->sq_mctab[mchash];

	/*
	 * Allocate hash bucket if it's not there.
	 */

	if (mcbucket == NULL) {
		sqp->sq_mctab[mchash] = mcbucket =
		    kmem_alloc(INIT_BUCKET_SIZE *
			    sizeof (struct ether_addr), KM_SLEEP);
		sqp->sq_mcsize[mchash] = INIT_BUCKET_SIZE;
	}

	/*
	 * We no longer bother checking to see if the address is already
	 * in the table (bugid 1209733).  We won't reinitialize the
	 * hardware, since we'll find the mc bit is already set.
	 */

	/*
	 * Expand table if necessary.
	 */
	if (sqp->sq_mccount[mchash] >= sqp->sq_mcsize[mchash]) {
		struct	ether_addr	*newbucket;
		int		newsize;

		newsize = sqp->sq_mcsize[mchash] * 2;

		newbucket = kmem_alloc(newsize * sizeof (struct ether_addr),
			KM_SLEEP);
		bcopy((caddr_t)mcbucket, (caddr_t)newbucket,
		    sqp->sq_mcsize[mchash] * sizeof (struct ether_addr));
		kmem_free(mcbucket, sqp->sq_mcsize[mchash] *
		    sizeof (struct ether_addr));

		sqp->sq_mctab[mchash] = mcbucket = newbucket;
		sqp->sq_mcsize[mchash] = newsize;
	}

	/*
	 * Add address to the table.
	 */
	mcbucket[sqp->sq_mccount[mchash]++] = *addrp;

	/*
	 * If this address's bit was not already set in the local address
	 * filter, add it and re-initialize the LANCE.
	 */
	ladrf_bit = qeladrf_bit(addrp);

	if (sqp->sq_ladrf_refcnt[ladrf_bit] == 0) {
		sqp->sq_ladrf[ladrf_bit >> 4] |= 1 << (ladrf_bit & 0xf);
		(void) qeinit(sqp->sq_qep);
	}
	sqp->sq_ladrf_refcnt[ladrf_bit]++;

	dlokack(wq, mp, DL_ENABMULTI_REQ);
}

static void
qedmreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	union	DL_primitives	*dlp;
	struct	ether_addr	*addrp, *mcbucket;
	t_uscalar_t	off, len;
	int	i;
	uint_t	mchash;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_DISABMULTI_REQ_SIZE) {
		dlerrorack(wq, mp, DL_DISABMULTI_REQ, DL_BADPRIM, 0);
		return;
	}

	if (sqp->sq_state == DL_UNATTACHED) {
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
	mcbucket = sqp->sq_mctab[mchash];

	/*
	 * Try and delete the address if we can find it.
	 */
	if (mcbucket) {
		for (i = 0; i < sqp->sq_mccount[mchash]; i++) {
			if (ether_cmp(addrp, &mcbucket[i]) == 0) {
				uint_t ladrf_bit;

				/*
				 * If there's more than one address in this
				 * bucket, delete the unwanted one by moving
				 * the last one in the list over top of it;
				 * otherwise, just free the bucket.
				 */
				if (sqp->sq_mccount[mchash] > 1) {
					mcbucket[i] =
					    mcbucket[sqp->sq_mccount[mchash]-1];
				} else {
					kmem_free(mcbucket,
					    sqp->sq_mcsize[mchash] *
					    sizeof (struct ether_addr));
					sqp->sq_mctab[mchash] = NULL;
				}
				sqp->sq_mccount[mchash]--;

				/*
				 * If this address's bit should no longer be
				 * set in the local address filter, clear it and
				 * re-initialize the LANCE.
				 */

				ladrf_bit = qeladrf_bit(addrp);
				sqp->sq_ladrf_refcnt[ladrf_bit]--;

				if (sqp->sq_ladrf_refcnt[ladrf_bit] == 0) {
					sqp->sq_ladrf[ladrf_bit >> 4] &=
					    ~(1 << (ladrf_bit & 0xf));
					(void) qeinit(sqp->sq_qep);
				}

				dlokack(wq, mp, DL_DISABMULTI_REQ);
				return;
			}
		}
	}

	dlerrorack(wq, mp, DL_DISABMULTI_REQ, DL_NOTENAB, 0);
}

static void
qepareq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	union	DL_primitives	*dlp;
	uint32_t	type;
	struct	qe	*qep;
	struct	ether_addr	addr;

	sqp = (struct qestr *)wq->q_ptr;

	if (MBLKL(mp) < DL_PHYS_ADDR_REQ_SIZE) {
		dlerrorack(wq, mp, DL_PHYS_ADDR_REQ, DL_BADPRIM, 0);
		return;
	}

	dlp = (union DL_primitives *)mp->b_rptr;
	type = dlp->physaddr_req.dl_addr_type;
	qep = sqp->sq_qep;

	if (qep == NULL) {
		dlerrorack(wq, mp, DL_PHYS_ADDR_REQ, DL_OUTSTATE, 0);
		return;
	}

	switch (type) {
		case	DL_FACT_PHYS_ADDR:
			(void) localetheraddr((struct ether_addr *)NULL, &addr);
			break;

		case	DL_CURR_PHYS_ADDR:
			ether_copy(&qep->qe_ouraddr, &addr);
			break;

		default:
			dlerrorack(wq, mp, DL_PHYS_ADDR_REQ,
				DL_NOTSUPPORTED, 0);
			return;
	}

	dlphysaddrack(wq, mp, &addr, ETHERADDRL);
}

static void
qespareq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	union	DL_primitives	*dlp;
	t_uscalar_t	off;
	t_uscalar_t	len;
	struct	ether_addr	*addrp;
	struct	qe	*qep;

	sqp = (struct qestr *)wq->q_ptr;

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
	if ((qep = sqp->sq_qep) == NULL) {
		dlerrorack(wq, mp, DL_SET_PHYS_ADDR_REQ, DL_OUTSTATE, 0);
		return;
	}

	/*
	 * Set new interface local address and re-init device.
	 * This is destructive to any other streams attached
	 * to this device.
	 */
	ether_copy(addrp, &qep->qe_ouraddr);
	(void) qeinit(sqp->sq_qep);

	dlokack(wq, mp, DL_SET_PHYS_ADDR_REQ);
}

static void
qeudreq(wq, mp)
queue_t	*wq;
mblk_t	*mp;
{
	struct	qestr	*sqp;
	struct	qe	*qep;
	dl_unitdata_req_t	*dludp;
	mblk_t	*nmp;
	struct	qedladdr	*dlap;
	struct	ether_header	*headerp;
	t_uscalar_t	off, len;
	t_uscalar_t	sap;

	sqp = (struct qestr *)wq->q_ptr;
	qep = sqp->sq_qep;

	if (sqp->sq_state != DL_IDLE) {
		dlerrorack(wq, mp, DL_UNITDATA_REQ, DL_OUTSTATE, 0);
		return;
	}

	dludp = (dl_unitdata_req_t *)mp->b_rptr;

	off = dludp->dl_dest_addr_offset;
	len = dludp->dl_dest_addr_length;

	/*
	 * Validate destination address format.
	 */
	if (!MBLKIN(mp, off, len) || (len != QEADDRL)) {
		dluderrorind(wq, mp, mp->b_rptr + off, len, DL_BADADDR, 0);
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

	dlap = (struct qedladdr *)(mp->b_rptr + off);

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
		ether_copy(&qep->qe_ouraddr, &headerp->ether_shost);
		sap = dlap->dl_sap;
		freeb(mp);
		mp = nmp;
	} else {
		DB_TYPE(mp) = M_DATA;
		headerp = (struct ether_header *)mp->b_rptr;
		mp->b_wptr = mp->b_rptr + sizeof (struct ether_header);
		ether_copy(&dlap->dl_phys, &headerp->ether_dhost);
		ether_copy(&qep->qe_ouraddr, &headerp->ether_shost);
		sap = dlap->dl_sap;
	}

	/*
	 * For transmitting, the driver looks at the
	 * sap field of the DL_BIND_REQ being 0 in addition to the type
	 * field in the range [0-1500]. If either is true, then the driver
	 * computes the length of the message, not including initial M_PROTO
	 * mblk (message block), of all subsequent DL_UNITDATA_REQ messages and
	 * transmits 802.3 frames that have this value in the MAC frame header
	 * length field.
	 */
	if ((sap <= ETHERMTU) || (sqp->sq_sap == 0))
		headerp->ether_type =
			(msgsize(mp) - sizeof (struct ether_header));
	else
		headerp->ether_type = sap;

	(void) qestart(wq, mp, qep);
}

/*
 * Start transmission.
 * Return zero on success,
 * otherwise put msg on wq, set 'want' flag and return nonzero.
 */

static
qestart(wq, mp, qep)
queue_t *wq;
mblk_t	*mp;
struct qe	*qep;
{
	volatile	struct	qmd	*tmdp1 = NULL;
	volatile	struct	qmd	*tmdp2 = NULL;
	volatile	struct	qmd	*ntmdp = NULL;
	mblk_t	*nmp = NULL;
	mblk_t  *bp;
	uint_t	len1, len2, flags;
	int	i,  j;
	ddi_dma_cookie_t c;
	struct ether_header *ehp; /* MIB II */

	TRACE_1(TR_FAC_QE, TR_QE_START_START, "qestart start:  wq %p", wq);

	/*
	 * update MIB II statistics
	 */
	ehp = (struct ether_header *)mp->b_rptr;
	BUMP_OutNUcast(qep, ehp);


	flags = qep->qe_flags;

	if ((flags & (QERUNNING|QEPROMISC|QEDMA)) != QERUNNING) {
		if (!(flags & QERUNNING)) {
			(void) putbq(wq, mp);
			return (1);
		}
	}

	if (qep->qe_dvmaxh == NULL)
	    return (qestart_slow(wq, mp, qep));

	if (flags & QEPROMISC)
	    if ((nmp = copymsg(mp)) == NULL)
	{
		qep->qe_allocbfail++;
		qep->qe_noxmtbuf++; /* MIB-II update */
	}

	mutex_enter(&qep->qe_xmitlock);

	if (qep->qe_tnextp > qep->qe_tcurp) {
		if ((qep->qe_tnextp - qep->qe_tcurp) > QETPENDING) {
			qereclaim(qep);
		}
	} else {
		if ((qep->qe_tcurp - qep->qe_tnextp) < QETPENDING) {
			qereclaim(qep);
		}
	}

	tmdp1 = qep->qe_tnextp;
	if ((ntmdp = NEXTTMD(qep, tmdp1)) == qep->qe_tcurp)
		goto notmds;

	i = tmdp1 - qep->qe_tmdp;

	/*
	 * here we deal with 3 cases.
	 * 	1. pkt has exactly one mblk
	 * 	2. pkt has exactly two mblks
	 * 	3. pkt has more than 2 mblks. Since this almost
	 *	   always never happens, we copy all of them into
	 *	   a msh with one mblk.
	 * for each mblk in the message, we allocate a tmd and
	 * figure out the tmd index. This index also passed to
	 * dvma_kaddr_load(), which establishes the IO mapping
	 * for the mblk data. This index is used as a index into
	 * the ptes reserved by dvma_reserve
	 */

	bp = mp->b_cont;
	len1 = mp->b_wptr - mp->b_rptr;
	qep->qe_opackets++;
	if (bp == NULL) {

		(void) dvma_kaddr_load(qep->qe_dvmaxh, (caddr_t)mp->b_rptr,
		    len1, 2 * i, &c);
		(void) dvma_sync(qep->qe_dvmaxh, 2 * i,
		    DDI_DMA_SYNC_FORDEV);
		tmdp1->qmd_addr = c.dmac_address;
		tmdp1->qmd_flags = QMD_SOP | QMD_EOP | QMD_OWN | len1;
		QESYNCIOPB(qep, tmdp1, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);
		qep->qe_tmblkp[i] = mp;
	} else if ((bp->b_cont == NULL) &&
			((len2 = bp->b_wptr - bp->b_rptr) >= 4)) {
		tmdp2 = ntmdp;
		if ((ntmdp = NEXTTMD(qep, tmdp2)) == qep->qe_tcurp)
			goto notmds;
		j = tmdp2 - qep->qe_tmdp;
		mp->b_cont = NULL;
		qep->qe_tmblkp[i] = mp;
		qep->qe_tmblkp[j] = bp;
		(void) dvma_kaddr_load(qep->qe_dvmaxh, (caddr_t)mp->b_rptr,
		    len1, 2 * i, &c);

		(void) dvma_sync(qep->qe_dvmaxh, 2 * i,
		    DDI_DMA_SYNC_FORDEV);
		tmdp1->qmd_addr = c.dmac_address;
		(void) dvma_kaddr_load(qep->qe_dvmaxh, (caddr_t)bp->b_rptr,
		    len2, 2 * j, &c);
		(void) dvma_sync(qep->qe_dvmaxh, 2 * j,
		    DDI_DMA_SYNC_FORDEV);
		tmdp2->qmd_addr = c.dmac_address;
		tmdp2->qmd_flags = QMD_EOP | QMD_OWN | len2;
		QESYNCIOPB(qep, tmdp2, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);
		tmdp1->qmd_flags = QMD_SOP | QMD_OWN | len1;
		QESYNCIOPB(qep, tmdp1, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);
	} else {
		len1 = msgsize(mp);

		if ((bp = allocb(len1 + 3 * QEBURSTSIZE, BPRI_HI)) == NULL) {
			qep->qe_allocbfail++;
			qep->qe_noxmtbuf++; /* MIB-II update */
			goto bad;
		}

		(void) mcopymsg(mp, (uchar_t *)bp->b_rptr);
		mp = bp;
		qep->qe_tmblkp[i] = mp;

		(void) dvma_kaddr_load(qep->qe_dvmaxh, (caddr_t)mp->b_rptr,
		    len1, 2 * i, &c);
		(void) dvma_sync(qep->qe_dvmaxh, 2 * i,
		    DDI_DMA_SYNC_FORDEV);
		tmdp1->qmd_addr = c.dmac_address;
		tmdp1->qmd_flags = QMD_SOP | QMD_EOP | QMD_OWN | len1;
		QESYNCIOPB(qep, tmdp1, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);
	}

	qep->qe_tnextp = ntmdp;
	qep->qe_chanregp->control = QECM_CONTROL_TDMD;
	qep->qe_once = 1;

	mutex_exit(&qep->qe_xmitlock);
	TRACE_1(TR_FAC_QE, TR_QE_START_END, "qestart end:  wq %p", wq);

	if ((flags & QEPROMISC) && nmp)
		qesendup(qep, nmp, qepaccept);

	return (0);

bad:
	mutex_exit(&qep->qe_xmitlock);
	if (nmp)
		freemsg(nmp);
	freemsg(mp);
	return (1);

notmds:
	qep->qe_once = 1;
	qep->qe_notmds++;
	qep->qe_wantw = 1;
	qep->qe_tnextp = tmdp1;
	qereclaim(qep);
done:
	mutex_exit(&qep->qe_xmitlock);
	if (nmp)
		freemsg(nmp);
	(void) putbq(wq, mp);
	qenable(wq);

	TRACE_1(TR_FAC_QE, TR_QE_START_END, "qestart end:  wq %p", wq);
	return (1);

}

static
qestart_slow(wq, mp, qep)
queue_t *wq;
mblk_t	*mp;
struct qe	*qep;
{
	volatile	struct	qmd	*tmdp1 = NULL;
	volatile	struct	qmd	*ntmdp = NULL;
	mblk_t	*nmp = NULL;
	mblk_t  *bp;
	uint32_t	len1;
	uint32_t	flags;
	ulong_t i;
	ddi_dma_cookie_t c;


	TRACE_1(TR_FAC_QE, TR_QE_START_START, "qestart start:  wq %p", wq);

	flags = qep->qe_flags;

	if (flags & QEPROMISC)
		if ((nmp = copymsg(mp)) == NULL)
		{
			qep->qe_allocbfail++;
			qep->qe_noxmtbuf++; /* MIB-II update */
		}


	mutex_enter(&qep->qe_xmitlock);

	if (qep->qe_tnextp > qep->qe_tcurp) {
		if ((qep->qe_tnextp - qep->qe_tcurp) > QETPENDING)
			qereclaim(qep);
	} else {
		if ((qep->qe_tcurp - qep->qe_tnextp) < QETPENDING)
			qereclaim(qep);
	}

	tmdp1 = qep->qe_tnextp;
	if ((ntmdp = NEXTTMD(qep, tmdp1)) == qep->qe_tcurp)
		goto notmds;

	i = tmdp1 - qep->qe_tmdp;
	qep->qe_opackets++;
	if (mp->b_cont == NULL) {
		len1 = mp->b_wptr - mp->b_rptr;

		if (ddi_dma_addr_setup(qep->qe_dip, (struct as *)0,
			(caddr_t)mp->b_rptr, len1, DDI_DMA_RDWR,
			DDI_DMA_DONTWAIT, 0, &qe_dma_limits,
			&qep->qe_dmaxh[i])) {
			qerror(qep->qe_dip, "ddi_dma_addr_setup failed");
			goto done;
		} else {
			if (ddi_dma_htoc(qep->qe_dmaxh[i],
				0, &c))
				panic("qe:  ddi_dma_htoc buf failed");
		}

		(void) ddi_dma_sync(qep->qe_dmaxh[i], (off_t)0, len1,
		    DDI_DMA_SYNC_FORDEV);
		tmdp1->qmd_addr = c.dmac_address;
		tmdp1->qmd_flags = QMD_SOP | QMD_EOP | QMD_OWN | len1;
		QESYNCIOPB(qep, tmdp1, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);
		qep->qe_tmblkp[i] = mp;
	} else {
		len1 = msgsize(mp);
		if ((bp = allocb(len1 + 3 * QEBURSTSIZE, BPRI_HI)) == NULL) {
			qep->qe_allocbfail++;
			goto bad;
		}

		(void) mcopymsg(mp, (uchar_t *)bp->b_rptr);
		mp = bp;
		qep->qe_tmblkp[i] = mp;

		if (ddi_dma_addr_setup(qep->qe_dip, (struct as *)0,
			(caddr_t)mp->b_rptr, len1, DDI_DMA_RDWR,
			DDI_DMA_DONTWAIT, 0, &qe_dma_limits,
			&qep->qe_dmaxh[i])) {
			qerror(qep->qe_dip, "ddi_dma_addr_setup failed");
			goto done;
		} else {
			if (ddi_dma_htoc(qep->qe_dmaxh[i],
				0, &c))
				panic("qe:  ddi_dma_htoc buf failed");
		}

		(void) ddi_dma_sync(qep->qe_dmaxh[i], (off_t)0, len1,
		    DDI_DMA_SYNC_FORDEV);
		tmdp1->qmd_addr = c.dmac_address;
		tmdp1->qmd_flags = QMD_SOP | QMD_EOP | QMD_OWN | len1;
		QESYNCIOPB(qep, tmdp1, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);
	}

	qep->qe_tnextp = ntmdp;
	qep->qe_chanregp->control = QECM_CONTROL_TDMD;
	qep->qe_once = 1;

	mutex_exit(&qep->qe_xmitlock);
	TRACE_1(TR_FAC_QE, TR_QE_START_END, "qestart end:  wq %p", wq);

	if ((flags & QEPROMISC) && nmp)
		qesendup(qep, nmp, qepaccept);

	return (0);

bad:
	mutex_exit(&qep->qe_xmitlock);
	if (nmp)
		freemsg(nmp);
	freemsg(mp);
	return (1);

notmds:
	qep->qe_once = 1;
	qep->qe_notmds++;
	qep->qe_wantw = 1;
	qep->qe_tnextp = tmdp1;
	qereclaim(qep);
done:
	mutex_exit(&qep->qe_xmitlock);
	if (nmp)
		freemsg(nmp);
	(void) putbq(wq, mp);
	qenable(wq);

	TRACE_1(TR_FAC_QE, TR_QE_START_END, "qestart end:  wq %p", wq);
	return (1);
}

/*
 * Initialize a channel.
 * Return 0 on success, nonzero on error.
 *
 * NOTE: Due to a hardware bug (1222358) in the SBUS interface for this device
 * accessess to this devices's registers have to be done carefully.
 * The MACE registers are all BYTE R/W, R or W.  The QEC is
 * only WORD R/W, R or W.  These are the rules when accessing MACE/QEC
 * registers:
 *
 *	current sequence ====> workaround sequence
 *
 *	RB->RW  ===>  RB->(WW)->RW
 *	RW->RB  ===>  RW->(WB)->RB
 *	WW->WB  ===>  WW->(RW)->WB
 *	WB->WW  ===>  WB->(RB)->WW
 *	WB->RW  ===>  WB->(RB)->(WW)->RW
 *	WW->RB  ===>  WW->(RW)->(WB)->RB
 *	R?->W?  ===>  SBus guarantees the 2 idle cycles for reads followed
 *			by writes
 *
 *	(RB = read byte, RW = read word, WB = write byte, WW = write word;
 *	 operations in parens are the dummy operations)
 *
 *
 * Any future subroutines that are called from this routine, must be
 * written with this in mind.
 */
static int
qeinit(qep)
struct	qe	*qep;
{
	volatile	struct	qecm_chan	*qcp;
	volatile	struct	mace	*macep;
	struct	qestr	*sqp;
	struct	qec_soft	*qsp;
	mblk_t	*bp;
	ushort_t	ladrf[4];
	dev_info_t	*dip;
	int	i;
	int	linktst;
	ddi_dma_cookie_t c;
	uint_t	qec_sbus_delay = 0;

	TRACE_1(TR_FAC_QE, TR_QE_INIT_START, "qeinit start:  qep %p", qep);

	/* Sleep if the interface is suspended */
	if (qep->qe_flags & QESUSPENDED) {
		(void) ddi_dev_is_needed(qep->qe_dip, 0, 1);
	}

	qcp = qep->qe_chanregp;
	macep = qep->qe_maceregp;
	dip = qep->qe_dip;

	mutex_enter(&qeclock);
	mutex_enter(&qep->qe_intrlock);
	rw_enter(&qestruplock, RW_WRITER);
	mutex_enter(&qep->qe_xmitlock);

	qep->qe_flags = 0;
	qep->qe_wantw = 0;

	qep->qe_inits++;
	qep->qe_once = 0;
	qep->qe_ihang = 0;
	qep->qe_oopackets = qep->qe_opackets;
	qep->qe_iipackets = qep->qe_ipackets;
	qep->qe_ifspeed = QE_SPEED;

	qesavecntrs(qep);

	if (qechanstop(qep))
		goto done;
	/*
	 * Allocate data structures.
	 */
	qeallocthings(qep);
	qefreebufs(qep);


	/*
	 * Reset RMD and TMD 'walking' pointers.
	 */
	qep->qe_rnextp = qep->qe_rmdp;
	qep->qe_rlastp = qep->qe_rmdp + QERPENDING - 1;
	qep->qe_tcurp = qep->qe_tmdp;
	qep->qe_tnextp = qep->qe_tmdp;

	/*
	 * Initialize QEC channel registers.
	 */
	qcp->rxring = (uint_t)QEIOPBIOADDR(qep, qep->qe_rmdp);
	qcp->txring = (uint_t)QEIOPBIOADDR(qep, qep->qe_tmdp);
	qcp->rintm = 0;
	qcp->tintm = 0;

	/*
	 * XXX Significant performence improvements can be achieved by
	 * disabling transmit interrupt. Thus TMD's are reclaimed only
	 * when we run out of them in qestart().
	 */
	qcp->tintm = 1;

	qcp->qecerrm = 0;
	qcp->macerrm = QECM_MACERRM_RVCCOM;
	qsp = (struct qec_soft *)ddi_get_driver_private(ddi_get_parent(dip));
	qsp->qe_intr_flag = 1;		/* fix for race condition on fusion */
	qcp->lmrxwrite = qcp->lmrxread = qep->qe_chan *
						qsp->qs_globregp->memsize;
	qcp->lmtxwrite = qcp->lmtxread = qcp->lmrxread +
						qsp->qs_globregp->rxsize;
	qcp->coll = 0;

	qcp->pifs = qeifs & 0x3f;
	qec_sbus_delay = qcp->status;		/* bug fix 1222358 */

	/*
	 * Determine if promiscuous mode.
	 */
	for (sqp = qestrup; sqp; sqp = sqp->sq_nextp)
		if ((sqp->sq_qep == qep) && (sqp->sq_flags & QESALLPHYS)) {
			qep->qe_flags |= QEPROMISC;
			break;
		}

	/*
	 * Initialize MACE registers.
	 */
	macep->xmtfc = MACE_XMTFC_APADXMT;
	macep->rcvfc = 0;

	/*
	 * QEC spec requires the driver to mask the recv interrupt.
	 */
	macep->imr = MACE_IMR_RCVINTM | MACE_IMR_CERRM;
	macep->biucc = MACE_BIU_BSWP |
			(MACE_BIU_XMTSP64 << MACE_BIU_XMTSPSHIFTL);
	macep->fifocc = MACE_FIFOCC_XMTFW16 | MACE_FIFOCC_XMTFWU |
		MACE_FIFOCC_RCVFW32 | MACE_FIFOCC_RCVFWU;
	macep->plscc = MACE_PLSCC_PORTSELTP;

	/*
	 * Disable the link test if the user requires so.
	 */

	linktst = ddi_getprop(DDI_DEV_T_NONE, dip, 0, "no-tpe-test", -1);

	if (linktst == 1)
		macep->phycc = MACE_PHYCC_DLNKTST;

	/*
	 * Program MACE with local individual ethernet address.
	 * XXX is byte ordering right?
	 */
	macep->iac = MACE_IAC_ADDRCHG | MACE_IAC_PHYADDR;
	macep->padr = qep->qe_ouraddr.ether_addr_octet[0];
	macep->padr = qep->qe_ouraddr.ether_addr_octet[1];
	macep->padr = qep->qe_ouraddr.ether_addr_octet[2];
	macep->padr = qep->qe_ouraddr.ether_addr_octet[3];
	macep->padr = qep->qe_ouraddr.ether_addr_octet[4];
	macep->padr = qep->qe_ouraddr.ether_addr_octet[5];

	/*
	 * Set up multicast address filter by passing all multicast
	 * addresses through a crc generator, and then using the
	 * high order 6 bits as a index into the 64 bit logical
	 * address filter. The high order two bits select the word,
	 * while the rest of the bits select the bit within the word.
	 */

	for (i = 0; i < 4; i++)
		ladrf[i] = 0;

	for (sqp = qestrup; sqp; sqp = sqp->sq_nextp)
		if (sqp->sq_qep == qep) {
			if (sqp->sq_flags & QESALLMULTI) {
				for (i = 0; i < 4; i++) {
					ladrf[i] = 0xffff;
				}
				break;	/* All bits are already on */
			}

			for (i = 0; i < 4; i++)
				ladrf[i] |= sqp->sq_ladrf[i];

		}

	/* XXX byte ordering on this below?? */
	macep->iac = MACE_IAC_ADDRCHG | MACE_IAC_LOGADDR;
	for (i = 0; i < 4; i++)  {
		macep->ladrf = (uchar_t)((ladrf[i]) & 0xff);
		macep->ladrf = (uchar_t)((ladrf[i] >> 8) & 0xff);
	}

	macep->iac = 0;

	/*
	 * Clear all descriptors.
	 */
	bzero((caddr_t)qep->qe_rmdp, QEC_QMDMAX * sizeof (struct qmd));
	bzero((caddr_t)qep->qe_tmdp, QEC_QMDMAX * sizeof (struct qmd));

	/*
	 * Hang out receive buffers.
	 */
	for (i = 0; i < QERPENDING; i++) {
		if ((bp = allocb(QEBUFSIZE, BPRI_LO)) == NULL) {
			qerror(qep->qe_dip, "qeinit allocb failed");
			goto done;
		}
		/* XXX alignment ??? */
		bp->b_rptr = QEROUNDUP2(bp->b_rptr, QEBURSTSIZE);
		if (qep->qe_dvmarh)
			(void) dvma_kaddr_load(qep->qe_dvmarh,
					    (caddr_t)bp->b_rptr,
					    (uint_t)QEBUFSIZE,
					    2 * i, &c);
		else {
			/* slow case */
			if (ddi_dma_addr_setup(qep->qe_dip, (struct as *)0,
				(caddr_t)bp->b_rptr, QEBUFSIZE,
				DDI_DMA_RDWR, DDI_DMA_DONTWAIT, 0,
				&qe_dma_limits,
				&qep->qe_dmarh[i]))

				panic("qe: ddi_dma_addr_setup of bufs failed");
			else {
				if (ddi_dma_htoc
				    (qep->qe_dmarh[i],
				    0, &c))
					panic("qe:  ddi_dma_htoc buf failed");
			}
		}
		qermdinit(&qep->qe_rmdp[i], c.dmac_address);
		qep->qe_rmblkp[i] = bp;	/* save for later use */
	}

	/*
	 * DMA sync descriptors.
	 */
	QESYNCIOPB(qep, qep->qe_rmdp, 2 * QEC_QMDMAX * sizeof (struct qmd),
		DDI_DMA_SYNC_FORDEV);

	/*
	 * After doing a port select of the twisted pair port, the
	 * driver needs to give ample time for the MACE to start
	 * sending pulses to the hub to mark the link state up.
	 * Loop here and check of the link state has gone into a
	 * pass state.
	 */
	if (!(macep->phycc & MACE_PHYCC_DLNKTST)) {
		QECDELAY(((macep->phycc & MACE_PHYCC_LNKST) == 0), QELNKTIME);
		if (macep->phycc & MACE_PHYCC_LNKST)
			qerror(dip, "Link state down");
	}

	/*
	 * Clear the missed packet counter.
	 */
	/* LINTED */
	if (macep->mpc)
		;

	/*
	 * Enable transmit and receive.
	 */
	macep->maccc = (qep->qe_flags & QEPROMISC ? MACE_MACCC_PROM : 0) |
		MACE_MACCC_ENXMT | MACE_MACCC_ENRCV;

	/* the 2 lines below are part of bug fix 1222358 */

	(void) macep->maccc;
	qcp->status = qec_sbus_delay;

	qep->qe_flags |= QERUNNING;

	qewenable(qep);

done:
	qe_start_timer(qep, qedog, SECOND(qe_sanity_timer));
	mutex_exit(&qep->qe_xmitlock);
	rw_exit(&qestruplock);
	mutex_exit(&qep->qe_intrlock);
	mutex_exit(&qeclock);

	return (!(qep->qe_flags & QERUNNING));
}

static void
qefreebufs(struct qe *qep)
{
	int		i;

	/*
	 * Free and dvma_unload pending xmit and recv buffers.
	 * Maintaining the 1-to-1 ordered sequence of
	 * dvma_load() followed by dvma_unload() is critical.
	 * Always unload anything before loading it again.
	 * Never unload anything twice.  Always unload
	 * before freeing the buffer.  We satisfy these
	 * requirements by unloading only those descriptors
	 * which currently have an mblk associated with them.
	 */
	for (i = 0; i < QEC_QMDMAX; i++) {
		if (qep->qe_tmblkp[i]) {
			if (qep->qe_dvmaxh)
				dvma_unload(qep->qe_dvmaxh, 2 * i, DONT_FLUSH);
			freeb(qep->qe_tmblkp[i]);
			qep->qe_tmblkp[i] = NULL;
		}
		if (qep->qe_rmblkp[i]) {
			if (qep->qe_dvmarh)
				dvma_unload(qep->qe_dvmarh, 2 * QERINDEX(i),
					DDI_DMA_SYNC_FORKERNEL);
			freeb(qep->qe_rmblkp[i]);
			qep->qe_rmblkp[i] = NULL;
		}
	}

	if (qep->qe_dmarh) {
		/* slow case */
		for (i = 0; i < QEC_QMDMAX; i++) {
			if (qep->qe_dmaxh[i]) {
				(void) ddi_dma_free(qep->qe_dmaxh[i]);
				qep->qe_dmaxh[i] = NULL;
			}
		}
		for (i = 0; i < QERPENDING; i++) {
			if (qep->qe_dmarh[i]) {
				(void) ddi_dma_free(qep->qe_dmarh[i]);
				qep->qe_dmarh[i] = NULL;
			}
		}
	}
}

/*
 * Un-initialize (STOP) QED channel.
 */

static void
qeuninit(struct qe *qep)
{
	/*
	 * Allow up to 'QEDRAINTIME' for pending xmit's to complete.
	 */
	QECDELAY((qep->qe_tcurp == qep->qe_tnextp), QEDRAINTIME);

	/*
	 * The frame maay be still in the QEC local memory waiting to be
	 * transmitted.
	 */
	drv_usecwait(QEDRAINTIME);

	mutex_enter(&qep->qe_intrlock);
	mutex_enter(&qep->qe_xmitlock);

	qep->qe_flags &= ~QERUNNING;

	(void) qechanstop(qep);

	mutex_exit(&qep->qe_xmitlock);
	mutex_exit(&qep->qe_intrlock);
}

static void
qeallocthings(qep)
struct	qe	*qep;
{
	uintptr_t a;
	int	size;
	ddi_dma_cookie_t	c;

	/*
	 * Return if resources are already allocated.
	 */
	if (qep->qe_rmdp)
		return;

	/*
	 * Allocate the TMD and RMD descriptors and extra for alignments.
	 */
	size = (2 * QEC_QMDMAX * sizeof (struct qmd)) + QEC_QMDALIGN;
	if (ddi_iopb_alloc(qep->qe_dip, &qe_dma_limits,
		(uint_t)size,
		(caddr_t *)&qep->qe_iopbkbase)) {
		panic("qeallocthings:  out of iopb space");
		/*NOTREACHED*/
	}
	a = qep->qe_iopbkbase;
	a = QEROUNDUP(a, QEC_QMDALIGN);
	qep->qe_rmdp = (struct qmd *)a;
	a += QEC_QMDMAX * sizeof (struct qmd);
	qep->qe_tmdp = (struct qmd *)a;

	/*
	 * IO map this and get an "iopb" dma handle.
	 */
	if (ddi_dma_addr_setup(qep->qe_dip, (struct as *)0,
		(caddr_t)qep->qe_iopbkbase, size,
		DDI_DMA_RDWR|DDI_DMA_CONSISTENT,
		DDI_DMA_DONTWAIT, 0, &qe_dma_limits,
		&qep->qe_iopbhandle))
		panic("qe:  ddi_dma_addr_setup iopb failed");

	/*
	 * Initialize iopb io virtual address.
	 */
	if (ddi_dma_htoc(qep->qe_iopbhandle, 0, &c))
		panic("qe:  ddi_dma_htoc iopb failed");
	qep->qe_iopbiobase = c.dmac_address;


	/*
	 * dvma_reserve() reserves DVMA space for private management by a
	 * device driver. Specifically we reserve n (QEC_QMDMAX * 2)
	 * pagetable enteries. Therefore we have 2 ptes for each
	 * descriptor. Since the ethernet buffers are 1518 bytes
	 * so they can at most use 2 ptes. The pte are updated when
	 * we do a dvma_kaddr_load.
	 */
	if (((dvma_reserve(qep->qe_dip, &qe_dma_limits, (QEC_QMDMAX * 2),
		&qep->qe_dvmaxh)) != DDI_SUCCESS) ||
	    (qe_force_dma)) {
		/*
		 * The reserve call has failed. This implies
		 * that we have to fall back to the older interface
		 * which will do a ddi_dma_addr_setup for each bufer
		 */
		qep->qe_dmaxh = (ddi_dma_handle_t *)
		    kmem_zalloc(((QEC_QMDMAX +  QERPENDING) *
			(sizeof (ddi_dma_handle_t))), KM_SLEEP);
		qep->qe_dmarh = qep->qe_dmaxh + QEC_QMDMAX;
		qep->qe_dvmaxh = qep->qe_dvmarh = NULL;
	} else {
		/*
		 * reserve dvma space for the receive side. If this call
		 * fails, we have ro release the resources and fall
		 * back to slow case
		 */
		if ((dvma_reserve(qep->qe_dip, &qe_dma_limits,
			(QERPENDING * 2), &qep->qe_dvmarh)) != DDI_SUCCESS) {
			(void) dvma_release(qep->qe_dvmaxh);

			qep->qe_dmaxh = (ddi_dma_handle_t *)
				kmem_zalloc(((QEC_QMDMAX +  QERPENDING) *
				    (sizeof (ddi_dma_handle_t))), KM_SLEEP);
			qep->qe_dmarh = qep->qe_dmaxh +
				QEC_QMDMAX;
			qep->qe_dvmaxh = qep->qe_dvmarh = NULL;
		}
	}
	/*
	 * Keep handy limit values for RMD, TMD, and Buffers.
	 */
	qep->qe_rmdlimp = &((qep->qe_rmdp)[QEC_QMDMAX]);
	qep->qe_tmdlimp = &((qep->qe_tmdp)[QEC_QMDMAX]);

	/*
	 * Zero out xmit and rcv holders.
	 */

	bzero((caddr_t)qep->qe_tmblkp, sizeof (qep->qe_tmblkp));
	bzero((caddr_t)qep->qe_rmblkp, sizeof (qep->qe_rmblkp));

}

static void
qeintr(qep)
struct	qe	*qep;
{
	volatile	struct	qmd	*rmdp;
	uint_t	bits;

	mutex_enter(&qep->qe_intrlock);

	bits = qep->qe_chanregp->status; /* auto-clears on read */

	if (bits & QECM_STATUS_RINT) {

		rmdp = qep->qe_rnextp;

		/*
		 * Sync RMD before looking at it.
		 */
		QESYNCIOPB(qep, rmdp, sizeof (struct qmd),
			DDI_DMA_SYNC_FORCPU);

		/*
		 * Loop through each RMD.
		 */
		while ((rmdp->qmd_flags & QMD_OWN) == 0) {
			qeread(qep, rmdp);

			/*
			 * Increment to next RMD.
			 */
			qep->qe_rnextp = rmdp = NEXTRMD(qep, rmdp);

			/*
			 * Sync the next RMD before looking at it.
			 */
			QESYNCIOPB(qep, rmdp, sizeof (struct qmd),
				DDI_DMA_SYNC_FORCPU);
		}
	}
	if (bits & QECM_STATUS_OTHER)
		qeother(qep, bits);

	mutex_exit(&qep->qe_intrlock);

	if (bits & QECM_STATUS_TINT) {
		mutex_enter(&qep->qe_xmitlock);
		qereclaim(qep);
		mutex_exit(&qep->qe_xmitlock);
	}
}

/*
 * Transmit completion reclaiming.
 */

static void
qereclaim(qep)
struct	qe	*qep;
{


	volatile struct	qmd	*tmdp;
	int		i;
#ifndef lint
	int			nbytes;
#endif lint

	tmdp = qep->qe_tcurp;


	/*
	 * Sync TMDs before looking at them.
	 * Lint gives error because QESYNCIOPB is a NOP now
	 */
#ifndef lint
	if (qep->qe_tnextp > qep->qe_tcurp) {
		nbytes = ((qep->qe_tnextp - qep->qe_tcurp)
				* sizeof (struct qmd));
		QESYNCIOPB(qep, tmdp, nbytes, DDI_DMA_SYNC_FORCPU);
	} else {
		nbytes = ((qep->qe_tmdlimp - qep->qe_tcurp)
				* sizeof (struct qmd));
		QESYNCIOPB(qep, tmdp, nbytes, DDI_DMA_SYNC_FORCPU);
		nbytes = ((qep->qe_tnextp - qep->qe_tmdp)
				* sizeof (struct qmd));
		QESYNCIOPB(qep, qep->qe_tmdp, nbytes, DDI_DMA_SYNC_FORCPU);
	}
#endif lint
	/*
	 * Loop through each TMD.
	 */
	while ((tmdp->qmd_flags & (QMD_OWN)) == 0 &&
		(tmdp != qep->qe_tnextp)) {


		/*
		 * In qe driver, we don't know, how many bytes were
		 * xferred at this point of time as MACE clears those bits
		 * so the only place where we know about bytes is the start
		 * routine, but that place we don't know if xfer would be
		 * successful or not, but neverthless we have to increase
		 * obytes there only.
		 */



		i = tmdp - qep->qe_tmdp;
		if (qep->qe_dvmaxh == NULL) {
			(void) ddi_dma_free(qep->qe_dmaxh[i]);
			qep->qe_dmaxh[i] = NULL;
		} else
			(void) dvma_unload(qep->qe_dvmaxh, 2 * i,
			    (uint_t)DONT_FLUSH);

		if (qep->qe_tmblkp[i]) {

		/*
		 *  MIB II
		 */

			qep->qe_xmtbytes += qep->qe_tmblkp[i]->b_wptr - \
				qep->qe_tmblkp[i]->b_rptr;

			freeb(qep->qe_tmblkp[i]);
			qep->qe_tmblkp[i] = NULL;
		}
		tmdp = NEXTTMD(qep, tmdp);
	}

	if (tmdp != qep->qe_tcurp) {
		/*
		 * we could recaim some TMDs so turn off interupts
		 */
		qep->qe_tcurp = tmdp;
		if (qep->qe_wantw) {
			qep->qe_chanregp->tintm = 1;
			qewenable(qep);
		}
	}
	/*
	 * enable TINTS: so that even if there is no further activity
	 * qereclaim will get called
	 */
	if (qep->qe_wantw)
		qep->qe_chanregp->tintm = 0;
}

static void
qeread(qep, rmdp)
struct	qe	*qep;
volatile	struct	qmd	*rmdp;
{
	int	rmdi;
	uint_t	dvma_rmdi, dvma_nrmdi;
	mblk_t	*bp, *nbp;
	volatile	struct	qmd	*nrmdp;
	struct		ether_header	*ehp;
	queue_t		*ipq;
	uint32_t	len;
	int	nrmdi;
	ddi_dma_cookie_t c;

	TRACE_0(TR_FAC_QE, TR_QE_READ_START, "qeread start");

	if (qep->qe_dvmaxh == NULL) {
		qeread_slow(qep, rmdp);
		return;
	}

	rmdi = rmdp - qep->qe_rmdp;
	dvma_rmdi = QERINDEX(rmdi);
	bp = qep->qe_rmblkp[rmdi];
	nrmdp = NEXTRMD(qep, qep->qe_rlastp);
	qep->qe_rlastp = nrmdp;
	nrmdi = nrmdp - qep->qe_rmdp;
	dvma_nrmdi = QERINDEX(nrmdi);

	ASSERT(dvma_rmdi == dvma_nrmdi);

	len = rmdp->qmd_flags - ETHERFCSL;

	if (len < ETHERMIN) {
		nrmdp->qmd_addr = rmdp->qmd_addr;
		nrmdp->qmd_flags = QEBUFSIZE | QMD_OWN;
		QESYNCIOPB(qep, nrmdp, sizeof (struct qmd),
			DDI_DMA_SYNC_FORDEV);
		qep->qe_rmblkp[nrmdi] = bp;
		qep->qe_rmblkp[rmdi] = NULL;
		qep->qe_ierrors++;
		qep->qe_ihang++;
		qep->qe_missed++;
		TRACE_0(TR_FAC_QE, TR_QE_READ_END, "qeread end");
		return;
	}

	bp->b_wptr = bp->b_rptr + len;

	dvma_unload(qep->qe_dvmarh, 2 * dvma_rmdi, DDI_DMA_SYNC_FORKERNEL);

	if ((nbp = allocb(QEBUFSIZE + QEBURSTSIZE, BPRI_LO))) {
		nbp->b_rptr = QEROUNDUP2(nbp->b_rptr, QEBURSTSIZE);
		(void) dvma_kaddr_load(qep->qe_dvmarh,
		    (caddr_t)nbp->b_rptr, QEBUFSIZE, 2 * dvma_nrmdi, &c);

		nrmdp->qmd_addr = (uint_t)(c.dmac_address);
		nrmdp->qmd_flags = QEBUFSIZE | QMD_OWN;
		QESYNCIOPB(qep, nrmdp, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);
		qep->qe_rmblkp[nrmdi] = nbp;
		qep->qe_rmblkp[rmdi] = NULL;
		qep->qe_ipackets++;
		qep->qe_ihang = 0;
		QESYNCIOPB(qep, nrmdp, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);

		ehp = (struct ether_header *)bp->b_rptr;

		/*
		 * update MIB II statistics
		 */
		BUMP_InNUcast(qep, ehp);
		qep->qe_rcvbytes += len;


		ipq = qep->qe_ipq;

		if ((ehp->ether_type == ETHERTYPE_IP) &&
		    ((ehp->ether_dhost.ether_addr_octet[0] & 01) == 0) &&
		    (ipq) &&
		    canputnext(ipq)) {
			bp->b_rptr += sizeof (struct ether_header);
			(void) putnext(ipq, bp);
		} else {
			/* Strip the PADs for 802.3 */
			if (ehp->ether_type + sizeof (struct ether_header)
							< ETHERMIN)
				bp->b_wptr = bp->b_rptr
						+ sizeof (struct ether_header)
						+ ehp->ether_type;
			qesendup(qep, bp, qeaccept);
		}
	} else {
		(void) dvma_kaddr_load(qep->qe_dvmarh, (caddr_t)bp->b_rptr,
		    QEBUFSIZE, 2 * dvma_nrmdi, &c);
		nrmdp->qmd_addr = (uint_t)(c.dmac_address);
		qep->qe_rmblkp[nrmdi] = bp;
		qep->qe_rmblkp[rmdi] = NULL;
		nrmdp->qmd_flags = QEBUFSIZE | QMD_OWN;
		QESYNCIOPB(qep, nrmdp, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);

		qep->qe_ierrors++;
		qep->qe_ihang++;
		qep->qe_allocbfail++;
		qep->qe_norcvbuf++; /* MIB-II update */
		if (qedebug)
			qerror(qep->qe_dip, "allocb fail");
	}

	TRACE_0(TR_FAC_QE, TR_QE_READ_END, "qeread end");
}

static void
qeread_slow(qep, rmdp)
struct	qe	*qep;
volatile	struct	qmd	*rmdp;
{
	int	rmdi;
	mblk_t	*bp, *nbp;
	volatile	struct	qmd	*nrmdp;
	struct		ether_header	*ehp;
	queue_t		*ipq;
	uint32_t	len;
	int	nrmdi;
	ddi_dma_cookie_t c;
	uint_t dvma_rmdi, dvma_nrmdi;

	TRACE_0(TR_FAC_QE, TR_QE_READ_START, "qeread start");

	rmdi = rmdp - qep->qe_rmdp;
	bp = qep->qe_rmblkp[rmdi];
	nrmdp = NEXTRMD(qep, qep->qe_rlastp);
	qep->qe_rlastp = nrmdp;
	nrmdi = nrmdp - qep->qe_rmdp;
	len = rmdp->qmd_flags - ETHERFCSL;
	bp->b_wptr = bp->b_rptr + len;
	dvma_rmdi = QERINDEX(rmdi);
	dvma_nrmdi = QERINDEX(nrmdi);

	/*
	 * Sync the received buffer before looking at it.
	 */
	(void) ddi_dma_sync(qep->qe_dmarh[dvma_rmdi], 0, len,
	    DDI_DMA_SYNC_FORCPU);

	/*
	 * Check for short packet
	 */
	if (len < ETHERMIN) {
		nrmdp->qmd_addr = rmdp->qmd_addr;
		nrmdp->qmd_flags = QEBUFSIZE | QMD_OWN;
		qep->qe_rmblkp[nrmdi] = bp;
		qep->qe_rmblkp[rmdi] = NULL;
		QESYNCIOPB(qep, nrmdp, sizeof (struct qmd),
			DDI_DMA_SYNC_FORDEV);
		qep->qe_ierrors++;
		qep->qe_ihang++;
		qep->qe_missed++;
		TRACE_0(TR_FAC_QE, TR_QE_READ_END, "qeread end");
		return;
	}

	if ((nbp = allocb(QEBUFSIZE, BPRI_LO))) {
		nbp->b_rptr = QEROUNDUP2(nbp->b_rptr, QEBURSTSIZE);

		/*
		 * tear down the old mapping then setup a new one
		 */
		(void) ddi_dma_free(qep->qe_dmarh[dvma_rmdi]);
		qep->qe_dmarh[dvma_rmdi] = NULL;

		if (ddi_dma_addr_setup(qep->qe_dip, (struct as *)0,
			(caddr_t)nbp->b_rptr, QEBUFSIZE, DDI_DMA_RDWR,
			DDI_DMA_DONTWAIT, 0, &qe_dma_limits,
			&qep->qe_dmarh[dvma_nrmdi]))

				panic("qe: ddi_dma_addr_setup of bufs failed");
		else {
			if (ddi_dma_htoc(qep->qe_dmarh[dvma_nrmdi], 0, &c))

				panic("qe:  ddi_dma_htoc buf failed");
		}
		nrmdp->qmd_addr = (uint_t)(c.dmac_address);
		nrmdp->qmd_flags = QEBUFSIZE | QMD_OWN;
		qep->qe_rmblkp[nrmdi] = nbp;
		qep->qe_rmblkp[rmdi] = NULL;
		qep->qe_ipackets++;
		qep->qe_ihang = 0;
		QESYNCIOPB(qep, nrmdp, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);

		ehp = (struct ether_header *)bp->b_rptr;

		/*
		 * update MIB II statistics
		 */
		BUMP_InNUcast(qep, ehp);
		qep->qe_rcvbytes += len;


		ipq = qep->qe_ipq;

		if ((ehp->ether_type == ETHERTYPE_IP) &&
		    ((ehp->ether_dhost.ether_addr_octet[0] & 01) == 0) &&
		    (ipq) &&
		    canputnext(ipq)) {
			bp->b_rptr += sizeof (struct ether_header);
			(void) putnext(ipq, bp);
		} else {
			/* Strip the PADs for 802.3 */
			if (ehp->ether_type + sizeof (struct ether_header)
							< ETHERMIN)
				bp->b_wptr = bp->b_rptr
						+ sizeof (struct ether_header)
						+ ehp->ether_type;
			qesendup(qep, bp, qeaccept);
		}
	} else {
		nrmdp->qmd_addr = rmdp->qmd_addr;
		qep->qe_rmblkp[nrmdi] = bp;
		qep->qe_rmblkp[rmdi] = NULL;
		nrmdp->qmd_flags = QEBUFSIZE | QMD_OWN;
		QESYNCIOPB(qep, nrmdp, sizeof (struct qmd),
		    DDI_DMA_SYNC_FORDEV);

		qep->qe_ierrors++;
		qep->qe_ihang++;
		qep->qe_allocbfail++;
		qep->qe_norcvbuf++; /* MIB-II update */
		if (qedebug)
			qerror(qep->qe_dip, "allocb fail");
	}

	TRACE_0(TR_FAC_QE, TR_QE_READ_END, "qeread end");
}

/*
 * Send packet upstream.
 * Assume mp->b_rptr points to ether_header.
 */
static void
qesendup(qep, mp, acceptfunc)
struct	qe	*qep;
mblk_t	*mp;
struct	qestr	*(*acceptfunc)();
{
	t_uscalar_t	type;
	struct	ether_addr	*dhostp, *shostp;
	struct	qestr	*sqp, *nsqp;
	mblk_t	*nmp;
	t_uscalar_t	isgroupaddr;

	TRACE_0(TR_FAC_QE, TR_QE_SENDUP_START, "qesendup start");

	dhostp = &((struct ether_header *)mp->b_rptr)->ether_dhost;
	shostp = &((struct ether_header *)mp->b_rptr)->ether_shost;
	type = ((struct ether_header *)mp->b_rptr)->ether_type;

	isgroupaddr = dhostp->ether_addr_octet[0] & 01;

	/*
	 * While holding a reader lock on the linked list of streams structures,
	 * attempt to match the address criteria for each stream
	 * and pass up the raw M_DATA ("fastpath") or a DL_UNITDATA_IND.
	 */

	rw_enter(&qestruplock, RW_READER);

	if ((sqp = (*acceptfunc)(qestrup, qep, type, dhostp)) == NULL) {
		rw_exit(&qestruplock);
		freemsg(mp);
		TRACE_0(TR_FAC_QE, TR_QE_SENDUP_END, "qesendup end");
		return;
	}

	/*
	 * Loop on matching open streams until (*acceptfunc)() returns NULL.
	 */
	for (; nsqp = (*acceptfunc)(sqp->sq_nextp, qep, type, dhostp);
		sqp = nsqp)
		if (canputnext(sqp->sq_rq))
			if (nmp = dupmsg(mp)) {
				if ((sqp->sq_flags & QESFAST) && !isgroupaddr) {
				nmp->b_rptr += sizeof (struct ether_header);
				(void) putnext(sqp->sq_rq, nmp);
				} else if (sqp->sq_flags & QESRAW)
					(void) putnext(sqp->sq_rq, nmp);
				else if ((nmp = qeaddudind(qep, nmp, shostp,
					dhostp, type, isgroupaddr)))
					(void) putnext(sqp->sq_rq, nmp);
			}
			else
				qep->qe_allocbfail++;
		else
			qep->qe_nocanput++;


	/*
	 * Do the last one.
	 */
	if (canputnext(sqp->sq_rq)) {
		if ((sqp->sq_flags & QESFAST) && !isgroupaddr) {
			mp->b_rptr += sizeof (struct ether_header);
			(void) putnext(sqp->sq_rq, mp);
		} else if (sqp->sq_flags & QESRAW)
			(void) putnext(sqp->sq_rq, mp);
		else if ((mp = qeaddudind(qep, mp, shostp, dhostp,
				type, isgroupaddr)))
			(void) putnext(sqp->sq_rq, mp);
	} else {
		freemsg(mp);
		qep->qe_nocanput++;
		qep->qe_norcvbuf++; /* MIB-II update */
	}

	rw_exit(&qestruplock);
	TRACE_0(TR_FAC_QE, TR_QE_SENDUP_END, "qesendup end");
}

/*
 * Test upstream destination sap and address match.
 */
static struct qestr *
qeaccept(sqp, qep, type, addrp)
struct	qestr	*sqp;
struct	qe	*qep;
int	type;
struct	ether_addr	*addrp;
{
	t_uscalar_t	sap;
	uint32_t	flags;

	for (; sqp; sqp = sqp->sq_nextp) {
		sap = sqp->sq_sap;
		flags = sqp->sq_flags;

		if ((sqp->sq_qep == qep) && QESAPMATCH(sap, type, flags))
			if ((ether_cmp(addrp, &qep->qe_ouraddr) == 0) ||
				(ether_cmp(addrp, &etherbroadcastaddr) == 0) ||
				(flags & QESALLPHYS) ||
				qemcmatch(sqp, addrp))
				return (sqp);
	}

	return (NULL);
}

/*
 * Test upstream destination sap and address match for QESALLPHYS only.
 */
/* ARGSUSED3 */
static struct qestr *
qepaccept(sqp, qep, type, addrp)
struct	qestr	*sqp;
struct	qe	*qep;
int	type;
struct	ether_addr	*addrp;
{
	t_uscalar_t	sap;
	uint32_t	flags;

	for (; sqp; sqp = sqp->sq_nextp) {
		sap = sqp->sq_sap;
		flags = sqp->sq_flags;

		if ((sqp->sq_qep == qep) &&
			QESAPMATCH(sap, type, flags) &&
			(flags & QESALLPHYS))
			return (sqp);
	}

	return (NULL);
}

/*
 * Set or clear the device ipq pointer.
 * XXX Assumes IP is SLFAST.
 */
static void
qesetipq(qep)
struct	qe	*qep;
{
	struct	qestr	*sqp;
	int	ok = 1;
	queue_t	*ipq = NULL;

	rw_enter(&qestruplock, RW_READER);

	for (sqp = qestrup; sqp; sqp = sqp->sq_nextp)
		if (sqp->sq_qep == qep) {
			if (sqp->sq_flags & (QESALLPHYS|QESALLSAP))
				ok = 0;
			if (sqp->sq_sap == ETHERTYPE_IP)
				if (ipq == NULL)
					ipq = sqp->sq_rq;
				else
					ok = 0;
		}

	rw_exit(&qestruplock);

	if (ok)
		qep->qe_ipq = ipq;
	else
		qep->qe_ipq = NULL;
}

/*
 * Prefix msg with a DL_UNITDATA_IND mblk and return the new msg.
 */
static mblk_t *
qeaddudind(qep, mp, shostp, dhostp, type, isgroupaddr)
struct	qe	*qep;
mblk_t	*mp;
struct	ether_addr	*shostp, *dhostp;
t_uscalar_t	type;
t_uscalar_t	isgroupaddr;
{
	dl_unitdata_ind_t	*dludindp;
	struct	qedladdr	*dlap;
	mblk_t	*nmp;
	int	size;

	TRACE_0(TR_FAC_QE, TR_QE_ADDUDIND_START, "qeaddudind start");

	mp->b_rptr += sizeof (struct ether_header);

	/*
	 * Allocate an M_PROTO mblk for the DL_UNITDATA_IND.
	 */
	size = sizeof (dl_unitdata_ind_t) + QEADDRL + QEADDRL;
	nmp = allocb(QEROUNDUP(QEHEADROOM + size, sizeof (double)), BPRI_LO);
	if (nmp == NULL) {
		qep->qe_allocbfail++;
		qep->qe_ierrors++;
		if (qedebug)
			qerror(qep->qe_dip, "allocb failed");
		freemsg(mp);
		TRACE_0(TR_FAC_QE, TR_QE_ADDUDIND_END, "qeaddudind end");
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
	dludindp->dl_dest_addr_length = QEADDRL;
	dludindp->dl_dest_addr_offset = sizeof (dl_unitdata_ind_t);
	dludindp->dl_src_addr_length = QEADDRL;
	dludindp->dl_src_addr_offset = sizeof (dl_unitdata_ind_t) + QEADDRL;
	dludindp->dl_group_address = isgroupaddr;

	dlap = (struct qedladdr *)(nmp->b_rptr + sizeof (dl_unitdata_ind_t));
	ether_copy(dhostp, &dlap->dl_phys);
	dlap->dl_sap = (ushort_t)type;

	dlap = (struct qedladdr *)(nmp->b_rptr + sizeof (dl_unitdata_ind_t) +
	    QEADDRL);
	ether_copy(shostp, &dlap->dl_phys);
	dlap->dl_sap = (ushort_t)type;

	/*
	 * Link the M_PROTO and M_DATA together.
	 */
	nmp->b_cont = mp;
	TRACE_0(TR_FAC_QE, TR_QE_ADDUDIND_END, "qeaddudind end");
	return (nmp);
}

/*
 * Return TRUE if the given multicast address is one
 * of those that this particular Stream is interested in.
 */
static
qemcmatch(sqp, addrp)
struct	qestr	*sqp;
struct	ether_addr	*addrp;
{
	struct	ether_addr	*mcbucket;
	uint32_t	mccount;
	int	i;
	uint_t	mchash;

	/*
	 * Return FALSE if not a multicast address.
	 */
	if (!(addrp->ether_addr_octet[0] & 01))
		return (0);

	/*
	 * Check if all multicasts have been enabled for this Stream
	 */
	if (sqp->sq_flags & QESALLMULTI)
		return (1);

	/*
	 * Compute the hash value for the address and
	 * grab the bucket and the number of entries in the
	 * bucket.
	 */

	mchash = MCHASH(addrp);
	mcbucket = sqp->sq_mctab[mchash];
	mccount = sqp->sq_mccount[mchash];

	/*
	 * Return FALSE if no multicast addresses enabled for this Stream.
	 */

	if (mccount == 0)
	    return (0);

	/*
	 * Otherwise, find it in the Hash Table.
	 */

	if (mcbucket)
		for (i = 0; i < mccount; i++)
			if (!ether_cmp(addrp, &mcbucket[i]))
				return (1);

	return (0);
}

/*
 * Handle interrupts other than TINT and RINT.
 */
static void
qeother(qep, bits)
struct	qe	*qep;
uint_t	bits;
{
	dev_info_t	*dip = qep->qe_dip;
	int	reinit = 0;

	if (bits & QECM_STATUS_EXDER) {
		if (qedebug)
			qerror(dip, "excessive defer error");
		qep->qe_defer++;
		qep->qe_oerrors++;
	}

	if ((bits & QECM_STATUS_LCAR) ||
	((qep->qe_maceregp->phycc & MACE_PHYCC_LNKST) &&
	(!(qep->qe_maceregp->phycc & MACE_PHYCC_DLNKTST)))) {
	/* Ignore the first read ..just in case Bug#4099253 */
		if ((bits & QECM_STATUS_LCAR) ||
		((qep->qe_maceregp->phycc & MACE_PHYCC_LNKST) &&
		(!(qep->qe_maceregp->phycc & MACE_PHYCC_DLNKTST)))) {

		qerror(dip,
	"No carrier - twisted pair cable problem or disabled hub link test?");
		qep->qe_tnocar++;
		qep->qe_oerrors++;
		}
	}

	if (bits & QECM_STATUS_RTRY) {
		if (qedebug)
			qerror(dip, "retry error");
		qep->qe_trtry++;
		qep->qe_oerrors++;
		if (mace_vers == 0)
			reinit++;
	}

	if (bits & QECM_STATUS_LCOL) {
		if (qedebug)
			qerror(dip, "late collision");
		qep->qe_tlcol++;
		qep->qe_oerrors++;
		if (mace_vers == 0)
			reinit++;
	}

	if (bits & QECM_STATUS_UFLO) {
		if (qedebug)
			qerror(dip, "underflow");
		qep->qe_uflo++;
		qep->qe_oerrors++;
		if (mace_vers == 0)
			reinit++;
	}

	if (bits & QECM_STATUS_JAB) {
		if (qedebug)
			qerror(dip, "jabber");
		qep->qe_jab++;
		qep->qe_oerrors++;
	}

	if (bits & QECM_STATUS_BABL) {
		if (qedebug)
			qerror(dip, "babble");
		qep->qe_babl++;
		qep->qe_oerrors++;
	}

	/*
	 * Not an error.
	 */
	if (bits & QECM_STATUS_COLCO)
		qep->qe_txcoll += 256;

	if (bits & QECM_STATUS_TMDER) {
		if (qedebug)
			qerror(dip, "chained packet descriptor error");
		qep->qe_tmder++;
		qep->qe_oerrors++;
		reinit++;
	}

	if (bits & QECM_STATUS_TXLATERR) {
		if (qedebug)
			qerror(dip, "sbus tx late error");
		qep->qe_laterr++;
		qep->qe_oerrors++;
		reinit++;
	}

	if (bits & QECM_STATUS_TXPARERR) {
		if (qedebug)
			qerror(dip, "sbus tx parity error");
		qep->qe_parerr++;
		qep->qe_oerrors++;
		reinit++;
	}

	if (bits & QECM_STATUS_TXERRACK) {
		if (qedebug)
			qerror(dip, "sbus tx error ack");
		qep->qe_errack++;
		qep->qe_oerrors++;
		reinit++;
	}

	/*
	 * Not an error.
	 */
	if (bits & QECM_STATUS_RVCCO)
		qep->qe_rxcoll += 256;

	if (bits & QECM_STATUS_RPCO) {
		qep->qe_runt += 256;
		qep->qe_ierrors += 256;
		qep->qe_ihang++;
	}

	if (bits & QECM_STATUS_MPCO) {
		qep->qe_missed += 256;
		qep->qe_ierrors += 256;
		qep->qe_ihang++;
	}

	if (bits & QECM_STATUS_OFLO) {
		qep->qe_oflo++;
		qep->qe_ierrors++;
		qep->qe_ihang++;
	}

	if (bits & QECM_STATUS_CLSN) {
		if (qedebug)
			qerror(dip, "rx late collision error");
		qep->qe_clsn++;
		qep->qe_ierrors++;
		qep->qe_ihang++;
	}

	if (bits & QECM_STATUS_FMC) {
		if (qedebug)
			qerror(dip, "rx framing error");
		qep->qe_fram++;
		qep->qe_ierrors++;
	}

	if (bits & QECM_STATUS_CRC) {
		if (qedebug)
			qerror(dip, "rx crc error");
		qep->qe_crc++;
		qep->qe_ierrors++;
	}

	if (bits & QECM_STATUS_DROP) {
		if (qedebug)
			qerror(dip, "rx pkt drop error");
		qep->qe_missed++;
		qep->qe_ierrors++;
		qep->qe_ihang++;
	}

	if (bits & QECM_STATUS_BUFF) {
		if (qedebug)
			qerror(dip, "rx pkt buff error");
		qep->qe_buff++;
		qep->qe_ierrors++;
	}

	if (bits & QECM_STATUS_RXLATERR) {
		if (qedebug)
			qerror(dip, "sbus rx late error");
		qep->qe_laterr++;
		qep->qe_ierrors++;
		reinit++;
	}

	if (bits & QECM_STATUS_RXPARERR) {
		if (qedebug)
			qerror(dip, "sbus rx parity error");
		qep->qe_parerr++;
		qep->qe_ierrors++;
		reinit++;
	}

	if (bits & QECM_STATUS_RXERRACK) {
		if (qedebug)
			qerror(dip, "sbus rx error ack");
		qep->qe_errack++;
		qep->qe_ierrors++;
		reinit++;
	}

	if (reinit) {
		mutex_exit(&qep->qe_intrlock);
		(void) qeinit(qep);
		mutex_enter(&qep->qe_intrlock);
	}
}

/*
 * Start xmit on any msgs previously enqueued on any write queues.
 */
static void
qewenable(qep)
struct	qe	*qep;
{
	struct	qestr	*sqp;
	queue_t	*wq;

	/*
	 * Order of wantw accesses is important.
	 */
	do {
		qep->qe_wantw = 0;
		for (sqp = qestrup; sqp; sqp = sqp->sq_nextp)
			if (sqp->sq_qep == qep &&
				(wq = WR(sqp->sq_rq))->q_first)
				qenable(wq);
	} while (qep->qe_wantw);
}

/*
 * Initialize RMD.
 */
static void
qermdinit(rmdp, dvma_addr)
volatile	struct	qmd	*rmdp;
ulong_t		dvma_addr;
{
	/* XXX check this again */
	rmdp->qmd_addr = (uint_t)dvma_addr;
	rmdp->qmd_flags = QEBUFSIZE | QMD_OWN;
}

/*VARARGS*/
static void
qerror(dev_info_t *dip, char *fmt, ...)
{
	static	uint32_t last;
	static	char	*lastfmt;
	char		msg_buffer[255];
	va_list ap;

	mutex_enter(&qelock);

	/*
	 * Don't print same error message too often.
	 */
	if ((last == (hrestime.tv_sec & ~1)) && (lastfmt == fmt)) {
		mutex_exit(&qelock);
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

	mutex_exit(&qelock);
}

/*
 * Save all the counters before qeinit() stops the chip.
 * XXX Since the rntcc and rcvcc in mace are not cleared upon read,
 * this routine assumes it is called only right before resetting
 * the mace chip.  Otherwise, the statistics will be incorrect.
 */
static	void
qesavecntrs(qep)
struct	qe	*qep;
{
	volatile	struct	qecm_chan	*qcp;
	volatile	struct	mace	*macep;
	int	runt;
	int	missed;
	uchar_t	qec_sbus_delay = 0;

	qcp = qep->qe_chanregp;
	macep = qep->qe_maceregp;

	runt = macep->rntpc;
	qep->qe_runt += runt;
	qep->qe_ierrors += runt;

	missed = macep->mpc;
	qep->qe_missed += missed;
	qep->qe_ierrors += missed;

	/*
	 * Not an error.
	 */
	qep->qe_txcoll += qcp->coll & QECM_QECERRM_COLLM;
	macep->rcvfifo = qec_sbus_delay;		/* bug fix 1222358 */
	qep->qe_rxcoll += macep->rcvcc;
}


static	void
qe_start_timer(struct qe *qep, void (*func)(void *), int msec)
{
	/*
	 * only start a timer if one isn't already running!
	 */
	ASSERT(MUTEX_HELD(&qep->qe_xmitlock));
	if ((qep->qe_timerid == 0) && ((qep->qe_flags & QESTOP) == 0)) {
		qep->qe_timerid = timeout(func, qep, drv_usectohz(1000*msec));
	}
}

static	void
qe_stop_timer(struct qe *qep)
{
	timeout_id_t tid;

	mutex_enter(&qep->qe_xmitlock);
	if ((tid = qep->qe_timerid) != 0) {
		qep->qe_flags |= QESTOP;	/* do not reschedule */
		mutex_exit(&qep->qe_xmitlock);	/* no hold across untimeout */
		(void) untimeout(tid);		/* try to kill it */
	} else {
		mutex_exit(&qep->qe_xmitlock);
	}
}

static	void
qedog(void *qe_arg)
{
	struct qe *qep = qe_arg;

	mutex_enter(&qep->qe_xmitlock);
	qep->qe_timerid = 0;		/* timer not scheduled */
	/* if packets haven't moved and qeinit() requested */
	if (((qep->qe_oopackets == qep->qe_opackets) && qep->qe_once) ||
		((qep->qe_iipackets == qep->qe_ipackets) && qep->qe_ihang)) {
		mutex_exit(&qep->qe_xmitlock);
		(void) qeinit(qep);		/* calls qe_start_timer */
	} else {
		qep->qe_oopackets = qep->qe_opackets;
		qep->qe_iipackets = qep->qe_ipackets;
		qep->qe_once = 0;
		qep->qe_ihang = 0;
		qe_start_timer(qep, qedog, SECOND(qe_sanity_timer));
		mutex_exit(&qep->qe_xmitlock);
	}
}
