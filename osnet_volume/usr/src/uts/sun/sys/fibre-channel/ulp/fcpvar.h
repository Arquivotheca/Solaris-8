/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_ULP_FCPVAR_H
#define	_SYS_FIBRE_CHANNEL_ULP_FCPVAR_H

#pragma ident	"@(#)fcpvar.h	1.3	99/10/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * Stuff to be defined in fc_ulpif.h FIXIT
 */
#define	PORT_DEVICE_CREATE	0x40

#define	SCMD_REPORT_LUN		0xa0	/* SCSI cmd to report on LUNs */

#define	FC4_SCSI_FCP		0x08	/* our (SCSI) FC4 type number */

#define	SSFCP_QUEUE_DELAY	(4)
#define	SSFCP_FAILED_DELAY	20
#define	SSFCP_RESET_DELAY	3	/* target reset dealy of 3 secs */

/*
 * Highest possible timeout value to indicate
 * the watch thread to return the I/O
 */
#define	SSFCP_INVALID_TIMEOUT	(0xFFFFFFFF)

/*
 * Events supported by soc+ HBA driver
 */
#define	FCAL_INSERT_EVENT	"SUNW,sf:DEVICE-INSERTION.1"
#define	FCAL_REMOVE_EVENT	"SUNW,sf:DEVICE-REMOVAL.1"

/*
 * All the stuff above needs to move intp appropriate header files.
 */
/*
 * Per port struct ssfcp_port
 */

#define	SSFCP_NUM_HASH			128

#define	SSFCP_HASH(x)	((x[0]+x[1]+x[2]+x[3]+x[4]+x[5]+x[6]+x[7]) & \
				(SSFCP_NUM_HASH-1))

#define	FCP_STATEC_MASK	(FC_STATE_OFFLINE | FC_STATE_ONLINE | \
			FC_STATE_LOOP | FC_STATE_NAMESERVICE | \
			FC_STATE_RESET | FC_STATE_RESET_REQUESTED | \
			FC_STATE_LIP | FC_STATE_DEVICE_CHANGE)

/*
 * the per-instance port data structure
 *
 * XXX: can't figure a good way to break this up into sub-structures
 */
struct ssfcp_port {
	kmutex_t			ssfcp_mutex; /* protect whole struct */

	struct scsi_hba_tran 		*ssfcp_tran; /* for SCSA */

	uint32_t			ssfcp_state; /* port state bitmap */

	/*
	 * from the port directly
	 */
	dev_info_t			*ssfcpp_dip;
	uint32_t			ssfcpp_top;
	uint32_t			ssfcpp_sid;
	uint32_t			ssfcpp_state; /* port state: fctl.h */
	la_wwn_t			ssfcpp_nwwn;
	la_wwn_t			ssfcpp_pwwn;
	uint32_t			ssfcpp_instance;
	uint32_t			ssfcpp_max_exch;
	fc_reset_action_t		ssfcpp_cmds_aborted;
	struct modlinkage		ssfcpp_linkage;
	ddi_dma_attr_t			ssfcpp_dma_attr;
	ddi_device_acc_attr_t		ssfcpp_dma_acc_attr;
	uint32_t			ssfcpp_priv_pkt_len;
	opaque_t			*ssfcpp_handle;

	struct ssfcp_port		*ssfcp_next; /* link to next in list */

	uint32_t			ssfcp_ncmds; /* # cmds outstanding */
	uint32_t			ssfcp_dev_cnt;
	uint32_t			ssfcp_link_cnt;

	uint32_t			ssfcp_timer;

	int				ssfcp_use_lock;	/* bool */
	int				ssfcp_take_core; /* bool */

	struct kmem_cache		*ssfcp_pkt_cache;

	kmutex_t			ssfcp_cmd_mutex;

	struct ssfcp_pkt		*ssfcp_pkt_head;
	struct ssfcp_pkt		*ssfcp_pkt_tail;

	struct ssfcp_tgt		*ssfcp_tgt_head;
	struct ssfcp_tgt		*ssfcp_tgt_tail;

	struct ssfcp_reset_elem		*ssfcp_reset_list;
	struct scsi_reset_notify_entry	*ssfcp_reset_notify_listf;

	/* hotplugging */
	uint32_t			ssfcp_hp_initted;
	int				ssfcp_ipkt_cnt;
	kmutex_t			ssfcp_hp_mutex;
	kcondvar_t			ssfcp_hp_cv;
	struct	ssfcp_hp_elem		*ssfcp_hp_elem_list;

	/* for the cmd/response pool */
	kcondvar_t			ssfcp_cr_cv;
	kmutex_t			ssfcp_cr_mutex;
	uint_t				ssfcp_cr_pool_cnt;
	uchar_t				ssfcp_cr_flag;
	struct ssfcp_cr_pool		*ssfcp_cr_pool;

