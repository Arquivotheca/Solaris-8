/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_info_ap.c	1.18	99/11/01 SMI"

/*
 * The dr_info_*.c files determine devices on a specified board and
 * their current usage by the system.
 *
 * This file deals with adding Alternate Pathing (AP) devices to
 * the board device information.
 *
 * Note that if compilation of Alternate pathing is disabled, this
 * is a NULL module.
 */

#ifdef AP

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mkdev.h>

#include "dr_info.h"
#include "dr_ap.h"

/*
 * To enable debug code local to this module, uncomment the
 * following define
 *	#define	INFO_AP_DEBUG	1
 */

/*
 * All AP disks have partition entries in these two directories.  These
 * defines were stolen from apd_subr.h in the ap daemon directory,
 */
#define	AP_DEV_DSK_DIR		"/dev/ap/dsk/"
#define	AP_DEV_RDSK_DIR		"/dev/ap/rdsk/"

/*
 * the ap_controller structure is used to gather potential AP
 * controller names.
 */
typedef struct ap_controller_t *ap_controllerp_t;

struct ap_controller_t {
	device_type_t		type;
	ap_controllerp_t	next;
	union {
		dr_iop_t		_notnet_node;
		dr_leafp_t		_net_leaf;
	} nodeval;
};
typedef struct ap_controller_t ap_controller_t;

#define	notnet_node	nodeval._notnet_node
#define	net_leaf	nodeval._net_leaf

/* Local Data */
static ap_controllerp_t	ap_list;	/* head of ap list */
static int 		ap_count;	/* number of entries in ap list */

/* Local Routines */
static void find_ap_entries(dr_iop_t dp, int netonly);
static void link_ap_entries(dr_iop_t dp);
static ctlr_t *create_ctlr_array(void);
static void free_ap_list(void);
static void add_net_ap_info(dr_leafp_t dp, struct dr_info *infop);
static void add_notnet_ap_info(dr_iop_t ctrlp, struct dr_info *infop);
static int compare_devid(const void *a, const void *b);
static void create_ap_partitions(dr_leaf_t *dip, char *apname);
static void add_ap_db_locs(void);
static void apdb_devt_match(dr_leafp_t dmp, all_db_info *info);


/*
 * dr_controller_names
 *
 * This routine is called from dr_ap.c to determine the names
 * of the controllers on the specified board.  Dr uses these names
 * to notify AP when controllers are attached/detached from the system.
 *
 * Input: board
 *	  cpp   addr of pointer for returning controller list.
 *
 * Output: *cpp  - user responsible for freeing space.
 *
 * Function return: number of potential AP devices found.
 */
int
dr_controller_names(int board, ctlr_t **cpp)
{
	dr_iop_t	rootp;

	/* read the dev-info tree */
	if ((rootp = build_device_tree(board)) == NULL) {
		*cpp = NULL;
		return (0);
	}

	/*
	 * Now find all the potential AP devices. The linked list
	 * and number of names is saved in ap_list and ap_count.
	 */
	ap_list = NULL;
	ap_count = 0;

	/* 0 flag indicates we want all devices, not just networks */
	find_ap_entries(rootp, 0);

	/* transfer linked list into ctlr structure */
	if (ap_count > 0)
		*cpp = create_ctlr_array();
	else
		*cpp = NULL;

	/* Free up the temporary structures we've created */
	free_dr_device_tree(rootp);
	free_ap_list();

	return (ap_count);
}

/*
 * add_ap_meta_devices
 *
 * This routine is called from get_dev_info() and dr_unplumb_network()
 * to determine if the board devices participate in AP, and if so,
 * save their /dev or network names.
 *
 * Input: rootp - root node of the device tree.
 *	  netonly - true if we only add the network metadevices
 *
 * Output: entries are added to the device tree if necessary.
 */
