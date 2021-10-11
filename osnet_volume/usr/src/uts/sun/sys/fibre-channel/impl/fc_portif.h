/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FC_PORTIF_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FC_PORTIF_H

#pragma ident	"@(#)fc_portif.h	1.3	99/10/25 SMI"
#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* job codes */
#define	JOB_ATTACH_ULP			1
#define	JOB_PORT_STARTUP		2
#define	JOB_PORT_GETMAP			3
#define	JOB_PORT_GETMAP_PLOGI_ALL	4
#define	JOB_SEND_FLOGI			5
#define	JOB_PLOGI_ONE			6
#define	JOB_PLOGI_GROUP			7
#define	JOB_PLOGI_ALL			8
#define	JOB_LOGO_ONE			9
#define	JOB_LOGO_GROUP			10
#define	JOB_LOGO_ALL			11
#define	JOB_PORT_OFFLINE		12
#define	JOB_PORT_ONLINE			13
#define	JOB_PORT_SHUTDOWN		14
#define	JOB_PORT_RSCN			15
#define	JOB_PORT_NS_REG			16
#define	JOB_UNSOL_REQUEST		17
#define	JOB_NS_CMD			18
#define	JOB_LINK_RESET			19
#define	JOB_ULP_NOTIFY			20
#define	JOB_HANDLE_LIP			21
#define	JOB_DUMMY			22

/* job flags */
#define	JOB_TYPE_FCTL_ASYNC		0x01
#define	JOB_TYPE_FP_ASYNC		0x02
#define	JOB_NEW_PKT_LIST		0x04
#define	JOB_CREATE_DEVICE		0x08
#define	JOB_CANCEL_ULP_NOTIFICATION	0x10

/*
 * To remove the port WWN from the orphan list; An orphan list
 * scan typically happens during ONLINE processing (after a LIP
 * in Public loop or link reset) or during RSCN validation.
 */
#define	FC_ORPHAN_SCAN_LIMIT		5

/*
 * Show a limited tolerance on the number of LOGOs that an
 * N/NL_Port can send; Beyond that it'll be removed entirely
 * from the port driver's data base. The tolerance counter
 * is reset after each link reset.
 */
#define	FC_LOGO_TOLERANCE_LIMIT		5

/*
 * ns_flags field definitions in struct
 * fctl_ns_req_t
 */
#define	FCTL_NS_FILL_NS_MAP		0x01
#define	FCTL_NS_GET_DEV_COUNT		0x02
#define	FCTL_NS_NO_DATA_BUF		0x04
#define	FCTL_NS_BUF_IS_USERLAND		0x08
#define	FCTL_NS_BUF_IS_FC_PORTMAP	0x10
#define	FCTL_NS_CREATE_DEVICE		0x20
#define	FCTL_NS_VALIDATE_PD		0x40
#define	FCTL_NS_ASYNC_REQUEST		0x80
#define	FCTL_GAN_START_ID		0xFFFFFF

/*
 * Port driver software state
 *
 * Notice below that in two cases, suspend and pm-suspend,there
 * is no usage of _IN_, which means the bits will stay even after
 * suspend/pm-suspend is complete they are cleared at the time of
 * resume/pm-resume.
 */
#define	FP_SOFT_IN_ATTACH		0x0001
#define	FP_SOFT_IN_DETACH		0x0002
#define	FP_SOFT_SUSPEND			0x0004
#define	FP_SOFT_PM_SUSPEND		0x0008
#define	FP_SOFT_IN_RESUME		0x0010
#define	FP_SOFT_IN_PM_RESUME		0x0020
#define	FP_SOFT_IN_STATEC_CB		0x0040
#define	FP_SOFT_IN_UNSOL_CB		0x0080
#define	FP_SOFT_IN_LINK_RESET		0x0100
#define	FP_SOFT_BAD_LINK		0x0200
#define	FP_SOFT_IN_FCA_RESET		0x0400

/*
 * The switch typically performs a PLOGI from 0xFFFC41
 * Instruct the port driver to just accept it.
 */
#define	FC_MUST_ACCEPT_D_ID(x)	((x) == 0xFFFC41)
#define	FC_NOT_A_REAL_DEVICE(x)	((x) == 0xFFFC41)

