/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * gldpriv.h - Private interfaces/structures needed by gld.c
 *
 * The definitions in this file are private to GLD and may change at any time.
 * They must not be used by any driver.
 */

#ifndef	_SYS_GLDPRIV_H
#define	_SYS_GLDPRIV_H

#pragma ident	"@(#)gldpriv.h	1.10	98/11/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	DEBUG
#define	GLD_DEBUG 1
#endif

/*
 * The version number should not be changed gratuitously!  A minor change
 * that is upward compatible does not require a version change.  A version
 * change will require a doc change.  On average, version changes happen
 * every 3 years.  Note that for the next version change, gldm_version will
 * need to be documented.  For now, any driver calling gld_mac_alloc is
 * considered a v2 driver; others are considered v0 drivers.
 */
#define	GLD_VERSION_200		0x200			/* version 2.0 */
#define	GLD_VERSION		GLD_VERSION_200		/* current version */
#define	GLD_VERSION_STRING	"v2"			/* in modinfo string */

/* gld_global_options bits */
#define	GLD_OPT_NO_IPQ		0x00000001	/* don't use IP shortcut */
#define	GLD_OPT_NO_FASTPATH	0x00000002	/* don't implement fastpath */
#define	GLD_OPT_NO_ETHRXSNAP	0x00000008	/* don't interp SNAP on ether */

/* minor numbers */
#define	GLD_PPA_INIT	0x1000		/* first clone minor number */
#define	GLD_MAXMIN	0x3ffff		/* Maximum minor number */
#define	GLD_USE_STYLE2	0		/* minor number for style 2 open */

/* gld_flag bits -- GLD PRIVATE */
#define	GLD_RAW		0x0001	/* lower stream is in RAW mode */
#define	GLD_FAST	0x0002	/* use "fast" path */
#define	GLD_PROM_PHYS	0x0004	/* stream is in physical promiscuous mode */
#define	GLD_PROM_SAP	0x0008
#define	GLD_PROM_MULT	0x0010
#define	GLD_STR_CLOSING	0x0020	/* stream is closing; don't putnext */

/*
 * definitions for the per driver class structure
 */
typedef struct glddevice {
	struct glddevice *gld_next, *gld_prev;
	int		gld_ndevice;	/* number of mac devices linked */
	gld_mac_info_t	*gld_mac_next, *gld_mac_prev;	/* the various macs */
	gld_t		*gld_str_next, *gld_str_prev;	/* open, unattached, */
							/* style 2 streams */
	char		gld_name[16];	/* name of device */
	kmutex_t	gld_devlock;	/* used to serialize read/write locks */
	int		gld_nextminor;	/* next unused minor number for clone */
	int		gld_major;	/* device's major number */
	int		gld_multisize;	/* # of multicast entries to alloc */
	int		gld_type;	/* for use before attach */
	int		gld_minsdu;
	int		gld_maxsdu;
	int		gld_addrlen;	/* physical address length */
	int		gld_saplen;	/* sap length, neg appends */
	unsigned char	*gld_broadcast;	/* pointer to broadcast address */
} glddev_t;

typedef struct pktinfo {
	uint_t		isBroadcast:1;
	uint_t		isMulticast:1;
	uint_t		isSpecial:1;
	uint_t		isLooped:1;
	uint_t		isForMe:1;
	uint_t		hasLLC:1;
	uint_t		hasSnap:1;
	uint_t		wasAccepted:1;
	uint_t		Sap;		/* destination sap */
	uint_t		sSap;		/* source sap */
	uint_t		macLen;
	uint_t		hdrLen;
	uint_t		pktLen;
	uchar_t		dhost[GLD_MAX_ADDRLEN];
	uchar_t		shost[GLD_MAX_ADDRLEN];
} pktinfo_t;

/*
 * Describes characteristics of the Media Access Layer.
 * The mac_type is one of the supported DLPI media types (see <sys/dlpi.h>).
 * The mtu_size is the size of the largest frame.
 * The interpreter is the function that "knows" how to interpret the frame.
 * Other routines create and/or add headers to packets.
 */
