/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)autoconf.c	1.4	99/10/04 SMI"

/*
 * This file contains platform ddi functions that are common
 * to all existing platforms. These functions are originally
 * in autoconf.c and ddi_impl.c.
 *
 * The object file is linked into unix, not genunix.
 */

#include <sys/bootconf.h>
#include <sys/conf.h>
#include <sys/ddi_impldefs.h>
#include <sys/devfs_log_event.h>
#include <sys/hwconf.h>
#include <sys/instance.h>
#include <sys/kmem.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/systeminfo.h>

#ifdef	DEBUG
int configdebug;
#define	CPRINTF(x)	if (configdebug) printf x
#else
#define	CPRINTF(x)	/* nothing */
#endif

dev_info_t *top_devinfo;	/* root of device tree */
static char *rootname;		/* node name of top_devinfo */

/*
 * Call driver's identify(9e) entry point for (non-compliant)
 * 3rd party drivers that depend on it.
 *
 * This is settable in /etc/system
 */
int identify_9e = 1;

/*
 * Forward declarations
 */
static void impl_create_root_class(void);
static void create_devinfo_tree(void);
static void mta_init(void);

/*
 * DDI Boot Configuration
 */

/*
 * Setup the DDI but don't necessarilly init the DDI.  This will happen
 * later once /boot is released.
 */
void
setup_ddi(void)
{
	/*
	 * Initialize the instance number data base--this must be done
	 * after mod_setup and before the bootops are given up
	 */
	e_ddi_instance_init();

	impl_ddi_init_nodeid();
	impl_create_root_class();
	create_devinfo_tree();
	impl_ddi_callback_init();
	log_event_init();
	mta_init();
}

/*
 * Create classes and major number bindings for the name of my root.
 * Called immediately before 'loadrootmodules'
 */
static void
impl_create_root_class(void)
{
	major_t major;
	size_t size;
	char *cp;
	extern struct bootops *bootops;

	/*
	 * The name for the root nexus is exactly as the manufacturer
	 * placed it in the prom name property.  No translation.
	 */
	if ((major = ddi_name_to_major("rootnex")) == (major_t)-1)
		cmn_err(CE_PANIC, "Couldn't find major number for 'rootnex'");

	size = (size_t)BOP_GETPROPLEN(bootops, "mfg-name");
	rootname = kmem_zalloc(size, KM_SLEEP);
	(void) BOP_GETPROP(bootops, "mfg-name", rootname);

	/*
	 * Fix conflict between OBP names and filesystem names.
	 * Substitute '_' for '/' in the name.  Ick.  This is only
	 * needed for the root node since '/' is not a legal name
	 * character in an OBP device name.
	 */
	for (cp = rootname; *cp; cp++)
		if (*cp == '/')
			*cp = '_';

	add_class(rootname, "root");
	if (make_mbind(rootname, major, NULL, mb_hashtab) != 0) {
		cmn_err(CE_WARN, "A driver or driver alias has already "
		    "registered the name \"%s\".  The root nexus needs to "
		    "use this name, and will override the existing entry. "
		    "Please correct /etc/name_to_major and/or "
		    "/etc/driver_aliases and reboot.", rootname);

		/*
		 * Resort to the emergency measure of blowing away the
		 * existing hash entry and replacing it with rootname's.
		 */
		delete_mbind(rootname, mb_hashtab);
		if (make_mbind(rootname, major, NULL, mb_hashtab) != 0)
			cmn_err(CE_PANIC, "mb_hashtab: inconsistent state.");
	}

	/*
	 * The `platform' or `implementation architecture' name has been
	 * translated by boot to be proper for file system use.  It is
	 * the `name' of the platform actually booted.  Note the assumption
	 * is that the name will `fit' in the buffer platform (which is
	 * of size SYS_NMLN, which is far bigger than will actually ever
	 * be needed).
	 */
	(void) BOP_GETPROP(bootops, "impl-arch-name", platform);
}

/*
 * Note that this routine does not take into account the endianness
 * of the host or the device (or PROM) when retrieving properties.
 */
static int
getlongprop_buf(int id, char *name, char *buf, int maxlen)
{
	int size;

	size = prom_getproplen((dnode_t)id, name);
	if (size <= 0 || (size > maxlen - 1))
		return (-1);

	if (-1 == prom_getprop((dnode_t)id, name, buf))
		return (-1);

	/*
	 * Workaround for bugid 1085575 - OBP may return a "name" property
	 * without null terminating the string with '\0'.  When this occurs,
	 * append a '\0' and return (size + 1).
	 */
	if (strcmp("name", name) == 0) {
		if (buf[size - 1] != '\0') {
			buf[size] = '\0';
			size += 1;
		}
	}

	return (size);
}

