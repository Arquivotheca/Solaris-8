/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FIBRE_CHANNEL_FCA_USOCVAR_H
#define	_SYS_FIBRE_CHANNEL_FCA_USOCVAR_H

#pragma ident	"@(#)usocvar.h	1.4	99/10/18 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * usocvar.h - SOC+ Driver data struct definitions
 */
#include <sys/fibre-channel/impl/fc_fcaif.h>

/* Define to Compile for driver hardening */
#define	USOC_DRV_HARDENING		1

/*
 * Define default name and # of SOC+s to allocate to the system
 */
#define	USOC_PORTA_NAME		"0"	/* node for port a */
#define	USOC_PORTB_NAME		"1"	/* node for port b */
#define	USOC_NT_PORT		NULL
#define	USOC_INIT_ITEMS		5
#define	USOC_WWN_SIZE		(sizeof (la_wwn_t))


/*
 * message printing
 */
#define	USOC_LOG_ONLY		1
#define	USOC_CONSOLE_ONLY	2
#define	USOC_LOG_AND_CONSOLE	3


/*
 * Timelist allocation, management.
 */
#define	USOC_TIMELIST_SIZE	(63)	/* (2 ^ n - 1) */
#define	USOC_TIMELIST_HASH(x)	((x) & (USOC_TIMELIST_SIZE))

/*
 * Defines for the Circular Queues
 */
#define	USOC_MAX_CQ_ENTRIES	256	/* Maximum number of CQ entries. */
#define	USOC_CQ_SIZE 		(sizeof (cqe_t) * USOC_MAX_CQ_ENTRIES)

#define	USOC_SMALL_CQ_ENTRIES	8	/* Number of CQ entries for small Q */

#define	USOC_N_CQS		4	/* Number of queues we use */
#define	USOC_HW_N_CQS		4	/* Number of queues the hardware has */
#define	USOC_CQ_ALIGN		64	/* alignment boundary */

/*
 * Misc. Macros
 */
#define	USOC_POOL_SIZE		2112
#define	USOC_SVC_LENGTH		80	/* must be multiple of 4 bytes */

#define	FABRIC_FLAG		1
#define	NPORT_FLAG		2

#define	FCIO_DIAG_LBTFQ		(FIOC|203)
#define	USOC_DIAG_LBTFQ		0x0a
#define	PORT_LBF_PENDING	0x00100000
#define	USOC_LBF_TIMEOUT	15		/* seconds */

#define	USOC_QFULL		82565
#define	USOC_UNAVAIL		82566

#define	USOC_INVALID_ID		((uint32_t)0xFFFFFFFF)

#define	USOC_MAX_SPURIOUS_INTR	0x50	/* Max spurious intrs allowed */
#define	USOC_MAX_REC_RESETS	10	/* Max resets in short time */
#define	USOC_MIN_REC_RESET_TIME	300	/* Min time between reset recs */
#define	USOC_LFD_INTERVAL	300	/* secs */
#define	USOC_LFD_MAX_RETRIES	6

#define	USOC_LFD_WAIT_COUNT	20	/* wait loop count during detach */
#define	USOC_LFD_WAIT_DELAY	1000000	/* usecs delay for wait loop */

#define	USOC_MIN_THROTTLE	50	/* minimum throttle for usoc */
#define	USOC_MAX_THROTTLE	800	/* max cmds to usoc */
#define	USOC_THROTTLE_THRESHOLD	200	/* threshold while firmware reload */


#define	USOC_STATUS_DIAG_BUSY		82565
#define	USOC_STATUS_DIAG_INVALID	82566

#define	USOC_PRIV_TO_PORTNUM(p)		((p)->spp_portp->sp_port)
#define	USOC_PORT_ANY			(-1)

/*
 * Taking firmware core when a lot of I/Os are going on results in
 * kernel data structure corruption (In some cases freed objects
 * had 8 bytes scribbled at the end of the kmem object). So define
 * control bits on taking core
 */
#define	USOC_CORE_ON_ABORT_TIMEOUT	0x0001
#define	USOC_CORE_ON_BAD_TOKEN		0x0002
#define	USOC_CORE_ON_BAD_ABORT		0x0004
#define	USOC_CORE_ON_BAD_UNSOL		0x0008
#define	USOC_CORE_ON_LOW_THROTTLE	0x0010
#define	USOC_CORE_ON_SEND_1		0x0020
#define	USOC_CORE_ON_SEND_2		0x0040
#define	USOC_FORCE_CORE			0x8000	/* To force a core */

typedef	struct flb_hdr {
	uint_t max_length;
	uint_t length;
} flb_hdr_t;

