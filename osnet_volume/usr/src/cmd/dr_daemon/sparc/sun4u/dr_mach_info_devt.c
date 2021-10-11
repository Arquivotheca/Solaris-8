/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dr_mach_info_devt.c	1.17	98/11/19 SMI"

/*
 * The dr_mach_info_*.c files determine devices on a specified board and
 * their current usage by the system.
 *
 * This file deals with reading the dev-info tree for a specified board
 * and creating/freeing a dr_io_t tree which represents it.
 */

#include <string.h>
#include <libdevinfo.h>

#include "dr_info.h"

#define	SBUSDRIVER	"sbus"
#define	SBUSMEMDRIVER	"sbusmem"
#define	PCIDRIVER	"pci"		/* XXX - SMC: Is this right? */

/*
 * The following define structures used to mark progress during libdevinfo
 * device tree walks.
 */
typedef struct walk_progress {
	dr_iop_t	root;
	dr_iop_t	parent;
	dr_iop_t	sibling;
	di_node_t	root_node;
	int		bus1;
	int		bus2;
	int		error_flag;
} walk_progress_t;

typedef struct minor_progress_struct {
	dr_iop_t	major;
	dr_leafp_t	current;
	int		error_flag;
} minor_progress_t;

/* Local Routines */
static int toplevel_devtree_walk(di_node_t node, void *argp);
static int inner_devtree_walk(di_node_t node, void *argp);
static int minor_walk(di_node_t node, di_minor_t minor, void *argp);
static dr_iop_t build_new_node(di_node_t node);
static int add_new_bus_node(di_node_t node, walk_progress_t *progress);
int build_notnet_leaf(dr_leafp_t dmpt, char *minor_name_addr);

/*
 * ------------------------------------------------------------------------
 * build_device_tree
 * ------------------------------------------------------------------------
 *
 * Create a tree of dr_io_t nodes represening the I/O devices for a board.
 *
 * Input:	board	(system board whose devices we want)
 *
 * Description:	The libdevinfo API is used to examine the system's device
 *		tree and construct an accompanying tree of dr_io_t nodes.
 *
 *		This routine initializes a libdevinfo device tree and a
 *		special walk_progress_t structure, and then uses a walk
 *		through the libdevinfo tree with that special progress
 *		structure to create the tree of dr_io_t nodes.
 *
 *		Most of the work this routine accomplishes is actually done
 *		by the di_walk_node() function (part of the libdevinfo API)
 *		and the accompanying toplevel_devtree_walk() callback routine.
 *
 *		Buses are identified by their UPA addresses, which have the
 *		board number embedded within it.
 *
 * Result:	If failure of any sort, NULL is returned and no memory is
 *		left in use.
 *
 *		If success, the root of the constructed tree is returned and
 *		the memory in which the tree resides is allocated.  The caller
 *		is responsible for freeing the tree's memory.
 *
 *		The resulting tree is of the form:
 *
 *                      ____                               ____
 *                     (sbus)---------------------------->(sbus)
 *                      ~~~~                               ~~~~
 *                      /                                   /
 *                     |                                   |
 *		      \|/                                 \|/
 *		   __________     __________
 *		  (controller)-->(controller)-->...       ...
 *                 ~~~~~~~~~~     ~~~~~~~~~~
 *                                  /|\ /|\
 *                                   |   |
 *                                  /     \
 *                                 /       \
 *                                |         |
 *                               \|/        |
 *                             ______     ______
 *		              (device)-->(device)-->...
 *			       ~~~~~~     ~~~~~~
 *                              /           /
 *                             /           /
 *                            |           |
 *                           \|/         \|/
 *                       __________   _________
 *                      (minor list) (minor list)
 *                       ~~~~~~~~~~   ~~~~~~~~~~
 *
 *		The tree has every child pointing to its parent node (except
 *		for controllers directly below the sbus, in order to make use
 *		of the old superdragon code).  And, every node points to its
 *		left-most child which in turn points to the parent node's other
 *		children as siblings.
 */