/*ARGSUSED1*/
static int
get_neighbors(dev_info_t *di, caddr_t arg)
{
	register int nid, snid, cnid;
	dev_info_t *parent;
	char buf[OBP_MAXPROPNAME];

	if (di == NULL)
		return (DDI_WALK_CONTINUE);

	nid = ddi_get_nodeid(di);
	snid = (int)prom_nextnode((dnode_t)nid);
	cnid = (int)prom_childnode((dnode_t)nid);

	if (snid && (snid != -1) && ((parent = ddi_get_parent(di)) != NULL)) {
		/*
		 * add the first sibling that passes check_status()
		 */
		for (; snid && (snid != -1);
		    snid = (int)prom_nextnode((dnode_t)snid)) {
			if (getlongprop_buf(snid, OBP_NAME, buf,
			    OBP_MAXPROPNAME) > 0) {
				if (check_status(snid, buf, parent) ==
				    DDI_SUCCESS) {
					(void) ddi_add_child(parent, buf,
					    snid, -1);
					break;
				}
			}
		}
	}

	if (cnid && (cnid != -1)) {
		/*
		 * add the first child that passes check_status()
		 */
		if (getlongprop_buf(cnid, OBP_NAME, buf, OBP_MAXPROPNAME) > 0) {
			if (check_status(cnid, buf, di) == DDI_SUCCESS) {
				(void) ddi_add_child(di, buf, cnid, -1);
			} else {
				for (cnid = (int)prom_nextnode((dnode_t)cnid);
				    cnid && (cnid != -1);
				    cnid = (int)prom_nextnode((dnode_t)cnid)) {
					if (getlongprop_buf(cnid, OBP_NAME,
					    buf, OBP_MAXPROPNAME) > 0) {
						if (check_status(cnid, buf, di)
						    == DDI_SUCCESS) {
							(void) ddi_add_child(
							    di, buf, cnid, -1);
							break;
						}
					}
				}
			}
		}
	}

	return (DDI_WALK_CONTINUE);
}

static void
di_dfs(dev_info_t *devi, int (*f)(dev_info_t *, caddr_t), caddr_t arg)
{
	(void) (*f)(devi, arg);
	if (devi) {
		di_dfs((dev_info_t *)DEVI(devi)->devi_child, f, arg);
		di_dfs((dev_info_t *)DEVI(devi)->devi_sibling, f, arg);
	}
}

static void
create_devinfo_tree(void)
{
	major_t major;

	top_devinfo = kmem_zalloc(sizeof (struct dev_info), KM_SLEEP);

	DEVI(top_devinfo)->devi_node_name = rootname;
	DEVI(top_devinfo)->devi_instance = -1;
	i_ddi_set_binding_name(top_devinfo, rootname);

	DEVI(top_devinfo)->devi_nodeid = (int)prom_nextnode((dnode_t)0);
	DEVI(top_devinfo)->devi_node_class = DDI_NC_PROM;
	DEVI(top_devinfo)->devi_node_attributes = DDI_PERSISTENT;
	(void) impl_ddi_take_nodeid(DEVI(top_devinfo)->devi_nodeid, KM_SLEEP);

	mutex_init(&(DEVI(top_devinfo)->devi_lock), NULL, MUTEX_DEFAULT, NULL);

	major = ddi_name_to_major("rootnex");
	devnamesp[major].dn_head = top_devinfo;

	/*
	 * Record that devinfos have been made for "rootnex."
	 * di_dfs() is used to read the prom because it doesn't get the
	 * next sibling until the function returns, unlike ddi_walk_devs().
	 */
	di_dfs(ddi_root_node(), get_neighbors, 0);
}


/*
 * DDI Node Configuration
 */

/*
 * Add a child to nexus
 */
dev_info_t *
i_ddi_add_child(dev_info_t *pdip, char *name, uint_t nodeid, uint_t unit)
{
	struct dev_info *devi;

	devi = kmem_zalloc(sizeof (*devi), KM_SLEEP);
	devi->devi_node_name = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	(void) strcpy(devi->devi_node_name,  name);
	devi->devi_instance = unit;

	/*
	 * Assign devi_nodeid, devi_node_class and devi_node_attributes
	 * according to the following algorithm:
	 *
	 * nodeid arg		node_class		node_attributes
	 *
	 * DEVI_PSEUDO_NODEID	DDI_NC_PSEUDO		A
	 * DEVI_SID_NODEID	DDI_NC_PSEUDO		A,P
	 * other		DDI_NC_PROM		P
	 *
	 * Where A = DDI_AUTO_ASSIGNED_NODEID (auto-assign a nodeid)
	 * and	P = DDI_PERSISTENT
	 *
	 * auto-assigned nodeids are also auto-freed.
	 */
	switch ((int)nodeid) {
	case DEVI_SID_NODEID:
		devi->devi_node_attributes = DDI_PERSISTENT;
		/*FALLTHROUGH*/
	case DEVI_PSEUDO_NODEID:
		devi->devi_node_attributes |= DDI_AUTO_ASSIGNED_NODEID;
		devi->devi_node_class = DDI_NC_PSEUDO;
		if (impl_ddi_alloc_nodeid(&devi->devi_nodeid))
			cmn_err(CE_PANIC, "i_ddi_add_child: out of nodeids");
		break;
	default:
		devi->devi_nodeid = (int)nodeid;
		devi->devi_node_class = DDI_NC_PROM;
		devi->devi_node_attributes = DDI_PERSISTENT;
		(void) impl_ddi_take_nodeid((int)nodeid, KM_SLEEP);
		break;
	}

	/*
	 * Cache the value of the 'compatible' property from the prom,
	 * only if its a prom node.  NB: .conf file nodes must name a driver.
	 * NB: The binding name is set in ddi_append_dev.
	 */
	if (devi->devi_node_class == DDI_NC_PROM) {
		int length;
		static const char compat[] = "compatible";

		if ((length = prom_getproplen((dnode_t)nodeid,
		    (caddr_t)compat)) > 0) {
			devi->devi_compat_names =
			    kmem_zalloc((size_t)length, KM_SLEEP);
			(void) prom_getprop((dnode_t)nodeid, (caddr_t)compat,
			    devi->devi_compat_names);
			devi->devi_compat_length = (size_t)length;
		}
	}

	mutex_init(&(devi->devi_lock), NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&(devi->devi_pm_lock), NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&(devi->devi_pm_power_lock), NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&(devi->devi_pm_busy_lock), NULL, MUTEX_DEFAULT, NULL);
	cv_init(&(devi->devi_cv), NULL, CV_DEFAULT, NULL);

	ddi_append_dev(pdip, (dev_info_t *)devi);
	return ((dev_info_t *)devi);
}

