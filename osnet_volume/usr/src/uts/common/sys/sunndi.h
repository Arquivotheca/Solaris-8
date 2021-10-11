/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SUNNDI_H
#define	_SYS_SUNNDI_H

#pragma ident	"@(#)sunndi.h	1.31	99/07/26 SMI"

/*
 * Sun Specific NDI definitions
 */


#include <sys/esunddi.h>
#include <sys/sunddi.h>
#include <sys/obpdefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#define	NDI_SUCCESS	DDI_SUCCESS	/* successful return */
#define	NDI_FAILURE	DDI_FAILURE	/* unsuccessful return */
#define	NDI_NOMEM	-2	/* failed to allocate resources */
#define	NDI_BADHANDLE	-3	/* bad handle passed to in function */
#define	NDI_FAULT	-4	/* fault during copyin/copyout */
#define	NDI_BUSY	-5	/* device busy - could not offline */
#define	NDI_UNBOUND	-6	/* device not bound to a driver */

#define	NDI_EVENT_CLAIMED	DDI_EVENT_CLAIMED
#define	NDI_EVENT_UNCLAIMED	DDI_EVENT_UNCLAIMED

/* ndi interface flag values */
#define	NDI_SLEEP		0x00
#define	NDI_NOSLEEP		0x01
#define	NDI_EVENT_NOPASS	0x10 /* do not pass event req up the tree */

/*
 * Property functions:   See also, ddipropdefs.h.
 *			In general, the underlying driver MUST be held
 *			to call it's property functions.
 */

/*
 * Used to create boolean properties
 */
int
ndi_prop_create_boolean(dev_t match_dev, dev_info_t *dip, char *name);

/*
 * Used to create, modify, and lookup integer properties
 */
int
ndi_prop_update_int(dev_t match_dev, dev_info_t *dip, char *name, int data);

int
ndi_prop_update_int_array(dev_t match_dev, dev_info_t *dip, char *name,
    int *data, uint_t nelements);

/*
 * Used to create, modify, and lookup string properties
 */
int
ndi_prop_update_string(dev_t match_dev, dev_info_t *dip, char *name,
    char *data);

int
ndi_prop_update_string_array(dev_t match_dev, dev_info_t *dip,
    char *name, char **data, uint_t nelements);

/*
 * Used to create, modify, and lookup byte properties
 */
int
ndi_prop_update_byte_array(dev_t match_dev, dev_info_t *dip,
    char *name, uchar_t *data, uint_t nelements);

/*
 * Used to remove properties
 */
int
ndi_prop_remove(dev_t dev, dev_info_t *dip, char *name);

void
ndi_prop_remove_all(dev_info_t *dip);

/*
 * Nexus Driver Functions
 */
/*
 * Allocate and initialize a new dev_info structure.
 * This routine will often be called at interrupt time by a nexus in
 * response to a hotplug event, therefore memory allocations are
 * not allowed to sleep.
 */
int
ndi_devi_alloc(dev_info_t *parent, char *node_name, dnode_t nodeid,
    dev_info_t **ret_dip);

void
ndi_devi_alloc_sleep(dev_info_t *parent, char *node_name, dnode_t nodeid,
    dev_info_t **ret_dip);

/*
 * Remove an initialized (but not yet attached) dev_info
 * node from it's parent.
 */
int
ndi_devi_free(dev_info_t *dip);

/*
 * Change the node name
 */
int
ndi_devi_set_nodename(dev_info_t *dip, char *name, int flags);

/*
 * Place the devinfo in the ONLINE state, allowing deferred
 * attach requests to re-attach the device instance.
 *
 * Flags:
 *	NDI_ONLINE_ATTACH - Attach driver to devinfo node when placing
 *			    the device Online.
 */
int
ndi_devi_online(dev_info_t *dip, uint_t flags);

/*
 * Configure children of a nexus node.
 *
 * Flags:
 *	NDI_ONLINE_ATTACH - Attach driver to devinfo node when placing
 *			    the device Online.
 *	NDI_CONFIG - Recursively configure children if child is nexus
 *			node
 */
int
ndi_devi_config(dev_info_t *dip, int flags);