typedef struct {
	uint_t	mac_type;
	uint_t	mtu_size;
	int	(*interpreter)(gld_mac_info_t *, mblk_t *, pktinfo_t *, int);
	mblk_t	*(*mkfastpath)(gld_t *, mblk_t *);
	mblk_t	*(*mkunitdata)(gld_t *, mblk_t *);
	void	(*init)(gld_mac_info_t *);
	void	(*uninit)(gld_mac_info_t *);
	uint_t	mac_hdr_fixed_size;
} gld_interface_t;

#define	IF_HDR_FIXED	0
#define	IF_HDR_VAR	1

/*
 * structure for names stat structure usage as required by "netstat"
 */
typedef union media_kstats {
	struct dot3kstat {
		kstat_named_t	first_coll;
		kstat_named_t	multi_coll;
		kstat_named_t	sqe_error;
		kstat_named_t	mac_xmt_error;
		kstat_named_t	frame_too_long;
		kstat_named_t	mac_rcv_error;
	} dot3;
	struct dot5kstat {
		kstat_named_t	ace_error;
		kstat_named_t	internal_error;
		kstat_named_t	lost_frame_error;
		kstat_named_t	frame_copied_error;
		kstat_named_t	token_error;
		kstat_named_t	freq_error;
	} dot5;
	struct fddikstat {
		kstat_named_t	mac_error;
		kstat_named_t	mac_lost;
		kstat_named_t	mac_token;
		kstat_named_t	mac_tvx_expired;
		kstat_named_t	mac_late;
		kstat_named_t	mac_ring_op;
	} fddi;
} media_kstats_t;

struct gldkstats {
	kstat_named_t	glds_pktxmt;
	kstat_named_t	glds_pktrcv;
	kstat_named_t	glds_errxmt;
	kstat_named_t	glds_errrcv;
	kstat_named_t	glds_collisions;
	kstat_named_t	glds_bytexmt;
	kstat_named_t	glds_bytercv;
	kstat_named_t	glds_multixmt;
	kstat_named_t	glds_multircv;	/* multicast but not broadcast */
	kstat_named_t	glds_brdcstxmt;
	kstat_named_t	glds_brdcstrcv;
	kstat_named_t	glds_unknowns;
	kstat_named_t	glds_blocked;	/* discard due to upstream flow */
					/* control */
	kstat_named_t	glds_excoll;
	kstat_named_t	glds_defer;
	kstat_named_t	glds_frame;
	kstat_named_t	glds_crc;
	kstat_named_t	glds_overflow;
	kstat_named_t	glds_underflow;
	kstat_named_t	glds_short;
	kstat_named_t	glds_missed;
	kstat_named_t	glds_xmtlatecoll;
	kstat_named_t	glds_nocarrier;
	kstat_named_t	glds_noxmtbuf;
	kstat_named_t	glds_norcvbuf;
	kstat_named_t	glds_xmtbadinterp;
	kstat_named_t	glds_rcvbadinterp;
	kstat_named_t	glds_intr;
	kstat_named_t	glds_xmtretry;
	kstat_named_t	glds_pktxmt64;
	kstat_named_t	glds_pktrcv64;
	kstat_named_t	glds_bytexmt64;
	kstat_named_t	glds_bytercv64;
	kstat_named_t	glds_speed;
	kstat_named_t	glds_duplex;
	kstat_named_t	glds_media;
	kstat_named_t	glds_prom;
	media_kstats_t	glds_media_specific;
};

/* Per-mac info used by GLD */
typedef	struct {
	struct gld	*gld_str_next;	/* list of attached streams */
	struct gld	*gld_str_prev;
	gld_interface_t	*interfacep;
	kmutex_t	datalock;	/* data lock for "data" */
	caddr_t		data;		/* media specific private data */
	queue_t		*ipq;		/* Pointer to IP's queue */
	struct gld	*last_sched;	/* last scheduled stream */
	struct glddevice *major_dev;	/* per-major device struct */
	int		nstreams;	/* Streams bound to this mac */
	int		nprom;		/* num streams in promiscuous mode */
	int		nprom_multi;	/* streams in promiscuous multicast */
	gld_mcast_t	*mcast_table;	/* per device multicast table */
	unsigned char	*curr_macaddr;	/* Currently programmed mac address */
	kstat_t		*kstatp;
	struct gld_stats *statistics;	/* The ones the driver updates */
	kmutex_t	gldp_txlock;	/* Lock for GLD xmit side */
	int		rde_enabled;	/* RDE (Source Routing) Enabled */
	int		rde_str_indicator_ste;	/* use STE when no SR info */
	int		rde_timeout;	/* route link inactivity timeout */
} gld_mac_pvt_t;

