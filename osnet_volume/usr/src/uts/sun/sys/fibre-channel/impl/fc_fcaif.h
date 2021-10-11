/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FC_FCAIF_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FC_FCAIF_H

#pragma ident	"@(#)fc_fcaif.h	1.3	99/10/18 SMI"
#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Version for FCA vectors
 */
#define	FCTL_FCA_MODREV_1	1

/*
 * State change codes
 */
#define	FC_SC_OFFLINE		0
#define	FC_SC_ONLINE		1

/*
 * pm_cmd_flag definitions
 */
#define	FC_FCA_PM_NOP		0x00
#define	FC_FCA_PM_READ		0x01
#define	FC_FCA_PM_WRITE		0x02
#define	FC_FCA_PM_RW		(FC_FCA_PM_READ | FC_FCA_PM_WRITE)

/*
 *  Command codes for fca_reset()
 */
#define	FC_FCA_LINK_RESET	0x01
#define	FC_FCA_CORE		0x02
#define	FC_FCA_RESET_CORE	0x03
#define	FC_FCA_RESET		0x04

/*
 * fca_port_manage() command codes
 */
#define	FC_PORT_BYPASS		0x01
#define	FC_PORT_UNBYPASS	0x02
#define	FC_PORT_DIAG		0x03
#define	FC_PORT_ERR_STATS	0x04
#define	FC_PORT_GET_FW_REV	0x05
#define	FC_PORT_GET_FCODE_REV	0x06
#define	FC_PORT_GET_DUMP_SIZE	0x07
#define	FC_PORT_FORCE_DUMP	0x08
#define	FC_PORT_GET_DUMP	0x09
#define	FC_PORT_LOOPBACK	0x0A
#define	FC_PORT_LINK_STATE	0x0B
#define	FC_PORT_INITIALIZE	0x0C
#define	FC_PORT_DOWNLOAD_FW	0x0D
#define	FC_PORT_RLS		0x0E
#define	FC_PORT_DOWNLOAD_FCODE	0x0F

/*
 * FCA capability strings
 */
#define	FC_NODE_WWN			"FCA node WWN"
#define	FC_LOGIN_PARAMS			"FCA login parameters"
#define	FC_CAP_UNSOL_BUF		"number of unsolicited bufs"
#define	FC_CAP_PAYLOAD_SIZE		"exchange payload max"
#define	FC_CAP_POST_RESET_BEHAVIOR	"port reset behavior"

typedef struct fc_fca_bind {
	int 			port_num;
	opaque_t 		port_handle;
	void (*port_statec_cb) (opaque_t port_handle, uint32_t state);
	void (*port_unsol_cb) (opaque_t port_handle,
		fc_unsol_buf_t *buf, uint32_t type);
} fc_fca_bind_info_t;

typedef struct fc_fca_port_info {
	uchar_t			pi_topology;	/* Unused */
	uint32_t		pi_error;
	uint32_t		pi_port_state;
	fc_portid_t		pi_s_id;	/* Unused */
	fc_hardaddr_t		pi_hard_addr;	/* Hard address */
	la_els_logi_t		pi_login_params;
} fc_fca_port_info_t;

typedef struct fc_fca_pm {
	uint32_t	pm_cmd_code;	/* port manage command */
	uint32_t	pm_cmd_flags;	/* READ/WRITE */
	size_t		pm_cmd_len;	/* cmd buffer length */
	caddr_t		pm_cmd_buf;	/* cmd buffer */
	size_t		pm_data_len;	/* data buffer length */
	caddr_t		pm_data_buf;	/* data buffer */
	size_t		pm_stat_len;	/* status buffer length */
	caddr_t		pm_stat_buf;	/* status buffer */
} fc_fca_pm_t;

typedef struct fca_tran {
	int				fca_version;
	int				fca_numports;
	int				fca_pkt_size;
	uint32_t			fca_cmd_max;
	ddi_dma_lim_t			*fca_dma_lim;
	ddi_iblock_cookie_t		*fca_iblock;
	ddi_dma_attr_t			*fca_dma_attr;
	ddi_device_acc_attr_t		*fca_acc_attr;

	opaque_t (*fca_bind_port) (dev_info_t *dip,
	    fc_fca_port_info_t *port_info, fc_fca_bind_info_t *bind_info);

	void (*fca_unbind_port) (opaque_t fca_handle);

	int (*fca_init_pkt) (opaque_t fca_handle, fc_packet_t *pkt, int sleep);

	int (*fca_un_init_pkt) (opaque_t fca_handle, fc_packet_t *pkt);

	int (*fca_els_send) (opaque_t fca_handle, fc_packet_t *pkt);

	int (*fca_get_cap) (opaque_t fca_handle, char *cap, void *ptr);

	int (*fca_set_cap) (opaque_t fca_handle, char *cap, void *ptr);

	int (*fca_getmap) (opaque_t fca_handle, fc_lilpmap_t *map);

	int (*fca_transport) (opaque_t fca_handle, fc_packet_t *pkt);

	int (*fca_ub_alloc) (opaque_t fca_handle, uint64_t *tokens,
	    uint32_t ub_size, uint32_t *ub_count, uint32_t type);

	int (*fca_ub_free) (opaque_t fca_handle, uint32_t count,
	    uint64_t tokens[]);

	int (*fca_ub_release) (opaque_t fca_handle, uint32_t count,
	    uint64_t tokens[]);

	int (*fca_abort) (opaque_t fca_handle, fc_packet_t *pkt, int flags);

	int (*fca_reset) (opaque_t fca_handle, uint32_t cmd);

	int (*fca_port_manage) (opaque_t fca_port, fc_fca_pm_t *arg);
} fc_fca_tran_t;

void fc_fca_init(struct dev_ops *fca_devops_p);
int fc_fca_attach(dev_info_t *, fc_fca_tran_t *);
int fc_fca_detach(dev_info_t *fca_dip);
int fc_fca_update_errors(fc_packet_t *pkt);
int fc_fca_error(int fc_errno, char **errmsg);
int fc_fca_pkt_error(fc_packet_t *pkt, char **state, char **reason,
    char **action, char **expln);

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per fca_bind", fc_fca_port_info))
_NOTE(SCHEME_PROTECTS_DATA("unique per fca_bind", fc_fca_bind))
_NOTE(SCHEME_PROTECTS_DATA("stable data", fca_tran))
#endif /* lint */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FC_FCAIF_H */
