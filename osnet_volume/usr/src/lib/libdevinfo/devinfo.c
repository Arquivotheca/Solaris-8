/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devinfo.c	1.12	99/09/10 SMI"

/*
 * Interfaces for getting device configuration data from kernel
 * through the devinfo driver.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stropts.h>
#include <fcntl.h>
#include <poll.h>
#include <synch.h>
#include <unistd.h>
#include <sys/mkdev.h>
#include <sys/obpdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/autoconf.h>

#include "libdevinfo.h"

#ifdef  DEBUG
int di_debug = 0;
#define	dprintf(args) if (di_debug) fprintf args
#else	/* DEBUG */
#define	dprintf(args) /* nothing */
#endif	/* DEBUG */

di_node_t
di_init(const char *phys_path, uint_t flag)
{
	return (di_init_impl(phys_path, flag, NULL));
}

/*
 * We use blocking_open() to guarantee access to the devinfo device, if open()
 * is failing with EAGAIN.
 */
static int
blocking_open(const char *path, int oflag)
{
	int fd;

	while ((fd = open(path, oflag)) == -1 && errno == EAGAIN)
		(void) poll(NULL, 0, 1 * MILLISEC);

	return (fd);
}

/* private interface */
di_node_t
di_init_driver(const char *drv_name, uint_t flag)
{
	int fd;
	char driver[MAXPATHLEN];

	/*
	 * Don't allow drv_name to exceed MAXPATHLEN - 1, or 1023,
	 * which should be sufficient for any sensible programmer.
	 */
	if ((drv_name == NULL) || (strlen(drv_name) >= MAXPATHLEN)) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}
	(void) strcpy(driver, drv_name);

	/*
	 * open the devinfo driver
	 */
	if ((fd = blocking_open("/devices/pseudo/devinfo@0:devinfo",
	    O_RDONLY)) == -1) {
		dprintf((stderr, "devinfo open failed with error: %d", errno));
		return (DI_NODE_NIL);
	}

	if (ioctl(fd, DINFOLODRV, driver) != 0) {
		dprintf((stderr, "fail to load driver %s\n", driver));
		(void) close(fd);
		errno = ENXIO;
		return (DI_NODE_NIL);
	}
	(void) close(fd);

	/*
	 * Driver load succeeded, return a snapshot
	 */
	return (di_init("/", flag));
}


di_node_t
di_init_impl(const char *phys_path, uint_t flag,
	struct di_priv_data *priv)
{
	const char *tmp;
	caddr_t pa;
	int fd, map_size;
	struct di_all *dap;
	struct dinfo_io dinfo_io;

	uint_t pageoffset = sysconf(_SC_PAGESIZE) - 1;
	uint_t pagemask = ~pageoffset;

	dprintf((stderr, "di_init: taking a snapshot\n"));

	/*
	 * Make sure there is no minor name in the path
	 * and the path do not start with /devices....
	 */
	if (strchr(phys_path, ':') ||
	    (strncmp(phys_path, "/devices", 8) == 0) ||
	    (strlen(phys_path) > MAXPATHLEN)) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	tmp = phys_path;
	if (strlen(tmp) == 0)
		(void) sprintf(dinfo_io.root_path, "/");
	else if (*tmp != '/')
		(void) sprintf(dinfo_io.root_path, "/%s", tmp);
	else
		(void) sprintf(dinfo_io.root_path, "%s", tmp);

	/*
	 * If private data is requested, copy the format specification
	 */
	if (flag & DINFOPRIVDATA & 0xff) {
		if (priv)
			bcopy(priv, &dinfo_io.priv,
			    sizeof (struct di_priv_data));
		else {
			errno = EINVAL;
			return (DI_NODE_NIL);
		}
	}

	/*
	 * Attempt to open the devinfo driver.  Make a second attempt at the
	 * read-only minor node if we don't have privileges to open the full
	 * version _and_ if we're not requesting operations that the read-only
	 * node can't perform.  (Setgid processes would fail an access() test,
	 * of course.)
	 */
	if ((fd = blocking_open("/devices/pseudo/devinfo@0:devinfo",
	    O_RDONLY)) == -1) {
		if ((flag & DINFOFORCE) == DINFOFORCE ||
		    (flag & DINFOPRIVDATA) == DINFOPRIVDATA) {
			/*
			 * We wanted to perform a privileged operation, but the
			 * privileged node isn't available.  Don't modify errno
			 * on our way out (but display it if we're running with
			 * di_debug set).
			 */
			dprintf((stderr, "devinfo open failed with error: %d",
			    errno));
			return (DI_NODE_NIL);
		}

		if ((fd = blocking_open("/devices/pseudo/devinfo@0:devinfo,ro",
		    O_RDONLY)) == -1) {
			dprintf((stderr, "devinfo open failed with error: %d",
			    errno));
			return (DI_NODE_NIL);
		}
	}

	/*
	 * Verify that there is no major conflict, i.e., we are indeed opening
	 * the devinfo driver.
	 */
	if (ioctl(fd, DINFOIDENT, NULL) != DI_MAGIC) {
		dprintf((stderr,
		    "driver identification failed; check for major conflict"));
		(void) close(fd);
		return (DI_NODE_NIL);
	}

	/*
	 * create snapshot
	 */
	if ((map_size = ioctl(fd, flag, &dinfo_io)) < 0) {
		dprintf((stderr, "devinfo ioctl failed with error: %d",
		    map_size));
		(void) close(fd);
		return (DI_NODE_NIL);
	} else if (map_size == 0) {
		dprintf((stderr, "%s not found", phys_path));
		errno = ENXIO;
		(void) close(fd);
		return (DI_NODE_NIL);
	}

	/*
	 * copy snapshot to userland
	 */
	map_size = (map_size + pageoffset) & pagemask;
	if ((pa = valloc(map_size)) == NULL) {
		dprintf((stderr, "valloc failed for snapshot\n"));
		(void) close(fd);
		return (DI_NODE_NIL);
	}

	if (ioctl(fd, DINFOUSRLD, pa) != map_size) {
		dprintf((stderr, "fail to copy snapshot to usrld\n"));
		(void) close(fd);
		return (DI_NODE_NIL);
	}

	(void) close(fd);