void
add_ap_meta_devices(dr_iop_t rootp, int netonly)
{
	int			indx;
	struct dr_info 		*infop;
	ctlr_t			*cp;
	ap_controllerp_t 	ap;

	/*
	 * If communication with the AP daemon is disabled. There's
	 * nothing left to do.
	 */
	if (noapdaemon)
		return;

	/*
	 * Find all the potential AP devices. The linked list
	 * and number of names is saved in ap_list and ap_count.
	 */
	ap_list = NULL;
	ap_count = 0;
	find_ap_entries(rootp, netonly);

	/* transfer linked list into ctlr structure */
	if (ap_count == 0 || (cp = create_ctlr_array()) == NULL) {
		free_ap_list();
		return;
	}

	/*
	 * Get info about each potential AP controller from AP.  This
	 * is returned as an array of ap_count infop structures.
	 */
	infop = do_ap_query(ap_count, cp);
	if (infop == NULL) {
		/* some sort of comm error - nothing more to do */
		free_ap_list();
		return;
	}

	/*
	 * Now go through the AP list and match up dr_info structures
	 * with dr_io_t structures.
	 */
	ap = ap_list;
	indx = 0;
	while (ap) {
		if (ap->type == DEVICE_NET) {
			add_net_ap_info(ap->net_leaf, &infop[indx]);
		} else {
			add_notnet_ap_info(ap->notnet_node, &infop[indx]);
		}
		ap = ap->next;
		indx++;
	}

	/* Check for usage due to AP database partitions */
	if (!netonly)
		add_ap_db_locs();

	/* Free up the temporary structures we've created */
	free_ap_list();
}

/*
 * find_ap_entries
 *
 * Search the dr_io_t tree looking for device entries which may be
 * part of the Alternate Pathing database.  These entries are
 *	- network interfaces
 *	- other controllers of leaf devices
 *
 * Input: dp - root of subtree to look for ap devices in
 *	  netonly - only look for network devices.
 */
static void
find_ap_entries(dr_iop_t dp, int netonly)
{
	if (dp->dv_child) {
		find_ap_entries(dp->dv_child, netonly);
		if (dp->dv_sibling)
			find_ap_entries(dp->dv_sibling, netonly);
		return;
	}

	/*
	 * Leaf node which is a network device.  Report this
	 * controller to AP.
	 */
	if (dp->dv_node_type == DEVICE_NET) {

		link_ap_entries(dp);

	} else if (!netonly) {

		/*
		 * We're collecting info on all node types, not just
		 * net.  For non-net entries, the potential AP device
		 * is the parent of this node.  For example, this leaf
		 * node could be a disk device (sd0) with a parent
		 * controller such as isp0.
		 */
		if (dp->dv_node_type == DEVICE_NOT_DEVI_LEAF) {
			/*
			 * This is _probably_ a controller node
			 * whose children are not attached.  Tell
			 * AP about this guy just in case AP finds
			 * it interesting.
			 */
			link_ap_entries(dp);

		} else if (dp->dv_parent != NULL)  {

			link_ap_entries(dp->dv_parent);
		}
	}

	/* Siblings may be of interest to AP also */
	if (dp->dv_sibling)
		find_ap_entries(dp->dv_sibling, netonly);
}

/*
 * link_ap_entries
 *
 * Create an ap_controller entry for the given device and link it
 * into our list.
 *
 * Globals modified: ap_list and ap_count.
 */
static void
link_ap_entries(dr_iop_t dp)
{
	ap_controllerp_t 	ap_ptr;
	ap_controllerp_t 	ap;
	dr_leafp_t		dlp;

	if (dp->dv_node_type == DEVICE_NET) {

		/*
		 * For net entries, save the leaf nodes.  This
		 * consists of the different interface names the net
		 * device may be known by.
		 */
		dlp = dp->dv_leaf;
		while (dlp) {
			if ((ap = calloc(1, sizeof (ap_controller_t))) ==
			    NULL) {
				dr_logerr(DRV_FAIL, errno,
					    "malloc failed (ap_controller)");
				return;
			}
			ap->next = ap_list;
			ap_list = ap;
			ap_count ++;

			ap->type = DEVICE_NET;
			ap->net_leaf = dlp;

			dlp = dlp->next;
		}
	} else {
		/*
		 * For notnet entries, save the controller node.  Check to see
		 * if the controller is already in the list, and skip this node
		 * if it is.
		 */
		for (ap_ptr = ap_list; ap_ptr != NULL; ap_ptr = ap_ptr->next)
			if (ap_ptr->notnet_node == dp)
				return;

		if ((ap = calloc(1, sizeof (ap_controller_t))) == NULL) {
			dr_logerr(DRV_FAIL, errno,
				    "malloc failed (ap_controller)");
			return;
		}
		ap->next = ap_list;
		ap_list = ap;
		ap_count++;

		ap->type = DEVICE_NOTNET;
		ap->notnet_node = dp;
	}
}

/*
 * create_ctlr_array
 *
 * translate the global linked list ap_list into an array of
 * AP ctlr_t structures.  When creating the ap_listlist, we determined
 * how many total controller entries there are.
 */
