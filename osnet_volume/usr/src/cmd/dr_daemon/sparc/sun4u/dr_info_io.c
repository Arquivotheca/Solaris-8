/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_info_io.c	1.31	98/08/30 SMI"

/*
 * Determines devices on a specified board and their current usage by the
 * system.
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/mnttab.h>
#include <sys/swap.h>
#include <sys/utssys.h>

#include "dr_info.h"

/* No system header file defines these calls -- make lint happy */
extern int swapctl();
extern int utssys();

/*
 * Data which is shared between the dr_info_*.c modules.
 *
 * num_leaves is a count of the number of dr_leaf entries allocated.
 * it is used to size arrays which contain pointeres to dr_leaf entries.
 *
 * devices_path is used to find the link between /devices and /dev entries.
 */
int num_leaves;			/* number of leaf devices found */
char devices_path[MAXFILNAM];	/* used to build /devices name */

/*
 * function declarations
 */
static void get_dev_info(dr_iop_t rootp);
static void init_leaf_array(dr_iop_t dp);
int compare_devices(const void *ap, const void *bp);
extern void walk_dir(char *name);
static void find_mount_entries(void);
static void find_swap_entries(void);
static void find_usage_counts(void);
extern void build_rpc_info(dr_iop_t dp, attached_board_infop_t bcp);
void build_cntrl_info(sbus_cntrlp_t *spp, dr_iop_t dip);
static void add_notnet_usage_record(sbus_devicep_t dp, dr_iop_t dip);

/*
 * local data
 */
dr_leafp_t *leaf_array;		/* leaf table */
dr_leafp_t swap_partition;	/* first swap partition found */


/*
 * get_io_info
 *
 * Get the IO configuration for the specified system board.
 * This is done in three major steps:
 *
 *    1. We examine the kernel dev-info tree looking for IO devices
 *       resident on this board.
 *    2, We look for system usage of the devices found.
 *    3. We build the RPC structure to return to the user from the
 *       structures created in steps 1 & 2.
 */
void
get_io_info(int board, attached_board_infop_t bcp)
{
	dr_iop_t	rootp;

	/* read the dev-info tree */
	rootp = build_device_tree(board);
	if (rootp == NULL) {
		return;
	}

	/*
	 * Now find the /dev names and cost info for the board devices.
	 */
	get_dev_info(rootp);

	/* Create the RPC structures to return */
	build_rpc_info(rootp, bcp);

	/* Free up the temporary structures we're created */
	free_dr_device_tree(rootp);
}

/*
 * get_dev_info
 *
 * After we've read the dev-info tree for the board, determine the usage of
 * the device on the board.  In order to do this we must:
 *
 *    1. Find all the /dev entries which correspond to the /devices
 *	 entries for the board devices.  The only way to do this is
 *	 to walk through /dev and find these links.
 *    2. Add in pseudo /dev entries whose usage of board devices is not
 *	 found by step 1.  This includes disk paritions used by Sun Online
 *	 DiskSuite and disk controllers used by the Alternate Pathing
 *	 subsystem.
 *    3. Once we have a complete set of /dev entries for the board, go
 *	 through these and look for usage of these devices.
 *
 * The information found by this routine is placed in the dr_io_t tree.
 */
