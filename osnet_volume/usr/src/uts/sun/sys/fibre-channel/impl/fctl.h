/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FCTL_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FCTL_H

#pragma ident	"@(#)fctl.h	1.2	99/09/28 SMI"
#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * port state definitions.
 *
 * port state is the least significant byte, and the
 * next byte is is the port speed.
 *
 *  +--------------+---------------+
 *  | port speed   |    port state |
 *  +--------------+---------------+
 */
#define	FC_STATE_OFFLINE		0x0000
#define	FC_STATE_ONLINE			0x0001
#define	FC_STATE_LOOP			0x0002
#define	FC_STATE_NAMESERVICE		0x0003
#define	FC_STATE_RESET			0x0004
#define	FC_STATE_RESET_REQUESTED	0x0005
#define	FC_STATE_LIP			0x0006
#define	FC_STATE_LIP_LBIT_SET		0x0007
#define	FC_STATE_DEVICE_CHANGE		0x0008		/* FOR ULPs */
#define	FC_STATE_FULL_SPEED		0x0100
#define	FC_STATE_HALF_SPEED		0x0200
#define	FC_STATE_QUARTER_SPEED		0x0300
#define	FC_STATE_DOUBLE_SPEED		0x0400

#define	FC_PORT_SPEED_MASK(state)	((state) & 0xFF00)
#define	FC_PORT_STATE_MASK(state)	((state) & 0xFF)

/*
 * fc_device state definitions.
 */
#define	FC_DEVICE_INVALID		0
#define	FC_DEVICE_VALID			1

/*
 * fc_port_device type (map_flags) definitions
 * (used in changelist determination)
 */
#define	PORT_DEVICE_NOCHANGE		0x0
#define	PORT_DEVICE_NEW			0x1
#define	PORT_DEVICE_OLD			0x2
#define	PORT_DEVICE_CHANGED		0x3
#define	PORT_DEVICE_DELETE		0x4
#define	PORT_DEVICE_USER_ADD		0x5
#define	PORT_DEVICE_USER_REMOVE		0x6

/*
 * fc_port_device flags definitions
 */
#define	PD_IDLE				0x00
#define	PD_ELS_IN_PROGRESS		0x01

/*
 * fc_port_device recepient definitions
 */
#define	PD_PLOGI_INITIATOR		0
#define	PD_PLOGI_RECEPIENT		1

/*
 * pkt_tran_flags definitions
 */
#define	FC_TRAN_CLASS(flag)		((flag) & 0xF0)
#define	FC_TRAN_INTR			0x01
#define	FC_TRAN_NO_INTR			0x02
#define	FC_TRAN_HI_PRIORITY		0x04
#define	FC_TRAN_CLASS1			0x10
#define	FC_TRAN_CLASS2			0x20
#define	FC_TRAN_CLASS3			0x30
#define	FC_TRAN_CLASS_INVALID		0xF0

/*
 * pkt_tran_type definitions
 */
#define	FC_PKT_NOP			0
#define	FC_PKT_INBOUND			1
#define	FC_PKT_OUTBOUND			2
#define	FC_PKT_EXCHANGE			3
#define	FC_PKT_FCP_READ			4
#define	FC_PKT_FCP_WRITE		5
#define	FC_PKT_IP_WRITE			6
#define	FC_PKT_BROADCAST		7

typedef struct fc_packet {
	uint16_t		pkt_tran_flags;		/* transport flag */
	uint16_t		pkt_tran_type;		/* transport type */
	uint32_t		pkt_timeout;		/* time-out length */
	uint32_t		pkt_cmdlen;		/* command length */
	uint32_t		pkt_rsplen;		/* response length */
	uint32_t		pkt_datalen;		/* data length */
	caddr_t			pkt_cmd;		/* command */
	caddr_t			pkt_resp;		/* response */
	caddr_t			pkt_data;		/* data */
	opaque_t		pkt_ulp_private; 	/* caller's private */
	void			(*pkt_comp)(struct fc_packet *); /* callback */
	struct port_device	*pkt_pd;		/* port device */
	ddi_dma_handle_t	pkt_cmd_dma;		/* command DMA */
	ddi_acc_handle_t	pkt_cmd_acc;		/* command access */
	ddi_dma_cookie_t	pkt_cmd_cookie;		/* command cookie */
	ddi_dma_handle_t	pkt_resp_dma;		/* response DMA */
	ddi_acc_handle_t	pkt_resp_acc;		/* response access */
	ddi_dma_cookie_t	pkt_resp_cookie;	/* response cookie */
	ddi_dma_handle_t	pkt_data_dma;		/* data DMA */
	ddi_acc_handle_t	pkt_data_acc;		/* data access */
	ddi_dma_cookie_t	pkt_data_cookie; 	/* data cookie */
	fc_frame_hdr_t		pkt_cmd_fhdr;		/* command frame hdr */
	opaque_t		pkt_fca_private; 	/* FCA private */
	uchar_t			pkt_state;		/* packet state */
	uchar_t			pkt_action;		/* packet action */
	uint32_t		pkt_reason;		/* expln of state */
	uchar_t			pkt_expln;		/* reason explanation */
	fc_frame_hdr_t		pkt_resp_fhdr;		/* response frame hdr */
	uint32_t		pkt_data_resid;		/* data resid length */
	uint32_t		pkt_resp_resid;		/* resp resid length */
	uint64_t		pkt_rsvd;		/* should be last */
} fc_packet_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("not messed with after transport", fc_packet))
#endif	/* lint */

