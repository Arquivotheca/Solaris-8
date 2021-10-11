/*
 * Copyright (c) 1993-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * gld - Generic LAN Driver support system for DLPI drivers.
 */

#ifndef	_SYS_GLD_H
#define	_SYS_GLD_H

#pragma ident	"@(#)gld.h	1.24	98/11/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	NO_GLD_V0_SUPPORT
#define	GLD_V0_SUPPORT 1	/* Compatibility with GLD v0 */
#endif

#ifndef	ETHERADDRL
#define	ETHERADDRL	6
#endif

/*
 * Media specific MIB-II counters/statistics
 *
 * This only includes those that aren't in the legacy counters.
 */

typedef union media_stats {
	struct dot3stat {
		/* Ethernet: RFC1643 Dot3Stats (subset) */
		uint32_t	first_coll;	/* SingleCollisionFrames */
		uint32_t	multi_coll;	/* MultipleCollisionFrames */
		uint32_t	sqe_error;	/* SQETestErrors */
		uint32_t	mac_xmt_error;	/* InternalMacTransmitErrors */
		uint32_t	frame_too_long;	/* FrameTooLongs */
		uint32_t	mac_rcv_error;	/* InternalMacReceiveErrors */
	} dot3;
	struct dot5stat {
		/* Token Ring: RFC1748 Dot5Stats (subset) */
		uint32_t	ace_error;
		uint32_t	internal_error;
		uint32_t	lost_frame_error;
		uint32_t	frame_copied_error;
		uint32_t	token_error;
		uint32_t	freq_error;
	} dot5;
	struct fddistat {
		/* FDDI: RFC1512 (subset) */
		uint32_t	mac_error;
		uint32_t	mac_lost;
		uint32_t	mac_token;
		uint32_t	mac_tvx_expired;
		uint32_t	mac_late;
		uint32_t	mac_ring_op;
	} fddi;
	uint32_t		pad[16];
} media_stats_t;

#define	glds_dot3_first_coll		glds_media_specific.dot3.first_coll
#define	glds_dot3_multi_coll		glds_media_specific.dot3.multi_coll
#define	glds_dot3_sqe_error		glds_media_specific.dot3.sqe_error
#define	glds_dot3_mac_xmt_error		glds_media_specific.dot3.mac_xmt_error
#define	glds_dot3_mac_rcv_error		glds_media_specific.dot3.mac_rcv_error
#define	glds_dot3_frame_too_long	glds_media_specific.dot3.frame_too_long

#define	glds_dot5_line_error		glds_crc
#define	glds_dot5_burst_error		glds_frame
#define	glds_dot5_ace_error		glds_media_specific.dot5.ace_error
#define	glds_dot5_internal_error	glds_media_specific.dot5.internal_error
#define	glds_dot5_lost_frame_error   glds_media_specific.dot5.lost_frame_error
#define	glds_dot5_frame_copied_error glds_media_specific.dot5.frame_copied_error
#define	glds_dot5_token_error		glds_media_specific.dot5.token_error
#define	glds_dot5_signal_loss		glds_nocarrier
#define	glds_dot5_freq_error		glds_media_specific.dot5.freq_error

#define	glds_fddi_mac_error		glds_media_specific.fddi.mac_error
#define	glds_fddi_mac_lost		glds_media_specific.fddi.mac_lost
#define	glds_fddi_mac_token		glds_media_specific.fddi.mac_token
#define	glds_fddi_mac_tvx_expired	glds_media_specific.fddi.mac_tvx_expired
#define	glds_fddi_mac_late		glds_media_specific.fddi.mac_late
#define	glds_fddi_mac_ring_op		glds_media_specific.fddi.mac_ring_op

/*
 * structure for driver statistics
 */