static ctlr_t *
create_ctlr_array(void)
{
	int			indx;
	ctlr_t			*cp;
	ap_controllerp_t 	ap;
	char			*name;

	cp = calloc(ap_count, sizeof (ctlr_t));
	if (cp == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (AP ctlr_t array)");
		ap_count = 0;
		return (NULL);
	}

	ap = ap_list;
	indx = 0;
	while (ap) {

		if (ap->type == DEVICE_NET) {
			name = drv_alias2nm(ap->net_leaf->ifname);
			cp[indx].name = strdup(name);
			cp[indx].instance = ap->net_leaf->ifinstance;
		} else {
			/*
			 * I've found on Solaris 2.6, AP 2.1 that non-network
			 * controller names can be passed as aliases, and
			 * in fact, in some cases (like pln), must be.  So,
			 * we will skip the alias->name conversion.
			 */
			name = ap->notnet_node->dv_name;
			cp[indx].name = strdup(name);
			cp[indx].instance = ap->notnet_node->dv_instance;
		}
		indx++;
		ap = ap->next;
	}

	if (indx != ap_count)
		dr_logerr(DRV_FAIL, 0,
			"create_ctlr_array: count mismatch [internal error]");

	return (cp);
}

/*
 * free_ap_list
 */
static void
free_ap_list(void)
{
	ap_controllerp_t 	ap;

	while (ap_list) {
		ap = ap_list;
		ap_list = ap_list->next;
		free(ap);
	}
}

/*
 * add_net_ap_info
 *
 * For network devices, the information includes whether the
 * device is an alternate/active.  Additionally, the metadevice
 * name of the interface (eg. ap_nmle0) is returned.  We add this
 * metadevice name to the network netname list.  This allows
 * it to be checked for in find_net_entries().
 */
static void
add_net_ap_info(dr_leafp_t dlp, struct dr_info *infop)
{
	dr_iop_t	dp;

	/*
	 * Since we may have multiple interface names we make this
	 * query with, only update if we're part of the AP database.
	 */
	if (!infop->is_alternate)
		return;

	dp = dlp->major_dev;
	dp->dv_ap_info.is_alternate = infop->is_alternate;
	dp->dv_ap_info.is_active = infop->is_active;

	if (!infop->is_alternate || infop->ap_aliases.ap_aliases_len == 0)
		return;

	if (infop->ap_aliases.ap_aliases_len != 1)
		dr_loginfo("add_net_ap_info: multiple AP aliases ignored\n");

	/*
	 * Now call create_ap_net_leaf to save the AP meta-interface
	 * information.  How this is done is privy to the dr_info_net.c
	 * module.
	 */
	create_ap_net_leaf(dlp, infop->is_active,
			    infop->ap_aliases.ap_aliases_val[0].name);
}

/*
 * add_notnet_ap_info
 *
 * For non-network devices, the information includes whether the
 * device is an alternate/active.  Additionally, we have an array
 * of {dev_t, mc?t?d?} for each disk controlled by ap. The dev_t
 * is that of a partition of the disk and we find the parent of the
 * partition (eg, the disk) from this pair.
 *
 * Since not all disks under a AP controller may be dual ported, we need
 * to mark the ap_info field of the disk with whether it is an ap alternate.
 *
 * For active AP alternates, we create all the /dev/ap names for the disk
 * partitions and link them to the end of the disk's leaf chain.  This
 * allows us to collect usage information for the AP metadevices.
 *
 * For inactive AP alternates. we add a ap_metaname field to the last
 * parition for the disk.  This ap_metaname will be displayed as usage
 * of the disk.
 */