/*
 * USOC UNIX circular queue descriptor.
 */

typedef struct usoc_kernel_cq {
	kmutex_t	skc_mtx;	/* MT lock for CQ manipulation  */
	kcondvar_t	skc_cv;		/* cond var for CQ manipulation. */
	ddi_dma_handle_t skc_dhandle;	/* DDI DMA handle to CQ. */
	ddi_dma_cookie_t skc_dcookie;	/* DDI DMA Cookie. */
	ddi_acc_handle_t skc_acchandle;	/* DDI DMA access handle */
	usoc_cq_t	*skc_xram_cqdesc; /* Pointer to XRAM CQ desc */
	caddr_t		skc_cq_raw;	/* Pointer to unaligned CQ mem pool */
	cqe_t		*skc_cq;	/* Pointer to CQ memory pool. */
	uchar_t		skc_in;		/* Current Input pointer. */
	uchar_t		skc_out;	/* Current Input pointer. */
	uchar_t		skc_last_index;	/* Last cq index. */
	uchar_t		skc_seqno;	/* Current Go Around in CQ. */
	uchar_t		skc_full;	/* Indication of full. */
	uchar_t		skc_saved_out;	/* Current Input pointer. */
	uchar_t		skc_saved_seqno;	/* Current Go Around in CQ. */
	timeout_id_t	deferred_intr_timeoutid;
	struct usoc_pkt_priv	*skc_overflowh; /* cq overflow head */
	struct usoc_pkt_priv	*skc_overflowt;	/* cq overflow tail */
} usoc_kcq_t;

/*
 * Values for skc_full
 */
#define	USOC_SKC_FULL	1
#define	USOC_SKC_SLEEP	2

/*
 * State change callback routine descriptor
 *
 * There is one entry in this list for each hba that is attached
 * to this port.
 * This structure will need to be mutex protected when parallel
 * attaches are supported.
 */
typedef struct usoc_unsol_cb {
	struct usoc_unsol_cb	*next;
	uchar_t			type;
	void			(*statec_cb)(void *, uint32_t);
	void			(*els_cb)(void *, cqe_t *, caddr_t);
	void			(*data_cb)(void *, cqe_t *, caddr_t);
	void			*arg;
} usoc_unsol_cb_t;

/*
 * Unsolicited buffer descriptors
 */
typedef struct usoc_unsol_buf {
	uint32_t		pool_type;	/* FC-4 type */
	uint32_t		pool_id; /* pool id for this buffer pool */
	uint32_t		pool_buf_size;	/* buf size of each pool buf */
	uint16_t		pool_nentries;	/* no.of bufs in pool */
	uint32_t		pool_hdr_mask;  /* header mask for pool */
	fc_unsol_buf_t		*pool_fc_ubufs; /* array of unsol buf structs */
	struct usoc_unsol_buf	*pool_next;
	struct pool_dma_res {
		ddi_dma_handle_t	pool_dhandle;
		ddi_dma_cookie_t	pool_dcookie;
		ddi_acc_handle_t	pool_acchandle;
		caddr_t			pool_buf;
	} *pool_dma_res_ptr;	/* dma resources for buffers in the pool */
} usoc_unsol_buf_t;

/*
 * SOC+ port status decriptor.
 */
typedef struct usoc_port {
	uint32_t		sp_status;	/* port status */
	struct usoc_state	*sp_board;	/* hardware for instance */

	uint32_t		sp_src_id;	/* Our nport id */
	uint32_t		sp_port;	/* Our physical port (0, 1) */
	la_wwn_t		sp_p_wwn;	/* Our Port WorldWide Name */

	void			(*sp_statec_callb)();
	void			(*sp_unsol_callb)();
	opaque_t		sp_tran_handle;
	uint32_t		sp_open;	/* open count */
	int			sp_pktinits;
	usoc_unsol_buf_t	*usoc_unsol_buf;

	kmutex_t		sp_mtx;		/* Per port mutex */
	kcondvar_t		sp_cv;		/* Per port condvariable */
	uint32_t		sp_of_ncmds;

	kcondvar_t		sp_unsol_cv;	/* XXX: temp for unsol ioctl */
	fc_unsol_buf_t		*sp_unsol_buf;	/* XXX: pointer to rcvd ub */
	kmutex_t		sp_unsol_mtx;	/* XXX: to serialize ioctls */
} usoc_port_t;