/* return values from gld_cmds */
#define	GLDE_OK		(-1)	/* internal procedure status is OK */
#define	GLDE_RETRY	0x1002	/* want to retry later */

/* special case SAP values */
#define	GLD_802_SAP	1500		/* boundary between ether length/type */
#define	GLD_MAX_802_SAP 0xFF		/* highest possible 802.2 sap value */
#define	GLD_SPECIAL_SAP	0x10000		/* arbitrary, but not a legal SAP */

/* lockflavor argument to gld_start */
#define	GLD_LOCK	0
#define	GLD_TRYLOCK	1
#define	GLD_HAVELOCK	2
#define	GLD_DONTLOCK	3

/*
 * Compare/copy two MAC addresses.
 * Note that unlike bcmp, we return zero if they are different.
 */
#define	mac_eq(a, b, l) (bcmp((caddr_t)(a), (caddr_t)(b), (l)) == 0)
#define	mac_copy(a, b, l) (bcopy((caddr_t)(a), (caddr_t)(b), (l)))
/* copy a mac address to/from canonical form */
#define	cmac_copy(a, b, l, macinfo) {					\
	    if ((macinfo)->gldm_options & GLDOPT_CANONICAL_ADDR)	\
		gld_bitrevcopy((caddr_t)(a), (caddr_t)(b), (l));	\
	    else							\
		mac_copy((a), (b), (l));				\
	}

/*
 * Macros to access possibly-unaligned variables
 */

#if	(_ALIGNMENT_REQUIRED == 0)

#define	REF_HOST_USHORT(lvalue) (lvalue)
#define	REF_NET_USHORT(lvalue) (ntohs(lvalue))
#define	SET_NET_USHORT(lvalue, val) ((lvalue) = htons(val))

#else	/* ALIGNMENT_REQUIRED */

#define	REF_NET_USHORT(lvalue) \
	((ushort_t)((((uchar_t *)(&(lvalue)))[0]<<8) | \
	((uchar_t *)(&(lvalue)))[1]))

#define	SET_NET_USHORT(lvalue, val) { \
	((uchar_t *)(&(lvalue)))[0] = (uchar_t)((val)>>8); \
	((uchar_t *)(&(lvalue)))[1] = (uchar_t)(val); \
}

#if defined(_LITTLE_ENDIAN)

#define	REF_HOST_USHORT(lvalue) \
	((ushort_t)((((uchar_t *)(&(lvalue)))[1]<<8) | \
	((uchar_t *)(&(lvalue)))[0]))

#elif defined(_BIG_ENDIAN)

#define	REF_HOST_USHORT(lvalue) \
	((ushort_t)((((uchar_t *)(&(lvalue)))[0]<<8) | \
	((uchar_t *)(&(lvalue)))[1]))

#else	/* unknown endian */
#error	"what endian is this machine?"
#endif	/* endian */

#endif	/* ALIGNMENT_REQUIRED */

/* ================================================================ */
/* Route Determination Entity definitions (IEEE 802.2 1994 edition) */
/* ================================================================ */

struct rde_pdu {
	uchar_t	rde_ver;
	uchar_t	rde_ptype;
	uchar_t	rde_target_mac[6];
	uchar_t	rde_orig_mac[6];
	uchar_t	rde_target_sap;
	uchar_t	rde_orig_sap;
};

#define	LSAP_RDE	0xa6	/* IEEE 802.2 section 3.3.1.2 */
#define	RDE_RQC		0x01	/* Route Query Command */
#define	RDE_RQR		0x02	/* Route Query Response */
#define	RDE_RS		0x03	/* Route Selected */