/*
 * Unconfigure children of a nexus node.
 *
 * Flags:
 *	NDI_DEVI_REMOVE - Remove child devinfo nodes
 *
 *	NDI_UNCONFIG - Put child devinfo nodes to uninitialized state,
 *			release resources held by child nodes.
 */
int
ndi_devi_unconfig(dev_info_t *dip, int flags);

/*
 * Take a device node "Offline".
 *
 * Offline means to detach the device instance from the bound
 * driver and setting the devinfo state to prevent deferred attach
 * from re-attaching the device instance.
 *
 * Flags:
 *	NDI_DEVI_REMOVE	- Remove the node from the devinfo tree after
 *			  first taking it Offline.
 */

#define	NDI_DEVI_REMOVE		0x01
#define	NDI_ONLINE_ATTACH	0x02
#define	NDI_DEVI_FORCE		0x04
#define	NDI_CONFIG		0x08
#define	NDI_UNCONFIG		0x10
#define	NDI_DEVI_BIND		0x20
#define	NDI_DEVI_PERSIST	0x40	/* do not config offlined nodes */

int
ndi_devi_offline(dev_info_t *dip, uint_t flags);

/*
 * Find the child dev_info node of parent nexus 'p' whose name
 * matches "cname"@"caddr".
 */
dev_info_t *
ndi_devi_find(dev_info_t *p, char *cname, char *caddr);

/*
 * Copy in the devctl IOCTL data structure and the strings referenced
 * by the structure.
 *
 * Convenience functions for use by nexus drivers as part of the
 * implementation of devctl IOCTL handling.
 */
int
ndi_dc_allochdl(void *iocarg, struct devctl_iocdata **rdcp);

void
ndi_dc_freehdl(struct devctl_iocdata *dcp);

char *
ndi_dc_getpath(struct devctl_iocdata *dcp);

char *
ndi_dc_getname(struct devctl_iocdata *dcp);

char *
ndi_dc_getaddr(struct devctl_iocdata *dcp);

char *
ndi_dc_getminorname(struct devctl_iocdata *dcp);

int
ndi_dc_return_dev_state(dev_info_t *dip, struct devctl_iocdata *dcp);

int
ndi_dc_return_ap_state(devctl_ap_state_t *ap, struct devctl_iocdata *dcp);

int
ndi_dc_return_bus_state(dev_info_t *dip, struct devctl_iocdata *dcp);

int
ndi_get_bus_state(dev_info_t *dip, uint_t *rstate);

int
ndi_set_bus_state(dev_info_t *dip, uint_t state);

/*
 * Post an event notification up the device tree hierarchy to the
 * parent nexus, until claimed by a bus nexus driver or the top
 * of the dev_info tree is reached.
 */
int
ndi_post_event(dev_info_t *dip, dev_info_t *rdip, ddi_eventcookie_t eventhdl,
    void *impl_data);

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_add_eventcall)() interface up the device tree hierarchy,
 * until claimed by a bus nexus driver or the top of the dev_info
 * tree is reached.
 */
int
ndi_busop_add_eventcall(dev_info_t *dip, dev_info_t *rdip,
    ddi_eventcookie_t eventhdl, int (*callback)(), void *arg);

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_remove_eventcall)() interface up the device tree hierarchy,
 * until claimed by a bus nexus driver or the top of the dev_info
 * tree is reached.
 */
int
ndi_busop_remove_eventcall(dev_info_t *dip, dev_info_t *rdip,
    ddi_eventcookie_t eventhdl);

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_get_eventcookie)() interface up the device tree hierarchy,
 * until claimed by a bus nexus driver or the top of the dev_info
 * tree is reached.
 */
int
ndi_busop_get_eventcookie(dev_info_t *dip, dev_info_t *rdip, char *name,
    ddi_eventcookie_t *event_cookiep, ddi_plevel_t *plevelp,
    ddi_iblock_cookie_t *iblock_cookiep);

/*
 * Called by a bus nexus driver.
 * Given a string that contains the name of a bus-specific event, lookup
 * or create a unique handle for the event "name".
 */
ddi_eventcookie_t
ndi_event_getcookie(char *name);

/*
 * ndi event callback support routines:
 *
 * these functions require an opaque ndi event handle
 */
typedef struct ndi_event_hdl *ndi_event_hdl_t;

/*
 * a nexus driver defines events that it can support using the
 * following structure
 */
