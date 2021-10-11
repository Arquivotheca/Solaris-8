/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_QE_H
#define	_SYS_QE_H

#pragma ident	"@(#)qe.h	1.33	99/10/22 SMI"

/*
 * Declarations and definitions specific to the
 * Quad Ethernet Device (QED) Driver.
 */

#include <sys/kstat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for module_info.
 */
#define	QEIDNUM		(102)		/* module ID number */
#define	QENAME		"qe"		/* module name */
#define	QEMINPSZ	(0)		/* min packet size */
#define	QEMAXPSZ	1514		/* max packet size */
#define	QEHIWAT		(32 * 1024)	/* hi-water mark */
#define	QELOWAT		(1)		/* lo-water mark */

/*
 * Per-Stream instance state information.
 *
 * Each instance is dynamically allocated at open() and free'd
 * at close().  Each per-Stream instance points to at most one
 * per-device structure using the sq_qep field.  All instances
 * are threaded together into one list of active instances
 * ordered on minor device number.
 */

#define	NMCHASH	64			/* # of multicast hash buckets */
#define	INIT_BUCKET_SIZE	16	/* Initial Hash Bucket Size */

struct	qestr {
	struct	qestr	*sq_nextp;	/* next in list */
	queue_t	*sq_rq;			/* pointer to our rq */
	struct	qe *sq_qep;		/* attached device */
	t_uscalar_t	sq_state;	/* current DL state */
	t_uscalar_t	sq_sap;		/* bound sap */
	uint_t	sq_flags;		/* misc. flags */
	minor_t	sq_minor;		/* minor device number */

	struct	ether_addr
		*sq_mctab[NMCHASH];	/* table of multicast addrs */
	uint_t	sq_mccount[NMCHASH];	/* # valid addresses in mctab[i] */
	uint_t	sq_mcsize[NMCHASH];	/* Allocated size of mctab[i] */

	ushort_t	sq_ladrf[4];		/* Multicast filter bits */
	ushort_t	sq_ladrf_refcnt[64];	/* Ref cnt for filter bits */

	kmutex_t	sq_lock;	/* protect this structure */
};

#define	MCHASH(a)	((*(((uchar_t *)(a)) + 0) ^		\
			*(((uchar_t *)(a)) + 1) ^		\
			*(((uchar_t *)(a)) + 2) ^		\
			*(((uchar_t *)(a)) + 3) ^		\
			*(((uchar_t *)(a)) + 4) ^		\
			*(((uchar_t *)(a)) + 5)) % (uint_t)NMCHASH)


/* per-stream flags */
#define	QESFAST		0x01	/* "M_DATA fastpath" mode */
#define	QESRAW		0x02	/* M_DATA plain raw mode */
#define	QESALLPHYS	0x04	/* "promiscuous mode" */
#define	QESALLMULTI	0x08	/* enable all multicast addresses */
#define	QESALLSAP	0x10	/* enable all ether type values */

/*
 * Maximum # of multicast addresses per Stream.
 */
#define	QEMAXMC	64
#define	QEMCALLOC	(QEMAXMC * sizeof (struct ether_addr))

/*
 * Maximum number of receive descriptors posted to the chip.
 */
#define	QERPENDING	64
#define	QETPENDING	64

/*
 * Full DLSAP address length (in struct dladdr format).
 */
#define	QEADDRL	(sizeof (ushort_t) + ETHERADDRL)

/*
 * Return the address of an adjacent descriptor in the given ring.
 */
#define	NEXTRMD(qep, rmdp)	(((rmdp) + 1) == (qep)->qe_rmdlimp	\
	? (qep)->qe_rmdp : ((rmdp) + 1))
#define	NEXTTMD(qep, tmdp)	(((tmdp) + 1) == (qep)->qe_tmdlimp	\
	? (qep)->qe_tmdp : ((tmdp) + 1))
