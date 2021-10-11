/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcgld.c	1.14	99/05/20 SMI"

/*
 * gld - Generic LAN Driver
 *
 * This is a utility module that provides generic facilities for
 * LAN	drivers.  The DLPI protocol and most STREAMS interfaces
 * are handled here.
 */

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
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/debug.h>
/* #include <sys/isa_defs.h> */
#include <sys/byteorder.h>

#ifdef	_DDICT

/*
 * This is not a strictly a driver, so ddict is not strictly
 * applicable, but we have run it anyway to improve somewhat
 * the cleanliness of this "misc" module.
 */

/* from sys/errno.h */
#define	ENOSR	63	/* out of streams resources		*/

/* from sys/modctl.h */
extern struct mod_ops mod_miscops;

/* from sys/time.h included in sys/kstat.h */
typedef	struct	_timespec {
	time_t		tv_sec;
	long		tv_nsec;
} timespec_t;
typedef	struct _timespec  timestruc_t;
extern	timestruc_t	hrestime;

/* from sys/strsun.h */
#define	DB_TYPE(mp)		((mp)->b_datap->db_type)

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/pctypes.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/pcmcia/pcgld.h>

#else	/* not _DDICT */

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/pccard.h>
#include <sys/strsun.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/pcmcia/pcgld.h>

#endif	/* not _DDICT */

#ifdef GLD_DEBUG
int	pcgld_debug = 0x0;
#endif

/*
 * function prototypes, etc.
 */
int pcgld_open(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *cred);
int pcgld_close(queue_t *q, int flag, cred_t *cred);
int pcgld_wput(queue_t *q, mblk_t *mp);
int pcgld_wsrv(queue_t *q);
int pcgld_rsrv(queue_t *q);
int	pcgld_update_kstat(kstat_t *, int);
u_int	pcgld_intr();
u_int	pcgld_intr_hi();
int pcgld_sched(gld_mac_info_t *macinfo);

static void gld_fastpath(gld_t *gld, queue_t *q, mblk_t *mp);
static void glderror();
static void gldinsque(void *elem, void *pred);
static void gldremque(void *arg);
static void gld_flushqueue(queue_t *q);
#ifdef i386
static int gld_findppa(glddev_t *device);
#endif
static int gld_findminor(glddev_t *device);
static void gld_send_disable_multi(gld_t *gld, gld_mac_info_t *macinfo,
	gld_mcast_t *mcast);
static int gld_physaddr(queue_t *q, mblk_t *mp);
static int gld_bind(queue_t *q, mblk_t *mp);
static int gld_looped(struct ether_header *hdr, gld_mac_info_t *macinfo);
static int gld_local(struct ether_header *hdr, gld_mac_info_t *macinfo);
static int gld_broadcast(struct ether_header *hdr, gld_mac_info_t *macinfo);
static int gld_cmds(queue_t *q, mblk_t *mp);
static int gld_promisc(queue_t *q, mblk_t *mp, int on);
static void gld_form_udata(gld_t *gld, gld_mac_info_t *macinfo, mblk_t *mp);
static int gld_setaddr(queue_t *q, mblk_t *mp);
static int gld_disable_multi(queue_t *q, mblk_t *mp);
static int gld_enable_multi(queue_t *q, mblk_t *mp);
static glddev_t *gld_devlookup(int major);
static int gldattach(queue_t *q, mblk_t *mp);
static void gld_initstats(gld_mac_info_t *macinfo);
static int gldunattach(queue_t *q, mblk_t *mp);
static int gld_unbind(queue_t *q, mblk_t *mp);
static int gld_unitdata(queue_t *q, mblk_t *mp);
static int gld_inforeq(queue_t *q, mblk_t *mp);
static int gld_multicast(struct ether_header *hdr, gld_t *gld);

/*
 * Allocate and zero-out "number" structures each of type "structure" in
 * kernel memory.
 */
#define	GETSTRUCT(structure, number)   \
	((structure *) kmem_zalloc(\
		(u_int) (sizeof (structure) * (number)), KM_NOSLEEP))

#define	abs(a) ((a) < 0 ? -(a) : a)

uchar_t pcgldbroadcastaddr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static char *gld_types[] = {
	"csmacd",
	"tpb",
	"tpr",
	"metro",
	"ether",
	"hdlc",
	"char",
	"ctca",
	"fddi",
	"other"
};

static char *gld_media_ether[] = {
	"unknown",
	"aui",
	"bnc",
	"10baseT",
	"fiber"
};

static struct glddevice gld_device_list;  /* Per-system root of GLD tables */

/*
 * Module linkage information for the kernel.
 */

struct modldrv modlmisc = {
	&mod_miscops,		/* Type of module - a utility provider */
	"Generic LAN Driver Utilities",
};

struct modlinkage modlinkage = {
	MODREV_1, &modlmisc, NULL
};


#if defined(NATIVE_GLD)
int
_init(void)
{
	register int e;


	/* initialize gld_device_list mutex */
	rw_init(&gld_device_list.gld_rwlock, NULL, RW_DRIVER, NULL);

	/* initialize device driver (per-major) list */
	gld_device_list.gld_next =
	    gld_device_list.gld_prev = &gld_device_list;
	e = mod_install(&modlinkage);
	if (e != 0) {
		rw_destroy(&gld_device_list.gld_rwlock);
	}
	return (e);
}