typedef struct fc_port {
	kmutex_t		fp_mutex;		/* port mutex */
	uint32_t		fp_state;		/* current state */
	fc_portid_t		fp_port_id;		/* This port's S_ID */
	opaque_t		fp_fca_handle;		/* FCA port handle */
	struct fca_tran		*fp_fca_tran;		/* FCA tran vectors */
	struct job_request	*fp_job_head;		/* port's task queue */
	struct job_request	*fp_job_tail;		/* port's task queue */
	struct fp_cmd 		*fp_wait_head;		/* waitQ head */
	struct fp_cmd		*fp_wait_tail;		/* waitQ tail */
	uint32_t		fp_topology;		/* topology */
	int			fp_task;		/* current task */
	int			fp_last_task;		/* last task */
	uint16_t		fp_soft_state;
	uint16_t		fp_restore;
	uchar_t			fp_flag;		/* open/close flag */
	uchar_t			fp_verbose;
	uchar_t			fp_ns_login_class;	/* NS Logi Class */
	uchar_t			fp_sym_port_namelen;	/* Symb port name len */
	uint32_t		fp_cos;			/* class of service */
	struct d_id_hash	*fp_did_table;		/* dest id hashtable */
	struct pwwn_hash	*fp_pwwn_table;		/* port WWN hashtable */
	struct kmem_cache	*fp_pkt_cache;		/* packet cache */
	int			fp_statec_busy;		/* statec counter */
	int			fp_port_num;		/* port number */
	struct fp_cmd		*fp_els_resp_pkt;	/* ready response pkt */
	int			fp_instance;		/* instance number */
	int			fp_ulp_attach;		/* ULP attach done ? */
	int			fp_dev_count;		/* number of devices */
	int			fp_ptpt_master;		/* my WWN is greater */
	int			fp_ulp_nload;		/* count of ULPs */
	int			fp_total_devices; 	/* total count */
	int			fp_els_resp_pkt_busy;
	uint32_t		fp_bind_state;		/* at bind time */
	uint32_t		fp_options;
	fc_porttype_t		fp_port_type;		/* type of port */
	uint32_t		fp_ub_count;		/* Number of UBs */
	int			fp_active_ubs;		/* outstanding UBs */
	uint64_t		*fp_ub_tokens;		/* UB tokens */
	major_t			*fp_ulp_majors;		/* ULPmajors for load */
	kthread_t		*fp_thread;		/* task thread */
	kcondvar_t		fp_cv;			/* port cv */
	kcondvar_t		fp_attach_cv;		/* ULP attach cv */
	dev_info_t		*fp_port_dip;		/* port dip */
	dev_info_t		*fp_fca_dip;		/* FCA dip */
	uintptr_t		fp_softid;		/* soft intr id */
	timeout_id_t		fp_wait_tid;		/* retry timer */
	timeout_id_t		fp_offline_tid;		/* Offline timeout ID */
	fc_lilpmap_t 		fp_lilp_map;		/* LILP map */
	la_els_logi_t		fp_service_params;	/* service parameters */
	fc_reset_action_t	fp_reset_action;	/* FCA reset behavior */
	uchar_t			fp_sym_node_namelen;	/* Sym node name len */
	uchar_t			fp_ipa[8];		/* initial proc assoc */
	uchar_t			fp_ip_addr[16];		/* IP address */
	uint32_t		fp_fc4_types[8];	/* fc4 types */
	struct fc_orphan	*fp_orphan_list;	/* orphan list */
	int			fp_orphan_count;	/* number of orphans */
	fc_hardaddr_t		fp_hard_addr;		/* Hard Address */
	char			fp_sym_port_name[255];	/* Symb port name */
	char			fp_sym_node_name[255];	/* Symb node name */
} fc_port_t;

#if	!defined(lint)
_NOTE(MUTEX_PROTECTS_DATA(fc_port::fp_mutex, fc_port))
_NOTE(SCHEME_PROTECTS_DATA("set once during attach",
    fc_port::fp_port_num fc_port::fp_instance fc_port::fp_cos
    fc_port::fp_port_type fc_port::fp_fc4_types fc_port::fp_ipa
    fc_port::fp_fca_dip fc_port::fp_fca_handle fc_port::fp_ulp_nload
    fc_port::fp_ulp_majors fc_port::fp_service_params
    fc_port::fp_pkt_cache fc_port::fp_did_table fc_port::fp_fca_tran
    fc_port::fp_pwwn_table fc_port::fp_port_dip fc_port::fp_options
    fc_port::fp_verbose fc_port::fp_ub_tokens fc_port::fp_reset_action))
