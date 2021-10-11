/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_NDI_IMPLDEFS_H
#define	_SYS_NDI_IMPLDEFS_H

#pragma ident	"@(#)ndi_impldefs.h	1.19	99/10/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/ddipropdefs.h>
#include <sys/devops.h>
#include <sys/autoconf.h>
#include <sys/mutex.h>
#include <vm/page.h>
#include <sys/ddi_impldefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Config Debug stuff
 */
#ifdef DEBUG
extern int ndi_config_debug;
#define	NDI_CONFIG_DEBUG(args)  if (ndi_config_debug) cmn_err args
#define	NDI_CONFIG_DEBUG1(args)  if (ndi_config_debug > 1) cmn_err args
#else
#define	NDI_CONFIG_DEBUG(args)
#define	NDI_CONFIG_DEBUG1(args)
#endif

/* structure for maintaining each registered callback */
typedef struct ndi_event_callbacks {
	struct ndi_event_callbacks *ndi_evtcb_next;
	struct ndi_event_callbacks *ndi_evtcb_prev;
	dev_info_t		*ndi_evtcb_dip;
	int			(*ndi_evtcb_callback)();
	void			*ndi_evtcb_arg;
	ddi_eventcookie_t	ndi_evtcb_cookie;
} ndi_event_callbacks_t;


/* event handle for callback management */
struct ndi_event_hdl {
	dev_info_t		*ndi_evthdl_dip;

	/*
	 * mutex that protect the handle and event defs
	 */
	kmutex_t		ndi_evthdl_mutex;

	/*
	 * mutex that just protects the callback list
	 */
	kmutex_t		ndi_evthdl_cb_mutex;

	ddi_iblock_cookie_t	ndi_evthdl_iblock_cookie;

	uint_t			ndi_evthdl_high_plevels;
	uint_t			ndi_evthdl_other_plevels;

	uint_t			ndi_evthdl_n_events;
	ndi_event_definition_t	*ndi_evthdl_event_defs;
	ddi_eventcookie_t	*ndi_evthdl_cookies;

	ndi_event_callbacks_t	*ndi_evthdl_cb_head;
	ndi_event_callbacks_t	*ndi_evthdl_cb_tail;
};

/* prototypes needed by sunndi.c */
int ddi_prop_fm_encode_bytes(prop_handle_t *, void *data, uint_t);

int ddi_prop_fm_encode_ints(prop_handle_t *, void *data, uint_t);

int ddi_prop_update_common(dev_t, dev_info_t *, int, char *, void *, uint_t,
    int (*)(prop_handle_t *, void *, uint_t));

int ddi_prop_lookup_common(dev_t, dev_info_t *, uint_t, char *, void *,
    uint_t *, int (*)(prop_handle_t *, void *, uint_t *));

int ddi_prop_remove_common(dev_t, dev_info_t *, char *, int);
void ddi_prop_remove_all_common(dev_info_t *, int);

int ddi_prop_fm_encode_string(prop_handle_t *, void *, uint_t);

int ddi_prop_fm_encode_strings(prop_handle_t *, void *, uint_t);
int ddi_prop_fm_decode_strings(prop_handle_t *, void *, uint_t *);

void ddi_orphan_devs(dev_info_t *);
void ddi_remove_orphan(dev_info_t *);

void i_ndi_block_device_tree_changes(uint_t *);
void i_ndi_allow_device_tree_changes(uint_t);
int i_ndi_devi_hotplug_queue_empty(uint_t, uint_t);

void i_ndi_devi_config_by_major(major_t);

/*
 * attach/detach hotplugged devinfo nodes
 */
int i_ndi_attach_new_devinfo(major_t);
void i_ndi_remove_new_devinfo(dev_info_t *);
void i_ndi_devi_detach_from_parent(dev_info_t *);

int match_parent(dev_info_t *, char *, char *);

char *ddi_pathname_work(dev_info_t *, char *);

/*
 * ndi_dev_is_auto_assigned_node: Return non-zero if the nodeid in dev
 * has been auto-assigned by the framework and should be auto-freed.
 * (Intended for use by the framework only.)
 */
int i_ndi_dev_is_auto_assigned_node(dev_info_t *);

/*
 * Get and set nodeclass and node attributes.
 * (Intended for ddi framework use only.)
 */
ddi_node_class_t i_ndi_get_node_class(dev_info_t *);
void i_ndi_set_node_class(dev_info_t *, ddi_node_class_t);

int i_ndi_get_node_attributes(dev_info_t *);
void i_ndi_set_node_attributes(dev_info_t *, int);

/*
 * Set nodeid .. not generally advisable.
 * (Intended for the ddi framework use only.)
 */
void i_ndi_set_nodeid(dev_info_t *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NDI_IMPLDEFS_H */
