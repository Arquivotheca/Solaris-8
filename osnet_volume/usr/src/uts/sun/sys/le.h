/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LE_H
#define	_SYS_LE_H

#pragma ident	"@(#)le.h	1.52	99/10/22 SMI"

/*
 * le.h header for LANCE Ethernet Driver.
 */

#include <sys/types.h>
#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for module_info.
 */
#define		LEIDNUM		(68)		/* module ID number */
#define		LENAME		"le"		/* module name */
#define		LEMINPSZ	(0)		/* min packet size */
#define		LEMAXPSZ	1514		/* max packet size */
#define		LEHIWAT		(32 * 1024)	/* hi-water mark */
#define		LELOWAT		(1)		/* lo-water mark */

/*
 * Per-Stream instance state information.
 *
 * Each instance is dynamically allocated at open() and free'd
 * at close().  Each per-Stream instance points to at most one
 * per-device structure using the sl_lep field.  All instances
 * are threaded together into one list of active instances
 * ordered on minor device number.
 */

#define	NMCHASH	64			/* # of multicast hash buckets */
#define	INIT_BUCKET_SIZE	16	/* Initial Hash Bucket Size */

struct	lestr {
	struct	lestr	*sl_nextp;	/* next in list */
	queue_t	*sl_rq;			/* pointer to our rq */
	struct	le *sl_lep;		/* attached device */
	t_uscalar_t sl_state;		/* current DL state */
	t_uscalar_t sl_sap;		/* bound sap */
	uint_t	sl_flags;		/* misc. flags */
	minor_t	sl_minor;		/* minor device number */

	struct	ether_addr
		*sl_mctab[NMCHASH];	/* table of multicast addrs */
	uint_t	sl_mccount[NMCHASH];	/* # valid addresses in mctab[i] */
	uint_t	sl_mcsize[NMCHASH];	/* Allocated size of mctab[i] */

	ushort_t	sl_ladrf[4];	/* Multicast filter bits */
	ushort_t	sl_ladrf_refcnt[64];	/* Ref. cnt for filter bits */

	kmutex_t	sl_lock;	/* protect this structure */
};

#define	MCHASH(a)	((*(((uchar_t *)(a)) + 0) ^		\
			*(((uchar_t *)(a)) + 1) ^		\
			*(((uchar_t *)(a)) + 2) ^		\
			*(((uchar_t *)(a)) + 3) ^		\
			*(((uchar_t *)(a)) + 4) ^		\
			*(((uchar_t *)(a)) + 5)) % (uint_t)NMCHASH)


/* per-stream flags */
#define	SLFAST		0x01	/* "M_DATA fastpath" mode */
#define	SLRAW		0x02	/* M_DATA plain raw mode */
#define	SLALLPHYS	0x04	/* "promiscuous mode" */
#define	SLALLMULTI	0x08	/* enable all multicast addresses */
#define	SLALLSAP	0x10	/* enable all ether type values */

/*
 * Overload LANCE tmd flags reserved field with a flag
 * to indicate that the tmd has been allocated (in use).
 */
#define	TMD_INUSE	TMD_RES

/*
 * Maximum # of multicast addresses per Stream.
 */
#define	LEMAXMC	64
#define	LEMCALLOC	(LEMAXMC * sizeof (struct ether_addr))

/*
 * Full DLSAP address length (in struct dladdr format).
 */
#define	LEADDRL	(sizeof (ushort_t) + ETHERADDRL)

/*
 * Return the address of an adjacent descriptor in the given ring.
 */
#define	NEXTRMD(lep, rmdp)	(((rmdp) + 1) == (lep)->le_rmdlimp	\
	? (lep)->le_rmdp : ((rmdp) + 1))
#define	NEXTTMD(lep, tmdp)	(((tmdp) + 1) == (lep)->le_tmdlimp	\
	? (lep)->le_tmdp : ((tmdp) + 1))
#define	PREVTMD(lep, tmdp)	((tmdp) == (lep)->le_tmdp		\
	? ((lep)->le_tmdlimp - 1) : ((tmdp) - 1))

/*
 * Per-Device instance state information.
 *
 * Each instance is dynamically allocated on first attach.
 */
struct	le {
	uint_t	loc_flag;
	struct	le		*le_nextp;	/* next in a linked list */
	volatile	struct	lanceregs	*le_regsp;	/* chip regs */
	uint_t	le_flags;			/* misc. flags */
	dev_info_t	*le_dip;	/* associated dev_info */
	struct	ether_addr	le_ouraddr;	/* individual address */
	ddi_iblock_cookie_t	le_cookie;	/* cookie from ddi_add_intr */

	kmutex_t	le_xmitlock;		/* protect xmit-side fields */
	kmutex_t	le_intrlock;		/* protect intr-side fields */
	kmutex_t	le_buflock;		/* protect private buffers */

	struct	lance_init_block *le_ibp;	/* chip init block */