int
_fini(void)
{
	register int e;

	e = mod_remove(&modlinkage);
	if (e == 0) {
		/* remove all mutex and locks */
		rw_destroy(&gld_device_list.gld_rwlock);
	}
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
#else
void
pcgld_init(void)
{
	/* initialize gld_device_list mutex */
	rw_init(&gld_device_list.gld_rwlock, NULL, RW_DRIVER, NULL);

	/* initialize device driver (per-major) list */
	gld_device_list.gld_next =
	    gld_device_list.gld_prev = &gld_device_list;
}
#endif

/*
 * GLD service routines
 */

/*
 * pcgld_register -- called once per device instance (PPA)
 *
 * During its attach routine, a real device driver will register with GLD
 * so that later opens and dl_attach_reqs will work.  The arguments are the
 * devinfo pointer, the device name, and a macinfo structure describing the
 * physical device instance.
 */
int
pcgld_register(dev_info_t *devinfo, char *devname, gld_mac_info_t *macinfo)
{
	int nintrs, nregs;
	int mediatype = 0;
	char *media;
	int major = ddi_name_to_major(devname);
	glddev_t *glddev = gld_devlookup(major);
	int multisize;
	char minordev[32];

	/*
	 * normal case doesn't use hi level interrupts
	 * but we now want to handle them if requested.
	 */
#if 0
	if (ddi_intr_hilevel(devinfo, 0) && macinfo->gldm_intr_hi == NULL) {
		cmn_err(CE_WARN, "hi level interrupt requested");
		return (DDI_FAILURE);
	}
#endif

	/*
	 *  Allocate per-driver (major) data structure if necessary
	 */
	if (glddev == NULL) {
		/* first occurrence of this device name (major number) */
		glddev = GETSTRUCT(glddev_t, 1);
		if (glddev == NULL)
			return (DDI_FAILURE);
		(void) strcpy(glddev->gld_name, devname);
		glddev->gld_major = major;
		glddev->gld_mac_next = glddev->gld_mac_prev =
			(gld_mac_info_t *)&glddev->gld_mac_next;
		glddev->gld_str_next = glddev->gld_str_prev =
			(gld_t *)&glddev->gld_str_next;
		/*
		 * create the file system device node
		 */
		if (ddi_create_minor_node(devinfo, devname, S_IFCHR,
					0, DDI_NT_NET, 0) == DDI_FAILURE) {
			glderror(devinfo, "ddi_create_minor_node failed");
			kmem_free(glddev, sizeof (glddev_t));
			return (DDI_FAILURE);
		}
		rw_init(&glddev->gld_rwlock, NULL, RW_DRIVER, NULL);
		rw_enter(&gld_device_list.gld_rwlock, RW_WRITER);
		gldinsque(glddev, gld_device_list.gld_prev);
		rw_exit(&gld_device_list.gld_rwlock);
	}
	glddev->gld_ndevice++;

	/*
	 *  Per-instance initialization
	 */

	/* all drivers need the iblock cookie so get it this way */
	if (!macinfo->gldm_options & GLDOPT_PCMCIA)
		(void) ddi_get_iblock_cookie(devinfo, macinfo->gldm_irq_index,
					&macinfo->gldm_cookie);

	/* add interrupt handler; PCMCIA does it via Card Services */

	if (macinfo->gldm_intr != NULL &&
	    !(macinfo->gldm_options & GLDOPT_PCMCIA) &&
	    ddi_dev_nintrs(devinfo, &nintrs) == DDI_SUCCESS) {
		if (nintrs && (macinfo->gldm_irq_index >= 0) &&
		    (macinfo->gldm_irq_index < nintrs)) {
			if (ddi_add_intr(devinfo, macinfo->gldm_irq_index,
					NULL, NULL,
					macinfo->gldm_intr_hi ?
						pcgld_intr_hi : pcgld_intr,
					(caddr_t)macinfo) != DDI_SUCCESS) {
#ifdef lint
	/*
	 *  'goto' ifdeffed out until all gld dependent drivers have
	 *  been modified to correctly handle new error checking logic.
	 *
	 */
				goto failure;
#endif
			}
		}
	}

	/* if a split priority, add soft interrupt now */
	if (macinfo->gldm_intr_hi != NULL && ddi_intr_hilevel(devinfo, 0)) {
		(void) ddi_add_softintr(devinfo, DDI_SOFTINT_MED,
					&macinfo->gldm_softid,
					&macinfo->gldm_cookie,
					NULL, pcgld_intr,
					(caddr_t)macinfo);
		macinfo->gldm_GLD_flags |= GLD_INTR_HI | GLD_INTR_SOFT;
	}

	/* map the device memory; PCMCIA does via Card Services */
	/* **** This code bogusly assumes only one memory address range **** */
	if (!(macinfo->gldm_options & GLDOPT_PCMCIA) &&
	    ddi_dev_nregs(devinfo, &nregs) == DDI_SUCCESS) {
		if (nregs && (macinfo->gldm_reg_index >= 0) &&
		    (macinfo->gldm_reg_index < nregs)) {
			if (ddi_map_regs(devinfo, macinfo->gldm_reg_index,
			    &macinfo->gldm_memp, macinfo->gldm_reg_offset,
			    macinfo->gldm_reg_len) != DDI_SUCCESS) {
#ifdef lint
	/*
	 *  'goto' ifdeffed out until all gld dependent drivers have
	 *  been modified to correctly handle new error checking logic.
	 *
	 */
				goto late_failure;
#endif
			}
		}
	}

	/* Initialize per-instance data structures */
	macinfo->gldm_dev = glddev;
	/* note that PCMCIA must use socket number for PPA */
	if (!(macinfo->gldm_options & (GLDOPT_PCMCIA | GLDOPT_DRIVER_PPA))) {
#if defined(i386)
		/* in 2.6 the instance should be used in all cases */
		macinfo->gldm_ppa = gld_findppa(glddev);
#else
		macinfo->gldm_ppa = ddi_get_instance(devinfo);
#endif
	}
	if (!(macinfo->gldm_options & GLDOPT_PCMCIA)) {
		/* PCMCIA will do this again so don't do it here */
		(void) sprintf(minordev, "%s%d",
			devname,
			(int)macinfo->gldm_ppa);
		(void) ddi_create_minor_node(devinfo, minordev, S_IFCHR,
					(int)macinfo->gldm_ppa + 1,
					DDI_NT_NET, 0);
	}

	macinfo->gldm_devinfo = devinfo;
	mutex_init(&macinfo->gldm_maclock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);

	gld_initstats(macinfo);

	ddi_set_driver_private(devinfo, (caddr_t)macinfo);

	/* **** Why do these things even exist in the glddev? **** */
	glddev->gld_type = macinfo->gldm_type;
	glddev->gld_minsdu = macinfo->gldm_minpkt;
	glddev->gld_maxsdu = macinfo->gldm_maxpkt;
	multisize = ddi_getprop(DDI_DEV_T_NONE,
		devinfo, 0, "multisize", glddev->gld_multisize);
	if (glddev->gld_multisize < multisize)
		glddev->gld_multisize = multisize;
	else if (glddev->gld_multisize == 0)
		glddev->gld_multisize = GLD_MAX_MULTICAST;

	/* add ourselves to this major device's linked list of instances */
	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	gldinsque(macinfo, glddev->gld_mac_prev);
	rw_exit(&glddev->gld_rwlock);

	/*
	 * need to indicate we are NOW ready to process interrupts
	 * any interrupt before this is set is for someone else
	 */
	macinfo->gldm_GLD_flags |= GLD_INTR_READY;

	/* log local ethernet address */
	(void) localetheraddr((struct ether_addr *)macinfo->gldm_macaddr, NULL);

	/* now put announcement into the message buffer */
	mediatype =
	    (macinfo->gldm_type < (sizeof (gld_types)/sizeof (char *))) ?
		macinfo->gldm_type : DL_OTHER;
	switch (mediatype) {
	case DL_ETHER:
		if (macinfo->gldm_media >
		    (sizeof (gld_media_ether) / sizeof (char *)))
			macinfo->gldm_media = GLDM_UNKNOWN;
		media = gld_media_ether[macinfo->gldm_media];
		break;
	default:
		media = "unknown";
		break;
	}
	if (macinfo->gldm_ident == NULL)
		macinfo->gldm_ident = "LAN Driver";

	cmn_err(CE_CONT, "!%s%d (@0x%x): %s: %s (%s) %s\n", devname,
		macinfo->gldm_ppa,
		ddi_getprop(DDI_DEV_T_ANY, devinfo, DDI_PROP_DONTPASS,
			"ioaddr", 0),
		macinfo->gldm_ident,
		gld_types[mediatype], media,
		ether_sprintf((struct ether_addr *)macinfo->gldm_macaddr));

	ddi_report_dev(devinfo);
	return (DDI_SUCCESS);

late_failure:
	if (nintrs && macinfo->gldm_intr != NULL &&
	    (macinfo->gldm_irq_index >= 0) &&
	    (macinfo->gldm_irq_index < nintrs))
		ddi_remove_intr(devinfo, macinfo->gldm_irq_index,
				macinfo->gldm_cookie);

failure:
	glddev->gld_ndevice--;
	if (glddev->gld_ndevice == 0) {
		ddi_remove_minor_node(devinfo, NULL);
		rw_enter(&gld_device_list.gld_rwlock, RW_WRITER);
		gldremque(glddev);
		rw_exit(&gld_device_list.gld_rwlock);
		rw_destroy(&glddev->gld_rwlock);
		kmem_free(glddev, sizeof (glddev_t));
	}

	return (DDI_FAILURE);

}

/*
 * pcgld_unregister (macinfo)
 * remove the macinfo structure from local structures
 * this is cleanup for a driver to be unloaded
 */
int
pcgld_unregister(gld_mac_info_t *macinfo)
{
	glddev_t *glddev = macinfo->gldm_dev;
	dev_info_t *devinfo = macinfo->gldm_devinfo;
	int nintrs, nregs;

	/* remove the interrupt handler */
	/* *** This code bogusly assumes only one interrupt on this dev *** */
	/* *** Should check error return from these DDI calls		*** */

	if (macinfo->gldm_GLD_flags & GLD_INTR_SOFT) {
		macinfo->gldm_GLD_flags &= ~GLD_INTR_SOFT;
		ddi_remove_softintr(macinfo->gldm_softid);
	}

	if (!(macinfo->gldm_options & GLDOPT_PCMCIA) &&
	    /* clean up non-PCMCIA devices */
		ddi_dev_nintrs(devinfo, &nintrs) == DDI_SUCCESS) {
		if (nintrs && macinfo->gldm_intr != NULL)
			ddi_remove_intr(devinfo, macinfo->gldm_irq_index,
					macinfo->gldm_cookie);
	}

	/* unmap the device memory */
	/* *** Should check error return from these DDI calls *** */
	if (!(macinfo->gldm_options & GLDOPT_PCMCIA) &&
	    ddi_dev_nregs(devinfo, &nregs) == DDI_SUCCESS) {
		if (nregs && macinfo->gldm_memp != NULL)
			ddi_unmap_regs(devinfo, macinfo->gldm_reg_index,
					&macinfo->gldm_memp,
					macinfo->gldm_reg_offset,
					macinfo->gldm_reg_len);
	}

	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	gldremque(macinfo);
	rw_exit(&glddev->gld_rwlock);

	/* destroy the mutex for interrupt locking */
	mutex_destroy(&macinfo->gldm_maclock);

	kstat_delete(macinfo->gldm_kstatp);
	macinfo->gldm_kstatp = NULL;

	ddi_set_driver_private(devinfo, (caddr_t)NULL);

	/* We now have one fewer instance for this major device */
	glddev->gld_ndevice--;
	if (glddev->gld_ndevice == 0) {
		ddi_remove_minor_node(macinfo->gldm_devinfo, NULL);
		rw_enter(&gld_device_list.gld_rwlock, RW_WRITER);
		gldremque(glddev);
		rw_exit(&gld_device_list.gld_rwlock);
		rw_destroy(&glddev->gld_rwlock);
		kmem_free(glddev, sizeof (glddev_t));
	}
	return (DDI_SUCCESS);
}

/*
 * gld_initstats
 */
static void
gld_initstats(gld_mac_info_t *macinfo)
{
	struct gldkstats *sp;
	glddev_t *glddev;
	kstat_t *ksp;

	glddev = macinfo->gldm_dev;

	if ((ksp = kstat_create(glddev->gld_name, macinfo->gldm_ppa,
	    NULL, "net", KSTAT_TYPE_NAMED,
	    sizeof (struct gldkstats) / sizeof (kstat_named_t),
	    (uchar_t)KSTAT_FLAG_VIRTUAL)) == NULL) {
		cmn_err(CE_WARN, "failed to create kstat structure for %s",
		    glddev->gld_name);
		return;
	}
	macinfo->gldm_kstatp = ksp;

	ksp->ks_update = pcgld_update_kstat;
	ksp->ks_private = (void *)macinfo;

	ksp->ks_data = (void *)&macinfo->gldm_kstats;

	sp = &macinfo->gldm_kstats;
	kstat_named_init(&sp->glds_pktrcv, "ipackets", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_pktxmt, "opackets", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_errrcv, "ierrors", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_errxmt, "oerrors", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_bytexmt, "obytes", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_bytercv, "rbytes", KSTAT_DATA_ULONG);
	kstat_named_init(&sp-> glds_multixmt, "multixmt", KSTAT_DATA_LONG);
	kstat_named_init(&sp-> glds_multircv, "multircv", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_brdcstxmt, "brdcstxmt", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_brdcstrcv, "brdcstrcv", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_blocked, "blocked", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_noxmtbuf, "noxmtbuf", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_norcvbuf, "norcvbuf", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_xmtretry, "xmtretry", KSTAT_DATA_LONG);
	kstat_named_init(&sp->glds_intr, "intr", KSTAT_DATA_LONG);

	switch (macinfo->gldm_type) {
	case DL_ETHER:
	case DL_CSMACD:
		kstat_named_init(&sp->glds_collisions, "collisions",
					KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_defer, "defer", KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_frame, "framing", KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_crc, "crc", KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_overflow, "oflo", KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_underflow, "uflo",
					KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_missed, "missed", KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_xmtlatecoll, "late_collisions",
					KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_nocarrier, "nocarrier",
					KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_short, "short", KSTAT_DATA_ULONG);
		kstat_named_init(&sp->glds_excoll, "excollisions",
					KSTAT_DATA_ULONG);
		break;

	case DL_TPR:
				/* fill in for token ring */
		break;
	case DL_FDDI:
				/* fill in for FDDI or CDDI */
		break;
	}
	kstat_install(ksp);
}

pcgld_update_kstat(kstat_t *ksp, int rw)
{
	gld_mac_info_t *macinfo;
	struct gldkstats *gsp;
	struct gld_stats *stats;

	if (rw == KSTAT_WRITE)
		return (EACCES);

	macinfo = (gld_mac_info_t *)ksp->ks_private;
	ASSERT(macinfo != NULL);

	gsp = &macinfo->gldm_kstats;
	stats = &macinfo->gldm_stats;

	if (macinfo->gldm_gstat) {
		mutex_enter(&macinfo->gldm_maclock);
		(*macinfo->gldm_gstat)(macinfo);
		mutex_exit(&macinfo->gldm_maclock);
	}

	gsp->glds_pktxmt.value.ul = stats->glds_pktxmt;
	gsp->glds_pktrcv.value.ul = stats->glds_pktrcv;
	gsp->glds_errxmt.value.ul = stats->glds_errxmt;
	gsp->glds_errrcv.value.ul = stats->glds_errrcv;
	gsp->glds_collisions.value.ul = stats->glds_collisions;
	gsp->glds_bytexmt.value.ul = stats->glds_bytexmt;
	gsp->glds_bytercv.value.ul = stats->glds_bytercv;
	gsp->glds_multixmt.value.ul = stats->glds_multixmt;
	gsp->glds_multircv.value.ul = stats->glds_multircv;
	gsp->glds_brdcstxmt.value.ul = stats->glds_brdcstxmt;
	gsp->glds_brdcstrcv.value.ul = stats->glds_brdcstrcv;
	gsp->glds_blocked.value.ul = stats->glds_blocked;
	gsp->glds_excoll.value.ul = stats->glds_excoll;
	gsp->glds_defer.value.ul = stats->glds_defer;
	gsp->glds_frame.value.ul = stats->glds_frame;
	gsp->glds_crc.value.ul = stats->glds_crc;
	gsp->glds_overflow.value.ul = stats->glds_overflow;
	gsp->glds_underflow.value.ul = stats->glds_underflow;
	gsp->glds_short.value.ul = stats->glds_short;
	gsp->glds_missed.value.ul = stats->glds_missed;
	gsp->glds_xmtlatecoll.value.ul = stats->glds_xmtlatecoll;
	gsp->glds_nocarrier.value.ul = stats->glds_nocarrier;
	gsp->glds_noxmtbuf.value.ul = stats->glds_noxmtbuf;
	gsp->glds_norcvbuf.value.ul = stats->glds_norcvbuf;
	gsp->glds_intr.value.ul = stats->glds_intr;
	gsp->glds_xmtretry.value.ul = stats->glds_xmtretry;
	return (0);
}

/*
 * pcgld_open (q, dev, flag, sflag, cred)
 * generic open routine.  Hardware open will call this. The
 * hardware open passes in the gldevice structure (one per device class) as
 * well as all of the normal open parameters.
 */
/*ARGSUSED2*/
int
pcgld_open(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *cred)
{
	gld_t  *gld;
	ushort	minordev;
	long ppa;
	glddev_t *glddev;
	gld_mac_info_t *mac;
	mblk_t *mp;

	ASSERT(q);

	/* Find our per-major glddev_t structure */
	glddev = gld_devlookup(getmajor(*dev));
	ppa = getminor(*dev) & GLD_PPA_MASK;
	if (glddev == NULL)
		return (ENXIO);

	/*
	 * Serialize access through open/close this will serialize across all
	 * gld devices, but open and close are not frequent so should not
	 * induce much, if any delay.
	 */
	rw_enter(&glddev->gld_rwlock, RW_WRITER);

	/* find a free minor device number(per stream) and clone our dev */
	minordev = gld_findminor(glddev);
	*dev = makedevice(getmajor(*dev), minordev);

	ASSERT(q->q_ptr == NULL);	/* Clone device gives us a fresh Q */

	/*
	 * get a per-stream structure and link things together so we
	 * can easily find them later.
	 */
	mp = allocb(sizeof (gld_t), BPRI_MED);
	if (mp == NULL) {
		rw_exit(&glddev->gld_rwlock);
		return (ENOSR);
	}
	gld = (gld_t *)mp->b_rptr;
	ASSERT(gld != NULL);
	bzero((caddr_t)mp->b_rptr, sizeof (gld_t));
	/*
	 * fill in the structure and state info
	 */
	gld->gld_state = DL_UNATTACHED;
	if (ppa == GLD_USE_STYLE2) {
		gld->gld_style = DL_STYLE2;
	} else {
		gld->gld_style = DL_STYLE1;
		/* the PPA is actually 1 less than the minordev */
		ppa--;
		for (mac = glddev->gld_mac_next;
		    mac != (gld_mac_info_t *)(&glddev->gld_mac_next);
		    mac = mac->gldm_next) {
			ASSERT(mac);
			if (mac->gldm_ppa == ppa) {
				/*
				 * we found the correct PPA
				 */
				gld->gld_mac_info = mac;

				/* now ready for action */
				gld->gld_state = DL_UNBOUND;

				gld->gld_stats = &mac->gldm_stats;
				if (mac->gldm_nstreams == 0) {
					mutex_enter(&mac->gldm_maclock);
					/* reset and setup */
					(*mac->gldm_reset) (mac);
					/* now make sure it is running */
					(*mac->gldm_start) (mac);
					mutex_exit(&mac->gldm_maclock);
				}
				mac->gldm_nstreams++;
				break;
			}
		}
		if (gld->gld_state == DL_UNATTACHED) {
			freeb(mp);
			rw_exit(&glddev->gld_rwlock);
			return (ENXIO);
		}
	}
	gld->gld_mb = mp;
	gld->gld_qptr = q;
	WR(q)->q_ptr = q->q_ptr = (caddr_t)gld;
	gld->gld_minor = minordev;
	gld->gld_device = glddev;
	gldinsque(gld, glddev->gld_str_prev);

	rw_exit(&glddev->gld_rwlock);
	qprocson(q);		/* start the queues running */
	qenable(WR(q));
	return (0);
}

/*
 * pcgld_close(q) normal stream close call checks current status and cleans up
 * data structures that were dynamically allocated
 */
/*ARGSUSED1*/
int
pcgld_close(queue_t *q, int flag, cred_t *cred)
{
	gld_t	*gld = (gld_t *)q->q_ptr;
	glddev_t *glddev = gld->gld_device;
	gld_mac_info_t *macinfo = NULL;
	/*
	 * We are introducing a new variable caqlled tmp_macinfo, so that
	 * we can reset macinfo->gldm_last without any other side effect
	 * Not resetting this causes PANIC under network stress test (1169035)
	 */
	gld_mac_info_t *tmp_macinfo = NULL;

	ASSERT(q);
	ASSERT(gld);

	qprocsoff(q);

	tmp_macinfo = gld->gld_mac_info;

	if (gld->gld_state == DL_IDLE || gld->gld_state == DL_UNBOUND) {
		macinfo = gld->gld_mac_info;
		ASSERT(macinfo);
		gld->gld_state = DL_UNBOUND;
		(void) gldunattach(q, NULL);
	}

	/* disassociate the stream from the device */
	q->q_ptr = WR(q)->q_ptr = NULL;
	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	ASSERT(gld->gld_next);
	gldremque(gld);			/* remove from active list */

	/* make sure that we can't touch this non-existent stream */
	if (tmp_macinfo != NULL && tmp_macinfo->gldm_last == gld)
		tmp_macinfo->gldm_last = NULL;

	rw_exit(&glddev->gld_rwlock);

	freeb(gld->gld_mb);

	return (0);
}

/*
 * pcgld_wput (q, mp)
 * general gld stream write put routine. Receives ioctl's from
 * user level and data from upper modules and processes them immediately.
 * M_PROTO/M_PCPROTO are queued for later processing by the service
 * procedure.
 */

int
pcgld_wput(queue_t *q, mblk_t *mp)
{
	gld_t  *gld = (gld_t *)(q->q_ptr);
	gld_mac_info_t *macinfo;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "pcgld_wput(%x %x): type %x",
		    (int)q, (int)mp, DB_TYPE(mp));
#endif
	switch (DB_TYPE(mp)) {

	case M_IOCTL:		/* no waiting in ioctl's */
		if (gld->gld_flags & GLD_LOCKED ||
		    gld->gld_state == DL_UNATTACHED)
				/* if locked, queue for now. */
			(void) putq(q, mp);
		else
			(void) pcgld_ioctl(q, mp);
		break;

	case M_FLUSH:		/* canonical flush handling */
		if (*mp->b_rptr & FLUSHW)
			flushq(q, 0);
		if (*mp->b_rptr & FLUSHR) {
			flushq(RD(q), 0);
			*mp->b_rptr &= ~FLUSHW;
			qreply(q, mp);
		} else
			freemsg(mp);
		break;

		/* for now, we will always queue */
	case M_PROTO:
	case M_PCPROTO:
		gld->gld_flags |= GLD_XWAIT;
		(void) putq(q, mp);
		break;

	case M_DATA:
		/* fast data / raw support */
		if ((gld->gld_flags & (GLD_RAW | GLD_FAST)) == 0 ||
		    (gld->gld_state != DL_IDLE)) {
			merror(q, mp, EPROTO);
			break;
		}
		/* need to do further checking */
		macinfo = gld->gld_mac_info;
		if (q->q_next == NULL && macinfo != NULL) {
			mblk_t *dup = NULL;
			mutex_enter(&macinfo->gldm_maclock);
			gld->gld_flags |= GLD_LOCKED;
			if (macinfo->gldm_nprom > 0)
				dup = dupmsg(mp);
			if (macinfo->gldm_send(macinfo, mp) == 0) {
				/* sent it early */
				macinfo->gldm_stats.glds_bytexmt +=
					msgdsize(mp);
				macinfo->gldm_stats.glds_pktxmt++;
				freemsg(mp);
				if (dup != NULL)
					(void) pcgld_recv(macinfo, dup);
			} else {
				gld->gld_flags |= GLD_XWAIT;
				(void) putq(q, mp);
				if (dup != NULL)
					freemsg(dup);
			}
			gld->gld_flags &= ~GLD_LOCKED;
			mutex_exit(&macinfo->gldm_maclock);
		} else {
			gld->gld_flags |= GLD_XWAIT;
			(void) putq(q, mp);
		}
		break;

	default:
#ifdef GLD_DEBUG
		if (pcgld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld: Unexpected packet type from queue: %x",
			    DB_TYPE(mp));
#endif
		freemsg(mp);
	}
	return (0);
}

