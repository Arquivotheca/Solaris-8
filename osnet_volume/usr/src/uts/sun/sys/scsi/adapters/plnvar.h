/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Copyright (c) 1995 by Cray Research, Inc.
 */

#ifndef _SYS_SCSI_ADAPTERS_PLNVAR_H
#define	_SYS_SCSI_ADAPTERS_PLNVAR_H

#pragma ident	"@(#)plnvar.h	1.29	98/01/25 SMI"

/*
 * Pluto (Sparc Storage Array) host adapter driver definitions
 */

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Compile options to turn on debugging code
 */
#ifdef	DEBUG
#define	PLNDEBUG
#define	PLNLOGGING
#endif	/* DEBUG */

/*
 * Debug logging options
 */
#ifdef	PLNLOGGING
#define	PLNLOG_NMSGS		128
#define	PLNLOG_MSGLEN		128
#endif	/* PLNLOGGING */

/*
 * Parameters for internally generated commands
 */
#define	PLN_INTERNAL_CMD_TIMEOUT	10	/* seconds */

/*
 * Turn a Group0 Mode Sense/Select into a Group1
 */
#define	SCMD_MS_GROUP1		0x40

/*
 * Undefined capabilities
 */
#define	CAP_UNDEFINED		(-1)

/*
 * Max size of the response payload data
 */
#define	PLN_RSP_DATA_SIZE	64

/*
 * Response payload packet
 */
typedef struct pln_rsp {
	union {
	    struct fcp_rsp	rsp_payload;
	    uchar_t		rsp_data[PLN_RSP_DATA_SIZE];
	} rp;
} pln_rsp_t;

/*
 * Structures used for managing fcp command and response pools
 */

/*
 * This structure is overlaid on the front end of a fcp command
 * when it is in the free list
 */
typedef struct	pln_cr_free_elem {
	struct pln_cr_free_elem	*next;		/* list of free elements */
	caddr_t			rsp;		/* ptr to corresponding rsp */
	uint_t			cmd_dmac;	/* dmac_address for cmd */
	uint_t			rsp_dmac;	/* dmac_address for rsp */
} pln_cr_free_t;

/*
 * A pool descriptor
 */
typedef struct	pln_cr_pool {
	pln_cr_free_t		*free;		/* list of free cmds */
	caddr_t			cmd_base;	/* start addr of this chunk */
	ddi_dma_handle_t	cmd_handle;	/* dma mapping for this chunk */
	caddr_t			rsp_base;
	ddi_dma_handle_t	rsp_handle;
	struct pln_fc_pkt	*waiters_head,	/* queue waiting for cmd/rsp */
				*waiters_tail;
} pln_cr_pool_t;

#define	PLN_CR_POOL_DEPTH	256	/* # of elem in per-pln pool, default */
#define	PLN_TGT_PRIV_LEN	(2*sizeof (void*))
					/* # of bytes in tgt private, default */

/*
 * pln scsi command structure
 * This takes the place of the scsi_disk structure which
 * was in sys/scsi/impl/pkt_wrapper.h
 * which went away in 494.
 */
struct pln_scsi_cmd {
	/*
	 * Note: scsi_pkt must be first.
	 */
	struct scsi_pkt	cmd_pkt;	/* the generic packet itself */
	caddr_t		cmd_fc_pkt;	/* back pointer to our fc packet. */
	uchar_t		cmd_cdblen;	/* SCSI command length */
	uchar_t		cmd_senselen;	/* SCSI Sense length */
	uchar_t		cmd_tgtlen;	/* target private len */
	uint32_t	cmd_flags;	/* flags */
	uint32_t	cmd_dmacount;	/* Transfer length */
	ddi_dma_handle_t	cmd_dmahandle;	/* dma handle */
	ddi_dma_cookie_t	cmd_dmacookie;	/* dma cookie */
	union scsi_cdb		cmd_cdb_un;	/* 'generic' Sun cdb */
	char	cmd_scsi_scb[sizeof (struct scsi_arq_status)];	/* status */
	char	cmd_tgtprivate[PLN_TGT_PRIV_LEN];
	struct pln_scsi_cmd	*cmd_next;	/* ptr for callback list */
};