#define	PORT_FABRIC_PRESENT	0x00000001
#define	PORT_OFFLINE		0x00000002
#define	NPORT_LOGIN_SUCCESS	0x00000004
#define	PORT_LOGIN_ACTIVE	0x00000008
#define	PORT_LOGIN_RECOVERY	0x00000010
#define	PORT_ONLINE_LOOP	0x00000020
#define	PORT_ONLINE		0x00000040
#define	PORT_STATUS_FLAG	0x00000080
#define	PORT_STATUS_MASK	0x000000ff
#define	PORT_OPEN		0x00000100
#define	PORT_CHILD_INIT		0x00000200
#define	PORT_BOUND		0x00000400
#define	PORT_EXCL		0x00000800
#define	PORT_LILP_PENDING	0x00001000
#define	PORT_LIP_PENDING	0x00002000
#define	PORT_ABORT_PENDING	0x00004000
#define	PORT_ELS_PENDING	0x00008000
#define	PORT_BYPASS_PENDING	0x00010000
#define	PORT_OFFLINE_PENDING	0x00020000
#define	PORT_ADISC_PENDING	0x00040000
#define	PORT_RLS_PENDING	0x00080000
#define	PORT_FLOGI_PENDING	0x00100000
#define	PORT_NS_PENDING		0x00200000
#define	PORT_IN_LINK_RESET	0x00400000
#define	PORT_OUTBOUND_PENDING	0x00800000
#define	PORT_DIAG_PENDING	0x01000000

#define	USOC_IS_QFULL(kcq)	(((((kcq)->skc_in + 1) &\
		(kcq)->skc_last_index) == (kcq)->skc_out))

#define	FC_TYPE_IOCTL		0x100000

#define	USOC_NOINTR_POLL_DELAY_TIME	1000    /* usec */

#define	USOC_LILP_TIMEOUT		30	/* sec */
#define	USOC_LIP_TIMEOUT		30	/* sec */
#define	USOC_ABORT_TIMEOUT		20	/* sec */
#define	USOC_BYPASS_TIMEOUT		5	/* sec */
#define	USOC_OFFLINE_TIMEOUT		90	/* sec */
#define	USOC_ADISC_TIMEOUT		15	/* sec */
#define	USOC_RLS_TIMEOUT		15	/* sec */
#define	USOC_DIAG_TIMEOUT		15	/* sec */
#define	USOC_PKT_TIMEOUT		45	/* sec */

#define	USOC_MIN_PACKETS		10	/* Advisory; Don't worry */
#define	USOC_MAX_PACKETS		1024	/* Advisory; Don't worry */
#define	USOC_NUM_TIMEOUT_THREADS	4	/* 2 threads per port */
#define	USOC_WATCH_TIMER		1000000	/* usec */


/*
 * usoc_timetag_t is the timeout record for timing out requests
 */
typedef struct usoc_timetag {
	void			(*sto_func) (fc_packet_t *);
	fc_packet_t		*sto_pkt;
	uint32_t		sto_ticks;
	struct usoc_timetag	*sto_next;
	struct usoc_timetag	*sto_prev;
	struct usoc_timetag	*sto_tonext;	/* timeout list */
} usoc_timetag_t;

/*
 * We wait for up to USOC_INITIAL_ONLINE seconds for the first
 * usoc to come on line. The timeout in the usoc firmware is 10 seconds.
 * The timeout is to let any outstanding commands drain before
 * coming back on line, after going off-line.
 */
#define	USOC_INITIAL_ONLINE	30	/* secs for first online from usoc */

#define	USOC_MAX_IDS		2048    /* Max cmds to usoc. Must be 2^x */
#define	USOC_ID_SHIFT		11	/* since 2^11 = 2048 */

typedef struct usoc_idinfo {
	fc_packet_t	*id_token[USOC_MAX_IDS];	/* id to token */
	unsigned short	id_multiplier[USOC_MAX_IDS];	/* for increasing IDs */
	unsigned short	id_nextfree[USOC_MAX_IDS];	/* free list */
	unsigned short	id_freelist_head;		/* free list head */
	unsigned short	id_pad;				/* pad */
} usoc_idinfo_t;

/*
 * USOC state structure
 */