static void
get_dev_info(dr_iop_t rootp)
{
	extern char    dirlist[];
	/*
	 * Find out if any of the board devices are active AP
	 * meta devices.  If so, add these entries as leaf nodes
	 * linked onto the appropriate dv_leaf list.  Do this
	 * now so we don't have to resize the leaf array.  Also,
	 * we want these metadevices in our leaf_array list prior
	 * to trying to match up the Online DiskSuite entries since
	 * it may make use of these metadevices.
	 */
#ifdef AP
	add_ap_meta_devices(rootp, 0);
#endif AP

	/*
	 * first sort the /devices name so we can quickly find
	 * /dev files symbolically linked to these entries.  Note
	 * that the leaf array does not include DEVICE_NET leaf
	 * entries since these entries are handles specially.
	 */
	leaf_array = (dr_leafp_t *)calloc(num_leaves, sizeof (dr_leafp_t));
	if (leaf_array == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (leaf_array)");
		return;
	}

	num_leaves = 0;
	init_leaf_array(rootp);

	qsort((void *)leaf_array, num_leaves, sizeof (dr_leafp_t),
	    compare_devices);

	/*
	 * Now walk the /dev directory looking for files symbolically
	 * linked to the leaf_array /devices names.  These /dev names are
	 * added to the leaf nodes.
	 */
	/* clear out buffer used for recursive link checking */
	dirlist[0] = '\0';
	walk_dir("/dev");

	/*
	 * Now sort the array by /dev name order.
	 */
	qsort((void *)leaf_array, num_leaves, sizeof (dr_leafp_t),
	    compare_devname);

	/*
	 * Once we have the /dev names, we can figure out if any
	 * on the SUN Online DiskSuite pseudo devices are using
	 * disks on this board.  If so, add them to our leaf_array.
	 */
#ifdef	DR_OLDS
	add_disksuite_to_device_tree(&leaf_array);
#endif	/* DR_OLDS */

	/*
	 * Now determine the usage of the leaf_array  board devices
	 */
	find_swap_entries();		/* must precede find_mount_entries */
	find_mount_entries();
	find_usage_counts();

	/*
	 * Here's where we look for network device usage.
	 */
	find_net_entries(rootp, NULL);

	/* All done */
	free(leaf_array);
}

/*
 * init_leaf_array
 *
 * Traverse the dr_io tree finding all leaf structures which are
 * placed into the leaf_array.
 */
static void
init_leaf_array(dr_iop_t dp)
{
	dr_leafp_t	dlp;

	if (dp->dv_child) init_leaf_array(dp->dv_child);
	if (dp->dv_sibling) init_leaf_array(dp->dv_sibling);

	/* Don't save network leaf entries */
	if (dp->dv_node_type == DEVICE_NET)
		return;

	dlp = dp->dv_leaf;
	while (dlp) {
		leaf_array[num_leaves++] = dlp;
		dlp = dlp->next;
	}
}

/*
 * compare_devices
 *
 * called from qsort/bsearch to determine if the devices_name of
 * two dr_leaf_t structures  are > = <.  Handle the case were
 * the /devices name is non-existant.  This can happen with
 * network devices (it uses a /devices/pseudo entry instead) or
 * if the /devices entries have not been created yet and
 * drvconfig needs to be run.  Also for cases where we have a
 * pseduo device which only has a dev_name entry.
 */
int
compare_devices(const void *a, const void *b)
{
	dr_leafp_t	ap, bp;

	ap = (dr_leafp_t)(*(dr_leafp_t *)a);
	bp = (dr_leafp_t)(*(dr_leafp_t *)b);

	/*
	 * check for NULL devices name entries and
	 * have them sorted at the end of the file.
	 */
	if (ap->devices_name == NULL) {
		if (bp->devices_name == NULL)
			return (0);
		else
			return (1);
	} else if (bp->devices_name == NULL) {
		return (-1);
	}

	return (strcmp(ap->devices_name, bp->devices_name));
}

/*
 * compare_devname
 *
 * called from qsort/bsearch to determine if the /dev name in
 * two dr_leaf_t structures are > = <.  Note
 * that if is there is no /dev name, we create one which consists
 * of the complete /devices path.  If there is no dev_name, just
 * sort these entries to the end of the list.
 */
int
compare_devname(const void *a, const void *b)
{
	dr_leafp_t	ap, bp;

	ap = (dr_leafp_t)(*(dr_leafp_t *)a);
	bp = (dr_leafp_t)(*(dr_leafp_t *)b);

	/*
	 * If there is no /dev name, use the /devices name
	 * instead.
	 */
	if (ap->dev_name == NULL) {
		ap->dev_name = ap->devices_name;
		ap->devices_name = NULL;
	}

	if (bp->dev_name == NULL) {
		bp->dev_name = bp->devices_name;
		bp->devices_name = NULL;
	}

	/*
	 * check for NULL dev_name entries and
	 * have them sorted at the end of the file
	 */
	if (ap->dev_name == NULL) {
		if (bp->dev_name == NULL)
			return (0);
		else
			return (1);
	} else if (bp->dev_name == NULL) {
		return (-1);
	}

	return (strcmp(ap->dev_name, bp->dev_name));
}

/*
 * find_mount_entries
 */