	dap = (struct di_all *)pa;
	if (dap->top_devinfo == 0) {	/* phys_path not found */
		dprintf((stderr, "%s not found\n", phys_path));
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	return ((di_node_t)(pa + dap->top_devinfo));
}

void
di_fini(di_node_t root)
{
	caddr_t pa;		/* starting address of map */

	dprintf((stderr, "di_fini: freeing a snapshot\n"));

	/*
	 * paranoid checking
	 */
	if (root == DI_NODE_NIL) {
		dprintf((stderr, "di_fini called with invalid arg\n"));
		return;
	}

	/*
	 * The root contains its own offset--self.
	 * Subtracting it from root address, we get the starting addr.
	 * The map_size is stored at the beginning of snapshot.
	 * Once we have starting address and size, we can free().
	 */
	pa = (caddr_t)root - DINO(root)->self;

	free(pa);
}

di_node_t
di_parent_node(di_node_t node)
{
	caddr_t pa;		/* starting address of map */

	if (node == DI_NODE_NIL) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	dprintf((stderr, "Get parent of node %s\n", di_node_name(node)));

	pa = (caddr_t)node - DINO(node)->self;

	if (DINO(node)->parent)
		return ((di_node_t)(pa + DINO(node)->parent));

	/*
	 * Deal with error condition:
	 *   If parent doesn't exist and node is not the root,
	 *   set errno to ENOTSUP. Otherwise, set errno to ENXIO.
	 */
	if (strcmp(((struct di_all *)pa)->root_path, "/") != 0)
		errno = ENOTSUP;
	else
		errno = ENXIO;

	return (DI_NODE_NIL);
}

di_node_t
di_sibling_node(di_node_t node)
{
	caddr_t pa;		/* starting address of map */

	if (node == DI_NODE_NIL) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	dprintf((stderr, "Get sibling of node %s\n", di_node_name(node)));

	pa = (caddr_t)node - DINO(node)->self;

	if (DINO(node)->sibling)
		return ((di_node_t)(pa + DINO(node)->sibling));

	/*
	 * Deal with error condition:
	 *   Sibling doesn't exist, figure out if ioctl command
	 *   has DINFOSUBTREE set. If it doesn't, set errno to
	 *   ENOTSUP.
	 */
	if (!(((struct di_all *)pa)->command & DINFOSUBTREE))
		errno = ENOTSUP;
	else
		errno = ENXIO;

	return (DI_NODE_NIL);
}

di_node_t
di_child_node(di_node_t node)
{
	caddr_t pa;		/* starting address of map */

	dprintf((stderr, "Get child of node %s\n", di_node_name(node)));

	if (node == DI_NODE_NIL) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	pa = (caddr_t)node - DINO(node)->self;

	if (DINO(node)->child)
		return ((di_node_t)(pa + DINO(node)->child));

	/*
	 * Deal with error condition:
	 *   Child doesn't exist, figure out if DINFOSUBTREE is set.
	 *   If it isn't, set errno to ENOTSUP.
	 */
	if (!(((struct di_all *)pa)->command & DINFOSUBTREE))
		errno = ENOTSUP;
	else
		errno = ENXIO;

	return (DI_NODE_NIL);
}

di_node_t
di_drv_first_node(const char *drv_name, di_node_t root)
{
	caddr_t pa;		/* starting address of map */
	int major, devcnt;
	struct di_devnm *devnm;

	dprintf((stderr, "Get first node of driver %s\n", drv_name));

	if (root == DI_NODE_NIL) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	/*
	 * get major number of driver
	 */
	pa = (caddr_t)root - DINO(root)->self;
	devcnt = ((struct di_all *)pa)->devcnt;
	devnm = (struct di_devnm *)(pa + ((struct di_all *)pa)->devnames);

	for (major = 0; major < devcnt; major++)
		if (devnm[major].name && (strcmp(drv_name,
		    (char *)(pa + devnm[major].name)) == 0))
			break;

	if (major >= devcnt) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	if (!(devnm[major].head)) {
		errno = ENXIO;
		return (DI_NODE_NIL);
	}

	return ((di_node_t)(pa + devnm[major].head));
}

di_node_t
di_drv_next_node(di_node_t node)
{
	caddr_t pa;		/* starting address of map */

	dprintf((stderr, "Get next node of driver\n"));

	if (node == DI_NODE_NIL) {
		errno = EINVAL;
		return (DI_NODE_NIL);
	}

	if (DINO(node)->next == (di_off_t)-1) {
		errno = ENOTSUP;
		return (DI_NODE_NIL);
	}

	pa = (caddr_t)node - DINO(node)->self;

	if (!(DINO(node)->next)) {
		errno = ENXIO;
		return (DI_NODE_NIL);
	}

	return ((di_node_t)(pa + DINO(node)->next));
}

/*
 * Internal library interfaces:
 *   node_list etc. for node walking
 */
struct node_list {
	struct node_list *next;
	di_node_t node;
};

static void
free_node_list(struct node_list **headp)
{
	struct node_list *tmp;

	while (*headp) {
		tmp = *headp;
		*headp = (*headp)->next;
		free(tmp);
	}
}

static void
append_node_list(struct node_list **headp, struct node_list *list)
{
	struct node_list *tmp;

	if (*headp == NULL) {
		*headp = list;
		return;
	}

	if (list == NULL)	/* a minor optimization */
		return;

	tmp = *headp;
	while (tmp->next)
		tmp = tmp->next;

	tmp->next = list;
}

static void
prepend_node_list(struct node_list **headp, struct node_list *list)
{
	struct node_list *tmp;

	if (list == NULL)
		return;

	tmp = *headp;
	*headp = list;

	if (tmp == NULL)	/* a minor optimization */
		return;

	while (list->next)
		list = list->next;

	list->next = tmp;
}

/*
 * returns 1 if node is a descendant of parent, 0 otherwise
 */
static int
is_descendant(di_node_t node, di_node_t parent)
{
	/*
	 * DI_NODE_NIL is parent of root, so it is
	 * the parent of all nodes.
	 */
	if (parent == DI_NODE_NIL) {
		return (1);
	}

	do {
		node = di_parent_node(node);
	} while ((node != DI_NODE_NIL) && (node != parent));

	return (node != DI_NODE_NIL);
}

/*
 * Insert list before the first node which is NOT a descendent of parent.
 * This is needed to reproduce the exact walking order of link generators.
 */
static void
insert_node_list(struct node_list **headp, struct node_list *list,
    di_node_t parent)
{
	struct node_list *tmp, *tmp1;

	if (list == NULL)
		return;

	tmp = *headp;
	if (tmp == NULL) {	/* a minor optimization */
		*headp = list;
		return;
	}

	if (!is_descendant(tmp->node, parent)) {
		prepend_node_list(headp, list);
		return;
	}

	/*
	 * Find first node which is not a descendant
	 */
	while (tmp->next && is_descendant(tmp->next->node, parent)) {
		tmp = tmp->next;
	}

	tmp1 = tmp->next;
	tmp->next = list;
	append_node_list(headp, tmp1);
}

/*
 *   Get a linked list of handles of all children
 */
static struct node_list *
get_children(di_node_t node)
{
	di_node_t child;
	struct node_list *result, *tmp;

	dprintf((stderr, "Get children of node %s\n", di_node_name(node)));

	if ((child = di_child_node(node)) == DI_NODE_NIL) {
		return (NULL);
	}

	if ((result = malloc(sizeof (struct node_list))) == NULL) {
		dprintf((stderr, "malloc of node_list fails\n"));
		return (NULL);
	}

	result->node = child;
	tmp = result;

	while ((child = di_sibling_node(tmp->node)) != DI_NODE_NIL) {
		if ((tmp->next = malloc(sizeof (struct node_list))) == NULL) {
			dprintf((stderr, "malloc of node_list fails\n"));
			free_node_list(&result);
			return (NULL);
		}
		tmp = tmp->next;
		tmp->node = child;
	}

	tmp->next = NULL;

	return (result);
}

/*
 * Internal library interface:
 *   Delete all siblings of the first node from the node_list, along with
 *   the first node itself.
 */
static void
prune_sib(struct node_list **headp)
{
	di_node_t parent, curr_par, curr_gpar;
	struct node_list *curr, *prev;

	/*
	 * get handle to parent of first node
	 */
	if ((parent = di_parent_node((*headp)->node)) == DI_NODE_NIL) {
		/*
		 * This must be the root of the snapshot, so can't
		 * have any siblings.
		 *
		 * XXX Put a check here just in case.
		 */
#ifdef DEBUG
		if ((*headp)->next)
			dprintf((stderr, "Unexpected err in di_walk_node.\n"));
#endif /* DEBUG */

		free(*headp);
		*headp = NULL;
		return;
	}

	/*
	 * To be complete, we should also delete the children
	 * of siblings that have already been visited.
	 * This happens for DI_WALK_SIBFIRST when the first node
	 * is NOT the first in the linked list of siblings.
	 *
	 * Hence, we compare parent with BOTH the parent and grandparent
	 * of nodes, and delete node is a match is found.
	 */
	prev = *headp;
	curr = prev->next;
	while (curr) {
		if (((curr_par = di_parent_node(curr->node)) != DI_NODE_NIL) &&
		    ((curr_par == parent) || ((curr_gpar =
		    di_parent_node(curr_par)) != DI_NODE_NIL) &&
		    (curr_gpar == parent))) {
			/*
			 * match parent/grandparent: delete curr
			 */
			prev->next = curr->next;
			free(curr);
			curr = prev->next;
		} else
			curr = curr->next;
	}

	/*
	 * delete the first node
	 */
	curr = *headp;
	*headp = curr->next;
	free(curr);
}

/*
 * Internal library function:
 *	Update node list based on action (return code from callback)
 *	and flag specifying walking behavior.
 */
static void
update_node_list(int action, uint_t flag, struct node_list **headp)
{
	struct node_list *children, *tmp;
	di_node_t parent = di_parent_node((*headp)->node);

	switch (action) {
	case DI_WALK_TERMINATE:
		/*
		 * free the node list and be done
		 */
		children = NULL;
		free_node_list(headp);
		break;

	case DI_WALK_PRUNESIB:
		/*
		 * Get list of children and prune siblings
		 */
		children = get_children((*headp)->node);
		prune_sib(headp);
		break;

	case DI_WALK_PRUNECHILD:
		/*
		 * Set children to NULL and pop first node
		 */
		children = NULL;
		tmp = *headp;
		*headp = tmp->next;
		free(tmp);
		break;

	case DI_WALK_CONTINUE:
	default:
		/*
		 * Get list of children and pop first node
		 */
		children = get_children((*headp)->node);
		tmp = *headp;
		*headp = tmp->next;
		free(tmp);
		break;
	}

	/*
	 * insert the list of children
	 */
	switch (flag) {
	case DI_WALK_CLDFIRST:
		prepend_node_list(headp, children);
		break;

	case DI_WALK_SIBFIRST:
		append_node_list(headp, children);
		break;

	case DI_WALK_LINKGEN:
	default:
		insert_node_list(headp, children, parent);
		break;
	}
}

/*
 * Internal library function:
 *   Invoke callback on one node and update the list of nodes to be walked
 *   based on the flag and return code.
 */
static void
walk_one_node(struct node_list **headp, uint_t flag, void *arg,
	int (*callback)(di_node_t, void *))
{
	dprintf((stderr, "Walking node %s\n", di_node_name((*headp)->node)));

	update_node_list(callback((*headp)->node, arg),
	    flag & DI_WALK_MASK, headp);
}

int
di_walk_node(di_node_t root, uint_t flag, void *arg,
	int (*node_callback)(di_node_t, void *))
{
	struct node_list  *head;	/* node_list for tree walk */

	if (root == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if ((head = malloc(sizeof (struct node_list))) == NULL) {
		dprintf((stderr, "malloc of node_list fails\n"));
		return (-1);
	}

	head->next = NULL;
	head->node = root;

	dprintf((stderr, "Start node walking from node %s\n",
	    di_node_name(root)));

	while (head != NULL)
		walk_one_node(&head, flag, arg, node_callback);

	return (0);
}

/*
 * Internal library function:
 *   Invoke callback for each minor on the minor list of first node
 *   on node_list headp, and place childern of first node on the list.
 *
 *   This is similar to walk_one_node, except we only walk in child
 *   first mode.
 */
static void
walk_one_minor_list(struct node_list **headp, const char *desired_type,
	uint_t flag, void *arg, int (*callback)(di_node_t, di_minor_t, void *))
{
	int ddm_type;
	int action = DI_WALK_CONTINUE;
	char *node_type;
	di_minor_t minor = DI_MINOR_NIL;
	di_node_t node = (*headp)->node;

	while ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
		ddm_type = di_minor_type(minor);

		if ((ddm_type == DDM_ALIAS) && !(flag & DI_CHECK_ALIAS))
			continue;

		if ((ddm_type == DDM_INTERNAL_PATH) &&
		    !(flag & DI_CHECK_INTERNAL_PATH))
			continue;

		node_type = di_minor_nodetype(minor);
		if ((desired_type != NULL) && ((node_type == NULL) ||
		    strncmp(desired_type, node_type, strlen(desired_type))
		    != 0))
			continue;

		if ((action = callback(node, minor, arg)) ==
		    DI_WALK_TERMINATE) {
			break;
		}
	}

	update_node_list(action, DI_WALK_LINKGEN, headp);
}

int
di_walk_minor(di_node_t root, const char *minor_type, uint_t flag, void *arg,
	int (*minor_callback)(di_node_t, di_minor_t, void *))
{
	struct node_list  *head;	/* node_list for tree walk */

#ifdef DEBUG
	char *path = di_devfs_path(root);
	dprintf((stderr, "walking minor nodes under %s\n", path));
	di_devfs_path_free(path);
#endif

	if (root == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if ((head = malloc(sizeof (struct node_list))) == NULL) {
		dprintf((stderr, "malloc of node_list fails\n"));
		return (-1);
	}

	head->next = NULL;
	head->node = root;

	dprintf((stderr, "Start minor walking from node %s\n",
		di_node_name(root)));

	while (head != NULL)
		walk_one_minor_list(&head, minor_type, flag, arg,
		    minor_callback);

	return (0);
}

/*
 * generic node parameters
 *   Calling these routines always succeeds.
 */
char *
di_node_name(di_node_t node)
{
	return ((caddr_t)node + DINO(node)->node_name - DINO(node)->self);
}

/* returns NULL ptr or a valid ptr to non-NULL string */
char *
di_bus_addr(di_node_t node)
{
	caddr_t pa = (caddr_t)node - DINO(node)->self;

	if (DINO(node)->address == 0)
		return (NULL);

	return ((char *)(pa + DINO(node)->address));
}

char *
di_binding_name(di_node_t node)
{
	caddr_t pa = (caddr_t)node - DINO(node)->self;

	if (DINO(node)->bind_name == 0)
		return (NULL);

	return ((char *)(pa + DINO(node)->bind_name));
}

int
di_compatible_names(di_node_t node, char **names)
{
	char *c;
	int len, size, entries = 0;

	if (DINO(node)->compat_names == 0) {
		*names = NULL;
		return (0);
	}

	*names = (caddr_t)node + DINO(node)->compat_names - DINO(node)->self;

	c = *names;
	len = DINO(node)->compat_length;
	while (len > 0) {
		entries++;
		size = strlen(c) + 1;
		len -= size;
		c += size;
	}

	return (entries);
}

int
di_instance(di_node_t node)
{
	return (DINO(node)->instance);
}

/*
 * XXX: emulate the return value of the old implementation
 * using info from devi_node_class and devi_node_attributes.
 */
int
di_nodeid(di_node_t node)
{
	if (DINO(node)->node_class == DDI_NC_PROM)
		return (DI_PROM_NODEID);

	if (DINO(node)->attributes & DDI_PERSISTENT)
		return (DI_SID_NODEID);

	return (DI_PSEUDO_NODEID);
}

uint_t
di_state(di_node_t node)
{
	uint_t result = 0;

	if (DINO(node)->node_state == 0)
		result |= DI_DRIVER_DETACHED;
	if (DINO(node)->state & DEVI_DEVICE_OFFLINE)
		result |= DI_DEVICE_OFFLINE;
	if (DINO(node)->state & DEVI_DEVICE_DOWN)
		result |= DI_DEVICE_OFFLINE;
	if (DINO(node)->state & DEVI_BUS_QUIESCED)
		result |= DI_BUS_QUIESCED;
	if (DINO(node)->state & DEVI_BUS_DOWN)
		result |= DI_BUS_DOWN;

	return (result);
}

ddi_devid_t
di_devid(di_node_t node)
{
	if (DINO(node)->devid == 0)
		return (NULL);

	return ((ddi_devid_t)((caddr_t)node +
	    DINO(node)->devid - DINO(node)->self));
}

char *
di_driver_name(di_node_t node)
{
	int major;
	caddr_t pa;
	struct di_devnm *devnm;

	major = DINO(node)->drv_major;
	if (major < 0)
		return (NULL);

	pa = (caddr_t)node - DINO(node)->self;
	devnm = (struct di_devnm *)(pa + ((struct di_all *)pa)->devnames);

	if (devnm[major].name)
		return (pa + devnm[major].name);
	else
		return (NULL);
}

uint_t
di_driver_ops(di_node_t node)
{
	int major;
	caddr_t pa;
	struct di_devnm *devnm;

	major = DINO(node)->drv_major;
	if (major < 0)
		return (0);

	pa = (caddr_t)node - DINO(node)->self;
	devnm = (struct di_devnm *)(pa + ((struct di_all *)pa)->devnames);

	return (devnm[major].ops);
}

/*
 * returns the length of the path, caller must free memory
 */
char *
di_devfs_path(di_node_t node)
{
	caddr_t pa;
	di_node_t parent;
	int depth = 0, len = 0;
	char *buf, *name[MAX_TREE_DEPTH], *addr[MAX_TREE_DEPTH];

	/*
	 * trace back to root, note the node_name & address
	 */
	while ((parent = di_parent_node(node)) != DI_NODE_NIL) {
		name[depth] = di_node_name(node);
		len += strlen(name[depth]) + 1;		/* 1 for '/' */

		if ((addr[depth] = di_bus_addr(node)) != NULL)
			len += strlen(addr[depth]) + 1;	/* 1 for '@' */

		node = parent;
		depth++;
	}

	/*
	 * get the path to the root of snapshot
	 */
	pa = (caddr_t)node - DINO(node)->self;
	name[depth] = ((struct di_all *)pa)->root_path;
	len += strlen(name[depth]) + 1;

	/*
	 * allocate buffer and assemble path
	 */
	if ((buf = malloc(len)) == NULL) {
		return (NULL);
	}

	(void) strcpy(buf, name[depth]);
	len = strlen(buf);
	if (buf[len - 1] == '/')
		len--;	/* delete trailing '/' */

	while (depth) {
		depth--;
		buf[len] = '/';
		(void) strcpy(buf + len + 1, name[depth]);
		len += strlen(name[depth]) + 1;
		if (addr[depth] && addr[depth][0] != '\0') {
			buf[len] = '@';
			(void) strcpy(buf + len + 1, addr[depth]);
			len += strlen(addr[depth]) + 1;
		}
	}

	return (buf);
}

void
di_devfs_path_free(char *buf)
{
#ifdef DEBUG
	if (buf == NULL) {
		dprintf((stderr, "Error: di_devfs_path_free NULL location!\n"));
		return;
	}
#endif

	free(buf);
}

/* minor data access */
di_minor_t
di_minor_next(di_node_t node, di_minor_t minor)
{
	caddr_t pa;

	/*
	 * paranoid error checking
	 */
	if (node == DI_NODE_NIL) {
		errno = EINVAL;
		return (DI_MINOR_NIL);
	}

	/*
	 * minor is not NIL
	 */
	if (minor != DI_MINOR_NIL) {
		if (DIMI(minor)->next != 0)
			return ((di_minor_t)((caddr_t)minor - DIMI(minor)->self
			    + DIMI(minor)->next));
		else {
			errno = ENXIO;
			return (DI_MINOR_NIL);
		}
	}

	/*
	 * minor is NIL-->caller asks for first minor node
	 */
	if (DINO(node)->minor_data != 0)
		return ((di_minor_t)((caddr_t)node - DINO(node)->self +
		    DINO(node)->minor_data));

	/*
	 * no minor data-->check if snapshot includes minor data
	 *	in order to set the correct errno
	 */
	pa = (caddr_t)node - DINO(node)->self;
	if (DINFOMINOR & ((struct di_all *)pa)->command)
		errno = ENXIO;
	else
		errno = ENOTSUP;

	return (DI_MINOR_NIL);
}

ddi_minor_type
di_minor_type(di_minor_t minor)
{
	return (DIMI(minor)->type);
}

char *
di_minor_name(di_minor_t minor)
{
	if (DIMI(minor)->name == 0)
		return (NULL);

	return ((caddr_t)minor - DIMI(minor)->self + DIMI(minor)->name);
}

dev_t
di_minor_devt(di_minor_t minor)
{
	return (makedev(DIMI(minor)->dev_major, DIMI(minor)->dev_minor));
}

int
di_minor_spectype(di_minor_t minor)
{
	return (DIMI(minor)->spec_type);
}

char *
di_minor_nodetype(di_minor_t minor)
{
	if (DIMI(minor)->node_type == 0)
		return (NULL);

	return ((caddr_t)minor - DIMI(minor)->self + DIMI(minor)->node_type);
}

unsigned int
di_minor_class(di_minor_t minor)
{
	return (DIMI(minor)->mdclass);
}

/*
 * Single public interface for accessing software properties
 */
di_prop_t
di_prop_next(di_node_t node, di_prop_t prop)
{
	int list = DI_PROP_DRV_LIST;

	/*
	 * paranoid check
	 */
	if (node == DI_NODE_NIL) {
		errno = EINVAL;
		return (DI_PROP_NIL);
	}

	/*
	 * Find which prop list we are at
	 */
	if (prop != DI_PROP_NIL)
		list = DIPROP(prop)->prop_list;

	do {
		switch (list++) {
		case DI_PROP_DRV_LIST:
			prop = di_prop_drv_next(node, prop);
			break;
		case DI_PROP_SYS_LIST:
			prop = di_prop_sys_next(node, prop);
			break;
		case DI_PROP_GLB_LIST:
			prop = di_prop_global_next(node, prop);
			break;
		case DI_PROP_HW_LIST:
			prop = di_prop_hw_next(node, prop);
			break;
		default:	/* shouldn't happen */
			errno = EFAULT;
			return (DI_PROP_NIL);
		}
	} while ((prop == DI_PROP_NIL) && (list <= DI_PROP_HW_LIST));

	return (prop);
}

dev_t
di_prop_devt(di_prop_t prop)
{
	return (makedev(DIPROP(prop)->dev_major, DIPROP(prop)->dev_minor));
}

char *
di_prop_name(di_prop_t prop)
{
	if (DIPROP(prop)->prop_name == 0)
		return (NULL);

	return ((caddr_t)prop - DIPROP(prop)->self + DIPROP(prop)->prop_name);
}

int
di_prop_type(di_prop_t prop)
{
	uint_t flags = DIPROP(prop)->prop_flags;

	if (flags & DDI_PROP_UNDEF_IT)
		return (DI_PROP_TYPE_UNDEF_IT);

	if (DIPROP(prop)->prop_len == 0)
		return (DI_PROP_TYPE_BOOLEAN);

	if ((flags & DDI_PROP_TYPE_MASK) == DDI_PROP_TYPE_ANY)
		return (DI_PROP_TYPE_UNKNOWN);

	if (flags & DDI_PROP_TYPE_INT)
		return (DI_PROP_TYPE_INT);

	if (flags & DDI_PROP_TYPE_STRING)
		return (DI_PROP_TYPE_STRING);

	if (flags & DDI_PROP_TYPE_BYTE)
		return (DI_PROP_TYPE_BYTE);

	/*
	 * Shouldn't get here. In case we do, return unknown type.
	 *
	 * XXX--When DDI_PROP_TYPE_COMPOSITE is implemented, we need
	 *	to add DI_PROP_TYPE_COMPOSITE.
	 */
	dprintf((stderr, "Unimplemented property type: 0x%x\n", flags));

	return (DI_PROP_TYPE_UNKNOWN);
}

/*
 * Extract type-specific values of an property
 */
extern int di_prop_decode_common(void *prop_data, int len,
	int ddi_type, int prom);

int
di_prop_ints(di_prop_t prop, int **prop_data)
{
	if (DIPROP(prop)->prop_len == 0)
		return (0);	/* boolean property */

	if ((DIPROP(prop)->prop_data == 0) ||
	    (DIPROP(prop)->prop_data == (di_off_t)-1)) {
		errno = EFAULT;
		*prop_data = NULL;
		return (-1);
	}

	*prop_data = (int *)((caddr_t)prop - DIPROP(prop)->self
	    + DIPROP(prop)->prop_data);

	return (di_prop_decode_common((void *)prop_data,
	    DIPROP(prop)->prop_len, DI_PROP_TYPE_INT, 0));
}

int
di_prop_strings(di_prop_t prop, char **prop_data)
{
	if (DIPROP(prop)->prop_len == 0)
		return (0);	/* boolean property */

	if ((DIPROP(prop)->prop_data == 0) ||
	    (DIPROP(prop)->prop_data == (di_off_t)-1)) {
		errno = EFAULT;
		*prop_data = NULL;
		return (-1);
	}

	*prop_data = (char *)((caddr_t)prop - DIPROP(prop)->self
	    + DIPROP(prop)->prop_data);

	return (di_prop_decode_common((void *)prop_data,
	    DIPROP(prop)->prop_len, DI_PROP_TYPE_STRING, 0));
}

int
di_prop_bytes(di_prop_t prop, uchar_t **prop_data)
{
	if (DIPROP(prop)->prop_len == 0)
		return (0);	/* boolean property */

	if ((DIPROP(prop)->prop_data == 0) ||
	    (DIPROP(prop)->prop_data == (di_off_t)-1)) {
		errno = EFAULT;
		*prop_data = NULL;
		return (-1);
	}

	*prop_data = (uchar_t *)((caddr_t)prop - DIPROP(prop)->self
	    + DIPROP(prop)->prop_data);

	return (di_prop_decode_common((void *)prop_data,
	    DIPROP(prop)->prop_len, DI_PROP_TYPE_BYTE, 0));
}

/*
 * returns 1 for match, 0 for no match
 */
static int
match_prop(di_prop_t prop, dev_t match_dev, const char *name, int type)
{
	int prop_type;

#ifdef DEBUG
	if (di_prop_name(prop) == NULL) {
		dprintf((stderr, "libdevinfo: property with has no name!\n"));
		return (0);
	}
#endif /* DEBUG */

	if (strcmp(name, di_prop_name(prop)) != 0)
		return (0);

	if ((match_dev != DDI_DEV_T_ANY) && (di_prop_devt(prop) != match_dev))
		return (0);

	/*
	 * XXX prop_type is different from DDI_*. See PSARC 1997/127.
	 */
	prop_type = di_prop_type(prop);
	if ((prop_type != DI_PROP_TYPE_UNKNOWN) && (prop_type != type) &&
	    (prop_type != DI_PROP_TYPE_BOOLEAN))
		return (0);

	return (1);
}

static di_prop_t
di_prop_search(dev_t match_dev, di_node_t node, const char *name,
    int type)
{
	di_prop_t prop = DI_PROP_NIL;

	/*
	 * The check on match_dev follows ddi_prop_lookup_common().
	 * Other checks are libdevinfo specific implementation.
	 */
	if ((node == DI_NODE_NIL) || (name == NULL) || (strlen(name) == 0) ||
	    (match_dev == DDI_DEV_T_NONE) || (type < DI_PROP_TYPE_INT) ||
	    (type > DI_PROP_TYPE_BYTE)) {
		errno = EINVAL;
		return (DI_PROP_NIL);
	}

	while ((prop = di_prop_next(node, prop)) != DI_PROP_NIL) {
		dprintf((stderr, "match prop name %s, devt 0x%lx, type %d\n",
		    di_prop_name(prop), di_prop_devt(prop),
		    di_prop_type(prop)));
		if (match_prop(prop, match_dev, name, type))
			return (prop);
	}

	return (DI_PROP_NIL);
}

int
di_prop_lookup_ints(dev_t dev, di_node_t node, const char *prop_name,
	int **prop_data)
{
	di_prop_t prop;

	if ((prop = di_prop_search(dev, node, prop_name,
	    DI_PROP_TYPE_INT)) == DI_PROP_NIL)
		return (-1);

	return (di_prop_ints(prop, (void *)prop_data));
}

int
di_prop_lookup_strings(dev_t dev, di_node_t node, const char *prop_name,
    char **prop_data)
{
	di_prop_t prop;

	if ((prop = di_prop_search(dev, node, prop_name,
	    DI_PROP_TYPE_STRING)) == DI_PROP_NIL)
		return (-1);

	return (di_prop_strings(prop, (void *)prop_data));
}

int
di_prop_lookup_bytes(dev_t dev, di_node_t node, const char *prop_name,
	uchar_t **prop_data)
{
	di_prop_t prop;

	if ((prop = di_prop_search(dev, node, prop_name,
	    DI_PROP_TYPE_BYTE)) == DI_PROP_NIL)
		return (-1);

	return (di_prop_bytes(prop, (void *)prop_data));
}

/*
 * Consolidation private property access functions
 */
di_prop_t
di_prop_drv_next(di_node_t node, di_prop_t prop)
{
	caddr_t pa;

	if (prop != DI_PROP_NIL) {
		if (DIPROP(prop)->next)
			return ((di_prop_t)((caddr_t)prop - DIPROP(prop)->self
			    + DIPROP(prop)->next));
		else
			return (DI_PROP_NIL);
	}

	/*
	 * prop is NIL, caller asks for first property
	 */
	pa = (caddr_t)node - DINO(node)->self;
	if (DINO(node)->drv_prop)
		return ((di_prop_t)(pa + DINO(node)->drv_prop));

	/*
	 * no prop found. Check the reason for not found
	 */
	if (DINFOPROP & ((struct di_all *)pa)->command)
		errno = ENXIO;
	else
		errno = ENOTSUP;

	return (DI_PROP_NIL);
}

di_prop_t
di_prop_sys_next(di_node_t node, di_prop_t prop)
{
	caddr_t pa;

	if (prop != DI_PROP_NIL) {
		if (DIPROP(prop)->next)
			return ((di_prop_t)((caddr_t)prop - DIPROP(prop)->self
			    + DIPROP(prop)->next));
		else
			return (DI_PROP_NIL);
	}

	/*
	 * prop is NIL, caller asks for first property in list
	 */
	pa = (caddr_t)node - DINO(node)->self;
	if (DINO(node)->sys_prop)
		return ((di_prop_t)(pa + DINO(node)->sys_prop));

	/*
	 * prop not found. Check the reason for not found
	 */
	if (DINFOPROP & ((struct di_all *)pa)->command)
		errno = ENXIO;
	else
		errno = ENOTSUP;

	return (DI_PROP_NIL);
}

di_prop_t
di_prop_global_next(di_node_t node, di_prop_t prop)
{
	caddr_t pa;
	struct di_devnm *devnm;

	pa = (caddr_t)node - DINO(node)->self;

	if (prop != DI_PROP_NIL) {
		if (DIPROP(prop)->next)
			return ((di_prop_t)(pa + DIPROP(prop)->next));
		else
			return (DI_PROP_NIL);
	}

	/*
	 * prop is NIL, caller asks for first property in list
	 */
	if (DINO(node)->drv_major < 0) {
		errno = ENXIO;
		return (DI_PROP_NIL);
	}

	devnm = (struct di_devnm *)(pa + ((struct di_all *)pa)->devnames +
	    DINO(node)->drv_major * sizeof (struct di_devnm));

	dprintf((stderr, "pa = 0x%lx, end = 0x%lx, devnm = 0x%x\n",
	    pa, pa + ((struct di_all *)pa)->map_size, devnm));

	if (devnm->global_prop) {
		dprintf((stderr, "found glob prop at 0x%lx\n",
		    pa + devnm->global_prop));
		return ((di_prop_t)(pa + devnm->global_prop));
	}

	dprintf((stderr, "devnm = 0x%x\n", devnm));
	/*
	 * prop not found. Check the reason for not found
	 */
	if (DINFOPROP & ((struct di_all *)pa)->command)
		errno = ENXIO;
	else
		errno = ENOTSUP;

	return (DI_PROP_NIL);
}

di_prop_t
di_prop_hw_next(di_node_t node, di_prop_t prop)
{
	caddr_t pa = (caddr_t)node - DINO(node)->self;

	if (prop != DI_PROP_NIL) {
		if (DIPROP(prop)->next)
			return ((di_prop_t)(pa + DIPROP(prop)->next));
		else
			return (DI_PROP_NIL);
	}

	/*
	 * prop is NIL, caller asks for first property in list
	 */
	if (DINO(node)->hw_prop)
		return ((di_prop_t)(pa + DINO(node)->hw_prop));

	/*
	 * prop not found. Check the reason for not found
	 */
	if (DINFOPROP & ((struct di_all *)pa)->command)
		errno = ENXIO;
	else
		errno = ENOTSUP;

	return (DI_PROP_NIL);
}

int
di_prop_rawdata(di_prop_t prop, uchar_t **prop_data)
{
#ifdef DEBUG
	if (prop == DI_PROP_NIL) {
		errno = EINVAL;
		return (-1);
	}
#endif /* DEBUG */

	if (DIPROP(prop)->prop_len == 0) {
		*prop_data = NULL;
		return (0);
	}

	if ((DIPROP(prop)->prop_data == 0) ||
	    (DIPROP(prop)->prop_data == (di_off_t)-1)) {
		errno = EFAULT;
		*prop_data = NULL;
		return (-1);
	}

	/*
	 * No memory allocation.
	 */
	*prop_data = (uchar_t *)((caddr_t)prop - DIPROP(prop)->self +
	    DIPROP(prop)->prop_data);

	return (DIPROP(prop)->prop_len);
}

/*
 * Consolidation private interfaces for private data
 */
void *
di_parent_private_data(di_node_t node)
{
	caddr_t pa;

	if (DINO(node)->parent_data == 0) {
		errno = ENXIO;
		return (NULL);
	}

	if (DINO(node)->parent_data == (di_off_t)-1) {
		/*
		 * Private data requested, but not obtained due to a memory
		 * error (e.g. wrong format specified)
		 */
		errno = EFAULT;
		return (NULL);
	}

	pa = (caddr_t)node - DINO(node)->self;
	if (DINO(node)->parent_data)
		return (pa + DINO(node)->parent_data);

	if ((((struct di_all *)pa)->command & DINFOPRIVDATA))
		errno = ENXIO;
	else
		errno = ENOTSUP;

	return (NULL);
}

void *
di_driver_private_data(di_node_t node)
{
	caddr_t pa;

	if (DINO(node)->driver_data == 0) {
		errno = ENXIO;
		return (NULL);
	}

	if (DINO(node)->driver_data == (di_off_t)-1) {
		/*
		 * Private data requested, but not obtained due to a memory
		 * error (e.g. wrong format specified)
		 */
		errno = EFAULT;
		return (NULL);
	}

	pa = (caddr_t)node - DINO(node)->self;
	if (DINO(node)->parent_data)
		return (pa + DINO(node)->parent_data);

	if ((((struct di_all *)pa)->command & DINFOPRIVDATA))
		errno = ENXIO;
	else
		errno = ENOTSUP;

	return (NULL);
}

/*
 * PROM property access
 */

/*
 * openprom driver stuff:
 *	The maximum property length depends on the buffer size. We use
 *	OPROMMAXPARAM defined in <sys/openpromio.h>
 *
 *	MAXNAMESZ is max property name. obpdefs.h defines it as 32 based on 1275
 *	MAXVALSZ is maximum value size, which is whatever space left in buf
 */

#define	OBP_MAXBUF	OPROMMAXPARAM - sizeof (int)
#define	OBP_MAXPROPLEN	OBP_MAXBUF - OBP_MAXPROPNAME;

struct di_prom_prop {
	char *name;
	int len;
	uchar_t *data;
	struct di_prom_prop *next;	/* form a linked list */
};

struct di_prom_handle { /* handle to prom */
	mutex_t lock;	/* synchrize access to openprom fd */
	int	fd;	/* /dev/openprom file descriptor */
	struct di_prom_prop *list;	/* linked list of prop */
	union {
		char buf[OPROMMAXPARAM];
		struct openpromio opp;
	} oppbuf;
};

di_prom_handle_t
di_prom_init()
{
	struct di_prom_handle *p;

	if ((p = malloc(sizeof (struct di_prom_handle))) == NULL)
		return (DI_PROM_HANDLE_NIL);

	dprintf((stderr, "di_prom_init: get prom handle 0x%p\n", p));

	(void) mutex_init(&p->lock, USYNC_THREAD, NULL);
	if ((p->fd = open("/dev/openprom", O_RDONLY)) < 0) {
		free(p);
		return (DI_PROM_HANDLE_NIL);
	}
	p->list = NULL;

	return ((di_prom_handle_t)p);
}

static void
di_prom_prop_free(struct di_prom_prop *list)
{
	struct di_prom_prop *tmp = list;

	while (tmp != NULL) {
		list = tmp->next;
		if (tmp->name != NULL) {
			free(tmp->name);
		}
		if (tmp->data != NULL) {
			free(tmp->data);
		}
		free(tmp);
		tmp = list;
	}
}

void
di_prom_fini(di_prom_handle_t ph)
{
	struct di_prom_handle *p = (struct di_prom_handle *)ph;

	dprintf((stderr, "di_prom_fini: free prom handle 0x%p\n", p));

	(void) close(p->fd);
	(void) mutex_destroy(&p->lock);
	di_prom_prop_free(p->list);

	free(p);
}

/*
 * Internal library interface for locating the property
 * XXX: ph->lock must be held for the duration of call.
 */
static di_prom_prop_t
di_prom_prop_found(di_prom_handle_t ph, int nodeid,
	di_prom_prop_t prom_prop)
{
	struct di_prom_handle *p = (struct di_prom_handle *)ph;
	struct openpromio *opp = &p->oppbuf.opp;
	int *ip = (int *)(opp->oprom_array);
	struct di_prom_prop *prop = (struct di_prom_prop *)prom_prop;

	dprintf((stderr, "Looking for nodeid 0x%x\n", nodeid));

	/*
	 * Set "current" nodeid in the openprom driver
	 */
	opp->oprom_size = sizeof (int);
	*ip = nodeid;
	if (ioctl(p->fd, OPROMSETNODEID, opp) < 0) {
		dprintf((stderr, "*** Nodeid not found 0x%x\n", nodeid));
		return (DI_PROM_PROP_NIL);
	}

	dprintf((stderr, "Found nodeid 0x%x\n", nodeid));

	bzero(opp, OBP_MAXBUF);
	opp->oprom_size = OBP_MAXPROPNAME;
	if (prom_prop != DI_PROM_PROP_NIL)
		(void) strcpy(opp->oprom_array, prop->name);

	if ((ioctl(p->fd, OPROMNXTPROP, opp) < 0) || (opp->oprom_size == 0))
		return (DI_PROM_PROP_NIL);

	/*
	 * Prom property found. Allocate struct for storing prop
	 *   (reuse variable prop)
	 */
	if ((prop = malloc(sizeof (struct di_prom_prop))) == NULL)
		return (DI_PROM_PROP_NIL);

	/*
	 * Get a copy of property name
	 */
	if ((prop->name = strdup(opp->oprom_array)) == NULL) {
		free(prop);
		return (DI_PROM_PROP_NIL);
	}

	/*
	 * get property value and length
	 */
	opp->oprom_size = OBP_MAXPROPLEN;

	if ((ioctl(p->fd, OPROMGETPROP, opp) < 0) ||
	    (opp->oprom_size == (uint_t)-1)) {
		free(prop->name);
		free(prop);
		return (DI_PROM_PROP_NIL);
	}

	/*
	 * make a copy of the property value
	 */
	prop->len = opp->oprom_size;

	if (prop->len == 0)
		prop->data = NULL;
	else if ((prop->data = malloc(prop->len)) == NULL) {
		free(prop->name);
		free(prop);
		return (DI_PROM_PROP_NIL);
	}

	bcopy(opp->oprom_array, prop->data, prop->len);

	/*
	 * Prepend prop to list in prom handle
	 */
	prop->next = p->list;
	p->list = prop;

	return ((di_prom_prop_t)prop);
}

di_prom_prop_t
di_prom_prop_next(di_prom_handle_t ph, di_node_t node, di_prom_prop_t prom_prop)
{
	struct di_prom_handle *p = (struct di_prom_handle *)ph;

	dprintf((stderr, "Search next prop for node 0x%p with ph 0x%p\n",
		node, p));

	/*
	 * paranoid check
	 */
	if ((ph == DI_PROM_HANDLE_NIL) || (node == DI_NODE_NIL)) {
		errno = EINVAL;
		return (DI_PROM_PROP_NIL);
	}

	if (di_nodeid(node) != DI_PROM_NODEID) {
		errno = ENXIO;
		return (DI_PROM_PROP_NIL);
	}

	/*
	 * synchronize access to prom file descriptor
	 */
	(void) mutex_lock(&p->lock);

	/*
	 * look for next property
	 */
	prom_prop = di_prom_prop_found(ph, DINO(node)->nodeid, prom_prop);

	(void) mutex_unlock(&p->lock);

	return (prom_prop);
}

char *
di_prom_prop_name(di_prom_prop_t prom_prop)
{
	/*
	 * paranoid check
	 */
	if (prom_prop == DI_PROM_PROP_NIL) {
		errno = EINVAL;
		return (NULL);
	}

	return (((struct di_prom_prop *)prom_prop)->name);
}

int
di_prom_prop_data(di_prom_prop_t prom_prop, uchar_t **prom_prop_data)
{
	/*
	 * paranoid check
	 */
	if (prom_prop == DI_PROM_PROP_NIL) {
		errno = EINVAL;
		return (NULL);
	}

	*prom_prop_data = ((struct di_prom_prop *)prom_prop)->data;

	return (((struct di_prom_prop *)prom_prop)->len);
}

/*
 * Internal library interface for locating the property
 *    Returns length if found, -1 if prop doesn't exist.
 */
static struct di_prom_prop *
di_prom_prop_lookup_common(di_prom_handle_t ph, di_node_t node,
	const char *prom_prop_name)
{
	struct openpromio *opp;
	struct di_prom_prop *prop;
	struct di_prom_handle *p = (struct di_prom_handle *)ph;

	/*
	 * paranoid check
	 */
	if ((ph == DI_PROM_HANDLE_NIL) || (node == DI_NODE_NIL)) {
		errno = EINVAL;
		return (NULL);
	}

	if (di_nodeid(node) != DI_PROM_NODEID) {
		errno = ENXIO;
		return (NULL);
	}

	opp = &p->oppbuf.opp;

	(void) mutex_lock(&p->lock);

	opp->oprom_size = sizeof (int);
	opp->oprom_node = DINO(node)->nodeid;
	if (ioctl(p->fd, OPROMSETNODEID, opp) < 0) {
		errno = ENXIO;
		dprintf((stderr, "*** Nodeid not found 0x%x\n",
		    DINO(node)->nodeid));
		(void) mutex_unlock(&p->lock);
		return (NULL);
	}

	/*
	 * get property length
	 */
	bzero(opp, OBP_MAXBUF);
	opp->oprom_size = OBP_MAXPROPLEN;
	(void) strcpy(opp->oprom_array, prom_prop_name);

	if ((ioctl(p->fd, OPROMGETPROPLEN, opp) < 0) ||
	    (opp->oprom_len == -1)) {
		/* no such property */
		(void) mutex_unlock(&p->lock);
		return (NULL);
	}

	/*
	 * Prom property found. Allocate struct for storing prop
	 */
	if ((prop = malloc(sizeof (struct di_prom_prop))) == NULL) {
		(void) mutex_unlock(&p->lock);
		return (NULL);
	}
	prop->name = NULL;	/* we don't need the name */
	prop->len = opp->oprom_len;

	if (prop->len == 0) {	/* boolean property */
		prop->data = NULL;
		prop->next = p->list;
		p->list = prop;
		(void) mutex_unlock(&p->lock);
		return (prop);
	}

	/*
	 * retrieve the property value
	 */
	bzero(opp, OBP_MAXBUF);
	opp->oprom_size = OBP_MAXPROPLEN;
	(void) strcpy(opp->oprom_array, prom_prop_name);

	if ((ioctl(p->fd, OPROMGETPROP, opp) < 0) ||
	    (opp->oprom_size == (uint_t)-1)) {
		/* error retrieving property value */
		(void) mutex_unlock(&p->lock);
		free(prop);
		return (NULL);
	}

	/*
	 * make a copy of the property value, stick in ph->list
	 */
	if ((prop->data = malloc(prop->len)) == NULL) {
		(void) mutex_unlock(&p->lock);
		free(prop);
		return (NULL);
	}

	bcopy(opp->oprom_array, prop->data, prop->len);

	prop->next = p->list;
	p->list = prop;
	(void) mutex_unlock(&p->lock);

	return (prop);
}

int
di_prom_prop_lookup_ints(di_prom_handle_t ph, di_node_t node,
	const char *prom_prop_name, int **prom_prop_data)
{
	int len;
	struct di_prom_prop *prop;

	prop = di_prom_prop_lookup_common(ph, node, prom_prop_name);

	if (prop == NULL) {
		*prom_prop_data = NULL;
		return (-1);
	}

	if (prop->len == 0) {	/* boolean property */
		*prom_prop_data = NULL;
		return (0);
	}

	len = di_prop_decode_common((void *)&prop->data, prop->len,
		DI_PROP_TYPE_INT, 1);
	*prom_prop_data = (int *)prop->data;

	return (len);
}

int
di_prom_prop_lookup_strings(di_prom_handle_t ph, di_node_t node,
	const char *prom_prop_name, char **prom_prop_data)
{
	int len;
	struct di_prom_prop *prop;

	prop = di_prom_prop_lookup_common(ph, node, prom_prop_name);

	if (prop == NULL) {
		*prom_prop_data = NULL;
		return (-1);
	}

	if (prop->len == 0) {	/* boolean property */
		*prom_prop_data = NULL;
		return (0);
	}

	/*
	 * Fix an openprom bug (OBP string not NULL terminated).
	 * XXX This should really be fixed in promif.
	 */
	if ((prop->data)[prop->len - 1] != '\0') {
		uchar_t *tmp;
		prop->len++;
		if ((tmp = realloc(prop->data, prop->len)) == NULL)
			return (-1);

		prop->data = tmp;
		(prop->data)[prop->len - 1] = '\0';
		dprintf((stderr, "OBP string not NULL terminated: "
		    "node=%s, prop=%s, val=%s\n",
		    di_node_name(node), prom_prop_name, prop->data));
	}

	len = di_prop_decode_common((void *)&prop->data, prop->len,
	    DI_PROP_TYPE_STRING, 1);
	*prom_prop_data = (char *)prop->data;

	return (len);
}

int
di_prom_prop_lookup_bytes(di_prom_handle_t ph, di_node_t node,
	const char *prom_prop_name, uchar_t **prom_prop_data)
{
	int len;
	struct di_prom_prop *prop;

	prop = di_prom_prop_lookup_common(ph, node, prom_prop_name);

	if (prop == NULL) {
		*prom_prop_data = NULL;
		return (-1);
	}

	if (prop->len == 0) {	/* boolean property */
		*prom_prop_data = NULL;
		return (0);
	}

	len = di_prop_decode_common((void *)&prop->data, prop->len,
	    DI_PROP_TYPE_BYTE, 1);
	*prom_prop_data = prop->data;

	return (len);
}

/* end of devinfo.c */
