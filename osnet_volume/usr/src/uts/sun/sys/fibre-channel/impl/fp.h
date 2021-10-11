/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FP_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FP_H

#pragma ident	"@(#)fp.h	1.8	99/11/04 SMI"
#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Hack around TNF problems by not compiling
 * it and keep lint happy at the same time
 */
#if	defined(lint) && !defined(__lock_lint)
#define	FP_TNF_ENABLED
#else	/* lint and !lock_lint */
#undef	FP_TNF_ENABLED
#endif /* lint and !lock_lint */

#ifdef	FP_TNF_ENABLED

#include <sys/tnf_probe.h>
extern int tnf_mod_load(void);
extern int tnf_mod_unload(struct modlinkage *e);

#define	FP_TNF_PROBE_0(x)		TNF_PROBE_0 x
#define	FP_TNF_PROBE_1(x)		TNF_PROBE_1 x
#define	FP_TNF_PROBE_2(x)		TNF_PROBE_2 x
#define	FP_TNF_PROBE_3(x)		TNF_PROBE_3 x
#define	FP_TNF_PROBE_4(x)		TNF_PROBE_4 x
#define	FP_TNF_PROBE_5(x)		TNF_PROBE_5 x
#define	FP_TNF_RELEASE_LOCK(lock)	mutex_exit(lock)
#define	FP_TNF_HOLD_LOCK(lock)		mutex_enter(lock)
#define	FP_TNF_INIT(e)			(void) tnf_mod_load()
#define	FP_TNF_FINI(e)			(void) tnf_mod_unload(e)

#else /* FP_TNF_ENABLED */

#define	FP_TNF_PROBE_0(x)
#define	FP_TNF_PROBE_1(x)
#define	FP_TNF_PROBE_2(x)
#define	FP_TNF_PROBE_3(x)
#define	FP_TNF_PROBE_4(x)
#define	FP_TNF_PROBE_5(x)
#define	FP_TNF_RELEASE_LOCK(lock)
#define	FP_TNF_HOLD_LOCK(lock)
#define	FP_TNF_INIT(e)
#define	FP_TNF_FINI(e)

#endif /* FP_TNF_ENABLED */

#define	FP_IS_PKT_ERROR(pkt)	(((pkt)->pkt_state != FC_PKT_SUCCESS) ||\
				((pkt)->pkt_state == FC_PKT_SUCCESS &&\
				(pkt)->pkt_resp_resid != 0))

/*
 * The following are used where there are no
 * well defined return codes.
 */
#define	FP_SUCCESS			(0)
#define	FP_FAILURE			(1)

#define	FP_ONE_SECOND			(1000*1000)

/*
 * Software restoration bit fields while doing
 * (PM)SUSPEND/(PM)RESUME
 */
#define	FP_RESTORE_WAIT_TIMEOUT		0x01
#define	FP_RESTORE_OFFLINE_TIMEOUT	0x02

/*
 * Bit definitions for fp_options field in fc_port
 * structure for Feature and Hack additions to make
 * the driver code a real hairball.
 */
#define	FP_NS_SMART_COUNT		0x01
#define	FP_SEND_RJT			0x02

#define	FP_SET_TASK(fp, new) {\
	(fp)->fp_last_task = (fp)->fp_task;\
	(fp)->fp_task = (new);\
}

#define	FCA_PKT_SIZE(port)	((port)->fp_fca_tran->fca_pkt_size)
#define	FP_RESTORE_TASK(fp) 	((fp)->fp_task = (fp)->fp_last_task)
#define	FP_ELS_TIMEOUT		(10)
#define	FP_NS_TIMEOUT		(120)
#define	FP_PKT_CACHE(port)	((port)->fp_pkt_cache)
#define	FP_IS_F_PORT(p)		((p) & 0x1000)
#define	FP_RETRY_COUNT		(5)
#define	FP_RETRY_DELAY		(3)			/* E_D_TOV + 1 second */
#define	FP_OFFLINE_TICKER	(90)			/* seconds */
#define	FP_ELS_TYPE(ptr)	(*(caddr_t)(ptr))
#define	FP_IS_SYSTEM_BOOTING	(modrootloaded != 1)
#define	FP_DEFAULT_SID		(0x000AE)
#define	FP_DEFAULT_DID		(0x000EA)
#define	FP_PORT_IDENTIFIER_LEN	(4)
#define	FP_UNSOL_BUF_COUNT	(20)
#define	FP_UNSOL_BUF_SIZE	(sizeof (la_els_logi_t))