#define	PREVTMD(qep, tmdp)	((tmdp) == (qep)->qe_tmdp		\
	? ((qep)->qe_tmdlimp - 1) : ((tmdp) - 1))

#define	SECOND(t)	t*1000
#define	QE_SANITY_TIMER		SECOND(30)

/*
 * QED Per-Channel instance state information.
 *
 * Each instance is dynamically allocated on first attach.
 */
struct	qe {
	struct	qe		*qe_nextp;	/* next in a linked list */
	dev_info_t		*qe_dip;	/* associated dev_info */
	struct	ether_addr	qe_ouraddr;	/* individual address */
	int			qe_chan;	/* channel no */
	timeout_id_t		qe_timerid;	/* Timer Id */
	int			qe_once;	/* attempt since last check */
	int			qe_ihang;	/* Port Hang Flag */
	uint_t			qe_flags;	/* misc. flags */
	uint_t			qe_wantw;	/* xmit: out of resources */
	ddi_iblock_cookie_t	qe_cookie;	/* cookie from ddi_add_intr */

	volatile struct	qecm_chan	*qe_chanregp;	/* QEC chan regs */
	volatile struct	mace		*qe_maceregp;	/* MACE regs */

	kmutex_t	qe_xmitlock;		/* protect xmit-side fields */
	kmutex_t	qe_intrlock;		/* protect intr-side fields */
	kmutex_t	qe_buflock;		/* protect private buffers */

	struct	qmd	*qe_rmdp;	/* receive descriptor ring start */
	struct	qmd	*qe_rmdlimp;	/* receive descriptor ring end */
	struct	qmd	*qe_tmdp;	/* transmit descriptor ring start */
	struct	qmd	*qe_tmdlimp;	/* transmit descriptor ring end */
	volatile	struct	qmd	*qe_rnextp;	/* next chip rmd */
	volatile	struct	qmd	*qe_rlastp;	/* last free rmd */
	volatile	struct	qmd	*qe_tnextp;	/* next free tmd */
	volatile	struct	qmd	*qe_tcurp;	/* next tmd to reclm */

	mblk_t 	*qe_tmblkp[QEC_QMDMAX];		/* qebuf associated with TMD */
	mblk_t 	*qe_rmblkp[QEC_QMDMAX];		/* qebuf associated with RMD */

	queue_t	*qe_ipq;		/* ip read queue */

	/*
	 * DDI dma handle, kernel virtual base,
	 * and io virtual base of IOPB area.
	 */
	ddi_dma_handle_t	qe_iopbhandle;
	uintptr_t		qe_iopbkbase;
	uintptr_t		qe_iopbiobase;

	/*
	 * these are handles for the dvma resources reserved
	 * by dvma_reserve
	 */
	ddi_dma_handle_t	qe_dvmarh;	/* dvma recv handle */
	ddi_dma_handle_t	qe_dvmaxh;	/* dvma xmit handle */

	/*
	 * these are used if dvma reserve fails, and we have to fall
	 * back on the older ddi_dma_addr_setup routines
	 */
	ddi_dma_handle_t	*qe_dmarh;
	ddi_dma_handle_t	*qe_dmaxh;

	kstat_t	*qe_ksp;	/* kstat pointer */