typedef struct usoc_state {
	dev_info_t		*usoc_dip;
	caddr_t 		usoc_eeprom;	/* pointer to soc+ eeprom */
	ddi_acc_handle_t	usoc_eeprom_acchandle;	/* for h/w reg access */

	caddr_t 		usoc_xrp;	/* pointer to soc+ xram */
	ddi_acc_handle_t	usoc_xrp_acchandle;	/* for h/w reg access */

	usoc_cq_t		*usoc_xram_reqp; /* addr of request queue */
	usoc_cq_t		*usoc_xram_rspp; /* addr of response queue */

	usoc_kcq_t		usoc_request[USOC_N_CQS]; /* request queues */
	usoc_kcq_t		usoc_response[USOC_N_CQS]; /* response queues */

	uint32_t		usoc_alive_time;  /* for latent fault */
	uint32_t		usoc_cfg;	/* copy of the config reg */

	kmutex_t		usoc_k_imr_mtx;	/* mutex for interrupt masks */
	uint32_t		usoc_k_imr;	/* copy of soc+'s mask reg */
	ddi_acc_handle_t	usoc_rp_acchandle;	/* for h/w reg access */
	usoc_reg_t		*usoc_rp;	/* pointer to soc+ registers */

	ddi_iblock_cookie_t	usoc_iblkc;	/* interrupt cookies */
	kmutex_t		usoc_fault_mtx;	/* mutex for reporting fault */

	uchar_t			*usoc_pool;	/* unsolicited buffer pool */
	ddi_dma_handle_t	usoc_pool_dhandle;
	ddi_dma_cookie_t	usoc_pool_dcookie;
	ddi_acc_handle_t	usoc_pool_acchandle;

	kmutex_t		usoc_time_mtx;
	usoc_timetag_t		*usoc_timelist[USOC_TIMELIST_SIZE + 1];

	taskq_t			*usoc_task_handle;
	timeout_id_t		usoc_watch_tid;

				/* handles to soc+ ports */
	usoc_port_t		usoc_port_state[N_USOC_NPORTS];
	la_wwn_t		usoc_n_wwn;	/* Our Node WorldWide Name */
	char			usoc_service_params[USOC_SVC_LENGTH];

	char			usoc_name[MAXPATHLEN];
	kstat_t			*usoc_ksp;
	struct usoc_stats	usoc_stats;	/* kstats */

	kmutex_t		usoc_board_mtx;	/* Per board mutex */
	uint32_t		usoc_ncmds;
	usoc_idinfo_t		usoc_idinfo;    /* For ID/Token translation */

	uint32_t		usoc_throttle;
	uint32_t		usoc_ticker;

	uint32_t		usoc_reset_rec_time;
	uchar_t			usoc_rec_resets;
	uchar_t			usoc_intr_added;
	uchar_t			usoc_lfd_pending; /* latent flt detection */
	uchar_t			usoc_shutdown;

	uchar_t			usoc_spurious_sol_intrs;
	uchar_t			usoc_spurious_unsol_intrs;
	uchar_t			usoc_reload_pending;
	uchar_t			usoc_pad;	/* pad */
	int			usoc_instance;
} usoc_state_t;


/*
 * Structure used when the usoc driver needs to issue commands of its own
 */
typedef struct usoc_priv_cmd {
	void			*fapktp;
	uint32_t		flags;
	caddr_t			cmd;
	caddr_t			rsp;
	ddi_dma_handle_t	cmd_handle;
	ddi_acc_handle_t	cmd_acchandle;
	ddi_dma_handle_t	rsp_handle;
	ddi_acc_handle_t	rsp_acchandle;
	void 			(*callback)();	/* callback to ULP, if any */
	void			*arg;		/* callback arg */
	caddr_t			*payload;	/* payload callback or stash */
} usoc_priv_cmd_t;

#define	PACKET_DORMANT		0x00000001
#define	PACKET_IN_TRANSPORT	0x00000002
#define	PACKET_IN_ABORT		0x00000004
#define	PACKET_INTERNAL_ABORT	0x00000008
#define	PACKET_CALLBACK_DONE	0x00010000
#define	PACKET_IN_TIMEOUT	0x00100000
#define	PACKET_IN_INTR		0x00200000
#define	PACKET_NO_CALLBACK	0x00400000
#define	PACKET_IN_OVERFLOW_Q	0x00800000
#define	PACKET_IN_USOC		0x01000000
#define	PACKET_RETURNED		0x02000000
#define	PACKET_INTERNAL_PACKET	0x04000000
#define	PACKET_VALID		0x08000000
#define	PACKET_IN_PROCESS	(PACKET_IN_TRANSPORT | PACKET_IN_ABORT)

typedef struct usoc_pkt_priv {
	usoc_port_t		*spp_portp;		/* our port */
	uint32_t		spp_flags;	/* private packet flags */
	void			(*spp_saved_comp) (fc_packet_t *);
	kmutex_t		spp_mtx;
	usoc_request_t		spp_sr;
	uint32_t		spp_endflag;
	uint32_t		spp_diagcode;
	usoc_timetag_t		spp_timetag;
	fc_packet_t		*spp_packet;
	struct usoc_pkt_priv	*spp_next;
#ifdef DEBUG
	fc_packet_t		*spp_abort_pkt;
#endif
} usoc_pkt_priv_t;

#define	UB_VALID		0x80000000
#define	UB_IN_FCA		0x00000001
#define	UB_TEMPORARY		0x00000002