	struct	lmd	*le_rmdp;	/* receive descriptor ring start */
	struct	lmd	*le_rmdlimp;	/* receive descriptor ring end */
	int	le_nrmdp2;		/* log(2) # receive descriptors */
	int	le_nrmds;		/* # receive descriptors */
	struct	lmd	*le_tmdp;	/* transmit descriptor ring start */
	struct	lmd	*le_tmdlimp;	/* transmit descriptor ring end */
	int	le_ntmdp2;		/* log(2) # transmit descriptors */
	int	le_ntmds;		/* # transmit descriptors */
	volatile	struct	lmd	*le_rnextp;	/* next chip rmd */
	volatile	struct	lmd	*le_tnextp;	/* next free tmd */
	volatile	struct	lmd	*le_tcurp;	/* next reclaim tmd */
	uint_t	le_wantw;			/* xmit: out of resources */

	struct	lebuf	*le_tbufp[128];	/* xmit completion tbufs */
	mblk_t	*le_tmblkp[128];	/* xmit completion msgs */
	struct	lebuf	*le_rbufp[128];	/* lebuf associated with RMD */

	uintptr_t	le_membase;	/* base address of slave card memory */
	int	le_memsize;		/* size of slave card memory in bytes */
	int	(*le_init)();		/* device-specific initialize */
	int	(*le_intr)();		/* device-specific interrupt */
	caddr_t	le_arg;			/* device-specific arg */

	int	le_nbufs;		/* # buffers */
	caddr_t	le_bufbase;		/* buffers base address */
	caddr_t	le_buflim;		/* buffers limit address */
	struct	lebuf	**le_buftab;	/* buffer pointer stack (fifo) */
	int	le_bufi;		/* index of buffer ptr stack top */

	int	le_tpe;			/* =1 => tpe selected */
	int	le_autosel;		/* =1 => auto-selection enabled */
	int	le_oopkts;		/* old le_opackets */
	int	le_intr_flag;		/* initialization flag for leintr() */
					/* bug 1204247 */

	/*
	 * DDI dma handle, kernel virtual base,
	 * and io virtual base of IOPB area.
	 */
	ddi_dma_handle_t	le_iopbhandle;
	uintptr_t		le_iopbkbase;
	uintptr_t		le_iopbiobase;

	queue_t	*le_ipq;		/* IPv4 read queue */
	queue_t	*le_ip6q;		/* IPv6 read queue */

	/*
	 * DDI dma handle, kernel virtual base,
	 * and io virtual base addresses of buffer area.
	 */
	ddi_dma_handle_t	le_bufhandle;
	uintptr_t		le_bufkbase;
	uintptr_t		le_bufiobase;

	kstat_t	*le_intrstats;		/* interrupt statistics */
	kstat_t	*le_ksp;		/* kstat pointer */
	uint32_t le_ipackets;		/* # packets received */
	uint32_t le_ierrors;		/* # total input errors */
	uint32_t le_opackets;		/* # packets sent */
	uint32_t le_oerrors;		/* # total output errors */
	uint32_t le_collisions;		/* # collisions */
	uint32_t le_defer;		/* # defers */
	uint32_t le_fram;		/* # receive framing errors */
	uint32_t le_crc;		/* # receive crc errors */
	uint32_t le_oflo;		/* # receiver overflows */
	uint32_t le_uflo;		/* # transmit underflows */
	uint32_t le_missed;		/* # receive missed */
	uint32_t le_tlcol;		/* # transmit late collisions */
	uint32_t le_trtry;		/* # transmit retry failures */
	uint32_t le_tnocar;		/* # loss of carrier errors */
	uint32_t le_inits;		/* # driver inits */
	uint32_t le_notmds;		/* # out of tmds occurences */
	uint32_t le_notbufs;		/* # out of buffers for xmit */
	uint32_t le_norbufs;		/* # out of buffers for receive */
	uint32_t le_nocanput;		/* # input canput() returned false */
	uint32_t le_allocbfail;		/* # esballoc/allocb failed */
	timeout_id_t le_timeout_id;	/* watchdog timeout id */
	int le_tx_lbolt;		/* time of last tx interrupt */
	int le_rx_lbolt;		/* time of last rx interrupt */
	uint32_t *le_dma2_tcsr;		/* pointer to dma2 tst_csr */

	/*
	 * MIB II variables
	 */
	uint32_t le_rcvbytes;		/* # bytes received */
	uint32_t le_xmtbytes;		/* # bytes transmitted */
	uint32_t le_multircv;		/* # multicast packets received */
	uint32_t le_multixmt;		/* # multicast packets for xmit */
	uint32_t le_brdcstrcv;		/* # broadcast packets received */
	uint32_t le_brdcstxmt;		/* # broadcast packets for xmit */
	uint32_t le_norcvbuf;		/* # rcv packets discarded */
	uint32_t le_noxmtbuf;		/* # xmit packets discarded */
};