/*
 *
 */
#define	FP_CP_OUT(h, s, d, c)	ddi_rep_put8((h), (uint8_t *)(s),\
	(uint8_t *)(d), (c), DDI_DEV_AUTOINCR)

/*
 * Response retrieval goodies.
 */
#define	FP_GET_16(h, s)		(ddi_get16((h), (uint16_t *)(s)))
#define	FP_GET_8(h, s)		(ddi_get8((h), (uint8_t *)(s)))
#define	FP_PUT_8(h, d, v)	(ddi_put8((h), (uint8_t *)(d), (v)))
#define	FP_CP_IN(h, s, d, n)	(ddi_rep_get8((h), (uint8_t *)(d),\
				(uint8_t *)(s), (n), DDI_DEV_AUTOINCR))

/* port driver task state machine */
#define	FP_TASK_IDLE			0
#define	FP_TASK_PORT_STARTUP		1
#define	FP_TASK_ULP_ATTACH		2
#define	FP_TASK_LOGO_ALL		3
#define	FP_TASK_OFFLINE			4
#define	FP_TASK_ONLINE			5
#define	FP_TASK_PORT_SHUTDOWN		6
#define	FP_TASK_EXIT_THREAD		7
#define	FP_TASK_GETMAP			8

/*
 * cmd_flags
 */
#define	FP_CMD_CFLAG_UNDEFINED		(-1)
#define	FP_CMD_PLOGI_DONT_CARE		0x00
#define	FP_CMD_PLOGI_RETAIN		0x01	/* Retain LOGIN */
#define	FP_CMD_DELDEV_ON_ERROR		0x02	/* remove device on error */

/*
 * cmd_dflags
 */
#define	FP_CMD_VALID_DMA_MEM		0x01
#define	FP_CMD_VALID_DMA_BIND		0x02
#define	FP_RESP_VALID_DMA_MEM		0x04
#define	FP_RESP_VALID_DMA_BIND		0x08

#define	FP_INIT_CMD(cmd, tran_flags, tran_type, cflag, retry, ulp_pkt) {\
	(cmd)->cmd_pkt.pkt_tran_type = (tran_type);\
	(cmd)->cmd_pkt.pkt_tran_flags = (tran_flags);\
	(cmd)->cmd_flags = (cflag);\
	(cmd)->cmd_retry_count = (retry);\
	(cmd)->cmd_ulp_pkt = (ulp_pkt);\
}

#define	FP_INIT_CMD_RESP(pkt, cmd_len, resp_len) {\
	(pkt)->pkt_cmdlen = (cmd_len);\
	(pkt)->pkt_rsplen = (resp_len);\
}

#define	FP_CMD_TO_PKT(cmd)	(&(cmd)->cmd_pkt)
#define	FP_PKT_TO_CMD(pkt)	((fp_cmd_t *)(pkt)->pkt_ulp_private)
#define	FP_CMD_TO_PORT(cmd)	((cmd)->cmd_port)
#define	FP_ATTACH_ULPS(port)	fp_startup_done((opaque_t)port, FC_PKT_SUCCESS)

#define	FP_UNLOCK_EXPEDITED_ELS_PKT(port) {\
	mutex_enter(&(port)->fp_mutex);\
	port->fp_els_resp_pkt_busy = 0;\
	mutex_exit(&(port)->fp_mutex);\
}

#define	FP_LOCK_EXPEDITED_ELS_PKT(port) {\
	port->fp_els_resp_pkt_busy = 1;\
}

#define	FP_ALREADY_IN_STATE(port, state)\
	(FC_PORT_STATE_MASK((port)->fp_state) == FC_PORT_STATE_MASK(state))

/* fp open flags */
#define	FP_IDLE		0x00
#define	FP_OPEN		0x01
#define	FP_EXCL		0x02
#define	FP_EXCL_BUSY	0x04	/* Exclusive operation in progress */