dr_iop_t
build_device_tree(int board)
{
	di_node_t		root_node;
	walk_progress_t		progress;

	/* Construct what the UPA addresses would be for this board's buses */
	progress.bus1 = ((board << 2) | 0x40);
	progress.bus2 = ((board << 2) | 0x41);

	/* Initialize the links in the progress struct */
	progress.root = (dr_iop_t)NULL;
	progress.parent = (dr_iop_t)NULL;
	progress.sibling = (dr_iop_t)NULL;

	/* Get the libdevinfo tree, and include the minor information */
	root_node = di_init("/", (DINFOSUBTREE | DINFOMINOR));
	if (root_node == DI_NODE_NIL) {
		dr_logerr(DRV_FAIL, errno, "libdevinfo failed.");
		return ((dr_iop_t)NULL);
	}

	/* Walk the libdevinfo tree to build a DR-ized device tree */
	progress.root_node = root_node;
	progress.error_flag = FALSE;
	di_walk_node(root_node, DI_WALK_SIBFIRST, (void *)&progress, \
		toplevel_devtree_walk);

	/* Release the libdevinfo tree */
	di_fini(root_node);

	/* Check for errors */
	if (progress.error_flag == TRUE) {
		dr_logerr(DRV_FAIL, 0, "device tree not built.");
		free_dr_device_tree(progress.root);
		return ((dr_iop_t)NULL);
	}

	/* Return the DR-ized device tree (if no errors occurred, of course) */
	return (progress.root);
}

/*
 * ------------------------------------------------------------------------
 * toplevel_devtree_walk
 * ------------------------------------------------------------------------
 *
 * This is the callback for the top level walk through the device tree --
 * the one that's called by build_device_tree().  It finds buses, walks their
 * subtrees, and merges all the buses' subtrees into one tree which it passes
 * back.
 *
 * Input:	node	(pointer to current devinfo node; should be top-level)
 *		argp	(pointer to our walk_progress_t struct)
 *
 * Description:	This routine is called during a sibling-first walk through the
 *		tree.  It cuts out the children of every node it sees, so that
 *		it only looks one level below the top_devinfo node.
 *
 *		When it finds an sbus or pci bus, it checks the UPA address of
 *		the bus to see if it's on the current board.  If so, it spins
 *		off a secondary walk of the bus's devices to create a subtree
 *		of dr_io_t nodes.  The resulting subtree is then merged in with
 *		the tree stored in the walk_progress_t struct given to this
 *		routine.
 *
 *		The parent pointers of all children directly below the bus nodes
 *		are set to null, in order to make use of old superdragon code.
 *		Then the bus nodes are merged as siblings -- each on the highest
 *		level of the finalized tree of dr_io_t nodes.
 *
 * Results:	If the current node for this walk represents a bus for the
 *		current board, then a subtree is created for it and added on.
 *		(This means memory is allocated for it).
 *
 *		Any errors result in the progress struct's error_flag being
 *		set and the DI_WALK_TERMINATE value being returned to end the
 *		top-level device tree walk.
 *
 *		In the absence of error, DI_WALK_PRUNECHILD is returned.
 */