static void
add_notnet_ap_info(dr_iop_t ctrlp, struct dr_info *infop)
{
	dr_iop_t	dp;
	dr_leafp_t	dmp, *ctrl_leaves;
	dr_leaf_t	search_node, *sp, **fp;
	ap_alias_t	*aliasp;
	int		i, num_ctrl_leaves;

	ctrlp->dv_ap_info.is_alternate = infop->is_alternate;
	ctrlp->dv_ap_info.is_active = infop->is_active;

	if (!infop->is_alternate || infop->ap_aliases.ap_aliases_len == 0)
		return;

	/*
	 * First go through all devices for this controller
	 * and make a list we can sort by devid.  We need this to
	 * find a match between physical device and metadevice.
	 */
	ctrl_leaves = calloc(num_leaves, sizeof (dr_leafp_t));
	if (ctrl_leaves == NULL) {
		dr_logerr(DRV_FAIL, errno, \
			"malloc failed (add_notnet_ap_info)");
		return;
	}
	num_ctrl_leaves = 0;

	dp = ctrlp->dv_child;
	while (dp) {
		dmp = dp->dv_leaf;
		while (dmp) {

			if (dmp->devices_name) {
				/*
				 * the device_id was stat'd when
				 * the leaf node was created.
				 */
				ctrl_leaves[num_ctrl_leaves] = dmp;
				num_ctrl_leaves++;
			}
			dmp = dmp->next;
		}
		dp = dp->dv_sibling;
	}

	if (num_ctrl_leaves == 0) {
		free(ctrl_leaves);
		return;
	}

	qsort((void *)ctrl_leaves, num_ctrl_leaves, sizeof (dr_leafp_t),
	    compare_devid);

	/*
	 * Now go through our list of metadevices and find which
	 * physical device each metadevice corresponds to.
	 */
	aliasp = infop->ap_aliases.ap_aliases_val;
	for (i = 0; i < infop->ap_aliases.ap_aliases_len; i++) {

		    search_node.device_id = aliasp[i].devid;
		    sp = &search_node;
		    fp = bsearch((void *)&sp, (void *)ctrl_leaves,
			    num_ctrl_leaves, sizeof (dr_leafp_t),
			    compare_devid);

		/*
		 * We should always get a match.  Chirp up if
		 * we don't.
		 */
		    if (fp == NULL) {
			    dr_loginfo("Cannot find physical device for %s\n",
				    aliasp[i].name);
			    continue;
		    }

#ifdef INFO_AP_DEBUG
		    dr_loginfo("AP MATCH: %s (%d) == (%d) %s\n", aliasp[i].name,
			    aliasp[i].devid, (*fp)->device_id,
			    (*fp)->devices_name);
#endif INFO_AP_DEBUG
		/*
		 * now find the parent (disk node) of this partition
		 * and mark it's AP information.
		 */
		    if ((*fp)->major_dev) {
			    dp = (*fp)->major_dev;
			    dp->dv_ap_info.is_alternate = infop->is_alternate;
			    dp->dv_ap_info.is_active = infop->is_active;
		    } else {
			/*
			 * A partition should always have a parent.
			 * Chirp up if this isn't so
			 */
			    dr_loginfo("Partition %s does not have parent\n",
				    (*fp)->devices_name);
		    }

		    /* Find the end of the leaf list for the disk */
		    dmp = (*fp);
		    while (dmp->next != NULL) {
			    dmp = dmp->next;
		    }

		/*
		 * Mark the last parition with the meta-device
		 * name so we display a usage record connecting the
		 * /dev name to the AP metaname.
		 */
		    dmp->ap_metaname = strdup(aliasp[i].name);

		/*
		 * For active alternates, also add the AP partition
		 * names so we discover usage via those names also.
		 */
		    if (infop->is_active != 0) {
			    create_ap_partitions(dmp, aliasp[i].name);
		    }
	}

	free(ctrl_leaves);
}

/*
 * compare_devid
 *
 * called from qsort/bsearch to determine if the dev_t for two
 * dr_leaf_t structures are <=>.  Used to match AP metadevices with
 * their corresponding physical devices.
 */
static int
compare_devid(const void *a, const void *b)
{
	dr_leafp_t	ap, bp;

	ap = (dr_leafp_t)(*(dr_leafp_t *)a);
	bp = (dr_leafp_t)(*(dr_leafp_t *)b);

	if (ap->device_id > bp->device_id)
		return (1);
	else if (ap->device_id < bp->device_id)
		return (-1);
	else
		return (0);
}

/*
 * create_ap_partitions
 *
 * This routine forms all the AP partition names for the given AP
 * metadevice and links them to the end of the specified leaf list.
 *
 * Input:
 *	dmp	- last element in the leaf list.  We add all new
 *		  AP partition leaves here.
 *	apname	- the mc?t?d? name used to create the AP partition names.
 *
 * Output: none
 */