	int				ssfcp_tmp_cnt; /* used for dev cnt */
	int				ssfcp_hp_nele;

	struct	ssfcp_ipkt		*ssfcp_ipkt_list;

	/* for framework event management */
	ndi_event_definition_t		*ssfcp_event_defs;
	ndi_event_hdl_t			ssfcp_event_hdl;
	ndi_events_t			ssfcp_events;

	/* hash lists of targets/LUNs attached to this port */
	struct ssfcp_tgt		*ssfcp_wwn_list[SSFCP_NUM_HASH];

#ifdef	KSTATS_CODE
	kstat_t				ssfcp_kstat;
#endif
};

/*
 * ssfcp_state definations.
 */

#define	SSFCP_STATE_INIT		0x01
#define	SSFCP_STATE_OFFLINE		0x02
#define	SSFCP_STATE_ONLINE		0x04
#define	SSFCP_STATE_SUSPENDED		0x08
#define	SSFCP_STATE_ONLINING		0x10
#define	SSFCP_STATE_DETACHING		0x20
#define	SSFCP_STATE_IN_WATCHDOG		0x40

#define	SSFCP_MAX_DEVICES		127

/* To remember that dip was allocated for a lun on this target. */

#define	SSFCP_DEVICE_CREATED		0x1
#ifdef	_LP64
#define	PKT_PRIV_SIZE			2
#define	PKT_PRIV_LEN			16
#else	/* _ILP32 */
#define	PKT_PRIV_SIZE			1
#define	PKT_PRIV_LEN			8
#endif

#define	SSFCP_EVENT_TAG_INSERT		0
#define	SSFCP_EVENT_TAG_REMOVE		1


#define	SSFCP_SCSI_SCB_SIZE		(sizeof (struct scsi_arq_status))

struct ssfcp_pkt {
	struct ssfcp_pkt		*cmd_forw;
	struct ssfcp_pkt		*cmd_back;
	struct ssfcp_pkt		*cmd_next;
	struct scsi_pkt			*cmd_pkt;
	struct fc_packet		*cmd_fp_pkt;
	uint_t				cmd_timeout;
	uint_t				cmd_state;
	uint_t				cmd_flags;
	uint_t				cmd_privlen;
	uint_t				cmd_dmacount;
	struct fcp_cmd			*cmd_block;
	struct fcp_rsp			*cmd_rsp_block;
	struct ssfcp_cr_pool		*cmd_cr_pool;
	uint32_t			cmd_scblen;
	uint64_t			cmd_pkt_private[PKT_PRIV_LEN];
	char				cmd_scsi_scb[SSFCP_SCSI_SCB_SIZE];
	struct scsi_pkt			cmd_scsi_pkt;
	struct fc_packet		cmd_fc_packet;
};

/*
 * ssfcp_ipkt : Packet for internal commands.
 */
struct ssfcp_ipkt {
	struct ssfcp_port		*ipkt_port;
	struct ssfcp_tgt		*ipkt_tgt;
	struct ssfcp_lun		*ipkt_lun;
	struct ssfcp_ipkt		*ipkt_next;
	struct ssfcp_ipkt		*ipkt_prev;
	struct fc_packet		*ipkt_fpkt;
	uint32_t			ipkt_lun_state;
	uint32_t			ipkt_timeout;
	uint32_t			ipkt_link_cnt;
	uint32_t			ipkt_datalen;
	uint32_t			ipkt_retries;
	uchar_t				ipkt_opcode;
	uint32_t			ipkt_change_cnt;
	struct fc_packet		ipkt_fc_packet;

};

/*
 * cmd_state definations
 */
#define	SSFCP_PKT_IDLE			0x1
#define	SSFCP_PKT_ISSUED		0x2
#define	SSFCP_PKT_ABORTING		0x3

#define	SSFCP_ICMD_TIMEOUT		5	/* seconds */
#define	SSFCP_ICMD_RETRY_CNT		3
#define	SSFCP_POLL_TIMEOUT		60	/* seconds */
/*
 * Define size of extended scsi cmd pkt (ie. includes ARQ)
 */
#define	EXTCMDS_STATUS_SIZE	(sizeof (struct scsi_arq_status))

/*
 * These are the defined cmd_flags for this structure.
 */