static void
find_mount_entries(void)
{
	FILE	*fd;
	int	ret;
	struct mnttab	mget;
	dr_leaf_t	search_node, *sp, **fp;

	if ((fd = fopen(MNTTAB, "r")) == NULL) {
		dr_loginfo("Cannot open mnttab (errno=%d)\n", errno);
		return;
	}

	while ((ret = getmntent(fd, &mget)) == 0) {

		if (mget.mnt_special && mget.mnt_mountp) {
			/*
			 * mnt_special is the device file
			 * which is mounted.  See if it
			 * is on the board.
			 */

			if (swap_partition && strcmp("swap", mget.mnt_special)
			    == 0) {
				/*
				 * Special hack for swap partitions.  In
				 * find_swap_entries, we saved a swap
				 * partition pointer if one was found
				 * but we don't have a dr_leaf entry for
				 * swap.  So, save where swap is mounted.
				 * The routine which extracts the RPC
				 * info know to look for swap partition
				 * mount points.
				 */
				swap_partition->mount_point =
					strdup(mget.mnt_mountp);

			} else {
				search_node.dev_name = mget.mnt_special;
				sp = &search_node;
				fp = bsearch((void *)&sp, (void *)leaf_array,
					    num_leaves, sizeof (dr_leafp_t),
					    compare_devname);

				if (fp != NULL) {
					/* Found a match */
					(*fp)->mount_point =
						strdup(mget.mnt_mountp);
				}
			}
		}
	}

	if (ret > 0) {
		dr_loginfo("getmntent returned error\n");
	}

	(void) fclose(fd);
}

/*
 * find_swap_entries
 *
 * Find all the swap partitions and then see if any of them
 * are on this board.  If so, save the first one found so we
 * can see if anything is mounted on swap (see usage of
 * swap_partition in find_mount_entries, and add_notnet_usage_record).
 */
static void
find_swap_entries(void)
{
	int 		num_swap, i;
	swaptbl_t	*st;
	struct swapent	*swapent;
	char		fullpath[MAXFILNAM];
	dr_leaf_t	search_node, *sp, **fp;
	char		*path;

	swap_partition = NULL;

	/* First figure out how many entries we'll need */
	num_swap = swapctl(SC_GETNSWP, NULL);
	if (num_swap < 0) {
		dr_loginfo("swapctl SC_GETNSWP failed (errno=%d)\n", errno);
		return;
	}

	/* malloc a structure for them */
	if ((st = (swaptbl_t *)malloc(num_swap * sizeof (swapent_t)
				    + sizeof (int))) == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (swaptbl)");
		return;
	}

	/* malloc pathname entries for the structure */
	if ((path = (char *)malloc(num_swap * MAXFILNAM)) == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (swap name entries)");
		return;
	}
	swapent = st->swt_ent;
	for (i = 0; i < num_swap; i++, swapent++) {
		swapent->ste_path = path;
		path += MAXFILNAM;
	}
	st->swt_n = num_swap;

	/* Now get the swap entries  */
	if ((num_swap = swapctl(SC_LIST, st)) < 0) {
		dr_loginfo("Unable to get swap entries (errno=%d)\n", errno);
		return;
	}

	/* Now go through all the entries looking for a match */
	swapent = st->swt_ent;
	for (i = 0; i < num_swap; i++, swapent++) {

		if (*swapent->ste_path != '/')
			sprintf(fullpath, "/dev/%s", swapent->ste_path);
		else
			strcpy(fullpath, swapent->ste_path);

		search_node.dev_name = fullpath;
		sp = &search_node;
		fp = bsearch((void *)&sp, (void *)leaf_array,
			    num_leaves, sizeof (dr_leafp_t),
			    compare_devname);

		if (fp != NULL) {
			/* Save the first swap partition we encounter */
			if (swap_partition == NULL)
				swap_partition = *fp;

			/* Found a match, save this information */
			(*fp)->notnetflags |= NOTNET_SWAP;
		}
	}

	/*
	 * Now free up the memory we allocated. the pathname buffers
	 * were allocated in a single chunk.  The first swap entry
	 * has the pointer to this chunk.
	 */
	free(st->swt_ent[0].ste_path);
	free(st);
}

/*
 * find_usage_counts
 *
 * For all /dev and /devices names, find the usages
 * counts for those files.  We use the (undocumented)
 * utssys call ala fuser to find this information.
 */