/* ============================================================= */
/* Source Routing fields and definitions (IEEE 802.2 and 802.1D) */
/* ============================================================= */

#define	MAX_RDFLDS	14	/* changed to 14 from 8 as per IEEE */

/*
 * Source Routing Route Information field.
 */
struct gld_ri {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t len:5;			/* length */
	uchar_t rt:3;			/* routing type */
	uchar_t res:4;			/* reserved */
	uchar_t mtu:3;			/* largest frame */
	uchar_t dir:1;			/* direction bit */
	struct tr_rd {			/* route designator fields */
		ushort_t bridge:4;	/* Note: assumes network order... */
		ushort_t ring:12;	/* ...(Big Endian) -- needs ntohs() */
	} rd[MAX_RDFLDS];
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t rt:3;			/* routing type */
	uchar_t len:5;			/* length */
	uchar_t dir:1;			/* direction bit */
	uchar_t mtu:3;			/* largest frame */
	uchar_t res:4;			/* reserved */
	struct tr_rd {			/* route designator fields */
		ushort_t ring:12;
		ushort_t bridge:4;
	} rd[MAX_RDFLDS];
#else
#error	"which way do bit fields get allocated?"
#endif
};

#define	RT_SRF		0x0		/* 0xx: specifically routed frame */
#define	RT_ARE		0x4		/* 10x: all routes explorer frame */
#define	RT_STE		0x6		/* 11x: spanning tree explorer frame */

#define	RT_MTU_MAX	0x7		/* Max MTU field (base only) */

/*
 * Source route table info
 */
struct srtab {
	struct srtab	*sr_next;		/* next in linked list */
	uchar_t		sr_mac[6];		/* MAC address */
	struct		gld_ri sr_ri;		/* routing information */
	clock_t		sr_timer;
};

#define	SR_HASH_SIZE	256		/* Number of bins */

/* ================================================================= */
/* Media dependent defines for media dependent routines in gldutil.c */
/* ================================================================= */

/*
 * Some "semi-generic" defines used by ether, token, and fddi,
 * and probably anything else with addrlen == 6 && saplen == -2.
 */

struct gld_dlsap {
	unsigned char   glda_addr[ETHERADDRL];
	unsigned short  glda_sap;
};

#define	DLSAP(p, offset) ((struct gld_dlsap *)((caddr_t)(p)+offset))

typedef uchar_t mac_addr_t[ETHERADDRL];

struct llc_snap_hdr {
	uchar_t  d_lsap;		/* destination service access point */
	uchar_t  s_lsap;		/* source link service access point */
	uchar_t  control;		/* short control field */
	uchar_t  org[3];		/* Ethernet style organization field */
	ushort_t type;			/* Ethernet style type field */
};

#define	LLC_HDR1_LEN		3	/* Length of the LLC1 header */
#define	LLC_SNAP_HDR_LEN	8	/* Full length of SNAP header */
#define	LSAP_SNAP		0xaa	/* SAP for SubNet Access Protocol */
#define	CNTL_LLC_UI		0x03	/* un-numbered information packet */

/* ============================ */
/* Ethernet related definitions */
/* ============================ */

struct ether_mac_frm {
	mac_addr_t	ether_dhost;
	mac_addr_t	ether_shost;
	ushort_t	ether_type;
};

/* ======================== */
/* FDDI related definitions */
/* ======================== */

struct	fddi_mac_frame {
	uchar_t		fddi_fc;
	mac_addr_t	fddi_dhost;
	mac_addr_t	fddi_shost;
};

/* ============================== */
/* Token Ring related definitions */
/* ============================== */

struct tr_mac_frm_nori {
	uchar_t		tr_ac;
	uchar_t		tr_fc;
	mac_addr_t	tr_dhost;
	mac_addr_t	tr_shost;
};

struct tr_mac_frm {
	uchar_t		tr_ac;
	uchar_t		tr_fc;
	mac_addr_t	tr_dhost;
	mac_addr_t	tr_shost;
	struct gld_ri	tr_ri;		/* Routing Information Field */
};

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_GLDPRIV_H */
