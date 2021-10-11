/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sunndi.c	1.50	99/10/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/model.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/open.h>
#include <sys/user.h>
#include <sys/t_lock.h>
#include <sys/vm.h>
#include <sys/stat.h>
#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/as.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi.h>
#include <sys/kstat.h>
#include <sys/conf.h>
#include <sys/ddi_impldefs.h>	/* include implementation structure defs */
#include <sys/ndi_impldefs.h>
#include <sys/hwconf.h>
#include <sys/pathname.h>
#include <sys/modctl.h>
#include <sys/epm.h>
#include <sys/devctl.h>
#include <sys/callb.h>
#include <sys/bootconf.h>
#include <sys/devfs_log_event.h>
#include <sys/dacf_impl.h>

#ifdef __sparcv9cpu
#include <sys/archsystm.h>	/* XXX	getpil/setpil */
#include <sys/membar.h>		/* XXX	membar_sync */
#endif

/*
 * unconfigure control structure
 */
struct unconfig_ctl {
	uint_t	flags;
	int	error;
	dev_info_t *root_dip;
};

/*
 * online node request queue:
 * ndi_devi_online adds dips to this list and the hotplug daemon
 * removes them and attaches drivers
 * the list is protected by the hotplug mutex
 */
typedef struct ndi_online_rqst {
	major_t		major;
	dev_info_t	*dip;
	struct ndi_online_rqst *next;
} ndi_online_rqst_t;

static ndi_online_rqst_t *newdev_list;

/*
 * prototypes
 */
static int i_ndi_devi_has_initialized_children(dev_info_t *);
static int i_ndi_devi_offline_node(dev_info_t *, void *);
static int i_ndi_devi_offline(dev_info_t *, uint_t);
static int i_ndi_attach_spec_children(dev_info_t *, struct par_list *,
	struct devnames *, major_t, major_t, int *, int);
static int i_ndi_check_pl_list(dev_info_t *, struct par_list *,
    struct devnames *, major_t, major_t, int *, int);
static void i_ndi_makechild_from_spec(dev_info_t *, struct hwc_spec *, uint_t);
static int i_ndi_walkdevs_bottom_up(dev_info_t *,
	int (*f)(dev_info_t *, void *), void *);
static int i_ndi_devi_attach_node(struct devnames *, dev_info_t *, uint_t);
static int i_ndi_devi_bind_driver(dev_info_t *, uint_t);
static void i_ndi_devi_hotplug_wakeup(dev_info_t *, major_t);
static void i_ndi_devi_config(struct devnames *, dev_info_t *, int);
static int i_ndi_devi_unconfig(struct devnames *, dev_info_t *, int);
static void i_ndi_devi_migrate(struct devnames *, dev_info_t *);

/*
 * ndi property handling
 */
int
ndi_prop_update_int(dev_t match_dev, dev_info_t *dip,
    char *name, int data)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_HW_DEF | DDI_PROP_TYPE_INT | DDI_PROP_DONTSLEEP,
	    name, &data, 1, ddi_prop_fm_encode_ints));
}

int
ndi_prop_create_boolean(dev_t match_dev, dev_info_t *dip,
    char *name)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_HW_DEF | DDI_PROP_TYPE_ANY | DDI_PROP_DONTSLEEP,
	    name, NULL, 0, ddi_prop_fm_encode_bytes));
}

int
ndi_prop_update_int_array(dev_t match_dev, dev_info_t *dip,
    char *name, int *data, uint_t nelements)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_HW_DEF | DDI_PROP_TYPE_INT | DDI_PROP_DONTSLEEP,
	    name, data, nelements, ddi_prop_fm_encode_ints));
}

int
ndi_prop_update_string(dev_t match_dev, dev_info_t *dip,
    char *name, char *data)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_HW_DEF | DDI_PROP_TYPE_STRING | DDI_PROP_DONTSLEEP,
	    name, &data, 1, ddi_prop_fm_encode_string));
}

int
ndi_prop_update_string_array(dev_t match_dev, dev_info_t *dip,
    char *name, char **data, uint_t nelements)
{
	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_HW_DEF | DDI_PROP_TYPE_STRING | DDI_PROP_DONTSLEEP,
	    name, data, nelements,
	    ddi_prop_fm_encode_strings));
}

int
ndi_prop_update_byte_array(dev_t match_dev, dev_info_t *dip,
    char *name, uchar_t *data, uint_t nelements)
{
	if (nelements == 0)
		return (DDI_PROP_INVAL_ARG);

	return (ddi_prop_update_common(match_dev, dip,
	    DDI_PROP_HW_DEF | DDI_PROP_TYPE_BYTE | DDI_PROP_DONTSLEEP,
	    name, data, nelements, ddi_prop_fm_encode_bytes));
}

int
ndi_prop_remove(dev_t dev, dev_info_t *dip, char *name)
{
	return (ddi_prop_remove_common(dev, dip, name, DDI_PROP_HW_DEF));
}

void
ndi_prop_remove_all(dev_info_t *dip)
{
	ddi_prop_remove_all_common(dip, (int)DDI_PROP_HW_DEF);
}

/*
 * I/O Hotplug control
 */

/*
 * initialization of hotplug_thread signals the availability
 * of the thread to ndi_devi_online()
 */
static kthread_id_t hotplug_thread;

static kcondvar_t hotplug_cv;
static kcondvar_t hotplug_completion_cv;
static kmutex_t   hotplug_lk;
static ksema_t	hotplug_sema;
static kthread_id_t holding_hotplug_sema;
static uint_t	hotplug_sema_circular;
static uint_t	hotplug_idle_time = 60;	/* idle time in secs before exiting */

static edesc_t *event_hash_table[EVC_BUCKETS];
static kmutex_t event_hash_mutex;

int impl_ddi_merge_child(dev_info_t *child);

static void i_ndi_insert_ordered_devinfo(struct devnames *dnp, dev_info_t *dip);
static char *i_encode_composite_string(char **strings, uint_t nstrings,
    size_t *retsz);
static int i_ndi_lookup_compatible(dev_info_t *dip);

/*
 * Compute the cumulative sum of the passed string
 */
static int
i_ndi_compute_sum(char *w)
{
	unsigned char c;
	int sum;

	for (sum = 0; ((c = *w) != '\0'); ) {
		sum += c;
		w++;
	}
	return (EVC_HASH(sum));
}

/*
 * Post an event notification up the device tree hierarchy to the
 * parent nexus.
 */
int
ndi_post_event(dev_info_t *dip, dev_info_t *rdip,
		ddi_eventcookie_t eventhdl, void *impl_data)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;


	/*
	 * check for a correct revno before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->busops_rev < BUSO_REV_3) {
		return (DDI_FAILURE);
	}
	/*
	 * check for a NULL ptr before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->bus_post_event == NULL)
		return (DDI_FAILURE);
	/*
	 * request parent to post the event
	 */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_post_event))
		(pdip, rdip, eventhdl, impl_data));
}

/*
 * Given a string that contains the name of a bus-specific event, lookup
 * or create a unique handle for the event "name".
 */
ddi_eventcookie_t
ndi_event_getcookie(char *name)
{
	int hash;
	edesc_t *edp, *eedp = NULL;

	ASSERT(name);
	hash = i_ndi_compute_sum(name);
	mutex_enter(&event_hash_mutex);
	for (edp = event_hash_table[hash]; edp != NULL; edp = edp->next) {
		if (strcmp(edp->name, name) == 0) {
			mutex_exit(&event_hash_mutex);
			return ((ddi_eventcookie_t)edp);
		}
		eedp = edp;
	}
	/*
	 * Not in table. Hash this event.
	 */
	edp = kmem_alloc(sizeof (edesc_t), KM_SLEEP);
	edp->name = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(edp->name, name);
	edp->next = NULL;

	if (eedp == NULL) {
		/*
		 * This hash index is empty.
		 */
		event_hash_table[hash] = edp;
	} else {
		/*
		 * Append to end of list for this hash entry.
		 */
		eedp->next = edp;
	}
	mutex_exit(&event_hash_mutex);
	return ((ddi_eventcookie_t)edp);
}

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_remove_eventcall)() interface up the device tree hierarchy.
 */
int
ndi_busop_remove_eventcall(dev_info_t *dip, dev_info_t *rdip,
		ddi_eventcookie_t eventhdl)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/*
	 * check for a correct revno before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->busops_rev < BUSO_REV_3) {
		return (DDI_FAILURE);
	}
	/*
	 * check for a NULL ptr before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->bus_remove_eventcall == NULL)
		return (DDI_FAILURE);
	/*
	 * request parent to remove the eventcall
	 */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_remove_eventcall))
		(pdip, rdip, eventhdl));
}

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_add_eventcall)() interface up the device tree hierarchy.
 */
int
ndi_busop_add_eventcall(dev_info_t *dip, dev_info_t *rdip,
		ddi_eventcookie_t eventhdl, int (*callback)(), void *arg)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/*
	 * check for a correct revno before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->busops_rev < BUSO_REV_3) {
		return (DDI_FAILURE);
	}
	/*
	 * check for a NULL ptr before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->bus_add_eventcall == NULL)
		return (DDI_FAILURE);
	/*
	 * request parent to add the eventcall
	 */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_add_eventcall))
		(pdip, rdip, eventhdl, callback, arg));
}

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_get_eventcookie)() interface up the device tree hierarchy.
 */
int
ndi_busop_get_eventcookie(dev_info_t *dip, dev_info_t *rdip, char *name,
		ddi_eventcookie_t *event_cookiep,
		ddi_plevel_t *plevelp,
		ddi_iblock_cookie_t *iblock_cookiep)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/*
	 * check for a correct revno before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->busops_rev < BUSO_REV_3) {
		return (DDI_FAILURE);
	}
	/*
	 * check for a NULL ptr before calling up the device tree.
	 */
	if (DEVI(pdip)->devi_ops->devo_bus_ops->bus_get_eventcookie == NULL)
		return (DDI_FAILURE);
	/*
	 * request parent to return an eventcookie
	 */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_get_eventcookie))
		(pdip, rdip, name, event_cookiep, plevelp, iblock_cookiep));
}


/*
 * Config Debug stuff
 */
#ifdef DEBUG
int ndi_config_debug = 0;
#endif

/*
 * Allocate and initialize a dev_info structure
 */