_NOTE(SCHEME_PROTECTS_DATA("busy counter protects it",
    fc_port::fp_els_resp_pkt))
#endif	/* lint */

struct d_id_hash {
	struct port_device 	*d_id_head;
	int 			d_id_count;
};

#if	!defined(lint)
_NOTE(MUTEX_PROTECTS_DATA(fc_port::fp_mutex, d_id_hash))
#endif	/* lint */

struct pwwn_hash {
	struct port_device 	*pwwn_head;
	int 			pwwn_count;
};

#if	!defined(lint)
_NOTE(MUTEX_PROTECTS_DATA(fc_port::fp_mutex, pwwn_hash))
#endif	/* lint */

typedef struct port_device {
	kmutex_t		pd_mutex;	/* mutex */
	fc_portid_t		pd_port_id;	/* Port Identifier */
	la_wwn_t		pd_port_name;	/* the port WWN */
	int			pd_count;	/* ref count */
	uint32_t		pd_state;	/* state */
	struct port_device 	*pd_wwn_hnext;	/* WWN hash list */
	struct port_device 	*pd_did_hnext;	/* D_ID hash list */
	struct port_device 	*pd_port_next;	/* next port device */
	struct fc_device 	*pd_device;	/* fc_device struct */
	fc_porttype_t		pd_porttype;	/* port type */
	fc_hardaddr_t		pd_hard_addr;	/* Hard Address */
	fc_port_t		*pd_port;	/* FC port */
	uchar_t			pd_type;	/* new or old */
	uchar_t			pd_flags;	/* login in progress */
	uchar_t			pd_login_class;	/* Logi Class */
	uchar_t			pd_recepient;	/* who did PLOGI ? */
	uchar_t			pd_ip_addr[8];	/* IP address */
	uint32_t		pd_fc4types[8];	/* FC-4 types */
	uint32_t		pd_cos;		/* class of service */
	struct common_service 	pd_csp;		/* common service */
	struct service_param 	pd_clsp1;	/* Class 1 */
	struct service_param 	pd_clsp2;	/* Class 2 */
	struct service_param 	pd_clsp3;	/* Class 3 */
	caddr_t			pd_private;	/* private data */
	int			pd_held;	/* number of holds */
	uchar_t			pd_spn_len; 	/* length of sym name */
	char			pd_spn[255]; 	/* symbolic port name */
	char			pd_tolerance;	/* number of LOGOs recvd */
} fc_port_device_t;

#if	!defined(lint)
_NOTE(MUTEX_PROTECTS_DATA(port_device::pd_mutex, port_device))
#endif	/* lint */

typedef struct fc_device {
	kmutex_t		fd_mutex;	/* device mutex */
	la_wwn_t		fd_node_name;	/* Node WWN */
	int			fd_count;	/* Number of ports */
	int			fd_flags;	/* device flags */
	fc_port_device_t 	*fd_ports;	/* linked list of ports */
	int			fd_state;	/* device state */
	uchar_t			fd_ipa[8];	/* Initial proc assoc */
	uchar_t			fd_vv[16];	/* Vendor Version */
	uchar_t			fd_snn_len;	/* node symbolic name len */
	uchar_t			fd_snn[255];	/* node symbolic name */
} fc_device_t;

#if	!defined(lint)
_NOTE(MUTEX_PROTECTS_DATA(fc_port::fp_mutex,
    port_device::pd_wwn_hnext port_device::pd_did_hnext))
_NOTE(MUTEX_PROTECTS_DATA(fc_device::fd_mutex, port_device::pd_port_next))
_NOTE(MUTEX_PROTECTS_DATA(port_device::pd_mutex,
    port_device::pd_state port_device::pd_count
    port_device::pd_porttype port_device::pd_port_id
    port_device::pd_hard_addr port_device::pd_port_name
    port_device::pd_spn_len port_device::pd_spn
    port_device::pd_ip_addr port_device::pd_cos
    port_device::pd_fc4types port_device::pd_device
    port_device::pd_port port_device::pd_login_class
    port_device::pd_csp port_device::pd_clsp1
    port_device::pd_clsp2 port_device::pd_clsp3))
_NOTE(MUTEX_PROTECTS_DATA(fc_device::fd_mutex, fc_device))
#endif	/* lint */

typedef struct unsolicited_buffer {
	uchar_t		ub_class;
	uint32_t	ub_bufsize;
	caddr_t		ub_buffer;
	void		*ub_port_private;
	void		*ub_fca_private;
	opaque_t	ub_port_handle;
	uint64_t	ub_token;
	fc_frame_hdr_t 	ub_frame;
} fc_unsol_buf_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per request", unsolicited_buffer))
#endif	/* lint */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FCTL_H */