/* message block/unblock'ing */
#define	FP_NO_MESSAGES			0x00
#define	FP_WARNING_MESSAGES		0x01
#define	FP_FATAL_MESSAGES		0x02

#define	FP_CAN_PRINT_WARNINGS(port)\
	(((port)->fp_verbose & FP_WARNING_MESSAGES) ? 1 : 0)
#define	FP_CAN_PRINT_FATALS(port)\
	(((port)->fp_verbose & FP_FATAL_MESSAGES) ? 1 : 0)

#define	MOD_4(x)	(((x) > 0) ? ((x) - (((x) >> 2) << 2)) : 0)

#define	FP_IS_CLASS_1_OR_2(x)	((x) == FC_TRAN_CLASS1 ||\
	(x) == FC_TRAN_CLASS2)

/*
 * The following macros are defined to avoid some repetitive code
 * in functions without using the most simple and oft-criticized
 * method of spraying gotos.
 *
 * The convention used for naming such macros is
 *     <funcion name>_<MACRO NAME>
 * Note the use of cases.
 */
#define	fp_attach_handler_CLEANUP(port, instance) {\
	if ((port)->fp_fca_handle) {\
		(port)->fp_fca_tran->fca_unbind_port((port)->fp_fca_handle);\
		(port)->fp_fca_handle = NULL;\
	}\
	if ((port)->fp_ub_tokens) {\
		(void) fc_ulp_ubfree((port), (port)->fp_ub_count,\
		    (port)->fp_ub_tokens);\
		kmem_free((port)->fp_ub_tokens, (port)->fp_ub_count *\
		    sizeof (*(port)->fp_ub_tokens));\
		(port)->fp_ub_tokens = NULL;\
	}\
	if ((port)->fp_els_resp_pkt) {\
		fp_free_pkt((port)->fp_els_resp_pkt);\
		(port)->fp_els_resp_pkt = NULL;\
	}\
	if ((port)->fp_pwwn_table) {\
		kmem_free((port)->fp_pwwn_table,\
		    pwwn_table_size * sizeof (struct pwwn_hash));\
		(port)->fp_pwwn_table = NULL;\
	}\
	if ((port)->fp_did_table) {\
		kmem_free((port)->fp_did_table,\
		    did_table_size * sizeof (struct d_id_hash));\
		(port)->fp_did_table = NULL;\
	}\
	if (FP_PKT_CACHE(port)) {\
		kmem_cache_destroy(FP_PKT_CACHE(port));\
		FP_PKT_CACHE(port) = NULL;\
	}\
	cv_destroy(&(port)->fp_attach_cv);\
	cv_destroy(&(port)->fp_cv);\
	mutex_destroy(&(port)->fp_mutex);\
	ddi_remove_minor_node((port)->fp_port_dip, NULL);\
	ddi_soft_state_free(fp_state, (instance));\
}

#define	fp_alloc_pkt_CLEANUP(port, cmd) {\
	if ((cmd)) {\
		fp_free_dma((cmd));\
		kmem_cache_free(FP_PKT_CACHE((port)), (void *)(cmd));\
	}\
}

#define	fp_handle_unsol_buf_INVALID_REQUEST(port, buf, job) {\
	if (FP_IS_CLASS_1_OR_2((buf)->ub_class)) {\
		cmd = fp_alloc_pkt((port), sizeof (la_els_rjt_t),\
		    0, KM_SLEEP);\
		if (cmd != NULL) {\
			fp_els_rjt_init((port), cmd, (buf),\
			    FC_ACTION_NON_RETRYABLE,\
			    FC_REASON_INVALID_LINK_CTRL, (job));\
			if (fp_sendcmd((port), (cmd),\
			    (port)->fp_fca_handle) != FC_SUCCESS) {\
				fp_free_pkt(cmd);\
			}\
		}\
	}\
}

#ifndef	__lock_lint
#define	fp_job_handler_EXIT(port, cpr_info) {\
	CALLB_CPR_EXIT(&(cpr_info));\
	thread_exit();\
}
#else
#define	fp_job_handler_EXIT(port, cpr_info) {\
	thread_exit();\
}
#endif /* __lock_lint */