/*
 * pcgld_wsrv - Incoming messages are processed according to the DLPI protocol
 * specification
 */

int
pcgld_wsrv(queue_t *q)
{
	mblk_t *mp, *nmp;
	register gld_t *gld = (gld_t *)q->q_ptr;
	union DL_primitives *prim;
	gld_mac_info_t *macinfo;
	int	err;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "pcgld_wsrv(%x)", (int)q);
#endif


	while ((mp = getq(q)) != NULL) {
		/* assume Q will empty.  A fail will reset the flag */
		gld->gld_flags &= ~GLD_XWAIT;
		switch (DB_TYPE(mp)) {
		case M_IOCTL:
			/* case where we couldn't do it in the put procedure */
			(void) pcgld_ioctl(q, mp);
			break;
		case M_PROTO:	/* Will be an DLPI message of some type */
		case M_PCPROTO:
			if ((err = gld_cmds(q, mp)) != GLDE_OK) {
				if (err == GLDE_RETRY)
					return (0); /* quit while we're ahead */
				prim = (union DL_primitives *)mp->b_rptr;
				dlerrorack(q, mp, prim->dl_primitive, err, 0);
			}
			break;
		case M_DATA:
			/*
			 * retry of a previously processed
			 * UNITDATA_REQ or is a RAW message from
			 * above
			 */
			ASSERT(gld->gld_state == DL_IDLE);
			macinfo = gld->gld_mac_info;
			ASSERT(macinfo != NULL);
			gld->gld_flags |= GLD_LOCKED;
			/* want to loop back if doing promiscuous mode */
			if (macinfo->gldm_nprom > 0)
				nmp = dupmsg(mp);
			else
				nmp = NULL;
			mutex_enter(&macinfo->gldm_maclock);
			if ((*macinfo->gldm_send) (macinfo, mp)) {
				/* for pcgld_sched */
				gld->gld_flags |= GLD_XWAIT;
				gld->gld_flags &= ~GLD_LOCKED;
				mutex_exit(&macinfo->gldm_maclock);
				macinfo->gldm_stats.glds_xmtretry++;
				(void) putbq(q, mp);
				if (nmp)
					freemsg(nmp);
				return (0);
			}
			/* want to loop back if doing promiscuous mode */
			if (nmp != NULL) {
				(void) pcgld_recv(macinfo, nmp);
			}
			gld->gld_flags &= ~GLD_LOCKED;
			mutex_exit(&macinfo->gldm_maclock);
			macinfo->gldm_stats.glds_bytexmt += msgdsize(mp);
			macinfo->gldm_stats.glds_pktxmt++;
			freemsg(mp);	/* free on success */
			break;

			/* This should never happen */
		default:
#ifdef GLD_DEBUG
			if (pcgld_debug & GLDERRS)
				cmn_err(CE_WARN,
				    "pcgld_wsrv: db_type(%x) not supported",
				    mp->b_datap->db_type);
#endif
			freemsg(mp);	/* unknown types are discarded */
			break;
		}
	}
	return (0);
}

/*
 * pcgld_rsrv (q)
 *	simple read service procedure
 *	purpose is to avoid the time it takes for packets
 *	to move through IP so we can get them off the board
 *	as fast as possible due to limited PC resources.
 */

int
pcgld_rsrv(queue_t *q)
{
	mblk_t *mp;

	while ((mp = getq(q)) != NULL) {
		if (canputnext(q)) {
			putnext(q, mp);
		} else {
			freemsg(mp);
		}
	}
	return (0);
}