typedef struct usoc_ub_priv {
	usoc_port_t		*ubp_portp;
	uint32_t 		ubp_flags;
	struct pool_dma_res	*ubp_pool_dma_res;
	uint64_t		ubp_ub_token;
} usoc_ub_priv_t;

#define	USOC_PKT_COMP(pkt, priv)	\
	(((pkt)->pkt_comp != NULL) && 	\
	    (((priv)->spp_flags & PACKET_NO_CALLBACK) == 0))


/* Temporary defines to simulate failures */
#define	USOC_DDH_NOERR				0
#define	USOC_DDH_RP_INTR_ACCHDL_FAULT		1
#define	USOC_DDH_XRP_INITCQ_ACCHDL_FAULT	2
#define	USOC_DDH_SPURIOUS_SOL			3
#define	USOC_DDH_SPURIOUS_UNSOL			4
#define	USOC_DDH_FAIL_ALLOCID			5
#define	USOC_DDH_FAIL_FREEID			6
#define	USOC_DDH_FAIL_LATENT_FAULT		7
#define	USOC_DDH_FAIL_LATENT_DETACH		8
#define	USOC_DDH_FAIL_LATENT_SUSPEND		9
#define	USOC_DDH_ENABLE_ACCHDL_FAULT		10
#define	USOC_DDH_ENQUE1_ACCHDL_FAULT		11
#define	USOC_DDH_ENQUE2_ACCHDL_FAULT		12
#define	USOC_DDH_SOLINTR_ACCHDL_FAULT		13
#define	USOC_DDH_UNSOLINTR_ACCHDL_FAULT		14
#define	USOC_DDH_PKT_DATA_DMAHDL_FAULT		15


#ifdef USOC_DRV_HARDENING
/*
 * Macros to read/write using acc handle and ddi_get/ddi_put routines
 */
#define	USOC_RD8(acchandle, addr)	\
	ddi_get8((acchandle), (uint8_t *)(addr))
#define	USOC_RD16(acchandle, addr)	\
	ddi_get16((acchandle), (uint16_t *)(addr))
#define	USOC_RD32(acchandle, addr)	\
	ddi_get32((acchandle), (uint32_t *)(addr))
#define	USOC_RD64(acchandle, addr)	\
	ddi_get64((acchandle), (uint64_t *)(addr))

#define	USOC_WR8(acchandle, addr, val)	\
	ddi_put8((acchandle), (uint8_t *)(addr), (uint8_t)(val))
#define	USOC_WR16(acchandle, addr, val)	\
	ddi_put16((acchandle), (uint16_t *)(addr), (uint16_t)(val))
#define	USOC_WR32(acchandle, addr, val)	\
	ddi_put32((acchandle), (uint32_t *)(addr), (uint32_t)(val))
#define	USOC_WR64(acchandle, addr, val)	\
	ddi_put64((acchandle), (uint64_t *)(addr), (uint64_t)(val))

#define	USOC_REP_RD(acchandle, hostaddr, devaddr, cnt)	\
	ddi_rep_get8((acchandle), (uint8_t *)(hostaddr), (uint8_t *)(devaddr),\
	    (size_t)(cnt), DDI_DEV_AUTOINCR)
#define	USOC_REP_RD32(acchandle, hostaddr, devaddr, cnt)	\
	ddi_rep_get32((acchandle), (uint32_t *)(hostaddr),	\
	    (uint32_t *)(devaddr), ((size_t)(cnt)) >> 2, DDI_DEV_AUTOINCR)

#define	USOC_REP_WR32(acchandle, hostaddr, devaddr, cnt)	\
	ddi_rep_put32((acchandle), (uint32_t *)(hostaddr),\
	    (uint32_t *)(devaddr), ((size_t)(cnt)) >> 2, DDI_DEV_AUTOINCR)

/* Macros to sync kernel and I/O view of memory */
#define	USOC_SYNC_FOR_DEV(acchandle, offset, length)	\
	(void) ddi_dma_sync((acchandle), (offset),\
	    (length), DDI_DMA_SYNC_FORDEV)
#define	USOC_SYNC_FOR_KERNEL(acchandle, offset, length)	\
	(void) ddi_dma_sync((acchandle), (offset),\
	    (length), DDI_DMA_SYNC_FORKERNEL)

/* Macros to check device fault */
#define	USOC_GET_DEVSTATE(usocp)	(ddi_get_devstate((usocp)->usoc_dip))
#define	USOC_DEVSTATE_BAD(state)	\
	((state) == (DDI_DEVSTATE_DOWN) || (state) == (DDI_DEVSTATE_OFFLINE))