static dev_info_t *
i_ndi_devi_alloc(dev_info_t *parent, char *node_name, dnode_t n, uint_t flags)
{
	struct dev_info *devi;
	int nodeid = (int)n;
	int kma_flag = ((flags & NDI_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP);
	static char failed[] = "failed to allocate device information node";

	ASSERT(node_name != NULL);

	if ((devi = kmem_zalloc(sizeof (*devi), kma_flag)) == NULL) {
		cmn_err(CE_NOTE, failed);
		return (NULL);
	}
	if ((devi->devi_node_name = kmem_alloc(strlen(node_name) + 1,
	    kma_flag)) == NULL) {
		kmem_free(devi, sizeof (*devi));
		cmn_err(CE_NOTE, failed);
		return (NULL);
	}
	(void) strcpy(devi->devi_node_name, node_name);

	/*
	 * We're almost ready to commit to the new node ... if the
	 * nodetype is 'prom', try to 'take' the nodeid now, since
	 * that may also require memory allocation ...
	 */
	switch (nodeid) {
	case DEVI_PSEUDO_NODEID:
	case DEVI_SID_NODEID:
		break;
	default:
		if (impl_ddi_take_nodeid(nodeid, kma_flag) != 0) {
			kmem_free(devi->devi_node_name, strlen(node_name) + 1);
			kmem_free(devi, sizeof (*devi));
			cmn_err(CE_NOTE, failed);
			return (NULL);
		}
	}

	/*
	 * default binding name to the dev_info nodename
	 * we re-bind in ndi_devi_online after the caller
	 * has had the opportunity to attach a "compatible" property
	 * for binding a generically named device.
	 */
	i_ddi_set_binding_name((dev_info_t *)devi, devi->devi_node_name);
	mutex_init(&(devi->devi_lock), NULL, MUTEX_DEFAULT, NULL);
	cv_init(&(devi->devi_cv), NULL, CV_DEFAULT, NULL);

	/*
	 * Assign devi_nodeid, devi_node_class, devi_node_attributes
	 * according to the following algorithm:
	 *
	 * nodeid arg		node class		node attributes
	 *
	 * DEVI_PSEUDO_NODEID	DDI_NC_PSEUDO		A
	 * DEVI_SID_NODEID	DDI_NC_PSEUDO		A,P
	 * other		DDI_NC_PROM		P
	 *
	 * Where A = DDI_AUTO_ASSIGNED_NODEID (auto-assign a nodeid)
	 * and	 P = DDI_PERSISTENT
	 *
	 * auto-assigned nodeids are also auto-freed.
	 */
	switch (nodeid) {
	case DEVI_SID_NODEID:
		devi->devi_node_attributes = DDI_PERSISTENT;
		/*FALLTHROUGH*/
	case DEVI_PSEUDO_NODEID:
		devi->devi_node_attributes |= DDI_AUTO_ASSIGNED_NODEID;
		devi->devi_node_class = DDI_NC_PSEUDO;
		if (impl_ddi_alloc_nodeid(&devi->devi_nodeid))
			cmn_err(CE_PANIC, "i_ndi_devi_alloc: out of nodeids");
		break;
	default:
		/* NB: impl_ddi_take_nodeid has already been done */
		devi->devi_nodeid = nodeid;
		devi->devi_node_class = DDI_NC_PROM;
		devi->devi_node_attributes = DDI_PERSISTENT;
		break;
	}


	/*
	 * Initialize the instance # to -1, indicating no instance
	 * has been assigned to the dev_info node.
	 *
	 * XXX investigate copying the property data into the
	 * dev_info node.
	 */
	devi->devi_instance = -1;
	devi->devi_state |= DEVI_S_UNBOUND;
	devi->devi_parent = DEVI(parent);
	devi->devi_bus_ctl = DEVI(parent);

	NDI_CONFIG_DEBUG((CE_CONT,
	    "i_ndi_devi_alloc: parent=%s%d->%p, name=%s id=%d\n",
	    ddi_driver_name(parent), ddi_get_instance(parent),
	    (void *)devi,  node_name, devi->devi_nodeid));

	return ((dev_info_t *)devi);
}

static void
i_ndi_devi_attach_to_parent(dev_info_t *dip)
{
	struct dev_info *devi = DEVI(dip);
	struct dev_info *parent = devi->devi_parent;

	/*
	 * if the devi is bound  then it has already been
	 * attached to the parent
	 */
	if ((devi->devi_state & DEVI_S_UNBOUND) == 0) {
		return;
	} else {
		dev_info_t **pdip, *list;

		/*
		 * attach the node to the specified parent
		 * at the end of the list unless already there
		 */
		rw_enter(&(devinfo_tree_lock), RW_WRITER);
		pdip = (dev_info_t **)(&DEVI(parent)->devi_child);
		list = *pdip;
		while (list && (list != dip)) {
			pdip = (dev_info_t **)(&DEVI(list)->devi_sibling);
			list = (dev_info_t *)DEVI(list)->devi_sibling;
		}
		if (list == NULL) {
			*pdip = dip;
			DEVI(dip)->devi_sibling = NULL;
		}
		rw_exit(&(devinfo_tree_lock));
	}
}

void
i_ndi_devi_detach_from_parent(dev_info_t *dip)
{
	struct dev_info *devi = DEVI(dip);
	struct dev_info *parent = devi->devi_parent;
	dev_info_t **pdip, *list;

	rw_enter(&(devinfo_tree_lock), RW_WRITER);
	pdip = (dev_info_t **)(&DEVI(parent)->devi_child);
	list = *pdip;
	while (list) {
		if (list == dip) {
			*pdip = (dev_info_t *)(DEVI(dip)->devi_sibling);
			break;
		}
		pdip = (dev_info_t **)(&DEVI(list)->devi_sibling);
		list = (dev_info_t *)DEVI(list)->devi_sibling;
	}
	DEVI(dip)->devi_sibling = NULL;
	rw_exit(&(devinfo_tree_lock));
}

/*
 * Allocate and initialize a new dev_info structure.
 *
 * This routine may be called at interrupt time by a nexus in
 * response to a hotplug event, therefore memory allocations are
 * not allowed to sleep.
 */
int
ndi_devi_alloc(dev_info_t *parent, char *node_name, dnode_t nodeid,
    dev_info_t **ret_dip)
{
	ASSERT(node_name != NULL);
	ASSERT(ret_dip != NULL);

	*ret_dip = i_ndi_devi_alloc(parent, node_name, nodeid,
							NDI_NOSLEEP);
	if (*ret_dip == NULL) {
		return (NDI_NOMEM);
	}

	return (NDI_SUCCESS);
}

/*
 * Allocate and initialize a new dev_info structure
 * This routine may sleep and should not be called at interrupt time
 */
void
ndi_devi_alloc_sleep(dev_info_t *parent, char *node_name, dnode_t nodeid,
    dev_info_t **ret_dip)
{
	ASSERT(node_name != NULL);
	ASSERT(ret_dip != NULL);

	*ret_dip = i_ndi_devi_alloc(parent, node_name, nodeid,
							NDI_SLEEP);
	ASSERT(*ret_dip);
}

/*
 * Change the node name
 */
/*ARGSUSED2*/
int
ndi_devi_set_nodename(dev_info_t *dip, char *name, int flags)
{
	char *nname, *oname;

	if ((dip == NULL) || (name == NULL) || (DDI_CF1(dip))) {
		return (NDI_FAILURE);
	}

	nname = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(nname, name);
	oname = DEVI(dip)->devi_node_name;

	rw_enter(&(devinfo_tree_lock), RW_WRITER);
	DEVI(dip)->devi_node_name = nname;
	i_ddi_set_binding_name(dip, nname);
	rw_exit(&(devinfo_tree_lock));

	kmem_free(oname, strlen(oname) + 1);
	return (NDI_SUCCESS);
}

/*
 * Remove an initialized (but not yet attached) dev_info
 * node from it's parent.
 */
int
ndi_devi_free(dev_info_t *dip)
{
	uint_t circular_count;

	ASSERT(dip != NULL);

	if (DDI_CF1(dip)) {
		return (NDI_FAILURE);
	}

	NDI_CONFIG_DEBUG((CE_CONT, "ndi_devi_free: %s%d (%p)\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip));

	i_ndi_block_device_tree_changes(&circular_count);
	(void) ddi_remove_child(dip, 0);
	i_ndi_allow_device_tree_changes(circular_count);

	return (NDI_SUCCESS);
}

/*
 * bind an initialized dev_info node to a device driver
 */
/* ARGSUSED1 */
static int
i_ndi_devi_bind_driver(dev_info_t *dip, uint_t flags)
{
	major_t major;

	/*
	 * If the device node has a "compatible" property, cache
	 * the strings from the property list in the devinfo
	 * node for use by i_ddi_bind_node_to_driver().
	 */
	if (i_ndi_lookup_compatible(dip) != NDI_SUCCESS) {
		return (NDI_FAILURE);
	}

	/*
	 * PSEUDO nodes are required to have a driver binding.	Return
	 * a failure if a PSEUDO node fails to bind to a driver.
	 * Hardware nodes get placed on the orphan list if they fail
	 * to bind, in hopes a driver will be added.
	 */
	major = i_ddi_bind_node_to_driver(dip);
	if (major == (major_t)-1) {
		if (ndi_dev_is_persistent_node(dip) == 0)
			return (NDI_FAILURE);
		DEVI(dip)->devi_next = NULL;
		ddi_orphan_devs(dip);
		return (NDI_UNBOUND);
	}
	DEVI_SET_BOUND(dip);

	NDI_CONFIG_DEBUG((CE_CONT,
	    "i_ndi_devi_bind_driver: %s%d (%p), flags=%d major=%d\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip,
	    flags,  major));

	return (NDI_SUCCESS);
}

/*
 * report device status
 */
static void
i_ndi_devi_report_status_change(dev_info_t *dip, char *path)
{
	char *status;

	if (!DEVI_NEED_REPORT(dip) || !(DDI_CF1(dip))) {
		return;
	}


	if (DEVI_IS_DEVICE_OFFLINE(dip)) {
		status = "offline";
	} else if (DEVI_IS_DEVICE_DOWN(dip)) {
		status = "down";
	} else if (DEVI_IS_BUS_QUIESCED(dip)) {
		status = "quiesced";
	} else if (DEVI_IS_BUS_DOWN(dip)) {
		status = "down";
	} else if (DDI_CF2(dip)) {
		status = "online";
	} else {
		status = "unknown";
	}

	if (path == NULL) {
		path = kmem_alloc(MAXPATHLEN, KM_SLEEP);
		cmn_err(CE_CONT, "?%s (%s%d) %s\n",
			ddi_pathname(dip, path), ddi_driver_name(dip),
			ddi_get_instance(dip), status);
		kmem_free(path, MAXPATHLEN);
	} else {
		cmn_err(CE_CONT, "?%s (%s%d) %s\n",
			path, ddi_driver_name(dip),
			ddi_get_instance(dip), status);
	}

	DEVI_REPORT_DONE(dip);
}

/*
 * log a notification that a new dev_info node has been added
 * to the device tree
 */
static int
i_log_devfs_add_devinfo(dev_info_t *dip)
{
	log_event_tuple_t	tuples[4];
	int			argc = 0;
	int			rv;
	char *pathname;

	pathname = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	tuples[argc].attr = LOGEVENT_CLASS;
	tuples[argc++].val = EC_DEVFS;
	tuples[argc].attr = LOGEVENT_TYPE;
	tuples[argc++].val = ET_DEVFS_DEVI_ADD;
	tuples[argc].attr = DEVFS_PATHNAME;
	(void) ddi_pathname(dip, pathname);
	ASSERT(strlen(pathname));
	tuples[argc++].val = pathname;

	if ((rv = i_ddi_log_event(argc, tuples, KM_SLEEP)) != DDI_SUCCESS) {
		cmn_err(CE_WARN,
			"i_log_devfs_add_devinfo: failed log_event");
	}

	kmem_free(pathname, MAXPATHLEN);
	return (rv);
}

static int
i_log_devfs_remove_devinfo(char *pathname)
{
	log_event_tuple_t	tuples[4];
	int			argc = 0;
	int			rv;
	tuples[argc].attr = LOGEVENT_CLASS;
	tuples[argc++].val = EC_DEVFS;
	tuples[argc].attr = LOGEVENT_TYPE;
	tuples[argc++].val = ET_DEVFS_DEVI_REMOVE;
	tuples[argc].attr = DEVFS_PATHNAME;
	ASSERT(strlen(pathname));
	tuples[argc++].val = pathname;
	if ((rv = i_ddi_log_event(argc, tuples, KM_SLEEP)) != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "i_log_devfs_remove_devinfo: failed log_event");
	}
	return (rv);
}

/*
 * synchronous attach
 */
static int
i_ndi_devi_attach_node(struct devnames *dnp, dev_info_t *dip, uint_t flags)
{
	int listcnt, major, is_persistent;
	struct dev_ops *devops;

	major = ddi_name_to_major(dnp->dn_name);
	ASSERT(major != -1);

	NDI_CONFIG_DEBUG((CE_CONT,
	    "i_ndi_devi_attach_node: %s%d (%p), flags=%x\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip, flags));

	/*
	 * remember if this is a persistent node, the framework
	 * automatically removes non-persistent nodes which fail initchild
	 */
	is_persistent = ndi_dev_is_persistent_node(dip);

	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &listcnt);

	if (DEVI_IS_IN_RECONFIG(dip) || DDI_CF2(dip)) {
		e_ddi_exit_driver_list(dnp, listcnt);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		if (DDI_CF2(dip)) {
			return (NDI_SUCCESS);
		}
		return (NDI_BUSY);
	}

	DEVI_SET_ONLINING(dip);

	/*
	 * use ddi_hold_installed_driver() to load the driver and
	 * attach all HW and .conf nodes the first time
	 * It is possible that a driver is installed but all device
	 * nodes have been destroyed and therefore we also need to
	 * test on CB_DRV_INSTALLED
	 */
	if (((dnp->dn_flags & DN_WALKED_TREE) == 0) &&
	    (!CB_DRV_INSTALLED(devopsp[major]))) {
		ASSERT(is_persistent);
		DEVI(dip)->devi_next = NULL;
		ddi_orphan_devs(dip);
		e_ddi_exit_driver_list(dnp, listcnt);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		if (ddi_hold_installed_driver(major) == NULL) {
			/* driver failed to attach any device nodes */
			DEVI_CLR_ONLINING(dip);
			i_ndi_devi_migrate(dnp, dip);
			return (NDI_FAILURE);
		}
		if (!DDI_CF2(dip)) {
			/* this node failed to attach */
			(void) ddi_uninitchild(dip);
			LOCK_DEV_OPS(&(dnp->dn_lock));
			DEVI_CLR_ONLINING(dip);
			DECR_DEV_OPS_REF(devopsp[major]);
			UNLOCK_DEV_OPS(&(dnp->dn_lock));
			return (NDI_FAILURE);
		}
	} else {
		/*
		 * driver is already loaded with instances attached
		 * insert the node in the per-driver list and attach it.
		 */
		INCR_DEV_OPS_REF(devopsp[major]);
		i_ndi_insert_ordered_devinfo(dnp, dip);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		if (impl_proto_to_cf2(dip) != DDI_SUCCESS) {
			/*
			 * uninitialize the node to permit ndi_devi_free()
			 * to remove it.
			 *
			 * NB: If the node is not persistent, it's already been
			 * removed by the framework, so be careful not to
			 * dereference it in that case.
			 */
			if (is_persistent) {
				/*
				 * if this is hardware node, clear
				 * the flag and uninitialize it
				 */
				DEVI_CLR_ONLINING(dip);
				(void) ddi_uninitchild(dip);
			}
			LOCK_DEV_OPS(&(dnp->dn_lock));
			DECR_DEV_OPS_REF(devopsp[major]);
			e_ddi_exit_driver_list(dnp, listcnt);
			UNLOCK_DEV_OPS(&(dnp->dn_lock));
			return (NDI_FAILURE);
		}
		LOCK_DEV_OPS(&(dnp->dn_lock));
		DECR_DEV_OPS_REF(devopsp[major]);
		e_ddi_exit_driver_list(dnp, listcnt);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
	}

	devops = devopsp[major];
	ASSERT(DDI_CF2(dip));
	ASSERT(CB_DRV_INSTALLED(devops));

	i_ndi_devi_report_status_change(dip, NULL);

	(void) i_log_devfs_add_devinfo(dip);

	if ((flags & NDI_CONFIG) || DEVI_NEED_NDI_CONFIG(dip)) {
		ASSERT(DEVI_IS_ONLINING(dip));
		i_ndi_devi_config(dnp, dip, flags);
	}

	LOCK_DEV_OPS(&(dnp->dn_lock));
	DEVI_CLR_ONLINING(dip);
	DECR_DEV_OPS_REF(devopsp[major]);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	return (NDI_SUCCESS);
}

/*
 * report status change on each child in the per driver list
 */
static void
i_ndi_log_hot_attach_child(struct devnames *child)
{
	dev_info_t *dip;
	int listcnt;

	LOCK_DEV_OPS(&child->dn_lock);
	e_ddi_enter_driver_list(child, &listcnt);
	UNLOCK_DEV_OPS(&child->dn_lock);

	for (dip = child->dn_head; dip != NULL; ) {
		if (DDI_CF2(dip)) {
			DEVI(dip)->devi_state |= DEVI_S_REPORT;
			i_ndi_devi_report_status_change(dip, NULL);
		}
		dip = (dev_info_t *)DEVI(dip)->devi_next;
	}

	LOCK_DEV_OPS(&child->dn_lock);
	e_ddi_exit_driver_list(child, listcnt);
	UNLOCK_DEV_OPS(&child->dn_lock);
}

/*
 * post-attach configuration of hot-attached nodes
 * (called by devinfo driver after loading a driver)
 */
void
i_ndi_devi_config_by_major(major_t major)
{
	uint_t circular_count;
	struct devnames *dnp;
	int flags = NDI_ONLINE_ATTACH | NDI_CONFIG | NDI_DEVI_PERSIST;
	dev_info_t *dip;
	struct dev_ops	*my_ops;

	ASSERT(major != (major_t)-1);

	/*
	 * only autoconfigure hotplug-capable nexus drivers
	 */
	my_ops = devopsp[major];
	if (!NEXUS_DRV(my_ops)) {
		return;
	}

	dnp = &(devnamesp[major]);

	i_ndi_block_device_tree_changes(&circular_count);

	dip = dnp->dn_head;
	while (dip) {
		/*
		 * only attempt to configure nodes which are attached
		 * but have no pre-existing initialized children
		 */
		if (DDI_CF2(dip) &&
		    i_ndi_devi_has_initialized_children(dip) == 0)
			i_ndi_devi_config(dnp, dip, flags);
		dip = (dev_info_t *)(DEVI(dip)->devi_next);
	}
	i_ndi_allow_device_tree_changes(circular_count);
}

/*
 * called by nexus drivers to configure/unconfigure its children
 */
/* ARGSUSED */
int
ndi_devi_config(dev_info_t *dip, int flags)
{
	major_t major;
	uint_t circular_count;
	struct devnames *dnp;

	major = ddi_name_to_major(ddi_binding_name(dip));
	ASSERT(major != (major_t)-1);
	dnp = &devnamesp[major];

	i_ndi_block_device_tree_changes(&circular_count);
	i_ndi_devi_config(dnp, dip, flags);
	i_ndi_allow_device_tree_changes(circular_count);

	return (NDI_SUCCESS);
}

/* ARGSUSED2 */
static void
i_ndi_devi_config(struct devnames *dnp, dev_info_t *dip, int flags)
{
	struct dev_ops	*my_ops;
	struct par_list *plist, *pl;
	struct devnames *child;
	int major, my_major;
	dev_info_t *cdip;
	int listcnt;
	char *name;

	my_major = (int)ddi_name_to_major(dnp->dn_name);
	ASSERT(my_major != -1);

	my_ops = devopsp[my_major];
	ASSERT(CB_DRV_INSTALLED(my_ops));

	/*
	 * only autoconfigure hotplug-capable nexus drivers
	 */
	if (!NEXUS_DRV(my_ops)) {
		return;
	}

	NDI_CONFIG_DEBUG((CE_CONT,
	    "i_ndi_devi_config: %s%d (%p), flags=%x\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip, flags));

	/*
	 * attach any child HW nodes that have been bound to this
	 * parent node.
	 */
	cdip = ddi_get_child(dip);
	while (cdip) {
		/*
		 * Do not attach offlined nodes if NDI_DEVI_PERSIST
		 * is set.
		 */
		if (((flags & NDI_DEVI_PERSIST) == 0) ||
		    !DEVI_IS_DEVICE_OFFLINE(cdip)) {
			(void) ndi_devi_online(cdip, flags);
		}
		cdip = ddi_get_next_sibling(cdip);
	}

	/*
	 * do not attempt to read .conf files during lights out period
	 */
	if (!bootops && !modrootloaded) {
		return;
	}

	/*
	 * scan the set of .conf generated nodes looking for any
	 * nodes that name this instance as a possible parent.
	 */
	for (major = 0; major < devcnt; major++) {
		if ((name = devnamesp[major].dn_name) == NULL || *name == 0)
			continue;

		child = &devnamesp[major];
		if (child == dnp) {
			continue;
		}


		/* single thread driver configuration */
		LOCK_DEV_OPS(&child->dn_lock);
		e_ddi_enter_driver_list(child, &listcnt);
		UNLOCK_DEV_OPS(&child->dn_lock);

		/* read the .conf file data for driver "major", if any */
		plist = impl_make_parlist(major);
		if (plist == NULL) {
			LOCK_DEV_OPS(&child->dn_lock);
			e_ddi_exit_driver_list(child, listcnt);
			UNLOCK_DEV_OPS(&child->dn_lock);
			continue;
		}

		/*
		 * walk the .conf spec nodes, looking for entries that
		 * name this bus nexus as it's parent node
		 */
		for (pl = plist; pl != NULL; pl = pl->par_next) {
			if (i_ndi_check_pl_list(dip, pl, child, major,
			    my_major, &listcnt, flags)) {
				break;
			}
		}
		LOCK_DEV_OPS(&child->dn_lock);
		e_ddi_exit_driver_list(child, listcnt);
		UNLOCK_DEV_OPS(&child->dn_lock);
	}
}


static int
i_ndi_check_pl_list(dev_info_t *dip, struct par_list *pl,
    struct devnames *child, major_t major, major_t my_major,
    int *listcnt, int flags)
{
	int done = 0;

	/*
	 * if this contains a class spec, replicate
	 * the hwc_specs for each potential parent
	 */
	if (pl->par_major == (major_t)-1) {
		struct par_list *ppl;
		struct par_list *new_pl = impl_replicate_class_spec(pl);

		for (ppl = new_pl; ppl != NULL; ppl = ppl->par_next) {
			if (ppl->par_major != my_major) {
				continue;
			}
			done = i_ndi_attach_spec_children(dip, ppl,
			    child, major, my_major, listcnt, flags);
			if (done) {
				break;
			}
		}
		impl_delete_par_list(new_pl);
		if (done) {
			/* we are done */
			return (done);
		}

	} else if (pl->par_major == my_major) {
		done = i_ndi_attach_spec_children(dip, pl, child, major,
		    my_major, listcnt, flags);
	}

	return (done);
}


/*
 * If the child's driver has not been loaded yet, load it
 * using ddi_hold_installed_driver().
 * Otherwise, walk thru all hwc specs, match the parent, and
 * if there is a match, make children from the spec
 */
static int
i_ndi_attach_spec_children(dev_info_t *dip, struct par_list *pl,
    struct devnames *child, major_t major, major_t my_major,
    int *listcnt, int flags)
{
	struct hwc_spec *specp;

	/*
	 * call ddi_hold_installed_driver() to load
	 * the driver and attach all nodes
	 */
	LOCK_DEV_OPS(&child->dn_lock);
	if ((child->dn_flags & DN_WALKED_TREE) == 0) {
		e_ddi_exit_driver_list(child, *listcnt);
		UNLOCK_DEV_OPS(&child->dn_lock);
		if (ddi_hold_installed_driver(major) !=
		    NULL) {
			i_ndi_log_hot_attach_child(child);
			ddi_rele_driver(major);
		}
		/* done with this driver */
		LOCK_DEV_OPS(&child->dn_lock);
		e_ddi_enter_driver_list(child, listcnt);
		UNLOCK_DEV_OPS(&child->dn_lock);

		/* we are done */
		return (1);
	}
	UNLOCK_DEV_OPS(&child->dn_lock);

	/*
	 * the driver is loaded with instances attached
	 * scan the parlist, attached any .conf spec
	 * dev_info nodes that match this specific
	 * parent node
	 */
	for (specp = pl->par_specs; specp != NULL;
	    specp = specp->hwc_next) {
		if (match_parent(dip, devnamesp[my_major].dn_name,
		    specp->hwc_parent_name))  {
			i_ndi_makechild_from_spec(dip, specp, flags);
		}
	}

	return (0);
}

/*
 * create and attach a dev_info node from a .conf file spec
 */
/*ARGSUSED2*/
static void
i_ndi_makechild_from_spec(dev_info_t *pdip, struct hwc_spec *specp,
	uint_t flags)
{
	dev_info_t *dip;
	char *node_name;

	if (((node_name = specp->hwc_proto->proto_devi_name) == NULL) ||
	    (ddi_name_to_major(node_name) == (major_t)-1)) {
		cmn_err(CE_CONT,
		    "i_ndi_makechild_from_spec: parent=%s, bad spec (%s)\n",
		    ddi_node_name(pdip),
		    ((node_name == NULL) ? "<none>": node_name));
		return;
	}

	dip = i_ndi_devi_alloc(pdip, node_name, (dnode_t)DEVI_PSEUDO_NODEID,
	    NDI_SLEEP);

	mutex_enter(&(DEVI(dip)->devi_lock));
	copy_prop(specp->hwc_proto->proto_devi_sys_prop_ptr,
		    &(DEVI(dip)->devi_sys_prop_ptr));
	mutex_exit(&(DEVI(dip)->devi_lock));

	if (impl_check_cpu(dip) != DDI_SUCCESS) {
		(void) ndi_devi_free(dip);
		return;
	}

	(void) ndi_devi_online(dip, NDI_ONLINE_ATTACH|NDI_CONFIG);
}

/* ARGSUSED */
int
ndi_devi_unconfig(dev_info_t *dip, int flags)
{
	int rv = NDI_SUCCESS;
	dev_info_t *child;
	uint_t circular_count;

	i_ndi_block_device_tree_changes(&circular_count);

	child = ddi_get_child(dip);
	while (child) {
		dev_info_t *next = ddi_get_next_sibling(child);
		(void) ndi_devi_offline(child, flags);
		child = next;
	}
	if (ddi_get_child(dip) != NULL) {
		rv = NDI_FAILURE;
	}

	i_ndi_allow_device_tree_changes(circular_count);

	return (rv);
}

/*
 * pre-detach unconfiguration for bus nexus device nodes
 */
static int
i_ndi_devi_unconfig(struct devnames *dnp, dev_info_t *dip, int flags)
{
	struct dev_ops *my_ops;
	int my_major;
	struct unconfig_ctl unc_ctl;

	NDI_CONFIG_DEBUG((CE_CONT,
	    "i_ndi_devi_unconfig: %s%d (%p), flags=%x\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip, flags));

	/*
	 * only unconfigure hotplug-capable nexus drivers
	 */
	my_major = (int)ddi_name_to_major(dnp->dn_name);
	if (my_major == -1) {
		return (NDI_SUCCESS);
	}

	my_ops = devopsp[my_major];
	if (!NEXUS_DRV(my_ops)) {
		return (NDI_SUCCESS);
	}

	unc_ctl.error = NDI_SUCCESS;
	unc_ctl.flags = flags | NDI_UNCONFIG;
	unc_ctl.root_dip = dip;

	(void) i_ndi_walkdevs_bottom_up(dip, i_ndi_devi_offline_node, &unc_ctl);

	if (i_ndi_devi_has_initialized_children(dip)) {
		return (NDI_FAILURE);
	}

	return (NDI_SUCCESS);
}

/*
 * scan the list of children of this dev_info node, returning an
 * TRUE if any of the attached children are initialized (CF1).
 */
static int
i_ndi_devi_has_initialized_children(dev_info_t *dip)
{

	struct dev_info *child;
	int rv = 0;

	rw_enter(&(devinfo_tree_lock), RW_READER);
	if ((child = DEVI(dip)->devi_child) != NULL) {
		while (child->devi_sibling != NULL) {
			if (DDI_CF1(child))
				break;
			child = child->devi_sibling;
		}
		if (DDI_CF1(child))
			rv = 1;
	}
	rw_exit(&(devinfo_tree_lock));
	return (rv);
}

/*
 * This routine traverses the tree of dev_info nodes bottom-up,
 * starting from the end node, and calls the given function for each
 * node that it finds with the current node and the pointer arg (which
 * can point to a structure of information that the function
 * needs) as arguments.
 *
 * The routine can deal with functions removing the child node by the
 * fact that ddi_remove_child() will link parent->devi_child to the
 * siblings if one exists.
 */
static int
i_ndi_walkdevs_bottom_up(dev_info_t *dev, int (*f)(dev_info_t *, void *),
    void *arg)
{
	dev_info_t *lw = dev;
	dev_info_t *parent = dev;
	dev_info_t *start = dev;
	dev_info_t *sib = NULL;
	int rv;
	static int i_walk_sibs(dev_info_t *, int (*f)(dev_info_t *, void *),
	    void *);

	if (lw == NULL)
		return (DDI_WALK_CONTINUE);

	/* Find the last child */
	while (ddi_get_child(lw) != NULL) {
		lw = ddi_get_child(lw);
	}

	/* lw points to last child at this point */
	while (lw != NULL) {
		/*
		 * If we are at that start of this tree, execute and return.
		 */
		if (lw == start) {
			return ((*f)(lw, arg));
		}

		/*
		 * save the parent & sibling pointers
		 */
		parent = ddi_get_parent(lw);
		sib = ddi_get_next_sibling(lw);

		/* execute the user-callback */
		rv = (*f)(lw, arg);
		if (rv == DDI_WALK_TERMINATE) {
			return (rv);
		}

		/*
		 * If lw is removed by (*f)(), parent->devi_child
		 * is pointing to the sibling.	If lw had no sibling,
		 * we move up to the parent level and continue.
		 */
		if (sib == NULL) {
			lw = parent;
			continue;
		}

		/*
		 * Continue with the siblings before we go back up to
		 * our parent. When all sibling nodes and their children
		 * are done, then we can go back to our parent node.
		 */
		if (i_walk_sibs(sib, f, arg) == DDI_WALK_TERMINATE) {
			return (DDI_WALK_TERMINATE);
		}

		lw = parent;
	}
	return (DDI_WALK_CONTINUE);
}

static int
i_walk_sibs(dev_info_t *dev, int (*f)(dev_info_t *, void *), void *arg)
{
	dev_info_t *dip, *sib;

	dip = sib = dev;

	while (dip != NULL) {
		sib = ddi_get_next_sibling(dip);
		if (i_ndi_walkdevs_bottom_up(dip, f, arg) ==
		    DDI_WALK_TERMINATE) {
			return (DDI_WALK_TERMINATE);
		}
		dip = sib;
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * Called by i_ndi_walkdevs_bottom_up() to offline a dev_info node at the
 * "bottom" of the device tree.  If the request to offline the node
 * fails, terminate the tree walking by returning TERMINATE.
 */
static int
i_ndi_devi_offline_node(dev_info_t *dip, void *arg)
{
	struct unconfig_ctl *handle = (struct unconfig_ctl *)arg;
	int err = 0;

	if (dip == handle->root_dip)
		return (DDI_WALK_TERMINATE);

	/*
	 * ignore errors and continue offlining so all children
	 * that can be offlined, are offlined
	 */
	err =  i_ndi_devi_offline(dip, handle->flags);
	if (err != NDI_SUCCESS) {
		handle->error = err;
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * Called by i_ndi_walkdevs_bottom_up() to remove a dev_info node at the
 * "bottom" of the device tree.  If the request to offline the node
 * fails, terminate the tree walking by returning TERMINATE.
 */
static int
i_ndi_devi_remove_node(dev_info_t *dip, void *arg)
{
	struct unconfig_ctl *handle = (struct unconfig_ctl *)arg;
	int err = 0;

	if (dip == handle->root_dip) {
		return (DDI_WALK_TERMINATE);
	}

	if (DDI_CF1(dip)) {
		return (DDI_WALK_TERMINATE);
	}

	err = ddi_remove_child(dip, 0);
	if (err != DDI_SUCCESS) {
		handle->error = NDI_FAILURE;
		return (DDI_WALK_TERMINATE);
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * place the devinfo in the ONLINE state, allowing e_ddi_deferred_attach
 * requests to re-attach the device instance.
 */
int
ndi_devi_online(dev_info_t *dip, uint_t flags)
{
	major_t	major;
	struct	devnames *dnp;
	int rv = NDI_SUCCESS;
	uint_t circular_count;

	ASSERT(dip);

	NDI_CONFIG_DEBUG((CE_CONT,
		"ndi_devi_online: %s%d (%p) flags=%x\n",
		ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip,
		flags));

	if (DEVI_NEEDS_BINDING(dip)) {
		i_ndi_devi_attach_to_parent(dip);
		rv = i_ndi_devi_bind_driver(dip, flags);
		if (rv != NDI_SUCCESS) {
			if (rv == NDI_UNBOUND) {
				return (NDI_SUCCESS);
			} else {
				return (rv);
			}
		}
	} else {
		ASSERT(DEVI(dip)->devi_binding_name);
	}

	major = ddi_name_to_major(ddi_binding_name(dip));
	if ((major == (major_t)-1) || (flags & NDI_DEVI_BIND)) {
		return (NDI_SUCCESS);
	}

	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&(dnp->dn_lock));
	DEVI_SET_DEVICE_ONLINE(dip);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	if (flags & NDI_CONFIG) {
		DEVI_SET_NDI_CONFIG(dip);
	}

	/*
	 * remove from orphan list. it will be returned if
	 * the driver is not loaded
	 */
	ddi_remove_orphan(dip);

	if ((flags & NDI_ONLINE_ATTACH) == NDI_ONLINE_ATTACH) {
		i_ndi_block_device_tree_changes(&circular_count);
		if (i_ndi_devi_attach_node(dnp, dip, flags | NDI_CONFIG) !=
		    NDI_SUCCESS) {
			rv = NDI_FAILURE;
		}
		i_ndi_allow_device_tree_changes(circular_count);
	} else {
		i_ndi_devi_hotplug_wakeup(dip, major);
	}

	return (rv);
}

/*
 * Take a device node Offline
 * To take a device Offline means to detach the device instance from
 * the driver and prevent deferred attach requests from re-attaching
 * the device instance.
 *
 * The flag NDI_DEVI_REMOVE causes removes the device node from
 * the driver list and the device tree.
 *
 * XXX check interaction with Power Management
 */
int
ndi_devi_offline(dev_info_t *dip, uint_t flags)
{
	int rval;
	uint_t circular_count;

	/*
	 * pause hotplug daemon since we might remove nodes
	 * from the new dev list
	 */
	i_ndi_block_device_tree_changes(&circular_count);

	rval = i_ndi_devi_offline(dip, flags);

	i_ndi_allow_device_tree_changes(circular_count);

	return (rval);
}

static int
i_ndi_devi_offline(dev_info_t *dip, uint_t flags)
{
	struct	devnames *dnp = NULL;
	struct dev_ops *ops;
	major_t major;
	int is_refed, listcnt, did_predetach = 0;
	int rv = NDI_SUCCESS;
	char *pathname = NULL;

	ASSERT(dip != NULL);

	major = (major_t)-1;
	if (DEVI(dip)->devi_binding_name != NULL) {
		major = ddi_name_to_major(ddi_binding_name(dip));
	}

	NDI_CONFIG_DEBUG((CE_CONT,
	    "ndi_devi_offline: %s%d (%p), flags=%x, major=%d\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip,
	    flags, major));

	pathname = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	(void) ddi_pathname(dip, pathname);

	if (major != (major_t)-1) {
		dnp = &(devnamesp[major]);
		LOCK_DEV_OPS(&(dnp->dn_lock));
		e_ddi_enter_driver_list(dnp, &listcnt);
		ops = devopsp[major];
		INCR_DEV_OPS_REF(ops);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));

		/*
		 * detach the driver from this instance, if attached.
		 */
		if (DDI_CF2(dip)) {
			/*
			 * Cause the device node to be auto-unconfigured
			 * so that it will (hopefully) no longer be
			 * referenced when devi_stillreferenced() is called
			 */
			mutex_enter(&dacf_lock);
			(void) dacfc_predetach(dip);
			mutex_exit(&dacf_lock);
			did_predetach = 1;

			/*
			 * verify the device instance is not busy by
			 * asking specfs if there are any minor devices
			 * open who reference this dip.
			 */
			is_refed = devi_stillreferenced(dip);

			/*
			 * device is busy
			 */
			if ((DEVI_IS_IN_RECONFIG(dip)) ||
			    (DEVI_IS_ONLINING(ddi_get_parent(dip))) ||
			    (is_refed == DEVI_REFERENCED)) {
				rv = NDI_BUSY;
				goto done;
			}


			/*
			 * If the open state of the dev_info node is
			 * not known by specfs, only allow the node to
			 * be taken offline if the FORCE flag is
			 * present.
			 */
			if ((is_refed == DEVI_REF_UNKNOWN) &&
			    !(flags & NDI_DEVI_FORCE)) {
				rv = NDI_FAILURE;
				goto done;
			}

			/*
			 * unconfigure nexus drivers by uninitializing
			 * any child dev_info nodes attached to it
			 */
			LOCK_DEV_OPS(&(dnp->dn_lock));
			DEVI_SET_OFFLINING(dip);
			e_ddi_exit_driver_list(dnp, listcnt);
			UNLOCK_DEV_OPS(&(dnp->dn_lock));

			if (NEXUS_DRV(ops) &&
			    ((i_ndi_devi_unconfig(dnp, dip, flags) !=
			    NDI_SUCCESS))) {
				rv = NDI_BUSY;
			}

			LOCK_DEV_OPS(&(dnp->dn_lock));
			e_ddi_enter_driver_list(dnp, &listcnt);
			UNLOCK_DEV_OPS(&(dnp->dn_lock));

			if (rv != NDI_SUCCESS) {
				DEVI_CLR_OFFLINING(dip);
				goto done;
			}

			/*
			 * device is either not referenced, or the
			 * reference is unknown but caller wants to
			 * force the detach.
			 *
			 * We pass DDI_HOTPLUG_DETACH to tell devi_detach
			 * not to try auto-unconfiguration.  We're handling
			 * that ourselves in this routine, and there's no
			 * need to try it twice.
			 */
			if (devi_detach(dip, DDI_HOTPLUG_DETACH) !=
			    DDI_SUCCESS) {
				DEVI_CLR_OFFLINING(dip);
				rv = NDI_FAILURE;
				goto done;
			}
			ddi_set_driver(dip, NULL);  /* back to CF1 */
		}
	}

	DEVI_SET_DEVICE_OFFLINE(dip);

	i_ndi_devi_report_status_change(dip, pathname);

	/*
	 * If the caller has requested that we destroy the device node
	 * call the nexus UNINITCHILD ctlop and remove the node from
	 * the parent node and the per-driver list
	 * Otherwise, just UNINIT the child if UNCONFIG is requested
	 * (this is needed for SCSI)
	 */
	if (flags & (NDI_DEVI_REMOVE | NDI_UNCONFIG)) {
		if (major != (major_t)-1) {
			(void) ddi_uninitchild(dip);
		}
	}

	if (flags & NDI_DEVI_REMOVE) {
		struct unconfig_ctl unc_ctl;

		/*
		 * remove any children left that didn't have a major
		 * number and were not unconfig'ed above because this
		 * dip doesn't have a major number
		 */
		unc_ctl.error = NDI_SUCCESS;
		unc_ctl.flags = 0;
		unc_ctl.root_dip = dip;
		(void) i_ndi_walkdevs_bottom_up(dip,
			i_ndi_devi_remove_node, &unc_ctl);

		if (ddi_remove_child(dip, 0) != DDI_SUCCESS) {
			DEVI_CLR_OFFLINING(dip);
			rv = NDI_BUSY;
		} else if (pathname && strlen(pathname)) {
			(void) i_log_devfs_remove_devinfo(pathname);
		}
	} else {
		DEVI_CLR_OFFLINING(dip);
	}

done:
	if (pathname) {
		kmem_free(pathname, MAXPATHLEN);
	}

	if (did_predetach && (rv != NDI_SUCCESS)) {
		/*
		 * Try to re-autoconfigure the node
		 */
		mutex_enter(&dacf_lock);
		(void) dacfc_postattach(dip);
		mutex_exit(&dacf_lock);
	}

	/*
	 * release the per-driver list if it was entered above
	 */
	if (dnp != NULL) {
		LOCK_DEV_OPS(&(dnp->dn_lock));
		DECR_DEV_OPS_REF(ops);
		e_ddi_exit_driver_list(dnp, listcnt);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
	}

	return (rv);
}

/*
 * insert devinfo node 'dip' into the per-driver instance list
 * headed by 'dnp'
 *
 * Nodes on the per-driver list are ordered: HW - SID - PSEUDO.  The order is
 * required for merging of .conf file data to work properly.
 */
static void
i_ndi_insert_ordered_devinfo(struct devnames *dnp, dev_info_t *dip)
{
	struct dev_info *idip;
	struct dev_info *ndip = NULL;
	static dev_info_t *in_dl(struct devnames *, dev_info_t *);

	ASSERT(mutex_owned(&(dnp->dn_lock)));

	/*
	 * return if the node is already in the list
	 */
	if (in_dl(dnp, dip) == dip)
		return;

	/*
	 * If the list is empty, insert the node at the list head ...
	 */
	if (DEVI(dnp->dn_head) == NULL) {
		dnp->dn_head = (dev_info_t *)dip;
		return;
	}

	idip = DEVI(dnp->dn_head);

	/*
	 * prom nodes go on the beginning of the list ...
	 */
	if (ndi_dev_is_prom_node((dev_info_t *)dip)) {
		/*
		 * If the first node on the list is a non-prom node,
		 * add dip to the beginning of the list ...
		 */
		if (ndi_dev_is_prom_node((dev_info_t *)idip) == 0) {
			DEVI(dip)->devi_next = DEVI(dnp->dn_head);
			dnp->dn_head = dip;
			return;
		}
		/*
		 * Otherwise, insert dip before any non-prom node ...
		 */
		for (; idip->devi_next != NULL; idip = ndip) {
			ndip = DEVI(idip)->devi_next;
			if (ndi_dev_is_prom_node((dev_info_t *)ndip) == 0) {
				DEVI(dip)->devi_next = ndip;
				idip->devi_next = DEVI(dip);
				return;
			}
		}
		/* end of list - append the node */
		idip->devi_next = DEVI(dip);
		return;
	}

	/*
	 * persistent nodes go before non-persistent nodes ...
	 */
	if (ndi_dev_is_persistent_node(dip)) {
		/*
		 * If the first node on the list is a non-persistent node,
		 * add dip to the beginning of the list ...
		 */
		if (ndi_dev_is_persistent_node((dev_info_t *)idip) == 0) {
			DEVI(dip)->devi_next = DEVI(dnp->dn_head);
			dnp->dn_head = dip;
			return;
		}
		/*
		 * Otherwise, insert dip before any non-persistent node ...
		 */
		for (; idip->devi_next != NULL; idip = ndip) {
			ndip = DEVI(idip)->devi_next;
			if (!ndi_dev_is_persistent_node((dev_info_t *)ndip)) {
				DEVI(dip)->devi_next = ndip;
				idip->devi_next = DEVI(dip);
				return;
			}
		}
		/* end of list - append the node */
		idip->devi_next = DEVI(dip);
		return;
	}

	/*
	 * Non-persistent pseudo nodes get appended to the list ...
	 */
	for (; idip->devi_next != NULL; idip = idip->devi_next)
		/* empty */;
	/* end of list - append the node */
	idip->devi_next = DEVI(dip);
}

/*
 * scan the per-driver list looking for dev_info "dip"
 * return itself if already in the list, otherwise NULL
 */
static dev_info_t *
in_dl(struct devnames *dnp, dev_info_t *dip)
{
	struct dev_info *idip;

	if ((idip = DEVI(dnp->dn_head)) == NULL)
		return (NULL);

	while (idip) {
		if (idip == DEVI(dip))
			return (dip);
		idip = idip->devi_next;
	}
	return (NULL);
}

/*
 * Find the child dev_info node of parent nexus 'p' whose name
 * matches "cname@caddr".
 */
dev_info_t *
ndi_devi_find(dev_info_t *pdip, char *cname, char *caddr)
{
	struct dev_info *child, *next = NULL;
	major_t major;
	struct devnames *dnp = NULL;
	int listcnt;
	int found = 0;

	if (pdip == NULL || cname == NULL || caddr == NULL)
		return ((dev_info_t *)NULL);

	rw_enter(&(devinfo_tree_lock), RW_READER);
	next = DEVI(pdip)->devi_child;
	while ((child = next) != NULL) {
		next = child->devi_sibling;

		/*
		 * skip nodes whose node name does not match the name
		 * or who don't have a driver
		 */
		if (((child->devi_node_name != NULL) &&
		    (strcmp(cname, child->devi_node_name) != 0)) ||
			(child->devi_binding_name == NULL)) {
			continue;
		}

		major = ddi_name_to_major(child->devi_binding_name);

		/*
		 * skip nodes that don't have a driver binding
		 */
		if (major == (major_t)-1) {
			continue;
		}

		dnp = &(devnamesp[major]);

		/*
		 * Attempt to lock the per-driver list to prevent the
		 * node from changing underneath us.  If we fail to enter
		 * the per-driver list, drop the device tree lock to avoid
		 * avoid deadlocking with thread holding the per-driver list.
		 */
		LOCK_DEV_OPS(&(dnp->dn_lock));
		if (e_ddi_tryenter_driver_list(dnp, &listcnt) == 0) {
			UNLOCK_DEV_OPS(&(dnp->dn_lock));
			rw_exit(&(devinfo_tree_lock));
			delay(1);
			rw_enter(&(devinfo_tree_lock), RW_READER);
			next = DEVI(pdip)->devi_child;
			continue;
		}
		UNLOCK_DEV_OPS(&(dnp->dn_lock));

		/*
		 * compare the node address with the arg, if they
		 * match, return the node.
		 */
		if (DDI_CF1(child)) {
			if (strcmp(caddr, child->devi_addr) == 0) {
				found++;
			}
		} else {
			char *addr = child->devi_last_addr;
			if ((addr != NULL) && (strcmp(caddr, addr) == 0)) {
				found++;
			}
		}
		LOCK_DEV_OPS(&dnp->dn_lock);
		e_ddi_exit_driver_list(dnp, listcnt);
		UNLOCK_DEV_OPS(&dnp->dn_lock);
		if (found) {
			break;
		}
	}
	rw_exit(&(devinfo_tree_lock));
	return ((dev_info_t *)child);
}

/*
 * Copy in the devctl IOCTL data structure and the strings referenced
 * by the structure.
 *
 * Convenience function for use by nexus drivers as part of the
 * implementation of devctl IOCTL handling.
 */
int
ndi_dc_allochdl(void *iocarg, struct devctl_iocdata **rdcp)
{
	struct devctl_iocdata *dcp;
	char *cpybuf;
	size_t cpylen;

	ASSERT(iocarg != NULL && rdcp != NULL);

	dcp = kmem_alloc(sizeof (*dcp), KM_SLEEP);

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyin(iocarg, dcp, sizeof (*dcp)) != 0) {
			kmem_free(dcp, sizeof (*dcp));
			return (NDI_FAULT);
		}
	}
#ifdef _SYSCALL32_IMPL
	else {
		struct devctl_iocdata32 dcp32;

		if (copyin(iocarg, &dcp32, sizeof (dcp32)) != 0) {
			kmem_free(dcp, sizeof (*dcp));
			return (NDI_FAULT);
		}
		dcp->cmd = (uint_t)dcp32.cmd;
		dcp->dev_path = (char *)dcp32.dev_path;
		dcp->dev_name = (char *)dcp32.dev_name;
		dcp->dev_addr = (char *)dcp32.dev_addr;
		dcp->dev_minor = (char *)dcp32.dev_minor;
		dcp->ret_state = (uint_t *)dcp32.ret_state;
	}
#endif

	/*
	 * copy in the full "/devices" pathname
	 */
	if (dcp->dev_path != NULL) {
		cpybuf = kmem_alloc(MAXPATHLEN, KM_SLEEP);
		if (copyinstr(dcp->dev_path, cpybuf, MAXPATHLEN,
		    &cpylen) != 0) {
			kmem_free(cpybuf, MAXPATHLEN);
			kmem_free(dcp, sizeof (*dcp));
			return (NDI_FAULT);
		}
		dcp->dev_path = cpybuf;
	}

	/*
	 * copy in the child device node name (devi_node_name)
	 */
	if (dcp->dev_name != NULL) {
		cpybuf = kmem_alloc(MAXNAMELEN, KM_SLEEP);
		if (copyinstr(dcp->dev_name, cpybuf, MAXNAMELEN,
		    &cpylen) != 0) {
			if (dcp->dev_path != NULL)
				kmem_free(dcp->dev_path, MAXPATHLEN);
			kmem_free(cpybuf, MAXNAMELEN);
			kmem_free(dcp, sizeof (*dcp));
			return (NDI_FAULT);
		}
		dcp->dev_name = cpybuf;
	}

	/*
	 * copy in the child device node address
	 */
	if (dcp->dev_addr != NULL) {
		cpybuf = kmem_alloc(MAXNAMELEN, KM_SLEEP);
		if (copyinstr(dcp->dev_addr, cpybuf, MAXNAMELEN,
		    &cpylen) != 0) {
			kmem_free(cpybuf, MAXNAMELEN);
			if (dcp->dev_path != NULL)
				kmem_free(dcp->dev_path, MAXPATHLEN);
			if (dcp->dev_name != NULL)
				kmem_free(dcp->dev_name, MAXNAMELEN);
			kmem_free(dcp, sizeof (*dcp));
			return (NDI_FAULT);
		}
		dcp->dev_addr = cpybuf;
	}

	/*
	 * copy in the child device minor name spec
	 */
	if (dcp->dev_minor != NULL) {
		cpybuf = kmem_alloc(MAXNAMELEN, KM_SLEEP);
		if (copyinstr(dcp->dev_minor, cpybuf, MAXNAMELEN,
		    &cpylen) != 0) {
			kmem_free(cpybuf, MAXNAMELEN);
			if (dcp->dev_path != NULL)
				kmem_free(dcp->dev_path, MAXPATHLEN);
			if (dcp->dev_name != NULL)
				kmem_free(dcp->dev_name, MAXNAMELEN);
			if (dcp->dev_addr != NULL)
				kmem_free(dcp->dev_addr, MAXNAMELEN);
			kmem_free(dcp, sizeof (*dcp));
			return (NDI_FAULT);
		}
		dcp->dev_minor = cpybuf;
	}

	*rdcp = dcp;
	return (NDI_SUCCESS);
}

/*
 * free the structure previously allocated by ndi_dc_allochdl.
 */
void
ndi_dc_freehdl(struct devctl_iocdata *dcp)
{
	ASSERT(dcp != NULL);

	if (dcp->dev_path != NULL)
		kmem_free(dcp->dev_path, MAXPATHLEN);
	if (dcp->dev_name != NULL)
		kmem_free(dcp->dev_name, MAXNAMELEN);
	if (dcp->dev_addr != NULL)
		kmem_free(dcp->dev_addr, MAXNAMELEN);
	if (dcp->dev_minor != NULL)
		kmem_free(dcp->dev_minor, MAXNAMELEN);
	kmem_free(dcp, sizeof (*dcp));
}

char *
ndi_dc_getpath(struct devctl_iocdata *dcp)
{
	ASSERT(dcp != NULL);
	return (dcp->dev_path);
}

char *
ndi_dc_getname(struct devctl_iocdata *dcp)
{
	ASSERT(dcp != NULL);
	return (dcp->dev_name);
}

char *
ndi_dc_getaddr(struct devctl_iocdata *dcp)
{
	ASSERT(dcp != NULL);
	return (dcp->dev_addr);
}

char *
ndi_dc_getminorname(struct devctl_iocdata *dcp)
{
	ASSERT(dcp != NULL);
	return (dcp->dev_minor);
}

/*
 * return the current state of the device "dip"
 */
int
ndi_dc_return_dev_state(dev_info_t *dip, struct devctl_iocdata *dcp)
{
	uint_t devstate = 0;
	struct devnames *dnp;
	major_t maj;
	int listcnt;

	if ((dip == NULL) || (dcp == NULL) || (ddi_binding_name(dip) == NULL))
		return (NDI_FAILURE);

	maj = ddi_name_to_major(ddi_binding_name(dip));

	if (maj == (major_t)-1)
		return (NDI_FAILURE);

	dnp = &(devnamesp[maj]);
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
	mutex_enter(&(DEVI(dip)->devi_lock));
	if (DEVI_IS_DEVICE_OFFLINE(dip)) {
		devstate = DEVICE_OFFLINE;
	} else {
		if (DEVI_IS_DEVICE_DOWN(dip)) {
			devstate = DEVICE_DOWN;
		} else {
			devstate = DEVICE_ONLINE;
			if (devi_stillreferenced(dip) == DEVI_REFERENCED)
				devstate |= DEVICE_BUSY;
		}
	}
	mutex_exit(&(DEVI(dip)->devi_lock));
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_exit_driver_list(dnp, listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	if (copyout(&devstate, dcp->ret_state, sizeof (uint_t)) != 0)
		return (NDI_FAULT);

	return (NDI_SUCCESS);
}

/*
 * Copyout the state of the Attachment Point "ap" to the requesting
 * user process.
 */
int
ndi_dc_return_ap_state(devctl_ap_state_t *ap, struct devctl_iocdata *dcp)
{
	if ((ap == NULL) || (dcp == NULL))
		return (NDI_FAILURE);


	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyout(ap, dcp->ret_state,
			sizeof (devctl_ap_state_t)) != 0)
		    return (NDI_FAULT);
	}
#ifdef _SYSCALL32_IMPL
	else {
		struct devctl_ap_state32 ap_state32;

		ap_state32.ap_rstate = ap->ap_rstate;
		ap_state32.ap_ostate = ap->ap_ostate;
		ap_state32.ap_condition = ap->ap_condition;
		ap_state32.ap_error_code = ap->ap_error_code;
		ap_state32.ap_in_transition = ap->ap_in_transition;
		ap_state32.ap_last_change = (time32_t)ap->ap_last_change;
		if (copyout(&ap_state32, dcp->ret_state,
			sizeof (devctl_ap_state32_t)) != 0)
		    return (NDI_FAULT);
	}
#endif

	return (NDI_SUCCESS);
}

/*
 * Copyout the bus state of the bus nexus device "dip" to the requesting
 * user process.
 */
int
ndi_dc_return_bus_state(dev_info_t *dip, struct devctl_iocdata *dcp)
{
	uint_t devstate = 0;

	if ((dip == NULL) || (dcp == NULL))
		return (NDI_FAILURE);

	if (ndi_get_bus_state(dip, &devstate) != NDI_SUCCESS)
		return (NDI_FAILURE);

	if (copyout(&devstate, dcp->ret_state, sizeof (uint_t)) != 0)
		return (NDI_FAULT);

	return (NDI_SUCCESS);
}

/*
 * return current soft bus state of bus nexus "dip"
 */
int
ndi_get_bus_state(dev_info_t *dip, uint_t *rstate)
{
	if (dip == NULL || rstate == NULL)
		return (NDI_FAILURE);

	if (DEVI(dip)->devi_ops->devo_bus_ops == NULL)
		return (NDI_FAILURE);

	mutex_enter(&(DEVI(dip)->devi_lock));
	if (DEVI_IS_BUS_QUIESCED(dip))
		*rstate = BUS_QUIESCED;
	else if (DEVI_IS_BUS_DOWN(dip))
		*rstate = BUS_SHUTDOWN;
	else
		*rstate = BUS_ACTIVE;
	mutex_exit(&(DEVI(dip)->devi_lock));
	return (NDI_SUCCESS);
}

/*
 * Set the soft state of bus nexus "dip"
 */
int
ndi_set_bus_state(dev_info_t *dip, uint_t state)
{
	int rv = NDI_SUCCESS;

	if (dip == NULL)
		return (NDI_FAILURE);

	mutex_enter(&(DEVI(dip)->devi_lock));

	switch (state) {
	case BUS_QUIESCED:
		DEVI_SET_BUS_QUIESCE(dip);
		break;

	case BUS_ACTIVE:
		DEVI_SET_BUS_ACTIVE(dip);
		DEVI_SET_BUS_UP(dip);
		break;

	case BUS_SHUTDOWN:
		DEVI_SET_BUS_DOWN(dip);
		break;

	default:
		rv = NDI_FAILURE;
	}

	mutex_exit(&(DEVI(dip)->devi_lock));
	return (rv);
}

/*
 * Hotplug work thread
 *
 * The process of attaching dev_info nodes is handled by this thread.
 * The dev_info nodes are created by calls to ndi_devi_alloc() and are
 * handed off to this thread to be attached by a call to
 * ndi_devi_online().
 *
 * The thread is started early on during system startup (main.c) and
 * runs for the life of the system
 */
static void hotplug_daemon(void);
extern pri_t minclsyspri;

void
hotplug_daemon_init(void)
{
	sema_init(&hotplug_sema, 1, NULL, SEMA_DEFAULT, NULL);
	cv_init(&hotplug_completion_cv, NULL, CV_DEFAULT, NULL);
}

/*
 * we allow recursive entering of the stable device tree, for example,
 * a nexus driver may call ndi_devi_offline() in its detach function
 */
void
i_ndi_block_device_tree_changes(uint_t *lkcnt)
{
	/*
	 * if the current thread owns the lock, just increment the count
	 */
	mutex_enter(&hotplug_lk);
	if (holding_hotplug_sema == curthread) {
		hotplug_sema_circular++;
	} else {

		mutex_exit(&hotplug_lk);
		sema_p(&hotplug_sema);
		mutex_enter(&hotplug_lk);

		holding_hotplug_sema = curthread;
		ASSERT(hotplug_sema_circular == 0);
	}
	*lkcnt = hotplug_sema_circular;
	mutex_exit(&hotplug_lk);
}

void
i_ndi_allow_device_tree_changes(uint_t lkcnt)
{
	mutex_enter(&hotplug_lk);
	if (lkcnt != 0) {
		hotplug_sema_circular--;
	} else {
		holding_hotplug_sema = NULL;
		ASSERT(hotplug_sema_circular == 0);
		sema_v(&hotplug_sema);
	}
	mutex_exit(&hotplug_lk);
}

/*
 * If the hotplug daemon is running, signal it
 * to scan the per-driver lists and attach any new
 * devinfo nodes.  If it has not yet been started,
 * bump the counter so the thread knows to scan the list
 * once it starts.
 */
static void
i_ndi_devi_hotplug_wakeup(dev_info_t *dip, major_t major)
{
	ndi_online_rqst_t *req = kmem_zalloc(
		sizeof (ndi_online_rqst_t), KM_SLEEP);

	NDI_CONFIG_DEBUG((CE_CONT, "i_ndi_devi_hotplug_wakeup: %s%d (%p)\n",
		ddi_driver_name(dip), ddi_get_instance(dip), (void *)dip));

	req->major = major;
	req->dip = dip;

	mutex_enter(&hotplug_lk);
	if (newdev_list == NULL) {
		newdev_list = req;
	} else {
		ndi_online_rqst_t *r = newdev_list;
		while (r) {
			if (r->next == NULL) {
				r->next = req;
				break;
			}
			r = r->next;
		}
	}

	/*
	 * start or wakeup hotplug daemon
	 */
	if (hotplug_thread == NULL) {
		/* create hotplug daemon */
		if ((hotplug_thread = thread_create(NULL,
		    2 * DEFAULTSTKSZ, (void (*)())hotplug_daemon,
		    0, 0, &p0, TS_RUN, minclsyspri)) == NULL) {
			cmn_err(CE_WARN, "unable to create hotplug daemon");
			cmn_err(CE_WARN, "please retry hotplug operation");
		}
	} else {
		cv_signal(&hotplug_cv);
	}

	mutex_exit(&hotplug_lk);
}

/*
 * i_ndi_devi_hotplug_queue_empty returns whether the hotplug
 * queue is empty or can block until the hotplug queue is empty
 *
 * when probing the bus is time-consuming, it is advantageous
 * to online children asynchronously and concurrently with probing the
 * rest of the children. However, the nexus may still
 * want to block till all children have been attached before terminating
 * its hotplug thread/routine.
 * We cannot sleep indefinitely because in pathological situations the
 * hotplug queue will never be empty or a deadlock situation may occur if
 * the calling nexus driver is offlined by its parent: its detach can not
 * complete because the hotplugging has not completed and the NDI hotplug
 * daemon hangs in ndi_block_device_tree_changes() and cannot empty the
 * hotplug queue
 */
int
i_ndi_devi_hotplug_queue_empty(uint_t sleep, uint_t timeout)
{
	int rval;

	mutex_enter(&hotplug_lk);
	if ((sleep == NDI_SLEEP) && timeout && newdev_list) {
		(void) cv_timedwait(&hotplug_completion_cv, &hotplug_lk,
		    ddi_get_lbolt() + drv_usectohz(timeout * 1000000));
	}
	rval = (newdev_list == NULL);
	mutex_exit(&hotplug_lk);

	return (rval);
}

static void
hotplug_daemon(void)
{
	callb_cpr_t cprinfo;
	uint_t circular_count;
	int rv;
	int done = 0;

	NDI_CONFIG_DEBUG((CE_CONT, "hotplug daemon started\n"));

	/*
	 * Setup the CPR callback for suspend/resume
	 */
	CALLB_CPR_INIT(&cprinfo, &hotplug_lk, callb_generic_cpr, "hotplugd");

	while (!done) {
		i_ndi_block_device_tree_changes(&circular_count);

		rv = i_ndi_attach_new_devinfo((major_t)-1);

		i_ndi_allow_device_tree_changes(circular_count);

		/*
		 * loop until we have no more devinfos to attach
		 * each time through the loop we will process all of the
		 * devinfo nodes on the new dev list, but the process
		 * of attaching nodes may cause other nodes to be created.
		 */
		mutex_enter(&hotplug_lk);
		if (newdev_list == NULL) {
			cv_broadcast(&hotplug_completion_cv);
		}

		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		if (rv) {
			(void) cv_timedwait(&hotplug_cv, &hotplug_lk,
			    ddi_get_lbolt() + drv_usectohz(100000));
		} else {
			while (newdev_list == NULL) {
				/*
				 * wait for work for 60 secs
				 */
				rv =  cv_timedwait(&hotplug_cv, &hotplug_lk,
				    ddi_get_lbolt() +
				    drv_usectohz(hotplug_idle_time * 1000000));
				if (rv == -1) {
					done++;
					break;
				}
			}
		}
		CALLB_CPR_SAFE_END(&cprinfo, &hotplug_lk);
		mutex_exit(&hotplug_lk);
	}

	mutex_enter(&hotplug_lk);

	hotplug_thread = NULL;

	/* this also releases the hotplug lock */
	CALLB_CPR_EXIT(&cprinfo);

	NDI_CONFIG_DEBUG((CE_CONT, "hotplug daemon exit\n"));
}

/*
 * migrate any hardware device nodes from the new devices list to
 * the orphanlist list.  This function is called by the hotplug
 * daemon thread if ddi_hold_installed_driver() fails for some
 * reason to load the driver.  If the driver loads successfully
 * at some point in the future, these nodes will move back to the
 * per-driver list as part of driver loading process.
 */
static void
i_ndi_devi_migrate(struct devnames *dnp, dev_info_t *dip)
{
	int listcnt;

	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	/*
	 * hardware nodes are added to the orphanlist, .conf
	 * nodes are tossed.
	 */
	if (ndi_dev_is_persistent_node(dip)) {
		DEVI(dip)->devi_next = NULL;
		ddi_orphan_devs(dip);
	} else {
		(void) ddi_remove_child(dip, 0);
	}
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_exit_driver_list(dnp, listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
}

/*
 * Attach new dev_info nodes
 * Remove and attach any node from the newdev_list matching the major number
 * unless major is -1 in which case we empty the entire queue
 */
/*ARGSUSED*/
int
i_ndi_attach_new_devinfo(major_t major)
{
	ndi_online_rqst_t *nrq, *prev;
	int attached;
	int cc = 0;

	mutex_enter(&hotplug_lk);
	while (newdev_list) {
		prev = NULL;
		nrq = newdev_list;
		attached = 0;
		do {

			/*
			 * Check if the per-driver list is busy and
			 * exit if we are the hotplug thread.
			 * Otherwise this is called from
			 * ddi_hold_installed_driver to attach any
			 * nodes that have been created for this
			 * driver
			 */
			if (curthread == hotplug_thread) {
				struct devnames *dnp =
					&(devnamesp[nrq->major]);
				LOCK_DEV_OPS(&(dnp->dn_lock));
				if (!e_ddi_tryenter_driver_list(dnp, &cc)) {
					UNLOCK_DEV_OPS(&(dnp->dn_lock));
					mutex_exit(&hotplug_lk);
					return (-1);
				}
				e_ddi_exit_driver_list(dnp, cc);
				UNLOCK_DEV_OPS(&(dnp->dn_lock));
			}

			if ((curthread == hotplug_thread) ||
			    (nrq->major == major)) {
				if (prev == NULL) {
					newdev_list = nrq->next;
				} else {
					prev->next = nrq->next;
				}
				mutex_exit(&hotplug_lk);

				(void) i_ndi_devi_attach_node(
				    &(devnamesp[nrq->major]), nrq->dip,
				    NDI_CONFIG);

				kmem_free(nrq, sizeof (*nrq));
				attached++;

				mutex_enter(&hotplug_lk);

				/*
				 * restart from the beginning of the list
				 * since the list may have changed
				 */
				break;

			} else {
				prev = nrq;
				nrq = nrq->next;
			}
		} while (nrq);

		if (attached == 0) {
			break;
		}
	}
	mutex_exit(&hotplug_lk);
	return (0);
}

/*
 * remove a dip from the new dev list
 */
void
i_ndi_remove_new_devinfo(dev_info_t *dip)
{
	ndi_online_rqst_t *next;
	ndi_online_rqst_t *prev = NULL;

	mutex_enter(&hotplug_lk);
	next = newdev_list;
	while (next) {
		if (next->dip == dip) {
			if (prev == NULL) {
				newdev_list = next->next;
			} else {
				prev->next = next->next;
			}
			kmem_free(next, sizeof (ndi_online_rqst_t));
			break;
		}
		prev = next;
		next = next->next;
	}
	mutex_exit(&hotplug_lk);
}

/*
 * lookup the "compatible" property and cache it's contents in the
 * device node.
 */
static int
i_ndi_lookup_compatible(dev_info_t *dip)
{
	int rv;
	int prop_flags;
	uint_t ncompatstrs;
	char **compatstrpp;
	char *di_compat_strp;
	size_t di_compat_strlen;

	if (DEVI(dip)->devi_compat_names) {
		return (NDI_SUCCESS);
	}

	prop_flags = DDI_PROP_TYPE_STRING | DDI_PROP_DONTSLEEP |
	    DDI_PROP_DONTPASS;

	if (ndi_dev_is_prom_node(dip) == 0) {
		prop_flags |= DDI_PROP_NOTPROM;
	}

	rv = ddi_prop_lookup_common(DDI_DEV_T_ANY, dip, prop_flags,
	    "compatible", &compatstrpp, &ncompatstrs,
	    ddi_prop_fm_decode_strings);

	if (rv == DDI_PROP_NOT_FOUND) {
		return (NDI_SUCCESS);
	}

	if (rv != DDI_PROP_SUCCESS) {
		return (NDI_FAILURE);
	}

	/*
	 * encode the compatible property data in the dev_info node
	 */
	rv = NDI_SUCCESS;
	if (ncompatstrs != 0) {
		di_compat_strp = i_encode_composite_string(compatstrpp,
		    ncompatstrs, &di_compat_strlen);
		if (di_compat_strp != NULL) {
			DEVI(dip)->devi_compat_names = di_compat_strp;
			DEVI(dip)->devi_compat_length = di_compat_strlen;
		} else {
			rv = NDI_FAILURE;
		}
	}
	ddi_prop_free(compatstrpp);
	return (rv);
}

/*
 * Create a composite string from a list of strings.
 *
 * A composite string consists of a single buffer containing one
 * or more NULL terminated strings.
 */
static char *
i_encode_composite_string(char **strings, uint_t nstrings, size_t *retsz)
{
	uint_t index;
	char  **strpp;
	uint_t slen;
	size_t cbuf_sz = 0;
	char *cbuf_p;
	char *cbuf_ip;

	if (strings == NULL || nstrings == 0 || retsz == NULL) {
		return (NULL);
	}

	for (index = 0, strpp = strings; index < nstrings; index++)
		cbuf_sz += strlen(*(strpp++)) + 1;

	if ((cbuf_p = kmem_alloc(cbuf_sz, KM_NOSLEEP)) == NULL) {
		cmn_err(CE_NOTE,
		    "?failed to allocate device node compatstr");
		return (NULL);
	}

	cbuf_ip = cbuf_p;
	for (index = 0, strpp = strings; index < nstrings; index++) {
		slen = strlen(*strpp);
		bcopy(*(strpp++), cbuf_ip, slen);
		cbuf_ip += slen;
		*(cbuf_ip++) = '\0';
	}

	*retsz = cbuf_sz;
	return (cbuf_p);
}

/*
 * Single thread entry into per-driver list
 */
void
e_ddi_enter_driver_list(struct devnames *dnp, int *listcnt)
{
	ASSERT(dnp != NULL);
	ASSERT(mutex_owned(&(dnp->dn_lock)));

	if (dnp->dn_busy_thread == curthread) {
		dnp->dn_circular++;
	} else {
		while (DN_BUSY_CHANGING(dnp->dn_flags))
			cv_wait(&(dnp->dn_wait), &(dnp->dn_lock));
		dnp->dn_flags |= DN_BUSY_LOADING;
		dnp->dn_busy_thread = curthread;
	}
	*listcnt = dnp->dn_circular;
}

/*
 * release the per-driver list
 */
void
e_ddi_exit_driver_list(struct devnames *dnp, int listcnt)
{
	ASSERT(dnp != NULL);
	ASSERT(mutex_owned(&(dnp->dn_lock)));

	if (listcnt != 0) {
		dnp->dn_circular--;
	} else {
		dnp->dn_flags &= ~(DN_BUSY_CHANGING_BITS);
		ASSERT(dnp->dn_busy_thread == curthread);
		dnp->dn_busy_thread = NULL;
		cv_broadcast(&(dnp->dn_wait));
	}
}

/*
 * Attempt to enter driver list
 */
int
e_ddi_tryenter_driver_list(struct devnames *dnp, int *listcnt)
{
	int rval = 1;			/* assume we enter */

	ASSERT(dnp != NULL);
	ASSERT(mutex_owned(&(dnp->dn_lock)));

	if (dnp->dn_busy_thread == curthread) {
		dnp->dn_circular++;
	} else {
		if (!DN_BUSY_CHANGING(dnp->dn_flags)) {
			dnp->dn_flags |= DN_BUSY_LOADING;
			dnp->dn_busy_thread = curthread;
		} else {
			rval = 0;	/* driver list is busy */
		}
	}
	*listcnt = dnp->dn_circular;
	return (rval);
}

/*
 * impl_ddi_merge_child:
 *
 * Framework function to merge .conf file nodes into a specifically
 * named hw devinfo node of the same name_addr, allowing .conf file
 * overriding of hardware properties. May be called from nexi, though the call
 * may result in the given "child" node being uninitialized and subject
 * to subsequent removal.  DDI_FAILURE indicates that the child has
 * been merged into another node, has been uninitialized and should
 * be removed and is not actually a failure, but should be returned
 * to the caller's caller.  DDI_SUCCESS means that the child was not
 * removed and was not merged into a hardware .conf node.
 */
static char *cantmerge = "Cannot merge %s devinfo node %s@%s";

int
impl_ddi_merge_child(dev_info_t *child)
{
	dev_info_t *parent, *och;
	char *name = ddi_get_name_addr(child);

	parent = ddi_get_parent(child);

	/*
	 * If another child already exists by this name,
	 * merge these properties into the other child.
	 *
	 * NOTE - This property override/merging depends on several things:
	 *
	 * 1) That hwconf nodes are 'named' (ddi_initchild()) before prom
	 *	devinfo nodes.
	 *
	 * 2) That ddi_findchild() will call ddi_initchild() for all
	 *	siblings with a matching devo_name field.
	 *
	 * 3) That hwconf devinfo nodes come "after" prom devinfo nodes.
	 *
	 * Then "och" should be a prom node with no attached properties.
	 */
	if ((och = ddi_findchild(parent, ddi_get_name(child), name)) != NULL &&
	    och != child) {
		if ((ndi_dev_is_persistent_node(och) == 0) ||
		    ndi_dev_is_persistent_node(child) ||
		    DEVI(och)->devi_sys_prop_ptr ||
		    DEVI(och)->devi_drv_prop_ptr || DDI_CF2(och)) {
			cmn_err(CE_WARN, cantmerge, "hwconf",
			    ddi_get_name(child), name);
			/*
			 * Do an extra hold on the parent,
			 * to compensate for ddi_uninitchild's
			 * extra release of the parent. [ plus the
			 * release done by returning "failure" from
			 * this function. ]
			 */
			(void) ddi_hold_devi(parent);
			(void) ddi_uninitchild(child);
			return (DDI_FAILURE);
		}
		/*
		 * Move "child"'s properties to "och." and allow the node
		 * to be init-ed (this handles 'reg' and 'interrupts'
		 * in hwconf files overriding those in a hw node)
		 *
		 * Note that 'och' is not yet in canonical form 2, so we
		 * can happily transform it to prototype form and recreate it.
		 */
		(void) ddi_uninitchild(och);
		mutex_enter(&(DEVI(child)->devi_lock));
		mutex_enter(&(DEVI(och)->devi_lock));
		DEVI(och)->devi_sys_prop_ptr = DEVI(child)->devi_sys_prop_ptr;
		DEVI(och)->devi_drv_prop_ptr = DEVI(child)->devi_drv_prop_ptr;
		DEVI(child)->devi_sys_prop_ptr = NULL;
		DEVI(child)->devi_drv_prop_ptr = NULL;
		mutex_exit(&(DEVI(och)->devi_lock));
		mutex_exit(&(DEVI(child)->devi_lock));
		(void) ddi_initchild(parent, och);
		/*
		 * To get rid of this child...
		 *
		 * Do an extra hold on the parent, to compensate for
		 * ddi_uninitchild's extra release of the parent.
		 * [ plus the release done by returning "failure" from
		 * this function. ]
		 */
		(void) ddi_hold_devi(parent);
		(void) ddi_uninitchild(child);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}


/*
 * ndi event handling support functions:
 * The NDI event support model is as follows:
 *
 * The nexus driver defines a set of events using some static structures (so
 * these structures can be shared by all instances of the nexus driver).
 * The nexus driver allocates an event handle and binds the event set
 * to this handle. The nexus driver's event busop functions can just
 * call the appropriate NDI event support function using this handle
 * as the first argument.
 *
 * The reasoning for tying events to the device tree is that the entity
 * generating the callback will typically be one of the device driver's
 * ancestors in the tree.  This callback framework is, in many ways, a more
 * general form of interrupt handler and the associated
 * registration/deregistration
 */
#ifdef DEBUG
#define	NDI_EVENT_DEBUG	ndi_event_debug
static int ndi_event_debug;
static void ndi_event_dump_hdl(struct ndi_event_hdl *hdl, char *where);
#else
#define	NDI_EVENT_DEBUG	0
#define	ndi_event_dump_hdl(hdl, where)
#endif /* DEBUG */

/*
 * allocate a new ndi event handle
 */
int
ndi_event_alloc_hdl(dev_info_t *dip, ddi_iblock_cookie_t cookie,
	ndi_event_hdl_t *handle, uint_t flag)
{
	struct ndi_event_hdl *ndi_event_hdl;

	ndi_event_hdl = kmem_zalloc(sizeof (struct ndi_event_hdl),
		((flag & NDI_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP));

	if (ndi_event_hdl) {
		ndi_event_hdl->ndi_evthdl_dip = dip;
		ndi_event_hdl->ndi_evthdl_iblock_cookie = cookie;
		mutex_init(&ndi_event_hdl->ndi_evthdl_mutex, NULL,
				MUTEX_DRIVER, (void *)cookie);

		mutex_init(&ndi_event_hdl->ndi_evthdl_cb_mutex, NULL,
				MUTEX_DRIVER, (void *)cookie);

		*handle = (ndi_event_hdl_t)ndi_event_hdl;

		return (NDI_SUCCESS);
	}

	return (NDI_FAILURE);
}

/*
 * free the ndi event handle
 */
int
ndi_event_free_hdl(ndi_event_hdl_t handle)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);
	mutex_enter(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	if (NDI_EVENT_DEBUG) {
		ndi_event_dump_hdl(ndi_event_hdl, "ndi_event_free_hdl");
	}

	/* are there callbacks outstanding? */
	if (ndi_event_hdl->ndi_evthdl_cb_head) {
		mutex_exit(&ndi_event_hdl->ndi_evthdl_cb_mutex);
		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		return (NDI_FAILURE);
	}

	/* deallocate ndi_event_list */
	if (ndi_event_hdl->ndi_evthdl_event_defs) {
		size_t	size = ndi_event_hdl->ndi_evthdl_n_events *
				sizeof (ndi_event_definition_t);

		/* XXX there is no eventcookie removal function! */

		kmem_free(ndi_event_hdl->ndi_evthdl_event_defs, size);
	}

	if (ndi_event_hdl->ndi_evthdl_cookies) {
		kmem_free(ndi_event_hdl->ndi_evthdl_cookies,
			ndi_event_hdl->ndi_evthdl_n_events *
				sizeof (ddi_eventcookie_t));
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_cb_mutex);
	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	/* destroy mutexes */
	mutex_destroy(&ndi_event_hdl->ndi_evthdl_mutex);
	mutex_destroy(&ndi_event_hdl->ndi_evthdl_cb_mutex);


	/* free event handle */
	kmem_free(ndi_event_hdl, sizeof (struct ndi_event_hdl));

	return (NDI_SUCCESS);
}


/*
 * ndi_event_bind_set() adds a set of events to the NDI event
 * handle.
 *
 * Events generated by high level interrupts should not
 * be mixed in the same event set with events generated by
 * normal interrupts or kernel events.
 *
 * This function can be called multiple times to bind
 * additional sets to the event handle.
 * However, events generated by high level interrupts cannot
 * be bound to a handle that already has bound events generated
 * by normal interrupts or from kernel context and vice versa.
 */
int
ndi_event_bind_set(ndi_event_hdl_t handle,
	ndi_events_t		*ndi_events,
	uint_t			flag)
{
	struct ndi_event_hdl	*ndi_event_hdl;
	ndi_event_definition_t	*ndi_event_defs, *old_defs, *new_defs;
	size_t			new_size, old_size;
	ddi_eventcookie_t	*cookie_list;

	uint_t			i, j;
	uint_t			high_plevels, other_plevels, n_events;
	int km_flag = ((flag & NDI_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP);


	/*
	 * if it is not the correct version or the event set is
	 * empty, bail out
	 */
	if ((ndi_events == NULL) ||
	    (ndi_events->ndi_events_version != NDI_EVENTS_REV0)) {

		return (NDI_FAILURE);
	}

	ndi_event_hdl	= (struct ndi_event_hdl *)handle;
	ndi_event_defs	= ndi_events->ndi_event_set;
	old_defs	= ndi_event_hdl->ndi_evthdl_event_defs;
	high_plevels	= other_plevels = 0;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	/*
	 * check for duplicate events, ie. events already bound
	 */
	for (i = 0; i < ndi_events->ndi_n_events; i++) {
		for (j = 0; j < ndi_event_hdl->ndi_evthdl_n_events; j++) {
			if (strcmp(ndi_event_defs[i].ndi_event_name,
			    old_defs[j].ndi_event_name) == 0) {
				mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

				return (NDI_FAILURE);
			}
		}
	}

	/* check for mixing events at high level with the other types */
	for (i = 0; i < ndi_events->ndi_n_events; i++) {
		if (ndi_event_defs[i].ndi_event_plevel == EPL_HIGHLEVEL) {
			high_plevels++;
		} else {
			other_plevels++;
		}
	}

	/*
	 * bail out if high level events are mixed with other types in this
	 * event set or the set is incompatible with the set in the handle
	 */
	if ((high_plevels && other_plevels) ||
	    (other_plevels && ndi_event_hdl->ndi_evthdl_high_plevels) ||
	    (high_plevels && ndi_event_hdl->ndi_evthdl_other_plevels)) {
		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		return (NDI_FAILURE);
	}

	/*
	 * make a copy of ndi_events and cookie list:
	 * if there is already a list bound to the handle, allocate new
	 * lists, copy the old lists, and deallocate the old lists.
	 */
	n_events = ndi_event_hdl->ndi_evthdl_n_events +
			ndi_events->ndi_n_events;

	new_size = n_events * sizeof (ndi_event_definition_t);
	old_size = ndi_event_hdl->ndi_evthdl_n_events *
			sizeof (ndi_event_definition_t);

	new_defs = kmem_zalloc(new_size, km_flag);
	if (new_defs == NULL) {
		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		return (NDI_FAILURE);
	}

	cookie_list = kmem_zalloc(
		sizeof (ddi_eventcookie_t) * n_events, km_flag);
	if (cookie_list == NULL) {
		kmem_free(new_defs, new_size);
		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		return (NDI_FAILURE);
	}

	if (old_defs) {
		size_t old_cookies_size = ndi_event_hdl->ndi_evthdl_n_events *
				sizeof (ddi_eventcookie_t);
		bcopy(old_defs, new_defs, old_size);
		bcopy(ndi_event_hdl->ndi_evthdl_cookies,
			cookie_list, old_cookies_size);
		kmem_free(old_defs, old_size);
		kmem_free(ndi_event_hdl->ndi_evthdl_cookies,
						old_cookies_size);
	}

	bcopy(ndi_event_defs, &new_defs[ndi_event_hdl->ndi_evthdl_n_events],
			new_size - old_size);

	/* initialize all new event cookies */
	j = ndi_event_hdl->ndi_evthdl_n_events;
	for (i = 0; i < ndi_events->ndi_n_events; i++) {
		cookie_list[j++] = ndi_event_getcookie(
			(char *)ndi_event_defs[i].ndi_event_name);
	}

	/* update the handle */
	ndi_event_hdl->ndi_evthdl_event_defs	= new_defs;
	ndi_event_hdl->ndi_evthdl_cookies	= cookie_list;
	ndi_event_hdl->ndi_evthdl_n_events	= n_events;
	ndi_event_hdl->ndi_evthdl_high_plevels	+= high_plevels;
	ndi_event_hdl->ndi_evthdl_other_plevels += other_plevels;

	if (NDI_EVENT_DEBUG) {
		ndi_event_dump_hdl(ndi_event_hdl, "ndi_event_bind_set");
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (NDI_SUCCESS);
}

/*
 * ndi_event_unbind_set() unbinds a set of events previously
 * bound using ndi_event_bind_set() from the NDI event
 * handle.
 *
 * All events in the event set must have been bound to the
 * handle in order for ndi_event_unbind_set() to succeed.
 * The event set may be a subset of the set of events that
 * was previously bound to the handle. For example, events
 * can be individually unbound.
 *
 * An event set cannot be unbound if callbacks are still
 * registered for any event in this event set.
 */
int
ndi_event_unbind_set(ndi_event_hdl_t   handle,
	ndi_events_t	*ndi_events, uint_t flag)
{
	struct ndi_event_hdl	*ndi_event_hdl;
	ndi_event_definition_t	*ndi_event_defs, *old_defs, *new_defs;
	size_t			old_size, new_size;
	ndi_event_callbacks_t	*next;
	ddi_eventcookie_t	cookie, *cookie_list;

	uint_t			i, j, n_events, found, n;
	uint_t			high_plevels, other_plevels;
	int km_flag = ((flag & NDI_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP);
	int			rval;

	/*
	 * if it is not the correct version or one of the sets is empty,
	 * bail out
	 */
	if ((ndi_events == NULL) ||
	    (ndi_events->ndi_events_version != NDI_EVENTS_REV0) ||
	    (ndi_events->ndi_event_set == NULL)) {

		return (NDI_FAILURE);
	}

	ndi_event_hdl	= (struct ndi_event_hdl *)handle;
	ndi_event_defs	= ndi_events->ndi_event_set;
	old_defs	= ndi_event_hdl->ndi_evthdl_event_defs;
	high_plevels	= other_plevels = 0;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);
	mutex_enter(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	/*
	 * check whether each event in ndi_event_defs is
	 * in old_defs
	 */
	for (i = 0; i < ndi_events->ndi_n_events; i++) {
		found = 0;
		for (j = 0; j < ndi_event_hdl->ndi_evthdl_n_events; j++) {
			if (strcmp(ndi_event_defs[i].ndi_event_name,
			    old_defs[j].ndi_event_name) == 0) {
				cookie = ndi_event_hdl->ndi_evthdl_cookies[j];
				found++;
				break;
			}
		}

		if (found == 0) {
			rval = NDI_FAILURE;
			goto done;
		}

		/*
		 * check whether any callbacks are outstanding for this
		 * event
		 */
		for (next = ndi_event_hdl->ndi_evthdl_cb_head;
		    next != NULL; next = next->ndi_evtcb_next) {

			if (next->ndi_evtcb_cookie == cookie) {
				rval = NDI_FAILURE;
				goto done;
			}
		}
	}


	/* allocate new lists (if events left), the set can be unbound now */
	n_events = ndi_event_hdl->ndi_evthdl_n_events -
				ndi_events->ndi_n_events;
	old_size = ndi_event_hdl->ndi_evthdl_n_events *
			sizeof (ndi_event_definition_t);
	high_plevels = other_plevels = 0;

	if (n_events) {
		new_size = n_events * sizeof (ndi_event_definition_t);

		new_defs = kmem_zalloc(new_size, km_flag);
		if (new_defs == NULL) {
			rval = NDI_FAILURE;
			goto done;
		}

		cookie_list = kmem_zalloc(
			n_events * sizeof (ddi_eventcookie_t), km_flag);
		if (cookie_list == NULL) {
			kmem_free(new_defs, new_size);
			rval = NDI_FAILURE;
			goto done;
		}

		/*
		 * copy each event in the old list and not in new list.
		 */
		for (n = j = 0; j < ndi_event_hdl->ndi_evthdl_n_events; j++) {
			found = 0;
			for (i = 0; i < ndi_events->ndi_n_events; i++) {
				if (strcmp(ndi_event_defs[i].ndi_event_name,
				    old_defs[j].ndi_event_name) == 0) {
					found++;
					break;
				}
			}
			if (found == 0) {
				/* copy event entry and cookie */
				new_defs[n] = old_defs[j];
				cookie_list[n++] =
					ndi_event_hdl->ndi_evthdl_cookies[j];
			}
		}

		/* find new plevel counts */
		for (i = 0; i < n_events; i++) {
			if (new_defs[i].ndi_event_plevel == EPL_HIGHLEVEL) {
				high_plevels++;
			} else {
				other_plevels++;
			}

			/* double check that cookies are still consistent */
			ASSERT(ndi_event_getcookie(
			    new_defs[i].ndi_event_name) == cookie_list[i]);
		}
	} else {
		new_defs = NULL;
		n_events = 0;
		cookie_list = NULL;
	}

	if (old_defs) {
		kmem_free(old_defs, old_size);
	}
	if (ndi_event_hdl->ndi_evthdl_cookies) {
		kmem_free(ndi_event_hdl->ndi_evthdl_cookies,
			ndi_event_hdl->ndi_evthdl_n_events *
				sizeof (ddi_eventcookie_t));
	}

	/* update handle */
	ndi_event_hdl->ndi_evthdl_event_defs	= new_defs;
	ndi_event_hdl->ndi_evthdl_n_events	= n_events;
	ndi_event_hdl->ndi_evthdl_cookies	= cookie_list;
	ndi_event_hdl->ndi_evthdl_high_plevels	= high_plevels;
	ndi_event_hdl->ndi_evthdl_other_plevels = other_plevels;

	if (NDI_EVENT_DEBUG) {
		ndi_event_dump_hdl(ndi_event_hdl, "ndi_event_unbind_set");
	}

	rval = NDI_SUCCESS;

done:
	mutex_exit(&ndi_event_hdl->ndi_evthdl_cb_mutex);
	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (rval);
}

/*
 * ndi_event_retrieve_cookie():
 * Return an event cookie for eventname if this nexus driver
 * has defined the named event. The event cookie returned
 * by this function is used to register callback handlers
 * for the event.
 * Refer also to bus_get_eventcookie(9n) and
 * ndi_busop_get_eventcookie(9n). ndi_event_retrieve_cookie(9F)
 * is intended to be used in the nexus driver's bus_get_eventcookie
 * busop function.
 * This function returns an interrupt block cookie that must
 * be used by the requesting driver to initialize any
 * locks (mutex(9F), rwlock(9F)) used by the event callback
 * handler.  The nexus must return an iblock_cookie
 * which corresponds to the interrupt that generates the event.
 * This function also returns an indication of the system level
 * the event will be delivered at.  Levels include kernel (thread)
 * interrupt, and hilevel (above lock level).
 * If the event is not defined by this bus nexus driver, and flag
 * does not include NDI_EVENT_NOPASS, then ndi_event_retrieve_cookie()
 * will pass the request up the device tree hierarchy by calling
 * ndi_busop_get_eventcookie(9N).
 * If the event is not defined by this bus nexus driver, and flag
 * does include NDI_EVENT_NOPASS, ndi_event_retrieve_cookie() will
 * return NDI_FAILURE.  The caller may then determine what further
 * action to take, such as using a different handle, passing the
 * request up the device tree using ndi_busop_get_eventcookie(9N),
 * or returning the failure to the caller, thus blocking the
 * progress of the request up the tree.
 */
int
ndi_event_retrieve_cookie(ndi_event_hdl_t handle,
	dev_info_t		*rdip,
	char			*eventname,
	ddi_eventcookie_t	*cookiep,
	ddi_plevel_t		*plevelp,
	ddi_iblock_cookie_t	*iblock_cookiep,
	uint_t			flag)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	ndi_event_definition_t	*ndi_event_defs =
				ndi_event_hdl->ndi_evthdl_event_defs;
	int		i;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	/*
	 * search the event set for the event name and return
	 * cookies and plevel if found.
	 */
	for (i = 0; i < ndi_event_hdl->ndi_evthdl_n_events; i++) {
		if (strcmp(ndi_event_defs[i].ndi_event_name,
		    eventname) == 0) {
			*cookiep = ndi_event_hdl->ndi_evthdl_cookies[i];
			*plevelp = ndi_event_defs[i].ndi_event_plevel;
			*iblock_cookiep = ndi_event_hdl->
						ndi_evthdl_iblock_cookie;

			ASSERT(ndi_event_getcookie(
			    ndi_event_defs[i].ndi_event_name) ==
			    ndi_event_hdl->ndi_evthdl_cookies[i]);

			mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

			return (NDI_SUCCESS);
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	/*
	 * event was not found, pass to parent or return failure
	 */
	if ((flag & NDI_EVENT_NOPASS) == 0) {

		return (ndi_busop_get_eventcookie(
			ndi_event_hdl->ndi_evthdl_dip, rdip,
			eventname, cookiep, plevelp, iblock_cookiep));

	} else {

		return (NDI_FAILURE);
	}
}

/*
 * check whether this nexus defined this event and look up attributes
 */
static int
ndi_event_is_defined(ndi_event_hdl_t handle,
	ddi_eventcookie_t cookie, int *attributes)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	int			i;

	for (i = 0; i < ndi_event_hdl->ndi_evthdl_n_events; i++) {
		if (ndi_event_hdl->ndi_evthdl_cookies[i] == cookie) {

			if (attributes) {
				*attributes = ndi_event_hdl->
					ndi_evthdl_event_defs[i].
					ndi_event_attributes;
			}

			return (NDI_SUCCESS);
		}
	}

	return (NDI_FAILURE);
}

/*
 * ndi_event_add_callback():
 * If the event has been defined by this bus nexus driver,
 * ndi_event_add_callback() adds an event callback registration
 * to the event handle.
 * Refer also to bus_add_eventcall(9n) and
 * ndi_busop_add_eventcall(9n).
 * ndi_event_add_callback(9n) is intended to be used in
 * the nexus driver's bus_add_eventcall(9n) busop function.
 * If the event is not defined by this bus nexus driver, and flag
 * does not include NDI_EVENT_NOPASS, then ndi_event_add_callback()
 * will pass the request up the device tree hierarchy by calling
 * ndi_busop_add_eventcall(9N).
 * If the event is not defined by this bus nexus driver, and flag
 * does include NDI_EVENT_NOPASS, ndi_event_add_callback() will
 * return NDI_FAILURE.  The caller may then determine what further
 * action to take, such as using a different handle, passing the
 * request up the device tree using ndi_busop_add_callback(9N),
 * or returning the failure to the caller, thus blocking the
 * progress of the request up the tree.
 */
int
ndi_event_add_callback(ndi_event_hdl_t handle, dev_info_t *child_dip,
	ddi_eventcookie_t cookie,
	int		(*event_callback)(dev_info_t *,
			ddi_eventcookie_t, void *arg, void *impldata),
	void		*arg,
	uint_t		flag)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	int km_flag = ((flag & NDI_NOSLEEP) ? KM_NOSLEEP : KM_SLEEP);
	ndi_event_callbacks_t *cb;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	/*
	 * if the event was not bound to this handle, pass to parent
	 * or return failure
	 */
	if (ndi_event_is_defined(handle, cookie, NULL) != NDI_SUCCESS) {

		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		if ((flag & NDI_EVENT_NOPASS) == 0) {

			return (ndi_busop_add_eventcall(
				ndi_event_hdl->ndi_evthdl_dip,
				child_dip, cookie, event_callback, arg));
		} else {

			return (NDI_FAILURE);
		}
	}

	/*
	 * allocate space for a callback structure
	 */
	cb = kmem_zalloc(sizeof (ndi_event_callbacks_t), km_flag);
	if (cb == NULL) {
		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		return (NDI_FAILURE);
	}

	/* initialize callback structure */
	cb->ndi_evtcb_dip	= child_dip;
	cb->ndi_evtcb_callback	= event_callback;
	cb->ndi_evtcb_arg	= arg;
	cb->ndi_evtcb_cookie	= cookie;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	/* add this callback structure to the list */
	if (ndi_event_hdl->ndi_evthdl_cb_head) {
		ndi_event_hdl->ndi_evthdl_cb_tail->ndi_evtcb_next = cb;
		cb->ndi_evtcb_prev = ndi_event_hdl->ndi_evthdl_cb_tail;
		ndi_event_hdl->ndi_evthdl_cb_tail = cb;
	} else {
		ndi_event_hdl->ndi_evthdl_cb_head =
			ndi_event_hdl->ndi_evthdl_cb_tail = cb;
	}

	if (NDI_EVENT_DEBUG) {
		ndi_event_dump_hdl(ndi_event_hdl, "ndi_event_add_callback");
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_cb_mutex);
	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (NDI_SUCCESS);
}

/*
 * ndi_event_remove_callback():
 *
 * If the event  has been defined by this bus nexus driver
 * ndi_event_remove_callback() removes a callback that was
 * previously registered using ndi_event_remove_callback(9N).
 *
 * Refer also to bus_remove_eventcall(9n) and
 * ndi_busop_remove_eventcall(9n).
 * ndi_event_remove_callback(9n) is intended to be used in
 * the nexus driver's bus_remove_eventcall (9n) busop function.
 *
 * If the event is not defined by this bus nexus driver, and flag
 * does not include NDI_EVENT_NOPASS, then ndi_event_remove_callback()
 * will pass the request up the device tree hierarchy by calling
 * ndi_busop_remove_eventcall(9N).
 *
 * If the event is not defined by this bus nexus driver, and flag
 * does include NDI_EVENT_NOPASS, ndi_event_remove_callback() will
 * return NDI_FAILURE.  The caller may then determine what further
 * action to take, such as using a different handle, passing the
 * request up the device tree using ndi_busop_remove_eventcall(9N),
 * or returning the failure to the caller, thus blocking the
 * progress of the request up the tree.
 */
static int do_ndi_event_remove_callback(struct ndi_event_hdl *ndi_event_hdl,
	dev_info_t *child_dip, ddi_eventcookie_t cookie, uint_t flag);

/*ARGSUSED*/
int
ndi_event_remove_callback(ndi_event_hdl_t handle, dev_info_t *child_dip,
	ddi_eventcookie_t cookie, uint_t flag)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	int rval;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	/* if this is not our event then pass or fail */
	if (ndi_event_is_defined(handle, cookie, NULL) != NDI_SUCCESS) {

		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		if ((flag & NDI_EVENT_NOPASS) == 0) {

			return (ndi_busop_remove_eventcall(
				ndi_event_hdl->ndi_evthdl_dip,
				child_dip, cookie));
		} else {

			return (NDI_FAILURE);
		}
	}

	/* search for a callback that matches this dip and cookie */
	mutex_enter(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	rval = do_ndi_event_remove_callback(ndi_event_hdl, child_dip,
		cookie, flag);

	mutex_exit(&ndi_event_hdl->ndi_evthdl_cb_mutex);
	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (rval);
}

/*ARGSUSED*/
static int
do_ndi_event_remove_callback(struct ndi_event_hdl *ndi_event_hdl,
	dev_info_t *child_dip,
	ddi_eventcookie_t cookie, uint_t flag)
{
	ndi_event_callbacks_t *next, *cb;
	int found = 0;

	for (next = ndi_event_hdl->ndi_evthdl_cb_head; next != NULL; ) {

		cb = next;
		next = next->ndi_evtcb_next;

		if ((cb->ndi_evtcb_dip == child_dip) &&
		    (cb->ndi_evtcb_cookie == cookie)) {

			/* remove from callback linked list */
			if (cb->ndi_evtcb_prev) {
				cb->ndi_evtcb_prev->ndi_evtcb_next =
					cb->ndi_evtcb_next;
			}
			if (cb->ndi_evtcb_next) {
				cb->ndi_evtcb_next->ndi_evtcb_prev =
					cb->ndi_evtcb_prev;
			}
			if (ndi_event_hdl->ndi_evthdl_cb_head == cb) {
				ndi_event_hdl->ndi_evthdl_cb_head =
						cb->ndi_evtcb_next;
			}
			if (ndi_event_hdl->ndi_evthdl_cb_tail == cb) {
				ndi_event_hdl->ndi_evthdl_cb_tail =
						cb->ndi_evtcb_prev;
			}

			kmem_free(cb, sizeof (ndi_event_callbacks_t));
			found++;
		}
	}

	return (found ? NDI_SUCCESS : NDI_FAILURE);
}

/*
 * ndi_event_run_callbacks() performs callbacks for the event
 * specified by cookie, if this is among those bound to the
 * supplied handle.
 * If the event is among those bound to the handle, none,
 * some, or all of the handlers registered for the event
 * will be called, according to the delivery attributes of
 * the event.
 * If the event attributes include NDI_EVENT_POST_TO_ALL
 * (the default), all the handlers for the event will be
 * called in an unspecified order.  The return value will be
 * NDI_EVENT_CLAIMED if one or more handler returned either
 * DDI_EVENT_CLAIMED or DDI_EVENT_CLAIMED_CANCELLED; in the
 * latter case, those handlers will also have been removed from
 * the list of handlers for future occurrences of the event.
 * If no handler claimed the event (including the case where
 * there are no registered handlers), the return value will be
 * NDI_EVENT_UNCLAIMED.
 * If the event attributes include NDI_EVENT_POST_TO_ONE,
 * the handlers for the event will be called sequentially, in
 * an unspecified order, until one returns DDI_EVENT_CLAIMED
 * or DDI_EVENT_CLAIMED_CANCELLED.  ndi_event_run_callbacks()
 * will then return NDI_EVENT_CLAIMED, after unregistering
 * the handler if it returned DDI_EVENT_CLAIMED_CANCELLED.  If
 * when each handler has been called, the event has still not
 * been claimed, the return value will be NDI_EVENT_UNCLAIMED.
 * If the event attributes include NDI_EVENT_POST_TO_TGT, only
 * the handler (if any) registered by the driver identified by
 * rdip will be called.  If no such handler has been
 * registered, or it returns DDI_EVENT_UNCLAIMED, the return
 * value will be NDI_EVENT_UNCLAIMED.  Otherwise, the return
 * value will be NDI_EVENT_CLAIMED, and, as usual, the handler
 * will have been unregistered if it returned
 * DDI_EVENT_CLAIMED_CANCELLED.
 * If the event identified by cookie is not bound to the handle,
 * then the flag parameter determines whether the call should
 * be passed up: if it does not include NDI_EVENT_NOPASS,
 * ndi_event_run_callbacks() will call ndi_post_event() and
 * return its result.  Otherwise, ndi_event_run_callbacks() will
 * return NDI_FAILURE.
 */
/*ARGSUSED*/
int
ndi_event_run_callbacks(ndi_event_hdl_t handle, dev_info_t *child_dip,
	ddi_eventcookie_t cookie, void *bus_impldata, uint_t flag)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	ndi_event_callbacks_t *next, *cb;
	int rval;
	int claimed = 0;
	int attributes;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	/* if this is not our event, pass up or fail */
	if (ndi_event_is_defined(handle, cookie, &attributes) !=
	    NDI_SUCCESS) {

		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		if ((flag & NDI_EVENT_NOPASS) == 0) {
			return (ndi_post_event(
				ndi_event_hdl->ndi_evthdl_dip,
				child_dip, cookie, bus_impldata));
		} else {
			return (NDI_FAILURE);
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	if (NDI_EVENT_DEBUG) {
		cmn_err(CE_CONT, "ndi_event_run_callbacks:\n\t"
		    "producer dip=%p (%s%d): cookie = %p, name = %s\n",
		    (void *)ndi_event_hdl->ndi_evthdl_dip,
		    ddi_node_name(ndi_event_hdl->ndi_evthdl_dip),
		    ddi_get_instance(ndi_event_hdl->ndi_evthdl_dip),
		    (void *)cookie,
		    ndi_event_cookie_to_name(handle, cookie));
	}


	/*
	 * we only grab the cb mutex because the callback handlers
	 * may call the conversion functions which would cause a recursive
	 * mutex problem
	 */
	mutex_enter(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	/* perform callbacks */
	for (next = ndi_event_hdl->ndi_evthdl_cb_head; next != NULL; ) {

		cb = next;
		next = next->ndi_evtcb_next;

		if (cb->ndi_evtcb_cookie == cookie) {

			if (!((attributes == NDI_EVENT_POST_TO_ALL) ||
			    (attributes == NDI_EVENT_POST_TO_ONE) ||
			    ((attributes == NDI_EVENT_POST_TO_TGT) &&
			    (child_dip == cb->ndi_evtcb_dip)))) {
				continue;
			}

			rval = cb->ndi_evtcb_callback(cb->ndi_evtcb_dip,
				cb->ndi_evtcb_cookie,
				cb->ndi_evtcb_arg,
				bus_impldata);

			if (NDI_EVENT_DEBUG) {
				cmn_err(CE_CONT,
				    "\t\tconsumer dip=%p (%s%d), rval=%d\n",
				    (void *)cb->ndi_evtcb_dip,
				    ddi_node_name(cb->ndi_evtcb_dip),
				    ddi_get_instance(cb->ndi_evtcb_dip),
				    rval);
			}

			if (rval == DDI_EVENT_CLAIMED) {
				claimed++;
				if ((attributes == NDI_EVENT_POST_TO_ONE) ||
				    (attributes == NDI_EVENT_POST_TO_TGT)) {
					break;
				}
			} else if (rval == DDI_EVENT_CLAIMED_UNREGISTER) {
				(void) do_ndi_event_remove_callback(
					ndi_event_hdl,
					cb->ndi_evtcb_dip,
					cookie, 0);
				claimed++;
			}
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	if (NDI_EVENT_DEBUG) {
		mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);
		ndi_event_dump_hdl(ndi_event_hdl, "ndi_event_run_callbacks");
		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);
	}

	return (claimed ? NDI_EVENT_CLAIMED : NDI_EVENT_UNCLAIMED);
}


/*
 * perform one callback for a specified cookie and just one target
 */
/*ARGSUSED*/
int
ndi_event_do_callback(ndi_event_hdl_t handle, dev_info_t *child_dip,
	ddi_eventcookie_t cookie, void *bus_impldata, uint_t flag)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	ndi_event_callbacks_t *next, *cb;
	int rval;
	int claimed = 0;
	int attributes;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	/* if this is not our event, pass up or fail */
	if (ndi_event_is_defined(handle, cookie, &attributes) !=
	    NDI_SUCCESS) {

		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

		if ((flag & NDI_EVENT_NOPASS) == 0) {
			return (ndi_post_event(
				ndi_event_hdl->ndi_evthdl_dip,
				child_dip, cookie, bus_impldata));
		} else {
			return (NDI_FAILURE);
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	if (NDI_EVENT_DEBUG) {
		cmn_err(CE_CONT, "ndi_event_run_callbacks:\n\t"
		    "producer dip=%p (%s%d): cookie = %p, name = %s\n",
		    (void *)ndi_event_hdl->ndi_evthdl_dip,
		    ddi_node_name(ndi_event_hdl->ndi_evthdl_dip),
		    ddi_get_instance(ndi_event_hdl->ndi_evthdl_dip),
		    (void *)cookie,
		    ndi_event_cookie_to_name(handle, cookie));
	}


	/*
	 * we only grab the cb mutex because the callback handlers
	 * may call the conversion functions which would cause a recursive
	 * mutex problem
	 */
	mutex_enter(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	/* perform callbacks */
	for (next = ndi_event_hdl->ndi_evthdl_cb_head; next != NULL; ) {
		cb = next;
		next = next->ndi_evtcb_next;

		if ((cb->ndi_evtcb_cookie == cookie) &&
		    (cb->ndi_evtcb_dip == child_dip)) {

			rval = cb->ndi_evtcb_callback(cb->ndi_evtcb_dip,
				cb->ndi_evtcb_cookie,
				cb->ndi_evtcb_arg,
				bus_impldata);

			if (NDI_EVENT_DEBUG) {
				cmn_err(CE_CONT,
				    "\t\tconsumer dip=%p (%s%d), rval=%d\n",
				    (void *)cb->ndi_evtcb_dip,
				    ddi_node_name(cb->ndi_evtcb_dip),
				    ddi_get_instance(cb->ndi_evtcb_dip),
				    rval);
			}
			claimed++;

			break;
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_cb_mutex);

	if (NDI_EVENT_DEBUG) {
		mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);
		ndi_event_dump_hdl(ndi_event_hdl, "ndi_event_run_callbacks");
		mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);
	}

	return (claimed ? NDI_EVENT_CLAIMED : NDI_EVENT_UNCLAIMED);
}


/*
 * ndi_event_tag_to_cookie: utility function to find an event cookie
 * given an event tag
 */
ddi_eventcookie_t
ndi_event_tag_to_cookie(ndi_event_hdl_t handle, int event_tag)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	ndi_event_definition_t	*ndi_event_defs =
				    ndi_event_hdl->ndi_evthdl_event_defs;
	int	i;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	for (i = 0; i < ndi_event_hdl->ndi_evthdl_n_events; i++) {
		if (ndi_event_defs[i].ndi_event_tag == event_tag) {
			mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

			return (ndi_event_hdl->ndi_evthdl_cookies[i]);
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (NULL);
}

/*
 * ndi_event_cookie_to_tag: utility function to find a event tag
 * given an event_cookie
 */
int
ndi_event_cookie_to_tag(ndi_event_hdl_t handle, ddi_eventcookie_t cookie)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	ndi_event_definition_t	*ndi_event_defs =
				    ndi_event_hdl->ndi_evthdl_event_defs;
	int	i;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	for (i = 0; i < ndi_event_hdl->ndi_evthdl_n_events; i++) {
		if (ndi_event_hdl->ndi_evthdl_cookies[i] == cookie) {
			mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

			return (ndi_event_defs[i].ndi_event_tag);
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (-1);
}

/*
 * ndi_event_cookie_to_name: utility function to find an event name
 * given an event_cookie
 */
char *
ndi_event_cookie_to_name(ndi_event_hdl_t handle, ddi_eventcookie_t cookie)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	ndi_event_definition_t	*ndi_event_defs =
				    ndi_event_hdl->ndi_evthdl_event_defs;
	int			i;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);
	for (i = 0; i < ndi_event_hdl->ndi_evthdl_n_events; i++) {
		if (ndi_event_hdl->ndi_evthdl_cookies[i] == cookie) {
			mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);
			return (ndi_event_defs[i].ndi_event_name);
		}
	}
	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (NULL);
}

/*
 * ndi_event_tag_to_name: utility function to find an event name
 * given an event tag
 */
char *
ndi_event_tag_to_name(ndi_event_hdl_t handle, int event_tag)
{
	struct ndi_event_hdl *ndi_event_hdl = (struct ndi_event_hdl *)handle;
	ndi_event_definition_t *ndi_event_defs =
				ndi_event_hdl->ndi_evthdl_event_defs;
	int			i;

	mutex_enter(&ndi_event_hdl->ndi_evthdl_mutex);

	for (i = 0; i < ndi_event_hdl->ndi_evthdl_n_events; i++) {
		if (ndi_event_defs[i].ndi_event_tag == event_tag) {
			mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);
			return (ndi_event_defs[i].ndi_event_name);
		}
	}

	mutex_exit(&ndi_event_hdl->ndi_evthdl_mutex);

	return (NULL);
}

#ifdef DEBUG
static void
ndi_event_dump_hdl(struct ndi_event_hdl *hdl, char *where)
{
	ndi_event_callbacks_t *next;
	ndi_event_definition_t *edefs = hdl->ndi_evthdl_event_defs;
	int i;

	cmn_err(CE_CONT, "%s: event handle (%p): dip = %p (%s%d)\n",
		where, (void *)hdl,
		(void *)hdl->ndi_evthdl_dip,
		ddi_node_name(hdl->ndi_evthdl_dip),
		ddi_get_instance(hdl->ndi_evthdl_dip));
	cmn_err(CE_CONT, "\thigh=%d other=%d n=%d\n",
		hdl->ndi_evthdl_high_plevels,
		hdl->ndi_evthdl_other_plevels,
		hdl->ndi_evthdl_n_events);

	cmn_err(CE_CONT, "\tevent set:\n");

	for (i = 0; i < hdl->ndi_evthdl_n_events; i++, edefs++) {

		cmn_err(CE_CONT,
			"\t\ttag=%d name=%s p=%d a=%x c=%p\n",
			edefs->ndi_event_tag,
			edefs->ndi_event_name,
			edefs->ndi_event_plevel,
			edefs->ndi_event_attributes,
			(void *)(hdl->ndi_evthdl_cookies[i]));
	}

	cmn_err(CE_CONT, "\tcallbacks:\n");
	for (next = hdl->ndi_evthdl_cb_head;
	    next != NULL; next = next->ndi_evtcb_next) {
		cmn_err(CE_CONT, "\t\tdip=%p (%s%d) cookie=%x arg=%p\n",
			(void *)next->ndi_evtcb_dip,
			ddi_node_name(next->ndi_evtcb_dip),
			ddi_get_instance(next->ndi_evtcb_dip),
			next->ndi_evtcb_cookie,
			next->ndi_evtcb_arg);
	}
	cmn_err(CE_CONT, "\n");
}
#endif

int
ndi_dev_is_prom_node(dev_info_t *dev)
{
	return (DEVI(dev)->devi_node_class == DDI_NC_PROM);
}

int
ndi_dev_is_pseudo_node(dev_info_t *dev)
{
	return (DEVI(dev)->devi_node_class == DDI_NC_PSEUDO);
}

int
ndi_dev_is_persistent_node(dev_info_t *dev)
{
	return ((DEVI(dev)->devi_node_attributes & DDI_PERSISTENT) != 0);
}

int
i_ndi_dev_is_auto_assigned_node(dev_info_t *dev)
{
	return ((DEVI(dev)->devi_node_attributes &
	    DDI_AUTO_ASSIGNED_NODEID) != 0);
}

void
i_ndi_set_node_class(dev_info_t *dev, ddi_node_class_t c)
{
	DEVI(dev)->devi_node_class = c;
}

ddi_node_class_t
i_ndi_get_node_class(dev_info_t *dev)
{
	return (DEVI(dev)->devi_node_class);
}

void
i_ndi_set_node_attributes(dev_info_t *dev, int p)
{
	DEVI(dev)->devi_node_attributes = p;
}

int
i_ndi_get_node_attributes(dev_info_t *dev)
{
	return (DEVI(dev)->devi_node_attributes);
}

void
i_ndi_set_nodeid(dev_info_t *dip, int n)
{
	DEVI(dip)->devi_nodeid = n;
}

void
ndi_set_acc_fault(ddi_acc_handle_t ah)
{
	i_ddi_acc_set_fault(ah);
}

void
ndi_clr_acc_fault(ddi_acc_handle_t ah)
{
	i_ddi_acc_clr_fault(ah);
}

void
ndi_set_dma_fault(ddi_dma_handle_t dh)
{
	i_ddi_dma_set_fault(dh);
}

void
ndi_clr_dma_fault(ddi_dma_handle_t dh)
{
	i_ddi_dma_clr_fault(dh);
}

/*
 *  The default fault-handler, called when the event posted by
 *  ddi_dev_report_fault() reaches rootnex.
 */
static void
i_ddi_fault_handler(dev_info_t *dip, struct ddi_fault_event_data *fedp)
{
	mutex_enter(&(DEVI(dip)->devi_lock));
	if (!DEVI_IS_DEVICE_OFFLINE(dip)) {
		switch (fedp->f_impact) {
		case DDI_SERVICE_LOST:
			DEVI_SET_DEVICE_DOWN(dip);
			break;

		case DDI_SERVICE_DEGRADED:
			DEVI_SET_DEVICE_DEGRADED(dip);
			break;

		case DDI_SERVICE_UNAFFECTED:
		default:
			break;

		case DDI_SERVICE_RESTORED:
			DEVI_SET_DEVICE_UP(dip);
			break;
		}
	}
	mutex_exit(&(DEVI(dip)->devi_lock));
}

/*
 * The default fault-logger, called when the event posted by
 * ddi_dev_report_fault() reaches rootnex.
 */
/*ARGSUSED*/
static void
i_ddi_fault_logger(dev_info_t *rdip, struct ddi_fault_event_data *fedp)
{
	ddi_devstate_t newstate;
	const char *action;
	const char *servstate;
	const char *location;
	int bad;
	int changed;
	int level;
	int still;

	bad = 0;
	switch (fedp->f_location) {
	case DDI_DATAPATH_FAULT:
		location = "in datapath to";
		break;
	case DDI_DEVICE_FAULT:
		location = "in";
		break;
	case DDI_EXTERNAL_FAULT:
		location = "external to";
		break;
	default:
		location = "somewhere near";
		bad = 1;
		break;
	}

	newstate = ddi_get_devstate(fedp->f_dip);
	switch (newstate) {
	case DDI_DEVSTATE_OFFLINE:
		servstate = "unavailable";
		break;
	case DDI_DEVSTATE_DOWN:
		servstate = "unavailable";
		break;
	case DDI_DEVSTATE_QUIESCED:
		servstate = "suspended";
		break;
	case DDI_DEVSTATE_DEGRADED:
		servstate = "degraded";
		break;
	default:
		servstate = "available";
		break;
	}

	changed = (newstate != fedp->f_oldstate);
	level = (newstate < fedp->f_oldstate) ? CE_WARN : CE_NOTE;
	switch (fedp->f_impact) {
	case DDI_SERVICE_LOST:
	case DDI_SERVICE_DEGRADED:
	case DDI_SERVICE_UNAFFECTED:
		/* fault detected; service [still] <servstate> */
		action = "fault detected";
		still = !changed;
		break;

	case DDI_SERVICE_RESTORED:
		if (newstate != DDI_DEVSTATE_UP) {
			/* fault cleared; service still <servstate> */
			action = "fault cleared";
			still = 1;
		} else if (changed) {
			/* fault cleared; service <servstate> */
			action = "fault cleared";
			still = 0;
		} else {
			/* no fault; service <servstate> */
			action = "no fault";
			still = 0;
		}
		break;

	default:
		bad = 1;
		break;
	}

	cmn_err(level, "!%s%d: %s %s device; service %s%s"+(bad|changed),
		ddi_driver_name(fedp->f_dip),
		ddi_get_instance(fedp->f_dip),
		bad ? "invalid report of fault" : action,
		location, still ? "still " : "", servstate);

	cmn_err(level, "!%s%d: %s"+(bad|changed),
		ddi_driver_name(fedp->f_dip),
		ddi_get_instance(fedp->f_dip),
		fedp->f_message);
}

/*
 * Platform-settable pointers to fault handler and logger functions.
 * These are called by the default rootnex event-posting code when
 * a fault event reaches rootnex.
 */
void (*plat_fault_handler)(dev_info_t *, struct ddi_fault_event_data *) =
	i_ddi_fault_handler;
void (*plat_fault_logger)(dev_info_t *, struct ddi_fault_event_data *) =
	i_ddi_fault_logger;

/*
 * Rootnex event definitions ...
 */
enum rootnex_event_tags {
	ROOTNEX_FAULT_EVENT
};
static ndi_event_hdl_t rootnex_event_hdl;
static ndi_event_definition_t rootnex_event_set[] = {
	{
		ROOTNEX_FAULT_EVENT,
		DDI_DEVI_FAULT_EVENT,
		EPL_INTERRUPT,
		NDI_EVENT_POST_TO_ALL
	}
};
static ndi_events_t rootnex_events = {
	NDI_EVENTS_REV0,
	sizeof (rootnex_event_set) / sizeof (rootnex_event_set[0]),
	rootnex_event_set
};

/*
 * Initialise rootnex event handle
 */
void
i_ddi_rootnex_init_events(dev_info_t *dip)
{
	if (ndi_event_alloc_hdl(dip, (ddi_iblock_cookie_t)(LOCK_LEVEL-1),
	    &rootnex_event_hdl, NDI_SLEEP) == NDI_SUCCESS) {
		if (ndi_event_bind_set(rootnex_event_hdl,
		    &rootnex_events, NDI_SLEEP) != NDI_SUCCESS) {
			(void) ndi_event_free_hdl(rootnex_event_hdl);
			rootnex_event_hdl = NULL;
		}
	}
}

/*
 *      Event-handling functions for rootnex
 *      These provide the standard implementation of fault handling
 */
/*ARGSUSED*/
int
i_ddi_rootnex_get_eventcookie(dev_info_t *dip, dev_info_t *rdip,
	char *eventname, ddi_eventcookie_t *cookiep, ddi_plevel_t *plevelp,
	ddi_iblock_cookie_t *iblock_cookiep)
{
	if (rootnex_event_hdl == NULL)
		return (NDI_FAILURE);
	return (ndi_event_retrieve_cookie(rootnex_event_hdl, rdip, eventname,
		cookiep, plevelp, iblock_cookiep, NDI_EVENT_NOPASS));
}

/*ARGSUSED*/
int
i_ddi_rootnex_add_eventcall(dev_info_t *dip, dev_info_t *rdip,
	ddi_eventcookie_t eventid, int (*handler)(dev_info_t *dip,
	ddi_eventcookie_t event, void *arg, void *impl_data), void *arg)
{
	if (rootnex_event_hdl == NULL)
		return (NDI_FAILURE);
	return (ndi_event_add_callback(rootnex_event_hdl, rdip,
		eventid, handler, arg, NDI_EVENT_NOPASS));
}

/*ARGSUSED*/
int
i_ddi_rootnex_remove_eventcall(dev_info_t *dip, dev_info_t *rdip,
	ddi_eventcookie_t eventid)
{
	if (rootnex_event_hdl == NULL)
		return (NDI_FAILURE);
	return (ndi_event_remove_callback(rootnex_event_hdl, rdip,
		eventid, NDI_EVENT_NOPASS));
}

/*ARGSUSED*/
int
i_ddi_rootnex_post_event(dev_info_t *dip, dev_info_t *rdip,
	ddi_eventcookie_t eventid, void *impl_data)
{
	int tag;

	if (rootnex_event_hdl == NULL)
		return (NDI_FAILURE);

	tag = ndi_event_cookie_to_tag(rootnex_event_hdl, eventid);
	if (tag == ROOTNEX_FAULT_EVENT) {
		(*plat_fault_handler)(rdip, impl_data);
		(*plat_fault_logger)(rdip, impl_data);
	}
	return (ndi_event_run_callbacks(rootnex_event_hdl, rdip,
		eventid, impl_data, NDI_EVENT_NOPASS));
}