/*
 * Remove a nexus child
 */
int
i_ddi_remove_child(dev_info_t *dip, int lockheld)
{
	major_t major;

	if ((dip == (dev_info_t *)0) || DDI_CF1(dip) ||
	    (DEVI(dip)->devi_child != (struct dev_info *)0) ||
	    (DEVI(dip)->devi_parent == (struct dev_info *)0)) {
		return (DDI_FAILURE);
	}

	/* remove dip from parent child list */
	i_ndi_devi_detach_from_parent(dip);

	major = ddi_name_to_major(ddi_get_name(dip));

	/*
	 * Remove unbound nodes from the orphanlist, if either the
	 * node is not bound or if the driver is not loaded.
	 * Remove 'dip' from the list held in the devnamesp table.
	 */
	i_ddi_remove_from_dn_list(&orphanlist, &orphanlist.dn_head, dip, 1, 0);

	if (major != (major_t)-1) {
		struct devnames *dnp = &(devnamesp[major]);
		i_ddi_remove_from_dn_list(dnp, &dnp->dn_head, dip, 1,
			lockheld);
	}

	/*
	 * Strip 'dip' clean and toss it over the side ..
	 */
	ddi_remove_minor_node(dip, NULL);
	impl_rem_dev_props(dip);
	impl_rem_hw_props(dip);
	mutex_destroy(&(DEVI(dip)->devi_lock));
	mutex_destroy(&(DEVI(dip)->devi_pm_lock));
	mutex_destroy(&(DEVI(dip)->devi_pm_power_lock));
	mutex_destroy(&(DEVI(dip)->devi_pm_busy_lock));
	cv_destroy(&(DEVI(dip)->devi_cv));

	i_ddi_set_binding_name(dip, NULL);
	if (DEVI(dip)->devi_compat_names)
		kmem_free(DEVI(dip)->devi_compat_names,
		    DEVI(dip)->devi_compat_length);

	if (i_ndi_dev_is_auto_assigned_node(dip))
		impl_ddi_free_nodeid(DEVI(dip)->devi_nodeid);

	kmem_free(DEVI(dip)->devi_node_name,
	    (size_t)(strlen(DEVI(dip)->devi_node_name) + 1));

	/* deallocate devi_last_addr */
	ASSERT(DEVI(dip)->devi_addr == NULL);
	ddi_set_name_addr(dip, NULL);

	kmem_free(dip, sizeof (struct dev_info));

	return (DDI_SUCCESS);
}

/*
 * This routine transforms either a prototype or canonical form 1 dev_info
 * node into a canonical form 2 dev_info node. If the transformation fails,
 * the node is removed.
 */
int
impl_proto_to_cf2(dev_info_t *dip)
{
	int error, circular;
	major_t major;
	struct devnames *dnp;

	major = ddi_name_to_major(ddi_get_name(dip));

	if ((major == (major_t)-1) || (mod_hold_dev_by_major(major) == NULL))
		return (DDI_FAILURE);

	/*
	 * Wait for or get busy/changing.  We need to stall here because
	 * of the alternate path for h/w devinfo nodes.
	 */
	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &circular);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	if (DDI_CF2(dip)) {
		error = DDI_SUCCESS;
		goto out;
	}

	if ((error = impl_initnode(dip)) != DDI_SUCCESS) {
		mod_rele_dev_by_major(major);
		goto out;
	}

	if ((error = impl_initdev(dip)) != DDI_SUCCESS) {
		mod_rele_dev_by_major(major);
	}

out:
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_exit_driver_list(dnp, circular);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
	return (error);
}