static void
create_ap_partitions(dr_leaf_t *dmp, char *apname)
{
	int		i;
	char		devpath[MAXFILNAM];
	struct stat	sst;

	/*
	 * Now create all the parition names.  Note that the
	 * number of partitions is hard coded here just like it
	 * is in the AP daemon code.  Both should probably use
	 * the system define.
	 */
	for (i = 0; i <= 7; i++) {

		if ((dmp->next = dr_leaf_malloc()) == NULL)
			break;
		dmp->next->major_dev = dmp->major_dev;
		dmp = dmp->next;

		/* The raw partitions */
		sprintf(devpath, "%s%ss%d", AP_DEV_RDSK_DIR,
			apname, i);
		dmp->dev_name = strdup(devpath);

		/*
		 * Now find the major/minor number of the /dev/ap
		 * file. This is needed in case the ap partition
		 * hosts an OL:DS database since all we know about
		 * these database locations are their major/minor
		 * numbers.
		 */
		if (stat(dmp->dev_name, &sst) != 0) {

			dr_loginfo("WARNING: cannot stat %s, errno=%d",
				    dmp->dev_name, errno);
		} else {
			dmp->device_id = sst.st_rdev;
		}

		if ((dmp->next = dr_leaf_malloc()) == NULL)
			break;
		dmp->next->major_dev = dmp->major_dev;
		dmp->next->device_id = dmp->device_id;
		dmp = dmp->next;

		/* The formatted partitions */
		sprintf(devpath, "%s%ss%d", AP_DEV_DSK_DIR,
			apname, i);
		dmp->dev_name = strdup(devpath);
	}
}

/*
 * add_ap_db_locs
 *
 * Find out from the ap_daemon which partitions it uses for it's
 * databases.  The ap_daemon know the major/minor number (and /dev
 * name) of the database.  At this time, all DR knows about is the
 * /devices name and the major/minor number, so match on major/minor.
 *
 * ap_list contains all the possible AP controllers on the board.  Since
 * this list also contains all disk controllers for the board, search
 * through ap_list looking for matches on the major/minor numbers.
 *
 * This routine doesn't attempt to get fancy about this search.  The
 * number of database partitions is probably small in comparison to the
 * partitions on the board, so just do it
 */
static void
add_ap_db_locs(void)
{
	all_db_info		*info;
	ap_controllerp_t 	ap;
	dr_iop_t		dp;
	dr_leafp_t		dmp;

	/* Find out where AP has it's databases */
	info = do_ap_db_query();
	if (info == NULL)
		/* A comm error or something - just return */
		return;

	/* If there are no databases, do nothing */
	if (info->numdb == 0)
		return;

	/* Invalid number of database copies; report an error and do nothing */
	if (info->numdb <= 0 || info->numdb > MAX_DB) {
		dr_loginfo("AP reported an invalid number of databases. "
			"(numdb = %d)\n", info->numdb);
		return;
	}

	ap = ap_list;
	while (ap) {

		/* Only look at nodes which may be disk controllers */
		if (ap->type != DEVICE_NOTNET) {
			ap = ap->next;
			continue;
		}

		/* Look at the child (disk) nodes of the controller */
		dp = ap->notnet_node->dv_child;
		while (dp) {

			/* Look at the leaves (partitions) of the disk */
			dmp = dp->dv_leaf;
			while (dmp) {

				apdb_devt_match(dmp, info);
				dmp = dmp->next;
			}
			dp = dp->dv_sibling;
		}
		ap = ap->next;
	}
}

/*
 * apdb_devt_match
 *
 * Search through the list of AP database locations and see if
 * the major/minor number of the database matches the major/minor
 * number of the given partition.  Note that since major/minor numbers
 * for the blocked and raw devices are identical, we make sure we're
 * matching the raw parition name so that we consistently report usage
 * to be for the raw partition.
 *
 * Input:
 *	dmp	pointer to the leaf to check
 *	info	pointer to the AP DB info structure
 */
static void
apdb_devt_match(dr_leafp_t dmp, all_db_info *info)
{
	int	i;
	ulong_t	devid;
	char 	*cp;

	if (info->numdb <= 0 || info->numdb > MAX_DB) {
		dr_loginfo("The AP database information has been corrupted. "
			"(numdb = %d)\n", info->numdb);
		return;
	}

	for (i = 0; i < info->numdb; i++) {

		devid = (ulong_t)makedev((ulong_t)(info->db[i].major),
					(ulong_t)(info->db[i].minor));

		if (dmp->device_id == devid) {

			/*
			 * First confirm this is the raw partition by
			 * checking for a ",raw" at the end of the devices
			 * name.
			 */
			if (dmp->devices_name) {

				cp = strrchr(dmp->devices_name, ',');
				if (cp == NULL || strcmp(cp, ",raw") != 0) {
					return;
				}
			}
			dmp->notnetflags |= NOTNET_AP_DB;
		}
	}
}

#endif AP