/*
 * short hand macros
 */
#define	MBZ		ls_code.mbz
#define	LS_CODE		ls_code.ls_code
#define	PD_PORT_ID	pd_port_id.port_id
#define	FP_PORT_ID	fp_port_id.port_id
#define	NS_PID		pid.port_id
#define	HARD_ADDR	hard_addr.hard_addr
#define	HARD_RSVD	hard_addr.rsvd
#define	PD_HARD_ADDR	pd_hard_addr.hard_addr
#define	PD_HARD_RSVD	pd_hard_addr.rsvd
#define	MAP_HARD_ADDR	map_hard_addr.hard_addr
#define	MAP_HARD_RSVD	map_hard_addr.rsvd


#define	FP_MUST_DROP_CALLBACKS(port)\
	(((port)->fp_soft_state & FP_SOFT_IN_DETACH) ||\
	    ((port)->fp_soft_state & FP_SOFT_SUSPEND) ||\
	    ((port)->fp_soft_state & FP_SOFT_PM_SUSPEND) ||\
	    ((port)->fp_soft_state & FP_SOFT_IN_RESUME) ||\
	    ((port)->fp_soft_state & FP_SOFT_IN_PM_RESUME))

#define	FP_CANT_ALLOW_TRANSPORT(port)	(FP_MUST_DROP_CALLBACKS(port))

#define	FP_CANT_ALLOW_ELS(port)\
	(((port)->fp_soft_state & FP_SOFT_IN_DETACH) ||\
	    ((port)->fp_soft_state & FP_SOFT_SUSPEND) ||\
	    ((port)->fp_soft_state & FP_SOFT_PM_SUSPEND))

#define	FP_NO_CALLBACKS(port)\
	(((port)->fp_soft_state & FP_SOFT_IN_UNSOL_CB) == 0 &&\
	    ((port)->fp_soft_state & FP_SOFT_IN_STATEC_CB) == 0)

#define	FP_IS_SAFE_TO_DETACH(port)	FP_NO_CALLBACKS(port)
#define	FP_IS_SAFE_TO_SUSPEND(port)	FP_NO_CALLBACKS(port)
#define	FP_IS_SAFE_TO_PM_SUSPEND(port)	FP_NO_CALLBACKS(port)

#define	FC_RELEASE_AN_UB(port, buf) {\
	ASSERT(!MUTEX_HELD(&(port)->fp_mutex));\
	mutex_enter(&(port)->fp_mutex);\
	ASSERT((port)->fp_active_ubs > 0);\
	if (--((port)->fp_active_ubs) == 0) {\
		(port)->fp_soft_state &= ~FP_SOFT_IN_UNSOL_CB;\
	}\
	mutex_exit(&(port)->fp_mutex);\
	(port)->fp_fca_tran->fca_ub_release(\
	    (port)->fp_fca_handle, 1, &(buf)->ub_token);\
}

#define	FC_STATEC_DONE(port) {\
	ASSERT(MUTEX_HELD(&(port)->fp_mutex));\
	if (--(port)->fp_statec_busy == 0) {\
		(port)->fp_soft_state &= ~FP_SOFT_IN_STATEC_CB;\
	}\
}

#if	!defined(lint)
#define	FCTL_SET_JOB_COUNTER(job, count) {\
	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS((job)->job_counter))\
	(job)->job_counter = (count);\
}	
#else
#define	FCTL_SET_JOB_COUNTER(job, count) {\
	(job)->job_counter = (count);\
}	
#endif /* lint */

#define	FCTL_NS_GID_PT_INIT(ptr, pt) {\
	((ns_req_gid_pt_t *)(ptr))->port_type.port_type = (pt);\
	((ns_req_gid_pt_t *)(ptr))->port_type.rsvd = 0;\
}

#define	FCTL_NS_GAN_INIT(ptr, s_id) {\
	((ns_req_gan_t *)(ptr))->pid.port_id = (s_id);\
	((ns_req_gan_t *)(ptr))->pid.rsvd = 0;\
}

#define	FCTL_NS_GID_PN_INIT(ptr, wwn) ((ns_req_gid_pn_t *)(ptr))->pwwn = (*wwn)