typedef struct ndi_event_definition {
	int			ndi_event_tag;
	char			*ndi_event_name;
	ddi_plevel_t		ndi_event_plevel;
	uint_t			ndi_event_attributes;
} ndi_event_definition_t;

/* ndi_event_attributes */
#define	NDI_EVENT_POST_TO_ALL	0x0 /* broadcast: post to all handlers */
#define	NDI_EVENT_POST_TO_TGT	0x1 /* call only specific child's hdlr */
#define	NDI_EVENT_POST_TO_ONE	0x2 /* stop calling once event is claimed */

typedef struct ndi_events {
	ushort_t		ndi_events_version;
	ushort_t		ndi_n_events;
	ndi_event_definition_t	*ndi_event_set;
} ndi_events_t;


#define	NDI_EVENTS_REV0			0

/*
 * allocate an ndi event handle
 */
int
ndi_event_alloc_hdl(dev_info_t *dip, ddi_iblock_cookie_t cookie,
	ndi_event_hdl_t *ndi_event_hdl, uint_t flag);

/*
 * free the ndi event handle
 */
int
ndi_event_free_hdl(ndi_event_hdl_t handle);

/*
 * bind or unbind a set of events to/from the event handle
 */
int
ndi_event_bind_set(ndi_event_hdl_t	handle,
	ndi_events_t		*ndi_event_set,
	uint_t			flag);

int
ndi_event_unbind_set(ndi_event_hdl_t	handle,
	ndi_events_t		*ndi_event_set,
	uint_t			flag);

/*
 * get an event cookie
 */
int
ndi_event_retrieve_cookie(ndi_event_hdl_t 	handle,
	dev_info_t		*child_dip,
	char			*eventname,
	ddi_eventcookie_t	*cookiep,
	ddi_plevel_t		*plevelp,
	ddi_iblock_cookie_t	*iblock_cookiep,
	uint_t			flag);

/*
 * add an event callback info to the ndi event handle
 */
int
ndi_event_add_callback(ndi_event_hdl_t 	handle,
	dev_info_t		*child_dip,
	ddi_eventcookie_t	cookie,
	int			(*event_callback)
					(dev_info_t *,
					ddi_eventcookie_t,
					void *arg,
					void *impldata),
	void			*arg,
	uint_t			flag);

/*
 * remove an event callback registration from the ndi event handle
 */
int
ndi_event_remove_callback(ndi_event_hdl_t 	handle,
	dev_info_t		*child_dip,
	ddi_eventcookie_t	cookie,
	uint_t			flag);

/*
 * perform callbacks for a specified cookie:
 * if dip is non-zero, only callback to one driver instance.
 * if dip is NULL, callback to all driver instances registered for
 * the specified event.
 */
int
ndi_event_run_callbacks(ndi_event_hdl_t 	handle,
	dev_info_t		*child_dip,
	ddi_eventcookie_t	cookie,
	void			*bus_impldata,
	uint_t			flag);

/*
 * do callback for just one child_dip, regardless of attributes
 */
int ndi_event_do_callback(ndi_event_hdl_t handle, dev_info_t *child_dip,
	ddi_eventcookie_t cookie, void *bus_impldata, uint_t flag);

/*
 * ndi_event_tag_to_cookie: utility function to find an event cookie
 * given an event tag
 */
ddi_eventcookie_t
ndi_event_tag_to_cookie(ndi_event_hdl_t handle, int event_tag);

/*
 * ndi_event_cookie_to_tag: utility function to find an event tag
 * given an event_cookie
 */
int
ndi_event_cookie_to_tag(ndi_event_hdl_t handle,
	ddi_eventcookie_t cookie);

/*
 * ndi_event_cookie_to_name: utility function to find an event
 * name given an event_cookie
 */
char *
ndi_event_cookie_to_name(ndi_event_hdl_t handle,
	ddi_eventcookie_t cookie);

/*
 * ndi_event_tag_to_name: utility function to find an event
 * name given an event_tag
 */
char *
ndi_event_tag_to_name(ndi_event_hdl_t 	handle, int event_tag);


/*
 * Bus Resource allocation structures and function prototypes exported
 * by busra module
 */