#define	FP_CT_INIT(port, cmd, ns_cmd, job) {\
	fp_ct_init((port), (cmd), (ns_cmd), (ns_cmd)->ns_cmd_code,\
	    (ns_cmd)->ns_cmd_buf, (ns_cmd)->ns_cmd_size,\
	    (ns_cmd)->ns_resp_size, (job));\
}

#define	fp_fcio_ns_CLEANUP() {\
	fctl_free_ns_cmd(ns_cmd);\
	fctl_dealloc_job(job);\
	kmem_free(ns_req, sizeof (*ns_req));\
}

/*
 * Driver message control
 */
typedef enum fp_mesg_dest {
	FP_CONSOLE_ONLY,
	FP_LOG_ONLY,
	FP_LOG_AND_CONSOLE
} fp_mesg_dest_t;

typedef struct soft_attach {
	ddi_attach_cmd_t    att_cmd;
	struct fc_port	*att_port;
} fp_soft_attach_t;

typedef struct fp_cmd {
	uint16_t	cmd_dflags;		/* DMA flags */
	ksema_t		cmd_sema;
	int		cmd_flags;		/* cmd flags */
	int		cmd_retry_count;
	int		cmd_retry_interval;	/* milli secs */
	fc_packet_t	cmd_pkt;
	fc_port_t	*cmd_port;
	opaque_t	cmd_private;
	struct fp_cmd	*cmd_next;
	fc_packet_t	*cmd_ulp_pkt;
	job_request_t	*cmd_job;
	int (*cmd_transport) (opaque_t fca_handle, fc_packet_t *);
} fp_cmd_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per request", fp_cmd))
_NOTE(SCHEME_PROTECTS_DATA("unique per request", soft_attach))
#endif	/* lint */

/*
 * Procedure templates.
 */
static int fp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int fp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int fp_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd,
    void *arg, void **result);
static int fp_open(dev_t *devp, int flag, int otype, cred_t *credp);
static int fp_close(dev_t dev, int flag, int otype, cred_t *credp);
static int fp_ioctl(dev_t dev, int cmd, intptr_t data, int mode,
    cred_t *credp, int *rval);
static int fp_attach_handler(dev_info_t *dip);
static int fp_resume_handler(dev_info_t *dip);
static int fp_pm_resume_handler(dev_info_t *dip);
static int fp_resume_all(fc_port_t *port, ddi_attach_cmd_t cmd);
static void fp_pm_resume_done(opaque_t arg, uchar_t result);
static void fp_resume_done(opaque_t arg, uchar_t result);
static int fp_detach_handler(fc_port_t *port);
static int fp_suspend_handler(fc_port_t *port);
static int fp_pm_suspend_handler(fc_port_t *port);
static void fp_suspend_all(fc_port_t *port);
static int fp_cache_constructor(void *buf, void *cdarg, int kmflags);
static void fp_cache_destructor(void *buf, void *cdarg);
static fp_cmd_t *fp_alloc_pkt(fc_port_t *port, int cmd_len,
    int resp_len, int kmflags);
static void fp_free_pkt(fp_cmd_t *cmd);
static void fp_free_dma(fp_cmd_t *cmd);
static void fp_job_handler(fc_port_t *port);
static int fp_port_startup(fc_port_t *port, job_request_t *job);
static void fp_startup_done(opaque_t arg, uchar_t result);
static int fp_ulp_port_attach(caddr_t arg);
static int fp_sendcmd(fc_port_t *port, fp_cmd_t *cmd, opaque_t fca_handle);
static void fp_resendcmd(void *port_handle);
static int fp_retry_cmd(fc_packet_t *pkt);
static void fp_enque_cmd(fc_port_t *port, fp_cmd_t *cmd);
static int fp_handle_reject(fc_packet_t *pkt);
static uchar_t fp_get_nextclass(fc_port_t *port, uchar_t cur_class);
static int fp_is_class_supported(uint32_t cos, uchar_t tran_class);
static fp_cmd_t *fp_deque_cmd(fc_port_t *port);
static void fp_jobwait(job_request_t *job);
int fp_state_to_rval(uchar_t state);
static void fp_iodone(fp_cmd_t *cmd);
static void fp_jobdone(job_request_t *job);
static int fp_port_shutdown(fc_port_t *port, job_request_t *job);
static void fp_get_loopmap(fc_port_t *port, job_request_t *job);
static void fp_loop_online(fc_port_t *port, job_request_t *job);
static int fp_get_lilpmap(fc_port_t *port, fc_lilpmap_t *lilp_map);
static int fp_fabric_login(fc_port_t *port, uint32_t s_id, job_request_t *job,
    int flag, int sleep);
