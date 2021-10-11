/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_IMPL_FC_ULPIF_H
#define	_SYS_FIBRE_CHANNEL_IMPL_FC_ULPIF_H

#pragma ident	"@(#)fc_ulpif.h	1.2	99/10/18 SMI"

#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	FCTL_ULP_MODREV_1		1

/*
 * Flag definitions to fc_ulp_get_portmap function.
 */
#define	FC_ULP_PLOGI_DONTCARE		0
#define	FC_ULP_PLOGI_PRESERVE		1

/*
 * fc_ulp_port_reset() command codes
 */
#define	FC_RESET_PORT			0x01
#define	FC_RESET_ADAPTER		0x02
#define	FC_RESET_DUMP			0x03
#define	FC_RESET_CRASH			0x04

typedef struct fc_portmap {
	int			map_state;
	int			map_flags;		/* OLD, NEW, CHANGED */
	uint32_t		map_fc4_types[8];	/* fc4 types */
	la_wwn_t    		map_pwwn;
	la_wwn_t		map_nwwn;
	fc_portid_t		map_did;
	fc_hardaddr_t		map_hard_addr;
	fc_port_device_t 	*map_pd;	/* port device */
} fc_portmap_t;

typedef struct ulp_port_info {
	struct modlinkage 	*port_linkage;
	dev_info_t		*port_dip;
	opaque_t		port_handle;
	ddi_dma_attr_t		*port_dma_attr;
	ddi_device_acc_attr_t 	*port_acc_attr;
	int			port_fca_pkt_size;
	int			port_fca_max_exch;
	uint32_t		port_state;
	uint32_t		port_flags;
	la_wwn_t		port_pwwn;		/* port WWN */
	la_wwn_t		port_nwwn;		/* node WWN */
	fc_reset_action_t	port_reset_action;	/* FCA reset action */
} fc_ulp_port_info_t;

typedef struct ulp_modinfo {
	opaque_t	ulp_handle;		/* not really needed */
	uint32_t	ulp_rev;		/* ULP revision */
	uchar_t		ulp_type;		/* FC-4 type */
	char 		*ulp_name;		/* ULP Name */
	int		ulp_statec_mask;	/* state change mask */
	int		(*ulp_port_attach) (opaque_t ulp_handle,
			    struct ulp_port_info *, ddi_attach_cmd_t cmd,
			    uint32_t s_id);
	int		(*ulp_port_detach) (opaque_t ulp_handle,
			    struct ulp_port_info *, ddi_detach_cmd_t cmd);
	int		(*ulp_port_ioctl) (opaque_t ulp_handle,
			    opaque_t port_handle, dev_t dev, int cmd,
			    intptr_t data, int mode, cred_t *credp,
			    int *rval, uint32_t claimed);
	int		(*ulp_els_callback) (opaque_t ulp_handle,
			    opaque_t port_handle, fc_unsol_buf_t *payload,
			    uint32_t claimed);
	int		(*ulp_data_callback) (opaque_t ulp_handle,
			    opaque_t port_handle, fc_unsol_buf_t *buf,
			    uint32_t claimed);
	void		(*ulp_statec_callback) (opaque_t ulp_handle,
			    opaque_t port_handle, uint32_t statec,
			    uint32_t port_flags, fc_portmap_t changelist[],
			    uint32_t listlen, uint32_t s_id);
} fc_ulp_modinfo_t;

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique for attach", ulp_port_info))
_NOTE(SCHEME_PROTECTS_DATA("stable data", ulp_modinfo))
_NOTE(SCHEME_PROTECTS_DATA("unique per request", fc_portmap))
#endif	/* lint */

int fc_ulp_add(fc_ulp_modinfo_t *ulp_info);
int fc_ulp_remove(fc_ulp_modinfo_t *ulp_info);
int fc_ulp_init_packet(opaque_t port_handle, fc_packet_t *pkt, int sleep);
int fc_ulp_uninit_packet(opaque_t port_handle, fc_packet_t *pkt);
int fc_ulp_getportmap(opaque_t port_handle, fc_portmap_t *map,
    uint32_t *len, int flag);
int fc_ulp_login(opaque_t port_handle, fc_packet_t **ulp_pkt,
    uint32_t listlen);
int fc_ulp_getmap(la_wwn_t *wwn_list, uint32_t *listlen);
fc_device_t *fc_ulp_get_device_by_nwwn(la_wwn_t *nwwn, int *error);
fc_port_device_t *fc_ulp_get_port_device(opaque_t port_handle, la_wwn_t *pwwn,
    int *error, int create);
int fc_ulp_get_portlist(fc_device_t *device,
    opaque_t *portlist, uint32_t *len);
int fc_ulp_node_ns(fc_device_t *node, uint32_t cmd, void *object);
int fc_ulp_port_ns(opaque_t port_handle, fc_port_device_t *pd,
    fc_ns_cmd_t *ns_req);
int fc_ulp_transport(opaque_t port_handle, fc_packet_t *pkt);
int fc_ulp_issue_els(opaque_t port_handle, fc_packet_t *pkt);
int fc_ulp_uballoc(opaque_t port_handle, uint32_t *count,
    uint32_t size, uint32_t type, uint64_t *tokens);
int fc_ulp_ubfree(opaque_t port_handle, uint32_t count,
    uint64_t *tokens);
int fc_ulp_ubrelease(opaque_t port_handle, uint32_t count,
    uint64_t *tokens);
int fc_ulp_abort(opaque_t port_handle, fc_packet_t *pkt, int flags);
int fc_ulp_linkreset(opaque_t port_handle, la_wwn_t *pwwn, int sleep);
int fc_ulp_port_reset(opaque_t port_handle, uint32_t cmd);
int fc_ulp_get_did(fc_port_device_t *pd, uint32_t *d_id);
int fc_ulp_get_pd_state(fc_port_device_t *pd, uint32_t *state);
int fc_ulp_logout(fc_port_device_t *pd);
int fc_ulp_is_fc4_bit_set(uint32_t *map, uchar_t ulp_type);
int fc_ulp_get_port_instance(opaque_t port_handle);
int fc_ulp_error(int fc_errno, char **errmsg);
int fc_ulp_pkt_error(fc_packet_t *pkt, char **state, char **reason,
    char **action, char **expln);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_IMPL_FC_ULPIF_H */
