/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gld.c	1.51	98/11/13 SMI"

/*
 * gld - Generic LAN Driver Version 2, PSARC/1997/382
 *
 * This is a utility module that provides generic facilities for
 * LAN	drivers.  The DLPI protocol and most STREAMS interfaces
 * are handled here.
 *
 * This revision of GLD supports the GLD v2 interface specified in
 * the case materials for PSARC/1997/382.
 *
 * It also provides source and binary compatibility with drivers
 * implemented according to the GLD v0 documentation published
 * in 1993.  It further maintains a level of compatibility with
 * some of the unpublished extensions to GLD v0 that provided
 * support for DMA devices and reduction of transmit interrupts.
 */


#ifdef	DEBUG
#define	TNF_DEBUG 1
#endif

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/kstat.h>
#include <sys/debug.h>

#include <sys/byteorder.h>
#include <sys/strsun.h>
#include <sys/dlpi.h>
#include <sys/ethernet.h>
#include <sys/tnf_probe.h>

#ifndef _PRE_SOLARIS_2_6
#include <sys/atomic.h>
#else
#define	atomic_add_32(x, y)	/* We'll be OK without this */
#define	membar_enter()
#define	membar_exit()
#define	membar_producer()
#define	membar_consumer()
#endif

#include <sys/gld.h>
#include <sys/gldpriv.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>

#ifndef	NPROBE
#define	BUG_4183496	/* Need tnf_mod_load/unload workaround */
#ifdef	BUG_4183496
extern int tnf_mod_load(void);
extern int tnf_mod_unload(struct modlinkage *mlp);
#endif
#endif


#ifdef GLD_DEBUG
int gld_debug = GLDERRS;
#endif

/* called from gld_register */
static int gld_initstats(gld_mac_info_t *);

/* called from kstat mechanism, and from wsrv's get_statistics */
static int gld_update_kstat(kstat_t *, int);

/* called from gld_getinfo */
static dev_info_t *gld_finddevinfo(dev_t);

/* called from wput, wsrv, unidata, and v0_sched to send a packet */
/* also from the source routing stuff for sending RDE protocol packets */
int gld_start(queue_t *, mblk_t *, int);

/* called from gld_sched at interrupt time for v0 drivers */
static void gld_v0_sched(gld_mac_info_t *);

/* called from gld_start to loopback a packet in promiscuous mode */
static void gld_precv(gld_mac_info_t *, mblk_t *);

/* receive group: called from gld_recv and gld_precv with mutex held */
static void gld_sendup(gld_mac_info_t *, mblk_t *, int (*)());
static int gld_accept(gld_t *, pktinfo_t *);
static int gld_mcmatch(gld_t *, pktinfo_t *);
static int gld_multicast(unsigned char *, gld_t *);
static int gld_paccept(gld_t *, pktinfo_t *);
static void gld_passon(gld_t *, mblk_t *, pktinfo_t *,
    void (*)(queue_t *, mblk_t *));
static mblk_t *gld_addudind(gld_t *, mblk_t *, pktinfo_t *);

/* wsrv group: called from wsrv, single threaded per queue */
static int gld_ioctl(queue_t *, mblk_t *);
static void gld_fastpath(gld_t *, queue_t *, mblk_t *);
static int gld_cmds(queue_t *, mblk_t *);
static int gld_bind(queue_t *, mblk_t *);
static int gld_unbind(queue_t *, mblk_t *);
static int gld_inforeq(queue_t *, mblk_t *);
static int gld_unitdata(queue_t *, mblk_t *);
static int gldattach(queue_t *, mblk_t *);
static int gldunattach(queue_t *, mblk_t *);
static int gld_enable_multi(queue_t *, mblk_t *);
static int gld_disable_multi(queue_t *, mblk_t *);
static void gld_send_disable_multi(gld_t *, gld_mac_info_t *, gld_mcast_t *);
static int gld_promisc(queue_t *, mblk_t *, int);
static int gld_physaddr(queue_t *, mblk_t *);
static int gld_setaddr(queue_t *, mblk_t *);
static int gld_get_statistics(queue_t *, mblk_t *);

/* misc utilities, some requiring various mutexes held */
static int gld_start_mac(gld_mac_info_t *);
static void gld_set_ipq(gld_mac_info_t *);
static void gld_flushqueue(queue_t *);
static glddev_t *gld_devlookup(int);
static int gld_findppa(glddev_t *);
static int gld_findminor(glddev_t *);
static void gldinsque(void *, void *);
static void gldremque(void *);
void gld_bitrevcopy(caddr_t, caddr_t, size_t);
void gld_bitreverse(u_char *, size_t);
char *gld_macaddr_sprintf(char *, unsigned char *, int);

#ifdef GLD_DEBUG
static void gld_check_assertions(void);
extern void gld_sr_dump(gld_mac_info_t *);
#endif

#ifdef _PRE_SOLARIS_2_6
extern void dlphysaddrack(queue_t *, mblk_t *, caddr_t, int);
extern int dluderrorind(queue_t *, mblk_t *, u_char *, u_long, u_long, int);
extern void dlokack(queue_t *, mblk_t *, u_long);
extern void dlbindack(queue_t *, mblk_t *, u_long, u_char *, int, int, u_long);
extern void dlerrorack(queue_t *, mblk_t *, u_long, u_long, u_long);
#endif

/*
 * Allocate and zero-out "number" structures each of type "structure" in
 * kernel memory.
 */
#define	GETSTRUCT(structure, number)   \
	((structure *) kmem_zalloc(\
		(u_int) (sizeof (structure) * (number)), KM_NOSLEEP))

#define	abs(a) ((a) < 0 ? -(a) : a)

#ifdef BUG_4236795
uint32_t gld_global_options = 0;
#endif
uint32_t gld_global_options = GLD_OPT_NO_ETHRXSNAP;

#ifdef	GLD_V0_SUPPORT
uchar_t gldbroadcastaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#endif

/*
 * DLPI media types supported
 */
static char *gld_types[] = {
	"csmacd",	/* DL_CSMACD	 IEEE 802.3 CSMA/CD network */
	"tpb",		/* DL_TPB	 IEEE 802.4 Token Passing Bus */
	"tpr",		/* DL_TPR	 IEEE 802.5 Token Passing Ring */
	"metro",	/* DL_METRO	 IEEE 802.6 Metro Net */
	"ether",	/* DL_ETHER	 Ethernet Bus */
	"hdlc",		/* DL_HDLC	 ISO HDLC protocol support */
	"char",		/* DL_CHAR	 Character Synchronous protocol */
	"ctca",		/* DL_CTCA	 IBM Channel-to-Channel Adapter */
	"fddi",		/* DL_FDDI	 Fiber Distributed data interface */
	"other",	/* DL_OTHER	 Any other medium not listed above */
};

/* Media must correspond to #defines in gld.h */
static char *gld_media[] = {
	"unknown",	/* GLDM_UNKNOWN - driver cannot determine media */
	"aui",		/* GLDM_AUI */
	"bnc",		/* GLDM_BNC */
	"twpair",	/* GLDM_TP */
	"fiber",	/* GLDM_FIBER */
	"100baseT",	/* GLDM_100BT */
	"100vgAnyLan",	/* GLDM_VGANYLAN */
	"10baseT",	/* GLDM_10BT */
	"ring4",	/* GLDM_RING4 */
	"ring16",	/* GLDM_RING16 */
	"PHY/MII",	/* GLDM_PHYMII */
	"100baseTX",	/* GLDM_100BTX */
	"100baseT4",	/* GLDM_100BT4 */
};

/* Must correspond to #defines in gld.h */
static char *gld_duplex[] = {
	"unknown",	/* GLD_DUPLEX_UNKNOWN - not known or not applicable */
	"half",		/* GLD_DUPLEX_HALF */
	"full"		/* GLD_DUPLEX_FULL */
};

int gld_interpret_ether(gld_mac_info_t *, mblk_t *, pktinfo_t *, int);
int gld_interpret_fddi(gld_mac_info_t *, mblk_t *, pktinfo_t *, int);
int gld_interpret_tr(gld_mac_info_t *, mblk_t *, pktinfo_t *, int);

mblk_t *gld_fastpath_ether(gld_t *, mblk_t *);
mblk_t *gld_fastpath_fddi(gld_t *, mblk_t *);
mblk_t *gld_fastpath_tr(gld_t *, mblk_t *);

mblk_t *gld_unitdata_ether(gld_t *, mblk_t *);
mblk_t *gld_unitdata_fddi(gld_t *, mblk_t *);
mblk_t *gld_unitdata_tr(gld_t *, mblk_t *);

void gld_init_ether(gld_mac_info_t *);
void gld_init_fddi(gld_mac_info_t *);
void gld_init_tr(gld_mac_info_t *);

void gld_uninit_ether(gld_mac_info_t *);
void gld_uninit_fddi(gld_mac_info_t *);
void gld_uninit_tr(gld_mac_info_t *);

/*
 * Interface types currently supported by GLD.
 * If you add new types, you must check all "XXX" strings in the GLD source
 * for implementation issues that may affect the support of your new type.
 * In particular, any type with gldm_addrlen > 6, or gldm_saplen != -2, will
 * require generalizing this GLD source to handle the new cases.  In other
 * words there are assumptions built into the code in a few places that must
 * be fixed.  Be sure to turn on DEBUG/ASSERT code when testing a new type.
 */
static gld_interface_t interfaces[] = {

	/* Ethernet Bus */
	{
		DL_ETHER, 1514,
		gld_interpret_ether, gld_fastpath_ether, gld_unitdata_ether,
		gld_init_ether, gld_uninit_ether,
		IF_HDR_FIXED
	},

	/* Fiber Distributed data interface */
	{
		DL_FDDI, 4500,
		gld_interpret_fddi, gld_fastpath_fddi, gld_unitdata_fddi,
		gld_init_fddi, gld_uninit_fddi,
		IF_HDR_VAR
	},

	/* Token Ring interface */
	{
		DL_TPR, 18200,
		gld_interpret_tr, gld_fastpath_tr, gld_unitdata_tr,
		gld_init_tr, gld_uninit_tr,
		IF_HDR_VAR
	},

};

/*
 * bit reversal lookup table.
 */
static	uchar_t bit_rev[] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0,
	0x30, 0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4,
	0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc,
	0x3c, 0xbc, 0x7c, 0xfc, 0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 0x0a, 0x8a, 0x4a, 0xca,
	0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6,
	0x36, 0xb6, 0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81, 0x41, 0xc1,
	0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9,
	0x39, 0xb9, 0x79, 0xf9, 0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd,
	0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3,
	0x33, 0xb3, 0x73, 0xf3, 0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7,
	0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf,
	0x3f, 0xbf, 0x7f, 0xff,
};

static struct glddevice gld_device_list;  /* Per-system root of GLD tables */

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modlmisc = {
	&mod_miscops,		/* Type of module - a utility provider */
	"Generic LAN Driver (" GLD_VERSION_STRING ")"
#ifdef GLD_DEBUG
	" DEBUG"
#endif
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlmisc, NULL
};

int
_init(void)
{
	register int e;

#ifdef	BUG_4183496
	(void) tnf_mod_load();
#endif

	/* initialize gld_device_list mutex */
	mutex_init(&gld_device_list.gld_devlock, NULL, MUTEX_DRIVER, NULL);

	/* initialize device driver (per-major) list */
	gld_device_list.gld_next =
	    gld_device_list.gld_prev = &gld_device_list;

	if ((e = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&gld_device_list.gld_devlock);
#ifdef	BUG_4183496
		(void) tnf_mod_unload(&modlinkage);
#endif
	}

	return (e);
}