static int
toplevel_devtree_walk(di_node_t node, void *argp)
{
	walk_progress_t 	*progress = (walk_progress_t *)argp;
	walk_progress_t 	bus_progress;
	char			*drvname;
	char			*busaddr;
	int			upa_addr;
	dr_iop_t		child_node;

	/* Sanity checks */
	if (progress == NULL || node == DI_NODE_NIL)
		return (DI_WALK_TERMINATE);

	/* Skip the root node and any uninteresting nodes */
	if (node == progress->root_node || DINO(node)->drv_major == -1)
		return (DI_WALK_CONTINUE);

	/* Is it an SBUS or PCI bus? */
	drvname = di_driver_name(node);
	if (drvname != NULL &&
		(strncmp(drvname, SBUSDRIVER, strlen(SBUSDRIVER)) == 0 ||
		strncmp(drvname, PCIDRIVER, strlen(PCIDRIVER)) == 0)) {

		/*
		 * So it's an SBUS or PCI.  Check for the following:
		 *
		 *	1) it has a valid bus address
		 * 	2) we can extract the UPA address
		 *	3) its UPA address corresponds to our board
		 */
		busaddr = di_bus_addr(node);
		if (busaddr != NULL &&
			(sscanf(busaddr, "%02x", &upa_addr) == 1) &&
				((upa_addr == progress->bus1) ||
				(upa_addr == progress->bus2))) {

			/*
			 * All three criteria are met.  So we must:
			 *
			 *	1) Build a DR-ized device subtree for the bus
			 *	2) Check for errors while walking subtree
			 *	3) Merge it into the main DR tree
			 *	4) Set first level nodes' parents to NULL
			 */

			/* Step 1 */
			bus_progress.root = (dr_iop_t)NULL;
			bus_progress.parent = (dr_iop_t)NULL;
			bus_progress.sibling = (dr_iop_t)NULL;
			bus_progress.error_flag = FALSE;
			di_walk_node(node, DI_WALK_CLDFIRST, \
				(void *)&bus_progress, inner_devtree_walk);

			/* Step 2 */
			if (bus_progress.error_flag == TRUE) {
				dr_logerr(DRV_FAIL, 0, \
					"I/O bus device tree not built.");
				progress->error_flag = TRUE;
				return (DI_WALK_TERMINATE);
			}

			/* Step 3 */
			if (progress->root == (dr_iop_t)NULL)
				progress->root = bus_progress.root;
			else
				progress->sibling->dv_sibling = \
					bus_progress.root;
			progress->sibling = bus_progress.root;

			/* Step 4 */
			child_node = bus_progress.root->dv_child;
			while (child_node != (dr_iop_t)NULL) {
				child_node->dv_parent = (dr_iop_t)NULL;
				child_node = child_node->dv_sibling;
			}
		}
	}

	/* Always prune it's children from our hi-level device tree walk */
	return (DI_WALK_PRUNECHILD);
}

/*
 * ------------------------------------------------------------------------
 * inner_devtree_walk
 * ------------------------------------------------------------------------
 *
 * Callback routine for walking through a bus's devices and creating a
 * subtree of dr_io_t nodes for them.
 *
 * Input:	node	(pointer to current devinfo node; should be inner-level)
 *		argp	(pointer to our walk_progress_t struct)
 *
 * Description:	The walk_progress_t struct holds parameters describing the
 *		current state of the walk through this bus's subtree.
 *
 *		This routine is called during a child-first walk through a bus's
 *		subtree.  Each node is examined to see if it's "interesting."
 *		That is, if it's attached.
 *
 *		New nodes are malloc'ed and their details are filled in.  Any
 *		leaf nodes have their list of minor devices walked and added
 *		to the node.
 *
 *		Note: the node representing the I/O bus for this subtree gets
 *		into this routine at the very beginning.  We catch that case
 *		and call add_new_bus_node() to process it specially.
 *
 * Results:	On each call of this routine, a single controller node or a
 *		device node is created (if it's interesting).  Memory is
 *		allocated for the dr_io_t structure and for any minor devices
 *		(dr_leaf_t) nodes associated with this routine (if it's a
 *		device).
 *
 *		If an error occurs, the progress.error_flag field is set to
 *		TRUE and the walk is terminated.
 */