/*
 * gld_multicast used to determine if the address is a multicast address for
 * this user.
 */
static int
gld_multicast(struct ether_header *hdr, gld_t *gld)
{
	register int i;

	if (gld->gld_mcast) {
		for (i = 0; i < gld->gld_multicnt; i++) {
			if (gld->gld_mcast[i] &&
			    gld->gld_mcast[i]->gldm_refcnt) {
				if (bcmp((caddr_t)gld->gld_mcast[i]->gldm_addr,
				    (caddr_t)hdr->ether_dhost.ether_addr_octet,
				    ETHERADDRL) == 0)
					return (1);
			}
		}
	}
	return (0);
}

/*
 * pcgld_ioctl (q, mp)
 * handles all ioctl requests passed downstream. This routine is
 * passed a pointer to the message block with the ioctl request in it, and a
 * pointer to the queue so it can respond to the ioctl request with an ack.
 */

int
pcgld_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp;
	register gld_t *gld;
	gld_mac_info_t *macinfo;
	cred_t *cred;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "pcgld_ioctl(%x %x)", (int)q, (int)mp);
#endif
	gld = (gld_t *)q->q_ptr;
	iocp = (struct iocblk *)mp->b_rptr;
	cred = iocp->ioc_cr;
	switch (iocp->ioc_cmd) {
	case DLIOCRAW:		/* raw M_DATA mode */
		if (cred == NULL || drv_priv(cred) == 0) {
			/* Only do if we have permission to avoid problems */
			gld->gld_flags |= GLD_RAW;
			DB_TYPE(mp) = M_IOCACK;
			qreply(q, mp);
		} else
			miocnak(q, mp, 0, EPERM);
		break;

	case DL_IOC_HDR_INFO:
				/* fastpath */
		gld_fastpath(gld, q, mp);
		break;
	default:
		macinfo	 = gld->gld_mac_info;
		if (macinfo != NULL && macinfo->gldm_ioctl != NULL) {
			mutex_enter(&macinfo->gldm_maclock);
			(*macinfo->gldm_ioctl) (q, mp);
			mutex_exit(&macinfo->gldm_maclock);
		} else
			miocnak(q, mp, 0, EINVAL);
		break;
	}
	return (0);
}

/*
 * gld_cmds (q, mp)
 *	process the DL commands as defined in dlpi.h
 *	note that the primitives return status which is passed back
 *	to the service procedure.  If the value is GLDE_RETRY, then
 *	it is assumed that processing must stop and the primitive has
 *	been put back onto the queue.  If the value is any other error,
 *	then an error ack is generated by the service procedure.
 */
static int
gld_cmds(queue_t *q, mblk_t *mp)
{
	register union DL_primitives *dlp;
	gld_t *gld = (gld_t *)(q->q_ptr);
	int result;

	dlp = (union DL_primitives *)mp->b_rptr;
#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE,
		    "gld_cmds(%x, %x):dlp=%x, dlp->dl_primitive=%d",
		    (int)q, (int)mp, (int)dlp, (int)dlp->dl_primitive);
#endif

	switch (dlp->dl_primitive) {
	case DL_BIND_REQ:
		result = gld_bind(q, mp);
		break;

	case DL_UNBIND_REQ:
		result = gld_unbind(q, mp);
		break;

	case DL_UNITDATA_REQ:
		result = gld_unitdata(q, mp);
		break;

	case DL_INFO_REQ:
		result = gld_inforeq(q, mp);
		break;

	case DL_ATTACH_REQ:
		if (gld->gld_style == DL_STYLE2)
			result = gldattach(q, mp);
		else
			result = DL_NOTSUPPORTED;
		break;

	case DL_DETACH_REQ:
		if (gld->gld_style == DL_STYLE2)
			result = gldunattach(q, mp);
		else
			result = DL_NOTSUPPORTED;
		break;

	case DL_ENABMULTI_REQ:
		result = gld_enable_multi(q, mp);
		break;

	case DL_DISABMULTI_REQ:
		result = gld_disable_multi(q, mp);
		break;

	case DL_PHYS_ADDR_REQ:
		result = gld_physaddr(q, mp);
		break;

	case DL_SET_PHYS_ADDR_REQ:
		result = gld_setaddr(q, mp);
		break;

	case DL_PROMISCON_REQ:
		result = gld_promisc(q, mp, 1);
		break;
	case DL_PROMISCOFF_REQ:
		result = gld_promisc(q, mp, 0);
		break;
	case DL_XID_REQ:
	case DL_XID_RES:
	case DL_TEST_REQ:
	case DL_TEST_RES:
		result = DL_NOTSUPPORTED;
		break;
	default:
#ifdef GLD_DEBUG
		if (pcgld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld_cmds: unknown M_PROTO message: %d",
			    (int)dlp->dl_primitive);
#endif
		result = DL_BADPRIM;
	}
	return (result);
}

/*
 * gld_bind - determine if a SAP is already allocated and whether it is legal
 * to do the bind at this time
 */
static int
gld_bind(queue_t *q, mblk_t *mp)
{
	int	sap;
	register dl_bind_req_t *dlp;
	gld_t  *gld = (gld_t *)q->q_ptr;

	ASSERT(gld);

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_bind(%x %x)", (int)q, (int)mp);
#endif

	dlp = (dl_bind_req_t *)mp->b_rptr;
	sap = dlp->dl_sap;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDPROT)
		cmn_err(CE_NOTE, "gld_bind: lsap=%x", sap);
#endif

	ASSERT(gld->gld_qptr == RD(q));
	if (gld->gld_state != DL_UNBOUND) {
#ifdef GLD_DEBUG
		if (pcgld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_bind: bound or not attached (%d)",
				(int)gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}
	if (dlp->dl_service_mode != DL_CLDLS) {
		return (DL_UNSUPPORTED);
	}
	if (dlp->dl_xidtest_flg & (DL_AUTO_XID | DL_AUTO_TEST)) {
		return (DL_NOAUTO);
	}
	if (sap > GLDMAXETHERSAP)
		return (DL_BADSAP);

	/* search for SAP already in use in this PPA */
#ifdef notdef
	glddev = gld->gld_device;
	/*
	 * currently we no longer restrict to one STREAM per SAP
	 * if this is ever desired again, enable this code.
	 */
	rw_enter(&glddev->gld_rwlock, RW_READER);
	for (lldp = glddev->gld_str_next;
		lldp != (gld_t *)&glddev->gld_str_next;
		lldp = lldp->gld_next) {
		if (gld == lldp)
			continue;
		if (lldp->gld_mac_info == gld->gld_mac_info &&
		    lldp->gld_state == DL_IDLE && lldp->gld_sap == sap) {
			/* SAP already in use */
			rw_exit(&glddev->gld_rwlock);
			return (DL_NOADDR);
		}
	}
	rw_exit(&glddev->gld_rwlock);
#endif
	/* if we fall through, then the SAP is legal */
	gld->gld_sap = sap;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDPROT)
		cmn_err(CE_NOTE, "gld_bind: ok - type = %d",
			(int)gld->gld_type);
#endif

	/* ACK the BIND */

	dlbindack(q, mp, sap, (u_char *)gld->gld_mac_info->gldm_macaddr,
			6, 0, 0);

	gld->gld_state = DL_IDLE;	/* bound and ready */
	return (GLDE_OK);
}

/*
 * gld_unbind - perform an unbind of an LSAP or ether type on the stream.
 * The stream is still open and can be re-bound.
 */