/*
 * This routine transforms a prototype dev_info node to canonical form 1,
 * and set the driver ops and assign instance.
 *
 * This is the portion of impl_proto_to_cf2() that needs to be single
 * threaded when doing concurrent driver probe/attach. Caller must
 * have single threaded driver entrance.
 */
int
impl_initnode(dev_info_t *dip)
{
	int error;

	/*
	 * If it's a prototype node, transform to CF1.
	 * Also set driver ops and assign instance.
	 */
	if ((error = ddi_initchild(ddi_get_parent(dip), dip)) == DDI_SUCCESS) {
		major_t major = ddi_name_to_major(ddi_get_name(dip));
		struct dev_ops *ops = devopsp[major];

		ASSERT(DEV_OPS_HELD(ops));

		DEVI(dip)->devi_ops = ops;
		DEVI(dip)->devi_instance = e_ddi_assign_instance(dip);
		return (DDI_SUCCESS);
	}

	/*
	 * Retain h/w devinfos, eliminate .conf file devinfos
	 */
	if (ndi_dev_is_persistent_node(dip) == 0)
		(void) ddi_remove_child(dip, 0);

	/*
	 * Translate error to DDI_FAILURE
	 */
	if (error == DDI_NOT_WELL_FORMED)
		error = DDI_FAILURE;

	return (error);
}

static int
impl_probe_attach_devi(dev_info_t *dev)
{
	int r;

	if (identify_9e != 0)
		(void) devi_identify(dev);

	switch (r = devi_probe(dev)) {
	case DDI_PROBE_DONTCARE:
	case DDI_PROBE_SUCCESS:
		break;
	default:
		return (r);
	}

	return (devi_attach(dev, DDI_ATTACH));
}

/*
 * This routine probe and attach a dev_info node.
 * If it fails, the node is removed.
 */
int
impl_initdev(dev_info_t *dev)
{
	struct dev_ops *ops;
	int r;

	ops = ddi_get_driver(dev);
	ASSERT(ops);
	ASSERT(DEV_OPS_HELD(ops));

	if ((r = impl_probe_attach_devi(dev)) == DDI_SUCCESS) {
		major_t major = ddi_name_to_major(ddi_get_name(dev));
		struct devnames *dnp = &devnamesp[major];
		ASSERT(major != (major_t)-1);

		e_ddi_keep_instance(dev);

		LOCK_DEV_OPS(&(dnp->dn_lock));
		dnp->dn_flags |= DN_DEVS_ATTACHED;
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (DDI_SUCCESS);
	}

	/*
	 * Partial probe or failed probe/attach...
	 * Retain leaf device driver nodes for deferred attach.
	 * (We need to retain the assigned instance number for
	 * deferred attach.  The call to e_ddi_free_instance is
	 * advisory -- it will retain the instance number if it's
	 * ever been kept before.)
	 */
	ddi_set_driver(dev, NULL);		/* dev --> CF1 */

	if (!NEXUS_DRV(ops)) {
		e_ddi_keep_instance(dev);
	} else {
		e_ddi_free_instance(dev);
		(void) ddi_uninitchild(dev);
		/*
		 * Retain h/w nodes in prototype form.
		 */
		if (ndi_dev_is_persistent_node(dev) == 0)
			(void) ddi_remove_child(dev, 0);
	}

	return (r);
}

/*
 * Bind this devinfo node to a driver.
 * If compat is NON-NULL, first try that ... failing that,
 * use the node-name.
 *
 * If we find a binding, set the binding name to the the string we used
 * and return the major number of the driver binding. If we don't find
 * a binding, we just bind to our own name, so the binding is always
 * set.  We try to rebind all unbound nodes when we load drivers.
 */
major_t
i_ddi_bind_node_to_driver(dev_info_t *dip)
{
	major_t maj;
	char *p = 0;
	void *compat;
	size_t len;

	compat = (void *)(DEVI(dip)->devi_compat_names);
	len = DEVI(dip)->devi_compat_length;

	while ((p = prom_decode_composite_string(compat, len, p)) != 0) {
		if ((maj = ddi_name_to_major(p)) != -1) {
			i_ddi_set_binding_name(dip, p);
			return (maj);
		}
	}

	i_ddi_set_binding_name(dip, ddi_node_name(dip));
	maj = ddi_name_to_major(ddi_node_name(dip));	/* -1 if unbound */
	return (maj);
}