static int
inner_devtree_walk(di_node_t node, void *argp)
{
	walk_progress_t		*progress = (walk_progress_t *)argp;
	minor_progress_t	minor_progress;
	dr_io_t			*new_dr_node;
	dr_iop_t		search_node;
	int			retval = DI_WALK_CONTINUE;

	/* Sanity checks.  A failure here terminates our walk. */
	if (progress == NULL || node == DI_NODE_NIL)
		return (DI_WALK_TERMINATE);

	/* We have special processing for the subtree root nodes */
	if (progress->root == (dr_iop_t)NULL)
		return (add_new_bus_node(node, progress));

	/* If this driver isn't attached, ... */
	if (di_state(node) & DI_DRIVER_DETACHED)
		retval = DI_WALK_PRUNECHILD;

	/* ... or if it's for sbus memory, its branch will be pruned out */
	else if (di_driver_name(node) != NULL && \
		strcmp(di_driver_name(node), SBUSMEMDRIVER) == 0)
		retval  = DI_WALK_PRUNECHILD;

	/* Otherwise, build a node for it */
	else {

		/* Allocate a node.  A failure here terminates our walk. */
		if ((new_dr_node = build_new_node(node)) == (dr_iop_t)NULL) {
			dr_loginfo("inner_devtree_walk: malloc failed " \
					"(new_dr_node).");
			progress->error_flag = TRUE;
			return (DI_WALK_TERMINATE);
		}

		/* Append this node's name to the "devices_path" variable */
		if (new_dr_node->dv_name == NULL || \
				new_dr_node->dv_addr == NULL)
			sprintf(devices_path, "%s/(Unknown)", devices_path);
		else
			sprintf(devices_path, "%s/%s@%s", devices_path, \
				new_dr_node->dv_name, new_dr_node->dv_addr);

		/*
		 *  Place the new DR-ized node into the DR-ized tree
		 *
		 *	1) Make this node point to its parent node
		 *	2) Is there a sibling before this node?
		 *		a) No.  Make this the parent node's first child.
		 *		b) Yes.  Link this node with its sibling.
		 */

		/* Step 1 */
		new_dr_node->dv_parent = progress->parent;

		/* Step 2 */
		if (progress->sibling == (dr_iop_t)NULL)
			/* Step 2a */
			progress->parent->dv_child = new_dr_node;
		else
			/* Step 2b */
			progress->sibling->dv_sibling = new_dr_node;

		/*
		 * If it's a leaf node, gather its leaf_node data
		 */
		if (di_child_node(node) == DI_NODE_NIL) {

			/* Mark it as a leaf node */
			new_dr_node->dv_node_type = DEVICE_DEVI_LEAF;

			/* Initialize the minor_progress structure */
			minor_progress.major = new_dr_node;
			minor_progress.current = (dr_leafp_t)NULL;
			minor_progress.error_flag = FALSE;

			/* Walk through the minors */
			di_walk_minor(node, (const char *)NULL, DI_CHECK_ALIAS,
				(void *)&minor_progress, minor_walk);

			/* Check for errors walking the minor nodes */
			if (minor_progress.error_flag == TRUE) {
				progress->error_flag = TRUE;
				return (DI_WALK_TERMINATE);
			}
		}
	}

	/*
	 * Discern where we'll go next during the tree walk.  There really
	 * are only 4 things that could happen here.
	 *
	 *	1) We go downwards in the tree (ie, this node has children)
	 *	2) We go laterally in the tree (ie, this node has no children
	 *	   but it has siblings)
	 *	3) We go upwards in the tree (ie, this node has no children
	 *	   or siblings but one of its ancestors has yet-unseen siblings)
	 *	4) We're done.  (ie, this node has no children or siblings,
	 *	   and none of its ancestors have any yet-unseen siblings)
	 */

	/* 1) We go downwards */
	if (retval != DI_WALK_PRUNECHILD &&
		di_child_node(node) != DI_NODE_NIL) {

		/*
		 * Represent in the progress structure that the next node is
		 * parented by the current node, and that the next node in our
		 * walk has no siblings before it.
		 */
		progress->parent = new_dr_node;
		progress->sibling = (dr_iop_t)NULL;
	}

	/* 2) We go lateral */
	else if (di_sibling_node(node) != DI_NODE_NIL) {


		/*
		 * Check if we've already decided to prune out this node.
		 * If not, then we need to represent in the progress structure
		 * that the next node has this node as its previous sibling.
		 * Also, we'll be moving up a level in the devices path.
		 */
		if (retval != DI_WALK_PRUNECHILD) {
			if (strrchr(devices_path, '/') != NULL)
				*strrchr(devices_path, '/') = '\0';
			progress->sibling = new_dr_node;
		}
	}

	/*
	 * 3 & 4) This code handles both situations.  We search for a node to
	 * go upwards through.  If the search succeeds, we set things up for
	 * the next node.  If our search fails, than the 4th situation seems
	 * to be occurring.
	 */
	else {

		/*
		 * Start looking at this node's parent, and be pessimistic.
		 * Also chop out this node's name from "devices_path."
		 */
		search_node = progress->parent;
		if (strrchr(devices_path, '/') != NULL)
			*strrchr(devices_path, '/') = '\0';
		retval = DI_WALK_TERMINATE;

		/* Stop looking when we break out or hit the tree's ceiling */
		while (search_node != progress->root) {

			/* If this node has a sibling... */
			if (di_sibling_node(search_node->di_node) \
				!= DI_NODE_NIL) {

				/* Transfer the walk to the sibling */
				progress->sibling = search_node;
				progress->parent = search_node->dv_parent;
				retval = DI_WALK_CONTINUE;
				break;
			}

			/* If not, continue searching */
			search_node = search_node->dv_parent;
			if (strrchr(devices_path, '/') != NULL)
				*strrchr(devices_path, '/') = '\0';
		}
	}

	/* Done processing this node, continue our walk */
	return (retval);
}