static int
gld_unbind(queue_t *q, mblk_t *mp)
{
	gld_t *gld = (gld_t *)q->q_ptr;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_unbind(%x %x)", (int)q, (int)mp);
#endif

	if (gld->gld_state != DL_IDLE) {
#ifdef GLD_DEBUG
		if (pcgld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unbind: wrong state (%d)",
				(int)gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}
	gld->gld_sap = 0;
	gld_flushqueue(q);	/* flush the queues */
	dlokack(q, mp, DL_UNBIND_REQ);
	gld->gld_state = DL_UNBOUND;
	return (GLDE_OK);
}

/*
 * gld_inforeq - generate the response to an info request
 */
static int
gld_inforeq(queue_t *q, mblk_t *mp)
{
	gld_t  *gld;
	mblk_t *nmp;
	dl_info_ack_t *dlp;
	int	bufsize;
	glddev_t *glddev;
	gld_mac_info_t *macinfo;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_inforeq(%x %x)", (int)q, (int)mp);
#endif
	gld = (gld_t *)q->q_ptr;
	ASSERT(gld);
	glddev = gld->gld_device;

	bufsize = sizeof (dl_info_ack_t) + 2 * (ETHERADDRL + 2);

	nmp = mexchange(q, mp, bufsize, M_PCPROTO, DL_INFO_ACK);

	if (nmp) {
		nmp->b_wptr = nmp->b_rptr + sizeof (dl_info_ack_t);
		dlp = (dl_info_ack_t *)nmp->b_rptr;
		bzero((caddr_t)dlp, sizeof (dl_info_ack_t));
		dlp->dl_primitive = DL_INFO_ACK;
		dlp->dl_service_mode = DL_CLDLS;
		dlp->dl_current_state = gld->gld_state;
		dlp->dl_provider_style = gld->gld_style;

		if (gld->gld_state == DL_IDLE || gld->gld_state == DL_UNBOUND) {
			macinfo = gld->gld_mac_info;
			ASSERT(macinfo != NULL);
			dlp->dl_min_sdu = macinfo->gldm_minpkt;
			dlp->dl_max_sdu = macinfo->gldm_maxpkt;
			dlp->dl_mac_type = macinfo->gldm_type;

			/* copy macaddr and sap */
			if (gld->gld_state == DL_IDLE)
				dlp->dl_addr_offset = sizeof (dl_info_ack_t);
			else
				dlp->dl_addr_offset = NULL;

			dlp->dl_addr_length = macinfo->gldm_addrlen;
			dlp->dl_sap_length = macinfo->gldm_saplen;
			dlp->dl_addr_length += abs(dlp->dl_sap_length);
			nmp->b_wptr += dlp->dl_addr_length +
				abs(macinfo->gldm_saplen);
			if (dlp->dl_addr_offset != NULL)
			    bcopy((caddr_t)macinfo->gldm_macaddr,
				((caddr_t)dlp) + dlp->dl_addr_offset,
				macinfo->gldm_addrlen);

			if (gld->gld_state == DL_IDLE) {
				/*
				 * save the correct number of bytes in the DLSAP
				 * we currently only handle negative sap lengths
				 * so a positive one will just get ignored.
				 */
				switch (macinfo->gldm_saplen) {
				case -1:
					*(((caddr_t)dlp) +
					    dlp->dl_addr_offset +
					    dlp->dl_addr_length) =
						gld->gld_sap;
					break;
				case -2:
					*(ushort *)(((caddr_t)dlp) +
					    dlp->dl_addr_offset +
					    dlp->dl_addr_length) =
						gld->gld_sap;
					break;
				}

				dlp->dl_brdcst_addr_offset =
				    dlp->dl_addr_offset + dlp->dl_addr_length;
			} else {
				dlp->dl_brdcst_addr_offset =
				    sizeof (dl_info_ack_t);
			}
			/* copy broadcast addr */
			dlp->dl_brdcst_addr_length = macinfo->gldm_addrlen;
			nmp->b_wptr += dlp->dl_brdcst_addr_length;
			bcopy((caddr_t)macinfo->gldm_broadcast,
				((caddr_t)dlp) + dlp->dl_brdcst_addr_offset,
				macinfo->gldm_addrlen);
		} else {
			/*
			 *** these are probably all bogus since we ***
			 *** don't have an attached device. ***
			 */
			dlp->dl_min_sdu = glddev->gld_minsdu;
			dlp->dl_max_sdu = glddev->gld_maxsdu;
			dlp->dl_mac_type = glddev->gld_type;
			dlp->dl_addr_offset = NULL;
/* *** VIOLATION *** */ dlp->dl_addr_length = 8;	/* ETHERADDRL + 2 */
			dlp->dl_sap_length = -2;

			dlp->dl_brdcst_addr_offset = sizeof (dl_info_ack_t);
			dlp->dl_brdcst_addr_length = ETHERADDRL;
			nmp->b_wptr += dlp->dl_brdcst_addr_length;
			bcopy((caddr_t)pcgldbroadcastaddr,
				((caddr_t)dlp) + dlp->dl_brdcst_addr_offset,
				ETHERADDRL);
		}
		dlp->dl_version = DL_VERSION_2;
		qreply(q, nmp);
	}
	return (GLDE_OK);
}

/*
 * gld_unitdata (q, mp)
 * send a datagram.  Destination address/lsap is in M_PROTO
 * message (first mblock), data is in remainder of message.
 *
 */
static int
gld_unitdata(queue_t *q, mblk_t *mp)
{
	register gld_t *gld = (gld_t *)q->q_ptr;
	dl_unitdata_req_t *dlp = (dl_unitdata_req_t *)mp->b_rptr;
	register struct ether_header *hdr;
	gld_mac_info_t *macinfo;
	struct gld_dlsap *gldp;
	mblk_t *nmp;
	long	msglen;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_unitdata(%x %x)", (int)q, (int)mp);
#endif

	if (gld->gld_state != DL_IDLE) {
#ifdef GLD_DEBUG
		if (pcgld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unitdata: wrong state (%d)",
				(int)gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	msglen = msgdsize(mp);
	if (msglen == 0 || msglen > macinfo->gldm_maxpkt) {
		dluderrorind(q, mp,
		    (u_char *)DLSAP(dlp, dlp->dl_dest_addr_offset),
		    dlp->dl_dest_addr_length, DL_BADDATA, 0);
		return (GLDE_OK);
	}

	/*
	 * make a valid header for transmission
	 */
	gldp = DLSAP(dlp, dlp->dl_dest_addr_offset);

	/* need a buffer big enough for the headers */
	nmp = allocb(macinfo->gldm_addrlen * 2 + 2 + 8, BPRI_MED);
	if (nmp == NULL) {
		dlerrorack(q, mp, DL_UNITDATA_REQ, DL_SYSERR, ENOSR);
		return (GLDE_OK);
	}
	hdr = (struct ether_header *)nmp->b_rptr;

	switch (macinfo->gldm_type) {
		/* fill in type dependent fields */
	case DL_ETHER:		/* Ethernet */
		if (gld->gld_sap <= GLD_MAX_802_SAP)
			hdr->ether_type = ntohs(msglen);
		else
			hdr->ether_type = ntohs(gld->gld_sap);
		nmp->b_wptr = nmp->b_rptr + sizeof (struct ether_header);
		bcopy((caddr_t)gldp->glda_addr,
		    (caddr_t)hdr->ether_dhost.ether_addr_octet, ETHERADDRL);
		bcopy((caddr_t)macinfo->gldm_macaddr,
		    (caddr_t)hdr->ether_shost.ether_addr_octet, ETHERADDRL);
		msglen += sizeof (struct ether_header);
		break;

	default:		/* either RAW or unknown, send as is */
		break;
	}
	DB_TYPE(nmp) = M_DATA; /* ether/gld header is data */
	linkb(nmp, mp->b_cont);
	freeb(mp);
	mp = nmp;

	if (ismulticast(hdr->ether_dhost.ether_addr_octet)) {
		if (bcmp((caddr_t)hdr->ether_dhost.ether_addr_octet,
		    (caddr_t)macinfo->gldm_broadcast,
		    macinfo->gldm_addrlen) == 0)
			macinfo->gldm_stats.glds_brdcstxmt++;
		else
			macinfo->gldm_stats.glds_multixmt++;
	}

	/* want to loop back if doing promiscuous mode */
	if (macinfo->gldm_nprom > 0)
		nmp = dupmsg(mp);
	else
		nmp = NULL;

	mutex_enter(&macinfo->gldm_maclock);
	if ((*macinfo->gldm_send) (macinfo, mp)) {
		macinfo->gldm_stats.glds_xmtretry++;
		gld->gld_flags |= GLD_XWAIT;	/* for pcgld_sched */
		gld->gld_stats->glds_noxmtbuf++;
		mutex_exit(&macinfo->gldm_maclock);
		(void) putbq(q, mp);
		if (nmp)
			freemsg(nmp);
		return (GLDE_RETRY);		/* GLDE_OK works too */
	}
	mutex_exit(&macinfo->gldm_maclock);
	gld->gld_stats->glds_bytexmt += msglen;
	gld->gld_stats->glds_pktxmt++;
	freemsg(mp);		/* free now that done */
	/* want to loop back if doing promiscuous mode */
	if (nmp != NULL) {
		(void) pcgld_recv(macinfo, nmp);
	}

	return (GLDE_OK);
}

int
gld_recv_defer(register gld_mac_info_t *macinfo, mblk_t *mp)
{
	mblk_t *prev;

	if (macinfo->gldm_rcvq) {
		for (prev = macinfo->gldm_rcvq; prev->b_next != NULL;
		    prev = prev->b_next)
			;
		prev->b_next = mp;
	} else {
		macinfo->gldm_rcvq = mp;
	}
	return (0);
}

/*
 * pcgld_recv (macinfo, mp)
 * called with an ethernet packet in a mblock; must decide whether
 * packet is for us and which streams to queue it to.
 */
int
pcgld_recv(register gld_mac_info_t *macinfo, mblk_t *mp)
{
	struct ether_header *ehdr;
	gld_t *gld;
	glddev_t *glddev;
	mblk_t *nmp;
	int	nmcast = 0, statcnt_normal = 0, statcnt_brdcst = 0;
	register int valid, msgsap;
	int looped_back, stream_valid;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "pcgld_recv(%x, %x)", (int)mp, (int)macinfo);
#endif

	if (macinfo == NULL) {
		freemsg(mp);
		return (0);
	}
	nmp = NULL;

	glddev = macinfo->gldm_dev;
	msgsap = -1;
	ehdr = NULL;

	switch (macinfo->gldm_type) {
	case DL_ETHER:
		ehdr = (struct ether_header *)mp->b_rptr;
		msgsap = ntohs(ehdr->ether_type);
		if (msgsap <= GLD_802_SAP) {
			int len;
			/* adjust message size to length indicated */
			len = msgdsize(mp);
			msgsap += sizeof (struct ether_header);
			if (len > msgsap) {
				(void) adjmsg(mp, msgsap-len);
			}
		}
#ifdef GLD_DEBUG
#define	MLEN(mp) ((mp)->b_wptr - (mp)->b_rptr)
		if (pcgld_debug & GLDRECV) {
			cmn_err(CE_CONT, "pcgld_recv: mlen=%ld machdr=<%s-",
				(long)MLEN(mp),
				ether_sprintf(&ehdr->ether_dhost));
			cmn_err(CE_CONT, "%s-%x> %x\n",
				ether_sprintf(&ehdr->ether_shost),
				ntohs(ehdr->ether_type),
				*(ushort_t *)mp->b_rptr);
		}
#endif
		break;
	case DL_TPR:
		/* use real SAP or just a fake one??? */
		msgsap = 2;	/* just for now */
		break;
	}
	if (gld_broadcast(ehdr, macinfo)) {
		valid = 2;	/* 2 means valid but multicast */
		statcnt_brdcst = 1;
	} else {
		valid = gld_local(ehdr, macinfo);
		statcnt_normal = msgdsize(mp);
	}

	looped_back = gld_looped(ehdr, macinfo);
	if (looped_back) {
		valid = 0;		/* don't want loopback as valid */
	}

	rw_enter(&glddev->gld_rwlock, RW_READER);
	for (gld = glddev->gld_str_next;
		gld != (gld_t *)&glddev->gld_str_next;
		gld = gld->gld_next) {
		if (gld->gld_qptr == NULL || gld->gld_state != DL_IDLE ||
		    gld->gld_mac_info != macinfo) {
			continue;
		}
		stream_valid = valid;
#ifdef GLD_DEBUG
		if (pcgld_debug & GLDRECV)
			cmn_err(CE_NOTE,
			    "pcgld_recv: type=%d, sap=%x, pkt-dsap=%x",
			    (int)gld->gld_type, (int)gld->gld_sap,
				(int)msgsap);
#endif

		if (!valid &&
		    ismulticast(ehdr->ether_dhost.ether_addr_octet) &&
		    ((gld->gld_multicnt > 0 && gld_multicast(ehdr, gld)) ||
		    gld->gld_flags & GLD_PROM_MULT)) {
			stream_valid |= 4;
			nmcast ++;
		} else if (gld->gld_flags & GLD_PROM_PHYS)
			/* promiscuous mode */
			stream_valid = 1;

		/* promiscuous streams or a real match */
		if (stream_valid &&
		    ((gld->gld_flags & GLD_PROM_SAP) ||
		    (gld->gld_sap == msgsap) ||
		    (gld->gld_sap <= GLD_802_SAP &&
		    msgsap <= GLD_802_SAP))) {
			/* sap matches */
			if (!canput(gld->gld_qptr)) {
#ifdef GLD_DEBUG
				if (pcgld_debug & GLDRECV)
					cmn_err(CE_WARN,
					    "pcgld_recv: canput failed");
#endif
				gld->gld_stats->glds_blocked++;
				qenable(gld->gld_qptr);
				continue;
			}

			nmp = dupmsg(mp);
			if (gld->gld_flags & GLD_FAST &&
			    !statcnt_brdcst && !nmcast) {
				switch (macinfo->gldm_type) {
				case DL_ETHER:
					mp->b_rptr +=
						sizeof (struct ether_header);
					break;
				case DL_TPR:
					break;
				}
				if (gld->gld_qptr->q_next == NULL &&
				    canputnext(gld->gld_qptr)) {
					(void) putnext(gld->gld_qptr, mp);
				} else {
					(void) putq(gld->gld_qptr, mp);
				}
			} else if (gld->gld_flags & GLD_RAW) {
				if (gld->gld_qptr->q_next == NULL &&
				    canputnext(gld->gld_qptr)) {
					(void) putnext(gld->gld_qptr, mp);
				} else {
					(void) putq(gld->gld_qptr, mp);
				}
			} else {
				gld_form_udata(gld, macinfo, mp);
			}
			mp = nmp;
			if (mp == NULL)
				break;	/* couldn't get resources; drop it */
		}
	}
	rw_exit(&glddev->gld_rwlock);

	if (mp != NULL) {
		freemsg(mp);
	}
	if (nmcast > 0)
		macinfo->gldm_stats.glds_multircv++;
	if (statcnt_brdcst) {
		macinfo->gldm_stats.glds_brdcstrcv++;
	}
	if (statcnt_normal) {
		macinfo->gldm_stats.glds_bytercv += statcnt_normal;
		macinfo->gldm_stats.glds_pktrcv++;
	}
	return (0);
}

/*
 * gld_local (hdr, macinfo)
 * check to see if the message is addressed to this system by
 * comparing with the board's address.
 */
static int
gld_local(struct ether_header *hdr, gld_mac_info_t *macinfo)
{
	return (bcmp((caddr_t)hdr->ether_dhost.ether_addr_octet,
			(caddr_t)macinfo->gldm_macaddr,
			macinfo->gldm_addrlen) == 0);
}

/*
 * gld_looped (hdr, macinfo)
 * check to see if the message is addressed to this system by
 * comparing with the board's address.
 */
static int
gld_looped(struct ether_header *hdr, gld_mac_info_t *macinfo)
{
	return (bcmp((caddr_t)hdr->ether_shost.ether_addr_octet,
			(caddr_t)macinfo->gldm_macaddr,
			macinfo->gldm_addrlen) == 0);
}

/*
 * gld_broadcast (hdr, macinfo)
 * check to see if a broadcast address is the destination of
 * this received packet
 */
static int
gld_broadcast(struct ether_header *hdr, gld_mac_info_t *macinfo)
{
	return (bcmp((caddr_t)hdr->ether_dhost.ether_addr_octet,
			(caddr_t)macinfo->gldm_broadcast,
			macinfo->gldm_addrlen) == 0);
}

/*
 * gldattach(q, mp)
 * DLPI DL_ATTACH_REQ
 * this attaches the stream to a PPA
 */
static int
gldattach(queue_t *q, mblk_t *mp)
{
	dl_attach_req_t *at;
	gld_mac_info_t *mac;
	gld_t  *gld = (gld_t *)q->q_ptr;
	glddev_t *glddev;

	at = (dl_attach_req_t *)mp->b_rptr;

	if (gld->gld_state != DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}
	glddev = gld->gld_device;
	for (mac = glddev->gld_mac_next;
		mac != (gld_mac_info_t *)(&glddev->gld_mac_next);
		mac = mac->gldm_next) {
		ASSERT(mac);
		if (mac->gldm_ppa == at->dl_ppa) {

			ASSERT(!gld->gld_mac_info);

			/*
			 * we found the correct PPA
			 */
			gld->gld_mac_info = mac;

			gld->gld_state = DL_UNBOUND; /* now ready for action */
			gld->gld_stats = &mac->gldm_stats;

			/*
			 * We must hold the mutex to prevent multiple calls
			 * to the reset and start routines.
			 *
			 * We know our mutex is not held because gld_cmds
			 * did not have a pointer to our mac when it called
			 * us, since we just assigned it, so it could not
			 * enter the mutex for us.
			 */
			mutex_enter(&mac->gldm_maclock);
			if (mac->gldm_nstreams == 0) {
				/* reset and setup */
				(*mac->gldm_reset) (mac);
				/* now make sure it is running */
				(*mac->gldm_start) (mac);
			}
			mac->gldm_nstreams++;
			mutex_exit(&mac->gldm_maclock);

			dlokack(q, mp, DL_ATTACH_REQ);
			return (GLDE_OK);
		}
	}
	return (DL_BADPPA);
}

/*
 * gldunattach(q, mp)
 * DLPI DL_DETACH_REQ
 * detaches the mac layer from the stream
 */
static int
gldunattach(queue_t *q, mblk_t *mp)
{
	gld_t  *gld = (gld_t *)q->q_ptr;
	glddev_t *glddev = gld->gld_device;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	int	state = gld->gld_state;
	int	i;

	if (state != DL_UNBOUND)
		return (DL_OUTSTATE);

	ASSERT(macinfo != NULL);
	rw_enter(&glddev->gld_rwlock, RW_WRITER);
	/* make sure no references to this gld for pcgld_sched */
	if (macinfo->gldm_last == gld) {
		macinfo->gldm_last = NULL;
	}
	gld->gld_mac_info = NULL;
	rw_exit(&glddev->gld_rwlock);

	/* cleanup remnants of promiscuous mode */
	if (gld->gld_flags & GLD_PROM_PHYS &&
	    --macinfo->gldm_nprom == 0) {
		mutex_enter(&macinfo->gldm_maclock);
		(*macinfo->gldm_prom) (macinfo, 0);
		mutex_exit(&macinfo->gldm_maclock);
	}

	/* cleanup mac layer if last stream */
	if (--macinfo->gldm_nstreams == 0) {
		mutex_enter(&macinfo->gldm_maclock);
		(*macinfo->gldm_stop) (macinfo);
		mutex_exit(&macinfo->gldm_maclock);
	}

	gld->gld_stats = NULL;

	if (gld->gld_mcast) {
		for (i = 0; i < glddev->gld_multisize; i++) {
			gld_mcast_t *mcast;

			if ((mcast = gld->gld_mcast[i]) != NULL) {
				/* disable from stream and possibly lower */
				gld_send_disable_multi(gld, gld->gld_mac_info,
							mcast);
				gld->gld_mcast[i] = NULL;
			}
		}
		kmem_free(gld->gld_mcast,
		    sizeof (gld_mcast_t *) * glddev->gld_multisize);
		gld->gld_mcast = NULL;
	}

	gld->gld_sap = 0;
	gld->gld_state = DL_UNATTACHED;
	if (mp) {
		dlokack(q, mp, DL_DETACH_REQ);
	}
	return (GLDE_OK);
}

/*
 * gld_enable_multi (q, mp)
 * enables multicast address on the stream if the mac layer
 * isn't enabled for this address, enable at that level as well.
 */
static int
gld_enable_multi(queue_t *q, mblk_t *mp)
{
	gld_t  *gld;
	glddev_t *glddev;
	gld_mac_info_t *macinfo;
	struct ether_addr *maddr;
	dl_enabmulti_req_t *multi;
	gld_mcast_t *mcast;
	int	status = DL_BADADDR;
	int	i;

#if defined(GLD_DEBUG)
	if (pcgld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gld_enable_multi(%x, %x)", (int)q, (int)mp);
	}
#endif

	gld = (gld_t *)q->q_ptr;
	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);

	if (macinfo->gldm_sdmulti == NULL) {
		return (DL_UNSUPPORTED);
	}

	glddev = macinfo->gldm_dev;
	multi = (dl_enabmulti_req_t *)mp->b_rptr;
	maddr = (struct ether_addr *)(multi + 1);

	/*
	 * check to see if this multicast address is valid if it is, then
	 * check to see if it is already in the per stream table and the per
	 * device table if it is already in the per stream table, if it isn't
	 * in the per device, add it.  If it is, just set a pointer.  If it
	 * isn't, allocate what's necessary.
	 */

	if (MBLKL(mp) >= sizeof (dl_enabmulti_req_t) &&
	    MBLKIN(mp, multi->dl_addr_offset, multi->dl_addr_length)) {
		if (!ismulticast(maddr)) {
			return (DL_BADADDR);
		}
		/* request appears to be valid */
		/* does this address appear in current table? */
		if (gld->gld_mcast == NULL) {
			/* no mcast addresses -- allocate table */
			gld->gld_mcast = GETSTRUCT(gld_mcast_t *,
						    glddev->gld_multisize);
			if (gld->gld_mcast == NULL)
				return (DL_SYSERR);
			gld->gld_multicnt = glddev->gld_multisize;
		} else {
			for (i = 0; i < glddev->gld_multisize; i++) {
				if (gld->gld_mcast[i] &&
/* *** should we add this test?	   gld->gld_mcast[i]->gldm_refcnt &&  *** */
				    bcmp((caddr_t)gld->gld_mcast[i]->gldm_addr,
					(caddr_t)maddr->ether_addr_octet,
					ETHERADDRL) == 0) {
					/* this is a match -- just succeed */
					dlokack(q, mp, DL_ENABMULTI_REQ);
					return (GLDE_OK);
				}
			}
		}
		/*
		 * there wasn't one so check to see if the mac layer has one
		 */
		if (macinfo->gldm_mcast == NULL) {
			macinfo->gldm_mcast = GETSTRUCT(gld_mcast_t,
							glddev->gld_multisize);
			if (macinfo->gldm_mcast == NULL)
				return (DL_SYSERR);
		}
		for (mcast = NULL, i = 0; i < glddev->gld_multisize; i++) {
			if (macinfo->gldm_mcast[i].gldm_refcnt &&
			    bcmp((caddr_t)macinfo->gldm_mcast[i].gldm_addr,
				(caddr_t)maddr->ether_addr_octet,
				ETHERADDRL) == 0) {
				mcast = &macinfo->gldm_mcast[i];
				break;
			}
		}
		if (mcast == NULL) {
			/* set mcast in hardware */
			mutex_enter(&macinfo->gldm_maclock);
			(*macinfo->gldm_sdmulti) (macinfo, maddr, 1);
			mutex_exit(&macinfo->gldm_maclock);
			/* find an empty slot to fill in */
			for (mcast = macinfo->gldm_mcast, i = 0;
			    i < glddev->gld_multisize; i++, mcast++) {
				if (mcast->gldm_refcnt == 0) {
				    bcopy((caddr_t)maddr->ether_addr_octet,
					(caddr_t)mcast->gldm_addr, ETHERADDRL);
					break;
				}
			}
		}
		if (mcast != NULL) {
			for (i = 0; i < glddev->gld_multisize; i++) {
				if (gld->gld_mcast[i] == NULL) {
					gld->gld_mcast[i] = mcast;
					mcast->gldm_refcnt++;
					dlokack(q, mp, DL_ENABMULTI_REQ);
					return (GLDE_OK);
				}
			}
		}
		status = DL_TOOMANY;
	}
	return (status);
}


/*
 * gld_disable_multi (q, mp)
 * disable the multicast address on the stream if last
 * reference for the mac layer, disable there as well
 */
static int
gld_disable_multi(queue_t *q, mblk_t *mp)
{
	gld_t  *gld;
	gld_mac_info_t *macinfo;
	struct ether_addr *maddr;
	dl_enabmulti_req_t *multi;
	int	status = DL_BADADDR, i;
	gld_mcast_t *mcast;

#if defined(GLD_DEBUG)
	if (pcgld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gld_enable_multi(%x, %x)", (int)q, (int)mp);
	}
#endif

	gld = (gld_t *)q->q_ptr;
	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	if (macinfo->gldm_sdmulti == NULL) {
		return (DL_UNSUPPORTED);
	}
	multi = (dl_enabmulti_req_t *)mp->b_rptr;
	maddr = (struct ether_addr *)(multi + 1);

	if (MBLKL(mp) >= sizeof (dl_enabmulti_req_t) &&
	    MBLKIN(mp, multi->dl_addr_offset, multi->dl_addr_length)) {
		/* request appears to be valid */
		/* does this address appear in current table? */
		if (gld->gld_mcast != NULL) {
			for (i = 0; i < gld->gld_multicnt; i++)
				if (((mcast = gld->gld_mcast[i]) != NULL) &&
				    mcast->gldm_refcnt &&
				    bcmp((caddr_t)mcast->gldm_addr,
					(caddr_t)maddr->ether_addr_octet,
					ETHERADDRL) == 0) {
					gld_send_disable_multi(gld, macinfo,
								mcast);
					gld->gld_mcast[i] = NULL;
					dlokack(q, mp, DL_DISABMULTI_REQ);
					return (GLDE_OK);
				}
			status = DL_NOTENAB; /* not an enabled address */
		}
	}
	return (status);
}

/*
 * gld_send_disable_multi(gld, macinfo, mcast)
 * this function is used to disable a multicast address if the reference
 * count goes to zero. The disable request will then be forwarded to the
 * lower stream.
 */

static void
gld_send_disable_multi(gld_t *gld, gld_mac_info_t *macinfo,
	gld_mcast_t *mcast)
{
#ifdef lint
	gld = gld;
#endif
	if (mcast == NULL) {
		return;
	}
	if (macinfo == NULL) {
		return;
	}
	if (--mcast->gldm_refcnt > 0)
		return;

	mutex_enter(&macinfo->gldm_maclock);
	(*macinfo->gldm_sdmulti) (macinfo, mcast->gldm_addr, 0);
	mutex_exit(&macinfo->gldm_maclock);
}

/*
 * gld_findminor(device)
 * searches the per device class list of STREAMS for
 * the first minor number not used.  Note that we currently don't allocate
 * minor 0.
 * This routine looks slow to me.
 */

static int
gld_findminor(glddev_t *device)
{
	gld_t  *next;
	int	minor;

	for (minor = GLD_PPA_INIT; minor > 0; minor++) {
		for (next = device->gld_str_next;
		    next != (gld_t *)&device->gld_str_next;
		    next = next->gld_next) {
			if (minor == next->gld_minor)
				goto nextminor;
		}
		return (minor);
nextminor:
		/* don't need to do anything */
		;
	}
	/*NOTREACHED*/
}

#ifdef i386

/*
 * gld_findppa(device)
 * searches the per device class list of device instances for
 * the first PPA number not used.
 *
 * This routine looks slow to me, but nobody cares, since it seldom executes.
 */

static int
gld_findppa(glddev_t *device)
{
	gld_mac_info_t	*next;
	int ppa;

	for (ppa = 0; ppa >= 0; ppa++) {
		for (next = device->gld_mac_next;
		    next != (gld_mac_info_t *)&device->gld_mac_next;
		    next = next->gldm_next) {
			if (ppa == next->gldm_ppa)
				goto nextppa;
		}
		return (ppa);
nextppa:
		/* don't need to do anything */
		;
	}
	/*NOTREACHED*/
}

#endif /* i386 */

/*
 * gld_form_udata(gld, macinfo, mp)
 * format a DL_UNITDATA_IND message to be sent to the user
 */
static void
gld_form_udata(gld_t *gld, gld_mac_info_t *macinfo, mblk_t *mp)
{
	mblk_t *udmp;
	register dl_unitdata_ind_t *udata;
	register struct ether_header *hdr;

	hdr = (struct ether_header *)mp->b_rptr;

	/* allocate the DL_UNITDATA_IND M_PROTO header */
	udmp = allocb(sizeof (dl_unitdata_ind_t) +
			2 * (macinfo->gldm_addrlen + 2), BPRI_MED);
	if (udmp == NULL) {
		/* might as well discard since we can't go further */
		freemsg(mp);
		return;
	}
	udata = (dl_unitdata_ind_t *)udmp->b_rptr;
	udmp->b_wptr += sizeof (dl_unitdata_ind_t);

	/* step past Ethernet header */
	mp->b_rptr += sizeof (struct ether_header);

	/*
	 * now setup the DL_UNITDATA_IND header
	 */
	DB_TYPE(udmp) = M_PROTO;
	udata->dl_primitive = DL_UNITDATA_IND;
	udata->dl_dest_addr_length = macinfo->gldm_addrlen +
		abs(macinfo->gldm_saplen);
	udata->dl_dest_addr_offset = sizeof (dl_unitdata_ind_t);
	bcopy((caddr_t)hdr->ether_dhost.ether_addr_octet,
	    (caddr_t)DLSAP(udata, udata->dl_dest_addr_offset)->glda_addr,
	    macinfo->gldm_addrlen);
	DLSAP(udata, udata->dl_dest_addr_offset)->glda_sap =
		ntohs(hdr->ether_type);
	udmp->b_wptr += udata->dl_dest_addr_length;
	udata->dl_src_addr_length = macinfo->gldm_addrlen +
		abs(macinfo->gldm_saplen);
	udata->dl_src_addr_offset = udata->dl_dest_addr_length +
		udata->dl_dest_addr_offset;
	bcopy((caddr_t)hdr->ether_shost.ether_addr_octet,
	    (caddr_t)DLSAP(udata, udata->dl_src_addr_offset)->glda_addr,
	    macinfo->gldm_addrlen);
	DLSAP(udata, udata->dl_src_addr_offset)->glda_sap =
		ntohs(hdr->ether_type);
	udata->dl_group_address = hdr->ether_dhost.ether_addr_octet[0] & 0x1;
	udmp->b_wptr += udata->dl_src_addr_length;
	linkb(udmp, mp);

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDRECV)
		cmn_err(CE_NOTE, "pcgld_recv: queued message to %x (%d)",
			(int)gld->gld_qptr, (int)gld->gld_minor);
#endif
	/* enqueue to the next layer */
	if (gld->gld_qptr->q_next == NULL && canputnext(gld->gld_qptr)) {
		(void) putnext(gld->gld_qptr, udmp);
	} else {
		(void) putq(gld->gld_qptr, udmp);
	}
}