static void
find_usage_counts(void)
{
	int		npids, i;
	f_user_t 	fuser_list[1024];

	for (i = 0; i < num_leaves; i++) {

		/* If there is a mount point, find usage on that */
		if (leaf_array[i]->mount_point) {

			npids = utssys(leaf_array[i]->mount_point,
				F_CONTAINED, UTS_FUSERS, fuser_list);
			if (npids < 0)  {
				dr_loginfo("utssys failed (%d) for %s\n",
				    errno, leaf_array[i]->mount_point);
				leaf_array[i]->open_count = -1;
			} else if (npids > 0)
				leaf_array[i]->open_count = npids;

		} else if (leaf_array[i]->dev_name) {

			/* no mount point, look at the raw device */

			npids = utssys(leaf_array[i]->dev_name, 0,
				    UTS_FUSERS, fuser_list);
			if (npids < 0) {
				dr_loginfo("utssys failed (%d) for %s\n",
				    errno, leaf_array[i]->dev_name);
				leaf_array[i]->open_count = -1;
			} else if (npids > 0)
				leaf_array[i]->open_count = npids;
		}
	}
}

/*
 * build_cntrl_info
 *
 * Build the sbus_cntrl_t and sbus_device_t records for the
 * devices and controllers in the dip tree.  The controller
 * records are linked into the slot controller list.
 *
 * Input: spp - pointer to head of the slot's controller list
 *	  dip	device tree for the slot.
 */
void
build_cntrl_info(sbus_cntrlp_t *spp, dr_iop_t dip)
{
	sbus_cntrlp_t	cp;
	sbus_devicep_t	dp, tdp;
	char		temp[MAXFILNAM];

	/*
	 * traverse the tree until we find a leaf node
	 */
	if (dip->dv_child != NULL) {
		/*
		 * In build_rpc_info above we traverse the silbing
		 * tree of the topmost nodes and sort them according to
		 * the slot number.  All the topmost nodes have a NULL
		 * parent and we ignore them here.
		 */
		if (dip->dv_parent && dip->dv_sibling)
			build_cntrl_info(spp, dip->dv_sibling);
		build_cntrl_info(spp, dip->dv_child);
		return;
	}

	/*
	 * A leaf node represents a device.  Build and link into the
	 * sbus_cntrl_t slot list a controller node for this child and
	 * it's siblings.
	 */
	cp = calloc(1, sizeof (sbus_cntrl_t));
	if (cp == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed(sbus_cntrl_t)");
		return;
	}
	cp->next = (*spp);
	(*spp) = cp;

	/*
	 * If there is a parent of this leaf node, then we use that
	 * device as the controller for the node.  This works for
	 * devices like disks which have a controller (isp, pln) and
	 * leaf children (sd or ssd nodes).
	 *
	 * If there is no parent, just use the leaf name as the
	 * controller.  Typically nodes like network interfaces or
	 * perhaps isp controllers with no attached disk children
	 * would be parentless.  Hostview and dr(1m) will list the
	 * controller and the AP properties of the controller, and
	 * then detail the usage of the leaf node if there is any.
	 * In the case of network interfaces, we may duplicate the
	 * AP information in the display (once for the controller and
	 * once in the usage record), but this is desired, especially
	 * for the Hostview display, so the AP information is consistently
	 * shown for the SBUS controller display.
	 */
	if (dip->dv_parent != NULL) {
		sprintf(temp, "%s%d", dip->dv_parent->dv_name,
			dip->dv_parent->dv_instance);
		cp->name = strdup(temp);
		cp->ap_info = dip->dv_parent->dv_ap_info;

	} else {
		sprintf(temp, "%s%d", dip->dv_name, dip->dv_instance);
		cp->name = strdup(temp);
		cp->ap_info = dip->dv_ap_info;
	}

	/*
	 * Now go through the sibling devices (children of the controller)
	 * and add sbus_device records for them.
	 */
	while (dip) {

		/*
		 * Some devices have children with siblings who have
		 * their own children (like SunSwift & FreshChoice)...
		 */
		if (dip->dv_child != NULL) {
			build_cntrl_info(spp, dip->dv_child);
		}

		/*
		 * Get a device entry --  add to the end of
		 * the list to maintain device order.
		 */
		dp = calloc(1, sizeof (sbus_device_t));
		if (dp == NULL) {
			dr_logerr(DRV_FAIL, errno,
				    "malloc failed (sbus_device_t)");
			return;
		}
		if (cp->devices == NULL) {
			cp->devices = dp;
		} else {
			tdp = cp->devices;
			while (tdp->next != NULL) tdp = tdp->next;
			tdp->next = dp;
		}

		sprintf(temp, "%s%d", dip->dv_name, dip->dv_instance);
		dp->name = strdup(temp);

		if (dip->dv_node_type == DEVICE_NET) {
			add_net_usage_record(dp, dip);
		} else {
			add_notnet_usage_record(dp, dip);
		}

		/*
		 * Once again, ignore the siblings of the topmost nodes.
		 */
		if (dip->dv_parent == NULL)
			dip = NULL;
		else
			dip = dip->dv_sibling;
	}
}

