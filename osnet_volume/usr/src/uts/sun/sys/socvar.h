/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * Copyright (c) 1995 by Cray Research, Inc.
 * All rights reserved.
 *
 */

#ifndef _SYS_SOCVAR_H
#define	_SYS_SOCVAR_H

#pragma ident	"@(#)socvar.h	1.29	99/09/13 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/id32.h>

/*
 * socvar.h - SOC Driver data struct definitions
 */

/*
 * Define default name and # of SOCS to allocate to the system
 */

#define	SOC_NAME	"soc"		/* For identify routine */
#define	MAX_NSOC	21		/* Maximum number of controllers */

/*
 * Defines for the Circular Queues
 */
#define	SOC_MAX_REQ_CQ_ENTRIES	64	/* Maximum number of CQ entries. */
#define	SOC_MAX_RSP_CQ_ENTRIES	8
#define	SOC_CQ_SIZE (sizeof (cqe_t) * SOC_MAX_CQ_ENTRIES)

#define	SOC_SMALL_CQ_ENTRIES	4	/* Number of CQ entries for a small Q */

#define	N_CQS			2	/* Number of queues we use */
#define	HW_N_CQS		4	/* Number of queues the hardware has */
#define	SOC_CQ_ALIGN		64	/* alignment boundary */

/*
 * Misc. Macros
 */
#define	SOC_POOL_SIZE	2112
#define	SOC_SVC_LENGTH	64


#define	FABRIC_FLAG	1
#define	NPORT_FLAG	2

/*
 * SOC UNIX circular queue descriptor.
 */

typedef struct soc_kernel_cq {
	kmutex_t	skc_mtx;	/* MT lock for CQ manipulation  */
	kcondvar_t	skc_cv;		/* MT lock for CQ manipulation. */
	ddi_dma_handle_t skc_dhandle;	/* DDI DMA handle to CQ. */
	ddi_dma_cookie_t skc_dcookie;	/* DDI DMA Cookie. */
	soc_cq_t	*skc_xram_cqdesc; /* Pointer to XRAM CQ desc */
	caddr_t		skc_cq_raw;	/* Pointer to unaligned CQ mem pool */
	cqe_t		*skc_cq;	/* Pointer to CQ memory pool. */
	uchar_t		skc_in;		/* Current Input pointer. */
	uchar_t		skc_out;	/* Current Input pointer. */
	uchar_t		skc_last_index;	/* Last cq index. */
	uchar_t		skc_seqno;	/* Current Go Around in CQ. */
	uchar_t		skc_full;	/* Indication of full. */
	struct fc_packet_extended
			*skc_overflowh,	/* cq overflow list */
			*skc_overflowt;
} soc_kcq_t;

/*
 * Values for skc_full
 */
#define	SOC_SKC_FULL	1
#define	SOC_SKC_SLEEP	2

_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, soc_kernel_cq::skc_cv))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_kernel_cq::skc_dhandle))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_kernel_cq::skc_dcookie))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_kernel_cq::skc_xram_cqdesc))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_kernel_cq::skc_cq))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_kernel_cq::skc_cq_raw))
_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, soc_kernel_cq::skc_in))
_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, soc_kernel_cq::skc_out))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_kernel_cq::skc_last_index))
_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, soc_kernel_cq::skc_seqno))
_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, soc_kernel_cq::skc_full))
_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, soc_kernel_cq::skc_overflowh))
_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, soc_kernel_cq::skc_overflowt))

/*
 * State change callback routine descriptor
 *
 * There is one entry in this list for each hba that is attached
 * to this port.
 * This structure will need to be mutex protected when parallel
 * attaches are supported.
 */
typedef struct soc_statec_cb {
	struct soc_statec_cb	*next;
	void			(*callback)(void *, fc_statec_t);
	void			*arg;
} soc_statec_cb_t;

_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_statec_cb::callback))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_statec_cb::arg))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_statec_cb::next))

/*
 * SOC port status decriptor.
 */