	uint32_t	qe_ipackets;	/* # packets received */
	uint32_t	qe_iipackets;	/* # packets received */
	uint32_t	qe_ierrors;	/* # total input errors */
	uint32_t	qe_opackets;	/* # packets sent */
	uint32_t	qe_oopackets;	/* # packets sent @ last check */
	uint32_t	qe_oerrors;	/* # total output errors */
	uint32_t	qe_txcoll;	/* # xmit collisions */
	uint32_t	qe_rxcoll;	/* # recv collisions */
	uint32_t	qe_defer;	/* # excessive defers */
	uint32_t	qe_fram;	/* # recv framing errors */
	uint32_t	qe_crc;		/* # recv crc errors */
	uint32_t	qe_buff;	/* # recv packet sizes > buffer size */
	uint32_t	qe_ifspeed;	/* interface speed */
	uint32_t	qe_oflo;	/* # recv overflow */
	uint32_t	qe_uflo;	/* # xmit underflow */
	uint32_t	qe_missed;	/* # recv missed */
	uint32_t	qe_tlcol;	/* # xmit late collision */
	uint32_t	qe_trtry;	/* # xmit retry failures */
	uint32_t	qe_tnocar;	/* # loss of carrier errors */
	uint32_t	qe_inits;	/* # driver inits */
	uint32_t	qe_nocanput;	/* # canput() failures */
	uint32_t	qe_allocbfail;	/* # allocb() failures */
	uint32_t	qe_runt;	/* # recv runt packets */
	uint32_t	qe_jab;		/* # mace jabber errors */
	uint32_t	qe_babl;	/* # mace babble errors */
	uint32_t	qe_tmder;	/* # chained tx desc. errors */
	uint32_t	qe_laterr;	/* # sbus tx late error */
	uint32_t	qe_parerr;	/* # sbus tx parity errors */
	uint32_t	qe_errack;	/* # sbus tx error acks */
	uint32_t	qe_notmds;	/* # out of tmds */
	uint32_t	qe_notbufs;	/* # out of xmit buffers */
	uint32_t	qe_norbufs;	/* # out of recv buffers */
	uint32_t	qe_clsn;	/* # recv late collisions */

	/*
	 * MIB II variables
	 */
	uint32_t	qe_rcvbytes;	/* # bytes received */
	uint32_t	qe_xmtbytes;	/* # bytes transmitted */
	uint32_t	qe_multircv;	/* # multicast packets received */
	uint32_t	qe_multixmt;	/* # multicast packets for xmit */
	uint32_t	qe_brdcstrcv;	/* # broadcast packets received */
	uint32_t	qe_brdcstxmt;	/* # broadcast packets for xmit */
	uint32_t	qe_norcvbuf;	/* # rcv packets discarded */
	uint32_t	qe_noxmtbuf;	/* # xmit packets discarded */

};

/* flags */
#define	QERUNNING	0x01	/* chip is initialized */
#define	QEPROMISC	0x02	/* promiscuous mode enabled */
#define	QEDMA		0x08	/* this is true when using the */
				/* the ddi_dma kind of interfaces */
#define	QESUSPENDED	0x10	/* suspended interface */
#define	QESTOP		0x20    /* Stopped Interface */

struct	qekstat {
	struct kstat_named	qk_ipackets;	/* # packets received */
	struct kstat_named	qk_ierrors;	/* # total input errors */
	struct kstat_named	qk_opackets;	/* # packets sent */
	struct kstat_named	qk_oerrors;	/* # total output errors */
	struct kstat_named	qk_txcoll;	/* # xmit collisions */
	struct kstat_named	qk_rxcoll;	/* # recv collisions */
	struct kstat_named	qk_defer;	/* # defers */
	struct kstat_named	qk_fram;	/* # recv framing errors */
	struct kstat_named	qk_crc;		/* # recv crc errors */
	struct kstat_named	qk_ifspeed;	/* interface speed  */
	struct kstat_named	qk_buff;	/* # rx pkt sizes > buf size */
	struct kstat_named	qk_oflo;	/* # recv overflow */
	struct kstat_named	qk_uflo;	/* # xmit underflow */
	struct kstat_named	qk_missed;	/* # recv missed */
	struct kstat_named	qk_tlcol;	/* # xmit late collision */
	struct kstat_named	qk_trtry;	/* # xmit retry failures */
	struct kstat_named	qk_tnocar;	/* # loss of carrier errors */
	struct kstat_named	qk_inits;	/* # driver inits */
	struct kstat_named	qk_nocanput;	/* # canput() failures */
	struct kstat_named	qk_allocbfail;	/* # allocb() failures */
	struct kstat_named	qk_runt;	/* # recv runt packets */
	struct kstat_named	qk_jab;		/* # mace jabber errors */
	struct kstat_named	qk_babl;	/* # mace babble errors */
	struct kstat_named	qk_tmder;	/* # chained tx desc. errors */
	struct kstat_named	qk_laterr;	/* # sbus tx late error */
	struct kstat_named	qk_parerr;	/* # sbus tx parity errors */
	struct kstat_named	qk_errack;	/* # sbus tx error acks */
	struct kstat_named	qk_notmds;	/* # out of tmds */
	struct kstat_named	qk_notbufs;	/* # out of xmit buffers */
	struct kstat_named	qk_norbufs;	/* # out of recv buffers */
	struct kstat_named	qk_clsn;	/* # late collisions */