/*
 * add_notnet_usage_record
 *
 * Fill in the usage records for the given non-network device.
 * Each device can have many dr_leaf_t entries in it's dv_leaf list.
 * each of these structure represents another name which the device may
 * be known by such as partition names on a disk.  In order to reduce
 * the volumne of data reported back to the user, only report those
 * leaf entries which are in use.  If no leaf entries are in use,
 * report back at least one entry so the user knows what /dev name it
 * is known by.
 *
 * Input: dp -- device to attach the usage record to.
 *	  dip -- record describing the device.
 */
static void
add_notnet_usage_record(sbus_devicep_t dp, dr_iop_t dip)
{
	sbus_usagep_t	up;
	dr_leafp_t	dmp;
	char		usage[MAXFILNAM];

	dmp = dip->dv_leaf;
	while (dmp) {

		/*
		 * Add a usage record only if this leaf record represents
		 * usage of the device or if it's the last leaf record
		 * and we've not reported usage as of yet.
		 */
		if (dmp->ds_dev == NULL && dmp->mount_point == NULL &&
		    dmp->notnetflags == 0 && dmp->open_count <= 0 &&
		    dmp->ap_metaname == NULL &&
		    !(dmp->next == NULL && dp->usage == NULL)) {

			dmp = dmp->next;
			continue;
		}

		up = calloc(1, sizeof (sbus_usage_t));
		if (up == NULL) {
			dr_logerr(DRV_FAIL, errno, \
				"malloc failed (sbus_usage_t)");
			return;
		}
		up->next = dp->usage;
		dp->usage = up;
		usage[0] = 0;

		if (dmp->dev_name)
			up->name = strdup(dmp->dev_name);
		up->usage_count = dmp->open_count;

		if (dmp->notnetflags & NOTNET_SWAP) {
			/*
			 * If swap is mounted them add that information
			 * here.
			 */
			if (swap_partition->mount_point) {

				up->usage_count = swap_partition->open_count;

				/* opt_info is 'swap, mount point' */
				sprintf(usage, "swap, %s",
					swap_partition->mount_point);
			} else
				strncat(usage, "swap", sizeof (usage));

		} else if (dmp->mount_point) {
			strncat(usage, dmp->mount_point, sizeof (usage));

		}

		if (dmp->ds_dev) {
			if (usage[0] != 0)
				strncat(usage, ", ", sizeof (usage));

			sprintf(usage, "metadisk %s%d",
				dmp->ds_dev->dv_name,
				dmp->ds_dev->dv_instance);
		}

		if (dmp->notnetflags & NOTNET_OLDS_DB) {
			if (usage[0] != 0)
				strncat(usage, ", ", sizeof (usage));

			strncat(usage, "Online DiskSuite database",
				sizeof (usage));
		}

		if (dmp->notnetflags & NOTNET_AP_DB) {
			if (usage[0] != 0)
				strncat(usage, ", ", sizeof (usage));

			strncat(usage, "AP database", sizeof (usage));
		}

		/*
		 * Add information about the AP metaname if this
		 * is approriate.
		 */
		if (dmp->ap_metaname) {
			if (usage[0] != 0)
				strncat(usage, ", ", sizeof (usage));

			strncat(usage, dmp->ap_metaname, sizeof (usage));

			/*
			 * Usually the device will be known by it's /dev/dsk
			 * or /dev/rdsk name.  Remove the slice number in
			 * this case.
			 */
			if (up->name &&
			    (strncmp(up->name, "/dev/dsk/c", 10) == 0 ||
			    strncmp(up->name, "/dev/rdsk/c", 11) == 0)) {

				char *slice;

				slice = strrchr(up->name, 's');
				if (slice != NULL)
					*slice = 0;
			}
		}

		up->opt_info = strdup(usage);
		dmp = dmp->next;
	}
}

