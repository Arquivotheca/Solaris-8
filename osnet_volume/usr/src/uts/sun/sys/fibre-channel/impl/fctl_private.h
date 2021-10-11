/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FCTL_PRIVATE_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FCTL_PRIVATE_H

#pragma ident	"@(#)fctl_private.h	1.4	99/10/25 SMI"
#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Stuff strictly internal to fctl that
 * isn't exposed to any other modules.
 */
#define	PWWN_HASH_TABLE_SIZE	(32)		/* 2^n */
#define	D_ID_HASH_TABLE_SIZE	(32)		/* 2^n */
#define	NWWN_HASH_TABLE_SIZE	(32)		/* 2^n */
#define	HASH_FUNC(key, size)	((key) & (size - 1))
#define	WWN_HASH_KEY(x)		((x)[0] + (x)[1] + (x)[2] +\
				    (x)[3] + (x)[4] + (x)[5] +\
				    (x)[6] + (x)[7])
#define	D_ID_HASH_FUNC(x, size)	((x) & (size - 1))
#define	FC4_TYPE_WORD_POS(x)	((uchar_t)(x) >> 5)
#define	FC4_TYPE_BIT_POS(x)	((uchar_t)(x) & 0x1F)
#define	FC_ACTION_INVALID	-1
#define	FC_REASON_INVALID	-1
#define	FC_EXPLN_INVALID	-1

/*
 * Internally translated and used state change values to ULPs
 */
#define	FC_ULP_STATEC_DONT_CARE		0
#define	FC_ULP_STATEC_ONLINE		1
#define	FC_ULP_STATEC_OFFLINE		2
#define	FC_ULP_STATEC_OFFLINE_TIMEOUT	3

/*
 * port_dstate values
 */
#define	ULP_PORT_ATTACH			0x01
#define	ULP_PORT_SUSPEND		0x02
#define	ULP_PORT_PM_SUSPEND		0x04
#define	ULP_PORT_BUSY			0x08
#define	FCTL_DISALLOW_CALLBACKS(x)	(!((x) & ULP_PORT_ATTACH) ||\
					((x) & ULP_PORT_BUSY))

typedef struct ulp_ports {
	struct ulp_ports 	*port_next;
	int			port_dstate;
	uint32_t		port_statec;
	kmutex_t		port_mutex;
	struct fc_port		*port_handle;
} fc_ulp_ports_t;

typedef struct ulp_module {
	struct ulp_module 	*mod_next;
	fc_ulp_modinfo_t 	*mod_info;
	fc_ulp_ports_t		*mod_ports;
} fc_ulp_module_t;

typedef struct fca_port {
	struct fca_port 	*port_next;
	struct fc_port		*port_handle;
} fc_fca_port_t;

typedef struct nwwn_elem {
	struct nwwn_elem 	*hash_next;
	struct fc_device 	*fc_device;
} fc_nwwn_elem_t;

typedef struct nwwn_list {
	struct nwwn_elem 	*hash_head;
	int 			num_devs;
} fc_nwwn_list_t;

typedef struct fc_errmap {
	int	fc_errno;
	char	*fc_errname;
} fc_errmap_t;

typedef struct fc_pkt_reason {
	int	reason_val;
	char	*reason_msg;
} fc_pkt_reason_t;

typedef struct fc_pkt_action {
	int	action_val;
	char	*action_msg;
} fc_pkt_action_t;

typedef struct fc_pkt_expln {
	int	expln_val;
	char	*expln_msg;
} fc_pkt_expln_t;

typedef struct fc_pkt_error {
	int			pkt_state;
	char			*pkt_msg;
	fc_pkt_reason_t		*pkt_reason;
	fc_pkt_action_t		*pkt_action;
	fc_pkt_expln_t		*pkt_expln;
} fc_pkt_error_t;

/* Function prototypes */
static int fctl_fca_bus_ctl(dev_info_t *fca_dip, dev_info_t *rip,
    ddi_ctl_enum_t op, void *arg, void *result);
static int fctl_initchild(dev_info_t *fca_dip, dev_info_t *port_dip);
static int fctl_uninitchild(dev_info_t *fca_dip, dev_info_t *port_dip);
static int fctl_cache_constructor(void *buf, void *cdarg, int size);
static void fctl_cache_destructor(void *buf, void *cdarg);
static int fctl_pre_attach(fc_ulp_ports_t *ulp_port, ddi_attach_cmd_t cmd);
static void fctl_post_attach(fc_ulp_module_t *mod, fc_ulp_ports_t *ulp_port,
    ddi_attach_cmd_t cmd, int rval);
static int fctl_pre_detach(fc_ulp_ports_t *ulp_port, ddi_detach_cmd_t cmd);
static void fctl_post_detach(fc_ulp_module_t *mod, fc_ulp_ports_t *ulp_port,
    ddi_detach_cmd_t cmd, int rval);
static fc_ulp_ports_t *fctl_add_ulp_port(fc_ulp_module_t *ulp_module,
    fc_port_t *port_handle, int sleep);
static int fctl_remove_ulp_port(struct ulp_module *ulp_module,
    fc_port_t *port_handle);
static fc_ulp_ports_t *fctl_get_ulp_port(struct ulp_module *ulp_module,
    fc_port_t *port_handle);
static int fctl_update_host_ns_values(fc_port_t *port, fc_ns_cmd_t *ns_req);
static int fctl_retrieve_host_ns_values(fc_port_t *port, fc_ns_cmd_t *ns_req);
static void fctl_link_reset_done(opaque_t port_handle, uchar_t result);
static int fctl_error(int fc_errno, char **errmsg);
static int fctl_pkt_error(fc_packet_t *pkt, char **state, char **reason,
    char **action, char **expln);
static void fctl_check_alpa_list(fc_port_t *port, fc_port_device_t *pd);
static int fctl_is_alpa_present(fc_port_t *port, uchar_t alpa);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FCTL_PRIVATE_H */