typedef struct soc_port {
	uint_t	sp_status;		/* Status word. */

#define	PORT_FABRIC_PRESENT	1
#define	PORT_OFFLINE		2
#define	NPORT_LOGIN_SUCCESS	4
#define	PORT_LOGIN_ACTIVE	8
#define	PORT_LOGIN_RECOVERY	0x10
#define	PORT_STATUS_FLAG	0x80

	uint_t		sp_dst_id;	/* Nport ID of destination. */
	uint_t		sp_src_id;	/* Nport ID of host port. */
	uint_t		sp_port;	/* Soc physical port. (0 or 1) */
	la_wwn_t	sp_s_wwn;	/* Source World Wide Name. */
	la_wwn_t	sp_d_wwn;	/* Destination World Wide Name. */

	int	(*sp_callback)(void *);	/* Callback for async events. */
	void 	*sp_cb_arg;		/* Async Callback Arg. */

	struct soc_state	*sp_state;	/* Pointer the soc_state */

	/*
	 * Callback routines invoked for interface state changes
	 */
	soc_statec_cb_t		*state_cb;

	struct fc_packet_extended
				*sp_login,	/* used for login */
				*sp_offline;	/* used for error recovery */

	int	sp_login_retries;	/* retry count for login attempts */
	timeout_id_t	login_timeout_id;	/* used to time out logins */
	int		login_timer_running;

	kmutex_t	sp_mtx;		/* used for state changes, etc. */
	int		sp_child_state;	/* used to track initchild */
} soc_port_t;

/*
 * Values for sp_child_state
 */
#define	SOC_CHILD_UNINIT	0	/* no initchild done yet */
#define	SOC_CHILD_INIT		1	/* successful initchild */

/*
 * Polling timeout values
 */
#define	SOC_LOGIN_TIMEOUT_FLAG	0x80000000
#define	SOC_LOGIN_TIMEOUT	10	/* seconds until timeout */
#define	SOC_LOGIN_RETRIES	4	/* number of times we'll try to login */
#define	SOC_OFFLINE_TIMEOUT_FLAG	0x40000000
#define	SOC_OFFLINE_TIMEOUT	5	/* seconds until timeout */
#define	SOC_ONLINE_TIMEOUT	5

#ifdef	NOT_USED
#define	SOC_GRACE		60	/* Timeout margin (sec.) */
#define	SOC_TIMEOUT_DELAY(secs, delay)  (secs * (10000000 / delay))
#endif	NOT_USED
#define	SOC_GRACE		10	/* Timeout margin (sec.) */

#define	SOC_TIMEOUT_DELAY(secs, delay)  (secs * (1000000 / delay))
#define	SOC_NOINTR_POLL_DELAY_TIME	1000    /* usec */

/*
 * We wait for up to SOC_INITIAL_ONLINE seconds for the first
 * soc to come on line. The timeout in the soc firmware is 10 seconds.
 * The timeout is to let any outstanding commands drain before
 * coming back on line, after going off-line.
 */
#define	SOC_INITIAL_ONLINE	10	/* secs for first online from soc */


_NOTE(MUTEX_PROTECTS_DATA(soc_port::sp_mtx, soc_port::sp_status))
_NOTE(DATA_READABLE_WITHOUT_LOCK(soc_port::sp_status))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_port::sp_dst_id))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_port::sp_src_id))
_NOTE(SCHEME_PROTECTS_DATA("write only at attach", soc_port::sp_port))
_NOTE(SCHEME_PROTECTS_DATA("write only at attach", soc_port::sp_state))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_port::sp_s_wwn))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_port::sp_d_wwn))

_NOTE(SCHEME_PROTECTS_DATA("write only at attach", soc_port::sp_login))
_NOTE(SCHEME_PROTECTS_DATA("write only at attach", soc_port::sp_offline))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_port::sp_login_retries))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_port::login_timeout_id))
_NOTE(MUTEX_PROTECTS_DATA(soc_port::sp_mtx, soc_port::login_timer_running))

_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_port::state_cb))


/*
 * FC pkt cache structure and macros definitions.
 */

/*
 * The fc_packet must be the first element in this structure
 */