#define	USOC_DEVICE_BAD(usocp, state)	\
	((state = USOC_GET_DEVSTATE(usocp)), USOC_DEVSTATE_BAD(state))

#define	USOC_DEVICE_STATE(devstate)	\
	(((devstate) == DDI_DEVSTATE_OFFLINE) ? "OFFLINE" :	\
	(((devstate) == DDI_DEVSTATE_DOWN) ? "DOWN" : "UNKNOWN"))

/* Macros to check if acc/dma handles are marked faulty */
#define	USOC_ACC_HANDLE_BAD(acc_handle)	\
	(ddi_check_acc_handle((acc_handle)) != DDI_SUCCESS)
#define	USOC_DMA_HANDLE_BAD(dma_handle)	\
	(ddi_check_dma_handle((dma_handle)) != DDI_SUCCESS)

#ifdef DEBUG
#define	USOC_DDH_DMA_HANDLE_BAD(dma_handle, val)	\
	((usoc_dbg_drv_hdn == val) || (USOC_DMA_HANDLE_BAD(dma_handle)))
#else
#define	USOC_DDH_DMA_HANDLE_BAD(dma_handle, val)	\
	(USOC_DMA_HANDLE_BAD(dma_handle))
#endif

#define	USOC_REPORT_FAULT(usocp)	usoc_report_fault((usocp))
#define	USOC_DDI_REPORT_FAULT(a, b, c, d)	\
	(void) ddi_dev_report_fault((a), (b), (c), (d))

/* Check access handle fault and report device faulty if fault occured */
#define	USOC_ACCHDL_FAULT(usocp, acchdl) \
	(USOC_ACC_HANDLE_BAD((acchdl)) && USOC_REPORT_FAULT(usocp))

/* Check dma handle fault and report device faulty if fault occured */
#define	USOC_DMAHDL_FAULT(usocp, acchdl) \
	(USOC_DMA_HANDLE_BAD((acchdl)) && USOC_REPORT_FAULT(usocp))

#ifdef DEBUG
#define	USOC_DDH_ACCHDL_FAULT(usocp, acchdl, val) \
	(((usoc_dbg_drv_hdn == val) || USOC_ACC_HANDLE_BAD(acchdl)) && \
	    usoc_report_fault((usocp)))
#else
#define	USOC_DDH_ACCHDL_FAULT(usocp, acchdl, val) \
	USOC_ACCHDL_FAULT(usocp, acchdl)
#endif


#else /* USOC_DRV_HARDENING */

/*
 * Macros to read/write using acc handle and ddi_get/ddi_put routines
 */
#define	USOC_RD8(acchandle, addr)	(*(uint8_t *)(addr))
#define	USOC_RD16(acchandle, addr)	(*(uint16_t *)(addr))
#define	USOC_RD32(acchandle, addr)	(*(uint32_t *)(addr))
#define	USOC_RD64(acchandle, addr)	(*(uint64_t *)(addr))

#define	USOC_WR8(acchandle, addr, val)	(*(uint8_t *)(addr) = (uint8_t)(val))
#define	USOC_WR16(acchandle, addr, val)	(*(uint16_t *)(addr) = (uint16_t)(val))
#define	USOC_WR32(acchandle, addr, val)	(*(uint32_t *)(addr) = (uint32_t)(val))
#define	USOC_WR64(acchandle, addr, val)	(*(uint64_t *)(addr) = (uint64_t)(val))

#define	USOC_REP_RD(acchandle, hostaddr, devaddr, cnt)	\
	bcopy(devaddr, hostaddr, cnt)
#define	USOC_REP_RD32(acchandle, hostaddr, devaddr, cnt)	\
	usoc_wcopy((uint_t *)(devaddr), (uint_t *)(hostaddr), (cnt))

#define	USOC_REP_WR32(acchandle, hostaddr, devaddr, cnt)	\
	usoc_wcopy((uint_t *)(hostaddr), (uint_t *)(devaddr), (cnt))

/* Macros to sync kernel and I/O view of memory */
#define	USOC_SYNC_FOR_DEV(acchandle, offset, length)	\
	(void) ddi_dma_sync((acchandle), (offset),\
	    (length), DDI_DMA_SYNC_FORDEV)
#define	USOC_SYNC_FOR_KERNEL(acchandle, offset, length)	\
	(void) ddi_dma_sync((acchandle), (offset),\
	    (length), DDI_DMA_SYNC_FORKERNEL)

/* Macros to check device fault */
#define	USOC_GET_DEVSTATE(usocp)			(DDI_DEVSTATE_UP)
#define	USOC_DEVICE_BAD(usocp, devstate)		(0)
#define	USOC_DEVICE_STATE(devstate)	\
	(((devstate) == DDI_DEVSTATE_OFFLINE) ? "OFFLINE" : "DOWN")