/*
 * cmd_flags definitions
 */
#define	P_CFLAG_DMAVALID	0x0001	/* dma mapping valid */
#define	P_CFLAG_DMAWRITE	0x0002	/* data is going 'out' */
#define	P_CFLAG_CONSISTENT	0x0004	/* buffer is consistant */
#define	P_CFLAG_CDBEXTERN	0x0008	/* external cdb allocated */
#define	P_CFLAG_SCBEXTERN	0x0010	/* external status blk allocated */
#define	P_CFLAG_TGTEXTERN	0x0020	/* external tgt allocated */
#define	P_CFLAG_FREE		0x0040	/* packet is on free list */
#define	P_CFLAG_EXTCMDS_ALLOC	0x0080	/* pkt has EXTCMDS_SIZE and */
					/* been fast alloc'ed */
/*
 * fc_transport fc_packet allocation structure
 */
typedef struct pln_fc_pkt {
	struct pln		*fp_pln;
	struct pln_disk		*fp_pd;
	struct fc_transport	*fp_fc;
	fc_packet_t		*fp_pkt;
	volatile int		fp_state;
	ulong_t			fp_timeout;
	volatile int		fp_timeout_flag;
	int			fp_retry_cnt;
	struct fcp_cmd		*fp_cmd;
	struct pln_rsp		*fp_rsp;
	fc_dataseg_t		fp_cmdseg;
	fc_dataseg_t		fp_rspseg;
	fc_dataseg_t		fp_dataseg;
	fc_dataseg_t		*fp_datasegs[2];
	struct pln_fc_pkt	*fp_next,
				*fp_prev;
	void			(*fp_cr_callback)(struct pln_fc_pkt *);
	struct pln_fc_pkt	*fp_cr_next;
	struct pln_fc_pkt	*fp_onhold;
	struct	pln_scsi_cmd	fp_scsi_cmd;
} pln_fc_pkt_t;

/*
 * Values for fp_state
 */
#define	FP_STATE_FREE		0	/* on free list, available */
#define	FP_STATE_IDLE		1	/* on active list, not issued to hw */
#define	FP_STATE_ISSUED		2	/* on active list, in hw */
#define	FP_STATE_ONHOLD		3	/* on active list, throttling */
#define	FP_STATE_NOTIMEOUT	4	/* on active list, issued to hw, */
					/* but not eligible for cmd timeout */
#define	FP_STATE_OFFLINE	5	/* on active list, offline timeout */
#define	FP_STATE_PRETRY		6	/* polled mode, cmd requires a retry */
#define	FP_STATE_PTHROT		7	/* polled mode, cmd throttling resp */
#define	FP_STATE_TIMED_OUT	8	/* transport timed out on this pkt */

/*
 * A padding we add to the command timeout value
 */
#define	PLN_TIMEOUT_PAD		10

/*
 * The number of retries we'll do for link errors
 */
#define	PLN_NRETRIES		5

/*
 * The delay time (in usecs) that we'll wait between iterations
 * in polled mode
 */
#define	PLN_POLL_DELAY		10000

/*
 * State information for each pluto disk
 */
struct pln_disk {
	int			pd_flags;
	kmutex_t		pd_pkt_inuse_mutex;
	pln_fc_pkt_t		*pd_pkt_pool;
	pln_fc_pkt_t		*pd_inuse_head,
				*pd_inuse_tail;
	pln_fc_pkt_t		*pd_onhold_head;
	volatile int		pd_onhold_flag;
	int			pd_resource_cb_id;
	struct pln_disk		*pd_next;
	dev_info_t		*pd_dip;
};

/*
 * pd_flags
 */
#define	PD_SPUN_DOWN		0x01		/* drive is spun down */
#define	PD_CANNOT_READ		0x02		/* cannot read drive */
#define	PD_NOT_READY		0x04		/* drive not ready */
#define	PD_NO_SELECT		0x08		/* no drive select */
#define	PD_NO_DRIVE		0x10		/* no drive present */
#define	PD_ERRED_OUT		0x20		/* drive is offline */