typedef struct fc_packet_extended {
	fc_packet_t			fpe_pkt;
	struct fc_packet_extended 	*fpe_next;
	cqe_t				fpe_cqe;
	fc_frame_header_t		fpe_resp_hdr;
	uint32_t			cmd_state;
	uint32_t			fpe_magic;
	uint32_t			fpe_id;
} fc_pkt_extended_t;

/* Macros to speed handling of 32-bit IDs */
#ifdef _LP64
#define	SOC_GET_ID(x)		(uint32_t)id32_alloc((void *)(x), KM_SLEEP)
#define	SOC_LOOKUP_ID(x)	(struct fc_packet_extended *)id32_lookup((x))
#define	SOC_FREE_ID(x)		id32_free((x))
#else
#define	SOC_GET_ID(x)		((uint32_t)(x))
#define	SOC_LOOKUP_ID(x)	((struct fc_packet_extended *)(x))
#define	SOC_FREE_ID(x)
#endif

_NOTE(SCHEME_PROTECTS_DATA("safe sharing", fc_packet_extended::cmd_state))
_NOTE(SCHEME_PROTECTS_DATA("unshared", fc_packet_extended::fpe_cqe))
_NOTE(SCHEME_PROTECTS_DATA("unshared", fc_packet_extended::fpe_resp_hdr))
_NOTE(SCHEME_PROTECTS_DATA("unshared", fc_packet_extended::fpe_magic))
_NOTE(MUTEX_PROTECTS_DATA(soc_kernel_cq::skc_mtx, fc_packet_extended::fpe_next))

/*
 * definitions for the cmd_state
 */
#define	FC_CMD_COMPLETE		0x4	/* command complete */
#define	FC_CMPLT_CALLED		0x10	/* Completion routine called */

/*
 * Magic number used to validate an extended packet
 */
#define	FPE_MAGIC		0x0489256a

#define	FC_PKT_CACHE_FIRST(cache)	((cache)->fpc_first)
#define	FC_PKT_CACHE_LAST(cache)	((cache)->fpc_last)
#define	FC_PKT_CACHE_MUTEX(cache)	(&(cache)->fpc_mutex)
#define	FC_PKT_NEXT(pkt)		((pkt)->fpe_next)

#define	FC_PKT_CACHE_INCREMENT		0x20

typedef struct fc_packet_cache {
	fc_pkt_extended_t	*fpc_first;
	fc_pkt_extended_t	*fpc_last;
	kmutex_t		fpc_mutex;
} fc_pkt_cache_t;

_NOTE(MUTEX_PROTECTS_DATA(fc_packet_cache::fpc_mutex,
	fc_packet_cache::fpc_first))
_NOTE(MUTEX_PROTECTS_DATA(fc_packet_cache::fpc_mutex,
	fc_packet_cache::fpc_last))

/*
 * The fc_cache_list is utilized to keep track of all of the
 * fc_packet_extended_t cache units allocated by soc_pkt_cache_alloc().
 * This allows us to free up all allocated units in soc_detach().
 */
typedef struct fc_cache_list {
	fc_pkt_extended_t	*fc_pktcache_location;
	struct fc_cache_list	*next_fc_cache_list;
} fc_cache_list_t;

/*
 * Length of the string used to hold the pathname to this device
 */
#define	SOC_PATH_NAME_LEN	256

/*
 * SOC state structure
 */