/* Macros to check if acc/dma handles are marked faulty */
#define	USOC_ACC_HANDLE_BAD(acc_handle)			(0)
#define	USOC_DMA_HANDLE_BAD(dma_handle)			(0)
#define	USOC_DDH_DMA_HANDLE_BAD(dma_handle, val)	(0)

#define	USOC_REPORT_FAULT(usocp)			(1)
#define	USOC_DDI_REPORT_FAULT(a, b, c, d)

/* Check access handle fault and report device faulty if fault occured */
#define	USOC_ACCHDL_FAULT(usocp, acchdl) 		(0)

/* Check dma handle fault and report device faulty if fault occured */
#define	USOC_DMAHDL_FAULT(usocp, acchdl) 		(0)

#define	USOC_DDH_ACCHDL_FAULT(usocp, acchdl, val)	(0)

#endif /* USOC_DRV_HARDENING */

/*
 * Driver Entry points.
 */
static int usoc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int usoc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int usoc_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd,
	void *arg, void **result);
static unsigned int usoc_intr(caddr_t arg);
static int usoc_open(dev_t *devp, int flag, int otyp,
	cred_t *cred_p);
static int usoc_close(dev_t dev, int flag, int otyp,
	cred_t *cred_p);
static int usoc_ioctl(dev_t dev, int cmd, intptr_t arg,
	int mode, cred_t *cred_p, int *rval_p);

/*
 * FC_AL transport functions.
 */
static opaque_t usoc_bind_port(dev_info_t *dip, fc_fca_port_info_t *port_info,
	fc_fca_bind_info_t *bind_info);
static void usoc_unbind_port(opaque_t portp);
static int usoc_init_pkt(opaque_t portp, fc_packet_t *pkt, int sleep);
static int usoc_uninit_pkt(opaque_t portp, fc_packet_t *pkt);
static int usoc_get_cap(opaque_t portp, char *cap, void *ptr);
static int usoc_set_cap(opaque_t portp, char *cap, void *ptr);
static int usoc_transport(opaque_t portp, fc_packet_t *pkt);
static int usoc_getmap(opaque_t portp, fc_lilpmap_t *lilpmap);
static int usoc_reset(opaque_t portp, uint32_t cmd);
static int usoc_ub_alloc(opaque_t portp, uint64_t *tokens, uint32_t size,
	uint32_t *count, uint32_t type);
static int usoc_ub_free(opaque_t portp, uint32_t count, uint64_t token[]);
static int usoc_ub_release(opaque_t portp, uint32_t count, uint64_t token[]);
static int usoc_port_manage(opaque_t portp, fc_fca_pm_t *pm);
static int usoc_add_ubuf(usoc_port_t *portp, uint32_t poolid,
	uint64_t token[], uint32_t size, uint32_t *count);

static void usoc_handle_unsol_intr_new(usoc_state_t *usocp, usoc_kcq_t *kcq,
	volatile cqe_t *cqe, volatile cqe_t *cqe_cont);
static int usoc_data_out(usoc_port_t *port_statep,
	struct usoc_send_frame *sftp, uint32_t sz, usoc_unsol_buf_t *ubufp);
static void usoc_memset(caddr_t buf, uint32_t pat, uint32_t size);
static void usoc_reuse_packet(fc_packet_t *pkt);

/*
 * Driver internal functions.
 */
static int usoc_intr_solicited(usoc_state_t *, uint32_t srq);
static int usoc_intr_unsolicited(usoc_state_t *, uint32_t urq);
static void usoc_handle_unsol_intr(usoc_state_t *usocp, usoc_kcq_t *kcq,
    volatile cqe_t *cqe, volatile cqe_t *cqe_cont);
static void usoc_doneit(fc_packet_t *);
static void usoc_abort_done(fc_packet_t *);
static fc_packet_t *usoc_packet_alloc(usoc_port_t *, int);
static void usoc_packet_free(fc_packet_t *);
static void usoc_disable(usoc_state_t *usocp);
static int usoc_cqalloc_init(usoc_state_t *usocp, uint32_t index);
static void usoc_cqinit(usoc_state_t *usocp, uint32_t index);
static int usoc_start(usoc_state_t *usocp);
static void usoc_doreset(usoc_state_t *usocp);
static int usoc_dodetach(dev_info_t *dip);
static int usoc_diag_request(usoc_port_t *portp,
    uint_t *diagcode, uint32_t cmd);