/*
 * ses card id
 */
#define	PLN_SES_TARGET_ID		15

/*
 * tmp - until defined in <sunddi.h>
 */
#define	DDI_NT_BLOCK_ARRAY_SINGLE	"ddi_block:array:single"
#define	DDI_NT_BLOCK_ARRAY_GROUPED	"ddi_block:array:grouped"
#define	DDI_NT_BLOCK_ARRAY_COMPONENT	"ddi_block:array:component"
#define	DDI_NT_BLOCK_ARRAY_NOBODY_HOME	"ddi_block:array:nobody_home"


/*
 * HBA interface macros
 */
#define	SDEV2TRAN(sd)		((sd)->sd_address.a_hba_tran)
#define	SDEV2ADDR(sd)		(&((sd)->sd_address))
#define	PKT2TRAN(pkt)		((pkt)->pkt_address.a_hba_tran)

#define	TRAN2PLN(tran)		((struct pln *)(tran)->tran_hba_private)
#define	SDEV2PLN(sd)		(TRAN2PLN(SDEV2TRAN(sd)))
#define	PKT2PLN(pkt)		(TRAN2PLN(PKT2TRAN(pkt)))
#define	ADDR2PLN(ap)		(TRAN2PLN((ap)->a_hba_tran))

#define	PKT2DIP(pkt)		(PKT2PLN(pkt)->pln_dip)

/*
 * Configuration information for this host adapter
 */
struct pln {
	/*
	 * Transport structure for this instance of the hba
	 */
	scsi_hba_tran_t		*pln_tran;

	/*
	* dev_info_t reference
	*/
	dev_info_t		*pln_dip;

	/*
	* mutex
	*/
	kmutex_t		pln_mutex;

	/*
	 * The transport data we get from our parent (soc)
	 */
	struct fc_transport	*pln_fc_tran;

	/*
	 * iblock cookie from soc for initializing mutexes
	 */
#define	pln_iblock		pln_fc_tran->fc_iblock

	/*
	 * Next is a linked list of host adapters
	 */
	struct pln		*pln_next;

	/*
	 * Configuration information gleaned from pluto
	 * mode sense pages describing the device.
	 * nports is the number of internal scsi busses,
	 * and ntargets is the number of targets per bus.
	 */
	ushort_t		pln_nports;
	ushort_t		pln_ntargets;

	/*
	 * state info for individual disks
	 */
	struct pln_disk		**pln_ids;

	/*
	 * A linked list of all pln_disk structures, including the pln_ctlr
	 */
	struct pln_disk		*pln_disk_list;

	/*
	 * a flag indicating whether disk mutexes have been initialized
	 */
	int			pln_disk_mtx_init;

	/*
	 * state info for the pluto device itself
	 */
	struct pln_disk		*pln_ctlr;
	struct pln_address	pln_ctlr_addr;
	struct scsi_address	pln_scsi_addr;

	/*
	 * The pools used for fcp commands and responses
	 */
	struct pln_cr_pool	pln_cmd_pool;
	kmutex_t		pln_cr_mutex;

	/*
	 * Cookies for unsolicited routines we'll use in case
	 * we're unloaded
	 */
	fc_uc_cookie_t		pln_uc_cookie;
	fc_statec_cookie_t	pln_statec_cookie;

	/*
	 * Instrumentation
	 */
	struct pln_disk	*cur_throttle;
	kmutex_t	pln_throttle_mtx;
	int		pln_ncmd_ref;		/* varies with # of commands */
	int		pln_maxcmds;		/* max commands we can issue */
	volatile int	pln_throttle_flag;	/* this pln has "onhold" cmds */
	int		pln_throttle_cnt;	/* # of "throttle" responses */

	/*
	 * The state of the pln device
	 */
	volatile uint_t		pln_state;
	volatile int		pln_ref_cnt;
	kmutex_t		pln_state_mutex;

	/*
	 * Used when we're performing timeout recovery
	 */
	int			pln_timer;
	int			pln_timeout_count;
	pln_fc_pkt_t		*pkt_offline,
				*pkt_reset;
	void			*pln_kmem_cache;
	int			pln_en_online_timeout;