/*
 * ------------------------------------------------------------------------
 * minor_walk
 * ------------------------------------------------------------------------
 *
 * Callback routine for walking through the minor devices of a device node
 *
 * Input:	node	(device node that owns this minor)
 *		minor	(the current minor seen during the walk)
 *		argp	(pointer to a minor_progress_t struct)
 *
 * Description:	This routine is called for each minor node associated with
 *		a device.  This routine will determine what kind of a minor
 *		it is, and build the appropriate sort of dr_leaf_t node.  It
 *		then links that dr_leaf_t node with the major device node.
 *
 * Results:	Memory is allocated for a new dr_leaf_t node and it is linked
 *		with the major device node.
 *
 *		If any errors occur, progress->error_flag is set to true and
 *		the walk is terminated.  Otherwise, the walk continues.
 */
/*ARGSUSED*/
static int
minor_walk(di_node_t node, di_minor_t minor, void *argp)
{
	minor_progress_t	*progress = (minor_progress_t *)argp;
	dr_leafp_t		leafp = (dr_leafp_t)NULL;
	ddi_minor_type		type;
	char			*nodetype;

	/* Sanity checks */
	if (minor == DI_MINOR_NIL || progress == (minor_progress_t *)NULL)
		return (DI_WALK_TERMINATE);

	/* Skip this node if it's invalid */
	type = DIMI(minor)->type;
	if (type != DDM_MINOR && type != DDM_DEFAULT && type != DDM_ALIAS)
		return (DI_WALK_CONTINUE);
	nodetype = di_minor_nodetype(minor);
	if (nodetype == NULL) {
		return (DI_WALK_CONTINUE);
	}

	/* Get a leaf entry */
	leafp = dr_leaf_malloc();
	if (leafp == (dr_leafp_t)NULL) {
		dr_loginfo("minor_walk: malloc failed (leafp).");
		progress->error_flag = TRUE;
		return (DI_WALK_TERMINATE);
	}

	/* Link it in */
	leafp->major_dev = progress->major;
	leafp->next = progress->major->dv_leaf;
	progress->major->dv_leaf = leafp;

	/*
	 * Build the leaf node.  How it's built depends on what type of node
	 * it is.
	 */
	if (strcmp(nodetype, DDI_NT_NET) == 0) {

		/* Network nodes are built specially */
		progress->major->dv_node_type = DEVICE_NET;
		if (build_net_leaf(leafp, di_minor_name(minor))) {
			dr_loginfo("minor_walk: failed to build net leaf.");
			progress->error_flag = TRUE;
			return (DI_WALK_TERMINATE);
		}

	} else {

		/*
		 * Non-network devices we pass on their name so that the next
		 * function down can figure out the proper fields and how to
		 * fill them in.
		 */
		progress->major->dv_node_type = DEVICE_NOTNET;
		if (build_notnet_leaf(leafp, di_minor_name(minor))) {
			dr_loginfo("minor_walk: failed to build non-net leaf.");
			progress->error_flag = TRUE;
			return (DI_WALK_TERMINATE);
		}
	}

	return (DI_WALK_CONTINUE);
}