/*
 * gld_physaddr()
 *	get the current or factory physical address value
 */
static int
gld_physaddr(queue_t *q, mblk_t *mp)
{
	gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	struct ether_addr addr;

	if (MBLKL(mp) < DL_PHYS_ADDR_REQ_SIZE) {
		return (DL_BADPRIM);
	}
	if (gld->gld_state == DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}

	macinfo = (gld_mac_info_t *)gld->gld_mac_info;
	ASSERT(macinfo != NULL);

	switch (prim->physaddr_req.dl_addr_type) {
	case DL_FACT_PHYS_ADDR:
		bcopy((caddr_t)macinfo->gldm_vendor, (caddr_t)&addr,
			sizeof (addr));
		break;
	case DL_CURR_PHYS_ADDR:
		bcopy((caddr_t)macinfo->gldm_macaddr, (caddr_t)&addr,
			sizeof (addr));
		break;
	default:
		return (DL_BADPRIM);
	}
	dlphysaddrack(q, mp, (caddr_t)&addr, macinfo->gldm_addrlen);
	return (GLDE_OK);
}

/*
 * gld_setaddr()
 *	change the hardware's physical address to a user specified value
 */
static int
gld_setaddr(queue_t *q, mblk_t *mp)
{
	gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	struct ether_addr *addr;

	if (MBLKL(mp) < DL_SET_PHYS_ADDR_REQ_SIZE) {
		return (DL_BADPRIM);
	}
	if (gld->gld_state == DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}

	macinfo = (gld_mac_info_t *)gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	if (prim->set_physaddr_req.dl_addr_length != macinfo->gldm_addrlen) {
		return (DL_BADADDR);
	}

	/*
	 * TBD:
	 * Are there any bound streams other than the one I'm on?
	 * If so, disallow the set.
	 */

	/* now do the set at the hardware level */
	addr = (struct ether_addr *)
		(mp->b_rptr + prim->set_physaddr_req.dl_addr_offset);
	bcopy((caddr_t)addr, (caddr_t)macinfo->gldm_macaddr,
		macinfo->gldm_addrlen);

	mutex_enter(&macinfo->gldm_maclock);
	(*macinfo->gldm_saddr) (macinfo);
	mutex_exit(&macinfo->gldm_maclock);

	dlokack(q, mp, DL_SET_PHYS_ADDR_REQ);
	return (GLDE_OK);
}