/* structure for specifying a request */
typedef struct ndi_ra_request {
	uint_t		ra_flags;	/* General flags		*/
					/* see bit definitions below	*/

	uint64_t	ra_len;		/* Requested allocation length	*/

	uint64_t	ra_addr;	/* Specific base address requested */

	uint64_t	ra_boundbase;	/* Base address of the area for	*/
					/* the allocated resource to be	*/
					/* restricted to		*/

	uint64_t	ra_boundlen;	/* Length of the area, starting	*/
					/* from ra_boundbase, for the 	*/
					/* allocated resource to be	*/
					/* restricted to.    		*/

	uint64_t	ra_align_mask;	/* Alignment mask used for	*/
					/* allocated base address	*/
} ndi_ra_request_t;


/* ra_flags bit definitions */
#define	NDI_RA_ALIGN_SIZE	0x0001	/* Set the alignment of the	*/
					/* allocated resource address	*/
					/* according to the ra_len	*/
					/* value (alignment mask will	*/
					/* be (ra_len - 1)). Value of	*/
					/* ra_len has to be power of 2.	*/
					/* If this flag is set, value of */
					/* ra_align_mask will be ignored. */


#define	NDI_RA_ALLOC_BOUNDED	0x0002	/* Indicates that the resource	*/
					/* should be restricted to the	*/
					/* area specified by ra_boundbase */
					/* and ra_boundlen */

#define	NDI_RA_ALLOC_SPECIFIED	0x0004	/* Indicates that a specific	*/
					/* address (ra_addr value) is	*/
					/* requested.			*/

#define	NDI_RA_ALLOC_PARTIAL_OK	0x0008  /* Indicates if requested size	*/
					/* (ra_len) chunk is not available */
					/* then allocate as big chunk as */
					/* possible which is less than or */
					/* equal to ra_len size. */


/* return values specific to bus resource allocator */
#define	NDI_RA_PARTIAL_REQ		-7




/* Predefined types for generic type of resources */
#define	NDI_RA_TYPE_MEM			"memory"
#define	NDI_RA_TYPE_IO			"io"
#define	NDI_RA_TYPE_PCI_BUSNUM		"pci_bus_number"
#define	NDI_RA_TYPE_PCI_PREFETCH_MEM	"pci_prefetchable_memory"
#define	NDI_RA_TYPE_INTR		"interrupt"



/* flag bit definition */
#define	NDI_RA_PASS	0x0001		/* pass request up the dev tree */


/*
 * Prototype definitions for functions exported
 */

int
ndi_ra_map_setup(dev_info_t *dip, char *type);

int
ndi_ra_map_destroy(dev_info_t *dip, char *type);

int
ndi_ra_alloc(dev_info_t *dip, ndi_ra_request_t *req, uint64_t *basep,
	uint64_t *lenp, char *type, uint_t flag);

int
ndi_ra_free(dev_info_t *dip, uint64_t base, uint64_t len, char *type,
	uint_t flag);


/*
 * ndi_dev_is_prom_node: Return non-zero if the node is a prom node
 */
int ndi_dev_is_prom_node(dev_info_t *);

/*
 * ndi_dev_is_pseudo_node: Return non-zero if the node is a pseudo node.
 * NB: all non-prom nodes are pseudo nodes.
 * c.f. ndi_dev_is_persistent_node
 */
int ndi_dev_is_pseudo_node(dev_info_t *);

/*
 * ndi_dev_is_persistent_node: Return non-zero if the node has the
 * property of persistence.
 */
int ndi_dev_is_persistent_node(dev_info_t *);

/*
 * Event posted when a fault is reported
 */
#define	DDI_DEVI_FAULT_EVENT	"DDI:DEVI_FAULT"

struct ddi_fault_event_data {
	dev_info_t		*f_dip;
	ddi_fault_impact_t	f_impact;
	ddi_fault_location_t	f_location;
	const char		*f_message;
	ddi_devstate_t		f_oldstate;
};

/*
 * Access handle/DMA handle fault flag setting/clearing functions for nexi
 */
void ndi_set_acc_fault(ddi_acc_handle_t ah);
void ndi_clr_acc_fault(ddi_acc_handle_t ah);
void ndi_set_dma_fault(ddi_dma_handle_t dh);
void ndi_clr_dma_fault(ddi_dma_handle_t dh);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SUNNDI_H */