	/*
	 * Per target scsi options. No need to define per target
	 * scsi options for a pluto since all disks are similar.
	 */
	int			pln_scsi_options;

	/*
	 * Environmental Sense Card (SES) scsi ID and scsi_options
	 */

	short			pln_ses_id;
	int			pln_ses_scsi_options;
};

/*
 * Values for pln_state
 */
#define	PLN_STATE_UNINIT	0
#define	PLN_STATE_ONLINE	1
#define	PLN_STATE_OFFLINE_RSP	2
#define	PLN_STATE_OFFLINE	4
#define	PLN_STATE_RESET		8
#define	PLN_STATE_TIMEOUT	0x10
#define	PLN_STATE_DO_OFFLINE	0x20
#define	PLN_STATE_DO_RESET	0x40
#define	PLN_STATE_LINK_DOWN	0x80
#define	PLN_STATE_SUSPENDED	0x100
#define	PLN_STATE_SUSPENDING	0x200

/*
 * Throttling
 */

/*
 * Maximum number of commands we can feed one pluto at a time
 */
#define	PLN_MAX_CMDS		254

/*
 * The amount we'll increment the throttle each second if there are no
 * rejections in the last second.
 */
#define	PLN_THROTTLE_SWING	4

/*
 * The number of commands to try after throttling up
 */
#define	PLN_THROTTLE_UP_CNT	2

/*
 * The number of commands we let finish before we call pln_throttle_start to
 * start the commands in the throttle queue.
 */
#define	PLN_THROTTLE_START	8

/*
 * A mask used to limit the frequency with which we check timeouts.
 * We'll only run through the timeout lists when the (per second)
 * counter, ANDed with this quantity, is zero.
 */
#define	PLN_TIME_CHECK_MSK	3

/*
 * The number of clock ticks we'll wait after detecting an individual
 * command timeout before trying error recovery
 */
#define	PLN_TIMEOUT_RECOVERY	4

/*
 * The number of clock ticks we'll wait for an offline response after
 * asking for an offline to occur
 */
#define	PLN_OFFLINE_TIMEOUT	5

/*
 * The length of time we'll wait for an online to occur after an
 * offline
 */
#define	PLN_ONLINE_TIMEOUT	60

/*
 * The concept of initiator id does not apply to us.
 * In case anyone asks, this is a convenient fiction.
 * Used to initialize a global, so it can be changed
 * via /etc/system.
 */
#define	PLN_INITIATOR_ID	(7)


/*
 * Warlock directives
 * _NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_tran))
 */
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_dip))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_tran))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_fc_tran))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_nports))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ntargets))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_fc_tran))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ids))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_disk_list))

_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_disk_mtx_init))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_nports))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ctlr))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ctlr_addr))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ntargets))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_scsi_addr))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ids))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_pool::free))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_pool::waiters_head))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_disk_list))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_pool::waiters_tail))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::cmd_base))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_disk_mtx_init))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::cmd_handle))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::rsp_base))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ctlr))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::rsp_handle))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_ctlr_addr))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_uc_cookie))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_statec_cookie))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_scsi_addr))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_ncmd_ref))

_NOTE(MUTEX_PROTECTS_DATA(pln::pln_throttle_mtx, pln::pln_maxcmds))
_NOTE(DATA_READABLE_WITHOUT_LOCK(pln::pln_maxcmds))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_pool::free))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_throttle_cnt))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_pool::waiters_head))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_throttle_flag))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_pool::waiters_tail))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_throttle_mtx, pln::cur_throttle))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::cmd_base))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_state_mutex, pln::pln_state))
_NOTE(DATA_READABLE_WITHOUT_LOCK(pln::pln_state))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::cmd_handle))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::rsp_base))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_cr_pool::rsp_handle))

_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_uc_cookie))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pln_statec_cookie))
_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_pln))