/*
 * gld_promisc (q, mp, on)
 *	enable or disable the use of promiscuous mode with the hardware
 */
static int
gld_promisc(queue_t *q, mblk_t *mp, int on)
{
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	int	mask;
	gld_t  *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;
	int result = DL_NOTSUPPORTED;

#ifdef GLD_DEBUG
	if (pcgld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_promisc(%x, %x, %x)", (int)q,
			(int)mp, (int)on);
#endif

	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);

	switch (prim->promiscon_req.dl_level) {
	case DL_PROMISC_PHYS:
		mask = GLD_PROM_PHYS;
		break;
	case DL_PROMISC_SAP:
		mask = GLD_PROM_SAP;
		break;
	case DL_PROMISC_MULTI:
		mask = GLD_PROM_MULT;
		break;
	default:
		mask = 0;	/* this is an error */
		result = DL_UNSUPPORTED;
	}
	if (mask) {
		if (on) {
			if (mask & GLD_PROM_PHYS &&
			    !(gld->gld_flags & GLD_PROM_PHYS)) {
				if (macinfo->gldm_nprom == 0) {
					mutex_enter(&macinfo->gldm_maclock);
					(*macinfo->gldm_prom) (macinfo, 1);
					mutex_exit(&macinfo->gldm_maclock);
				}
				macinfo->gldm_nprom++;
			}
			gld->gld_flags |= mask;
			dlokack(q, mp, DL_PROMISCON_REQ);
			return (GLDE_OK);
		} else {
		    if ((gld->gld_flags & mask) != mask) {
			result = DL_NOTENAB;
			goto err;
		    } else if (gld->gld_flags & mask) {
			gld->gld_flags &= ~mask;
			if (mask & GLD_PROM_PHYS) {
				macinfo->gldm_nprom--;
				if (macinfo->gldm_nprom == 0) {
					mutex_enter(&macinfo->gldm_maclock);
					(*macinfo->gldm_prom) (macinfo, 0);
					mutex_exit(&macinfo->gldm_maclock);
				}
			}
			if (mp != NULL)
				dlokack(q, mp, DL_PROMISCOFF_REQ);
			return (GLDE_OK);
		    }
		}
	    /* already in requested state; simply return success */
	    if (mp != NULL)
		dlokack(q, mp, on ? DL_PROMISCON_REQ : DL_PROMISCOFF_REQ);
	    return (GLDE_OK);
	}

err:
	return (result);
}

/*
 * pcgld_sched (macinfo)
 *
 * This routine scans the streams that refer to a specific macinfo
 * structure and causes the STREAMS scheduler to try to run them if
 * they are marked as waiting for the transmit buffer.	The first such
 * message found will be queued to the hardware if possible.
 *
 * This routine is called at interrupt time after each interrupt is
 * delivered to the driver; it should be made to run Fast.
 *
 * This routine is called with the maclock mutex set
 */