#define	CFLAG_DMAVALID		0x0010	/* dma mapping valid */
#define	CFLAG_DMASEND		0x0020	/* data is going 'out' */
#define	CFLAG_CMDIOPB		0x0040	/* this is an 'iopb' packet */
#define	CFLAG_CDBEXTERN		0x0100	/* cdb kmem_alloc'd */
#define	CFLAG_SCBEXTERN		0x0200	/* scb kmem_alloc'd */
#define	CFLAG_FREE		0x0400	/* packet is on free list */
#define	CFLAG_PRIVEXTERN	0x1000	/* target private was */
#define	CFLAG_IN_QUEUE		0x2000	/* command in sf queue */
#define	CFLAG_RESET_OCCURED	0x4000	/* Reset occured on the SCSI address */

#define	SSFCP_MAX_LUNS		256

/*
 * Per target struct
 */
struct ssfcp_tgt {
	struct ssfcp_tgt	*tgt_next;
	struct ssfcp_port	*tgt_port;
	uint32_t		tgt_state;
	kmutex_t		tgt_mutex;
	struct ssfcp_lun	*tgt_lun;
	uint_t			tgt_lun_cnt;
	uint_t			tgt_tmp_cnt;
	opaque_t		tgt_pd_handle;
	la_wwn_t		tgt_node_wwn;
	la_wwn_t		tgt_port_wwn;
	uint32_t		tgt_d_id;
	uint32_t		tgt_hard_addr;
	uchar_t			tgt_device_created;
	uint32_t		tgt_change_cnt;
	uchar_t			tgt_decremented;
	timeout_id_t		tgt_tid;
};

/*
 * Target State
 */
#define	SSFCP_TGT_INIT			0x01
#define	SSFCP_TGT_BUSY			0x02
#define	SSFCP_TGT_MARK			0x04
#define	SSFCP_TGT_OFFLINE		0x08

/* Flag to indicate  tgt is user requested on deamnd creation */

#define	SSFCP_TGT_ON_DEMAND		0x10


#define	SSFCP_NO_CHANGE			0x1
#define	SSFCP_LINK_CHANGE		0x2
#define	SSFCP_DEV_CHANGE		0x3

/* hotplug event struct */
struct ssfcp_hp_event {
	int (*callback)();
	void *arg;
};

/* per lun */
struct ssfcp_lun {
	struct ssfcp_pkt	*lun_pkt_head;
	struct ssfcp_pkt	*lun_pkt_tail;
	uint_t			lun_state;
	dev_info_t		*lun_dip;
	struct ssfcp_tgt	*lun_tgt;	/* back ptr to our tgt */
	uchar_t			lun_num;
	uchar_t			lun_string[FCP_LUN_SIZE];
	struct ssfcp_lun	*lun_next;
	struct ssfcp_lun	*lun_prev;
	struct scsi_hba_tran	*lun_tran;
#ifdef	THESE_USED
	struct ssfcp_hp_event	ssfcp_insert_ev;
	struct ssfcp_hp_event	ssfcp_remove_ev;
#endif
	uchar_t			lun_type;
	kmutex_t		lun_mutex;
};

/*
 * Lun State -- these have the same values as the target states so
 * that they can be interchanged (in cases where the same state occurs
 * for both targets and luns)
 */

#define	SSFCP_LUN_INIT			SSFCP_TGT_INIT
#define	SSFCP_LUN_BUSY			SSFCP_TGT_BUSY
#define	SSFCP_LUN_MARK			SSFCP_TGT_MARK
#define	SSFCP_LUN_OFFLINE		SSFCP_TGT_OFFLINE

#define	SSFCP_LUN_ZERO			0x10
#define	SSFCP_SCSI_LUN_TGT_INIT		0x20	/* target/LUNs all inited */

/*
 * Report Lun Format
 */
struct fcp_reportlun_resp {
	uint32_t	num_lun;	/* num LUNs * 8 */
	uint32_t	reserved;
	longlong_t	lun_string[SSFCP_MAX_LUNS];
};

/*
 * pool of ssfcp command response blocks (the "cr pool")
 */
struct	ssfcp_cr_pool {
	struct	ssfcp_cr_pool	*next;
	struct	ssfcp_cr_free_elem	*free;
	struct	ssfcp_port		*pptr;
	caddr_t			cmd_base;	/* start addr of this chunk */
	ddi_dma_handle_t	cmd_dma_handle; /* dma mapping for chunk */
	ddi_acc_handle_t	cmd_acc_handle;
	caddr_t			rsp_base;
	ddi_dma_handle_t	rsp_dma_handle;
	ddi_acc_handle_t	rsp_acc_handle;
	uint_t			nfree;
	uint_t			ntot;
};

#define	SSFCP_CR_POOL_MAX		32 /* allow 4096 outstanding packets */

#define	SSFCP_ELEMS_IN_POOL		128
#define	SSFCP_LOG2_ELEMS_IN_POOL	7	/* LOG2 SSFCP_ELEMS_IN_POOL */
#define	SSFCP_FREE_CR_EPSILON		64	/* SSFCP_ELEMS_IN_POOL /2 */