_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_pd))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_ncmd_ref))
_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_pkt))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_timeout))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_timeout_flag))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_throttle_cnt))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_fc_pkt::fp_retry_cnt))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_fc_pkt::fp_cmd))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_fc_pkt::fp_rsp))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_cmdseg))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln::pln_throttle_flag))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_rspseg))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_dataseg))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_throttle_mtx, pln::cur_throttle))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_datasegs))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_fc_pkt::fp_cr_next))

_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_scsi_cmd))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_cr_callback))
_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_fc))

_NOTE(MUTEX_PROTECTS_DATA(pln::pln_throttle_mtx, pln_fc_pkt::fp_onhold))
_NOTE(SCHEME_PROTECTS_DATA("pln_state controls access", pln::pln_timer))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_state_mutex, pln::pln_timeout_count))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pkt_offline))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln::pkt_reset))
_NOTE(SCHEME_PROTECTS_DATA("stable data", pln::pln_en_online_timeout))

/*
 * Warlock directives for pln_fc_pkt
 */
_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_pln))
_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_pd))
_NOTE(SCHEME_PROTECTS_DATA("write only at pkt init", pln_fc_pkt::fp_pkt))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_free_elem::next))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_timeout))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_state))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_cr_free_elem::rsp))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_cr_free_elem::cmd_dmac))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_timeout_flag))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_cr_free_elem::rsp_dmac))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_fc_pkt::fp_retry_cnt))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_fc_pkt::fp_cmd))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_fc_pkt::fp_rsp))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_cmdseg))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_rspseg))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_dataseg))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_fc_pkt::fp_datasegs))
_NOTE(MUTEX_PROTECTS_DATA(pln_disk::pd_pkt_inuse_mutex, pln_fc_pkt::fp_next))
_NOTE(MUTEX_PROTECTS_DATA(pln_disk::pd_pkt_inuse_mutex, pln_fc_pkt::fp_prev))

/*
 * Warlock directives for pln_disk
 */
_NOTE(MUTEX_PROTECTS_DATA(pln_disk::pd_pkt_inuse_mutex, pln_disk::pd_pkt_pool))
_NOTE(MUTEX_PROTECTS_DATA(pln_disk::pd_pkt_inuse_mutex, pln_disk::pd_flags))
_NOTE(MUTEX_PROTECTS_DATA(pln_disk::pd_pkt_inuse_mutex,
		pln_disk::pd_inuse_head))
_NOTE(MUTEX_PROTECTS_DATA(pln_disk::pd_pkt_inuse_mutex,
		pln_disk::pd_inuse_tail))
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_throttle_mtx, pln_disk::pd_onhold_head))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_disk::pd_onhold_flag))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_disk::pd_resource_cb_id))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_disk::pd_next))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_disk::pd_dip))

/*
 * Warlock directives for pln_cr_free_elem
 */
_NOTE(MUTEX_PROTECTS_DATA(pln::pln_cr_mutex, pln_cr_free_elem::next))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_cr_free_elem::rsp))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_cr_free_elem::cmd_dmac))
_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_cr_free_elem::rsp_dmac))

#if	defined(_KERNEL) || defined(_KMEMUSER)
#ifdef	PLNDEBUG
/*
 * Macros for flexible debugging and logging of various portions
 * of the driver.  When compiled with debugging, the driver can
 * direct msgs with debugging information to either the console,
 * a circular queue of msgs, or both.
 */

#define	P_1_FLAG	0x80000000		/* Level 1 debug statements */
#define	P_E_FLAG	0x00000001		/* all error conditions */
#define	P_PA_FLAG	0x00000002		/* probe/attach */
#define	P_D_FLAG	0x00000004		/* detach */
#define	P_B_FLAG	0x00000008		/* bus_ops calls */
#define	P_I_FLAG	0x00000010		/* initchild calls */
#define	P_C_FLAG	0x00000020		/* capability operations */
#define	P_T_FLAG	0x00000040		/* transport requests */
#define	P_A_FLAG	0x00000080		/* abort requests */
#define	P_R_FLAG	0x00000100		/* reset requests */
#define	P_W_FLAG	0x00000200		/* watch handlers */
#define	P_S_FLAG	0x00000400		/* scsi cdb, xfer len/dir */
#define	P_CNF_FLAG	0x00000800		/* disk configuration */
#define	P_RA_FLAG	0x00001000		/* resource allocation */
#define	P_RD_FLAG	0x00002000		/* resource deallocation */
#define	P_PC_FLAG	0x00004000		/* private cmds */
#define	P_X_FLAG	0x00008000		/* command execution */
#define	P_PR_FLAG	0x00010000		/* priority reserve */
#define	P_UC_FLAG	0x00020000		/* unsolicited cmds */
#define	P_PS_FLAG	0x00040000		/* dump pluto mode sense */
						/* state info */