int
_fini(void)
{
	register int e;

	if ((e = mod_remove(&modlinkage)) != 0)
		return (e);

#ifdef	BUG_4183496
	(void) tnf_mod_unload(&modlinkage);
#endif

	ASSERT(gld_device_list.gld_next ==
	    (glddev_t *)&gld_device_list.gld_next);
	ASSERT(gld_device_list.gld_prev ==
	    (glddev_t *)&gld_device_list.gld_next);
	mutex_destroy(&gld_device_list.gld_devlock);

	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * GLD service routines
 */

/* So this gld binary maybe can be forward compatible with future v2 drivers */
#define	GLD_MAC_RESERVED (16 * sizeof (caddr_t))

gld_mac_info_t *
gld_mac_alloc(dev_info_t *devinfo)
{
	gld_mac_info_t *macinfo =
	    (gld_mac_info_t *)kmem_zalloc(sizeof (gld_mac_info_t) +
	    GLD_MAC_RESERVED, KM_SLEEP);
#ifdef lint
	devinfo = devinfo;
#endif

	TNF_PROBE_2(gld_mac_alloc, "gld gld_config gld_9f", /* */,
	    tnf_opaque, devinfo, devinfo,
	    tnf_opaque, returns_macinfo, macinfo);

	if (macinfo == NULL)
		return (NULL);

	/*
	 * v0 drivers don't call this function; if this gets called, then
	 * the caller is at least a v2 driver.  Assume v2, unless the driver
	 * sets gldm_driver_version to something greater.  The setting of
	 * gldm_driver_version will not be documented or allowed until a
	 * future release.
	 */
	macinfo->gldm_driver_version = GLD_VERSION_200;

	/*
	 * GLD's version.  This also is undocumented for now, but will be
	 * available if needed in the future.
	 */
	macinfo->gldm_GLD_version = GLD_VERSION;

	return (macinfo);
}

/*
 * gld_mac_free must be called after the v2 driver has removed interrupts
 * and completely stopped calling gld_recv() and gld_sched().  At that
 * point the interrupt routine is guaranteed by the system to have been
 * exited and the maclock mutex is no longer needed.  Of course, it is
 * expected (required) that (assuming gld_register() succeeded),
 * gld_unregister() was called before gld_mac_free().
 */
void
gld_mac_free(gld_mac_info_t *macinfo)
{
	ASSERT(macinfo);
	ASSERT(macinfo->gldm_GLD_version == GLD_VERSION);

	TNF_PROBE_1(gld_mac_free, "gld gld_config gld_9f", /* */,
	    tnf_opaque, macinfo, macinfo);

	/* Did we make it through gld_register? */
	if (macinfo->gldm_GLD_flags & GLD_MUTEX_INITED) {
		/* Yes, we better have also unregistered */
		ASSERT(macinfo->gldm_GLD_flags & GLD_UNREGISTERED);
		mutex_destroy(&macinfo->gldm_maclock);
	}

	kmem_free(macinfo, sizeof (gld_mac_info_t) + GLD_MAC_RESERVED);
}

/*
 * gld_register -- called once per device instance (PPA)
 *
 * During its attach routine, a real device driver will register with GLD
 * so that later opens and dl_attach_reqs will work.  The arguments are the
 * devinfo pointer, the device name, and a macinfo structure describing the
 * physical device instance.
 */
int
gld_register(dev_info_t *devinfo, char *devname, gld_mac_info_t *macinfo)
{
	int nintrs = 0, nregs = 0;
	int mediatype;
	int major = ddi_name_to_major(devname), i;
	glddev_t *glddev;
	gld_mac_pvt_t *mac_pvt;
	char minordev[32];
	char pbuf[3*GLD_MAX_ADDRLEN];

	ASSERT(devinfo != NULL);
	ASSERT(macinfo != NULL);

	TNF_PROBE_5(gld_register, "gld gld_config gld_9f", /* */,
	    tnf_opaque, devinfo, devinfo,
	    tnf_string, devname, devname,
	    tnf_int, instance, ddi_get_instance(devinfo),
	    tnf_int, driver_version, macinfo->gldm_driver_version,
	    tnf_opaque, macinfo, macinfo);

	if (macinfo->gldm_driver_version > GLD_VERSION) {
		cmn_err(CE_WARN, "GLD: %s driver version %x cannot register "
		    "with GLD version %x", devname,
		    macinfo->gldm_driver_version, GLD_VERSION);
		return (DDI_FAILURE);
	}

	mediatype =
	    (macinfo->gldm_type < (sizeof (gld_types)/sizeof (char *))) ?
		macinfo->gldm_type : DL_OTHER;

	if (macinfo->gldm_driver_version < GLD_VERSION_200) {
		/* v0 only supports ethernet and tokenring */
		if (macinfo->gldm_type != DL_ETHER &&
		    macinfo->gldm_type != DL_TPR) {
			cmn_err(CE_WARN, "GLD: does not support v0 %s driver "
			    "of type %s, v0 only supports DL_ETHER and DL_TPR",
			    devname, gld_types[mediatype]);
			return (DDI_FAILURE);
		}
		macinfo->gldm_GLD_version = GLD_VERSION;
		macinfo->gldm_vendor_addr = macinfo->gldm_vendor;
		macinfo->gldm_broadcast_addr = macinfo->gldm_broadcast;
		macinfo->gldm_devinfo = devinfo;
		if (macinfo->gldm_ident == NULL)
			macinfo->gldm_ident = "LAN Driver";
	} else {
#ifdef GLD_DEBUG
		/* Old v0 cruft should be unused */
		if (macinfo->gldm_flags != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_flags != 0", devname);
		if (macinfo->gldm_state != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_state != 0", devname);
		if (macinfo->gldm_port != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_port != 0", devname);
		if (macinfo->gldm_media != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_media != 0", devname);
		if (macinfo->gldm_reg_offset != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_reg_offset != 0", devname);
		if (macinfo->gldm_memp != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_memp != 0", devname);
		if (macinfo->gldm_reg_len != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_reg_len != 0", devname);
		if (macinfo->gldm_options != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_options != 0", devname);
		if (macinfo->gldm_reg_index != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_reg_index != 0", devname);
		if (macinfo->gldm_irq_index != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_irq_index != 0", devname);
		if (*(uchar_t *)(macinfo->gldm_macaddr) != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_macaddr != 0", devname);
		if (*(uchar_t *)(macinfo->gldm_vendor) != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_vendor != 0", devname);
		if (*(uchar_t *)(macinfo->gldm_broadcast) != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_broadcast != 0", devname);

		/* v2 driver should set these */
		if (macinfo->gldm_devinfo != devinfo)
			cmn_err(CE_WARN, "GLD register: v2 %s driver:"
			    " macinfo->gldm_devinfo != passed dip", devname);
#endif
		ASSERT(macinfo->gldm_set_multicast != NULL);	/* v2 */
		ASSERT(macinfo->gldm_get_stats != NULL);	/* v2 */

		macinfo->gldm_irq_index = -1;
		macinfo->gldm_reg_index = -1;
		macinfo->gldm_options = GLDOPT_DONTFREE | GLDOPT_DRIVER_PPA;
	}

	/*
	 * Entry points should be ready for us.
	 * ioctl is optional.
	 * set_multicast and get_stats are optional in v0.
	 * intr is only required if you add an interrupt.
	 */
	ASSERT(macinfo->gldm_reset != NULL);
	ASSERT(macinfo->gldm_start != NULL);
	ASSERT(macinfo->gldm_stop != NULL);
	ASSERT(macinfo->gldm_set_mac_addr != NULL);
	ASSERT(macinfo->gldm_set_promiscuous != NULL);
	ASSERT(macinfo->gldm_send != NULL);

	ASSERT(macinfo->gldm_devinfo != NULL);
	ASSERT(macinfo->gldm_maxpkt >= macinfo->gldm_minpkt);
	ASSERT(macinfo->gldm_GLD_version == GLD_VERSION);
	ASSERT(macinfo->gldm_broadcast_addr != NULL);
	ASSERT(macinfo->gldm_vendor_addr != NULL);
	ASSERT(macinfo->gldm_ident != NULL);

	/* All currently supported media use addrlen == 6, saplen == -2 */
	/* XXX GLD does not function properly with saplen != -2 at this time */
	if (macinfo->gldm_addrlen > GLD_MAX_ADDRLEN) {
		cmn_err(CE_WARN, "GLD: %s driver gldm_addrlen %d > %d not sup"
		    "ported", devname, macinfo->gldm_addrlen, GLD_MAX_ADDRLEN);
		return (DDI_FAILURE);
	}
	if (macinfo->gldm_saplen != -2) {
		cmn_err(CE_WARN, "GLD: %s driver gldm_saplen %d != -2 "
		    "not supported", devname, macinfo->gldm_saplen);
		return (DDI_FAILURE);
	}

	ASSERT(macinfo->gldm_addrlen <= GLD_MAX_ADDRLEN);
	ASSERT(macinfo->gldm_saplen == -2);	/* XXX */

	/* see gld_rsrv() */
	if (ddi_getprop(DDI_DEV_T_NONE, devinfo, 0, "fast_recv", 0))
		macinfo->gldm_options |= GLDOPT_FAST_RECV;

	mutex_enter(&gld_device_list.gld_devlock);
	glddev = gld_devlookup(major);

	/*
	 *  Allocate per-driver (major) data structure if necessary
	 */
	if (glddev == NULL) {
		/* first occurrence of this device name (major number) */
		glddev = GETSTRUCT(glddev_t, 1);
		if (glddev == NULL) {
			mutex_exit(&gld_device_list.gld_devlock);
			return (DDI_FAILURE);
		}
		(void) strncpy(glddev->gld_name, devname,
		    sizeof (glddev->gld_name) - 1);
		glddev->gld_major = major;
		glddev->gld_nextminor = GLD_PPA_INIT;
		glddev->gld_mac_next = glddev->gld_mac_prev =
			(gld_mac_info_t *)&glddev->gld_mac_next;
		glddev->gld_str_next = glddev->gld_str_prev =
			(gld_t *)&glddev->gld_str_next;
		/*
		 * create the file system device node
		 */
		if (ddi_create_minor_node(devinfo, glddev->gld_name, S_IFCHR,
					0, DDI_NT_NET, 0) == DDI_FAILURE) {
			mutex_exit(&gld_device_list.gld_devlock);
			cmn_err(CE_WARN, "GLD: %s%d:  "
			    "ddi_create_minor_node %s failed",
			    ddi_get_name(devinfo), ddi_get_instance(devinfo),
			    glddev->gld_name);
			kmem_free(glddev, sizeof (glddev_t));
			return (DDI_FAILURE);
		}
		mutex_init(&glddev->gld_devlock, NULL, MUTEX_DRIVER, NULL);

		/* allow increase of number of supported multicast addrs */
		glddev->gld_multisize = ddi_getprop(DDI_DEV_T_NONE,
		    devinfo, 0, "multisize", GLD_MAX_MULTICAST);

		/* Stuff that's needed before any PPA gets attached */
		glddev->gld_type = macinfo->gldm_type;
		glddev->gld_minsdu = macinfo->gldm_minpkt;
		glddev->gld_saplen = macinfo->gldm_saplen;
		glddev->gld_addrlen = macinfo->gldm_addrlen;
		glddev->gld_broadcast = kmem_zalloc(macinfo->gldm_addrlen,
		    KM_SLEEP);
		if (glddev->gld_broadcast != NULL)
			bcopy(macinfo->gldm_broadcast_addr,
			    glddev->gld_broadcast, macinfo->gldm_addrlen);
		glddev->gld_maxsdu = macinfo->gldm_maxpkt;
		gldinsque(glddev, gld_device_list.gld_prev);
	}
	glddev->gld_ndevice++;
	/* Now glddev can't go away until we unregister this mac (or fail) */
	mutex_exit(&gld_device_list.gld_devlock);

	/*
	 *  Per-instance initialization
	 */

	/* add interrupt handler (v0 only) */
	ASSERT(macinfo->gldm_driver_version < GLD_VERSION_200 ||
	    macinfo->gldm_irq_index == -1);
	if (macinfo->gldm_intr != NULL && macinfo->gldm_irq_index >= 0 &&
	    ddi_dev_nintrs(devinfo, &nintrs) == DDI_SUCCESS &&
	    macinfo->gldm_irq_index < nintrs) {
		if (ddi_intr_hilevel(devinfo, macinfo->gldm_irq_index)) {
			cmn_err(CE_WARN, "GLD: %s: hi level interrupt",
				    devname);
			goto failure;
		}
		if (ddi_add_intr(devinfo, macinfo->gldm_irq_index,
		    &macinfo->gldm_cookie, NULL, gld_intr,
		    (caddr_t)macinfo) != DDI_SUCCESS) {
#ifdef lint
	/*
	 *  XXX 'goto' ifdeffed out because some v0 drivers don't
	 *  correctly handle this failure.
	 *
	 */
			goto failure;
#endif
		}
	}

	/* map the device memory (v0 only) */
	ASSERT(macinfo->gldm_driver_version < GLD_VERSION_200 ||
	    macinfo->gldm_reg_index == -1);
	if (macinfo->gldm_reg_index >= 0 &&
	    ddi_dev_nregs(devinfo, &nregs) == DDI_SUCCESS &&
	    macinfo->gldm_reg_index < nregs) {
		if (ddi_map_regs(devinfo, macinfo->gldm_reg_index,
		    &macinfo->gldm_memp, macinfo->gldm_reg_offset,
		    macinfo->gldm_reg_len) != DDI_SUCCESS) {
#ifdef lint
	/*
	 *  XXX 'goto' ifdeffed out because some v0 drivers don't
	 *  correctly handle this failure.
	 *
	 */
			goto late_failure;
#endif
		}
	}

	/*
	 * Initialise per-mac structure that is private to GLD.
	 * Set up interface pointer. These are device class specific pointers
	 * used to handle FDDI/TR/ETHER specific packets.
	 */
	for (i = 0; i < sizeof (interfaces)/sizeof (*interfaces); i++)
		if (mediatype == interfaces[i].mac_type) {
			macinfo->gldm_mac_pvt =
				kmem_zalloc(sizeof (gld_mac_pvt_t), KM_SLEEP);
			if (!macinfo->gldm_mac_pvt)
				goto late_late_failure;
			((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->
				interfacep = &interfaces[i];
			break;
		}

	if (!macinfo->gldm_mac_pvt) {
		cmn_err(CE_WARN, "GLD: this version does not support %s driver "
		    "of type %s", devname, gld_types[mediatype]);
		goto late_late_failure;
	}

	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	mutex_init(&mac_pvt->gldp_txlock, NULL, MUTEX_DRIVER, NULL);
	mac_pvt->gld_str_next = mac_pvt->gld_str_prev =
		(gld_t *)&mac_pvt->gld_str_next;
	mac_pvt->major_dev = glddev;

	if (macinfo->gldm_driver_version < GLD_VERSION_200) {
		/* Use the ones in the macinfo for v0 compatibility */
		mac_pvt->statistics = &macinfo->gldm_stats;
		mac_pvt->curr_macaddr = macinfo->gldm_macaddr;
	} else {
		/* Allocate them */
		if ((mac_pvt->curr_macaddr =
		    kmem_zalloc(macinfo->gldm_addrlen, KM_SLEEP)) == NULL)
			goto latest_failure;
		/*
		 * XXX Do bit-reversed devices store gldm_vendor in canonical
		 * format or in wire format?  Also gldm_broadcast.  For now
		 * we are assuming canonical, but I'm not sure that makes the
		 * most sense for ease of driver implementation.
		 */
		bcopy(macinfo->gldm_vendor_addr, mac_pvt->curr_macaddr,
		    macinfo->gldm_addrlen);
		if ((mac_pvt->statistics =
		    kmem_zalloc(sizeof (struct gld_stats), KM_SLEEP)) == NULL)
			goto latest_failure;
	}

	mutex_init(&macinfo->gldm_maclock, NULL, MUTEX_DRIVER,
	    macinfo->gldm_cookie);
	macinfo->gldm_GLD_flags |= GLD_MUTEX_INITED;

	ddi_set_driver_private(devinfo, (caddr_t)macinfo);

	/*
	 * Now atomically get a PPA and put ourselves on the mac list.
	 * Must be atomic because gld_findppa searches all macs.
	 */
	mutex_enter(&glddev->gld_devlock);

	/*
	 * v0 drivers will continue to use the old ppa assignment method,
	 * unless they set GLDOPT_DRIVER_PPA
	 */
	ASSERT(macinfo->gldm_driver_version < GLD_VERSION_200 ||
	    (macinfo->gldm_options & GLDOPT_DRIVER_PPA));
	if (!(macinfo->gldm_options & GLDOPT_DRIVER_PPA))
		macinfo->gldm_ppa = gld_findppa(glddev);

	/* add ourselves to this major device's linked list of instances */
	gldinsque(macinfo, glddev->gld_mac_prev);

	mutex_exit(&glddev->gld_devlock);

	/*
	 * Unfortunately we need the ppa before we call gld_initstats();
	 * otherwise we would like to do this just above the mutex_enter
	 * above.  In which case we could have set MAC_READY inside the
	 * mutex and we wouldn't have needed to check it in open and
	 * DL_ATTACH.  We wouldn't like to do the initstats/kstat_create
	 * inside the mutex because it might get taken in our kstat_update
	 * routine and cause a deadlock with kstat_chain_lock.
	 */

	/* gld_initstats() calls (*ifp->init)() */
	if (gld_initstats(macinfo) != GLD_SUCCESS) {
		mutex_enter(&glddev->gld_devlock);
		gldremque(macinfo);
		mutex_exit(&glddev->gld_devlock);
		goto latest_failure;
	}

	/*
	 * Need to indicate we are NOW ready to process interrupts;
	 * any interrupt before this is set is for someone else.
	 * This flag is also now used to tell open, et. al. that this
	 * mac is now fully ready and available for use.
	 */
	mutex_enter(&macinfo->gldm_maclock);
	macinfo->gldm_GLD_flags |= GLD_MAC_READY;
	mutex_exit(&macinfo->gldm_maclock);

	(void) sprintf(minordev, "%s%d", glddev->gld_name, macinfo->gldm_ppa);
	/* XXX Should check return code from the following call */
	(void) ddi_create_minor_node(devinfo, minordev, S_IFCHR,
				macinfo->gldm_ppa + 1, DDI_NT_NET, 0);

	/* log local ethernet address -- XXX not DDI compliant */
	(void) localetheraddr(
	    (struct ether_addr *)macinfo->gldm_vendor_addr, NULL);

	/* now put announcement into the message buffer */

	cmn_err(CE_CONT, "!%s%d: %s: type \"%s\" mac address %s\n",
	    glddev->gld_name,
	    macinfo->gldm_ppa, macinfo->gldm_ident, gld_types[mediatype],
	    gld_macaddr_sprintf(pbuf, macinfo->gldm_vendor_addr,
	    macinfo->gldm_addrlen));

	ddi_report_dev(devinfo);
	return (DDI_SUCCESS);

latest_failure:
	if (macinfo->gldm_GLD_flags & GLD_MUTEX_INITED) {
		mutex_destroy(&macinfo->gldm_maclock);
		macinfo->gldm_GLD_flags &= ~GLD_MUTEX_INITED;
	}
	if (macinfo->gldm_driver_version >= GLD_VERSION_200) {
		if (mac_pvt->curr_macaddr != NULL)
		    kmem_free(mac_pvt->curr_macaddr, macinfo->gldm_addrlen);
		if (mac_pvt->statistics != NULL)
		    kmem_free(mac_pvt->statistics, sizeof (struct gld_stats));
	}
	mutex_destroy(&mac_pvt->gldp_txlock);
	kmem_free(macinfo->gldm_mac_pvt, sizeof (gld_mac_pvt_t));
	macinfo->gldm_mac_pvt = NULL;

late_late_failure:
	    if (macinfo->gldm_reg_index >= 0 && macinfo->gldm_reg_index < nregs)
		ddi_unmap_regs(devinfo, macinfo->gldm_reg_index,
		    &macinfo->gldm_memp, macinfo->gldm_reg_offset,
		    macinfo->gldm_reg_len);

late_failure:
	if (macinfo->gldm_intr != NULL && macinfo->gldm_irq_index >= 0 &&
	    macinfo->gldm_irq_index < nintrs)
		ddi_remove_intr(devinfo, macinfo->gldm_irq_index,
				macinfo->gldm_cookie);

failure:
	mutex_enter(&gld_device_list.gld_devlock);
	glddev->gld_ndevice--;
	/*
	 * Note that just because this goes to zero here does not necessarily
	 * mean that we were the one who added the glddev above.  It's
	 * possible that the first mac unattached while were were in here
	 * failing to attach the second mac.  But we're now the last.
	 */
	if (glddev->gld_ndevice == 0) {
		/* There should be no macinfos left */
		ASSERT(glddev->gld_mac_next ==
		    (gld_mac_info_t *)&glddev->gld_mac_next);
		ASSERT(glddev->gld_mac_prev ==
		    (gld_mac_info_t *)&glddev->gld_mac_next);

		/*
		 * There should be no DL_UNATTACHED streams: the system
		 * should not have detached the "first" devinfo which has
		 * all the open style 2 streams.
		 *
		 * XXX This is not clear.  See gld_getinfo and Bug 1165519
		 */
		ASSERT(glddev->gld_str_next == (gld_t *)&glddev->gld_str_next);
		ASSERT(glddev->gld_str_prev == (gld_t *)&glddev->gld_str_next);

		ddi_remove_minor_node(devinfo, NULL);
		gldremque(glddev);
		mutex_destroy(&glddev->gld_devlock);
		if (glddev->gld_broadcast != NULL)
			kmem_free(glddev->gld_broadcast, glddev->gld_addrlen);
		kmem_free(glddev, sizeof (glddev_t));
	}
	mutex_exit(&gld_device_list.gld_devlock);

	return (DDI_FAILURE);
}

/*
 * gld_unregister (macinfo)
 * remove the macinfo structure from local structures
 * this is cleanup for a driver to be unloaded
 */
int
gld_unregister(gld_mac_info_t *macinfo)
{
	gld_mac_pvt_t *mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	glddev_t *glddev = mac_pvt->major_dev;
	dev_info_t *devinfo = macinfo->gldm_devinfo;
	int nintrs, nregs;
	gld_interface_t *ifp;
	int multisize = sizeof (gld_mcast_t) * glddev->gld_multisize;

	mutex_enter(&glddev->gld_devlock);
	mutex_enter(&macinfo->gldm_maclock);

	ASSERT(mac_pvt->nstreams >= 0);

	/*
	 * detach (and therefore gld_unregister) should not be called if
	 * there are still Style 1 open streams, but there could be Style 2
	 * streams DL_ATTACHed to this PPA that were originally opened on
	 * a different devinfo.
	 */
	if (mac_pvt->nstreams > 0) {
		mutex_exit(&macinfo->gldm_maclock);
		mutex_exit(&glddev->gld_devlock);
		TNF_PROBE_1(gld_unregister_FAILS, "gld gld_config gld_9f",
		    /* */,
		    tnf_opaque, macinfo, macinfo);
		return (DDI_FAILURE);
	}

	/* Delete this mac */
	gldremque(macinfo);

	/* Disallow further entries to gld_recv() and gld_sched() */
	macinfo->gldm_GLD_flags |= GLD_UNREGISTERED;

	mutex_exit(&macinfo->gldm_maclock);
	mutex_exit(&glddev->gld_devlock);

	/* nstreams was zero, so there shouldn't be any on the list */
	ASSERT(mac_pvt->gld_str_next == (gld_t *)&mac_pvt->gld_str_next);
	ASSERT(mac_pvt->gld_str_prev == (gld_t *)&mac_pvt->gld_str_next);

	/* remove the interrupt handler (v0 only) */
	ASSERT(macinfo->gldm_driver_version < GLD_VERSION_200 ||
	    macinfo->gldm_irq_index == -1);
	if (macinfo->gldm_intr != NULL && macinfo->gldm_irq_index >= 0 &&
	    ddi_dev_nintrs(devinfo, &nintrs) == DDI_SUCCESS &&
	    macinfo->gldm_irq_index < nintrs)
		ddi_remove_intr(devinfo, macinfo->gldm_irq_index,
		    macinfo->gldm_cookie);

	/* unmap the device memory (v0 only) */
	ASSERT(macinfo->gldm_driver_version < GLD_VERSION_200 ||
	    macinfo->gldm_reg_index == -1);
	if (macinfo->gldm_reg_index >= 0 &&
	    ddi_dev_nregs(devinfo, &nregs) == DDI_SUCCESS &&
	    macinfo->gldm_memp != NULL && macinfo->gldm_reg_index < nregs)
		ddi_unmap_regs(devinfo, macinfo->gldm_reg_index,
		    &macinfo->gldm_memp, macinfo->gldm_reg_offset,
		    macinfo->gldm_reg_len);

	ifp = ((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->interfacep;
	(*ifp->uninit)(macinfo);

	ASSERT(mac_pvt->kstatp);
	kstat_delete(mac_pvt->kstatp);

	/*
	 * v0: destroy the mutex for per-PPA locking.
	 *
	 * In v2 we keep the maclock until gld_mac_free so that we can
	 * use it to help protect against uncompleted interrupt threads.
	 * In v0 the interrupts were removed above, so the system guarantees
	 * that there are no such uncompleted interrupt threads.
	 */
	ASSERT(macinfo->gldm_GLD_flags & GLD_MUTEX_INITED);
	if (macinfo->gldm_driver_version < GLD_VERSION_200) {
		macinfo->gldm_GLD_flags &= ~(GLD_MUTEX_INITED | GLD_MAC_READY);
		mutex_destroy(&macinfo->gldm_maclock);
	}

	if (macinfo->gldm_driver_version >= GLD_VERSION_200) {
		kmem_free(mac_pvt->curr_macaddr, macinfo->gldm_addrlen);
		kmem_free(mac_pvt->statistics, sizeof (struct gld_stats));
	}

	if (mac_pvt->mcast_table != NULL)
		kmem_free(mac_pvt->mcast_table, multisize);
	mutex_destroy(&mac_pvt->gldp_txlock);
	kmem_free(macinfo->gldm_mac_pvt, sizeof (gld_mac_pvt_t));
	macinfo->gldm_mac_pvt = (caddr_t)NULL;

#ifdef GLD_DEBUG
	if (macinfo->gldm_driver_version >= GLD_VERSION_200) {
		char *devname = glddev->gld_name;
		/* Old v0 cruft should be unused */
		if (macinfo->gldm_flags != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_flags != 0", devname);
		if (macinfo->gldm_state != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_state != 0", devname);
		if (macinfo->gldm_port != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_port != 0", devname);
		if (macinfo->gldm_media != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_media != 0", devname);
		if (macinfo->gldm_reg_offset != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_reg_offset != 0", devname);
		if (macinfo->gldm_memp != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_memp != 0", devname);
		if (macinfo->gldm_reg_len != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_reg_len != 0", devname);
		if (macinfo->gldm_reg_index != -1)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_reg_index != -1", devname);
		if (macinfo->gldm_irq_index != -1)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_irq_index != -1", devname);
		if (*(uchar_t *)(macinfo->gldm_macaddr) != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_macaddr != 0", devname);
		if (*(uchar_t *)(macinfo->gldm_vendor) != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_vendor != 0", devname);
		if (*(uchar_t *)(macinfo->gldm_broadcast) != 0)
			cmn_err(CE_WARN, "GLD register: v2 %s driver: obsolete"
			    " macinfo->gldm_broadcast != 0", devname);
	}
#endif

	/* We now have one fewer instance for this major device */
	mutex_enter(&gld_device_list.gld_devlock);
	glddev->gld_ndevice--;
	if (glddev->gld_ndevice == 0) {
		/* There should be no macinfos left */
		ASSERT(glddev->gld_mac_next ==
		    (gld_mac_info_t *)&glddev->gld_mac_next);
		ASSERT(glddev->gld_mac_prev ==
		    (gld_mac_info_t *)&glddev->gld_mac_next);

		/*
		 * There should be no DL_UNATTACHED streams: the system
		 * should not have detached the "first" devinfo which has
		 * all the open style 2 streams.
		 *
		 * XXX This is not clear.  See gld_getinfo and Bug 1165519
		 */
		ASSERT(glddev->gld_str_next == (gld_t *)&glddev->gld_str_next);
		ASSERT(glddev->gld_str_prev == (gld_t *)&glddev->gld_str_next);

		ddi_remove_minor_node(macinfo->gldm_devinfo, NULL);
		gldremque(glddev);
		mutex_destroy(&glddev->gld_devlock);
		if (glddev->gld_broadcast != NULL)
			kmem_free(glddev->gld_broadcast, glddev->gld_addrlen);
		kmem_free(glddev, sizeof (glddev_t));
	}
	mutex_exit(&gld_device_list.gld_devlock);

	TNF_PROBE_1(gld_unregister, "gld gld_config gld_9f", /* */,
	    tnf_opaque, macinfo, macinfo);

	return (DDI_SUCCESS);
}

/*
 * gld_initstats
 * called from gld_register
 */
static int
gld_initstats(gld_mac_info_t *macinfo)
{
	gld_mac_pvt_t *mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	struct gldkstats *sp;
	glddev_t *glddev;
	kstat_t *ksp;
	gld_interface_t *ifp;
	struct gld_stats *stats = mac_pvt->statistics;

	glddev = mac_pvt->major_dev;

	if ((ksp = kstat_create(glddev->gld_name, macinfo->gldm_ppa,
	    NULL, "net", KSTAT_TYPE_NAMED,
	    sizeof (struct gldkstats) / sizeof (kstat_named_t), 0)) == NULL) {
		cmn_err(CE_WARN, "GLD: failed to create kstat structure for %s",
		    glddev->gld_name);
		return (GLD_FAILURE);
	}
	mac_pvt->kstatp = ksp;

	ksp->ks_update = gld_update_kstat;
	ksp->ks_private = (void *)macinfo;

	sp = ksp->ks_data;
	kstat_named_init(&sp->glds_pktrcv, "ipackets", KSTAT_DATA_UINT32);
	kstat_named_init(&sp->glds_pktxmt, "opackets", KSTAT_DATA_UINT32);
	kstat_named_init(&sp->glds_errrcv, "ierrors", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_errxmt, "oerrors", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_bytexmt, "obytes", KSTAT_DATA_UINT32);
	kstat_named_init(&sp->glds_bytercv, "rbytes", KSTAT_DATA_UINT32);
	kstat_named_init(&sp->glds_multixmt, "multixmt", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_multircv, "multircv", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_brdcstxmt, "brdcstxmt", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_brdcstrcv, "brdcstrcv", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_blocked, "blocked", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_noxmtbuf, "noxmtbuf", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_norcvbuf, "norcvbuf", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_xmtretry, "xmtretry", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_intr, "intr", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_pktrcv64, "ipackets64", KSTAT_DATA_UINT64);
	kstat_named_init(&sp->glds_pktxmt64, "opackets64", KSTAT_DATA_UINT64);
	kstat_named_init(&sp->glds_bytexmt64, "obytes64", KSTAT_DATA_UINT64);
	kstat_named_init(&sp->glds_bytercv64, "rbytes64", KSTAT_DATA_UINT64);
	kstat_named_init(&sp->glds_unknowns, "unknowns", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_speed, "ifspeed", KSTAT_DATA_UINT64);
	kstat_named_init(&sp->glds_media, "media", KSTAT_DATA_CHAR);
	kstat_named_init(&sp->glds_prom, "promisc", KSTAT_DATA_CHAR);

	kstat_named_init(&sp->glds_overflow, "oflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_underflow, "uflo", KSTAT_DATA_ULONG);
	kstat_named_init(&sp->glds_missed, "missed", KSTAT_DATA_ULONG);

	kstat_named_init(&sp->glds_xmtbadinterp, "xmt_badinterp",
	    KSTAT_DATA_UINT32);
	kstat_named_init(&sp->glds_rcvbadinterp, "rcv_badinterp",
	    KSTAT_DATA_UINT32);

	ifp = ((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->interfacep;

	(*ifp->init)(macinfo);

	kstat_install(ksp);

	if (macinfo->gldm_driver_version >= GLD_VERSION_200)
		return (GLD_SUCCESS);

	/* set stats that v0 drivers don't know about to reasonable values */
	stats->glds_media = macinfo->gldm_media;
	switch (macinfo->gldm_media) {
	default:
		/* don't know */
		stats->glds_speed = 0;
		break;
	case GLDM_AUI:
	case GLDM_BNC:
	case GLDM_10BT:
	case GLDM_TP:	/* probably 10MB */
		/* 10Mbit */
		stats->glds_speed = 10000000;
		break;
	case GLDM_FIBER:
	case GLDM_100BT:
	case GLDM_100BTX:
	case GLDM_100BT4:
	case GLDM_VGANYLAN:
		/* 100Mbit */
		stats->glds_speed = 100000000;
		break;
	case GLDM_RING4:
		stats->glds_speed = 4000000;
		break;
	case GLDM_RING16:
		stats->glds_speed = 16000000;
		break;
	}
	return (GLD_SUCCESS);
}

/* called from kstat mechanism, and from wsrv's get_statistics_req */
static int
gld_update_kstat(kstat_t *ksp, int rw)
{
	gld_mac_info_t	*macinfo;
	gld_mac_pvt_t	*mac_pvt;
	struct gldkstats *gsp;
	struct gld_stats *stats;

	if (rw == KSTAT_WRITE)
		return (EACCES);

	macinfo = (gld_mac_info_t *)ksp->ks_private;
	ASSERT(macinfo != NULL);

	mutex_enter(&macinfo->gldm_maclock);

	if (!(macinfo->gldm_GLD_flags & GLD_MAC_READY)) {
		mutex_exit(&macinfo->gldm_maclock);
		return (EIO);	/* this one's not ready yet */
	}

	if (macinfo->gldm_GLD_flags & GLD_UNREGISTERED) {
		mutex_exit(&macinfo->gldm_maclock);
		return (EIO);	/* this one's not ready anymore */
	}

	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	gsp = mac_pvt->kstatp->ks_data;
	ASSERT(gsp);
	stats = mac_pvt->statistics;

	TNF_PROBE_1(gldm_get_stats_start, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo);

	if (macinfo->gldm_get_stats)
		(void) (*macinfo->gldm_get_stats)(macinfo, stats);

	TNF_PROBE_1(gldm_get_stats_end, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo);

	/*
	 * The GLD-maintained transmit-side statistics are protected by
	 * gldp_txlock; all others are protected by gldm_maclock.
	 */

	mutex_enter(&mac_pvt->gldp_txlock);

	gsp->glds_pktxmt.value.ui32 = stats->glds_pktxmt64 & 0xffffffff;
	gsp->glds_bytexmt.value.ui32 = stats->glds_bytexmt64 & 0xffffffff;
	gsp->glds_multixmt.value.ul = stats->glds_multixmt;
	gsp->glds_brdcstxmt.value.ul = stats->glds_brdcstxmt;
	gsp->glds_noxmtbuf.value.ul = stats->glds_noxmtbuf;	/* 0 for now */
	gsp->glds_xmtretry.value.ul = stats->glds_xmtretry;

	/* new in version GLD_VERSION_200 */
	gsp->glds_pktxmt64.value.ui64 = stats->glds_pktxmt64;
	gsp->glds_bytexmt64.value.ui64 = stats->glds_bytexmt64;
	gsp->glds_xmtbadinterp.value.ui32 = stats->glds_xmtbadinterp;

	mutex_exit(&mac_pvt->gldp_txlock);

	gsp->glds_pktrcv.value.ui32 = stats->glds_pktrcv64 & 0xffffffff;
	gsp->glds_errxmt.value.ul = stats->glds_errxmt;
	gsp->glds_errrcv.value.ul = stats->glds_errrcv;
	gsp->glds_bytercv.value.ui32 = stats->glds_bytercv64 & 0xffffffff;
	gsp->glds_multircv.value.ul = stats->glds_multircv;
	gsp->glds_brdcstrcv.value.ul = stats->glds_brdcstrcv;
	gsp->glds_blocked.value.ul = stats->glds_blocked;
	gsp->glds_overflow.value.ul = stats->glds_overflow;
	gsp->glds_underflow.value.ul = stats->glds_underflow;
	gsp->glds_missed.value.ul = stats->glds_missed;
	gsp->glds_norcvbuf.value.ul = stats->glds_norcvbuf +
	    stats->glds_gldnorcvbuf;
	gsp->glds_intr.value.ul = stats->glds_intr;

	/* new in version GLD_VERSION_200 */
	gsp->glds_speed.value.ui64 = stats->glds_speed;
	gsp->glds_unknowns.value.ul = stats->glds_unknowns;
	gsp->glds_pktrcv64.value.ui64 = stats->glds_pktrcv64;
	gsp->glds_bytercv64.value.ui64 = stats->glds_bytercv64;
	gsp->glds_rcvbadinterp.value.ui32 = stats->glds_rcvbadinterp;

	if (mac_pvt->nprom)
		(void) strcpy(gsp->glds_prom.value.c, "phys");
	else if (mac_pvt->nprom_multi)
		(void) strcpy(gsp->glds_prom.value.c, "multi");
	else
		(void) strcpy(gsp->glds_prom.value.c, "off");

	(void) strcpy(gsp->glds_media.value.c, gld_media[
	    stats->glds_media < sizeof (gld_media) / sizeof (gld_media[0])
	    ? stats->glds_media : 0]);

	/* XXX Should be in a media specific routine in gldutil.c */
	switch (macinfo->gldm_type) {
	case DL_ETHER:
		gsp->glds_frame.value.ul = stats->glds_frame;
		gsp->glds_crc.value.ul = stats->glds_crc;
		gsp->glds_collisions.value.ul = stats->glds_collisions;
		gsp->glds_excoll.value.ul = stats->glds_excoll;
		gsp->glds_defer.value.ul = stats->glds_defer;
		gsp->glds_short.value.ul = stats->glds_short;
		gsp->glds_xmtlatecoll.value.ul = stats->glds_xmtlatecoll;
		gsp->glds_nocarrier.value.ul = stats->glds_nocarrier;

		/* v0 drivers don't know about the below stats */
		if (macinfo->gldm_driver_version < GLD_VERSION_200)
			break;

		gsp->glds_dot3_first_coll.value.ui32 =
		    stats->glds_dot3_first_coll;
		gsp->glds_dot3_multi_coll.value.ui32 =
		    stats->glds_dot3_multi_coll;
		gsp->glds_dot3_sqe_error.value.ui32 =
		    stats->glds_dot3_sqe_error;
		gsp->glds_dot3_mac_xmt_error.value.ui32 =
		    stats->glds_dot3_mac_xmt_error;
		gsp->glds_dot3_mac_rcv_error.value.ui32 =
		    stats->glds_dot3_mac_rcv_error;
		gsp->glds_dot3_frame_too_long.value.ui32 =
		    stats->glds_dot3_frame_too_long;
		(void) strcpy(gsp->glds_duplex.value.c, gld_duplex[
		    stats->glds_duplex <
		    sizeof (gld_duplex) / sizeof (gld_duplex[0]) ?
		    stats->glds_duplex : 0]);
		break;
	case DL_TPR:
		gsp->glds_dot5_line_error.value.ui32 =
		    stats->glds_dot5_line_error;
		gsp->glds_dot5_burst_error.value.ui32 =
		    stats->glds_dot5_burst_error;
		gsp->glds_dot5_signal_loss.value.ui32 =
		    stats->glds_dot5_signal_loss;

		/* v0 drivers don't know about the below stats */
		if (macinfo->gldm_driver_version < GLD_VERSION_200)
			break;

		gsp->glds_dot5_ace_error.value.ui32 =
		    stats->glds_dot5_ace_error;
		gsp->glds_dot5_internal_error.value.ui32 =
		    stats->glds_dot5_internal_error;
		gsp->glds_dot5_lost_frame_error.value.ui32 =
		    stats->glds_dot5_lost_frame_error;
		gsp->glds_dot5_frame_copied_error.value.ui32 =
		    stats->glds_dot5_frame_copied_error;
		gsp->glds_dot5_token_error.value.ui32 =
		    stats->glds_dot5_token_error;
		gsp->glds_dot5_freq_error.value.ui32 =
		    stats->glds_dot5_freq_error;
		break;
	case DL_FDDI:
		gsp->glds_fddi_mac_error.value.ui32 =
		    stats->glds_fddi_mac_error;
		gsp->glds_fddi_mac_lost.value.ui32 =
		    stats->glds_fddi_mac_lost;
		gsp->glds_fddi_mac_token.value.ui32 =
		    stats->glds_fddi_mac_token;
		gsp->glds_fddi_mac_tvx_expired.value.ui32 =
		    stats->glds_fddi_mac_tvx_expired;
		gsp->glds_fddi_mac_late.value.ui32 =
		    stats->glds_fddi_mac_late;
		gsp->glds_fddi_mac_ring_op.value.ui32 =
		    stats->glds_fddi_mac_ring_op;
		break;
	default:
		break;
	}

	mutex_exit(&macinfo->gldm_maclock);

#ifdef GLD_DEBUG
	gld_check_assertions();
	if (gld_debug & GLDRDE)
		gld_sr_dump(macinfo);
#endif

	return (0);
}

/*
 * The device dependent driver specifies gld_getinfo as its getinfo routine.
 */
int
gld_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
	dev_info_t	*devinfo;
	minor_t		minor = getminor((dev_t)arg);
	int		rc = DDI_FAILURE;
#ifdef  lint
	dip = dip;
#endif

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((devinfo = gld_finddevinfo((dev_t)arg)) != NULL) {
			*(dev_info_t *)resultp = devinfo;
			rc = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		/* Need static mapping for deferred attach */
		if (minor == GLD_USE_STYLE2) {
			/*
			 * Style 2:  this minor number does not correspond to
			 * any particular instance number.
			 *
			 * XXX So how does deferred attach know which instance
			 *	it is supposed to attach in this case?  I guess
			 *	it's a good thing the implementation seems to
			 *	just always attempt to attach all instances
			 *	every time the driver loads:  see Bug 1165519.
			 */
			rc = DDI_FAILURE;
		} else if (minor < GLD_PPA_INIT) {
			/* Style 1:  instance == ppa == minor - 1 */
			*(int *)resultp = minor - 1;
			rc = DDI_SUCCESS;
		} else {
			/* Clone:  look for it.  Not a static mapping */
			if ((devinfo = gld_finddevinfo((dev_t)arg)) != NULL) {
				*(int *)resultp = ddi_get_instance(devinfo);
				rc = DDI_SUCCESS;
			}
		}
		break;
	}

	TNF_PROBE_4(gld_getinfo, "gld gld_config", /* */,
	    tnf_opaque, dip, dip,
	    tnf_int, cmd, cmd,
	    tnf_int, rc, rc,
	    tnf_opaque, result, *resultp);

	return (rc);
}

/* called from gld_getinfo */
dev_info_t *
gld_finddevinfo(dev_t dev)
{
	minor_t		minor = getminor(dev);
	glddev_t	*device;
	gld_mac_info_t	*mac;
	gld_t		*str;
	dev_info_t	*devinfo = NULL;

	if (minor == GLD_USE_STYLE2) {
		/*
		 * Style 2:  this minor number does not correspond to
		 * any particular instance number.
		 *
		 * XXX We don't know what to say.  See Bug 1165519.
		 */
		return (NULL);
	}

	mutex_enter(&gld_device_list.gld_devlock);	/* hold the device */

	device = gld_devlookup(getmajor(dev));
	if (device == NULL) {
		/* There are no attached instances of this device */
		mutex_exit(&gld_device_list.gld_devlock);
		return (NULL);
	}

	/*
	 * Search all attached macs and streams.
	 *
	 * XXX We don't bother checking the DL_UNATTACHED streams since
	 * we don't know what devinfo we should report back even if we
	 * found the minor.  Maybe we should associate streams that are
	 * not currently attached to a PPA with the "first" devinfo node
	 * of the major device to attach -- the one that created the
	 * minor node for the generic device.
	 */
	mutex_enter(&device->gld_devlock);

	for (mac = device->gld_mac_next;
	    mac != (gld_mac_info_t *)&device->gld_mac_next;
	    mac = mac->gldm_next) {
		gld_mac_pvt_t *pvt = (gld_mac_pvt_t *)mac->gldm_mac_pvt;

		if (!(mac->gldm_GLD_flags & GLD_MAC_READY))
			continue;	/* this one's not ready yet */
		if (minor < GLD_PPA_INIT) {
			/* Style 1 -- look for the corresponding PPA */
			if (minor - 1 == mac->gldm_ppa) {
				devinfo = mac->gldm_devinfo;
				goto out;	/* found it! */
			} else
				continue;	/* not this PPA */
		}

		/* We are looking for a clone */
		for (str = pvt->gld_str_next;
		    str != (gld_t *)&pvt->gld_str_next;
		    str = str->gld_next) {
			ASSERT(str->gld_mac_info == mac);
			if (minor == str->gld_minor) {
				devinfo = mac->gldm_devinfo;
				goto out;
			}
		}
	}
out:
	mutex_exit(&device->gld_devlock);
	mutex_exit(&gld_device_list.gld_devlock);
	return (devinfo);
}

/*
 * STREAMS open routine.  The device dependent driver specifies this as its
 * open entry point.
 */
/*ARGSUSED2*/
int
gld_open(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *cred)
{
	gld_mac_pvt_t *mac_pvt;
	gld_t  *gld;
#ifdef t_uscalar_t
	t_uscalar_t ppa;
#else
	long ppa;
#endif
	glddev_t *glddev;
	gld_mac_info_t *mac;

	ASSERT(q);

	ppa = getminor(*dev);
	if (ppa >= GLD_PPA_INIT)
		return (ENXIO);

	ASSERT(q->q_ptr == NULL);	/* Clone device gives us a fresh Q */

	/* Find our per-major glddev_t structure */
	mutex_enter(&gld_device_list.gld_devlock);
	glddev = gld_devlookup(getmajor(*dev));
	/*
	 * This glddev will hang around since detach (and therefore
	 * gld_unregister) can't run while we're here in the open routine.
	 */
	mutex_exit(&gld_device_list.gld_devlock);

	if (glddev == NULL)
		return (ENXIO);

	TNF_PROBE_4(gld_open, "gld gld_config", /* */,
	    tnf_opaque, q, q,
	    tnf_opaque, dev, *dev,
	    tnf_int, style, ppa == GLD_USE_STYLE2 ? 2 : 1,
	    tnf_int, ppa, ppa - 1);

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT) {
		if (ppa == GLD_USE_STYLE2)
			cmn_err(CE_NOTE, "gld_open(%p, Style 2)", (void *)q);
		else
			cmn_err(CE_NOTE, "gld_open(%p, Style 1, PPA = %ld)",
			    (void *)q, ppa - 1);
	}
#endif

	/*
	 * get a per-stream structure and link things together so we
	 * can easily find them later.
	 */
	if ((gld = (gld_t *)kmem_zalloc(sizeof (gld_t), KM_SLEEP)) == NULL)
		return (ENOSR);

	/*
	 * fill in the structure and state info
	 */
	gld->gld_qptr = q;
	gld->gld_device = glddev;
	gld->gld_state = DL_UNATTACHED;

	/*
	 * we must atomically find a free minor number and add the stream
	 * to a list, because gld_findminor has to traverse the lists to
	 * determine which minor numbers are free.
	 */
	mutex_enter(&glddev->gld_devlock);

	/* find a free minor device number for the clone */
	gld->gld_minor = gld_findminor(glddev);
	if (gld->gld_minor == 0) {
		mutex_exit(&glddev->gld_devlock);
		kmem_free(gld, sizeof (gld_t));
		return (ENOSR);
	}

	if (ppa == GLD_USE_STYLE2) {
		gld->gld_style = DL_STYLE2;
		*dev = makedevice(getmajor(*dev), gld->gld_minor);
		WR(q)->q_ptr = q->q_ptr = (caddr_t)gld;
		gldinsque(gld, glddev->gld_str_prev);
#ifdef GLD_VERBOSE_DEBUG
		if (gld_debug & GLDPROT)
			cmn_err(CE_NOTE, "GLDstruct added to device list");
#endif
	} else {
		gld->gld_style = DL_STYLE1;
		/* the PPA is actually 1 less than the minordev */
		ppa--;
		for (mac = glddev->gld_mac_next;
		    mac != (gld_mac_info_t *)(&glddev->gld_mac_next);
		    mac = mac->gldm_next) {
			ASSERT(mac);
			if (!(mac->gldm_GLD_flags & GLD_MAC_READY))
				continue;	/* this one's not ready yet */
			if (mac->gldm_ppa == ppa) {
				/*
				 * we found the correct PPA
				 */
				mac_pvt = (gld_mac_pvt_t *)mac->gldm_mac_pvt;

				mutex_enter(&mac->gldm_maclock);

				gld->gld_mac_info = mac;

				/* now ready for action */
				gld->gld_state = DL_UNBOUND;

				if (mac_pvt->nstreams == 0) {
					if (gld_start_mac(mac) != GLD_SUCCESS) {
					    mutex_exit(&mac->gldm_maclock);
					    mutex_exit(&glddev->gld_devlock);
					    kmem_free(gld, sizeof (gld_t));
					    return (EIO);
					}
				}
				mac_pvt->nstreams++;
				*dev = makedevice(getmajor(*dev),
				    gld->gld_minor);
				WR(q)->q_ptr = q->q_ptr = (caddr_t)gld;
				gldinsque(gld, mac_pvt->gld_str_prev);

				mutex_exit(&mac->gldm_maclock);
#ifdef GLD_VERBOSE_DEBUG
				if (gld_debug & GLDPROT)
					cmn_err(CE_NOTE,
					    "GLDstruct added to instance list");
#endif
				break;
			}
		}
		if (gld->gld_state == DL_UNATTACHED) {
			mutex_exit(&glddev->gld_devlock);
			kmem_free(gld, sizeof (gld_t));
			TNF_PROBE_2(gld_open_fails, "gld gld_config", /* */,
			    tnf_opaque, q, q,
			    tnf_string, _, "requested PPA not registered");
			return (ENXIO);
		}
	}

	mutex_exit(&glddev->gld_devlock);

#ifdef GLD_VERBOSE_DEBUG
	if (gld_debug & GLDPROT)
		cmn_err(CE_NOTE, "gld_open() gld ptr: %p minor: %d",
		    (void *)gld, gld->gld_minor);
#endif

	noenable(WR(q));	/* We'll do the qenables manually */
	qprocson(q);		/* start the queues running */
	qenable(WR(q));
	return (0);
}

/*
 * normal stream close call checks current status and cleans up
 * data structures that were dynamically allocated
 */
/*ARGSUSED1*/
int
gld_close(queue_t *q, int flag, cred_t *cred)
{
	gld_t	*gld = (gld_t *)q->q_ptr;
	glddev_t *glddev = gld->gld_device;

	ASSERT(q);
	ASSERT(gld);

	TNF_PROBE_1(gld_close, "gld gld_config", /* */,
	    tnf_opaque, q, q);

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gld_close(%p, Style %d)",
		    (void *)q, (gld->gld_style & 0x1) + 1);
	}
#endif

	/* Hold all device streams lists still while we check for a macinfo */
	mutex_enter(&glddev->gld_devlock);

	if (gld->gld_mac_info != NULL) {
		/* If there's a macinfo, block recv while we change state */
		mutex_enter(&gld->gld_mac_info->gldm_maclock);
		gld->gld_flags |= GLD_STR_CLOSING; /* no more rcv putnexts */
		gld_set_ipq(gld->gld_mac_info);
		mutex_exit(&gld->gld_mac_info->gldm_maclock);
	} else {
		/* no mac DL_ATTACHED right now */
		gld->gld_flags |= GLD_STR_CLOSING;
	}

	mutex_exit(&glddev->gld_devlock);

	/*
	 * qprocsoff before we call gld_unbind/gldunattach, so that
	 * we know wsrv isn't in there trying to undo what we're doing.
	 */
	qprocsoff(q);

	ASSERT(gld->gld_wput_count == 0);
	gld->gld_wput_count = 0;	/* just in case */

	if (gld->gld_state == DL_IDLE) {
		/* Need to unbind */
		ASSERT(gld->gld_mac_info != NULL);
		(void) gld_unbind(WR(q), NULL);
	}

	if (gld->gld_state == DL_UNBOUND) {
		/* Need to unattach */
		ASSERT(gld->gld_mac_info != NULL);
		(void) gldunattach(WR(q), NULL);
	}

	/* disassociate the stream from the device */
	q->q_ptr = WR(q)->q_ptr = NULL;

	/*
	 * Since we unattached above (if necessary), we know that we're
	 * on the per-major list of unattached streams, rather than a
	 * per-PPA list.  So we know we should hold the devlock.
	 */
	mutex_enter(&glddev->gld_devlock);
	gldremque(gld);			/* remove from Style 2 list */
	mutex_exit(&glddev->gld_devlock);

	kmem_free(gld, sizeof (gld_t));

	return (0);
}

/*
 * gld_rsrv (q)
 *	simple read service procedure
 *	purpose is to avoid the time it takes for packets
 *	to move through IP so we can get them off the board
 *	as fast as possible due to limited PC resources.
 *
 *	This is not normally used in the current implementation.  It
 *	can be selected with the undocumented property "fast_recv".
 *	If that property is set, gld_recv will send the packet
 *	upstream with a putq() rather than a putnext(), thus causing
 *	this routine to be scheduled.
 */
int
gld_rsrv(queue_t *q)
{
	mblk_t *mp;

	TNF_PROBE_1(gld_RSRV, "gld gld_recv", /* */,
	    tnf_opaque, q, q);

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
 * gld_wput (q, mp)
 * general gld stream write put routine. Receives fastpath data from upper
 * modules and processes it immediately.  ioctl and M_PROTO/M_PCPROTO are
 * queued for later processing by the service procedure.
 */

int
gld_wput(queue_t *q, mblk_t *mp)
{
	gld_t  *gld = (gld_t *)(q->q_ptr);
	int	rc;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_wput(%p %p): type %x",
		    (void *)q, (void *)mp, DB_TYPE(mp));
#endif
	switch (DB_TYPE(mp)) {

	case M_DATA:
		/* fast data / raw support */
		/* we must be DL_ATTACHED and DL_BOUND to do this */
		/* Tricky to access memory without taking the mutex */
		if ((gld->gld_flags & (GLD_RAW | GLD_FAST)) == 0 ||
						gld->gld_state != DL_IDLE) {
			merror(q, mp, EPROTO);
			break;
		}

		/* Only call gld_start() directly if nothing queued ahead */
		/* No guarantees about ordering with different threads */
		if (q->q_first)
			goto use_wsrv;

		/*
		 * This can happen if wsrv has taken off the last mblk but
		 * is still processing it.
		 */
		membar_consumer();
		if (gld->gld_in_wsrv)
			goto use_wsrv;

		/*
		 * Keep a count of current wput calls to start.
		 * Nonzero count delays any attempted DL_UNBIND.
		 * See comments above gld_start().
		 */
		atomic_add_32((uint32_t *)&gld->gld_wput_count, 1);
		membar_enter();

		/* Recheck state now wput_count is set to prevent DL_UNBIND */
		/* If this Q is in process of DL_UNBIND, don't call start */
		if (gld->gld_state != DL_IDLE || gld->gld_in_unbind) {
			/* Extremely unlikely */
			atomic_add_32((uint32_t *)&gld->gld_wput_count, -1);
			goto use_wsrv;
		}

		rc = gld_start(q, mp, GLD_TRYLOCK);

		/* Allow DL_UNBIND again */
		membar_exit();
		atomic_add_32((uint32_t *)&gld->gld_wput_count, -1);

		if (rc != GLD_NORESOURCES)
			break;	/*  Done with this packet */

use_wsrv:
		/* Q not empty, in DL_DETACH, or start gave NORESOURCES */
		(void) putq(q, mp);
		qenable(q);
		break;

	case M_IOCTL:
		/* ioctl relies on wsrv single threading per queue */
		(void) putq(q, mp);
		qenable(q);
		break;

	case M_CTL:
		(void) putq(q, mp);
		qenable(q);
		break;

	case M_FLUSH:		/* canonical flush handling */
		TNF_PROBE_3(gld_wput_flush, "gld gld_config", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_uchar, flag, *mp->b_rptr);
		/* XXX Should these be FLUSHALL? */
		if (*mp->b_rptr & FLUSHW)
			flushq(q, 0);
		if (*mp->b_rptr & FLUSHR) {
			flushq(RD(q), 0);
			*mp->b_rptr &= ~FLUSHW;
			qreply(q, mp);
		} else
			freemsg(mp);
		break;

	case M_PROTO:
	case M_PCPROTO:
		/* these rely on wsrv single threading per queue */
		(void) putq(q, mp);
		qenable(q);
		break;

	default:
		TNF_PROBE_3(gld_wput_DB_TYPE_unknown, "gld gld_config", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_opaque, DB_TYPE, DB_TYPE(mp));
#ifdef GLD_DEBUG
		if (gld_debug & GLDETRACE)
			cmn_err(CE_WARN,
			    "gld: Unexpected packet type from queue: 0x%x",
			    DB_TYPE(mp));
#endif
		freemsg(mp);
	}
	return (0);
}

/*
 * gld_wsrv - Incoming messages are processed according to the DLPI protocol
 * specification.
 *
 * wsrv is single-threaded per Q.  We make use of this to avoid taking the
 * mutex for reading data items that are only ever written by us.
 */

int
gld_wsrv(queue_t *q)
{
	mblk_t *mp;
	register gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;
	union DL_primitives *prim;
	int	err;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_wsrv(%p)", (void *)q);
#endif

	ASSERT(gld->gld_in_wsrv == 0);

	gld->gld_xwait = 0;	/* We are now going to processes this Q */

	if (q->q_first == NULL)
		return (0);

	/*
	 * v0:  Acquire the maclock prior to the getq().
	 *
	 * Note: This is done to eliminate a race condition between wsrv()
	 * and v0 gld_v0_sched() which could result in xmit packet reordering.
	 * A failed send does the putbq inside the mutex.
	 *
	 * The macinfo can be NULL if we're an un-DL_ATTACHed Style 2 stream.
	 */
	macinfo = gld->gld_mac_info;
	if (macinfo && macinfo->gldm_driver_version < GLD_VERSION_200)
		mutex_enter(&macinfo->gldm_maclock);

	/*
	 * Help wput avoid a call to gld_start if there might be a message
	 * previously queued by that thread being processed here.
	 */
	gld->gld_in_wsrv = 1;
	membar_enter();

	while ((mp = getq(q)) != NULL) {
		switch (DB_TYPE(mp)) {
		case M_DATA:
			/*
			 * retry of a previously processed UNITDATA_REQ
			 * or is a RAW or FAST message from above.
			 */
			if (macinfo == NULL) {
				/* No longer attached to a PPA, drop packet */
				freemsg(mp);
				break;
			}

			gld->gld_sched_ran = 0;
			membar_enter();
			if (gld_start(q, mp, GLD_HAVELOCK) == GLD_NORESOURCES) {
				(void) putbq(q, mp);
				/* gld_sched will quenable us later */
				gld->gld_xwait = 1;	/* want qenable */
				membar_enter();
				/*
				 * v2:  we're not holding the mutex; it's
				 * possible that the driver could have already
				 * called gld_sched (following up on its
				 * return of GLD_NORESOURCES), before we got a
				 * chance to do the putbq() and set gld_xwait.
				 * So if we saw a call to gld_sched that
				 * examined this queue, since our call to
				 * gld_start() above, then it's possible we've
				 * already seen the only call to gld_sched()
				 * we're ever going to see.  So we better retry
				 * transmitting this packet right now.
				 */
				if (gld->gld_sched_ran) {
#ifdef GLD_DEBUG
					if (gld_debug & GLDTRACE)
						cmn_err(CE_NOTE, "gld_wsrv: "
						    "sched was called");
#endif
					break;	/* try again right now */
				}
				gld->gld_in_wsrv = 0;
				if (macinfo &&
				    macinfo->gldm_driver_version
				    < GLD_VERSION_200)
					mutex_exit(&macinfo->gldm_maclock);
				return (0);
			}
			break;

		case M_IOCTL:
			if (macinfo &&
			    macinfo->gldm_driver_version < GLD_VERSION_200)
				mutex_exit(&macinfo->gldm_maclock);
			(void) gld_ioctl(q, mp);
			if (macinfo &&
			    macinfo->gldm_driver_version < GLD_VERSION_200)
				mutex_enter(&macinfo->gldm_maclock);
			break;

		case M_CTL:
			if (macinfo &&
			    macinfo->gldm_driver_version >= GLD_VERSION_200 &&
			    macinfo->gldm_mctl != NULL) {
				mutex_enter(&macinfo->gldm_maclock);

				TNF_PROBE_3(gldm_mctl_start,
				    "gld gld_config gld_9e", /* */,
				    tnf_opaque, mac, macinfo,
				    tnf_opaque, q, q,
				    tnf_opaque, mp, mp);

				(void) (*macinfo->gldm_mctl) (macinfo, q, mp);

				TNF_PROBE_1(gldm_mctl_end,
				    "gld gld_config gld_9e", /* */,
				    tnf_opaque, mac, macinfo);

				mutex_exit(&macinfo->gldm_maclock);
			} else {
				/* This driver doesn't recognize, just drop */
				TNF_PROBE_3(gldm_mctl_unknown,
				    "gld gld_config",
				    "gld driver does not implement gldm_mctl",
				    tnf_opaque, mac, macinfo,
				    tnf_opaque, q, q,
				    tnf_opaque, mp, mp);
				freemsg(mp);
			}
			break;

		case M_PROTO:	/* Will be an DLPI message of some type */
		case M_PCPROTO:
			if (macinfo &&
			    macinfo->gldm_driver_version < GLD_VERSION_200)
				mutex_exit(&macinfo->gldm_maclock);
			if ((err = gld_cmds(q, mp)) != GLDE_OK) {
				if (err == GLDE_RETRY) {
					gld->gld_in_wsrv = 0;
					return (0); /* quit while we're ahead */
				}
				prim = (union DL_primitives *)mp->b_rptr;
				dlerrorack(q, mp, prim->dl_primitive, err, 0);
			}
			/*
			 * macinfo could have changed if we processed a
			 * DL_ATTACH or DL_DETACH
			 */
			macinfo = gld->gld_mac_info;	/* recompute macinfo */
			if (macinfo &&
			    macinfo->gldm_driver_version < GLD_VERSION_200)
				mutex_enter(&macinfo->gldm_maclock);
			break;

		default:
			/* This should never happen */
#ifdef GLD_DEBUG
			if (gld_debug & GLDERRS)
				cmn_err(CE_WARN,
				    "gld_wsrv: db_type(%x) not supported",
				    mp->b_datap->db_type);
#endif
			freemsg(mp);	/* unknown types are discarded */
			break;
		}
	}

	membar_exit();
	gld->gld_in_wsrv = 0;

	if (macinfo && macinfo->gldm_driver_version < GLD_VERSION_200)
		mutex_exit(&macinfo->gldm_maclock);

	return (0);
}

/*
 * gld_start() can get called from gld_wput(), gld_wsrv(), gld_unitdata(),
 * and in v0, gld_v0_sched().
 *
 * We only come directly from wput() in the GLD_FAST (fastpath) or RAW case.
 *
 * In v0 there are various lock conditions in place before we are called:
 *	gld_wput:	GLD_TRYLOCK	(We try to enter the mutex)
 *	gld_wsrv:	GLD_HAVELOCK	(Lock already held)
 *	gld_unitdata:	GLD_LOCK	(We need to enter the mutex)
 *	gld_v0_sched:	GLD_HAVELOCK	(Lock already held)
 *
 * In v2, there are no locks held when we are called, and we don't take the
 * lock, but the "lockflavor" variable is still used to tell us where we
 * came from.
 *
 * In particular, we must avoid calling gld_precv() if we came from wput().
 * gld_precv() is where we, on the transmit side, loop back our outgoing
 * packets to the receive side if we are in physical promiscuous mode.
 * Since the receive side holds a mutex across its call to the upstream
 * putnext, and that upstream module could well have looped back to our
 * wput() routine on the same thread, we cannot call gld_precv from here
 * for fear of causing a recursive mutex entry in our receive code.
 *
 * There is a problem here when coming from gld_wput().  While wput
 * only comes here if the queue is attached to a PPA and bound to a SAP
 * and there are no messages on the queue ahead of the M_DATA that could
 * change that, it is theoretically possible that another thread could
 * now wput a DL_UNBIND and a DL_DETACH message, and the wsrv() routine
 * could wake up and process them, before we finish processing this
 * send of the M_DATA.  This can only possibly happen on a Style 2 RAW or
 * FAST (fastpath) stream:  non RAW/FAST streams always go through wsrv(),
 * and Style 1 streams only DL_DETACH in the close routine, where
 * qprocsoff() protects us.  (In v0 it can't happen once we take the
 * mutex, but even there there is still theoretically the window between
 * the time we check DL_IDLE and the time we take the mutex.  In v2 it
 * can happen even up to the time we call the gldm_send() routine and
 * beyond.)  If this happens we could end up calling gldm_send() after
 * we have detached the stream and possibly called gldm_stop().  Worse,
 * once the number of attached streams goes to zero, detach/unregister
 * could be called, and the macinfo could go away entirely.
 *
 * No one has ever seen this happen.
 *
 * It is some trouble to fix this, and we would rather not add any mutex
 * logic into the wput() routine, which is supposed to be a "fast"
 * path.
 *
 * What I've done is use an atomic counter to keep a count of the number
 * of threads currently calling gld_start() from wput() on this stream.
 * If DL_DETACH sees this as nonzero, it putbqs the request back onto
 * the queue and qenables, hoping to have better luck next time.  Since
 * people shouldn't be trying to send after they've asked to DL_DETACH,
 * hopefully very soon all the wput=>start threads should have returned
 * and the DL_DETACH will succeed.  It's hard to test this since the odds
 * of the failure even trying to happen are so small.  I probably could
 * have ignored the whole issue and never been the worse for it.
 */
int
gld_start(queue_t *q, mblk_t *mp, int lockflavor)
{
	mblk_t *nmp;
	gld_t *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo;
	gld_mac_pvt_t *mac_pvt;
	int rc;
	gld_interface_t *ifp;
	pktinfo_t pktinfo;

	macinfo = gld->gld_mac_info;

#ifndef _PRE_SOLARIS_2_6
	ASSERT(macinfo != NULL);
#endif

	/*
	 * If we were called from wput(), it is possible for the state
	 * of the stream to have changed since wput() decided to call
	 * us, since the mutex is not held and wsrv() can be running
	 * concurrently.  So we check to make sure there is still a
	 * macinfo associated with this stream.
	 *
	 * I think this is all fixed now with 2.6 and above, using the
	 * gld_wput_count/gld_in_unbind protocol, but I'm leaving this
	 * check in anyway.
	 *
	 * We want to know if this ever happens.
	 */
	if (macinfo == NULL) {
		freemsg(mp);
		cmn_err(CE_WARN, "GLD: wsrv macinfo NULL (lock %d)",
		    lockflavor);
		return (GLD_FAILURE);
	}

	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;

	/*
	 * We're not holding the mutex for this check.  If the promiscuous
	 * state is in flux it doesn't matter much if we get this wrong.
	 */
	if (mac_pvt->nprom > 0) {
		/*
		 * We want to loopback to the receive side, but to avoid
		 * recursive mutex entry:  if we came from wput(), which
		 * could have looped back via IP from our own receive
		 * interrupt thread, we decline this request.  wput()
		 * will then queue the packet for wsrv().  This means
		 * that when snoop is running we don't get the advantage
		 * of the wput() multithreaded direct entry to the
		 * driver's send routine.
		 */
		if (lockflavor == GLD_TRYLOCK)	/* we came from wput() */
			return (GLD_NORESOURCES);

		nmp = dupmsg(mp);	/* for the loopback */
	} else
		nmp = NULL;		/* we need no loopback */


	/* If we're a v2 driver, we don't do maclock mutex locking on xmit */
	if (macinfo->gldm_driver_version >= GLD_VERSION_200)
		lockflavor = GLD_DONTLOCK;
	else if (lockflavor == GLD_LOCK)
		mutex_enter(&macinfo->gldm_maclock);
	else if (lockflavor == GLD_TRYLOCK)
		/*
		 * In v0 we have to take the maclock on xmit.  This thread might
		 * be holding it already if we came down wput() from our own
		 * interrupt/receive thread.  If someone is holding the mutex
		 * it might be us, so we just fail and let wput queue for wsrv.
		 */
		if (!mutex_tryenter(&macinfo->gldm_maclock)) {
			if (nmp)
				freemsg(nmp);
			/*
			 * wput will qenable, but these flags speed us up
			 * most of the time.
			 */
			macinfo->gldm_GLD_flags |= GLD_INTR_WAIT;
			gld->gld_xwait = 1;	/* want gld_v0_sched */
			return (GLD_NORESOURCES);	/* Try again later */
		}

	/* v0 drivers are holding the mutex by now */
	ASSERT(macinfo->gldm_driver_version >= GLD_VERSION_200 ||
	    mutex_owned(&macinfo->gldm_maclock));

	ifp = mac_pvt->interfacep;
	if ((*ifp->interpreter)(macinfo, mp, &pktinfo, 1) ||
	    pktinfo.pktLen > ifp->mtu_size) {
		/* bad packet */
		if (lockflavor == GLD_LOCK || lockflavor == GLD_TRYLOCK)
			mutex_exit(&macinfo->gldm_maclock);
		freemsg(mp);
		if (nmp)
			freemsg(nmp);	/* free the dupped message */
		TNF_PROBE_3(gld_send_BADARG, "gld gld_send",
		    "gld_start rejected outbound packet",
		    tnf_opaque, mac, macinfo,
		    tnf_opaque, mp, mp,
		    tnf_int, size, pktinfo.pktLen);
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld_start: rejected outbound packet, size %d, "
			    "max %d", pktinfo.pktLen, ifp->mtu_size);
#endif
		mutex_enter(&mac_pvt->gldp_txlock);
		mac_pvt->statistics->glds_xmtbadinterp++;
		mutex_exit(&mac_pvt->gldp_txlock);
		return (GLD_BADARG);
	}

	TNF_PROBE_2(gldm_send_start, "gld gld_9e gld_send", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_opaque, mp, mp);

	rc = (*macinfo->gldm_send) (macinfo, mp);	/* try the send */

	TNF_PROBE_3(gldm_send_end, "gld gld_9e gld_send", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_opaque, mp, mp,
	    tnf_int, rc, rc);

	if (rc != GLD_SUCCESS) {
		if (rc == GLD_NORESOURCES) {
			TNF_PROBE_0(gldm_send_NORESOURCES, "gld gld_flow",
			    /* */);
			/* our caller will (re)queue */
			if (macinfo->gldm_driver_version < GLD_VERSION_200) {
				macinfo->gldm_GLD_flags |= GLD_INTR_WAIT;
				gld->gld_xwait = 1;	/* want gld_v0_sched */
			}
			if (lockflavor == GLD_LOCK || lockflavor == GLD_TRYLOCK)
				mutex_exit(&macinfo->gldm_maclock);
			mutex_enter(&mac_pvt->gldp_txlock);
			mac_pvt->statistics->glds_xmtretry++;
			mutex_exit(&mac_pvt->gldp_txlock);
		} else {
			/* transmit error; drop the packet */
			if (lockflavor == GLD_LOCK || lockflavor == GLD_TRYLOCK)
				mutex_exit(&macinfo->gldm_maclock);
			freemsg(mp);
			/* We're supposed to count failed attempts as well */
			mutex_enter(&mac_pvt->gldp_txlock);
			mac_pvt->statistics->glds_bytexmt64 += pktinfo.pktLen;
			mac_pvt->statistics->glds_pktxmt64++;
			if (pktinfo.isBroadcast)
				mac_pvt->statistics->glds_brdcstxmt++;
			else if (pktinfo.isMulticast)
				mac_pvt->statistics->glds_multixmt++;
			mutex_exit(&mac_pvt->gldp_txlock);
#ifdef GLD_DEBUG
			if (gld_debug & GLDERRS)
				cmn_err(CE_WARN,
				    "gld_start: gldm_send failed %d", rc);
#endif
		}
		if (nmp)
			freemsg(nmp);	/* free the dupped message */
		return (rc);
	}

	mutex_enter(&mac_pvt->gldp_txlock);
	if (pktinfo.isBroadcast)
		mac_pvt->statistics->glds_brdcstxmt++;
	else if (pktinfo.isMulticast)
		mac_pvt->statistics->glds_multixmt++;
	mac_pvt->statistics->glds_bytexmt64 += pktinfo.pktLen;
	mac_pvt->statistics->glds_pktxmt64++;
	mutex_exit(&mac_pvt->gldp_txlock);

	/*
	 * Loopback case. The message needs to be returned back on
	 * the read side. This would silently fail if the dumpmsg fails
	 * above. This is probably OK, if there is no memory to dup the
	 * block, then there isn't much we could do anyway.
	 */
	if (nmp) {
		if (lockflavor == GLD_DONTLOCK)
			mutex_enter(&macinfo->gldm_maclock);	/* v2 */
		gld_precv(macinfo, nmp);
		if (lockflavor == GLD_DONTLOCK)
			mutex_exit(&macinfo->gldm_maclock);	/* v2 */
	}

	if (lockflavor == GLD_LOCK || lockflavor == GLD_TRYLOCK)
		mutex_exit(&macinfo->gldm_maclock);

	/*
	 * The device may be using the mblk for direct dma, in which case
	 * it will ask us not to free the mblk. The device would now be
	 * responsible for freeing this message block, once the dma is
	 * completed.  This is the default behaviour for post-v0 drivers.
	 */
	if (!(macinfo->gldm_options & GLDOPT_DONTFREE))
		freemsg(mp);	/* free on success */

	return (GLD_SUCCESS);
}

/*
 * gld_intr (macinfo)
 */
uint_t
gld_intr(gld_mac_info_t *macinfo)
{
	int claimed;
	int v0;

	ASSERT(macinfo != NULL);

	if (!(macinfo->gldm_GLD_flags & GLD_MAC_READY)) {
		TNF_PROBE_2(gld_intr, "gld gld_config gld_intr",
		    "gld_intr called, MAC not ready",
		    tnf_opaque, mac, macinfo,
		    tnf_string, _, "MAC NOT REGISTERED");
		return (DDI_INTR_UNCLAIMED);
	}

	v0 = macinfo->gldm_driver_version < GLD_VERSION_200;

	if (v0)
		mutex_enter(&macinfo->gldm_maclock);

	ASSERT(macinfo->gldm_intr != NULL);

	TNF_PROBE_1(gldm_intr_start, "gld gld_9e gld_intr", /* */,
	    tnf_opaque, mac, macinfo);

	claimed = (*macinfo->gldm_intr)(macinfo);

	TNF_PROBE_2(gldm_intr_end, "gld gld_9e gld_intr", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_int, claimed, claimed);

	if (v0) {
		if (claimed == DDI_INTR_CLAIMED)
			(void) gld_v0_sched(macinfo);
		mutex_exit(&macinfo->gldm_maclock);
	}

	return (claimed);
}

/*
 * gld_sched (macinfo)
 *
 * This routine scans the streams that refer to a specific macinfo
 * structure and causes the STREAMS scheduler to try to run them if
 * they are marked as waiting for the transmit buffer.
 */
void
gld_sched(gld_mac_info_t *macinfo)
{
	register gld_mac_pvt_t *mac_pvt;
	register gld_t *gld;

	ASSERT(macinfo != NULL);

	TNF_PROBE_1(gld_sched, "gld gld_9f gld_flow", /* */,
	    tnf_opaque, mac, macinfo);

	/*
	 * If we're a v0 driver, do it the old way.
	 *
	 * gld_intr() now calls gld_v0_sched() in the v0 case.  But
	 * we must do this check because some old v0/v1 drivers call
	 * gld_sched() directly.
	 */
	if (macinfo->gldm_driver_version < GLD_VERSION_200) {
		gld_v0_sched(macinfo);
		return;
	}

	mutex_enter(&macinfo->gldm_maclock);

	if (macinfo->gldm_GLD_flags & GLD_UNREGISTERED) {
		/* We're probably being called from a leftover interrupt */
		mutex_exit(&macinfo->gldm_maclock);
		return;
	}

	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;

	for (gld = mac_pvt->gld_str_next;
	    gld != (gld_t *)&mac_pvt->gld_str_next; gld = gld->gld_next) {
		ASSERT(gld->gld_mac_info == macinfo);
		gld->gld_sched_ran = 1;
		membar_enter();
		if (gld->gld_xwait) {
			gld->gld_xwait = 0;
			qenable(WR(gld->gld_qptr));
		}
	}

	mutex_exit(&macinfo->gldm_maclock);
}

/*
 * In v0, this routine is called at interrupt time after each interrupt is
 * delivered to the driver; it should be made to run Fast.
 */
void
gld_v0_sched(gld_mac_info_t *macinfo)
{
	register gld_t *gld, *first;
	mblk_t *mp = NULL;
	gld_mac_pvt_t *mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;

	ASSERT(macinfo != NULL);
	ASSERT(macinfo->gldm_driver_version < GLD_VERSION_200);
	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	if (macinfo->gldm_GLD_flags & GLD_UNREGISTERED) {
		/* We're probably being called from a leftover interrupt */
		return;
	}

	/*
	 * v1 drivers (v0 drivers using undocumented interfaces) used this
	 * flag to know that GLD needed them to generate an interrupt when
	 * xmit resources became available.  This ill-conceived interface
	 * is supported for backward compatibility with such drivers.
	 */
	macinfo->gldm_GLD_flags &= ~GLD_INTR_WAIT;	/* for v1 drivers */

	gld = mac_pvt->last_sched;  /* The last one on which we failed */
	if (gld == NULL)
		gld = mac_pvt->gld_str_next;	/* start with the first one */

	first = gld;
	do {
		if (gld == (gld_t *)&(mac_pvt->gld_str_next))
			continue; /* This is the list head, not a gld_t */

		ASSERT(gld->gld_mac_info == macinfo);

		if (!gld->gld_xwait)
			continue;

		gld->gld_xwait = 0;

		if (gld->gld_flags & GLD_STR_CLOSING)
			continue;

		while ((mp = getq(WR(gld->gld_qptr)))) {
			if (DB_TYPE(mp) != M_DATA) {
				/*
				 * We can't help here -- put the mp
				 * back on the Q
				 */
				(void) putbq(WR(gld->gld_qptr), mp);
				qenable(WR(gld->gld_qptr));
				break;	/* try next queue */
			}

			/* We've got a formed packet, try to send it */
			if (gld_start(WR(gld->gld_qptr), mp, GLD_HAVELOCK)
			    == GLD_NORESOURCES) {
				/* device too busy */
				/* gld_sched will retry this again later */
				(void) putbq(WR(gld->gld_qptr), mp);
				macinfo->gldm_GLD_flags |= GLD_INTR_WAIT;
				gld->gld_xwait = 1;	/* want qenable */
				mac_pvt->last_sched = gld;
				return;	/* device too busy, wait for later */
			}
		}
	} while ((gld = gld->gld_next) != first);
}

/*
 * gld_precv (macinfo, mp)
 * called from gld_start to loopback a packet when in promiscuous mode
 */
static void
gld_precv(gld_mac_info_t *macinfo, mblk_t *mp)
{
	ASSERT(mutex_owned(&macinfo->gldm_maclock));
	gld_sendup(macinfo, mp, gld_paccept);
}

/*
 * gld_recv (macinfo, mp)
 * called with an mac-level packet in a mblock; take the mutex,
 * try the IPQ hack, and otherwise call gld_sendup.
 *
 * V0 drivers already are holding the mutex when they call us.
 */
void
gld_recv(gld_mac_info_t *macinfo, mblk_t *mp)
{
	gld_mac_pvt_t *mac_pvt;
	struct ether_header *ehp = (struct ether_header *)mp->b_rptr;
	queue_t *ipq;
	char pbuf[3*GLD_MAX_ADDRLEN];

	ASSERT(macinfo != NULL);
	ASSERT(mp->b_datap->db_ref);

	TNF_PROBE_2(gld_recv_start, "gld gld_9f gld_recv", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_opaque, mp, mp);

	if (macinfo->gldm_driver_version >= GLD_VERSION_200)
		mutex_enter(&macinfo->gldm_maclock);

	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	if (macinfo->gldm_GLD_flags & GLD_UNREGISTERED) {
		/* We're probably being called from a leftover interrupt */
		if (macinfo->gldm_driver_version >= GLD_VERSION_200)
			mutex_exit(&macinfo->gldm_maclock);
		TNF_PROBE_2(gld_recv_end, "gld gld_config gld_9f gld_recv",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_string, _, "has been UNREGISTERED");
		return;
	}

	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	ipq = mac_pvt->ipq;

	mac_pvt->statistics->glds_bytercv64 += mp->b_wptr - mp->b_rptr;
	mac_pvt->statistics->glds_pktrcv64++;

	/*
	 * Special case for IP; we can simply do the putnext here, if:
	 * o the device type is ethernet.
	 * o there are no PROMISC_SAP streams;
	 * o there is one, and only one, IP stream attached;
	 * o that stream is a "fastpath" stream;
	 * o the packet is of type 0x800 (ETHERTYPE_IP);
	 * o the packet is not multicast or broadcast (fastpath only
	 *	wants unicast packets).
	 *
	 * XXX Presently this can be enabled for Ethernet only, as the below
	 * XXX code knows too much about Ethernet format specifically.
	 */
	if (ipq != NULL && ntohs(ehp->ether_type) == ETHERTYPE_IP &&
	    (ehp->ether_dhost.ether_addr_octet[0] & 1) == 0 &&
	    canputnext(ipq)) {
		TNF_PROBE_4_DEBUG(gld_recv_IPQ, "gld gld_recv", /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_opaque, ipq, ipq,
		    tnf_opaque, mp, mp,
		    tnf_string, shost,
			gld_macaddr_sprintf(pbuf, mp->b_rptr+6, 6));
		/* skip the mac header */
		/* we know there is no LLC1/SNAP header in this packet */
		mp->b_rptr += sizeof (struct ether_header);
		putnext(ipq, mp);
	} else
		gld_sendup(macinfo, mp, gld_accept);	/* the normal way */

	if (macinfo->gldm_driver_version >= GLD_VERSION_200)
		mutex_exit(&macinfo->gldm_maclock);

	TNF_PROBE_1(gld_recv_end, "gld gld_9f gld_recv", /* */,
	    tnf_opaque, mac, macinfo);
}

/* ================================================================= */
/* receive group: called from gld_recv and gld_precv with mutex held */
/* ================================================================= */

/*
 * gld_sendup (macinfo, mp)
 * called with an ethernet packet in a mblock; must decide whether
 * packet is for us and which streams to queue it to.
 */
static void
gld_sendup(gld_mac_info_t *macinfo, mblk_t *mp, int (*acceptfunc)())
{
	gld_t *gld;
	gld_t *fgld = NULL;
	mblk_t *nmp;
	pktinfo_t pktinfo;
	gld_interface_t *ifp;
	void	(*send)(queue_t *qp, mblk_t *mp);
	int	(*cansend)(queue_t *qp);
	gld_mac_pvt_t *mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	char pbuf[3*GLD_MAX_ADDRLEN];

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_sendup(%p, %p)", (void *)mp,
		    (void *)macinfo);
#endif

	ASSERT(mp != NULL);
	ASSERT(macinfo != NULL);
	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	/*
	 * The "fast" in "GLDOPY_FAST_RECV" refers to the speed at which
	 * gld_recv returns to the caller's interrupt routine.  The total
	 * network throughput would normally be lower when selecting this
	 * option, because we putq the messages and process them later,
	 * instead of sending them with putnext now.  Some time critical
	 * device might need this, so it's here but undocumented.
	 */
	if (macinfo->gldm_options & GLDOPT_FAST_RECV) {
		send = (void (*)(queue_t *, mblk_t *))putq;
		cansend = canput;
	} else {
		send = (void (*)(queue_t *, mblk_t *))putnext;
		cansend = canputnext;
	}

	ifp = mac_pvt->interfacep;

	/*
	 * call the media specific packet interpreter routine
	 */
	if ((*ifp->interpreter)(macinfo, mp, &pktinfo, 0)) {
		freemsg(mp);
		mac_pvt->statistics->glds_rcvbadinterp++;
		TNF_PROBE_2(gld_sendup_BAD, "gld gld_recv",
		    "gld_sendup cannot interpret packet",
		    tnf_opaque, mac, macinfo,
		    tnf_opaque, mp, mp);
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld_sendup: interpreter failed");
#endif
		return;
	}

	/*
	 * "Special" packets are non-LLC/non-Ethernet packets, such as
	 * Token Ring MAC packets, or FDDI MAC or SMT packets.  As such,
	 * they have no SAP value.  We use an out-of-range SAP value to
	 * match these packets with Streams that have indicated they want
	 * to receive these special packets.  This saves a little time
	 * in the frequently-executed SAPMATCH macro.
	 */
	if (pktinfo.isSpecial)
		pktinfo.Sap = GLD_SPECIAL_SAP;

#ifdef GLD_DEBUG
	if ((gld_debug & GLDRECV) &&
			(!(gld_debug & GLDNOBR) ||
			(!pktinfo.isBroadcast && !pktinfo.isMulticast))) {

		cmn_err(CE_CONT, "gld_sendup: machdr=<%x:%x:%x:%x:%x:%x -> "
					"%x:%x:%x:%x:%x:%x>\n",
			pktinfo.shost[0], pktinfo.shost[1], pktinfo.shost[2],
			pktinfo.shost[3], pktinfo.shost[4], pktinfo.shost[5],
			pktinfo.dhost[0], pktinfo.dhost[1], pktinfo.dhost[2],
			pktinfo.dhost[3], pktinfo.dhost[4], pktinfo.dhost[5]);
		cmn_err(CE_CONT, "gld_sendup: Snap: %s Sap: %4x Len: %4d "
				"Hdr: %d,%d isMulticast: %s\n",
				pktinfo.hasSnap ? "Y" : "N",
				pktinfo.Sap,
				pktinfo.pktLen,
				pktinfo.macLen,
				pktinfo.hdrLen,
				pktinfo.isMulticast ? "Y" : "N");
	}
#endif

	/*
	 * Search all the streams attached to this macinfo looking for
	 * those eligible to receive the present packet.
	 */
	for (gld = mac_pvt->gld_str_next;
	    gld != (gld_t *)&mac_pvt->gld_str_next; gld = gld->gld_next) {
#ifdef GLD_VERBOSE_DEBUG
		cmn_err(CE_NOTE, "gld_sendup: SAP: %4x QPTR: %p QSTATE: %s",
		    gld->gld_sap, (void *)gld->gld_qptr,
		    gld->gld_state == DL_IDLE ? "IDLE": "NOT IDLE");
#endif
		ASSERT(gld->gld_qptr != NULL);
		ASSERT(gld->gld_state == DL_IDLE ||
		    gld->gld_state == DL_UNBOUND);
		ASSERT(gld->gld_mac_info == macinfo);

		if (gld->gld_state != DL_IDLE)
			continue;	/* not eligible to receive */
		if (gld->gld_flags & GLD_STR_CLOSING)
			continue;	/* not eligible to receive */

#ifdef GLD_DEBUG
		if ((gld_debug & GLDRECV) &&
				(!(gld_debug & GLDNOBR) ||
				(!pktinfo.isBroadcast && !pktinfo.isMulticast)))
			cmn_err(CE_NOTE,
			    "gld_sendup: queue sap: %4x promis: %s %s %s",
			    gld->gld_sap,
			    gld->gld_flags & GLD_PROM_PHYS ? "phys " : "     ",
			    gld->gld_flags & GLD_PROM_SAP  ? "sap  " : "     ",
			    gld->gld_flags & GLD_PROM_MULT ? "multi" : "     ");
#endif

		/*
		 * The accept function differs depending on whether this is
		 * a packet that we received from the wire or a loopback.
		 */
		if ((*acceptfunc)(gld, &pktinfo, mp)) {
			/* sap matches */
			pktinfo.wasAccepted = 1;	/* known protocol */

			if (!(*cansend)(gld->gld_qptr)) {
				/*
				 * Upper stream is not accepting messages, i.e.
				 * it is flow controlled, therefore we will
				 * forgo sending the message up this stream.
				 */
				TNF_PROBE_2(gld_sendup_nocanput,
				    "gld gld_flow gld_recv",
				    "gld_sendup canput failed",
				    tnf_opaque, mac, macinfo,
				    tnf_opaque, q, gld->gld_qptr);
#ifdef GLD_DEBUG
				if (gld_debug & GLDETRACE)
					cmn_err(CE_WARN,
					    "gld_sendup: canput failed");
#endif
				mac_pvt->statistics->glds_blocked++;
				qenable(gld->gld_qptr);
				continue;
			}
			/*
			 * we are trying to avoid an extra dumpmsg() here.
			 * If this is the first eligible queue, remember the
			 * queue and send up the message after the loop.
			 */
			if (!fgld) {
				fgld = gld;
				continue;
			}
			/* duplicate the packet for this stream */
			nmp = dupmsg(mp);
			if (nmp == NULL) {
				mac_pvt->statistics->glds_gldnorcvbuf++;
#ifdef GLD_DEBUG
				if (gld_debug & GLDERRS)
					cmn_err(CE_WARN,
					    "gld_sendup: dupmsg failed");
#endif
				break;	/* couldn't get resources; drop it */
			}
			/* pass the message up the stream */
			gld_passon(gld, nmp, &pktinfo, send);
		}
	}

	ASSERT(mp);
	/* send the original dup of the packet up the first stream found */
	if (fgld)
		gld_passon(fgld, mp, &pktinfo, send);
	else
		freemsg(mp);	/* no streams matched */

	TNF_PROBE_5_DEBUG(gld_sendup, "gld gld_recv", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_string, shost,
		gld_macaddr_sprintf(pbuf, pktinfo.shost, macinfo->gldm_addrlen),
	    tnf_string, dhost,
		gld_macaddr_sprintf(pbuf, pktinfo.dhost, macinfo->gldm_addrlen),
	    tnf_int, dsap, pktinfo.Sap,
	    tnf_opaque, flags, *(uint_t *)&pktinfo);

	/* If this was a looped back packet, return now */
	if (acceptfunc == gld_paccept)
		return;		/* transmit loopback case */

	/* We do not count looped back packets */
	if (pktinfo.isBroadcast)
		mac_pvt->statistics->glds_brdcstrcv++;
	else if (pktinfo.isMulticast)
		mac_pvt->statistics->glds_multircv++;

	/* No stream accepted this packet */
	if (!pktinfo.wasAccepted)
		mac_pvt->statistics->glds_unknowns++;
}

/*
 * The saps are defined as matching
 *	if the packet sap and the streams sap are exactly the same
 *  or	the stream is in the SAP promiscuos mode
 *  or  both saps are LLC SAPs
 *  or	the stream is an LLC SAP and the packet has a SNAP header (so SAP 0xaa)
 */
#define	SAPMATCH(stream,  pktinfo)					\
	((stream->gld_sap == pktinfo->Sap) ||				\
	(stream->gld_flags & GLD_PROM_SAP) ||				\
	(stream->gld_sap <= GLD_MAX_802_SAP && stream->gld_sap >= 0 &&	\
	    (pktinfo->Sap <= GLD_802_SAP || pktinfo->hasSnap)))

/*
 * This function validates a packet for sending up a particular
 * stream. The message header has been parsed and its characteristic
 * are recorded in the pktinfo data structure. The streams stack info
 * are preseneted in gld data structures.
 */
static int
gld_accept(gld_t *gld, pktinfo_t *pktinfo)
{
	/*
	 * if the saps do not match do not bother checking further.
	 */
	if (SAPMATCH(gld, pktinfo) == 0)
		return (0);

	/*
	 * We don't accept any packet from the hardware if we originated it.
	 * (Contrast gld_paccept, the send-loopback accept function.)
	 */
	if (pktinfo->isLooped) {
		return (0);
	}

	/*
	 * If the packet is broadcast or sent to us directly we will accept it.
	 * Also we will accept multicast packets requested by the stream.
	 */
	if (pktinfo->isForMe || pktinfo->isBroadcast ||
	    gld_mcmatch(gld, pktinfo)) {
		return (1);
	}

	/*
	 * Finally, accept anything else if we're in promiscuous mode
	 */
	if (gld->gld_flags & GLD_PROM_PHYS) {
		return (1);
	}

	return (0);
}

/*
 * Return TRUE if the given multicast address is one
 * of those that this particular Stream is interested in.
 */
static int
gld_mcmatch(gld_t *gld, pktinfo_t *pktinfo)
{
	/*
	 * Return FALSE if not a multicast address.
	 */
	if (!pktinfo->isMulticast)
		return (0);

	/*
	 * Check if all multicasts have been enabled for this Stream
	 */
	if (gld->gld_flags & GLD_PROM_MULT)
		return (1);

	/*
	 * Return FALSE if no multicast addresses enabled for this Stream.
	 */
	if (!gld->gld_mcast)
		return (0);

	/*
	 * Otherwise, look for it in the table.
	 */
	return (gld_multicast(pktinfo->dhost, gld));
}

/*
 * gld_multicast determines if the address is a multicast address for
 * this stream.
 */
static int
gld_multicast(unsigned char *macaddr, gld_t *gld)
{
	register int i;

	ASSERT(mutex_owned(&gld->gld_mac_info->gldm_maclock));

	if (!gld->gld_mcast)
		return (0);

	for (i = 0; i < gld->gld_multicnt; i++) {
		if (gld->gld_mcast[i]) {
			ASSERT(gld->gld_mcast[i]->gldm_refcnt);
			if (mac_eq(gld->gld_mcast[i]->gldm_addr, macaddr,
			    gld->gld_mac_info->gldm_addrlen))
				return (1);
		}
	}

	return (0);
}

/*
 * accept function for looped back packets
 */
static int
gld_paccept(gld_t *gld, pktinfo_t *pktinfo)
{
	return (SAPMATCH(gld, pktinfo) && (gld->gld_flags & GLD_PROM_PHYS));
}

static void
gld_passon(gld_t *gld, mblk_t *mp, pktinfo_t *pktinfo,
	void (*send)(queue_t *qp, mblk_t *mp))
{
	int skiplen;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_passon(%p, %p, %p)", (void *)gld,
		    (void *)mp, (void *)pktinfo);

	if ((gld_debug & GLDRECV) && (!(gld_debug & GLDNOBR) ||
	    (!pktinfo->isBroadcast && !pktinfo->isMulticast)))
		cmn_err(CE_NOTE, "gld_passon: q: %p mblk: %p minor: %d sap: %x",
		    (void *)gld->gld_qptr->q_next, (void *)mp, gld->gld_minor,
		    gld->gld_sap);
#endif

	/*
	 * Figure out how much of the packet header to throw away.
	 *
	 * RAW streams expect to see the whole packet.
	 *
	 * Other streams expect to see the packet with the MAC header
	 * removed.  Streams bound to sap > 0xff expect to have any
	 * LLC+SNAP header removed.  Streams bound to sap <= 0xff want
	 * to see the LLC header.
	 *
	 * Normal DLPI (non RAW/FAST) streams also want the
	 * DL_UNITDATA_IND M_PROTO message block prepended to the M_DATA.
	 */
	if (gld->gld_flags & GLD_RAW) {
		skiplen = 0;
	} else {
		skiplen = pktinfo->macLen;		/* skip mac header */
		if (pktinfo->hasSnap && gld->gld_sap > GLD_MAX_802_SAP)
			skiplen += pktinfo->hdrLen;	/* skip LLC1+SNAP hdr */
	}

	if (skiplen >= pktinfo->pktLen) {
		/*
		 * If the interpreter did its job right, then it cannot be
		 * asking us to skip more bytes than are in the packet!
		 * However, there could be zero data bytes left after the
		 * amount to skip.  DLPI specifies that passed M_DATA blocks
		 * should contain at least one byte of data, so if we have
		 * none we just drop it.
		 */
		ASSERT(!(skiplen > pktinfo->pktLen));
		freemsg(mp);
		return;
	}

	/*
	 * Skip over the header(s), taking care to possibly handle message
	 * fragments shorter than the amount we need to skip.  Hopefully
	 * the driver will put the entire packet, or at least the entire
	 * header, into a single message block.  But we handle it if not.
	 */
	while (skiplen >= MBLKL(mp)) {
		mblk_t *tmp = mp;
		skiplen -= MBLKL(mp);
		mp = mp->b_cont;
		ASSERT(mp != NULL);	/* because skiplen < pktinfo->pktLen */
		freeb(tmp);
	}
	mp->b_rptr += skiplen;

	/* Add M_PROTO if necessary, and pass upstream */
	if (((gld->gld_flags & GLD_FAST) && !pktinfo->isMulticast &&
	    !pktinfo->isBroadcast) || (gld->gld_flags & GLD_RAW)) {
		/* RAW/FAST: just send up the M_DATA */
		(*send)(gld->gld_qptr, mp);
	} else {
		/* everybody else wants to see a unitdata_ind structure */
		mp = gld_addudind(gld, mp, pktinfo);
		if (mp)
			(*send)(gld->gld_qptr, mp);
		/* if it failed, gld_addudind already bumped statistic */
	}
}

/*
 * gld_addudind(gld, mp, pktinfo)
 * format a DL_UNITDATA_IND message to be sent upstream to the user
 */
static mblk_t *
gld_addudind(gld_t *gld, mblk_t *mp, pktinfo_t *pktinfo)
{
	gld_mac_info_t		*macinfo = gld->gld_mac_info;
	dl_unitdata_ind_t	*dludindp;
	mblk_t			*nmp;
	int			size;
	int			ssap, dsap;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_addudind(%p, %p, %p)", (void *)gld,
		    (void *)mp, (void *)pktinfo);
#endif
	ASSERT(macinfo != NULL);

	/*
	 * Allocate the DL_UNITDATA_IND M_PROTO header, if allocation fails
	 * might as well discard since we can't go further
	 */
	size = sizeof (dl_unitdata_ind_t) +
	    2 * (macinfo->gldm_addrlen + abs(macinfo->gldm_saplen));
	if ((nmp = allocb(size, BPRI_MED)) == NULL) {
		freemsg(mp);
		((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->
		    statistics->glds_gldnorcvbuf++;
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld_addudind: allocb failed");
#endif
		return ((mblk_t *)NULL);
	}
	DB_TYPE(nmp) = M_PROTO;
	nmp->b_rptr = nmp->b_datap->db_lim - size;

	ssap = pktinfo->sSap;
	dsap = pktinfo->Sap;

	if (pktinfo->hasSnap && gld->gld_sap <= GLD_MAX_802_SAP) {
		/*
		 * This is an 802 stream.  It wants to see the 0xaa SNAP
		 * LLC sap, not the SNAP ethertype we have in pktinfo.
		 */
		ssap = dsap = LSAP_SNAP;
	}

	/*
	 * now setup the DL_UNITDATA_IND header
	 *
	 * XXX This looks broken if the saps aren't two bytes.
	 */
	dludindp = (dl_unitdata_ind_t *)nmp->b_rptr;
	dludindp->dl_primitive = DL_UNITDATA_IND;
	dludindp->dl_src_addr_length =
	    dludindp->dl_dest_addr_length = macinfo->gldm_addrlen +
					abs(macinfo->gldm_saplen);
	dludindp->dl_dest_addr_offset = sizeof (dl_unitdata_ind_t);
	dludindp->dl_src_addr_offset = dludindp->dl_dest_addr_offset +
					dludindp->dl_dest_addr_length;

	dludindp->dl_group_address = (pktinfo->isMulticast ||
					pktinfo->isBroadcast);

	nmp->b_wptr = nmp->b_rptr + dludindp->dl_dest_addr_offset;

	mac_copy(pktinfo->dhost, nmp->b_wptr, macinfo->gldm_addrlen);
	nmp->b_wptr += macinfo->gldm_addrlen;

	ASSERT(macinfo->gldm_saplen == -2);	/* XXX following code assumes */
	*(ushort *)(nmp->b_wptr) = dsap;
	nmp->b_wptr += abs(macinfo->gldm_saplen);

	ASSERT(nmp->b_wptr == nmp->b_rptr + dludindp->dl_src_addr_offset);

	mac_copy(pktinfo->shost, nmp->b_wptr, macinfo->gldm_addrlen);
	nmp->b_wptr += macinfo->gldm_addrlen;

	*(ushort *)(nmp->b_wptr) = ssap;
	nmp->b_wptr += abs(macinfo->gldm_saplen);

	linkb(nmp, mp);
	return (nmp);
}

/* ======================================================= */
/* wsrv group: called from wsrv, single threaded per queue */
/* ======================================================= */

/*
 * We go to some trouble to avoid taking the same mutex during normal
 * transmit processing as we do during normal receive processing.
 *
 * Elements of the per-instance macinfo and per-stream gld_t structures
 * are for the most part protected by the macinfo->gldm_maclock mutex.
 * (Elements of the gld_mac_pvt_t structure are considered part of the
 * macinfo structure for purposes of this discussion).
 *
 * However, it is more complicated than that:
 *
 *	Elements of the macinfo structure that are set before the macinfo
 *	structure is added to its device list by gld_register(), and never
 *	thereafter modified, are accessed without requiring taking the mutex.
 *	A similar rule applies to those elements of the gld_t structure that
 *	are written by gld_open() before the stream is added to any list.
 *
 *	Most other elements of the macinfo structure may only be read or
 *	written while holding the maclock mutex.
 *
 *	Transmit statistics in the macinfo structure are protected by
 *	a separate gldp->txlock rather than the maclock, to avoid the
 *	send thread taking the same mutex as the receive thread.
 *
 *	Most writable elements of the gld_t structure are written only
 *	within the single-threaded domain of wsrv() and subsidiaries.
 *	(This domain includes open/close while qprocs are not on.)
 *	The maclock mutex need not be taken while within that domain
 *	simply to read those elements.  Writing to them, even within
 *	that domain, or reading from it outside that domain, requires
 *	holding the maclock mutex.  Exception:  if the stream is not
 *	presently attached to a PPA, there is no associated macinfo,
 *	and no maclock need be taken.
 *
 *	The curr_macaddr element of the mac private structure is also
 *      protected by the gldm_maclock mutex, like most other members
 *      of that structure. However, there are a few instances in the
 *      transmist path where we choose to forgo mutex protection when
 *      reading this variable. This is to avoid mutex contention between
 *      threads executing the DL_UNITDATA_REQ case and receive threads.
 *      In doing so we will take a small risk or a few corrupted packets
 *      during the short an rare times when someone is changing the interface's
 *      physical address. We consider the small cost in this rare case to be
 *      worth the benefit of reduced mutex contention under normal operating
 *      conditions. The risk/cost is small becase:
 *          1. there is no guarantee at this layer of uncorrupted delivery.
 *          2. the physaddr doesn't change very often - no performance hit.
 *          3. if the physaddr changes, other stuff is going to be screwed
 *             up for a while anyway, while other sites refigure ARP, etc.,
 *             so losing a couple of packets is the least of our worries.
 *
 *	The list of streams associated with a macinfo is protected by
 *	two mutexes:  the per-macinfo maclock, and the per-major-device
 *	gld_devlock.  Both must be held to modify the list, but either
 *	may be held to protect the list during reading/traversing.  This
 *	allows independent locking for multiple instances in the receive
 *	path (using macinfo), while facilitating routines that must search
 *	the entire set of streams associated with a major device, such as
 *	gld_findminor(), gld_finddevinfo(), close().  The "nstreams"
 *	macinfo	element, and the gld_mac_info gld_t element, are similarly
 *	protected, since they change at exactly the same time macinfo
 *	streams list does.
 *
 *	The list of macinfo structures associated with a major device
 *	structure is protected by the gld_devlock, as is the per-major
 *	list of Style 2 streams in the DL_UNATTACHED state.
 *
 *	The list of major devices is kept on a module-global list
 *	gld_device_list, which has its own lock to protect the list.
 *
 *	When it is necessary to hold more than one mutex at a time, they
 *	are acquired in this "outside in" order:
 *		gld_device_list.gld_devlock
 *		glddev->gld_devlock
 *		macinfo->gldm_maclock
 *		mac_pvt->gldp_txlock
 *
 *	Finally, there are some "volatile" elements of the gld_t structure
 *	used for synchronization between various routines that don't share
 *	the same mutexes.  See the routines for details.  These are:
 *		gld_xwait	between gld_wsrv() and gld_sched()
 *		gld_sched_ran	between gld_wsrv() and gld_sched()
 *		gld_in_unbind	between gld_wput() and wsrv's gld_unbind()
 *		gld_wput_count	between gld_wput() and wsrv's gld_unbind()
 *		gld_in_wsrv	between gld_wput() and gld_wsrv()
 *				(used in conjunction with q->q_first)
 */

/*
 * gld_ioctl (q, mp)
 * handles all ioctl requests passed downstream. This routine is
 * passed a pointer to the message block with the ioctl request in it, and a
 * pointer to the queue so it can respond to the ioctl request with an ack.
 */
int
gld_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp;
	register gld_t *gld;
	gld_mac_info_t *macinfo;
	cred_t *cred;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_ioctl(%p %p)", (void *)q, (void *)mp);
#endif
	gld = (gld_t *)q->q_ptr;
	iocp = (struct iocblk *)mp->b_rptr;
	cred = iocp->ioc_cr;
	switch (iocp->ioc_cmd) {
	case DLIOCRAW:		/* raw M_DATA mode */
		if (cred == NULL || drv_priv(cred) == 0) {
			/* Only do if we have permission to avoid problems */
			TNF_PROBE_1(gld_ioctl_RAW, "gld gld_config", /* */,
			    tnf_opaque, q, q);
			gld->gld_flags |= GLD_RAW;
			DB_TYPE(mp) = M_IOCACK;
			qreply(q, mp);
		} else {
			TNF_PROBE_1(gld_ioctl_RAW_EPERM, "gld gld_config",
			    "gld queue no permission for RAW mode",
			    tnf_opaque, q, q);
			miocnak(q, mp, 0, EPERM);
		}
		break;

	case DL_IOC_HDR_INFO:
				/* fastpath */
		if (gld_global_options & GLD_OPT_NO_FASTPATH) {
			TNF_PROBE_1(gld_ioctl_FAST_NO, "gld gld_config",
			    "gld FASTPATH disabled",
			    tnf_opaque, q, q);
			miocnak(q, mp, 0, EINVAL);
			break;
		}
		gld_fastpath(gld, q, mp);
		break;

/* This should be moved into dlpi.h once it is approved */
#define	DLIOCSPECIAL	(DLIOC|2)	/* Stream wants SMT/MAC packets */

	case DLIOCSPECIAL:		/* Stream wants SMT/MAC packets */
		if (cred == NULL || drv_priv(cred) == 0) {
			TNF_PROBE_1(gld_ioctl_SPECIAL, "gld gld_config", /* */,
			    tnf_opaque, q, q);
			gld->gld_sap = GLD_SPECIAL_SAP;
			DB_TYPE(mp) = M_IOCACK;
			qreply(q, mp);
		} else {
			TNF_PROBE_1(gld_ioctl_SPECIAL_EPERM, "gld gld_config",
			    "gld queue no permission for SPECIAL packets mode",
			    tnf_opaque, q, q);
			miocnak(q, mp, 0, EPERM);
		}
		break;

	default:
		macinfo	 = gld->gld_mac_info;
		if (macinfo == NULL || macinfo->gldm_ioctl == NULL) {
			miocnak(q, mp, 0, EINVAL);
			break;
		}
		mutex_enter(&macinfo->gldm_maclock);
		if (macinfo->gldm_driver_version >= GLD_VERSION_200 ||
		    (macinfo->gldm_options & GLDOPT_V2_IOCTL)) {
			TNF_PROBE_3(gldm_ioctl_start, "gld gld_config gld_9e",
			    /* */,
			    tnf_opaque, mac, macinfo,
			    tnf_opaque, mp, mp,
			    tnf_int, ioc_cmd, iocp->ioc_cmd);

			(void) (*macinfo->gldm_ioctl) (macinfo, q, mp);

			TNF_PROBE_1(gldm_ioctl_end, "gld gld_config gld_9e",
			    /* */,
			    tnf_opaque, mac, macinfo);
		} else {
			TNF_PROBE_3(gldm_ioctl_start, "gld gld_config gld_9e",
			    /* */,
			    tnf_opaque, mac, macinfo,
			    tnf_opaque, mp, mp,
			    tnf_int, ioc_cmd, iocp->ioc_cmd);

			(void) (*macinfo->gldm_ioctl) (q, mp);

			TNF_PROBE_1(gldm_ioctl_end, "gld gld_config gld_9e",
			    /* */,
			    tnf_opaque, mac, macinfo);
		}
		mutex_exit(&macinfo->gldm_maclock);
		break;
	}
	return (0);
}

/*
 * Since the rules for "fastpath" mode don't seem to be documented
 * anywhere, I will describe GLD's rules for fastpath users here:
 *
 * Once in this mode you remain there until close.
 * If you unbind/rebind you should get a new header using DL_IOC_HDR_INFO.
 * You must be bound (DL_IDLE) to transmit.
 * There are other rules not listed above.
 *
 * XXX Where is the FASTPATH consolidation private interface documented?
 */
static void
gld_fastpath(gld_t *gld, queue_t *q, mblk_t *mp)
{
	dl_unitdata_req_t *udp;
	int len;
	gld_interface_t *ifp;
	mblk_t *nmp;
	gld_mac_info_t *macinfo;

	/*
	 * sanity check - we want correct state and valid message
	 */
	if (gld->gld_state != DL_IDLE || (mp->b_cont == NULL) ||
	    MBLKL(mp->b_cont) < sizeof (dl_unitdata_req_t) ||
	    ((dl_unitdata_req_t *)(mp->b_cont->b_rptr))->dl_primitive !=
	    DL_UNITDATA_REQ) {
		TNF_PROBE_1(gld_ioctl_FAST_EINVAL, "gld gld_config",
		    "gld bad FASTPATH req",
		    tnf_opaque, q, q);
		miocnak(q, mp, 0, EINVAL);
		return;
	}

	udp = (dl_unitdata_req_t *)(mp->b_cont->b_rptr);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);

	len = macinfo->gldm_addrlen + abs(macinfo->gldm_saplen);

	if (!MBLKIN(mp->b_cont, udp->dl_dest_addr_offset,
	    udp->dl_dest_addr_length) ||
	    udp->dl_dest_addr_length != len) {
		TNF_PROBE_1(gld_ioctl_FAST_EINVAL, "gld gld_config",
		    "gld bad FASTPATH req 2",
		    tnf_opaque, q, q);
		miocnak(q, mp, 0, EINVAL);
		return;
	}

	/*
	 * We take his fastpath request as a declaration that he will accept
	 * M_DATA messages from us, whether or not we are willing to accept
	 * them from him.  This allows us to have fastpath in one direction
	 * (flow upstream) even on media with Source Routing, where we are
	 * unable to provide a fixed MAC header to be prepended to downstream
	 * flowing packets.  So we set GLD_FAST whether or not we decide to
	 * allow him to send M_DATA down to us.
	 */
	mutex_enter(&macinfo->gldm_maclock);
	gld->gld_flags |= GLD_FAST;
	gld_set_ipq(macinfo);
	mutex_exit(&macinfo->gldm_maclock);

	ifp = ((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->interfacep;

	/* This will fail for Source Routing media */
	/* Also on Ethernet on 802.2 SAPs */
	if ((nmp = (*ifp->mkfastpath)(gld, mp)) == NULL) {
		TNF_PROBE_1(gld_ioctl_FAST_HALF, "gld gld_config",
		    "gld queue FASTPATH upstream only",
		    tnf_opaque, q, q);
		miocnak(q, mp, 0, ENOMEM);
		return;
	}

	/*
	 * Link new mblk in after the "request" mblks.
	 */
	TNF_PROBE_1(gld_ioctl_FAST, "gld gld_config",
	    "gld queue FASTPATH succeeds",
	    tnf_opaque, q, q);

	linkb(mp, nmp);
	miocack(q, mp, msgdsize(mp->b_cont), 0);
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
	int result = DL_BADPRIM;
	int mblkl = MBLKL(mp);

	int dlreq;
	char *dlname = NULL;

	dlp = (union DL_primitives *)mp->b_rptr;
#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE,
		    "gld_cmds(%p, %p):dlp=%p, dlp->dl_primitive=%d",
		    (void *)q, (void *)mp, (void *)dlp, dlp->dl_primitive);
#endif

	/* Make sure we at least have dlp->dl_primitive */
	if ((caddr_t)mp->b_wptr <
	    (caddr_t)&dlp->dl_primitive + sizeof (dlp->dl_primitive)) {
		TNF_PROBE_2(gld_DLPI_BAD, "gld gld_config",
		    "gld got malformed DLPI message",
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp);
		return (DL_BADPRIM);
	}

	dlreq = dlp->dl_primitive;

	switch (dlp->dl_primitive) {
	case DL_BIND_REQ:
		dlname = "BIND";
		if (mblkl < DL_BIND_REQ_SIZE)
			break;
		result = gld_bind(q, mp);
		break;

	case DL_UNBIND_REQ:
		dlname = "UNBIND";
		if (mblkl < DL_UNBIND_REQ_SIZE)
			break;
		result = gld_unbind(q, mp);
		break;

	case DL_UNITDATA_REQ:
		dlname = "UNITDATA";
		if (mblkl < DL_UNITDATA_REQ_SIZE)
			break;
		result = gld_unitdata(q, mp);
		break;

	case DL_INFO_REQ:
		dlname = "INFO";
		if (mblkl < DL_INFO_REQ_SIZE)
			break;
		result = gld_inforeq(q, mp);
		break;

	case DL_ATTACH_REQ:
		dlname = "ATTACH";
		if (mblkl < DL_ATTACH_REQ_SIZE)
			break;
		if (gld->gld_style == DL_STYLE2)
			result = gldattach(q, mp);
		else
			result = DL_NOTSUPPORTED;
		break;

	case DL_DETACH_REQ:
		dlname = "DETACH";
		if (mblkl < DL_DETACH_REQ_SIZE)
			break;
		if (gld->gld_style == DL_STYLE2)
			result = gldunattach(q, mp);
		else
			result = DL_NOTSUPPORTED;
		break;

	case DL_ENABMULTI_REQ:
		dlname = "ENABMULTI";
		if (mblkl < DL_ENABMULTI_REQ_SIZE)
			break;
		result = gld_enable_multi(q, mp);
		break;

	case DL_DISABMULTI_REQ:
		dlname = "DISABMULTI";
		if (mblkl < DL_DISABMULTI_REQ_SIZE)
			break;
		result = gld_disable_multi(q, mp);
		break;

	case DL_PHYS_ADDR_REQ:
		dlname = "PHYS_ADDR";
		if (mblkl < DL_PHYS_ADDR_REQ_SIZE)
			break;
		result = gld_physaddr(q, mp);
		break;

	case DL_SET_PHYS_ADDR_REQ:
		dlname = "SET_PHYS_ADDR";
		if (mblkl < DL_SET_PHYS_ADDR_REQ_SIZE)
			break;
		result = gld_setaddr(q, mp);
		break;

	case DL_PROMISCON_REQ:
		dlname = "PROMISCON";
		if (mblkl < DL_PROMISCON_REQ_SIZE)
			break;
		result = gld_promisc(q, mp, 1);
		break;

	case DL_PROMISCOFF_REQ:
		dlname = "PROMISCOFF";
		if (mblkl < DL_PROMISCOFF_REQ_SIZE)
			break;
		result = gld_promisc(q, mp, 0);
		break;

	case DL_GET_STATISTICS_REQ:
		dlname = "GET_STATISTICS";
		if (mblkl < DL_GET_STATISTICS_REQ_SIZE)
			break;
		result = gld_get_statistics(q, mp);
		break;

	case DL_XID_REQ:
	case DL_XID_RES:
	case DL_TEST_REQ:
	case DL_TEST_RES:
		result = DL_NOTSUPPORTED;
		break;

	default:
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_WARN,
			    "gld_cmds: unknown M_PROTO message: %d",
			    dlp->dl_primitive);
#endif
		result = DL_BADPRIM;
	}

#ifndef	NPROBE
	if (dlreq == DL_UNITDATA_REQ)
		return (result);

	if (result == GLDE_OK) {
		TNF_PROBE_3(gld_DLPI_cmd, "gld gld_config", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_string, DL_REQ, dlname);
	} else if (dlname != NULL) {
		TNF_PROBE_4(gld_DLPI_cmd, "gld gld_config", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_string, DL_REQ, dlname,
		    tnf_int, error, result);
	} else {
		TNF_PROBE_4(gld_DLPI_cmd, "gld gld_config", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_int, DL_UNKNOWN_REQ, dlreq,
		    tnf_int, error, result);
	}
#endif

	return (result);
}

/*
 * gld_bind - determine if a SAP is already allocated and whether it is legal
 * to do the bind at this time
 */
static int
gld_bind(queue_t *q, mblk_t *mp)
{
	uchar_t bind_addr[GLD_MAX_ADDRLEN];
	ulong	sap;
	register dl_bind_req_t *dlp;
	gld_t  *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo = gld->gld_mac_info;

	ASSERT(gld);
	ASSERT(gld->gld_qptr == RD(q));

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_bind(%p %p)", (void *)q, (void *)mp);
#endif

	dlp = (dl_bind_req_t *)mp->b_rptr;
	sap = dlp->dl_sap;

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT)
		cmn_err(CE_NOTE, "gld_bind: lsap=%lx", sap);
#endif

	if (gld->gld_state != DL_UNBOUND) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_bind: bound or not attached (%d)",
				gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}
	ASSERT(macinfo);

	if (dlp->dl_service_mode != DL_CLDLS) {
		return (DL_UNSUPPORTED);
	}
	if (dlp->dl_xidtest_flg & (DL_AUTO_XID | DL_AUTO_TEST)) {
		return (DL_NOAUTO);
	}
	/* XXX Assumes abs(macinfo->gldm_saplen) == 2 */
	ASSERT(abs(macinfo->gldm_saplen) == 2);
	if (sap > 0xffff)
		return (DL_BADSAP);

	/* if we get to here, then the SAP is legal enough */
	mutex_enter(&macinfo->gldm_maclock);
	gld->gld_state = DL_IDLE;	/* bound and ready */
	gld->gld_sap = sap;
	gld_set_ipq(macinfo);

	/*
	 * Get a copy of the current mac addr within
	 * the mutex
	 */
#define	MAC_PVT	((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)
	mac_copy((caddr_t)(MAC_PVT->curr_macaddr),
		(caddr_t)bind_addr, macinfo->gldm_addrlen);

	mutex_exit(&macinfo->gldm_maclock);

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT)
		cmn_err(CE_NOTE, "gld_bind: ok - sap = %d", gld->gld_sap);
#endif

	/* ACK the BIND */

	dlbindack(q, mp, sap,
	    bind_addr,
	    macinfo->gldm_addrlen, 0, 0);

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
	gld_mac_info_t *macinfo = gld->gld_mac_info;

	ASSERT(gld);

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_unbind(%p %p)", (void *)q, (void *)mp);
#endif

	if (gld->gld_state != DL_IDLE) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unbind: wrong state (%d)",
				gld->gld_state);
#endif
		return (DL_OUTSTATE);
	}
	ASSERT(macinfo);

	/*
	 * Avoid unbinding (DL_UNBIND_REQ) while FAST/RAW is inside wput.
	 * See comments above gld_start().
	 */
	gld->gld_in_unbind = 1;		/* disallow wput=>start */
	membar_enter();
	if (gld->gld_wput_count) {
		gld->gld_in_unbind = 0;
		ASSERT(mp);		/* we didn't come from close */
#ifdef GLD_DEBUG
		if (gld_debug & GLDETRACE)
			cmn_err(CE_NOTE, "gld_unbind: defer for wput");
#endif
		(void) putbq(q, mp);
		qenable(q);		/* try again soon */
		return (GLDE_RETRY);
	}

	mutex_enter(&macinfo->gldm_maclock);
	gld->gld_state = DL_UNBOUND;
	gld->gld_sap = 0;
	gld_set_ipq(macinfo);
	mutex_exit(&macinfo->gldm_maclock);

	membar_exit();
	gld->gld_in_unbind = 0;

	/* mp is NULL if we came from close */
	if (mp) {
		gld_flushqueue(q);	/* flush the queues */
		dlokack(q, mp, DL_UNBIND_REQ);
	}
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
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_inforeq(%p %p)", (void *)q, (void *)mp);
#endif
	gld = (gld_t *)q->q_ptr;
	ASSERT(gld);
	glddev = gld->gld_device;
	ASSERT(glddev);

	bufsize = sizeof (dl_info_ack_t) + 2 * glddev->gld_addrlen +
	    abs(glddev->gld_saplen);

	nmp = mexchange(q, mp, bufsize, M_PCPROTO, DL_INFO_ACK);

	if (nmp == NULL) {
		/* mexchange already sent up an merror ENOSR */
		return (GLDE_OK);	/* nothing more to be done */
	}

	nmp->b_wptr = nmp->b_rptr + sizeof (dl_info_ack_t); /* superfluous */
	dlp = (dl_info_ack_t *)nmp->b_rptr;
	bzero((caddr_t)dlp, sizeof (dl_info_ack_t));
	dlp->dl_primitive = DL_INFO_ACK;
	dlp->dl_version = DL_VERSION_2;
	dlp->dl_service_mode = DL_CLDLS;
	dlp->dl_current_state = gld->gld_state;
	dlp->dl_provider_style = gld->gld_style;

	if (gld->gld_state == DL_IDLE || gld->gld_state == DL_UNBOUND) {
		macinfo = gld->gld_mac_info;
		ASSERT(macinfo != NULL);
		dlp->dl_min_sdu = macinfo->gldm_minpkt;
		dlp->dl_max_sdu = macinfo->gldm_maxpkt;
		dlp->dl_mac_type = macinfo->gldm_type;
		dlp->dl_addr_length = macinfo->gldm_addrlen +
		    abs(macinfo->gldm_saplen);
		dlp->dl_sap_length = macinfo->gldm_saplen;

		if (gld->gld_state == DL_IDLE) {
			/*
			 * If we are bound to a non-LLC SAP on any medium
			 * other than Ethernet, then we need room for a
			 * SNAP header.  So we have to adjust the MTU size
			 * accordingly.  XXX I suppose this should be done
			 * in gldutil.c, but it seems likely that this will
			 * always be true for everything GLD supports but
			 * Ethernet.  Check this if you add another medium.
			 */
			if (macinfo->gldm_type != DL_ETHER &&
			    gld->gld_sap > GLD_MAX_802_SAP)
				dlp->dl_max_sdu -= LLC_SNAP_HDR_LEN;

			/* copy macaddr and sap */
			dlp->dl_addr_offset = sizeof (dl_info_ack_t);


			mutex_enter(&macinfo->gldm_maclock);
			mac_copy(((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->
			    curr_macaddr, nmp->b_wptr, macinfo->gldm_addrlen);
			mutex_exit(&macinfo->gldm_maclock);

			nmp->b_wptr += macinfo->gldm_addrlen;
			/*
			 * save the correct number of bytes in the DLSAP
			 * we currently only handle negative sap lengths
			 * so a positive one will just get ignored. XXX
			 */
			switch (macinfo->gldm_saplen) {
			case -1:
				/*
				 * XXX Despite this code I don't think GLD
				 *	is prepared to handle saplen -1.
				 *	See gld_addudind().
				 */
				*(unsigned char *)(nmp->b_wptr) = gld->gld_sap;
				break;
			case -2:
				*(ushort *)(nmp->b_wptr) = gld->gld_sap;
				break;
			default:
				ASSERT(!"inforeq bad saplen");
			}
			nmp->b_wptr += abs(macinfo->gldm_saplen);
			dlp->dl_brdcst_addr_offset =
			    dlp->dl_addr_offset + dlp->dl_addr_length;
		} else {
			dlp->dl_addr_offset = 0;
			dlp->dl_brdcst_addr_offset = sizeof (dl_info_ack_t);
		}

		/* copy broadcast addr */
		dlp->dl_brdcst_addr_length = macinfo->gldm_addrlen;
		mac_copy((caddr_t)macinfo->gldm_broadcast_addr,
			nmp->b_wptr, macinfo->gldm_addrlen);
		nmp->b_wptr += dlp->dl_brdcst_addr_length;
	} else {
		/*
		 * No PPA is attached.
		 * The best we can do is use the values provided
		 * by the first mac that called gld_register.
		 */
		dlp->dl_min_sdu = glddev->gld_minsdu;
		dlp->dl_max_sdu = glddev->gld_maxsdu;
		dlp->dl_mac_type = glddev->gld_type;
		dlp->dl_addr_length = glddev->gld_addrlen
		    + abs(glddev->gld_saplen);
		dlp->dl_sap_length = glddev->gld_saplen;
		dlp->dl_addr_offset = 0;
		dlp->dl_brdcst_addr_offset = sizeof (dl_info_ack_t);
		dlp->dl_brdcst_addr_length = glddev->gld_addrlen;
		mac_copy((caddr_t)glddev->gld_broadcast,
			nmp->b_wptr, glddev->gld_addrlen);
		nmp->b_wptr += dlp->dl_brdcst_addr_length;
	}
	qreply(q, nmp);
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
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	size_t	msglen;
	mblk_t	*nmp;
	gld_interface_t *ifp;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_unitdata(%p %p)", (void *)q, (void *)mp);
#endif

	if (gld->gld_state != DL_IDLE) {
		TNF_PROBE_3(gld_unitdata, "gld gld_config gld_send", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_string, error, "DL_OUTSTATE");
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unitdata: wrong state (%d)",
				gld->gld_state);
#endif
		dluderrorind(q, mp, mp->b_rptr + dlp->dl_dest_addr_offset,
		    dlp->dl_dest_addr_length, DL_OUTSTATE, 0);
		return (GLDE_OK);
	}
	ASSERT(macinfo != NULL);

	if (!MBLKIN(mp, dlp->dl_dest_addr_offset, dlp->dl_dest_addr_length) ||
	    dlp->dl_dest_addr_length !=
	    macinfo->gldm_addrlen + abs(macinfo->gldm_saplen)) {
		TNF_PROBE_3(gld_unitdata, "gld gld_send", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_string, error, "DL_BADADDR");
		dluderrorind(q, mp, mp->b_rptr + dlp->dl_dest_addr_offset,
		    dlp->dl_dest_addr_length, DL_BADADDR, 0);
		return (GLDE_OK);
	}

	msglen = msgdsize(mp);
	if (msglen == 0 || msglen > macinfo->gldm_maxpkt) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unitdata: bad msglen (%d)",
				(int)msglen);
#endif
		TNF_PROBE_3(gld_unitdata, "gld gld_send", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_string, error, "DL_BADDATA");
		dluderrorind(q, mp, mp->b_rptr + dlp->dl_dest_addr_offset,
		    dlp->dl_dest_addr_length, DL_BADDATA, 0);
		return (GLDE_OK);
	}

	ASSERT(mp->b_cont != NULL);	/* because msgdsize(mp) is nonzero */

	ifp = ((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->interfacep;

	/*
	 * Prepend a valid header for transmission
	 */
	if ((nmp = (*ifp->mkunitdata)(gld, mp)) == NULL) {
#ifdef GLD_DEBUG
		if (gld_debug & GLDERRS)
			cmn_err(CE_NOTE, "gld_unitdata: mkunitdata failed.");
#endif
		TNF_PROBE_3(gld_unitdata, "gld gld_send", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_string, error, "mkunitdata failed");
		dluderrorind(q, mp, mp->b_rptr + dlp->dl_dest_addr_offset,
		    dlp->dl_dest_addr_length, DL_SYSERR, ENOSR);
		return (GLDE_OK);
	}

	if (gld_start(q, nmp, GLD_LOCK) == GLD_NORESOURCES) {
		TNF_PROBE_3(gld_unitdata_retry, "gld gld_send", /* */,
		    tnf_opaque, q, q,
		    tnf_opaque, mp, mp,
		    tnf_string, error, "GLD_NORESOURCES (will retry)");
		(void) putbq(q, nmp);
		qenable(q);
		return (GLDE_RETRY);
	}

	TNF_PROBE_2(gld_unitdata_success, "gld gld_send", /* */,
	    tnf_opaque, q, q,
	    tnf_opaque, mp, mp);

	return (GLDE_OK);
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
	gld_mac_pvt_t *mac_pvt;

	at = (dl_attach_req_t *)mp->b_rptr;

	if (gld->gld_state != DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}
	ASSERT(!gld->gld_mac_info);

	glddev = gld->gld_device;
	mutex_enter(&glddev->gld_devlock);
	for (mac = glddev->gld_mac_next;
	    mac != (gld_mac_info_t *)&glddev->gld_mac_next;
	    mac = mac->gldm_next) {
		ASSERT(mac);
		if (!(mac->gldm_GLD_flags & GLD_MAC_READY))
			continue;	/* this one's not ready yet */
		if (mac->gldm_ppa == at->dl_ppa) {
			/*
			 * We found the correct PPA
			 * Take the stream off the per-driver-class list
			 */
			gldremque(gld);

			mac_pvt = (gld_mac_pvt_t *)mac->gldm_mac_pvt;

			/*
			 * We must hold the mutex to prevent multiple calls
			 * to the reset and start routines.
			 */
			mutex_enter(&mac->gldm_maclock);
			if (mac_pvt->nstreams == 0) {
				if (gld_start_mac(mac) != GLD_SUCCESS) {
					mutex_exit(&mac->gldm_maclock);
					gldinsque(gld, glddev->gld_str_prev);
					mutex_exit(&glddev->gld_devlock);
					dlerrorack(q, mp, DL_ATTACH_REQ,
					    DL_SYSERR, EIO);
					return (GLDE_OK);
				}
			}
			mac_pvt->nstreams++;
			gld->gld_mac_info = mac;
			gld->gld_state = DL_UNBOUND;
			gldinsque(gld, mac_pvt->gld_str_next);
			mutex_exit(&mac->gldm_maclock);

#ifdef GLD_DEBUG
			if (gld_debug & GLDPROT) {
				cmn_err(CE_NOTE, "gldattach(%p, %p, PPA = %d)",
				    (void *)q, (void *)mp, mac->gldm_ppa);
			}
#endif
			mutex_exit(&glddev->gld_devlock);
			dlokack(q, mp, DL_ATTACH_REQ);
			return (GLDE_OK);
		}
	}
	mutex_exit(&glddev->gld_devlock);
	return (DL_BADPPA);
}

/*
 * gldunattach(q, mp)
 * DLPI DL_DETACH_REQ
 * detaches the mac layer from the stream
 */
int
gldunattach(queue_t *q, mblk_t *mp)
{
	gld_t  *gld = (gld_t *)q->q_ptr;
	glddev_t *glddev = gld->gld_device;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	int	state = gld->gld_state;
	int	i, change = 0;
	gld_mac_pvt_t *mac_pvt;

	if (state != DL_UNBOUND)
		return (DL_OUTSTATE);

	ASSERT(macinfo != NULL);
	ASSERT(gld->gld_sap == 0);
	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gldunattach(%p, %p, PPA = %d)",
		    (void *)q, (void *)mp, macinfo->gldm_ppa);
	}
#endif

	mutex_enter(&macinfo->gldm_maclock);

	if (gld->gld_mcast) {
		for (i = 0; i < gld->gld_multicnt; i++) {
			gld_mcast_t *mcast;

			if ((mcast = gld->gld_mcast[i]) != NULL) {
				ASSERT(mcast->gldm_refcnt);
				/* disable from stream and possibly lower */
				gld_send_disable_multi(gld, macinfo, mcast);
			}
		}
		kmem_free(gld->gld_mcast,
		    sizeof (gld_mcast_t *) * gld->gld_multicnt);
		gld->gld_mcast = NULL;
		gld->gld_multicnt = 0;
	}

	/* cleanup remnants of promiscuous mode */
	if (gld->gld_flags & GLD_PROM_PHYS &&
	    --mac_pvt->nprom == 0)
		change++;

	if (gld->gld_flags & GLD_PROM_MULT &&
	    --mac_pvt->nprom_multi == 0)
		change++;

	/* XXX I think spec allows promisc in unattached state */
	gld->gld_flags &= ~(GLD_PROM_PHYS | GLD_PROM_SAP | GLD_PROM_MULT);

	if (change) {
		TNF_PROBE_2(gldm_set_promisc_start, "gld gld_config gld_9e",
		    "gldunattach calls gldm_set_promiscuous",
		    tnf_opaque, mac, macinfo,
		    tnf_string, _, mac_pvt->nprom ? "PHYS" :
		    mac_pvt->nprom_multi ? "MULTI" : "NONE");

		(void) (*macinfo->gldm_set_promiscuous)(macinfo,
		    mac_pvt->nprom ? GLD_MAC_PROMISC_PHYS :
		    mac_pvt->nprom_multi ? GLD_MAC_PROMISC_MULTI :
		    GLD_MAC_PROMISC_NONE);

		TNF_PROBE_1(gldm_set_promisc_end, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo);
	}

	mutex_exit(&macinfo->gldm_maclock);

	/*
	 * We need to hold both locks when modifying the mac stream list
	 * to protect findminor as well as everyone else.
	 */
	mutex_enter(&glddev->gld_devlock);
	mutex_enter(&macinfo->gldm_maclock);

	/* cleanup mac layer if last stream */
	if (--mac_pvt->nstreams == 0) {
		TNF_PROBE_1(gldm_stop_start, "gld gld_config gld_9e", /* */,
		    tnf_opaque, mac, macinfo);

		(void) (*macinfo->gldm_stop)(macinfo);
		macinfo->gldm_GLD_flags &= ~GLD_INTR_WAIT;

		TNF_PROBE_1(gldm_stop_end, "gld gld_config gld_9e", /* */,
		    tnf_opaque, mac, macinfo);
	}

	/* make sure no references to this gld for gld_v0_sched */
	if (mac_pvt->last_sched == gld)
		mac_pvt->last_sched = NULL;

	/* disassociate this stream with its mac */
	gldremque(gld);
	gld->gld_mac_info = NULL;
	gld->gld_state = DL_UNATTACHED;
	gld_set_ipq(macinfo);

	mutex_exit(&macinfo->gldm_maclock);

	/* put the stream on the unattached Style 2 list */
	gldinsque(gld, glddev->gld_str_prev);

	mutex_exit(&glddev->gld_devlock);

	/* There will be no mp if we were called from close */
	if (mp) {
		dlokack(q, mp, DL_DETACH_REQ);
	}
	return (GLDE_OK);
}

/*
 * gld_enable_multi (q, mp)
 * Enables multicast address on the stream.  If the mac layer
 * isn't enabled for this address, enable at that level as well.
 */
static int
gld_enable_multi(queue_t *q, mblk_t *mp)
{
	gld_t  *gld = (gld_t *)q->q_ptr;
	glddev_t *glddev;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	unsigned char *maddr;
	dl_enabmulti_req_t *multi;
	gld_mcast_t *mcast;
	int	i, rc;
	gld_mac_pvt_t *mac_pvt;

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gld_enable_multi(%p, %p)", (void *)q,
		    (void *)mp);
	}
#endif

	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	ASSERT(macinfo != NULL);
	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;

	if (macinfo->gldm_set_multicast == NULL) {
		return (DL_UNSUPPORTED);
	}

	multi = (dl_enabmulti_req_t *)mp->b_rptr;

	if (!MBLKIN(mp, multi->dl_addr_offset, multi->dl_addr_length) ||
	    multi->dl_addr_length != macinfo->gldm_addrlen)
		return (DL_BADADDR);

	/* request appears to be valid */

	glddev = mac_pvt->major_dev;
	ASSERT(glddev == gld->gld_device);

	maddr = mp->b_rptr + multi->dl_addr_offset;

	/*
	 * The multicast addresses live in a per-device table, along
	 * with a reference count.  Each stream has a table that
	 * points to entries in the device table, with the reference
	 * count reflecting the number of streams pointing at it.  If
	 * this multicast address is already in the per-device table,
	 * all we have to do is point at it.
	 */
	mutex_enter(&macinfo->gldm_maclock);

	/* does this address appear in current table? */
	if (gld->gld_mcast == NULL) {
		/* no mcast addresses -- allocate table */
		gld->gld_mcast = GETSTRUCT(gld_mcast_t *,
					    glddev->gld_multisize);
		if (gld->gld_mcast == NULL) {
			mutex_exit(&macinfo->gldm_maclock);
			dlerrorack(q, mp, DL_ENABMULTI_REQ, DL_SYSERR, ENOSR);
			return (GLDE_OK);
		}
		gld->gld_multicnt = glddev->gld_multisize;
	} else {
		for (i = 0; i < gld->gld_multicnt; i++) {
			if (gld->gld_mcast[i] &&
			    mac_eq(gld->gld_mcast[i]->gldm_addr,
				maddr, macinfo->gldm_addrlen)) {
				/* this is a match -- just succeed */
				ASSERT(gld->gld_mcast[i]->gldm_refcnt);
				mutex_exit(&macinfo->gldm_maclock);
				dlokack(q, mp, DL_ENABMULTI_REQ);
				return (GLDE_OK);
			}
		}
	}

	/*
	 * it wasn't in the stream so check to see if the mac layer has it
	 */
	mcast = NULL;
	if (mac_pvt->mcast_table == NULL) {
		mac_pvt->mcast_table = GETSTRUCT(gld_mcast_t,
						glddev->gld_multisize);
		if (mac_pvt->mcast_table == NULL) {
			mutex_exit(&macinfo->gldm_maclock);
			dlerrorack(q, mp, DL_ENABMULTI_REQ, DL_SYSERR, ENOSR);
			return (GLDE_OK);
		}
	} else {
		for (i = 0; i < glddev->gld_multisize; i++) {
			if (mac_pvt->mcast_table[i].gldm_refcnt &&
			    mac_eq(mac_pvt->mcast_table[i].gldm_addr,
			    maddr, macinfo->gldm_addrlen)) {
				mcast = &mac_pvt->mcast_table[i];
				break;
			}
		}
	}
	if (mcast == NULL) {
		/* not in mac layer -- find an empty mac slot to fill in */
		for (i = 0; i < glddev->gld_multisize; i++) {
			if (mac_pvt->mcast_table[i].gldm_refcnt == 0) {
				mcast = &mac_pvt->mcast_table[i];
				mac_copy(maddr, mcast->gldm_addr,
				    macinfo->gldm_addrlen);
				break;
			}
		}
	}
	if (mcast == NULL) {
		/* couldn't get a mac layer slot */
		mutex_exit(&macinfo->gldm_maclock);
		return (DL_TOOMANY);
	}

	/* now we have a mac layer slot in mcast -- get a stream slot */
	for (i = 0; i < gld->gld_multicnt; i++) {
		if (gld->gld_mcast[i] != NULL)
			continue;
		/* found an empty slot */
		if (!mcast->gldm_refcnt) {
			/* set mcast in hardware */
			unsigned char cmaddr[GLD_MAX_ADDRLEN];
			char pbuf[3*GLD_MAX_ADDRLEN];
			ASSERT(sizeof (cmaddr) >= macinfo->gldm_addrlen);
			cmac_copy(maddr, cmaddr,
			    macinfo->gldm_addrlen, macinfo);

			TNF_PROBE_3(gldm_set_multi_start,
			    "gld gld_config gld_9e", /* */,
			    tnf_opaque, mac, macinfo,
			    tnf_string, mcaddr,
				gld_macaddr_sprintf(pbuf, cmaddr,
				    macinfo->gldm_addrlen),
			    tnf_string, _, "ENABLE");

			rc = (*macinfo->gldm_set_multicast)
			    (macinfo, cmaddr, GLD_MULTI_ENABLE);

			TNF_PROBE_2(gldm_set_multi_end,
			    "gld gld_config gld_9e", /* */,
			    tnf_opaque, mac, macinfo,
			    tnf_int, rc, rc);

			if (macinfo->gldm_driver_version < GLD_VERSION_200) {
				rc = GLD_SUCCESS;
			} else if (rc == GLD_NOTSUPPORTED) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_NOTSUPPORTED);
			} else if (rc == GLD_NORESOURCES) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_TOOMANY);
			} else if (rc == GLD_BADARG) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_BADADDR);
			} else if (rc != GLD_SUCCESS) {
				mutex_exit(&macinfo->gldm_maclock);
				dlerrorack(q, mp, DL_ENABMULTI_REQ,
				    DL_SYSERR, EIO);
				return (GLDE_OK);
			}
		}
		gld->gld_mcast[i] = mcast;
		mcast->gldm_refcnt++;
		mutex_exit(&macinfo->gldm_maclock);
		dlokack(q, mp, DL_ENABMULTI_REQ);
		return (GLDE_OK);
	}

	/* couldn't get a stream slot */
	mutex_exit(&macinfo->gldm_maclock);
	return (DL_TOOMANY);
}


/*
 * gld_disable_multi (q, mp)
 * Disable the multicast address on the stream.  If last
 * reference for the mac layer, disable there as well.
 */
static int
gld_disable_multi(queue_t *q, mblk_t *mp)
{
	gld_t  *gld;
	gld_mac_info_t *macinfo;
	unsigned char *maddr;
	dl_disabmulti_req_t *multi;
	int i;
	gld_mcast_t *mcast;

#ifdef GLD_DEBUG
	if (gld_debug & GLDPROT) {
		cmn_err(CE_NOTE, "gld_disable_multi(%p, %p)", (void *)q,
		    (void *)mp);
	}
#endif

	gld = (gld_t *)q->q_ptr;
	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	macinfo = gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	if (macinfo->gldm_set_multicast == NULL) {
		return (DL_UNSUPPORTED);
	}

	multi = (dl_disabmulti_req_t *)mp->b_rptr;

	if (!MBLKIN(mp, multi->dl_addr_offset, multi->dl_addr_length) ||
	    multi->dl_addr_length != macinfo->gldm_addrlen)
		return (DL_BADADDR);

	maddr = mp->b_rptr + multi->dl_addr_offset;

	/* request appears to be valid */
	/* does this address appear in current table? */
	mutex_enter(&macinfo->gldm_maclock);
	if (gld->gld_mcast != NULL) {
		for (i = 0; i < gld->gld_multicnt; i++)
			if (((mcast = gld->gld_mcast[i]) != NULL) &&
			    mac_eq(mcast->gldm_addr,
			    maddr, macinfo->gldm_addrlen)) {
				ASSERT(mcast->gldm_refcnt);
				gld_send_disable_multi(gld, macinfo, mcast);
				gld->gld_mcast[i] = NULL;
				mutex_exit(&macinfo->gldm_maclock);
				dlokack(q, mp, DL_DISABMULTI_REQ);
				return (GLDE_OK);
			}
	}
	mutex_exit(&macinfo->gldm_maclock);
	return (DL_NOTENAB); /* not an enabled address */
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
	int rc;
	char pbuf[3*GLD_MAX_ADDRLEN];
#ifdef lint
	gld = gld;
#endif

	ASSERT(macinfo != NULL);
	ASSERT(mutex_owned(&macinfo->gldm_maclock));
	ASSERT(mcast != NULL);
	ASSERT(mcast->gldm_refcnt);
	if (!mcast->gldm_refcnt) {
		return;			/* "cannot happen" */
	}

	if (--mcast->gldm_refcnt > 0) {
		return;
	}

	/*
	 * This must be converted from canonical form to device form.
	 * The refcnt is now zero so we can trash the data.
	 */
	if (macinfo->gldm_options & GLDOPT_CANONICAL_ADDR)
		gld_bitreverse(mcast->gldm_addr, macinfo->gldm_addrlen);

	TNF_PROBE_3(gldm_set_multi_start, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_string, mcaddr,
		gld_macaddr_sprintf(pbuf, mcast->gldm_addr,
		    macinfo->gldm_addrlen),
	    tnf_string, _, "DISABLE");

	/* XXX Ought to check for GLD_NORESOURCES or GLD_FAILURE */
	rc = (*macinfo->gldm_set_multicast)
	    (macinfo, mcast->gldm_addr, GLD_MULTI_DISABLE);

	TNF_PROBE_2(gldm_set_multi_end, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_int, rc_ignored, rc);
}

/*
 * gld_promisc (q, mp, on)
 *	enable or disable the use of promiscuous mode with the hardware
 */
static int
gld_promisc(queue_t *q, mblk_t *mp, int on)
{
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	int	change = 0, rc = GLD_SUCCESS;
	gld_t  *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	gld_mac_pvt_t *mac_pvt;

#ifdef GLD_DEBUG
	if (gld_debug & GLDTRACE)
		cmn_err(CE_NOTE, "gld_promisc(%p, %p, %x)", (void *)q,
		    (void *)mp, on);
#endif

	/* XXX I think spec allows promisc in unattached state */
	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	ASSERT(macinfo != NULL);
	ASSERT(mp != NULL);

	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;

	mutex_enter(&macinfo->gldm_maclock);
	if (on) {
		switch (prim->promiscon_req.dl_level) {
		case DL_PROMISC_PHYS:
			if (!(gld->gld_flags & GLD_PROM_PHYS)) {
				if (mac_pvt->nprom++ == 0)
					change++;
				gld->gld_flags |= GLD_PROM_PHYS;
			}
			break;
		case DL_PROMISC_MULTI:
			if (!(gld->gld_flags & GLD_PROM_MULT)) {
				if (mac_pvt->nprom_multi++ == 0)
					change++;
				gld->gld_flags |= GLD_PROM_MULT;
			}
			break;
		case DL_PROMISC_SAP:
			gld->gld_flags |= GLD_PROM_SAP;
			break;
		default:
			mutex_exit(&macinfo->gldm_maclock);
			return (DL_UNSUPPORTED);	/* this is an error */
		}
	} else {
		switch (prim->promiscon_req.dl_level) {
		case DL_PROMISC_PHYS:
			if (!(gld->gld_flags & GLD_PROM_PHYS)) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_NOTENAB);
			}
			if (--mac_pvt->nprom == 0)
				change++;
			gld->gld_flags &= ~GLD_PROM_PHYS;
			break;
		case DL_PROMISC_MULTI:
			if (!(gld->gld_flags & GLD_PROM_MULT)) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_NOTENAB);
			}
			if (--mac_pvt->nprom_multi == 0)
				change++;
			gld->gld_flags &= ~GLD_PROM_MULT;
			break;
		case DL_PROMISC_SAP:
			if (!(gld->gld_flags & GLD_PROM_SAP)) {
				mutex_exit(&macinfo->gldm_maclock);
				return (DL_NOTENAB);
			}
			gld->gld_flags &= ~GLD_PROM_SAP;
			break;
		default:
			mutex_exit(&macinfo->gldm_maclock);
			return (DL_UNSUPPORTED);	/* this is an error */
		}
	}
	if (change) {
		TNF_PROBE_2(gldm_set_promisc_start, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_string, _, mac_pvt->nprom ? "PHYS" :
		    mac_pvt->nprom_multi ? "MULTI" : "NONE");

		rc = (*macinfo->gldm_set_promiscuous)(macinfo,
		    mac_pvt->nprom ? GLD_MAC_PROMISC_PHYS :
		    mac_pvt->nprom_multi ? GLD_MAC_PROMISC_MULTI :
		    GLD_MAC_PROMISC_NONE);

		TNF_PROBE_2(gldm_set_promisc_end, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_int, rc, rc);
	}
	gld_set_ipq(macinfo);
	mutex_exit(&macinfo->gldm_maclock);

	if (rc == GLD_NOTSUPPORTED)
		return (DL_NOTSUPPORTED);

	if (rc == GLD_NORESOURCES)
		dlerrorack(q, mp, on ? DL_PROMISCON_REQ : DL_PROMISCOFF_REQ,
		    DL_SYSERR, ENOSR);
	else if (rc == GLD_FAILURE)
		dlerrorack(q, mp, on ? DL_PROMISCON_REQ : DL_PROMISCOFF_REQ,
		    DL_SYSERR, EIO);
	else
		dlokack(q, mp, on ? DL_PROMISCON_REQ : DL_PROMISCOFF_REQ);

	return (GLDE_OK);
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
	unsigned char addr[GLD_MAX_ADDRLEN];

	if (gld->gld_state == DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}

	macinfo = (gld_mac_info_t *)gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	ASSERT(macinfo->gldm_addrlen <= GLD_MAX_ADDRLEN);

	switch (prim->physaddr_req.dl_addr_type) {
	case DL_FACT_PHYS_ADDR:
		mac_copy((caddr_t)macinfo->gldm_vendor_addr,
		    (caddr_t)addr, macinfo->gldm_addrlen);
		break;
	case DL_CURR_PHYS_ADDR:
		/* make a copy so we don't hold the mutex across qreply */
		mutex_enter(&macinfo->gldm_maclock);
		mac_copy((caddr_t)
		    ((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)->curr_macaddr,
		    (caddr_t)addr, macinfo->gldm_addrlen);
		mutex_exit(&macinfo->gldm_maclock);
		break;
	default:
		return (DL_BADPRIM);
	}
	dlphysaddrack(q, mp, (caddr_t)addr, macinfo->gldm_addrlen);
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
	gld_mac_pvt_t *mac_pvt;
	union DL_primitives *prim = (union DL_primitives *)mp->b_rptr;
	unsigned char *addr;
	unsigned char cmaddr[GLD_MAX_ADDRLEN];
	char pbuf[3*GLD_MAX_ADDRLEN];
	int rc;

	if (gld->gld_state == DL_UNATTACHED) {
		return (DL_OUTSTATE);
	}

	macinfo = (gld_mac_info_t *)gld->gld_mac_info;
	ASSERT(macinfo != NULL);
	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;

	if (!MBLKIN(mp, prim->set_physaddr_req.dl_addr_offset,
	    prim->set_physaddr_req.dl_addr_length) ||
	    prim->set_physaddr_req.dl_addr_length != macinfo->gldm_addrlen)
		return (DL_BADADDR);

	mutex_enter(&macinfo->gldm_maclock);

	/* now do the set at the hardware level */
	addr = mp->b_rptr + prim->set_physaddr_req.dl_addr_offset;
	ASSERT(sizeof (cmaddr) >= macinfo->gldm_addrlen);
	cmac_copy(addr, cmaddr, macinfo->gldm_addrlen, macinfo);

	if (macinfo->gldm_driver_version >= GLD_VERSION_200) {
		TNF_PROBE_2(gldm_set_mac_addr_start, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_string, macaddr,
		    gld_macaddr_sprintf(pbuf, cmaddr, macinfo->gldm_addrlen));

		rc = (*macinfo->gldm_set_mac_addr)(macinfo, cmaddr);

		TNF_PROBE_2(gldm_set_mac_addr_end, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_int, rc, rc);

		if (rc == GLD_SUCCESS)
			mac_copy(addr, mac_pvt->curr_macaddr,
			    macinfo->gldm_addrlen);
	} else {
		/* v0 drivers take the mac address from gldm_macaddr */
		/* Fortunately v0 does not support CANONICAL, so no cmac_copy */
		mac_copy(addr, mac_pvt->curr_macaddr, macinfo->gldm_addrlen);

		TNF_PROBE_2(gldm_set_mac_addr_start, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_string, macaddr,
		    gld_macaddr_sprintf(pbuf, addr, macinfo->gldm_addrlen));

		(void) (*macinfo->gldm_set_mac_addr)(macinfo);

		TNF_PROBE_2(gldm_set_mac_addr_end, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_string, rc, "none");

		rc = GLD_SUCCESS;
	}

	mutex_exit(&macinfo->gldm_maclock);

	if (rc == GLD_NOTSUPPORTED)
		return (DL_NOTSUPPORTED);
	if (rc == GLD_BADARG)
		return (DL_BADADDR);

	if (rc == GLD_NORESOURCES)
		dlerrorack(q, mp, DL_SET_PHYS_ADDR_REQ, DL_SYSERR, ENOSR);
	else if (rc == GLD_FAILURE)
		dlerrorack(q, mp, DL_SET_PHYS_ADDR_REQ, DL_SYSERR, EIO);
	else
		dlokack(q, mp, DL_SET_PHYS_ADDR_REQ);
	return (GLDE_OK);
}

int
gld_get_statistics(queue_t *q, mblk_t *mp)
{
	dl_get_statistics_ack_t *dlsp;
	gld_t  *gld = (gld_t *)q->q_ptr;
	gld_mac_info_t *macinfo = gld->gld_mac_info;
	gld_mac_pvt_t *mac_pvt;

	if (gld->gld_state == DL_UNATTACHED)
		return (DL_OUTSTATE);

	ASSERT(macinfo != NULL);

	mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	(void) gld_update_kstat(mac_pvt->kstatp, KSTAT_READ);

	mp = mexchange(q, mp, DL_GET_STATISTICS_ACK_SIZE +
	    sizeof (struct gldkstats), M_PCPROTO, DL_GET_STATISTICS_ACK);

	if (mp == NULL)
		return (GLDE_OK);	/* mexchange already sent merror */

	dlsp = (dl_get_statistics_ack_t *)mp->b_rptr;
	dlsp->dl_primitive = DL_GET_STATISTICS_ACK;
	dlsp->dl_stat_length = sizeof (struct gldkstats);
	dlsp->dl_stat_offset = DL_GET_STATISTICS_ACK_SIZE;

	mutex_enter(&macinfo->gldm_maclock);
	bcopy((caddr_t)mac_pvt->kstatp->ks_data,
	    (caddr_t)(mp->b_rptr + DL_GET_STATISTICS_ACK_SIZE),
	    sizeof (struct gldkstats));
	mutex_exit(&macinfo->gldm_maclock);

	qreply(q, mp);
	return (GLDE_OK);
}

/* =================================================== */
/* misc utilities, some requiring various mutexes held */
/* =================================================== */

/*
 * Initialize and start the driver.
 */
static int
gld_start_mac(gld_mac_info_t *macinfo)
{
	int	rc;
	unsigned char cmaddr[GLD_MAX_ADDRLEN];
	char pbuf[3*GLD_MAX_ADDRLEN];

	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	TNF_PROBE_1(gldm_reset_start, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo);

	rc = (*macinfo->gldm_reset)(macinfo);

	TNF_PROBE_2(gldm_reset_end, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_int, rc, rc);

	if (rc != GLD_SUCCESS &&
	    macinfo->gldm_driver_version >= GLD_VERSION_200)
		return (GLD_FAILURE);

	/* v0 drivers are responsible for setting macaddr if reset trashed it */
	/* But for v2 we need to set the addr after we reset the device */
	if (macinfo->gldm_driver_version >= GLD_VERSION_200) {
		ASSERT(sizeof (cmaddr) >= macinfo->gldm_addrlen);
		cmac_copy(((gld_mac_pvt_t *)macinfo->gldm_mac_pvt)
		    ->curr_macaddr, cmaddr, macinfo->gldm_addrlen, macinfo);

		TNF_PROBE_2(gldm_set_mac_addr_start, "gld gld_config gld_9e",
		    "gld_start_mac calls gldm_set_mac_addr",
		    tnf_opaque, mac, macinfo,
		    tnf_string, macaddr,
		    gld_macaddr_sprintf(pbuf, cmaddr, macinfo->gldm_addrlen));

		rc = (*macinfo->gldm_set_mac_addr)(macinfo, cmaddr);

		TNF_PROBE_2(gldm_set_mac_addr_end, "gld gld_config gld_9e",
		    /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_int, rc, rc);

		ASSERT(rc != GLD_BADARG);  /* this address was good before */
		if (rc != GLD_SUCCESS && rc != GLD_NOTSUPPORTED)
			return (GLD_FAILURE);
	}

	TNF_PROBE_1(gldm_start_start, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo);

	rc = (*macinfo->gldm_start)(macinfo);

	TNF_PROBE_2(gldm_start_end, "gld gld_config gld_9e", /* */,
	    tnf_opaque, mac, macinfo,
	    tnf_int, rc, rc);

	if (rc != GLD_SUCCESS &&
	    macinfo->gldm_driver_version >= GLD_VERSION_200)
		return (GLD_FAILURE);

	return (GLD_SUCCESS);
}

/*
 * gld_set_ipq will set a pointer to the queue which is bound to the
 * IP sap if:
 * o the device type is ethernet.
 * o there is no stream in SAP promiscuous mode.
 * o there is exactly one stream bound to the IP sap.
 * o the stream is in "fastpath" mode.
 */
static void
gld_set_ipq(gld_mac_info_t *macinfo)
{
	gld_t		*gld;
	queue_t		*ipq = NULL;
	int		ok = 1;
	gld_mac_pvt_t *mac_pvt = (gld_mac_pvt_t *)macinfo->gldm_mac_pvt;
	queue_t		*old_ipq = mac_pvt->ipq;

	ASSERT(mutex_owned(&macinfo->gldm_maclock));

	if (gld_global_options & GLD_OPT_NO_IPQ)
		ok = 0;

	/* The ipq code in gld_recv() is intimate with ethernet */
	if (macinfo->gldm_type != DL_ETHER)
		ok = 0;

	/* Try to find a single stream eligible to receive IP packets */
	for (gld = mac_pvt->gld_str_next;
	    gld != (gld_t *)&mac_pvt->gld_str_next && ok == 1;
	    gld = gld->gld_next) {
		if (gld->gld_state != DL_IDLE)
			continue;	/* not eligible to receive */
		if (gld->gld_flags & GLD_STR_CLOSING)
			continue;	/* not eligible to receive */
		if (gld->gld_flags & GLD_PROM_SAP)
			ok = 0;
		if (gld->gld_sap == ETHERTYPE_IP)
			if (ipq == NULL && (gld->gld_flags & GLD_FAST))
				ipq = gld->gld_qptr;
			else
				ok = 0;
	}

	mac_pvt->ipq = ok ? ipq : NULL;

#ifndef	NPROBE
	if (mac_pvt->ipq != old_ipq) {
		TNF_PROBE_2(gld_set_ipq, "gld gld_config", /* */,
		    tnf_opaque, mac, macinfo,
		    tnf_opaque, ipq, mac_pvt->ipq);
	}
#endif
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
	/* XXX Should these be FLUSHALL? */
	flushq(q, FLUSHDATA);
	flushq(WR(q), FLUSHDATA);
	/* flush all the queues upstream */
	(void) putctl1(q, M_FLUSH, FLUSHRW);
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

	ASSERT(mutex_owned(&gld_device_list.gld_devlock));

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

	ASSERT(mutex_owned(&device->gld_devlock));

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
	ASSERT(!"gld_findppa failed");
	/*NOTREACHED*/
}

/*
 * gld_findminor(device)
 * Returns a minor number currently unused by any stream in the current
 * device class (major) list.
 */
static int
gld_findminor(glddev_t *device)
{
	gld_t		*next;
	gld_mac_info_t	*nextmac;
	int		minor;

	ASSERT(mutex_owned(&device->gld_devlock));

	/* The fast way */
	if (device->gld_nextminor <= GLD_MAXMIN &&
	    device->gld_nextminor >= GLD_PPA_INIT)
		return (device->gld_nextminor++);

	/* The steady way */
	for (minor = GLD_PPA_INIT; minor <= GLD_MAXMIN; minor++) {
		/* Search all unattached streams */
		for (next = device->gld_str_next;
		    next != (gld_t *)&device->gld_str_next;
		    next = next->gld_next) {
			if (minor == next->gld_minor)
				goto nextminor;
		}
		/* Search all attached streams; we don't need maclock because */
		/* mac stream list is protected by devlock as well as maclock */
		for (nextmac = device->gld_mac_next;
		    nextmac != (gld_mac_info_t *)&device->gld_mac_next;
		    nextmac = nextmac->gldm_next) {
			if (!(nextmac->gldm_GLD_flags & GLD_MAC_READY))
				continue;	/* this one's not ready yet */
			for (next = ((gld_mac_pvt_t *)
			    nextmac->gldm_mac_pvt)->gld_str_next;
			    next != (gld_t *)&((gld_mac_pvt_t *)
			    nextmac->gldm_mac_pvt)->gld_str_next;
			    next = next->gld_next) {
				if (minor == next->gld_minor)
					goto nextminor;
			}
		}

		return (minor);
nextminor:
		/* don't need to do anything */
		;
	}
	cmn_err(CE_WARN, "GLD ran out of minor numbers for %s",
		device->gld_name);
	return (0);
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

/*
 * gld_bitrevcopy()
 * This is essentialy bcopy, with the ability to bit reverse the
 * the source bytes. The MAC addresses bytes as transmitted by FDDI
 * interfaces are bit reversed.
 */
void
gld_bitrevcopy(register caddr_t src, register caddr_t target, size_t n)
{
	while (n--)
		*target++ = bit_rev[(uchar_t)*src++];
}

/*
 * gld_bitreverse()
 * Convert the bit order by swaping all the bits, using a
 * lookup table.
 */
void
gld_bitreverse(register u_char *rptr, register size_t n)
{
	while (n--) {
		*rptr = bit_rev[*rptr];
		rptr++;
	}
}

char *
gld_macaddr_sprintf(char *etherbuf, unsigned char *ap, int len)
{
	register int i;
	register char *cp = etherbuf;
	static char digits[] = "0123456789abcdef";

	for (i = 0; i < len; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}

#ifdef GLD_DEBUG
static void
gld_check_assertions()
{
	glddev_t	*dev;
	gld_mac_info_t	*mac;
	gld_t		*str;

	mutex_enter(&gld_device_list.gld_devlock);

	for (dev = gld_device_list.gld_next;
	    dev != (glddev_t *)&gld_device_list.gld_next;
	    dev = dev->gld_next) {
		mutex_enter(&dev->gld_devlock);
		ASSERT(dev->gld_broadcast != NULL);
		for (str = dev->gld_str_next;
		    str != (gld_t *)&dev->gld_str_next;
		    str = str->gld_next) {
			ASSERT(str->gld_device == dev);
			ASSERT(str->gld_mac_info == NULL);
			ASSERT(str->gld_qptr != NULL);
			ASSERT(str->gld_minor >= GLD_PPA_INIT);
			ASSERT(str->gld_multicnt == 0);
			ASSERT(str->gld_mcast == NULL);
			ASSERT(!(str->gld_flags &
			    (GLD_PROM_PHYS|GLD_PROM_MULT|GLD_PROM_SAP)));
			ASSERT(str->gld_sap == 0);
			ASSERT(str->gld_state == DL_UNATTACHED);
		}
		for (mac = dev->gld_mac_next;
		    mac != (gld_mac_info_t *)&dev->gld_mac_next;
		    mac = mac->gldm_next) {
			int nstr = 0;
			gld_mac_pvt_t *pvt = (gld_mac_pvt_t *)mac->gldm_mac_pvt;
			if (!(mac->gldm_GLD_flags & GLD_MAC_READY))
				continue;	/* this one's not ready yet */
			mutex_enter(&mac->gldm_maclock);
			ASSERT(mac->gldm_devinfo != NULL);
			ASSERT(mac->gldm_mac_pvt != NULL);
			ASSERT(pvt->interfacep != NULL);
			ASSERT(pvt->kstatp != NULL);
			ASSERT(pvt->statistics != NULL);
			ASSERT(pvt->major_dev == dev);
			for (str = pvt->gld_str_next;
			    str != (gld_t *)&pvt->gld_str_next;
			    str = str->gld_next) {
				ASSERT(str->gld_device == dev);
				ASSERT(str->gld_mac_info == mac);
				ASSERT(str->gld_qptr != NULL);
				ASSERT(str->gld_minor >= GLD_PPA_INIT);
				ASSERT(str->gld_multicnt == 0 ||
				    str->gld_mcast);
				nstr++;
			}
			ASSERT(pvt->nstreams == nstr);
			mutex_exit(&mac->gldm_maclock);
		}
		mutex_exit(&dev->gld_devlock);
	}
	mutex_exit(&gld_device_list.gld_devlock);
}
#endif

#ifdef GLD_V0_SUPPORT
/*
 * gldcrc32 (addr)
 * this provides a common multicast hash algorithm by doing a
 * 32bit CRC on the 6 octets of the address handed in.	Used by the National
 * chip set for Ethernet for multicast address filtering.  May be used by
 * others as well.
 *
 * This function is not part of the DDI.  Do not use.
 */
ulong_t
gldcrc32(uchar_t *addr)
{
	register int i, j;
	union gldhash crc;
	unsigned char fb, ch;

	crc.value = (uint32_t)0xFFFFFFFF; /* initialize as the HW would */

	for (i = 0; i < ETHERADDRL; i++) {
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
#endif