static int fp_port_login(fc_port_t *port, uint32_t d_id, job_request_t *job,
    int cmd_flag, int sleep, fc_port_device_t *pd, fc_packet_t *ulp_pkt);
static void fp_register_login(ddi_acc_handle_t *handle, fc_port_device_t *pd,
    la_els_logi_t *acc, uchar_t class);
static void fp_port_device_offline(fc_port_device_t *pd);
static void fp_unregister_login(fc_port_device_t *pd);
static void fp_port_offline(fc_port_t *port, int notify);
static void fp_offline_timeout(void *port_handle);
static void fp_els_init(fp_cmd_t *cmd, uint32_t s_id, uint32_t d_id,
    void (*comp) (), job_request_t *job);
static void fp_xlogi_init(fc_port_t *port, fp_cmd_t *cmd, uint32_t s_id,
    uint32_t d_id, void (*intr) (), job_request_t *job, uchar_t ls_code);
static void fp_logo_init(fc_port_device_t *pd, fp_cmd_t *cmd,
    job_request_t *job);
static void fp_adisc_init(fc_port_device_t *pd, fp_cmd_t *cmd,
    job_request_t *job);
static int fp_ulp_statec_cb(fc_port_t *port, uint32_t state,
    fc_portmap_t *changelist, uint32_t listlen, uint32_t alloc_len, int sleep);
static int fp_ulp_devc_cb(fc_port_t *port, fc_portmap_t *changelist,
    uint32_t listlen, uint32_t alloc_len, int sleep);
static void fp_plogi_group(fc_port_t *port, job_request_t *job);
static void fp_ns_init(fc_port_t *port, job_request_t *job, int sleep);
static void fp_ns_fini(fc_port_t *port, job_request_t *job);
static int fp_ns_reg(fc_port_t *port, fc_port_device_t *pd,
    uint16_t cmd_code, job_request_t *job, int polled, int sleep);
static int fp_common_intr(fc_packet_t *pkt, int iodone);
static void fp_flogi_intr(fc_packet_t *pkt);
static void fp_plogi_intr(fc_packet_t *pkt);
static void fp_adisc_intr(fc_packet_t *pkt);
static void fp_logo_intr(fc_packet_t *pkt);
static void fp_rls_intr(fc_packet_t *pkt);
static void fp_intr(fc_packet_t *pkt);
static void fp_statec_cb(opaque_t port_handle, uint32_t state);
static int fp_ns_scr(fc_port_t *port, job_request_t *job,
    uchar_t scr_func, int sleep);
static int fp_ns_get_devcount(fc_port_t *port, job_request_t *job, int sleep);
static int fp_fciocmd(fc_port_t *port, intptr_t data, int mode, fcio_t *fcio);
static int fp_copyout(void *from, void *to, size_t len, int mode);
static int fp_fcio_copyout(fcio_t *fcio, intptr_t data, int mode);
static void fp_pt_pt_online(fc_port_t *port, job_request_t *job);
static void fp_fabric_online(fc_port_t *port, job_request_t *job);
static int fp_fillout_loopmap(fc_port_t *port, fcio_t *fcio, int mode);
static void fp_unsol_intr(fc_packet_t *pkt);
static void fp_linit_intr(fc_packet_t *pkt);
static void fp_unsol_cb(opaque_t port_handle, fc_unsol_buf_t *buf,
    uint32_t type);
static void fp_handle_unsol_buf(fc_port_t *port, fc_unsol_buf_t *buf,
    job_request_t *job);
static void fp_ba_rjt_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    job_request_t *job);
static void fp_els_rjt_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    uchar_t action, uchar_t reason, job_request_t *job);
static void fp_els_acc_init(fc_port_t *port, fp_cmd_t *cmd, fc_unsol_buf_t *buf,
    job_request_t *job);
static void fp_handle_unsol_logo(fc_port_t *port, fc_unsol_buf_t *buf,
    fc_port_device_t *pd, job_request_t *job);