/*
 * ---------------------------------------------------------------------------
 * build_new_node
 * ---------------------------------------------------------------------------
 *
 * Build (allocate and fill in basic details for) a single dr_io_t node.
 *
 * Input:	node	(the corresponding libdevinfo node)
 *
 * Description:	Memory for the node is allocated and then the "node" argument
 *		is used to fill in some basic details about the node.  The
 *		points for the node are then all set to NULL.
 *
 * Results:	NULL is returned in case of error, otherwise the address of
 *		the newly-constructed node is returned.
 */
static dr_iop_t
build_new_node(di_node_t node)
{
	dr_iop_t	dr_node = (dr_iop_t)NULL;

	if (node != DI_NODE_NIL) {

		dr_node = dr_dev_malloc();

		if (dr_node != (dr_iop_t)NULL) {
			dr_node->dv_name = (char *)strdup(di_node_name(node));
			dr_node->dv_addr = (char *)strdup(di_bus_addr(node));
			dr_node->dv_instance = DINO(node)->instance;
			dr_node->dv_node_type = DEVICE_NOT_DEVI_LEAF;
			dr_node->dv_parent = (dr_iop_t)NULL;
			dr_node->dv_child = (dr_iop_t)NULL;
			dr_node->dv_sibling = (dr_iop_t)NULL;
			dr_node->di_node = node;
		}
	}

	return (dr_node);
}

/*
 * ---------------------------------------------------------------------------
 * add_new_bus_node
 * ---------------------------------------------------------------------------
 *
 * When "node" points to an I/O bus node, add it into the subtree associated
 * with "progress."
 *
 * Input:	node		(the corresponding libdevinfo node)
 *		progress	(walk_progress_t pointing for bus's subtree)
 *
 * Description:	This allocates memory for the new node, fills in its details,
 *		and links it in as the root of the subtree described by
 *		"progress."
 *
 * Results:	In case of success, the new node is added as the root of the
 *		current subtree and DI_WALK_CONTINUE is returned.
 *
 *		In case of failure, the error_flag is set for the progress
 *		structure and DI_WALK_TERMINATE is returned.
 */
static int
add_new_bus_node(di_node_t node, walk_progress_t *progress)
{
	dr_iop_t	new_dr_node;

	/* Sanity check */
	if (node == DI_NODE_NIL || progress == (walk_progress_t *)NULL)
		return (DI_WALK_TERMINATE);

	/* Create the new node */
	new_dr_node = build_new_node(node);
	if (new_dr_node == (dr_iop_t)NULL) {
		dr_loginfo("add_new_bus_node: malloc failed (new_dr_node).");
		progress->error_flag = TRUE;
		return (DI_WALK_TERMINATE);
	}

	/* Start off the new tree with this sbus node */
	progress->root = new_dr_node;
	progress->parent = new_dr_node;
	progress->sibling = (dr_iop_t)NULL;

	/* Initialize the devices_path variable */
	if (strncmp(new_dr_node->dv_name, "pci", 3) == 0)
		sprintf(devices_path, "/devices/%s@%s", "pci", \
			new_dr_node->dv_addr);
	else
		sprintf(devices_path, "/devices/%s@%s", new_dr_node->dv_name, \
			new_dr_node->dv_addr);

	return (DI_WALK_CONTINUE);
}