/* flags */
#define	LERUNNING	0x01	/* chip is initialized */
#define	LESLAVE		0x02	/* slave device (no DMA) */
#define	LEPROMISC	0x04	/* promiscuous mode enabled */
#define	LESUSPENDED	0x10	/* suspended interface */

/*
 * Fast aligned copy requires both the source and destination
 * addresses have the same offset from some N-byte boundary.
 */
#define	LEBURSTSIZE	(64)
#define	LEBURSTMASK	(LEBURSTSIZE-1)

#define	LEDRAINTIME	(200000)	/* # microseconds xmit drain */

#define	LEROUNDUP(a, n)	(((a) + ((n) - 1)) & ~((n) - 1))

/*
 * The lance can only handle addresses in the top 16 Meg of memory.
 */
#define	LETOP16MEG	(uint32_t)0xff000000

/*
 * Xmit/receive buffer structure.
 * This structure is organized to meet the following requirements:
 * - lb_buf[] starts on an LEBURSTSIZE boundary.
 * - lebuf is an even multiple of LEBURSTSIZE
 * - lb_buf[] is large enough to contain max frame (ETHERMAX)
 *	plus (3 x LEBURSTSIZE) rounded up to the next LEBURSTSIZE
 */
#define	LEBUFSIZE	(1728)
#define	LEBUFPAD	(LEBURSTSIZE - sizeof (struct le *) \
			    - sizeof (frtn_t))

struct	lebuf {
	uchar_t	lb_buf[LEBUFSIZE];	/* raw buffer */
	struct	le	*lb_lep;	/* link to device structure */
	frtn_t	lb_frtn;		/* for esballoc() */
	uchar_t	pad[LEBUFPAD];
};

/*
 * Define offset from start of lb_buf[] to point receive descriptor.
 * Requirements:
 * - must be 14 bytes back of a 4-byte boundary so the start of
 *   the network packet is 4-byte aligned.
 * - leave some headroom for others
 */
#define		LEHEADROOM	34

/*
 * Private DLPI full dlsap address format.
 */
struct	ledladdr {
	struct	ether_addr	dl_phys;
	ushort_t		dl_sap;
};

/*
 * Ops structure to accomodate the differences between
 * different implementations of the LANCE in Suns.
 * This declaration will grow as time goes on...
 */
struct	leops {
	struct	leops	*lo_next;	/* next in linked list */
	dev_info_t	*lo_dip;	/* node pointer (key) */
	uint_t	lo_flags;		/* misc. flags */
	uintptr_t lo_base;		/* LANCE memory base address */
	int	lo_size;		/* LANCE memory size in bytes */
	int	(*lo_init)();		/* device-specific init routine */
	int	(*lo_intr)();		/* device-specific intr routine */
	caddr_t	lo_arg;			/* device-specific arg */
};

/* leops flags */
#define	LOSLAVE	0x1			/* Slave (no DMA) device */

/*
 * "Export" a few of the error counters via the kstats mechanism.
 */
struct	lestat {
	struct	kstat_named	les_ipackets;
	struct	kstat_named	les_ierrors;
	struct	kstat_named	les_opackets;
	struct	kstat_named	les_oerrors;
	struct	kstat_named	les_collisions;
	struct	kstat_named	les_defer;
	struct	kstat_named	les_fram;
	struct	kstat_named	les_crc;
	struct	kstat_named	les_oflo;
	struct	kstat_named	les_uflo;
	struct	kstat_named	les_missed;
	struct	kstat_named	les_tlcol;
	struct	kstat_named	les_trtry;
	struct	kstat_named	les_tnocar;
	struct	kstat_named	les_inits;
	struct	kstat_named	les_notmds;
	struct	kstat_named	les_notbufs;
	struct	kstat_named	les_norbufs;
	struct	kstat_named	les_nocanput;
	struct	kstat_named	les_allocbfail;

	/*
	 * required by kstat for MIB II objects (RFC 1213)
	 */
	struct  kstat_named	les_rcvbytes;	/* # octets received */
						/* MIB - ifInOctets */
	struct  kstat_named	les_xmtbytes;  	/* # octets transmitted */
						/* MIB - ifOutOctets */
	struct  kstat_named	les_multircv;	/* # multicast packets */
						/* delivered to upper layer */
						/* MIB - ifInNUcastPkts */
	struct  kstat_named	les_multixmt;	/* # multicast packets */
						/* requested to be sent */
						/* MIB - ifOutNUcastPkts */
	struct  kstat_named	les_brdcstrcv; 	/* # broadcast packets */
						/* delivered to upper layer */
						/* MIB - ifInNUcastPkts */
	struct  kstat_named	les_brdcstxmt;	/* # broadcast packets */
						/* requested to be sent */
						/* MIB - ifOutNUcastPkts */
	struct  kstat_named	les_norcvbuf;	/* # rcv packets discarded */
						/* MIB - ifInDiscards */
	struct  kstat_named	les_noxmtbuf;	/* # xmt packets discarded */
						/* MIB - ifOutDiscards */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LE_H */