static int usoc_download_ucode(usoc_state_t *usocp, uint_t fw_len);
static int usoc_init_cq_desc(usoc_state_t *usocp);
static int usoc_init_wwn(usoc_state_t *usocp);
static int usoc_enable(usoc_state_t *usocp);
static int usoc_establish_pool(usoc_state_t *usocp, uint32_t poolid);
static int usoc_add_pool_buffer(usoc_state_t *usocp, uint32_t poolid);
static fc_unsol_buf_t *usoc_ub_temp(usoc_port_t *, cqe_t *, caddr_t);
static int usoc_getmap2(usoc_port_t *port_statep, caddr_t arg, int flags);
static int usoc_getrls(usoc_port_t *port_statep, caddr_t arg, int flags);
static uint_t usoc_local_rls(usoc_port_t *port_statep, uint32_t bufid,
    uint_t polled);
static void usoc_flush_overflow_Q(usoc_state_t *usocp, int port, int qindex);
static void usoc_flush_all(usoc_state_t *usocp);
static uint32_t usoc_flush_timelist(usoc_state_t *usocp);
static void usoc_deferred_intr(void *arg);
static int usoc_force_reset(usoc_state_t *usocp, int restart, int flag);
static void usoc_take_core(usoc_state_t *usocp, int core_flags);
static void usoc_task_force_reload(void *arg);
static int usoc_bypass_dev(usoc_port_t *port_statep, uint_t dest);
static int usoc_force_lip(usoc_port_t *port_statep, uint_t polled);
static int usoc_force_offline(usoc_port_t *port_statep, uint_t polled);
static int usoc_reset_link(usoc_port_t *port_statep, int polled);
static int usoc_doit(usoc_port_t *port_statep, fc_packet_t *pkt, int polled,
    uint32_t endflag, uint32_t *diagcode);
static void usoc_pkt_timeout(fc_packet_t *pkt);
static void usoc_abort_timeout(fc_packet_t *pkt);
static int usoc_abort_cmd(opaque_t portp, fc_packet_t *pkt, int kmflags);
static int usoc_external_abort(opaque_t portp, fc_packet_t *pkt, int kmflags);
static int usoc_dump_xram_buf(void *arg);
static void usoc_update_pkt_state(fc_packet_t *pkt, uint_t status);
static void usoc_watchdog(void *arg);
static void usoc_untimeout(usoc_state_t *usocp, usoc_pkt_priv_t *priv);
static void usoc_timeout(usoc_state_t *usocp, void (*tfunc)(fc_packet_t *),
    fc_packet_t *pkt);
static void usoc_timeout_held(usoc_state_t *usocp,
    void (*tfunc) (fc_packet_t *), fc_packet_t *pkt);
static void usoc_check_overflow_Q(usoc_state_t *usoc, int qindex,
    uint32_t imr_mask);
static int usoc_remove_overflow_Q(usoc_state_t *usocp, fc_packet_t *pkt,
    int index);
static void usoc_add_overflow_Q(usoc_kcq_t *kcq, usoc_pkt_priv_t *fca_pkt);
static void usoc_monitor(void *arg);
static void usoc_flush_of_queues(usoc_port_t *port_statep);
static void usoc_finish_xfer(fc_packet_t *pkt);
static void usoc_reestablish_ubs(usoc_port_t *port_statep);
static int usoc_dosuspend(dev_info_t *dip);
static int usoc_doresume(dev_info_t *dip);
static int usoc_report_fault(usoc_state_t *usocp);
static uint32_t usoc_alloc_id(usoc_state_t *usocp, fc_packet_t *token);
static fc_packet_t *usoc_free_id(usoc_state_t *usocp, uint32_t id, int src);
static void usoc_init_ids(usoc_state_t *usocp);
static void usoc_free_id_for_pkt(usoc_state_t *usocp, fc_packet_t *pkt);
static void usoc_lfd_done(fc_packet_t *pkt);
static void usoc_perform_lfd(usoc_state_t *usocp);
static int usoc_finish_polled_cmd(usoc_state_t *usocp, fc_packet_t *pkt);

/*
 * SOC+ Circular Queue Management routines.
 */
static int usoc_cq_enque(usoc_state_t *usocp, usoc_port_t *port_statep,
    cqe_t *cqe, int rqix, fc_packet_t *pkt, fc_packet_t *token, int mtxheld);

/*
 * Utility functions
 */
static void usoc_updt_pkt_stat_for_devstate(fc_packet_t *pkt);
static void usoc_wcopy(uint_t *, uint_t *, int);
static void usoc_display(usoc_state_t *usocp, int port, int level, int dest,
    fc_packet_t *pkt, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_FIBRE_CHANNEL_FCA_USOCVAR_H */