int
i_ddi_initchild(dev_info_t *prnt, dev_info_t *proto)
{
	int (*f)(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);
	int error;

	ASSERT(prnt);
	ASSERT(DEVI(prnt) == DEVI(proto)->devi_parent);

	/*
	 * The parent must be in canonical form 2 in order to use its bus ops.
	 */
	if (impl_proto_to_cf2(prnt) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/*
	 * The parent must have a bus_ctl operation.
	 */
	if ((DEVI(prnt)->devi_ops->devo_bus_ops == NULL) ||
	    (f = DEVI(prnt)->devi_ops->devo_bus_ops->bus_ctl) == NULL) {
		/*
		 * Release the dev_ops which were held in impl_proto_to_cf2().
		 */
		ddi_rele_devi(prnt);
		return (DDI_FAILURE);
	}

	/*
	 * Invoke the parent's bus_ctl operation with the DDI_CTLOPS_INITCHILD
	 * command to transform the child to canonical form 1. If there
	 * is an error, ddi_remove_child should be called, to clean up.
	 */
	error = (*f)(prnt, prnt, DDI_CTLOPS_INITCHILD, proto, (void *)0);
	if (error != DDI_SUCCESS)
		ddi_rele_devi(prnt);
	else {
		/*
		 * Apply multi-parent/deep-nexus optimization to the new node
		 */
		ddi_optimize_dtree(proto);
	}

	return (error);
}

/*
 * i_find_node: Internal routine used by i_path_to_drv
 * to locate a given nodeid in the device tree.
 */
struct i_findnode {
	dnode_t	nodeid;
	dev_info_t *dip;
};

static int
i_find_node(dev_info_t *dev, void *arg)
{
	struct i_findnode *f = (struct i_findnode *)arg;

	if (ddi_get_nodeid(dev) == (int)f->nodeid) {
		f->dip = dev;
		return (DDI_WALK_TERMINATE);
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * i_path_to_drv:
 *
 * Return an alternate driver name binding for the leaf device
 * of the given pathname, if there is one. The purpose of this
 * function is to deal with generic pathnames. The default action
 * for platforms that can't do this (ie: x86 or any platform that
 * does not have prom_finddevice functionality, which matches
 * nodenames and unit-addresses without the drivers participation)
 * is to return NULL.
 *
 * Used in loadrootmodules() in the swapgeneric module to
 * associate a given pathname with a given leaf driver.
 *
 * Used in ddi_pathname_to_dev_t/bind_child in sunddi.c to
 * associate a given generic pathname with a given devinfo node.
 */

char *
i_path_to_drv(char *path)
{
	struct i_findnode fn;
	char *p, *q;

	/*
	 * Get the nodeid of the given pathname, if such a mapping exists.
	 */
	fn.nodeid = prom_finddevice(path);
	if (fn.nodeid == OBP_BADNODE) {
		CPRINTF(("i_path_to_drv: can't bind <%s>\n", path));
		return ((char *)0);
	}

	/*
	 * Find the nodeid in our copy of the device tree and return
	 * whatever name we used to bind this node to a driver.
	 */
	fn.dip = (dev_info_t *)0;

	rw_enter(&(devinfo_tree_lock), RW_READER);
	ddi_walk_devs(top_devinfo, i_find_node, (void *)(&fn));
	rw_exit(&(devinfo_tree_lock));

	/*
	 * We *must* have a copy of any given nodeid in our copy of
	 * the device tree, if finddevice returned one.
	 */
	ASSERT(fn.dip);

	/*
	 * If we're bound to something other than the nodename,
	 * note that in the message buffer and system log.
	 */
	p = ddi_binding_name(fn.dip);
	q = ddi_node_name(fn.dip);
	if (p && q && (strcmp(p, q) != 0))
		CPRINTF(("%s bound to %s\n", path, p));
	return (p);
}


/*
 * DDI interrupt
 */

/*
 * i_ddi_get_intrspec:  convert an interrupt number to an interrupt
 *			specification. The interrupt number determines which
 *			interrupt will be returned if more than one exists.
 *			returns an interrupt specification if successful and
 *			NULL if the interrupt specification could not be found.
 *			If "name" is NULL, first (and only) interrupt
 *			name is searched for.  this is the wrapper for the
 *			bus function bus_get_intrspec.
 */
ddi_intrspec_t
i_ddi_get_intrspec(dev_info_t *dip, dev_info_t *rdip, uint_t inumber)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/* request parent to return an interrupt specification */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_get_intrspec))(pdip,
	    rdip, inumber));
}

/*
 * i_ddi_add_intrspec:  Add an interrupt specification. If successful,
 *			the parameters "iblock_cookiep", "device_cookiep",
 *			"int_handler", "int_handler_arg", and return codes
 *			are set or used as specified in "ddi_add_intr". This
 *			is the wrapper for the bus function bus_add_intrspec.
 */
int
i_ddi_add_intrspec(dev_info_t *dip, dev_info_t *rdip, ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/* request parent to add an interrupt specification */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_add_intrspec))(pdip,
	    rdip, intrspec, iblock_cookiep, idevice_cookiep,
	    int_handler, int_handler_arg, kind));
}

/*
 * i_ddi_remove_intrspec: this is a wrapper for the bus function
 *			bus_remove_intrspec.
 */
void
i_ddi_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;

	/* request parent to remove an interrupt specification */
	(*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_remove_intrspec))(pdip,
	    rdip, intrspec, iblock_cookie);
}

/*
 * Misc implementation functions
 */

void
impl_rem_dev_props(dev_info_t *dip)
{
	ddi_prop_remove_all(dip);
	e_ddi_prop_remove_all(dip);
}

void
impl_rem_hw_props(dev_info_t *dip)
{
	ndi_prop_remove_all(dip);
}

