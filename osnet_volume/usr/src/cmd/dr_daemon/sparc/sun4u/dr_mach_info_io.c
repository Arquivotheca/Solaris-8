/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dr_mach_info_io.c	1.13	99/06/04 SMI"

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

#include "dr_info.h"
#include "dr_openprom.h"

extern int num_leaves;			/* number of leaf devices found */
extern char devices_path[MAXFILNAM];	/* used to build /devices name */
extern dr_leafp_t *leaf_array;		/* leaf table */

/*
 * Buffers used to hold a list of encountered directories.
 * This list is used to detect recursive links in the
 * file system which can cause the daemon to hang.
 * NOTE - this also protects against "NULL" links, i.e.
 * links to nothing (like  'link -> '), which are interpreted by
 * the OS as links to '.' -  which are recursive.
 */
#define	MAX_DIRBUF_SIZE 2048	/* should be sufficient */
char	dirlist[MAX_DIRBUF_SIZE];
static char	tmpdirlist[MAX_DIRBUF_SIZE];

/*
 * function declarations
 */
int compare_devices(const void *ap, const void *bp);
void walk_dir(char *name);
void build_rpc_info(dr_iop_t dp, attached_board_infop_t bcp);
void build_cntrl_info(sbus_cntrlp_t *spp, dr_iop_t dip);

/*
 * walk_dir
 *
 * Recursively walk the /dev directory looking for files which are
 * symbolically liked to the board devices.  If one is found, save
 * the /dev name in the dr_leaf_t structure entry.
 *
 * Input: name - file/directory to examine
 */