struct gld_stats {
	ulong_t		glds_multixmt;	/* (G) ifOutMulticastPkts */
	ulong_t		glds_multircv;	/* (G) ifInMulticastPkts */
	ulong_t		glds_brdcstxmt;	/* (G) ifOutBroadcastPkts */
	ulong_t		glds_brdcstrcv;	/* (G) ifInBroadcastPkts */
	uint32_t	glds_blocked;	/* (G) discard: upstream flow cntrl */
	uint32_t	glds_reserved1;
	uint32_t	glds_reserved2;
	uint32_t	glds_reserved3;
	uint32_t	glds_reserved4;
	uint32_t	glds_errxmt;	/* (D) ifOutErrors */
	uint32_t	glds_errrcv;	/* (D) ifInErrors */
	uint32_t	glds_collisions; /* (e) Sun MIB's rsIfCollisions */
	uint32_t	glds_excoll;	/* (e) dot3StatsExcessiveCollisions */
	uint32_t	glds_defer;	/* (e) dot3StatsDeferredTransmissions */
	uint32_t	glds_frame;	/* (e) dot3StatsAlignErrors */
	uint32_t	glds_crc;	/* (e) dot3StatsFCSErrors */
	uint32_t	glds_overflow;	/* (D) */
	uint32_t	glds_underflow;	/* (D) */
	uint32_t	glds_short;	/* (e) */
	uint32_t	glds_missed;	/* (D) */
	uint32_t	glds_xmtlatecoll; /* (e) dot3StatsLateCollisions */
	uint32_t	glds_nocarrier; /* (e) dot3StatsCarrierSenseErrors */
	uint32_t	glds_noxmtbuf;	/* (G) ifOutDiscards */
	uint32_t	glds_norcvbuf;	/* (D) ifInDiscards */
	uint32_t	glds_intr;	/* (D) */
	uint32_t	glds_xmtretry;	/* (G) */
	/* all before here must be kept in place for v0 compatibility */
	uint64_t	glds_pktxmt64;	/* (G) 64-bit rsIfOutPackets */
	uint64_t	glds_pktrcv64;	/* (G) 64-bit rsIfInPackets */
	uint64_t	glds_bytexmt64;	/* (G) ifHCOutOctets */
	uint64_t	glds_bytercv64;	/* (G) ifHCInOctets */
	uint64_t	glds_speed;	/* (D) ifSpeed */
	uint32_t	glds_duplex;	/* (e) Invented for GLD */
	uint32_t	glds_media;	/* (D) Invented for GLD */
	uint32_t	glds_unknowns;	/* (G) ifInUnknownProtos */
	uint32_t	reserved[19];
	media_stats_t	glds_media_specific;
	uint32_t	glds_xmtbadinterp; /* (G) bad packet len/format */
	uint32_t	glds_rcvbadinterp; /* (G) bad packet len/format */
	uint32_t	glds_gldnorcvbuf;  /* (G) norcvbuf from inside GLD */
};

/*
 * Defines to keep the macinfo structure offsets compatible with v0.
 * Must not be changed.
 */
#define	GLD_STATS_SIZE_ORIG	(sizeof (uint32_t) * 26) /* don't change */
#define	GLD_STATS_SIZE		(sizeof (struct gld_stats))
#define	GLDKSTAT_SIZE_ORIG (sizeof (kstat_named_t) * 26) /* don't change */
#define	GLDKSTAT_PAD  ((int)GLDKSTAT_SIZE_ORIG - \
		((int)GLD_STATS_SIZE - (int)GLD_STATS_SIZE_ORIG))

/*
 * gld_mac_info structure.  Used to define the per-board data for all
 * drivers.
 *
 * The below definition of gld_mac_info contains PRIVATE or obsolete
 * entries that should not be used by the device dependent driver, but
 * are here for now.  They will be deleted or moved to a private structure
 * in a future release.
 */