typedef struct soc_state {
	dev_info_t	*dip;		/* Link to kernel kept info for */
					/* this dev */
	caddr_t		soc_eepromp;	/* Pointer to SOC EEPROM. */
	caddr_t		socxrp;		/* Pointer to SOC XRAM. */
	soc_reg_t	*socrp;		/* Pointer to Soc registers. */

	soc_cq_t	*xram_reqp;	/* Pointer to SOC XRAM REQ DESC. */
	soc_cq_t	*xram_rspp;	/* Pointer to SOC XRAM RSP DESC. */

	soc_kcq_t	request[N_CQS];	/* Request Queue 0 data structure. */
	soc_kcq_t	response[N_CQS]; /* Response Queues */

	uint_t		busy;		/* Non-zero when SOC is doing work. */
	uint_t		cfg;		/* Driver copy of config register. */

	kmutex_t	k_imr_mtx;	/* mutex for interrupt masks */
	uint_t		k_soc_imr;	/* copy of soc's mask register */

	fc_transport_t	xport[N_SOC_NPORTS];

	/*
	 * The FC packet cache is a cache of fc packets with the associated
	 * memory for soc request packets. Initially, the cache is 512 entries.
	 * The cache will grow as resource demands increase for this SOC.
	 * However, in the future, the cache should shrink as load decreases.
	 */
	fc_pkt_cache_t	fc_pkt_cache;

	fc_pkt_extended_t	*fc_pkt_hi,	/* used for resp validation */
				*fc_pkt_lo;

	fc_cache_list_t		*fc_cache_locations;

	ddi_iblock_cookie_t	iblkc;
	ddi_idevice_cookie_t	idevc;

	uchar_t	soc_service_params[SOC_SVC_LENGTH];	/* Service params. */
	la_wwn_t	soc_ww_name;		/* SOC World wide name */

	uchar_t			*pool;	/* Scratch pool for soc. */
	ddi_dma_handle_t	pool_dhandle;
	ddi_dma_cookie_t	pool_dcookie;

	/*
	 * Flag saying SOC shutdown because of serious problem
	 * like it went off-line and is being re-initialized
	 * so don't accept commands from children
	 */
	uchar_t			soc_shutdown;
	uchar_t			soc_reset;
	int			port_mtx_init;
	soc_port_t		port_status[N_SOC_NPORTS]; /* Status of soc. */

	/*
	 * The value of soc_init_time when the host adapter is initialized.
	 * This is used at attach/initchild time to determine how
	 * long to wait before giving up on the ports coming online.
	 */
	ulong_t			init_time;
	/*
	 * reset_time is set when the SOC is reset to determine
	 * how much time to wait at attach/init
	 */
	ulong_t			reset_time;

	/*
	 * The full name of this instance is stored here
	 */
	char			soc_name[SOC_PATH_NAME_LEN];

} soc_state_t;

_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::dip))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::soc_eepromp))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::socxrp))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::socrp))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::xram_reqp))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::xram_rspp))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::busy))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::cfg))
_NOTE(MUTEX_PROTECTS_DATA(soc_state::k_imr_mtx, soc_state::k_soc_imr))
_NOTE(DATA_READABLE_WITHOUT_LOCK(soc_state::k_soc_imr))
_NOTE(MUTEX_PROTECTS_DATA(fc_packet_cache::fpc_mutex, soc_state::fc_pkt_hi))
_NOTE(MUTEX_PROTECTS_DATA(fc_packet_cache::fpc_mutex, soc_state::fc_pkt_lo))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::iblkc))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::idevc))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_state::soc_service_params))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_state::soc_ww_name))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::port_mtx_init))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_state::port_status.sp_dst_id))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_state::port_status.sp_src_id))
_NOTE(MUTEX_PROTECTS_DATA(soc_state::port_status.sp_mtx,
	soc_state::port_status.sp_status))
_NOTE(DATA_READABLE_WITHOUT_LOCK(soc_state::port_status.sp_status))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_state::init_time))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", soc_state::soc_name))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_state::request.skc_cq))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_state::request.skc_cq_raw))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_state::response.skc_cq))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_state::response.skc_cq_raw))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_state::soc_shutdown))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", soc_state::soc_reset))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", scsi_device))

/*
 * Structure used when the soc driver needs to issue commands of its own
 */
typedef struct soc_priv_cmd {
	fc_packet_t		*fpktp;
	int			flags;
	caddr_t			cmd;
	caddr_t			resp;
	ddi_dma_handle_t	cmd_handle;
	ddi_dma_handle_t	resp_handle;
} soc_priv_cmd_t;

_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_priv_cmd::fpktp))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_priv_cmd::flags))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_priv_cmd::cmd))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_priv_cmd::resp))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_priv_cmd::cmd_handle))
_NOTE(SCHEME_PROTECTS_DATA("unshared", soc_priv_cmd::resp_handle))

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_SOCVAR_H */