void
walk_dir(char *name)
{
	struct stat	sst,
			lst;
	struct dirent	*de;
	char		*p;
	char		*p2;
	int		skipit = 0;
	mode_t		type;
	char		tpath[MAXFILNAM];
	char		plink[MAXFILNAM];
	char		junk[MAXFILNAM];
	char		junk2[MAXFILNAM];
	DIR		*dir;
	dr_leaf_t	search_node, *sp, **fp;

	if (stat(name, &sst) != 0) {
		/*
		 * Nothing to do if the file can't be stat'd.  This
		 * can happen if the file is a symbolic link to a
		 * non-existant file.
		 */
		return;
	}
	type = sst.st_mode & S_IFMT;

	if (type == S_IFDIR) {

		/*
		 * This is a directory, visit it's children
		 */
		if ((dir = opendir(name)) == NULL) {
			dr_loginfo("Unable to open directory %s %d",
			    name, errno);
			return;
		}

		strcpy(tpath, name);
		p = &tpath[strlen(name)];
		*p++ = '/';
		*p = '\0';

		/*
		 * We change the current working directory since symbolic
		 * links may be relative to '.'.  Be sure to restore
		 * the CWD before returning.
		 */
		if (getcwd(junk, sizeof (junk)) == NULL) {
			dr_loginfo("Unable to find cwd %d", errno);
			closedir(dir);
			return;
		}
		if (chdir(name) < 0) {
			dr_loginfo("Unable to set cwd %d\n", errno);
			closedir(dir);
			return;
		}

		/*
		 * We get the cwd again in case 'name' is a link to
		 * directory, we will get the "real" directory it
		 * points to.
		 */
		if (getcwd(junk2, sizeof (junk2)) == NULL) {
			dr_loginfo("Unable to find the cwd %d", errno);
			closedir(dir);
			return;
		}

		/*
		 * If we have already been to this directory before,
		 * then this is a recursive link. Just ignore it.
		 * Otherwise put this directory into the list.
		 */

		strcpy(tmpdirlist, dirlist); /* copy over for strtok */

		for (p2 = strtok(tmpdirlist, ":"); p2;
		    p2 = strtok(NULL, ":")) {
			if (strcmp(p2, junk2) == 0) {
				dr_loginfo("Recursive symlink found '%s'."
				    " Please remove it.\n", name);
				skipit = 1;
				break;
			}
		}

		if (!skipit) {
			if ((strlen(dirlist) + strlen(junk2) + 1) >
			    MAX_DIRBUF_SIZE) {
				/*
				 * we have traversed so many directories,
				 * that we will overflow dirlist. This
				 * should never happen, but if it does,
				 * we will just stop keeping track of
				 * recent directories and hope we don't
				 * hit a recursive link. That's the way
				 * it goes with static buffers!
				 */
				dr_loginfo("walk_dir: dirlist buffer "
				    "overflow.\n");
			} else {
				strcat(dirlist, junk2);
				strcat(dirlist, ":");
			}
			while ((de = readdir(dir)) != (struct dirent *)NULL) {
				if (!(strcmp(".", de->d_name)) ||
				    !(strcmp("..", de->d_name)))
					continue;
				if ((strlen(tpath) + strlen(de->d_name)) >
				    MAXFILNAM) {
					/*
					 * we are about to overflow
					 * tpath. This is bad news, so
					 * just give an error message
					 * and ignore this directory.
					 * Not much else we can do... :((
					 */
					dr_loginfo("walk_dir: tpath buffer "
					    "overflow. %s, %s\n", tpath,
					    de->d_name);
					continue;
				}
				strcpy(p, de->d_name);
				walk_dir(tpath);
			}
		}

		closedir(dir);

		if (chdir(junk) < 0) {
			dr_loginfo("Unable to restore cwd %d\n", errno);
		}

		return;
	}

	if ((type != S_IFCHR) && (type != S_IFBLK))
		return;

	/*
	 * Resolve links.
	 */
	strcpy(plink, name);
	do {
		int len;

		if (lstat(plink, &lst) != 0) {
			dr_loginfo("Unable to lstat %s %d\n", plink, errno);
			return;
		}

		if ((type = (lst.st_mode & S_IFMT)) == S_IFLNK) {
			if ((len = readlink(plink, junk, sizeof (junk))) < 0) {
				dr_loginfo("Unable to readlink %s %d\n",
					plink, errno);
				return;
			}

			junk[len] = '\0';
			if ((p = strstr(junk, "/devices")) != NULL) {
				/* make an absolute /devices pathname */
				strcpy(plink, p);
			} else {
			/* link not to /devices entry - not interested */
				return;
			}
		}
	} while (type == S_IFLNK);

	/*
	 * Match the /devices name
	 */
	search_node.devices_name = plink;
	sp = &search_node;
	fp = bsearch((void *)&sp, (void *)leaf_array,
		    num_leaves, sizeof (dr_leafp_t), compare_devices);

	/*
	 * No match means that we have /dev entries which
	 * are linked to board devices which are not present in
	 * the current configuration.
	 */
	if (fp == NULL) {
		return;
	}

	/*
	 * Now save the /dev name.  We ignore
	 * multiple names linked to the same /devices
	 * path since all costing info for them
	 * is identical.  The first /dev name found
	 * is the one we'll use.
	 */
	if ((*fp)->dev_name == NULL) {
		/*
		 * An extreme hack for sonoma devices.
		 * The sonoma software creates links under /dev/dsk
		 * to a pseudo device. The real links to the actual
		 * devices under /devices for sonoma are under
		 * /dev/osa/dev/dsk. However, it is documented that users
		 * should only use the devices under /dev/dsk for sonoma.
		 * Since we always find the /dev/osa... devices (because
		 * they are the REAL devices) just hack off the /dev/osa
		 * part of the device name. This way we will find
		 * usage on the device correctly. I KNOW - IT'S A HACK!!
		 */
		if (strncmp("/dev/osa", name, strlen("/dev/osa")) == 0) {
			name += strlen("/dev/osa");
		}
		(*fp)->dev_name = strdup(name);
		if (verbose > 10)
			dr_loginfo("FOUND - %s, %s\n", name, plink);
	}
}