/*
 * ssfcp command/response free structure which is overlaid on fcp_cmd
 */

struct ssfcp_cr_free_elem {
	struct ssfcp_cr_free_elem	*next;
	caddr_t				rsp;	/* ptr to corresponding rsp */
};

#define	ADDR2SSFCP(ap)	(struct ssfcp_port *) \
				((ap)->a_hba_tran->tran_hba_private)
#define	ADDR2LUN(ap)	(struct ssfcp_lun *)((ap)->a_hba_tran->tran_tgt_private)
#define	CMD2PKT(cmd)	((cmd)->cmd_pkt)
#define	PKT2CMD(pkt)	((struct ssfcp_pkt *)pkt->pkt_ha_private)

#ifdef	_LP64
#define	PKT_PRIV_SIZE			2
#define	PKT_PRIV_LEN			16
#else /* _ILP32 */
#define	PKT_PRIV_SIZE			1
#define	PKT_PRIV_LEN			8
#endif

/*
 * timeout values
 */
#define	SSFCP_OFFLINE_TIMEOUT		90000000	/* 90 secs */
#define	SSFCP_ELS_TIMEOUT		10		/* 5 secs */
#define	SSFCP_SCSI_CMD_TIMEOUT		60		/* 30 secs */

#define	SSFCP_ELS_RETRIES		4

union ssfcp_internal_cmd {
	la_els_logi_t logi;
	la_els_logo_t logo;
	la_els_prli_t prli;
	la_els_adisc_t adisc;
	struct fcp_cmd cmd;
};

union ssfcp_internal_rsp {
	struct la_els_logi logi;
	struct la_els_logo logo;
	struct la_els_prli prli;
	struct la_els_adisc adisc;
	uchar_t rsp[FCP_MAX_RSP_IU_SIZE];
};

struct ssfcp_hp_elem {
	struct ssfcp_hp_elem	*next;
	dev_info_t		*dip;
	int 			what;
	struct ssfcp_lun	*lun;
	struct ssfcp_port	*pptr;
};

struct ssfcp_reset_elem {
	struct ssfcp_reset_elem	*next;
	struct ssfcp_tgt	*tgt;
	clock_t			timeout;
	uint_t			link_cnt;
};


#define	SSFCP_CP_IN(s, d, handle, len)	(ddi_rep_get8((handle), \
					(uint8_t *)(d), (uint8_t *)(s), \
					(len), DDI_DEV_AUTOINCR))

#define	SSFCP_CP_OUT(s, d, handle, len)	(ddi_rep_put8((handle), \
					(uint8_t *)(s), (uint8_t *)(d), \
					(len), DDI_DEV_AUTOINCR))

#define	SSFCP_ONLINE		0x1
#define	SSFCP_OFFLINE		0x2

#define	FP_IDLE		0x0
#define	FP_OPEN		0x1
#define	FP_EXCL		0x2
#define	LFA(x)		(x & 0xFFFF00)
#define	FCP_SET		1
#define	FCP_RESET	0

/* init() and attach() wait timeout values (in usecs) */
#define	SSFCP_INIT_WAIT_TIMEOUT		60000000	/* 60 seconds */
#define	SSFCP_ATTACH_WAIT_TIMEOUT	10000000	/* 10 seconds */

#ifdef	TRUE
#undef	TRUE
#endif
#define	TRUE			1

#ifdef	FALSE
#undef	FALSE
#endif
#define	FALSE			0

#define	UNDEFINED		-1

/* for softstate */
#define	SSFCP_INIT_ITEMS	5

/*
 * debugging stuff
 */
#ifdef DEBUG

#ifndef	SSFCP_DEBUG_DEFAULT_VAL
#define	SSFCP_DEBUG_DEFAULT_VAL		1
#endif

extern int	ssfcp_debug;

#include <sys/debug.h>


#define	SSFCP_DEBUG(level, stmnt) \
	if (ssfcp_debug >= (level)) ssfcp_log stmnt

/* assume int is large enough for our debugging bitmap purposes */
extern int			ssfcp_debug_flag;

/* bitmap debug values */
#define	SSFCP_MODUNLOAD_DEBUG		0x1	/* to debug modunload */
#define	SSFCP_NO_LINKRESET_DEBUG	0x2	/* to disallow linkreset */
#define	SSFCP_ABORT_DEBUG		0x4	/* do just one abort */

/* for checking the debug flag for bitmap values */
#define	SSFCP_TEST_FLAG(f)		(ssfcp_debug_flag & (f))

#else	/* !DEBUG */

#define	SSFCP_DEBUG(level, stmnt)	/* do nothing */

#endif	/* !DEBUG */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_ULP_FCPVAR_H */