typedef
struct gld_mac_info {
	struct gld_mac_info *gldm_next, *gldm_prev;	/* GLD PRIVATE */
	caddr_t		reserved1;
	caddr_t		reserved2;
	uint16_t	gldm_driver_version;	/* GLD PRIVATE for now */
	uint16_t	gldm_GLD_version;	/* GLD PRIVATE for now */
	uint16_t	gldm_GLD_flags;	/* GLD PRIVATE */
	uint16_t	gldm_options;	/* GLD PRIVATE */
	dev_info_t	*gldm_devinfo;	/* SET BY DRIVER */
	unsigned char	*gldm_vendor_addr;	/* SET BY DRIVER */
	unsigned char	*gldm_broadcast_addr;	/* SET BY DRIVER */
	kmutex_t	gldm_maclock;	/* GLD PRIVATE since v2 */
	ddi_iblock_cookie_t gldm_cookie;	/* SET BY DRIVER */
	uint32_t	gldm_flags;	/* v0 COMPAT, DO NOT USE */
	uint32_t	gldm_state;	/* v0 COMPAT, DO NOT USE */
	uint32_t	gldm_maxpkt;	/* SET BY DRIVER */
	uint32_t	gldm_minpkt;	/* SET BY DRIVER */
	char		*gldm_ident;	/* SET BY DRIVER */
	uint32_t	gldm_type;	/* SET BY DRIVER */
	uint32_t	gldm_media;	/* v0 COMPAT, DO NOT USE */
	uint32_t	gldm_addrlen;	/* SET BY DRIVER, usually 6 */
	int32_t		gldm_saplen;	/* SET BY DRIVER, usually -2 */
	unsigned char	gldm_macaddr[ETHERADDRL];	/* v0, DO NOT USE */
	unsigned char	gldm_vendor[ETHERADDRL];	/* v0, DO NOT USE */
	unsigned char	gldm_broadcast[ETHERADDRL];	/* v0, DO NOT USE */
#ifdef t_uscalar_t
	t_uscalar_t	gldm_ppa;	/* PPA number - SET BY V2 DRIVER */
#else
	int32_t		gldm_ppa;
#endif
	int32_t		gldm_reg_offset; /* v0 compat, DO NOT USE */
	uint32_t	reserved3;
	uint32_t	reserved4;
	uint32_t	gldm_port;	/* v0 compat, DO NOT USE */
	caddr_t		gldm_memp;	/* v0 compat, DO NOT USE */
	int32_t		gldm_reg_index;	/* v0 compat, DO NOT USE */
	uint32_t	gldm_reg_len;	/* v0 compat, DO NOT USE */
	int32_t		gldm_irq_index;	/* v0 compat, DO NOT USE */
	caddr_t		gldm_mac_pvt;	/* Per-mac info - GLD PRIVATE */
	caddr_t		reserved5;
	struct gld_stats gldm_stats;	/* v0 compat, DO NOT USE */
	char		gldm_pad[GLDKSTAT_PAD]; /* for backward compat */
	caddr_t		reserved6;
	caddr_t		gldm_private;	/* Pointer to driver private state */
	int		(*gldm_reset)();	/* reset procedure */
	int		(*gldm_start)();	/* start board */
	int		(*gldm_stop)();		/* stop board completely */
	int		(*gldm_set_mac_addr)();	/* set physical address */
	int		(*gldm_send)();		/* transmit procedure */
	int		(*gldm_set_promiscuous)(); /* set promiscuous mode */
	int		(*gldm_get_stats)();	/* get board statistics */
	int		(*gldm_ioctl)();	/* Driver specific ioctls */
	int		(*gldm_set_multicast)(); /* set/delete multicast */
						/* address */
	uint_t		(*gldm_intr)();		/* interrupt handler */
	int		(*gldm_mctl)();	/* Driver specific mctls, v2 only */

	/*
	 * Additional entries can be added after here, without breaking
	 * binary compatibility with Version 0 drivers.  But anything
	 * used by GLD below this point may only be accessed or written
	 * in a v2 code path, since nothing below this point exists in
	 * an older v0 binary macinfo structure.
	 */

} gld_mac_info_t;

/* flags for physical promiscuous state */
#define	GLD_MAC_PROMISC_NONE	0
#define	GLD_MAC_PROMISC_PHYS	1	/* receive all packets */
#define	GLD_MAC_PROMISC_MULTI	2	/* receive all multicast packets */

#define	GLD_MULTI_ENABLE	1
#define	GLD_MULTI_DISABLE	0

/*
 * media type: this identifies the media/connector currently used by the
 * driver.  Possible types will be defined for each DLPI type defined in
 * gldm_type.  The below definitions should be used by the device dependent
 * drivers to set glds_media.
 */

/* if driver cannot determine media/connector type  */
#define	GLDM_UNKNOWN	0

#define	GLDM_AUI	1
#define	GLDM_BNC	2
#define	GLDM_TP		3
#define	GLDM_FIBER	4
#define	GLDM_100BT	5
#define	GLDM_VGANYLAN	6
#define	GLDM_10BT	7
#define	GLDM_RING4	8
#define	GLDM_RING16	9
#define	GLDM_PHYMII	10
#define	GLDM_100BTX	11
#define	GLDM_100BT4	12