#define	FCTL_NS_GPN_ID_INIT(ptr, s_id) {\
	((ns_req_gpn_id_t *)(ptr))->pid.port_id = (s_id);\
	((ns_req_gpn_id_t *)(ptr))->pid.rsvd = 0;\
}

#define	FCTL_PORT_QUEUE_EMPTY(p)	(((p)->fp_job_tail == NULL) ? 1 : 0)

#define	FCTL_WWN_SIZE(wwn)	(sizeof ((wwn)->raw_wwn)\
				/ sizeof ((wwn)->raw_wwn[0]))

typedef struct job_request {
	int			job_code;
	int			job_result;
	int			job_flags;
	int			job_counter;
	opaque_t		job_cb_arg;		/* callback func arg */
	kmutex_t		job_mutex;
	ksema_t			job_fctl_sema;
	ksema_t			job_port_sema;
	void			(*job_comp) (opaque_t, uchar_t result);
	fc_packet_t		**job_ulp_pkts;
	uint32_t		job_ulp_listlen;	/* packet list length */
	void			*job_private;		/* caller's private */
	void			*job_arg;		/* caller's argument */
	struct job_request 	*job_next;
} job_request_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per request",
	job_request::job_code job_request::job_result job_request::job_flags
	job_request::job_cb_arg job_request::job_comp
	job_request::job_ulp_pkts job_request::job_ulp_listlen
	job_request::job_private job_request::job_arg))
_NOTE(MUTEX_PROTECTS_DATA(fc_port::fp_mutex, job_request::job_next))
_NOTE(MUTEX_PROTECTS_DATA(job_request::job_mutex, job_request::job_counter))
#endif	/* lint */

typedef struct fc_port_clist {
	opaque_t	clist_port;		/* port handle */
	uint32_t	clist_state;		/* port state */
	uint32_t	clist_len;		/* map len */
	uint32_t	clist_size;		/* alloc len */
	fc_portmap_t 	*clist_map;		/* changelist */
	uint32_t	clist_flags;		/* port topology */
} fc_port_clist_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per state change", fc_port_clist))
#endif	/* lint */

/*
 * The cmd_size and resp_size shouldn't include the CT HEADER.
 *
 * For commands like GAN, the ns_resp_size should indicate the
 * total number of bytes allocated in the ns_resp_buf to get all
 * the NS objects.
 */
typedef struct fctl_ns_req {
	int			ns_result;
	uint32_t		ns_gan_index;
	uint32_t		ns_gan_sid;
	uint32_t		ns_flags;
	uint16_t		ns_cmd_code;	/* NS command code */
	caddr_t			ns_cmd_buf;	/* NS command buffer */
	uint16_t		ns_cmd_size;	/* NS command length */
	uint16_t		ns_resp_size;	/* NS response length */
	caddr_t			ns_data_buf;	/* User buffer */
	uint32_t		ns_data_len;	/* User buffer length */
	uint32_t		ns_gan_max;
	fc_ct_header_t		ns_resp_hdr;
	fc_port_device_t	*ns_pd;
} fctl_ns_req_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per state change", fctl_ns_req))
#endif	/* lint */

/*
 * Orphan list of Port WWNs
 */
typedef struct fc_orphan {
	int			orp_nscan;	/* Number of scans */
	clock_t			orp_tstamp;	/* When it disappeared */
	la_wwn_t		orp_pwwn;	/* Port WWN */
	struct fc_orphan	*orp_next;	/* Next orphan */
} fc_orphan_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("scans don't interleave",
	fc_orphan::orp_nscan fc_orphan::orp_pwwn fc_orphan::orp_tstamp))
_NOTE(MUTEX_PROTECTS_DATA(fc_port::fp_mutex, fc_orphan::orp_next))
#endif /* lint */


job_request_t *fctl_alloc_job(int job_code, int job_flags,
    void (*comp) (opaque_t, uchar_t), opaque_t arg, int sleep);