/*
 * Code to search hardware layer (PROM), if it exists,
 * on behalf of child.
 *
 * if input dip != child_dip, then call is on behalf of child
 * to search PROM, do it via ddi_prop_search_common() and ascend only
 * if allowed.
 *
 * if input dip == ch_dip (child_dip), call is on behalf of root driver,
 * to search for PROM defined props only.
 *
 * Note that the PROM search is done only if the requested dev
 * is either DDI_DEV_T_ANY or DDI_DEV_T_NONE. PROM properties
 * have no associated dev, thus are automatically associated with
 * DDI_DEV_T_NONE.
 *
 * Modifying flag DDI_PROP_NOTPROM inhibits the search in the h/w layer.
 *
 * Returns DDI_PROP_FOUND_1275 if found to indicate to framework
 * that the property resides in the prom.
 */
int
impl_ddi_bus_prop_op(dev_t dev, dev_info_t *dip, dev_info_t *ch_dip,
    ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	int	len;
	caddr_t buffer;

	/*
	 * If requested dev is DDI_DEV_T_NONE or DDI_DEV_T_ANY, then
	 * look in caller's PROM if it's a self identifying device...
	 *
	 * Note that this is very similar to ddi_prop_op, but we
	 * search the PROM instead of the s/w defined properties,
	 * and we are called on by the parent driver to do this for
	 * the child.
	 */

	if (((dev == DDI_DEV_T_NONE) || (dev == DDI_DEV_T_ANY)) &&
	    ndi_dev_is_prom_node(ch_dip) &&
	    ((mod_flags & DDI_PROP_NOTPROM) == 0)) {
		len = prom_getproplen((dnode_t)DEVI(ch_dip)->devi_nodeid, name);
		if (len == -1) {
			return (DDI_PROP_NOT_FOUND);
		}

		/*
		 * If exists only request, we're done
		 */
		if (prop_op == PROP_EXISTS) {
			return (DDI_PROP_FOUND_1275);
		}

		/*
		 * If length only request or prop length == 0, get out
		 */
		if ((prop_op == PROP_LEN) || (len == 0)) {
			*lengthp = len;
			return (DDI_PROP_FOUND_1275);
		}

		/*
		 * Allocate buffer if required... (either way `buffer'
		 * is receiving address).
		 */

		switch (prop_op) {

		case PROP_LEN_AND_VAL_ALLOC:

			buffer = kmem_alloc((size_t)len,
			    mod_flags & DDI_PROP_CANSLEEP ?
			    KM_SLEEP : KM_NOSLEEP);
			if (buffer == NULL) {
				return (DDI_PROP_NO_MEMORY);
			}
			*(caddr_t *)valuep = buffer;
			break;

		case PROP_LEN_AND_VAL_BUF:

			if (len > (*lengthp)) {
				*lengthp = len;
				return (DDI_PROP_BUF_TOO_SMALL);
			}

			buffer = valuep;
			break;
		}

		/*
		 * Call the PROM function to do the copy.
		 */
		(void) prom_getprop((dnode_t)DEVI(ch_dip)->devi_nodeid,
			name, buffer);

		*lengthp = len; /* return the actual length to the caller */
		(void) impl_fix_props(dip, ch_dip, name, len, buffer);
		return (DDI_PROP_FOUND_1275);
	}

	return (DDI_PROP_NOT_FOUND);
}

/*
 * Reset all the pure leaf drivers on the system at halt time
 */
/*ARGSUSED1*/
static int
reset_leaf_device(dev_info_t *dev, void *arg)
{
	struct dev_ops *ops;

	if ((ops = DEVI(dev)->devi_ops) != (struct dev_ops *)0 &&
	    ops->devo_cb_ops != 0 && ops->devo_reset != nodev) {
		CPRINTF(("resetting %s%d\n", ddi_get_name(dev),
			ddi_get_instance(dev)));
		(void) devi_reset(dev, DDI_RESET_FORCE);
	}

	return (DDI_WALK_CONTINUE);
}

void
reset_leaves(void)
{
	ddi_walk_devs(top_devinfo, reset_leaf_device, 0);
}

/*
 * The following code is for doing multithreaded attach for selected drivers
 * defined by each platform.
 */

#ifdef	DEBUG
/*
 * for experimentation in debug kernel
 *	0 = use mta_drivers list
 *	1 = turn of stats printing
 *	2 = 1 + simulate mt attach, but do not launch threads
 *	3 = 1 + mt attach every driver in the system
 */
int mta_flag = 0;

static int time_diff_in_msec(timestruc_t, timestruc_t);
static void mta_print_stats(mta_handle_t *);
#endif DEBUG

char *mta_drivers;			/* settable in etc/system */
extern char *default_mta_drivers;	/* platform default list */

/*
 * A linked list of dev_info with the same parent
 */
struct dip_elem {
	struct dip_elem *next;
	dev_info_t *dip;
};

/*
 * A linked list of dip_elem's
 */