int
pcgld_sched(gld_mac_info_t *macinfo)
{
	register gld_t *gld, *first;
	mblk_t *mp, *nmp;
	glddev_t *glddev = macinfo->gldm_dev;

	ASSERT(macinfo != NULL);
	ASSERT(glddev != NULL);

	rw_enter(&glddev->gld_rwlock, RW_READER);

	gld = macinfo->gldm_last;  /* The last one on which we failed */
	if (gld == NULL)
		gld = glddev->gld_str_next;	/* start with the first one */

	first = gld;		/* Remember where we started this time */
	do {
		if (gld == (gld_t *)&(glddev->gld_str_next))
			continue;	/* This is the list head, not a gld_t */
		if (gld->gld_mac_info != macinfo)
			continue;	/* This is not our device */
		if (gld->gld_flags & GLD_XWAIT) {
			if ((mp = getq(WR(gld->gld_qptr))) == NULL) {
				/* nothing here, clear XWAIT */
				gld->gld_flags &= ~GLD_XWAIT;
				continue; /* try another Q */
			}
			if (DB_TYPE(mp) != M_DATA) {
				/*
				 * We can't help here -- put the mp
				 * back on the Q, which automatically
				 * does a qenable.
				 */
				(void) putbq(WR(gld->gld_qptr), mp);
				gld->gld_flags &= ~GLD_XWAIT;
				continue; /* try another Q */
			}

			/* want to loop back if doing promiscuous mode */
			if (macinfo->gldm_nprom > 0)
				nmp = dupmsg(mp);
			else
				nmp = NULL;

			/* Here's a packet to try to send */
			mutex_enter(&macinfo->gldm_maclock);
			if ((*macinfo->gldm_send) (macinfo, mp)) {
				/*
				 * The board refused our send -- we
				 * will retry later.  We leave XWAIT
				 * set and arrange to be first next
				 * time.  The putbq does a qenable,
				 * so we can save a few cycles here.
				 */
				mutex_exit(&macinfo->gldm_maclock);
				macinfo->gldm_stats.glds_xmtretry++;
				(void) putbq(WR(gld->gld_qptr), mp);
				if (nmp)
					freemsg(nmp);
				/* remember where we left off */
				macinfo->gldm_last = gld;
				break;

			} else {
				mutex_exit(&macinfo->gldm_maclock);
				/*
				 * Send worked -- we enable the Q so
				 * the wsrv will run, but we leave
				 * XWAIT set so this routine can try
				 * more on this Q later.  We will try
				 * to service other Queues first, though.
				 */
				qenable(WR(gld->gld_qptr));
				macinfo->gldm_stats.glds_pktxmt++;
				macinfo->gldm_stats.glds_bytexmt +=
						msgdsize(mp);
				freemsg(mp);
				/* want to loop back if promiscuous mode */
				if (nmp != NULL) {
					(void) pcgld_recv(macinfo, nmp);
				}
				continue; /* try another Q */
			}
		}
	} while ((gld = gld->gld_next) != first);	/* until we come */
							/* full circle */
	rw_exit(&glddev->gld_rwlock);
	return (0);
}

/*
 * gld_flushqueue (q)
 *	used by DLPI primitives that require flushing the queues.
 *	essentially, this is DL_UNBIND_REQ.
 */
static void
gld_flushqueue(queue_t *q)
{
	/* flush all data in both queues */
	flushq(q, FLUSHDATA);
	flushq(WR(q), FLUSHDATA);
	/* flush all the queues upstream */
	(void) putctl1(q, M_FLUSH, FLUSHRW);
}

/*
 * pcgld_intr (macinfo)
 *	run all interrupt handlers through here in order to simplify
 *	the mutex code and speed up scheduling.
 */
u_int
pcgld_intr(gld_mac_info_t *macinfo)
{
	int claimed;
#if 0
	mblk_t *mp;
#endif
	ASSERT(macinfo != NULL);
	if (!(macinfo->gldm_GLD_flags & GLD_INTR_READY))
		return (DDI_INTR_UNCLAIMED);   /* our mutex isn't inited yet! */

	mutex_enter(&macinfo->gldm_maclock);
	claimed = (*macinfo->gldm_intr)(macinfo);
#if 0
	while (macinfo->gldm_rcvq != NULL) {
		mp = macinfo->gldm_rcvq;
		macinfo->gldm_rcvq = mp->b_next;
		mp->b_next = NULL;
		gld_recv_process(macinfo, mp);
	}
#endif
	mutex_exit(&macinfo->gldm_maclock);
	if (claimed == DDI_INTR_CLAIMED)
		(void) pcgld_sched(macinfo);
	return (claimed);
}

/*
 * pcgld_intr_hi (macinfo)
 *	run all interrupt handlers through here in order to simplify
 *	the mutex code and speed up scheduling.  This part simplifies
 *	split interrupt handlers.
 */
u_int
pcgld_intr_hi(gld_mac_info_t *macinfo)
{
	int claimed;
	ASSERT(macinfo != NULL);
	if (!(macinfo->gldm_GLD_flags & GLD_INTR_READY) ||
	    macinfo->gldm_intr_hi == NULL) {
		return (DDI_INTR_UNCLAIMED);
	}
	claimed = (*macinfo->gldm_intr_hi)(macinfo);
	if (claimed == DDI_INTR_CLAIMED &&
	    macinfo->gldm_GLD_flags & GLD_INTR_SOFT)
		ddi_trigger_softintr(macinfo->gldm_softid);
	return (claimed);
}

/*
 * gld_devlookup (major)
 * search the device table for the device with specified
 * major number and return a pointer to it if it exists
 */
static glddev_t *
gld_devlookup(int major)
{
	struct glddevice *dev;

	for (dev = gld_device_list.gld_next;
	    dev != &gld_device_list;
	    dev = dev->gld_next) {
		ASSERT(dev);
		if (dev->gld_major == major)
			return (dev);
	}
	return (NULL);
}

/*
 * pcgldcrc32 (addr)
 * this provides a common multicast hash algorithm by doing a
 * 32bit CRC on the 6 octets of the address handed in.	Used by the National
 * chip set for Ethernet for multicast address filtering.  May be used by
 * others as well.
 */

ulong_t
pcgldcrc32(uchar_t *addr)
{
	register int i, j;
	union gldhash crc;
	unsigned char fb, ch;

	crc.value = (ulong_t)0xFFFFFFFF; /* initialize as the HW would */

	for (i = 0; i < LLC_ADDR_LEN; i++) {
		ch = addr[i];
		for (j = 0; j < 8; j++) {
			fb = crc.bits.a31 ^ ((ch >> j) & 0x01);
			crc.bits.a25 ^= fb;
			crc.bits.a22 ^= fb;
			crc.bits.a21 ^= fb;
			crc.bits.a15 ^= fb;
			crc.bits.a11 ^= fb;
			crc.bits.a10 ^= fb;
			crc.bits.a9 ^= fb;
			crc.bits.a7 ^= fb;
			crc.bits.a6 ^= fb;
			crc.bits.a4 ^= fb;
			crc.bits.a3 ^= fb;
			crc.bits.a1 ^= fb;
			crc.bits.a0 ^= fb;
			crc.value = (crc.value << 1) | fb;
		}
	}
	return (crc.value);
}

/*
 * version of insque/remque for use by this driver
 */
struct qelem {
	struct qelem *q_forw;
	struct qelem *q_back;
	/* rest of structure */
};

static void
gldinsque(void *elem, void *pred)
{
	register struct qelem *pelem = elem;
	register struct qelem *ppred = pred;
	register struct qelem *pnext = ppred->q_forw;

	pelem->q_forw = pnext;
	pelem->q_back = ppred;
	ppred->q_forw = pelem;
	pnext->q_back = pelem;
}

static void
gldremque(void *arg)
{
	register struct qelem *pelem = arg;
	register struct qelem *elem = arg;

	pelem->q_forw->q_back = pelem->q_back;
	pelem->q_back->q_forw = pelem->q_forw;
	elem->q_back = elem->q_forw = NULL;
}

/* VARARGS */
static void
glderror(dip, fmt, a1, a2, a3, a4, a5, a6)
	dev_info_t *dip;
	char   *fmt, *a1, *a2, *a3, *a4, *a5, *a6;
{
	static long last;
	static char *lastfmt;

	/*
	 * Don't print same error message too often.
	 */
	if ((last == (hrestime.tv_sec & ~1)) && (lastfmt == fmt))
		return;
	last = hrestime.tv_sec & ~1;
	lastfmt = fmt;

	cmn_err(CE_CONT, "%s%d:  ",
		ddi_get_name(dip), ddi_get_instance(dip));
	cmn_err(CE_CONT, fmt, a1, a2, a3, a4, a5, a6);
	cmn_err(CE_CONT, "\n");
}

#ifdef notdef
/* debug helpers -- remove in final version */
gld_dumppkt(mp)
	mblk_t *mp;
{
	int	i, total = 0, len;

	while (mp != NULL && total < 64) {
		cmn_err(CE_NOTE, "mb type %d len %d (%x/%x)", DB_TYPE(mp),
			MBLKL(mp), mp->b_rptr, mp->b_wptr);
		for (i = 0, len = MBLKL(mp); i < len; i++)
			cmn_err(CE_CONT, " %x", mp->b_rptr[i]);
		total += len;
		cmn_err(CE_CONT, "\n");
		mp = mp->b_cont;
	}
}
#endif

#ifdef notdef
void
gldprintf(str, a, b, c, d, e, f, g, h, i)
	char   *str;
{
	char	buff[256];

	(void) sprintf(buff, str, a, b, c, d, e, f, g, h, i);
	gld_prints(buff);
}
#endif

#ifdef notdef
void
gld_prints(char *s)
{
	if (!s)
		return;		/* sanity check for s == 0 */
	while (*s)
		cnputc (*s++, 0);
}
#endif

static void
gld_fastpath(gld_t *gld, queue_t *q, mblk_t *mp)
{
	dl_unitdata_req_t *udp;
	mblk_t *nmp;
	int len;

	/* sanity check - we want correct state and valid message */
	if (gld->gld_state != DL_IDLE || (mp->b_cont == NULL) ||
	    ((dl_unitdata_req_t *)(mp->b_cont->b_rptr))->dl_primitive !=
	    DL_UNITDATA_REQ) {
		miocnak(q, mp, 0, EINVAL);
		return;
	}

	udp = (dl_unitdata_req_t *)(mp->b_cont->b_rptr);

	len = gld->gld_mac_info->gldm_addrlen +
		abs(gld->gld_mac_info->gldm_saplen);
	if (!MBLKIN(mp->b_cont, udp->dl_dest_addr_offset,
	    udp->dl_dest_addr_length) ||
	    udp->dl_dest_addr_length != len) {
		miocnak(q, mp, 0, EINVAL);
		return;
	}

	switch (gld->gld_mac_info->gldm_type) {
	case DL_ETHER:
		{
			struct ether_header *hdr;

			nmp = allocb(sizeof (struct ether_header), BPRI_MED);
			if (nmp == NULL) {
				miocnak(q, mp, 0, ENOMEM);
				break;
			}
			nmp->b_wptr += sizeof (struct ether_header);
			hdr = (struct ether_header *)(nmp->b_rptr);
			bcopy((caddr_t)mp->b_cont->b_rptr +
			    udp->dl_dest_addr_offset,
			    (caddr_t)&hdr->ether_dhost, ETHERADDRL);
			bcopy((caddr_t)gld->gld_mac_info->gldm_macaddr,
			    (caddr_t)&hdr->ether_shost, ETHERADDRL);
			hdr->ether_type = htons((short)gld->gld_sap);
			linkb(mp, nmp);
			gld->gld_flags |= GLD_FAST;

			miocack(q, mp, msgdsize(mp->b_cont), 0);
		}
		break;
	default:
		miocnak(q, mp, 0, EINVAL);
		break;
	}
}