void fctl_dealloc_job(job_request_t *job);
void fctl_enque_job(fc_port_t *port, job_request_t *job);
void fctl_priority_enque_job(fc_port_t *port, job_request_t *job);
job_request_t *fctl_deque_job(fc_port_t *port);
void fctl_add_port(fc_port_t *port);
void fctl_remove_port(fc_port_t *port);
int fctl_wwn_cmp(la_wwn_t *src, la_wwn_t *dst);
int fctl_atoi(caddr_t string, int base);
void fctl_jobwait(job_request_t *job);
void fctl_jobdone(job_request_t *job);
fc_port_device_t *fctl_alloc_port_device(fc_port_t *port, la_wwn_t *port_wwn,
    uint32_t d_id, uchar_t recepient, int sleep);
fc_port_device_t *fctl_get_port_device_by_did(fc_port_t *port, uint32_t d_id);
fc_port_device_t *fctl_hold_port_device_by_did(fc_port_t *port, uint32_t d_id);
fc_port_device_t *fctl_get_port_device_by_pwwn(fc_port_t *port, la_wwn_t *pwwn);
fc_port_device_t *fctl_hold_port_device_by_pwwn(fc_port_t *port,
    la_wwn_t *pwwn);
void fctl_release_port_device(fc_port_device_t *pd);
void fctl_dealloc_port_device(fc_port_device_t *pd);
void fctl_add_port_to_device(struct fc_device *device, fc_port_device_t *pd);
void fctl_enlist_did_table(fc_port_t *port, fc_port_device_t *pd);
void fctl_delist_did_table(fc_port_t *port, fc_port_device_t *pd);
void fctl_set_pd_state(fc_port_device_t *pd, int state);
int fctl_get_pd_state(fc_port_device_t *pd);
void fctl_enlist_pwwn_table(fc_port_t *port, fc_port_device_t *pd);
void fctl_delist_pwwn_table(fc_port_t *port, fc_port_device_t *pd);
struct fc_device *fctl_create_device(la_wwn_t *nwwn, int sleep);
void fctl_remove_device(struct fc_device *device);
fc_port_device_t *fctl_get_port_device_by_pwwn(fc_port_t *port, la_wwn_t *pwwn);
void fctl_wwn_to_str(la_wwn_t *wwn, caddr_t string);
int fctl_wwn_match(la_wwn_t *src, la_wwn_t *dst);
int fctl_detach_ulps(fc_port_t *port, ddi_detach_cmd_t cmd,
    struct modlinkage *linkage);
int fctl_ulp_statec_cb(caddr_t arg);
void fctl_attach_ulps(fc_port_t *port, ddi_attach_cmd_t cmd,
    struct modlinkage *linkage);
struct fc_device *fctl_get_device_by_nwwn(la_wwn_t *node_wwn);
int fctl_enlist_nwwn_table(struct fc_device *device, int sleep);
void fctl_delist_nwwn_table(struct fc_device *device);
void fctl_fillout_map(fc_port_t *port, fc_portmap_t **map,
    uint32_t *len, int whole_map);
void fctl_remove_port_from_device(struct fc_device *device,
    fc_port_device_t *pd);
fc_port_device_t *fctl_create_port_device(fc_port_t *port, la_wwn_t *node_wwn,
    la_wwn_t *port_wwn, uint32_t d_id, uchar_t recepient, int sleep);
void fctl_remove_port_device(fc_port_t *port, fc_port_device_t *pd);
void fctl_remall(fc_port_t *port);
int fctl_is_wwn_zero(la_wwn_t *wwn);
void fctl_ulp_unsol_cb(fc_port_t *port, fc_unsol_buf_t *buf, uchar_t type);
void fctl_copy_portmap(fc_portmap_t *map, fc_port_device_t *pd);
fctl_ns_req_t *fctl_alloc_ns_cmd(uint32_t cmd_len, uint32_t resp_len,
    uint32_t data_len, uint32_t ns_flags, int sleep);
void fctl_free_ns_cmd(fctl_ns_req_t *ns_cmd);
int fctl_ulp_port_ioctl(fc_port_t *port, dev_t dev, int cmd, intptr_t data,
    int mode, cred_t *credp, int *rval);
int fctl_remove_if_orphan(fc_port_t *port, la_wwn_t *pwwn);
int fctl_add_orphan(fc_port_t *port, fc_port_device_t *pd, int sleep);
void fctl_remove_oldies(fc_port_t *port);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FC_PORTIF_H */