struct dip_list {
	struct dip_list *next;
	dev_info_t *parent;
	struct dip_elem *head;
	struct dip_elem *tail;
};

/*
 * Implementation of mta_handle
 */
struct mta_handle {
	struct dip_list *pplist;
	kmutex_t lock;
	kcondvar_t cv;
	int thr_count;
	major_t major;
#ifdef DEBUG
	int success_count;
	int success_time;
	int fail_count;
	int fail_time;
	int real_time;
#endif DEBUG
};

/*
 * Arg to be passed to threads
 */
struct mta_initdev {
	struct dip_elem *list;
	mta_handle_t *mta;
};

static void mta_attach_one_list(struct dip_elem *, mta_handle_t *);
static void mta_wait_attach(mta_handle_t *);
static void mta_impl_initdev_list(struct mta_initdev *);

/*
 * Initialize MT attach handles based on mta_drivers string
 */
static void
mta_init()
{
	major_t major;
	char *drvname, *end;

#ifdef DEBUG
	/*
	 * Enable MT Attach for all drivers if mta_flag == 3
	 * This is for testing purpose in debug kernel
	 */
	if (mta_flag == 3) {
		for (major = 0; major < devcnt; major++) {
			/*
			 * Skip empty entries
			 */
			if (devnamesp[major].dn_name == NULL)
				continue;

			devnamesp[major].dn_mta = kmem_zalloc(
			    sizeof (struct mta_handle), KM_SLEEP);
			devnamesp[major].dn_mta->major = major;
		}
		return;
	}
#endif DEBUG

	if (mta_drivers == NULL)
		mta_drivers = default_mta_drivers;

	/*
	 * Process a space separated list of driver names
	 */
	drvname = mta_drivers;
	end = drvname + strlen(drvname);
	while (*drvname != '\0') {
		char *tmp;

		tmp = drvname;
		while (*tmp && *tmp != ' ')
			tmp++;
		if (*tmp != '\0')
			*tmp = '\0';

		if ((major = ddi_name_to_major(drvname)) != (major_t)-1) {
			devnamesp[major].dn_mta = kmem_zalloc(
			    sizeof (struct mta_handle), KM_SLEEP);
		} else if (mta_drivers != default_mta_drivers) {
			/*
			 * Emit warning only if driver list came from
			 * /etc/system
			 */
			cmn_err(CE_WARN, "invalid MT attach driver: %s",
			    drvname);
		}

		drvname = tmp + 1;
		if (tmp < end)
			*tmp = ' ';
	}
}

/*
 * Get mta_handle_t
 *
 * Caller must have single threaded driver entry.
 * Should only be called once per MT attach session.
 */
mta_handle_t *
mta_get_handle(major_t major)
{
	struct devnames *dnp = &devnamesp[major];

	ASSERT(DN_BUSY_CHANGING(dnp->dn_flags));
	ASSERT(dnp->dn_busy_thread == curthread);

#ifdef DEBUG
	/*
	 * Reset stats in the handle for this session
	 */
	if (dnp->dn_mta != NULL) {
		bzero(dnp->dn_mta, sizeof (*dnp->dn_mta));
		dnp->dn_mta->major = major;
	}
#endif DEBUG

	return (dnp->dn_mta);
}

/*
 * Organize dev_info into one list per parent
 */
void
mta_add_dip(mta_handle_t *mta, dev_info_t *dip)
{
	struct dip_list *pplist;
	struct dip_elem *elem;
	dev_info_t *parent = ddi_get_parent(dip);

	/*
	 * Find out if we already have a dip list with the same parent.
	 */
	pplist = mta->pplist;
	while (pplist && pplist->parent != parent)
		pplist = pplist->next;

	if (pplist == NULL) {
		pplist = kmem_zalloc(sizeof (*pplist), KM_SLEEP);
		pplist->parent = parent;
		pplist->next = mta->pplist;
		mta->pplist = pplist;
	}

	/*
	 * Create dip_elem
	 */
	elem = kmem_alloc(sizeof (*elem), KM_SLEEP);
	elem->dip = dip;
	elem->next = NULL;

	/*
	 * Add to per-parent list. Preserve the probing order.
	 */
	if (pplist->tail == NULL) {
		pplist->head = pplist->tail = elem;
	} else {
		pplist->tail->next = elem;
		pplist->tail = elem;
	}
}

/*
 * Launch threads for probe-attach
 */
void
mta_attach_devi_list(mta_handle_t *mta)
{
	struct dip_list *pplist = mta->pplist;

#ifdef DEBUG
	timestruc_t start_time, end_time;
	start_time = hrestime;
#endif DEBUG

	while (pplist) {
		struct dip_list *pptmp = pplist->next;

		mta_attach_one_list(pplist->head, mta);
		kmem_free(pplist, sizeof (*pplist));
		pplist = pptmp;
	}

	mta_wait_attach(mta);

#ifdef DEBUG
	end_time = hrestime;
	mutex_enter(&mta->lock);
	mta->real_time = time_diff_in_msec(start_time, end_time);
	mutex_exit(&mta->lock);

	mta_print_stats(mta);
#endif DEBUG
}