	/*
	 * required by kstat for MIB II objects(RFC 1213)
	 */
	struct kstat_named	qk_rcvbytes;	/* # octets received */
						/* MIB - ifInOctets */
	struct kstat_named	qk_xmtbytes;	/* # octets transmitted */
						/* MIB - ifOutOctets */
	struct kstat_named	qk_multircv;	/* # multicast packets */
						/* delivered to upper layer */
						/* MIB - ifInNUcastPkts */
	struct kstat_named	qk_multixmt;	/* # multicast packets */
						/* requested to be sent */
						/* MIB - ifOutNUcastPkts */
	struct kstat_named	qk_brdcstrcv;	/* # broadcast packets */
						/* delivered to upper layer */
						/* MIB - ifInNUcastPkts */
	struct kstat_named	qk_brdcstxmt;	/* # broadcast packets */
						/* requested to be sent */
						/* MIB - ifOutNUcastPkts */
	struct kstat_named	qk_norcvbuf;	/* # rcv packets discarded */
						/* MIB - ifInDiscards */
	struct kstat_named	qk_noxmtbuf;	/* # xmt packets discarded */
						/* MIB - ifOutDiscards */


};

/*
 * Fast aligned copy requires both the source and destination
 * addresses have the same offset from some N-byte boundary.
 */
#define	QEBCOPYALIGN	(64)
#define	QEBCOPYMASK	(QEBCOPYALIGN-1)

#define	QEDRAINTIME	(400000)	/* # microseconds xmit drain */
#define	QELNKTIME	(500000)	/* time to mark link state up */

#define	QEROUNDUP(a, n)	(((a) + ((n) - 1)) & ~((n) - 1))
#define	QEROUNDUP2(a, n) (uchar_t *)((((uintptr_t)(a)) + \
				((n) - 1)) & ~((n) - 1) + 2)

/* speed */
#define	QE_SPEED	10


/*
 * Xmit/receive buffer structure.
 * This structure is organized to meet the following requirements:
 * - qb_buf starts on an QEBURSTSIZE boundary.
 * - qebuf is an even multiple of QEBURSTSIZE
 * - qb_buf[] is large enough to contain max frame (1518) plus
 *   QEBURSTSIZE for alignment adjustments
 */
#define	QEBURSTSIZE	(64)
#define	QEBURSTMASK	(QEBURSTSIZE - 1)
#define	QEBUFSIZE	(1728 - sizeof (struct qe *) - sizeof (frtn_t))

struct	qebuf {
	uchar_t	qb_buf[QEBUFSIZE];	/* raw buffer */
	struct	qe	*qb_qep;	/* link to device structure */
	frtn_t	qb_frtn;		/* for esballoc() */
};

/*
 * Define offset from start of qb_buf[] to point receive descriptor.
 * Requirements:
 * - must be 14 bytes back of a 4-byte boundary so the start of
 *   the network packet is 4-byte aligned.
 * - leave some headroom for others
 */
#define	QEHEADROOM	34

/*
 * Private DLPI full dlsap address format.
 */
struct	qedladdr {
	struct	ether_addr	dl_phys;
	ushort_t		dl_sap;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_QE_H */