/* defines for possible duplex states (glds_duplex) */
#define	GLD_DUPLEX_UNKNOWN	0
#define	GLD_DUPLEX_HALF		1
#define	GLD_DUPLEX_FULL		2

/* Values returned from driver entry points */
#define	GLD_SUCCESS		0
#define	GLD_NORESOURCES		1
#define	GLD_NOTSUPPORTED	2
#define	GLD_BADARG		3
#define	GLD_FAILURE		(-1)

#if defined(_KERNEL)
/* Functions exported to drivers */
extern gld_mac_info_t *gld_mac_alloc(dev_info_t *);
extern void gld_mac_free(gld_mac_info_t *);
extern int gld_register(dev_info_t *, char *, gld_mac_info_t *);
extern int gld_unregister(gld_mac_info_t *);
extern void gld_recv(gld_mac_info_t *, mblk_t *);
extern void gld_sched(gld_mac_info_t *);
extern uint_t gld_intr();

extern int gld_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
extern int gld_open(queue_t *, dev_t *, int, int, cred_t *);
extern int gld_close(queue_t *, int, cred_t *);
extern int gld_wput(queue_t *, mblk_t *);
extern int gld_wsrv(queue_t *);
extern int gld_rsrv(queue_t *);
#endif

/*
 * The below definitions should not be used, but they are here for now because
 * some older drivers may (incorrectly) use them.  They may be removed
 * or moved to gldpriv.h in a future release.
 *
 *  DO NOT USE ANYTHING BELOW THIS POINT!!
 */

/* gldm_GLD_flags  -- GLD PRIVATE */
#define	GLD_MAC_READY 0x0001	/* this mac has succeeded gld_register */
#define	GLD_INTR_READY 0x0001	/* v0 compat name */
#define	GLD_INTR_WAIT 0x0002	/* v1: waiting for interrupt to do scheduling */
#define	GLD_MUTEX_INITED 0x0004	/* maclock is currently initialized */
#define	GLD_UNREGISTERED 0x0008	/* this mac has succeeded gld_unregister */

/* gldm_options -- GLD PRIVATE, v0 COMPAT */
#define	GLDOPT_PCMCIA		0x01	/* DO NOT USE */
#define	GLDOPT_DRIVER_PPA	0x02	/* Driver sets ppa (standard in v2) */
#define	GLDOPT_RW_LOCK		0x04	/* DO NOT USE */
#define	GLDOPT_CANONICAL_ADDR	0x08	/* Media uses canonical addresses */
#define	GLDOPT_DONTFREE		0x10	/* Driver frees xmit mblk (std in v2) */
#define	GLDOPT_CMD_ACK		0x20	/* DO NOT USE */
#define	GLDOPT_FAST_RECV	0x40	/* gld_recv putq instead of putnext */
#define	GLDOPT_V2_IOCTL		0x80	/* v0 driver takes v2 gldm_ioctl args */

/* This is the largest macaddr currently supported by GLD */
#define	GLD_MAX_ADDRLEN	ETHERADDRL	/* Largest mac addr in all media  */

#define	GLD_MAX_MULTICAST	64	/* default multicast table size */

/* multicast structures */
typedef struct gld_multicast_addr {
	int		gldm_refcnt;	/* number of streams referring */
					/* to this per-mac entry */
	unsigned char	gldm_addr[GLD_MAX_ADDRLEN];
} gld_mcast_t;

/*
 * definitions for debug tracing
 */
#define	GLDTRACE	0x0001	/* basic procedure level tracing */
#define	GLDERRS		0x0002	/* trace errors */
#define	GLDRECV		0x0004	/* trace receive path */
#define	GLDSEND		0x0008	/* trace send path */
#define	GLDPROT		0x0010	/* trace DLPI protocol */
#define	GLDNOBR		0x0020	/* do not show broadcast messages */
#define	GLDETRACE	0x0040	/* trace "normal case" errors */
#define	GLDRDE		0x0080	/* netstat -k dump routing table */

/*
 * gld structure.  Used to define the per-stream information required to
 * implement DLPI -- GLD PRIVATE.
 *
 * Unfortunately some v0 drivers look in this structure to find the macinfo
 * associated with a stream, because the (rarely implemented) gldm_ioctl
 * function got passed a q but not a macinfo.  So they would chase
 * q->q_ptr->gld_mac_info to find their device.  This means we must
 * preserve the offset of gld_macinfo in this structure until such time as
 * we stop supporting v0 drivers, at least the ones that implement ioctl.
 */