static void fp_unsol_resp_init(fc_packet_t *pkt, fc_unsol_buf_t *buf,
    uchar_t r_ctl, uchar_t type);
static void fp_i_handle_unsol_els(fc_port_t *port, fc_unsol_buf_t *buf);
static void fp_handle_unsol_plogi(fc_port_t *port, fc_unsol_buf_t *buf,
    job_request_t *job, int sleep);
static void fp_handle_unsol_flogi(fc_port_t *port, fc_unsol_buf_t *buf,
    job_request_t *job, int sleep);
static void fp_login_acc_init(fc_port_t *port, fp_cmd_t *cmd,
    fc_unsol_buf_t *buf, job_request_t *job, int sleep);
static void fp_handle_unsol_rscn(fc_port_t *port, fc_unsol_buf_t *buf,
    job_request_t *job, int sleep);
static void fp_fillout_old_map(fc_portmap_t *map, fc_port_device_t *pd);
static void fp_fillout_changed_map(fc_portmap_t *map, fc_port_device_t *pd,
    uint32_t *new_did, la_wwn_t *new_pwwn);
static void fp_fillout_new_nsmap(fc_port_t *port, ddi_acc_handle_t *handle,
    fc_portmap_t *port_map, ns_resp_gan_t *gan_resp, uint32_t d_id);
static int fp_remote_lip(fc_port_t *port, la_wwn_t *pwwn, int sleep,
    job_request_t *job);
static void fp_stuff_device_with_gan(ddi_acc_handle_t *handle,
    fc_port_device_t *pd, ns_resp_gan_t *gan_resp);
static int fp_ns_query(fc_port_t *port, fctl_ns_req_t *ns_cmd,
    job_request_t *job, int polled, int sleep);
static void fp_ct_init(fc_port_t *port, fp_cmd_t *cmd, fctl_ns_req_t *ns_cmd,
    uint16_t cmd_code, caddr_t cmd_buf, uint16_t cmd_len, uint16_t resp_len,
    job_request_t *job);
static void fp_ns_intr(fc_packet_t *pkt);
static void fp_gan_handler(fc_packet_t *pkt, fctl_ns_req_t *ns_cmd);
static void fp_ns_query_handler(fc_packet_t *pkt, fctl_ns_req_t *ns_cmd);
static void fp_handle_unsol_adisc(fc_port_t *port, fc_unsol_buf_t *buf,
    fc_port_device_t *pd, job_request_t *job);
static void fp_adisc_acc_init(fc_port_t *port, fp_cmd_t *cmd,
    fc_unsol_buf_t *buf, job_request_t *job);
static void fp_load_ulp_modules(dev_info_t *dip, fc_port_t *port);
static int fp_logout(fc_port_t *port, fc_port_device_t *pd, job_request_t *job);
static void fp_attach_ulps(fc_port_t *port, ddi_attach_cmd_t cmd);
static int fp_ulp_notify(fc_port_t *port, uint32_t statec, int sleep);
static int fp_ns_getmap(fc_port_t *port, job_request_t *job,
    fc_portmap_t **map, uint32_t *len);
static fc_port_device_t *fp_create_port_device_by_ns(fc_port_t *port,
    uint32_t d_id, int sleep);
static int fp_check_perms(uchar_t open_flag, uint16_t ioctl_cmd);
static int fp_bind_callbacks(fc_port_t *port);
static void fp_retrieve_caps(fc_port_t *port);
static void fp_validate_area_domain(fc_port_t *port, uint32_t id,
    job_request_t *job, int sleep);
static void fp_validate_rscn_page(fc_port_t *port, fc_affected_id_t *page,
    job_request_t *job, fctl_ns_req_t *ns_cmd, fc_portmap_t *listptr,
    int *listindex, int sleep);
static int fp_ns_validate_device(fc_port_t *port, fc_port_device_t *pd,
    job_request_t *job, int polled, int sleep);
static int fp_validate_lilp_map(fc_lilpmap_t *lilp_map);
static int fp_is_valid_alpa(uchar_t al_pa);
static void fp_printf(fc_port_t *port, int level, fp_mesg_dest_t dest,
    int fc_errno, fc_packet_t *pkt, const char *fmt, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FP_H */