/*
 * Launch a thread to probe/attach a list of dev_info belonging to
 * the same parent.
 *
 * If thread creation fails, do synchronous probe/attach.
 */
static void
mta_attach_one_list(struct dip_elem *list, mta_handle_t *mta)
{
	struct mta_initdev *arg;

	ASSERT(list != NULL);

	arg = kmem_alloc(sizeof (*arg), KM_SLEEP);
	arg->list = list;
	arg->mta = mta;

#ifdef DEBUG
	/*
	 * Dry run collecting stats in debug kernel, do not launch thread
	 */
	if (mta_flag == 2) {
		mta_impl_initdev_list(arg);
		return;
	}
#endif DEBUG

	/*
	 * The thread must belong to the current process because driver attach
	 * routine may modify process attributes. One example of this is the
	 * "sysmsg" driver, which opens "/dev/console", causing the current
	 * process to own the console.
	 *
	 * Early in the boot, the current process is p0.
	 *
	 * thr_count is decremented in mta_impl_initdev_list(), whether
	 * it executes synchronously or in a separate thread.
	 */

	mutex_enter(&mta->lock);
	mta->thr_count++;
	mutex_exit(&mta->lock);

	if (thread_create(NULL, DEFAULTSTKSZ, mta_impl_initdev_list,
	    (caddr_t)arg, 0, ttoproc(curthread), TS_RUN, 60) == NULL) {
		mta_impl_initdev_list(arg);
	}
}

/*
 * Wait for all probe/attach threads to finish
 */
static void
mta_wait_attach(mta_handle_t *mta)
{
	mutex_enter(&mta->lock);
	while (mta->thr_count > 0) {
		cv_wait(&mta->cv, &mta->lock);
	}
	mta->pplist = NULL;
	mutex_exit(&mta->lock);
}

/*
 * Probe and attach a list of dip's
 */
static void
mta_impl_initdev_list(struct mta_initdev *arg)
{
#ifdef DEBUG
	int s_count = 0, f_count = 0;
	int s_time = 0, f_time = 0;
#endif DEBUG

	struct dip_elem *list = arg->list;
	mta_handle_t *mta = arg->mta;

	kmem_free(arg, sizeof (*arg));

	while (list) {
		int error;
		struct dip_elem *tmp = list->next;

#ifdef DEBUG
		int time_in_msec;
		timestruc_t start_time, end_time;
		start_time = hrestime;
#endif DEBUG

		error = impl_initdev(list->dip);
		kmem_free(list, sizeof (*list));
		list = tmp;

#ifdef DEBUG
		/*
		 * Collect stats
		 */
		end_time = hrestime;
		time_in_msec = time_diff_in_msec(start_time, end_time);

		if (error == DDI_SUCCESS) {
			s_count++;
			s_time += time_in_msec;
		} else {
			f_count++;
			f_time += time_in_msec;
		}
#endif DEBUG
	}

	/*
	 * Collect stats and signal if thread count is down to 0
	 */
	mutex_enter(&mta->lock);
#ifdef DEBUG
	mta->success_count += s_count;
	mta->success_time += s_time;
	mta->fail_count += f_count;
	mta->fail_time += f_time;

	if (mta_flag == 2) {
		mutex_exit(&mta->lock);
		return;
	}
#endif DEBUG

	if (--mta->thr_count == 0) {
		cv_broadcast(&mta->cv);
	}
	mutex_exit(&mta->lock);
}

#ifdef DEBUG
static int
time_diff_in_msec(timestruc_t start, timestruc_t end)
{
	int nsec, sec;

	sec = end.tv_sec - start.tv_sec;
	nsec = end.tv_nsec - start.tv_nsec;
	if (nsec < 0) {
		nsec += NANOSEC;
		sec -= 1;
	}

	return (sec * (NANOSEC >> 20) + (nsec >> 20));
}

/*
 * Print mt attach stats
 */
void
mta_print_stats(mta_handle_t *mta)
{
	if (mta_flag == 0)
		return;

	mutex_enter(&mta->lock);

	cmn_err(CE_NOTE, "MTA_STATS %s success count %d, time %d msec",
	    ddi_major_to_name(mta->major), mta->success_count,
	    mta->success_time);
	cmn_err(CE_NOTE, "MTA_STATS %s failure count %d, time %d msec",
	    ddi_major_to_name(mta->major), mta->fail_count, mta->fail_time);
	cmn_err(CE_NOTE, "MTA_STATS %s total time %d, real time %d msec",
	    ddi_major_to_name(mta->major),
	    mta->success_time + mta->fail_time, mta->real_time);

	if ((mta_flag != 2) && mta->real_time) {
		int ratio = (mta->success_time + mta->fail_time) * 100 /
		    mta->real_time;
		cmn_err(CE_NOTE, "MTA_STATS %s compression factor = %d%%",
		    ddi_major_to_name(mta->major), ratio);
	}

	mutex_exit(&mta->lock);
}
#endif DEBUG