typedef struct gld {
	struct gld	*gld_next, *gld_prev;
	caddr_t		gld_dummy1;
	int32_t		gld_state;	/* DL_UNATTACHED, DL_UNBOUND, DL_IDLE */
	int32_t		gld_style;	/* open style 1 or style 2 */
	int32_t		gld_minor;	/* cloned minor number */
	int32_t		gld_type;	/* DL_ETHER, DL_TPR, DL_FDDI, etc */
	int32_t		gld_sap;	/* Bound SAP */
	int32_t		gld_flags;	/* flags defined in gldpriv.h */
	int32_t		gld_multicnt;	/* # of stream multicast addresses */
	gld_mcast_t	**gld_mcast;	/* multicast table or NULL */
	queue_t		*gld_qptr;	/* pointer to streams queue */
	caddr_t		gld_dummy2;
	caddr_t		gld_dummy3;
	struct gld_mac_info *gld_mac_info;	/* if not DL_UNATTACHED */
	caddr_t		gld_dummy4;
	struct glddevice *gld_device;	/* per-major structure */

	/* Below this was introduced in v2, so no driver looks at them */
	volatile int32_t gld_xwait;	/* want an xmit qenable */
	volatile int32_t gld_sched_ran;	/* gld_sched examined this Q */
	volatile int32_t gld_in_unbind;	/* DL_UNBIND in progress */
	volatile uint32_t gld_wput_count; /* number of threads in wput=>start */
	volatile uint32_t gld_in_wsrv;	/* Q thread currently running in wsrv */
} gld_t;

#ifdef	GLD_V0_SUPPORT

/* Defines to allow v0 driver to continue compiling -- DO NOT USE */
#define	gldm_saddr	gldm_set_mac_addr
#define	gldm_prom	gldm_set_promiscuous
#define	gldm_gstat	gldm_get_stats
#define	gldm_sdmulti	gldm_set_multicast

/* Old cruft that some old v0 drivers may or may not have used */

#define	GLD_ATTACHED	0x0001
#define	GLD_PROMISC	0x0010
#define	GLD_IN_INTR	0x0020

#define	ismulticast(cp) ((*(caddr_t)(cp)) & 0x01)

union gldhash {
	uint32_t   value;
	struct {
		unsigned	a0:1;
		unsigned	a1:1;
		unsigned	a2:1;
		unsigned	a3:1;
		unsigned	a4:1;
		unsigned	a5:1;
		unsigned	a6:1;
		unsigned	a7:1;
		unsigned	a8:1;
		unsigned	a9:1;
		unsigned	a10:1;
		unsigned	a11:1;
		unsigned	a12:1;
		unsigned	a13:1;
		unsigned	a14:1;
		unsigned	a15:1;
		unsigned	a16:1;
		unsigned	a17:1;
		unsigned	a18:1;
		unsigned	a19:1;
		unsigned	a20:1;
		unsigned	a21:1;
		unsigned	a22:1;
		unsigned	a23:1;
		unsigned	a24:1;
		unsigned	a25:1;
		unsigned	a26:1;
		unsigned	a27:1;
		unsigned	a28:1;
		unsigned	a29:1;
		unsigned	a30:1;
		unsigned	a31:1;
	} bits;
};

#define	DEPENDS_ON_GLD	char _depends_on[] = "misc/gld"

#define	llcp_int gldm_irq
#define	LLC_ADDR_LEN ETHERADDRL
#define	GLD_EHDR_SIZE sizeof (struct ether_header)
#define	LOW(x) ((x)&0xFF)
#define	HIGH(x) (((x)>>8)&0xFF)

#define	gldnvm(ptr) ((NVM_SLOTINFO *)(ptr))
#define	gld_boardid(nvm) (*(ushort_t *)(gldnvm(nvm)->boardid))
#define	gld_check_boardid(nvm, id) (gld_boardid(nvm) == id)

#if defined(_KERNEL)
extern uchar_t  gldbroadcastaddr[];
extern ulong_t  gldcrc32(uchar_t *);
#endif

#endif	/* GLD_V0_SUPPORT */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_GLD_H */
