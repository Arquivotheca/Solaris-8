/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootdev.c	1.17	99/11/01 SMI"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pathname.h>
#include <sys/autoconf.h>
#include <sys/sunndi.h>

/*
 * internal functions
 */
static char *get_devfs_name(char *bootstr, char *buffer);
static void parse_name(char *, char **, char **, char **);
static dev_info_t *find_alternate_node(dev_info_t *, char *);
static int i_find_node(dev_info_t *, void *);
static int handle_naming_exceptions(dev_info_t *parent_dip,
	struct pathname *pn, char *ret_buf, char *component);
static int validate_dip(dev_info_t *dip, char *addr, int hold);
static dev_info_t *srch_child_addrs(dev_info_t *parent_dip, char *addr,
	major_t maj);
static dev_info_t *srch_child_names(dev_info_t *parent_dip, char *nodename,
	char *addr);
static void clone_dev_fix(dev_info_t *dip, char *curpath);
static major_t path_to_major(char *parent_path, char *leaf_name);

/* External function prototypes */
extern char *i_binding_to_drv_name(char *);

/* internal global data */
static struct modlmisc modlmisc = {
	&mod_miscops, "bootdev misc module 1.17"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	/*
	 * misc modules are not safely unloadable: 1170668
	 */
	return (EBUSY);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * given an absolute pathname, convert it, if possible, to a devfs
 * name.  Examples:
 * /dev/rsd3a to /pci@1f,4000/glm@3/sd@0,0:a
 * /dev/dsk/c0t0d0s0 to /pci@1f,4000/glm@3/sd@0,0:a
 * /devices/pci@1f,4000/glm@3/sd@0,0:a to /pci@1f,4000/glm@3/sd@0,0:a
 *
 * This routine deals with symbolic links. Return a pointer to buffer
 * on success or NULL on failure.
 */
static char *
get_devfs_name(char *bootstr, char *buffer)
{
	vnode_t *node;
	char *ret;

	if (lookupname(bootstr, UIO_SYSSPACE, FOLLOW, NULL, &node))
		return (NULL);

	if (node->v_type != VCHR && node->v_type != VBLK) {
		VN_RELE(node);
		return (NULL);
	}

	/* make sure this is really a device. */
	if (getmajor(node->v_rdev) >= devcnt) {
		VN_RELE(node);
		return (NULL);
	}
	if (ddi_dev_pathname(node->v_rdev, buffer) == DDI_FAILURE)
		ret = NULL;
	else
		ret = buffer;

	VN_RELE(node)
	return (ret);
}

/*
 * i_find_node: Internal routine used by i_path_to_drv
 * to locate a given nodeid in the device tree.
 */
struct i_findnode {
	dnode_t nodeid;
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
 * Handle cases where we are not able to use prom_finddevice
 * to verify the validity of a device path in the prom.
 * Currently there is only 1 such case...
 * for plutos, the following translation is necessary....
 *	/devices/.../SUNW,soc@a,b/SUNW,pln@k,j/ssd@1,0
 *		to
 *	/SUNW,soc@a,b/SUNW,pln@k,j/SUNW,ssd@1,0
 * However, the nodes below SUNW,soc may never be created by the
 * prom - they are only probed out by the prom if they are explicitly
 * opened (for example, as result of booting from an ssd).  Since
 * the PROM nodes for SUNW,pln and SUNW,soc may not be in the kernel's
 * device tree, there may be no way to convert ssd --> SUNW,ssd.
 * We handle this as a special case and handcraft the name.
 * Note that the device does exist, since we had to derive the
 * original pathname from a valid dev_t that exists in the kernel's device
 * tree.
 *
 * parent_dip is dip of the last component that prom_finddevice recognized
 * pn is the pathname buffer with the remaining components contained in it.
 * ret_buf contains the current path components we have parsed including the
 * component that caused us to fail the prom_finddevice call.
 * component is used as a buffer for the pn operations.
 */
static int
handle_naming_exceptions(dev_info_t *parent_dip, struct pathname *pn,
	char *ret_buf, char *component)
{
	char *soc_driver = "soc";
	char *ssd_driver = "ssd";
	char *sunw_prefix = "SUNW,";

	/*
	 * if the last valid node we saw was not SUNW,soc, then
	 * we know we can reject this as an invalid device path
	 */
	if (ddi_name_to_major(ddi_get_name(parent_dip)) !=
	    ddi_name_to_major(soc_driver)) {
		return (-1);
	}

	pn_skipslash(pn);
	/* substitute SUNW,ssd for ssd in the path name */
	while (pn_pathleft(pn)) {
		(void) pn_getcomponent(pn, component);
		(void) strcat(ret_buf, "/");
		if (strncmp(component, ssd_driver, strlen(ssd_driver)) == 0) {
			(void) strcat(ret_buf, sunw_prefix);
		}
		(void) strcat(ret_buf, component);
		pn_skipslash(pn);
	}
	return (0);
}

/*
 * translate a devfs pathname to one that will be acceptable
 * by the prom.  In most cases, there is no translation needed.
 * However, in a few cases, there is some translation needed:
 * obp 1.x only understands devices names of the form dd(c,u,p).
 * For systems supporting generically named devices, the prom
 * may support nodes such as 'disk' that do not have any unit
 * address information (i.e. target,lun info).  If this is the
 * case, the ddi framework will reject the node as invalid and
 * populate the devinfo tree with nodes froms the .conf file
 * (e.g. sd).  In this case, the names that show up in /devices
 * are sd - since the prom only knows about 'disk' nodes, this
 * routine detects this situation and does the conversion
 * There are also cases such as pluto where the disk node in the
 * prom is named "SUNW,ssd" but in /devices the name is "ssd".
 *
 * return a 0 on success with the new device string in ret_buf.
 * Otherwise return the appropriate error code as we may be called
 * from the openprom driver.
 */
int
i_devname_to_promname(char *dev_name, char *ret_buf)
{
	dnode_t pnode;
	dnode_t parent_pnode;
	dev_info_t *dip;
	struct pathname pn;
	char *sub_path, *component;
	char *unit_address, *minorname, *nodename, *ptr;
	struct i_findnode fn;
	major_t maj;

	/* do some sanity checks */
	if (dev_name == NULL) {
		return (EINVAL);
	}
	if (ret_buf == NULL) {
		return (EINVAL);
	}
	if (*dev_name != '/') {
		return (EINVAL);
	}

	if (strlen(dev_name) > MAXPATHLEN) {
		return (EINVAL);
	}

	ptr = strchr(dev_name, ':');
	if ((ptr != NULL) && (strchr(ptr, '/'))) {
		return (EINVAL);
	}

	/* allocate buffers */
	sub_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	component = kmem_alloc(MAXNAMELEN, KM_SLEEP);

	/*
	 * convert to a devfs name.  If the conversion fails, we
	 * we keep going.  This is to support any users of reboot
	 * who want to use this call to pass device arguments with the
	 * /devices arguments already stripped off.
	 */
	if (get_devfs_name(dev_name, sub_path) != NULL)
		dev_name = sub_path;

	/*
	 * try a finddevice on the entire name - if it succeeds,
	 * we know we have a valid name.
	 */
	pnode = prom_finddevice(dev_name);
	if (pnode != OBP_BADNODE) {
		(void) strcpy(ret_buf, dev_name);
		kmem_free(sub_path, MAXPATHLEN);
		kmem_free(component, MAXNAMELEN);
		return (0);
	}

	/*
	 * if we get here, then some portion of the device path is
	 * not understood by the prom.  We need to determine if
	 * there is a portion of the path that could be replaced with
	 * with an alternate node name (e.g. replace sd with disk).
	 * We do this by doing a prom_finddevice on each level of the
	 * device path starting at the root until we hit a point in
	 * the path where the prom_finddevice fails.  Then we check
	 * the devinfo nodes at this level for a possible subistute
	 * node.  If we find one, we substitute the name, try
	 * prom_finddevice, and continue parsing the path if it was
	 * successful
	 */
	if (pn_get(dev_name, UIO_SYSSPACE, &pn)) {
		kmem_free(sub_path, MAXPATHLEN);
		kmem_free(component, MAXNAMELEN);
		return (EIO);
	}
	pn_skipslash(&pn);

	/* we need to keep track of the last parent node we visited */
	parent_pnode = (dnode_t)ddi_get_nodeid(ddi_root_node());

	/* buffer for the prom device path we are building */
	sub_path[0] = '\0';

	/* for each component in the path ... */
	while (pn_pathleft(&pn)) {
		(void) pn_getcomponent(&pn, component);
		(void) strcat(sub_path, "/");
		(void) strcat(sub_path, component);

		pnode = prom_finddevice(sub_path);
		if (pnode == OBP_BADNODE) {
			/*
			 * the prom does not understand this portion of
			 * the path.  check for a substitue name.
			 */
			parse_name(component, &nodename, &unit_address,
			    &minorname);

			/*
			 * first find the parent devinfo node so we can
			 * look at all of his children
			 */
			fn.nodeid = parent_pnode;
			fn.dip = (dev_info_t *)0;
			rw_enter(&(devinfo_tree_lock), RW_READER);
			ddi_walk_devs(top_devinfo, i_find_node, (void *)(&fn));
			rw_exit(&(devinfo_tree_lock));

			/*
			 * We *must* have a copy of any given nodeid in our copy
			 * of the device tree, if finddevice returned one.
			 */
			ASSERT(fn.dip);

			maj = ddi_name_to_major(ddi_binding_name(fn.dip));
			if ((maj != (major_t)-1) &&
			    (ddi_hold_installed_driver(maj) != NULL)) {
				(void) e_ddi_deferred_attach(maj, NODEV);
				/* check for an alternate node */
				dip = find_alternate_node(fn.dip, nodename);
				ddi_rele_driver(maj);
			} else {
				dip = NULL;
			}

			/*
			 * if there was none, then we deal with an exceptions
			 * and return failure if this still cannot resolve
			 * the path name
			 */
			if (dip == NULL) {
				if (handle_naming_exceptions(fn.dip, &pn,
				    sub_path, component) == 0) {
					break;
				}
				kmem_free(sub_path, MAXPATHLEN);
				kmem_free(component, MAXNAMELEN);
				pn_free(&pn);
				return (EINVAL);
			}
			/*
			 * substitute the new name in for the old and try the
			 * prom_finddevice for it.
			 */
			ptr = strrchr(sub_path, '/');
			*(ptr + 1) = '\0';
			(void) strcat(sub_path, ddi_node_name(dip));
			if ((unit_address) && (*unit_address)) {
				(void) strcat(sub_path, "@");
				(void) strcat(sub_path, unit_address);
			}
			if ((minorname) && (*minorname)) {
				(void) strcat(sub_path, ":");
				(void) strcat(sub_path, minorname);
			}
			if (prom_finddevice(sub_path) == OBP_BADNODE) {
				kmem_free(sub_path, MAXPATHLEN);
				kmem_free(component, MAXNAMELEN);
				pn_free(&pn);
				return (EINVAL);
			}
		}
		/*
		 * move on down to the next path component.
		 */
		parent_pnode = pnode;
		pn_skipslash(&pn);
	}
	/* success */
	(void) strcpy(ret_buf, sub_path);
	kmem_free(sub_path, MAXPATHLEN);
	kmem_free(component, MAXNAMELEN);
	pn_free(&pn);
	return (0);
}

/*
 * check for a possible substitute node.  This routine searches the
 * children of parent_dip, looking for a node that:
 *	1. is a prom node
 *	2. has a node name different from devfs_nodenamE(otherwise
 *		we might return the node that we are trying to find
 *		a substitute for)
 *	3. has a binding name that maps to the same major number as
 *		devfs_nodename (the node we are	trying to find a substitute
 *		for.)
 *	4. there is no need to verify that the unit-address information
 *		match since it is likely that the substitute node
 *		will have none (e.g. disk) - this would be the reason the
 *		framework rejected it in the first place.
 *
 * assumes parent_dip is held
 */
static dev_info_t *
find_alternate_node(dev_info_t *parent_dip, char *devfs_nodename)
{
	dev_info_t *child_dip;
	major_t alt_node_major, node_major;
	int found = 0;

	/*
	 * this should probably never be the case
	 */
	if ((node_major = ddi_name_to_major(devfs_nodename)) == -1)
		return (NULL);

	/* loop thru the kids */
	rw_enter(&(devinfo_tree_lock), RW_READER);
	for (child_dip = ddi_get_child(parent_dip); child_dip != NULL;
	    child_dip = ddi_get_next_sibling(child_dip)) {

		/* if its not a prom node, we keep going */
		if (ndi_dev_is_prom_node(child_dip) == 0)
			continue;

		/*
		 * if the devinfo node name and the name from /devices
		 * match, we can skip this node since we are looking
		 * for a node that is not in /devices and therefore would
		 * ghave a different node name
		 */
		if (strcmp(ddi_node_name(child_dip), devfs_nodename) == 0)
			continue;

		/*
		 * if we get here and the binding names of this node
		 * maps to the same major number as devfs_nodename,
		 * we have a match and we are done.
		 */
		alt_node_major = ddi_name_to_major(ddi_binding_name(child_dip));
		if (alt_node_major == node_major) {
			found = 1;
			break;
		}
	}
	rw_exit(&(devinfo_tree_lock));
	if (found) {
		return (child_dip);
	} else {
		/* fail */
		return (NULL);
	}
}

/*
 * break device@a,b:minor into components
 */
static void
parse_name(char *name, char **drvname, char **addrname, char **minorname)
{
	register char *cp, ch;
	static char nulladdrname[] = ":\0";

	cp = *drvname = name;
	*addrname = *minorname = NULL;
	while ((ch = *cp) != '\0') {
		if (ch == '@')
			*addrname = ++cp;
		else if (ch == ':')
			*minorname = ++cp;
		++cp;
	}
	if (!*addrname)
		*addrname = &nulladdrname[1];
	*((*addrname)-1) = '\0';
	if (*minorname)
		*((*minorname)-1) = '\0';
}

/*
 * convert a prom device path to an equivalent path in /devices
 * Does not deal with aliases.  Does deal with pathnames which
 * are not fully qualified.  This routine is generalized
 * to work across several flavors of OBP
 */
int
i_promname_to_devname(char *prom_name, char *ret_buf)
{
	dev_info_t *parent_dip, *child_dip;
	char *sub_path;
	char *prom_sub_path;
	struct pathname pn;
	char *ptr;
	char *ua;
	char *component;
	char *unit_address;
	char *minorname;
	char *nodename;
	major_t child_maj, parent_maj;

	parent_maj = (major_t)-1;

	if (prom_name == NULL) {
		return (EINVAL);
	}
	if (ret_buf == NULL) {
		return (EINVAL);
	}
	if (strlen(prom_name) >= MAXPATHLEN) {
		return (EINVAL);
	}

	if (*prom_name != '/') {
		return (EINVAL);
	}

	ptr = strchr(prom_name, ':');
	if ((ptr != NULL) && (strchr(ptr, '/'))) {
		return (EINVAL);
	}

	/* allocate some buffers */
	sub_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

	prom_sub_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

	component = kmem_alloc(MAXNAMELEN, KM_SLEEP);

	ua = kmem_alloc(MAXNAMELEN, KM_SLEEP);

	/* time to walk to tree */
	if (pn_get(prom_name, UIO_SYSSPACE, &pn)) {
		kmem_free(sub_path, MAXPATHLEN);
		kmem_free(prom_sub_path, MAXPATHLEN);
		kmem_free(component, MAXNAMELEN);
		kmem_free(ua, MAXNAMELEN);
		return (EIO);
	}

	pn_skipslash(&pn);
	parent_dip = ddi_root_node();
	sub_path[0] = '\0';
	prom_sub_path[0] = '\0';

	/*
	 * walk the device tree attempting to find a node in the
	 * tree for each component in the path
	 */
	while (pn_pathleft(&pn)) {
		(void) pn_getcomponent(&pn, component);
		/* keep track of were we current are in the prom path */
		(void) strcat(prom_sub_path, "/");
		(void) strcat(prom_sub_path, component);

		parse_name(component, &nodename, &unit_address, &minorname);

		if ((unit_address == NULL) || (*unit_address == '\0')) {
			/*
			 * if the path component has no unit address info,
			 * i.e. a name that is not fully qualified, we
			 * search through the list of children comparing
			 * names.
			 * srch_child_names fills in the correct unit address
			 * info for us if it exists.
			 *
			 * the driver for child_dip is held
			 */
			unit_address = ua;
			child_dip = srch_child_names(parent_dip, nodename,
			    unit_address);
		} else {
			/*
			 * we have the unit address information so we can
			 * just search through the children looking for a
			 * matching address.
			 *
			 * we need to get the major number of the node
			 * we are looking for first - this is what
			 * path_to_major does.
			 *
			 * the function may fill modify unit_address if
			 * the one we pass it is not fully qualified.
			 *
			 * the driver for child_dip is held
			 */

			if ((child_maj = path_to_major(prom_sub_path, nodename))
			    != (major_t)-1) {
				(void) strcpy(ua, unit_address);
				unit_address = ua;
				child_dip = srch_child_addrs(parent_dip,
				    unit_address, child_maj);
			} else {
				child_dip = NULL;
			}
		}
		/* did we find a corresponding dip for the path component? */
		if (child_dip == NULL) {
			if (parent_maj != (major_t)-1) {
				ddi_rele_driver(parent_maj);
			}
			kmem_free(sub_path, MAXPATHLEN);
			kmem_free(prom_sub_path, MAXPATHLEN);
			kmem_free(component, MAXNAMELEN);
			kmem_free(ua, MAXNAMELEN);
			pn_free(&pn);
			return (EINVAL);
		}

		child_maj = ddi_name_to_major(ddi_binding_name(child_dip));
		ASSERT(child_maj != (major_t)-1);

		/* continue building the devfs path */
		(void) strcat(sub_path, "/");
		(void) strcat(sub_path, ddi_node_name(child_dip));
		if ((unit_address != NULL) && (*unit_address != '\0')) {
			(void) strcat(sub_path, "@");
			(void) strcat(sub_path, unit_address);
		}
		if ((minorname != NULL) && (*minorname != '\0')) {
			(void) strcat(sub_path, ":");
			(void) strcat(sub_path, minorname);
		}
		/* move on to the next component */
		if (parent_maj != (major_t)-1) {
			ddi_rele_driver(parent_maj);
		}
		parent_maj = child_maj;
		parent_dip = child_dip;
		pn_skipslash(&pn);
	}
	/*
	 * if we get here, every component in the path matched a node
	 * in the devinfo tree.  So we have a valid path.
	 *
	 * if we matched to a clone device, modify the path to reflect the
	 * correct clone device node and finish up.
	 */
	clone_dev_fix(child_dip, sub_path);
	ddi_rele_driver(child_maj);
	(void) strcpy(ret_buf, sub_path);
	kmem_free(sub_path, MAXPATHLEN);
	kmem_free(prom_sub_path, MAXPATHLEN);
	kmem_free(component, MAXNAMELEN);
	kmem_free(ua, MAXNAMELEN);
	pn_free(&pn);
	return (0);
}

/*
 * take a dip which does not have any external representation in
 * /devices because it is using the clone driver and convert it
 * to the proper clone device name.
 *
 * dip must be held.
 */
static void
clone_dev_fix(dev_info_t *dip, char *curpath)
{
	struct ddi_minor_data *dmn;
	struct ddi_minor_data *save_dmn = NULL;
	dev_info_t *clone_dip;
	major_t clone_major;
	char *clone_path;

	mutex_enter(&(DEVI(dip)->devi_lock));
	/*
	 * we will return if this dip does have some sort of external
	 * pathname representation in /devices. If we find only
	 * minor nodes of type, DDM_ALIAS, we have a clone device and
	 * will convert the name.
	 */
	for (dmn = DEVI(dip)->devi_minor; dmn; dmn = dmn->next) {
		if ((dmn->type == DDM_MINOR) ||
		    (dmn->type == DDM_DEFAULT)) {
			mutex_exit(&(DEVI(dip)->devi_lock));
			return;
		}
		if (dmn->type == DDM_ALIAS) {
			save_dmn = dmn;
		}
	}
	mutex_exit(&(DEVI(dip)->devi_lock));

	if (save_dmn == NULL) {
		return;
	}

	/* convert to the clone device representation */
	clone_major = ddi_name_to_major("clone");
	if (clone_major == (major_t)-1) {
		return;
	}
	if (ddi_hold_installed_driver(clone_major) == NULL) {
		return;
	}
	/* get the clone dip */
	clone_dip = ddi_find_devinfo("clone", -1, 1);
	if (clone_dip == NULL) {
		ddi_rele_driver(clone_major);
		return;
	}

	clone_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

	mutex_enter(&(DEVI(dip)->devi_lock));

	/* make sure its still there */
	for (dmn = DEVI(dip)->devi_minor; dmn; dmn = dmn->next) {
		if (dmn == save_dmn) {
			break;
		}
	}

	if (dmn == NULL) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		ddi_rele_driver(clone_major);
		kmem_free(clone_path, MAXPATHLEN);
		return;
	}
	/* build the pathname for the clone and append minor name info */
	if (ddi_pathname(clone_dip, clone_path) != NULL) {
		(void) sprintf(curpath, "%s:%s", clone_path,
							save_dmn->ddm_aname);
	}
	mutex_exit(&(DEVI(dip)->devi_lock));
	ddi_rele_driver(clone_major);
	kmem_free(clone_path, MAXPATHLEN);
}

/*
 * search for 'name' in all of the children of 'dip'.
 * also return the correct address information in 'addr'
 *
 * The assumption here is that we have do not have an address to
 * compare with...so if we find more than one child with nodename,
 * we fail, since there is no way to determine which path we
 * should follow.
 *
 * since we are called in order to convert from a prom name
 * to a devfs name, we have to handle some special cases here.
 *
 * the parent dip, dip, must be held by the caller.
 *
 * the dip returned is held.
 */
static dev_info_t *
srch_child_names(dev_info_t *parent_dip, char *nodename, char *addr)
{
	char *ptr;
	dev_info_t *child_dip;
	dev_info_t *save_dip = NULL;
	int unique = 1;

	/*
	 * look for a straight match... comparing against node name
	 */
	rw_enter(&(devinfo_tree_lock), RW_READER);
	for (child_dip = ddi_get_child(parent_dip);
	    child_dip != NULL;
	    child_dip = ddi_get_next_sibling(child_dip)) {

		if (strcmp(ddi_node_name(child_dip), nodename) == 0) {
			/* not unique, then fail */
			if (save_dip != NULL) {
				unique = 0;
				break;
			} else {
				save_dip = child_dip;
				unique = 1;
			}
		}
	}
	rw_exit(&(devinfo_tree_lock));

	/*
	 * we exited the loop for one of 3 reasons:
	 * 1. we found multiple instances of the same name - there is no
	 * we can accurately determine which of the instances is the
	 * correct one so we fail.
	 * 2. We found a single node that matches
	 * 3. We found no nodes that match
	 */

	if (!unique) {
		/* couldn't find a *unique* instance of the name */
		return (NULL);
	}
	if (save_dip != NULL) {

		/*
		 * if we found a single node that matched nodename.
		 * We might have found...
		 *
		 * 1. the exact match (the dip is valid)
		 * 2. a generic name(disk) that is specific in devfs(sd)
		 * (the dip we found is not valid(disk), but there may be
		 * an equivalent dip that was created from a .conf file).
		 * 3. an alias(SUNW,ssd) that is the driver name in devfs(ssd)
		 * (the dip we found is not valid(disk), but there may be
		 * an equivalent dip that was created from a .conf file).
		 */

		if (validate_dip(save_dip, addr, 1)) {
			/* a exact match */
			return (save_dip);
		}
		/*
		 * get the driver name and look for a single node with the
		 * driver name as the node name.
		 * for cases 2) and 3) above
		 */
		ptr = i_binding_to_drv_name(ddi_binding_name(save_dip));
	} else {
		/*
		 * no name match...could still be a valid name though...
		 * the prom may not have *needed* to create a node for this
		 * device if we did not boot from it.  If it is a device
		 * such as SUNW,ssd that has its devinfo nodes created from
		 * a .conf file (and prom node is discarded), we should check
		 * for the existence of a node with the driver name (ssd).
		 */
		ptr = i_binding_to_drv_name(nodename);
	}
	if (ptr == NULL)
		return (NULL);
	unique = 0;
	rw_enter(&(devinfo_tree_lock), RW_READER);
	for (child_dip = ddi_get_child(parent_dip);
	    child_dip != NULL;
	    child_dip = ddi_get_next_sibling(child_dip)) {
		if (strcmp(ddi_node_name(child_dip), ptr) == 0) {
			if (save_dip != NULL) {
				unique = 0;
				break;
			} else {
				unique = 1;
				save_dip = child_dip;
			}
		}
	}
	rw_exit(&(devinfo_tree_lock));
	/* could not find unique instance */
	if (!unique) {
		return (NULL);
	}
	/* nothing matched - quit */
	if (save_dip == NULL)
		return (NULL);

	/* we found something - let's see if its valid. */
	if (validate_dip(save_dip, addr, 1))
		return	(save_dip);

	return (NULL);
}

/*
 * valid means:
 *      - a driver can be loaded and attached to the instance of the
 *	      device this dip represents.
 * we also optionally return the address information for the node in addr
 * we are either passed a dip which is held and locked (hold = 0)
 * or we are asked to hold the driver and lock the dip (hold = 1)
 *
 * regardless, if successful, the driver for the dip will be returned held.
 */
static int
validate_dip(dev_info_t *dip, char *addr, int hold)
{
	major_t major_no;
	int ret = 0, circular;
	char *ptr;
	struct devnames *dnp;

	if (hold) {
		major_no = ddi_name_to_major(ddi_binding_name(dip));
		if (major_no == (major_t)-1) {
			return (0);
		}
		if (ddi_hold_installed_driver(major_no) == NULL) {
			return (0);
		}
		(void) e_ddi_deferred_attach(major_no, NODEV);

		dnp = &devnamesp[major_no];
		LOCK_DEV_OPS(&dnp->dn_lock);
		e_ddi_enter_driver_list(dnp, &circular);
		UNLOCK_DEV_OPS(&dnp->dn_lock);
	}
	if (DDI_CF2(dip)) {
		ret = 1;
		/*
		 * fill in the unit address information for the
		 * caller if they passed us a buffer
		 */
		if (addr != NULL) {
			ptr = ddi_get_name_addr(dip);
			if (ptr == NULL) {
				*addr = '\0';
			} else {
				(void) strcpy(addr, ptr);
			}
		}
	}
	if (hold) {
		LOCK_DEV_OPS(&dnp->dn_lock);
		e_ddi_exit_driver_list(dnp, circular);
		UNLOCK_DEV_OPS(&dnp->dn_lock);

		if (ret == 0) {
			ddi_rele_driver(major_no);
		}
	}
	return (ret);
}


/*
 * search thru the children of 'dip' looking for an address that
 * matches 'addr'.
 * dip (parent dip) must be held.
 * major is the major number of the child we are looking for
 * addr is the address of the node we are looking for.
 *
 * the dip returned is held.
 */
static dev_info_t *
srch_child_addrs(dev_info_t *parent_dip, char *addr, major_t major)
{
	dev_info_t *child_dip;
	char *ptr;
	char *wildcard_addr;
	char *wildcard = ",0";
	uint_t len;
	struct devnames *dnp;
	int circular;

	if (major == (major_t)-1) {
		return (NULL);
	}
	if (ddi_hold_installed_driver(major) == NULL)  {
		return (NULL);
	}
	(void) e_ddi_deferred_attach(major, NODEV);

	dnp = &devnamesp[major];
	LOCK_DEV_OPS(&dnp->dn_lock);
	e_ddi_enter_driver_list(dnp, &circular);
	UNLOCK_DEV_OPS(&dnp->dn_lock);

	for (child_dip = dnp->dn_head; child_dip != NULL;
	    child_dip = ddi_get_next(child_dip))  {
		if (ddi_get_parent(child_dip) != parent_dip) {
			continue;
		}
		if (validate_dip(child_dip, NULL, 0) == 0) {
			continue;
		}
		ptr = ddi_get_name_addr(child_dip);
		if ((ptr != NULL) && (strcmp(ptr, addr) == 0)) {
			break;
		}
	}

	LOCK_DEV_OPS(&dnp->dn_lock);
	e_ddi_exit_driver_list(dnp, circular);
	UNLOCK_DEV_OPS(&dnp->dn_lock);

	if (child_dip != NULL) {
		return (child_dip);
	}
	/*
	 * no matching addresses were found if we get here.
	 * so we relax our check and check do a wildcard check -
	 * append a ",0" to addr and try the search again.
	 */
	len = strlen(addr) + strlen(wildcard) + 1;
	wildcard_addr = kmem_alloc(len, KM_SLEEP);
	(void) sprintf(wildcard_addr, "%s,0", addr);

	LOCK_DEV_OPS(&dnp->dn_lock);
	e_ddi_enter_driver_list(dnp, &circular);
	UNLOCK_DEV_OPS(&dnp->dn_lock);

	for (child_dip = dnp->dn_head; child_dip != NULL;
	    child_dip = ddi_get_next(child_dip))  {
		if (ddi_get_parent(child_dip) != parent_dip) {
			continue;
		}
		if (validate_dip(child_dip, NULL, 0) == 0) {
			continue;
		}
		ptr = ddi_get_name_addr(child_dip);
		if ((ptr != NULL) && (strcmp(ptr, wildcard_addr) == 0)) {
			break;
		}
	}

	LOCK_DEV_OPS(&dnp->dn_lock);
	e_ddi_exit_driver_list(dnp, circular);
	UNLOCK_DEV_OPS(&dnp->dn_lock);

	(void) strcpy(addr, wildcard_addr);
	kmem_free(wildcard_addr, len);
	if (child_dip == NULL) {
		ddi_rele_driver(major);
	}
	return (child_dip);
}
/*
 * attempt to convert a path to a major number
 */
static major_t
path_to_major(char *path, char *leaf_name)
{
	extern char *i_path_to_drv(char *pathname);
	char *binding;

	if ((binding = i_path_to_drv(path)) == NULL) {
		binding = leaf_name;
	}
	return (ddi_name_to_major(binding));
}