#define	P_FC_FLAG	0x00400000		/* fibre ch packets */
#define	P_PKT_FLAG	0x00800000		/* packet alloc/dealloc */

#define	PLN_PRINTF(flag, args)						\
		if (plnflags & (flag))					\
			pln_printf args


#define	P_1_PRINTF(args)	PLN_PRINTF(P_1_FLAG, args)
#define	P_E_PRINTF(args)	PLN_PRINTF(P_E_FLAG, args)
#define	P_PA_PRINTF(args)	PLN_PRINTF(P_PA_FLAG, args)
#define	P_D_PRINTF(args)	PLN_PRINTF(P_D_FLAG, args)
#define	P_B_PRINTF(args)	PLN_PRINTF(P_B_FLAG, args)
#define	P_I_PRINTF(args)	PLN_PRINTF(P_I_FLAG, args)
#define	P_C_PRINTF(args)	PLN_PRINTF(P_C_FLAG, args)
#define	P_T_PRINTF(args)	PLN_PRINTF(P_T_FLAG, args)
#define	P_A_PRINTF(args)	PLN_PRINTF(P_A_FLAG, args)
#define	P_R_PRINTF(args)	PLN_PRINTF(P_R_FLAG, args)
#define	P_W_PRINTF(args)	PLN_PRINTF(P_W_FLAG, args)
#define	P_S_PRINTF(args)	PLN_PRINTF(P_S_FLAG, args)
#define	P_CNF_PRINTF(args)	PLN_PRINTF(P_CNF_FLAG, args)
#define	P_RA_PRINTF(args)	PLN_PRINTF(P_RA_FLAG, args)
#define	P_RD_PRINTF(args)	PLN_PRINTF(P_RD_FLAG, args)
#define	P_PC_PRINTF(args)	PLN_PRINTF(P_PC_FLAG, args)
#define	P_X_PRINTF(args)	PLN_PRINTF(P_X_FLAG, args)
#define	P_PR_PRINTF(args)	PLN_PRINTF(P_PR_FLAG, args)
#define	P_PS_PRINTF(args)	PLN_PRINTF(P_PS_FLAG, args)
#define	P_UC_PRINTF(args)	PLN_PRINTF(P_UC_FLAG, args)
#define	P_FC_PRINTF(args)	PLN_PRINTF(P_FC_FLAG, args)
#define	P_PKT_PRINTF(args)	PLN_PRINTF(P_PKT_FLAG, args)

#else

#define	P_1_PRINTF(args)
#define	P_E_PRINTF(args)
#define	P_PA_PRINTF(args)
#define	P_D_PRINTF(args)
#define	P_B_PRINTF(args)
#define	P_I_PRINTF(args)
#define	P_C_PRINTF(args)
#define	P_T_PRINTF(args)
#define	P_A_PRINTF(args)
#define	P_R_PRINTF(args)
#define	P_W_PRINTF(args)
#define	P_S_PRINTF(args)
#define	P_CNF_PRINTF(args)
#define	P_RA_PRINTF(args)
#define	P_RD_PRINTF(args)
#define	P_PC_PRINTF(args)
#define	P_X_PRINTF(args)
#define	P_PR_PRINTF(args)
#define	P_PS_PRINTF(args)
#define	P_UC_PRINTF(args)
#define	P_FC_PRINTF(args)
#define	P_PKT_PRINTF(args)

#endif	/* PLNDEBUG */
#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */



#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_ADAPTERS_PLNVAR_H */