/*
 * build_rpc_info
 *
 * Now that all the data is gathered, transfer it from our work
 * structures into the RPC info structure.
 *
 * the node structure from  build_device_tree() basically looks like:
 *
 *		rootp -> IObus -----> IObus
 *			/           /
 *		    controllers    controllers
 *		   /               /
 *		  devices         devices
 *
 * Where "IObus" is either Sbus or PCI.
 *
 * The "controllers" nodes have address fields which
 * indicate which slot the device's board is in.  Each slot may contain
 * multiple controllers.
 *
 * The RPC info structure contains three types of records:
 *
 *    sbus_cntrl_t - These records represent controller such as isp0.
 *	We find the controllers by finding the dr_io_t leaf nodes (devices such
 *	as sd0).  The controller is the parent of the leaf device.  Note
 *	that some devices don't have a parent (such as the css node).  In
 *	this case, the controller record has no name (NULL).
 *
 *    sbus_device_t - These records represent the devices such as sd0.
 *	These records are created from the dr_io_t leaf nodes.
 *
 *    sbus_usage_t - These records are created from the dr_leaf_t records
 *	for the dr_io_t leaf nodes or in the case of network devices, from
 *	the info stored in the leaf dr_io_t record.
 */
#define	MAX_PSYCHO_PER_BOARD	2
#define	PSYCHO_IOC_SLOT		2
void
build_rpc_info(dr_iop_t dp, attached_board_infop_t bcp)
{
	int		slot;
	int		sysio_num;
	int		psycho_num;
	int		upaid;
	dr_iop_t 	cdp;

	/*
	 * Determine which slot a device is in and then build the RPC
	 * info for that slot.
	 * We must save the sysio number to create the slot number (0-3)
	 */
	while (dp) {

		if (sscanf(dp->dv_addr, "%x,", &upaid) != 1) {
			dr_logerr(DRV_FAIL, 0,
				"build_rpc_info: I/O bus node "
				    "address format error");
			return;
		}

		/*
		 * Sbus or PCI? Slot encoding is different
		 */
		if (strcmp(dp->dv_name, "sbus") == 0) {

			sysio_num = DEVICEID_UNIT_NUM(upaid);

			if (sysio_num < 0 ||
					sysio_num >= MAX_SBUS_SLOTS_PER_IOC) {
				dr_logerr(DRV_FAIL, 0,
				"build_rpc_info: sysio number out of range");
				return;
			}

			cdp = dp->dv_child;  /* get the cntrl nodes list head */

			while (cdp) {
				if (sscanf(cdp->dv_addr, "%d,", &slot) != 1) {
					dr_logerr(DRV_FAIL, 0,
						"build_rpc_info: device "
						"address format error");
					return;
				}

				if (slot < 0 ||
					slot >= MAX_SBUS_SLOTS_PER_IOC) {
					dr_logerr(DRV_FAIL, 0,
					"build_rpc_info: bad slot number");
					return;
				}

				if (sysio_num == 0) {
					build_cntrl_info(&(bcp->ioc0[slot]),
							cdp);
				} else {
					build_cntrl_info(&(bcp->ioc1[slot]),
							cdp);
				}

				cdp = cdp->dv_sibling;
			}

			dp = dp->dv_sibling;

		} else if (strncmp(dp->dv_name, "pci", 3) == 0) {

			psycho_num = DEVICEID_UNIT_NUM(upaid);

			if (psycho_num < 0 ||
					psycho_num >= MAX_PSYCHO_PER_BOARD) {
				dr_logerr(DRV_FAIL, 0,
				"build_rpc_info: psycho number out of range");
				return;
			}

			cdp = dp->dv_child;  /* get the cntrl nodes list head */

			while (cdp) {
				if (sscanf(cdp->dv_addr, "%d,", &slot) != 1) {
					dr_logerr(DRV_FAIL, 0,
						"build_rpc_info: device "
						"address format error");
					return;
				}

				if (slot != PSYCHO_IOC_SLOT) {
					dr_logerr(DRV_FAIL, 0,
					"build_rpc_info: bad slot number");
					return;
				}

				if (psycho_num == 0) {
					build_cntrl_info(&(bcp->ioc0[0]),
							cdp);
				} else {
					build_cntrl_info(&(bcp->ioc1[0]),
							cdp);
				}

				cdp = cdp->dv_sibling;
			}

			dp = dp->dv_sibling;
		} else {
			dr_loginfo("unknown node type\n");
		}
	}
}