/*
 * The code below hashes in the /etc/driver_aliases mappings
 */

static struct devnm	*dv_hashtab[HASHTABSIZE];
static int		dv_hashcnt[HASHTABSIZE];

static int
nm_hash(char *name)
{
	register char c;
	register int hash = 0;

	for (c = *name++; c; c = *name++)
		hash ^= c;

	return (hash & HASHMASK);
}


static void
make_dvhash(char *name, char *alias)
{
	register struct devnm *bp1;
	register int namelen, alias_namelen, hashndx;

	bp1 = (struct devnm *)calloc(1, sizeof (struct devnm));
	if (bp1 == NULL) {
		dr_logerr(DRV_FAIL, 0, "calloc failed(struct devnm)");
		exit(1);
		/*NOTREACHED*/
	}
	namelen = strlen(name);
	if ((bp1->name = (char *)malloc(namelen + 1)) == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (struct devnm)");
		exit(1);
		/*NOTREACHED*/
	}
	(void) strcpy(bp1->name, name);
	if (alias != NULL) {
		alias_namelen = strlen(alias);
		if ((bp1->alias = (char *)malloc(alias_namelen + 1)) == NULL) {
			dr_logerr(DRV_FAIL, errno, \
				"malloc failed (alias_namelen)");
			exit(1);
			/*NOTREACHED*/
		}
		(void) strcpy(bp1->alias, alias);
	} else
		bp1->alias = NULL;
	hashndx = nm_hash(alias);
	bp1->next = dv_hashtab[hashndx];
	dv_hashtab[hashndx] = bp1;
	dv_hashcnt[hashndx]++;
}

#ifdef	DUMP_HASH
static void
dump_table()
{
	int i;
	struct devnm *p;

	dr_loginfo("--- Dumping Driver Alias Table ---\n");
	for (i = 0; i < HASHTABSIZE; i++) {
		dr_loginfo("Bucket %d\n", i);
		for (p = dv_hashtab[i]; p != NULL; p = p->next) {
			dr_loginfo("name = %s, alias = %s\n",
				p->name, p->alias);
		}
	}
}
#endif	DUMP_HASH

static void
dequote(char *word)
{
	char	buf[1024];
	char	*s;
	int	i;

	s = word; i = 0;
	while (*s != '\0') {
		if (*s != '"') {
			buf[i++] = *s;
		}
		s++;
	}
	buf[i] = '\0';
	strcpy(word, buf);
}

char *
drv_alias2nm(char *alias)
{
	struct devnm	*p;
	int		hashndx;

	hashndx = nm_hash(alias);
	for (p = dv_hashtab[hashndx]; p; p = p->next) {
		if (strcmp(alias, p->alias) == 0) {
			return (p->name);
		}
	}
	return (alias);
}

/*
 * Read the /etc/driver_aliases mappings into a hash table for easy
 * lookup by dr_daemon.  Although we can function without this information,
 * it is very likely DR operations will be impacted somewhere down the line
 * if this information is not available.
 */
void
hash_drv_aliases()
{
	FILE	*f;
	int	i;
	char	*name;
	char	*alias;
	char	buf[1024];

	for (i = 0; i < HASHTABSIZE; i++) {
		dv_hashtab[i] = NULL;
		dv_hashcnt[i] = 0;
	}

	if ((f = fopen(DAFILE, "r")) == NULL) {
		dr_logerr(DRV_FAIL, errno, "cannot open /etc/driver_aliases; " \
			"dr_daemon may not operate correctly without driver " \
			"alias mappings");
	}
	while (fgets(buf, 1024, f) != NULL) {

		/*
		 * tokenize
		 */
		name = strtok(buf, " \n");
		if (name == NULL)
			continue;
		(void) dequote(name);
		alias = strtok(NULL, " \n");
		if (alias == NULL)
			continue;
		(void) dequote(alias);
		make_dvhash(name, alias);
	}
#ifdef	DUMP_HASH
	dump_table();
#endif	DUMP_HASH

	(void) fclose(f);
}
