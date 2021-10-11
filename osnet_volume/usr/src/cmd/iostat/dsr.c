/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dsr.c	1.13	99/12/01 SMI"

#include <sys/stat.h>
#include <sys/types.h>

/*
 * Dependent on types.h, but not including it...
 */
#include <sys/dkio.h>
#include <ctype.h>
#include <dirent.h>
#include <libdevinfo.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/dktp/fdisk.h>

#include "iostat.h"

static void rummage_dev(ldinfo_t *);
static void do_snm(char *, char *);
static int look_up_name(const char *, disk_list_t *);
static disk_list_t *make_an_entry(char *, char *,
    char *, dir_info_t *, int, ldinfo_t *);
static char *trim(char *, char *, int);
static ldinfo_t	*rummage_devinfo(void);
static void pline(char *, int, char *, ldinfo_t **);
static void insert_dlist_ent(disk_list_t *, disk_list_t **);
static int str_is_digit(char *);
static ldinfo_t *find_ldinfo_match(char *, ldinfo_t *);

static void insert_into_dlist(dir_info_t *, disk_list_t *);
static void cleanup_dlist(dir_info_t *);
static void cleanup_ldinfo(ldinfo_t *);
static int devinfo_ident_disks(di_node_t, void *);
static int devinfo_ident_tapes(di_node_t, void *);
static void process_dir_ent(char *dent, int curr_type,
    char *last_snm, dir_info_t *, ldinfo_t *);

/*
 * To do: add VXVM support: /dev/vx/dsk and ap support: /dev/ap/
 *
 * Note: Adding support for VxVM is *not* as simple as adding another
 * entry in the table and magically getting to see stuff related to
 * VxVM. The structure is radically different *AND* they don't produce
 * any IO kstats.
 */

#define	OSA_DISK	0
#define	DISK		1
#define	MD_DISK		2
#define	TAPE		3

#define	MAX_TYPES	4

#define	OSA_DISK_PATH	"/dev/osa/dev/dsk"
#define	MD_DISK_PATH	"/dev/md/dsk"
#define	DISK_PATH	"/dev/dsk"
#define	TAPE_PATH	"/dev/rmt"

#define	BASE_TRIM	"../../devices"
#define	MD_TRIM		"../../../devices"
#define	COLON		':'
#define	COMMA		','

dir_info_t dlist[MAX_TYPES] = {
	OSA_DISK_PATH, 0, 0, 0, 0, "sd", BASE_TRIM, COLON,
	DISK_PATH, 0, 0, 0, 0, "sd", BASE_TRIM, COLON,
	MD_DISK_PATH, 0, 0, 0, 1, "md", MD_TRIM, COMMA,
	TAPE_PATH, 0, 0, 0, 0, "st", BASE_TRIM, COLON,
};

/*
 * Build a list of disks attached to the system.
 */
void
build_disk_list(void)
{
	ldinfo_t *ptoi;

	/*
	 * Build the list of devices connected to the system.
	 */
	ptoi = rummage_devinfo();
	rummage_dev(ptoi);
	cleanup_ldinfo(ptoi);
}

/*
 * Walk the /dev/dsk and /dev/rmt directories building a
 * list of interesting devices. Interesting is everything in the
 * /dev/dsk directory. We skip some of the stuff in the /dev/rmt
 * directory.
 *
 * Note that not finding one or more of the directories is not an
 * error.
 */
static void
rummage_dev(ldinfo_t *ptoi)
{
	DIR		*dskp;
	int		i;
	struct stat 	buf;

	for (i = 0; i < MAX_TYPES; i++) {
		if (stat(dlist[i].name, &buf) == 0) {
			if (dlist[i].mtime != buf.st_mtime) {
				/*
				 * We've found a change. We need to cleanup
				 * old information and then rebuild the list
				 * for this device type.
				 */
				cleanup_dlist(&dlist[i]);
				dlist[i].mtime = buf.st_mtime;
				if ((dskp = opendir(dlist[i].name))) {
					struct dirent 	*bpt;
					char	last_snm[NAME_BUFLEN];

					last_snm[0] = NULL;
					while ((bpt = readdir(dskp)) != NULL) {
						if (bpt->d_name[0] != '.') {
							process_dir_ent(
							    bpt->d_name,
							    i, last_snm,
							    &dlist[i],
							    ptoi);
						}
					}
				}
				(void) closedir(dskp);
			}
		} else if (errno != ENOENT) {
			fail(1, "stat of %s failed\n", dlist[i].name);
		}
	}
}

/*
 * Walk the list of located devices and see if we've
 * seen this device before. We look at the short name.
 */
static int
look_up_name(const char *nm, disk_list_t *list)
{
	while (list) {
		if (strcmp(list->dsk, nm) != 0)
			list = list->next;
		else {
			return (1);
		}
	}
	return (0);
}

/*
 * Take a name of the form cNtNdNsN or cNtNdNpN
 * or /dev/dsk/CNtNdNsN or /dev/dsk/cNtNdNpN
 * remove the trailing sN or pN. Simply looking
 * for the first 's' or 'p' doesn't cut it.
 */
static void
do_snm(char *orig, char *shortnm)
{
	char *tmp;
	char *ptmp;
	int done = 0;
	char repl_char = 0;

	tmp = strrchr(orig, 's');
	if (tmp) {
		ptmp = tmp;
		ptmp++;
		done = str_is_digit(ptmp);
	}
	if (done == 0) {
		/*
		 * The string either has no 's' in it
		 * or the stuff trailing the s has a
		 * non-numeric in it. Look to see if
		 * we have an ending 'p' followed by
		 * numerics.
		 */
		tmp = strrchr(orig, 'p');
		if (tmp) {
			ptmp = tmp;
			ptmp++;
			if (str_is_digit(ptmp))
				repl_char = 'p';
			else
				tmp = 0;
		}
	} else {
		repl_char = 's';
	}
	if (tmp)
		*tmp = '\0';
	(void) strcpy(shortnm, orig);
	if (repl_char)
		*tmp = repl_char;
}

/*
 * Create and insert an entry into the device list.
 */
static disk_list_t *
make_an_entry(char *lname, char *shortnm, char *longnm,
	dir_info_t *drent, int devtype, ldinfo_t *ptoi)
{
	disk_list_t	*entry;
	char	*nlnm;
	char 	snm[NAME_BUFLEN];
	ldinfo_t *p;

	safe_alloc((void **)&entry, sizeof (disk_list_t), 0);
	nlnm = trim(lname, drent->trimstr, drent->trimchr);
	safe_strdup(shortnm, &entry->dsk);
	do_snm(longnm, snm);
	safe_strdup(snm, &entry->dname);
	entry->devtype = devtype;
	if ((p = find_ldinfo_match(nlnm, ptoi))) {
		entry->dnum = p->dnum;
		safe_strdup(p->dtype, &entry->dtype);
	} else {
		safe_strdup(drent->dtype, &entry->dtype);
		entry->dnum = -1;
		if (drent->dtype) {
			if (strcmp(drent->dtype, "md") == 0) {
				(void) sscanf(shortnm, "d%d", &entry->dnum);
			}
		}
	}
	entry->seen = 0;
	entry->next = 0;
	insert_dlist_ent(entry, &drent->list);
	return (entry);
}

/*
 * slice stuff off beginning and end of /devices directory names derived from
 * device links.
 */
static char *
trim(char *fnm, char *lname, int rchr)
{
	char	*ptr;

	while (*lname == *fnm) {
		lname++;
		fnm++;
	}
	if ((ptr = strrchr(fnm, rchr)))
		*ptr = NULL;
	return (fnm);
}

/*
 * Find an entry matching the name passed in
 */
static ldinfo_t *
find_ldinfo_match(char *name, ldinfo_t *ptoi)
{
	if (name) {
		while (ptoi) {
			if (strcmp(ptoi->name, name))
				ptoi = ptoi->next;
			else
				return (ptoi);
		}
	}
	return (NULL);
}

/*
 * Determine if a name is already in the list of disks. If not, insert the
 * name in the list.
 */
static void
insert_dlist_ent(disk_list_t *n, disk_list_t **hd)
{
	disk_list_t *tmp_ptr;

	if (n->dtype != NULL) {
		tmp_ptr = *hd;
		while (tmp_ptr) {
			if (strcmp(n->dsk, tmp_ptr->dsk) != 0)
				tmp_ptr = tmp_ptr->next;
			else
				break;
	    }
	    if (!tmp_ptr) {
			/*
			 * We don't do anything with MD_DISK types here
			 * since they don't have partitions.
			 */
			if (n->devtype == DISK || n->devtype == OSA_DISK) {
				n->flags = SLICES_OK;
#if defined(i386) || defined(__ia64)
				n->flags |= PARTITIONS_OK;
#endif
			} else {
				n->flags = 0;
			}
			/*
			 * Figure out where to insert the name. The list is
			 * ostensibly in sorted order.
			 */
			if (*hd) {
				disk_list_t *follw;
				int	mv;

				tmp_ptr = *hd;

				/*
				 * Look through the list. While the strcmp
				 * value is less than the current value,
				 */
				while (tmp_ptr) {
					if ((mv = strcmp(n->dtype,
					    tmp_ptr->dtype)) < 0) {
						follw = tmp_ptr;
						tmp_ptr = tmp_ptr->next;
					} else
						break;
				}
				if (mv == 0) {
					/*
					 * We're now in the area where the
					 * leading chars of the kstat name
					 * match. We need to insert in numeric
					 * order after that.
					 */
					while (tmp_ptr) {
						if (strcmp(n->dtype,
						    tmp_ptr->dtype) != 0)
							break;
						if (n->dnum > tmp_ptr->dnum) {
							follw = tmp_ptr;
							tmp_ptr = tmp_ptr->next;
						} else
							break;
					}
				}
				/*
				 * We should now be ready to insert an
				 * entry...
				 */
				if (mv >= 0) {
					if (tmp_ptr == *hd) {
						n->next = tmp_ptr;
						*hd = n;
					} else {
						n->next = follw->next;
						follw->next = n;
					}
				} else {
					/*
					 * insert at the end of the
					 * list
					 */
					follw->next = n;
					n->next = 0;
				}
			} else {
				*hd = n;
				n->next = 0;
			}
		}
	}
}

/*
 * find an entry matching the given kstat name in the list
 * of disks, tapes and metadevices.
 */
disk_list_t *
lookup_ks_name(char *dev_nm, dir_info_t *dl)
{
	int	dv;
	int	len;
	char	cmpbuf[PATH_MAX + 1];
	struct	list_of_disks *list;
	char	*nm;
	dev_name_t *tmp;
	uint_t i;

	/*
	 * extract the device type from the kstat name. We expect the
	 * name to be one or more alphabetics followed by the device
	 * numeric id. We do this solely for speed purposes .
	 */
	len = 0;
	nm = dev_nm;
	while (*nm) {
		if (isalpha(*nm)) {
			nm++;
			len++;
		} else
			break;
	}
	if (*nm) {
		/*
		 * For each of the elements in the dlist array we keep
		 * an array of pointers to chains for each of the kstat
		 * prefixes found within that directory. This is typically
		 * 'sd' and 'ssd'. We walk the list in the directory and
		 * match on that type. Since the same prefixes can be
		 * in multiple places we keep checking if we don't find
		 * it in the first place.
		 */

		(void) strncpy(cmpbuf, dev_nm, len);
		cmpbuf[len] = NULL;
		dv = atoi(nm);
		for (i = 0; i < MAX_TYPES; i++) {
			tmp = dl[i].nf;
			while (tmp) {
				if (strcmp(tmp->name, cmpbuf) == 0) {
					/*
					 * As an optimization we keep mins
					 * and maxes for the devices found.
					 * This helps chop the lists up and
					 * avoid some really long chains as
					 * we would get if we kept only prefix
					 * lists.
					 */
					if (dv >= tmp->min && dv <= tmp->max) {
						list = tmp->list_start;
						while (list) {
							if (list->dnum < dv)
								list =
								    list->next;
							else
								break;
						}
						if (list && list->dnum == dv) {
							return (list);
						}
					}
				}
				tmp = tmp->next;
			}
		}
	}
	return (0);
}

static int
str_is_digit(char *str)
{
	while (*str) {
		if (isdigit(*str))
		    str++;
		else
		    return (0);
	}
	return (1);
}

static void
insert_into_dlist(dir_info_t *d, disk_list_t *e)
{
	dev_name_t *tmp;

	tmp = d->nf;
	while (tmp) {
		if (strcmp(e->dtype, tmp->name) != 0) {
			tmp = tmp->next;
		} else {
			if (e->dnum < tmp->min) {
				tmp->min = e->dnum;
				tmp->list_start = e;
			} else if (e->dnum > tmp->max) {
				tmp->max = e->dnum;
				tmp->list_end = e;
			}
			break;
		}
	}
	if (tmp == 0) {
		safe_alloc((void **)&tmp, sizeof (dev_name_t), 0);
		tmp->name = e->dtype;
		tmp->min = e->dnum;
		tmp->max = e->dnum;
		tmp->list_start = e;
		tmp->list_end = e;
		tmp->next = d->nf;
		d->nf = tmp;
	}
}

/*
 * devinfo_ident_disks() and devinfo_ident_tapes() are the callback functions we
 * use while walking the device tree snapshot provided by devinfo.  If
 * devinfo_ident_disks() identifies that the device being considered has one or
 * more minor nodes _and_ is a block device, then it is a potential disk.
 * Similarly for devinfo_ident_tapes(), except that the second criterion is that
 * the minor_node be a character device.  (This is more inclusive than only
 * tape devices, but will match any entries in /dev/rmt/.)
 *
 * Note: if a driver was previously loaded but is now unloaded, the kstat may
 * still be around (e.g., st) but no information will be found in the
 * libdevinfo tree.
 */

static int
devinfo_ident_disks(di_node_t node, void *arg)
{
	di_minor_t minor = DI_MINOR_NIL;

	if ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
		int spectype = di_minor_spectype(minor);

		if (S_ISBLK(spectype)) {
			char *physical_path = di_devfs_path(node);
			int instance = di_instance(node);
			char *binding_name = di_binding_name(node);

			pline(physical_path, instance, binding_name, arg);
			di_devfs_path_free(physical_path);
		}
	}
	return (DI_WALK_CONTINUE);
}

static int
devinfo_ident_tapes(di_node_t node, void *arg)
{
	di_minor_t minor = DI_MINOR_NIL;

	if ((minor = di_minor_next(node, minor)) != DI_MINOR_NIL) {
		int spectype = di_minor_spectype(minor);

		if (S_ISCHR(spectype)) {
			char *physical_path = di_devfs_path(node);
			int instance = di_instance(node);
			char *binding_name = di_binding_name(node);

			pline(physical_path, instance, binding_name, arg);
			di_devfs_path_free(physical_path);
		}
	}
	return (DI_WALK_CONTINUE);
}

/*
 * rummage_devinfo() is the driver routine that walks the devinfo snapshot.
 */
static ldinfo_t *
rummage_devinfo(void)
{
	di_node_t root_node;
	ldinfo_t *rv = NULL;

	if ((root_node = di_init("/", DINFOCPYALL)) != DI_NODE_NIL) {
		(void) di_walk_node(root_node, DI_WALK_CLDFIRST, (void *)&rv,
			devinfo_ident_disks);
		(void) di_walk_node(root_node, DI_WALK_CLDFIRST, (void *)&rv,
			devinfo_ident_tapes);
		di_fini(root_node);
	}
	return (rv);
}

/*
 * pline() performs the lookup of the device path in the current list of disks,
 * and adds the appropriate information to the nms list in the case of a match.
 */
static void
pline(char *devfs_path, int instance, char *driver_name, ldinfo_t **list)
{
	ldinfo_t *entry;

	safe_alloc((void **)&entry, sizeof (ldinfo_t), 0);
	entry->dnum = instance;
	safe_strdup(devfs_path, &entry->name);
	safe_strdup(driver_name, &entry->dtype);
	entry->next = *list;
	*list = entry;
}

/*
 * Cleanup space allocated in dlist processing.
 * We're only interested in cleaning up the list and nf
 * fields in the structure. Everything else is static
 * data.
 */
static void
cleanup_dlist(dir_info_t *d)
{
	dev_name_t *tmp;
	dev_name_t *t1;
	disk_list_t *t2;
	disk_list_t *t3;

	/*
	 * All of the entries in a dev_name_t use information
	 * from a disk_list_t structure that is freed later.
	 * All we need do here is free the dev_name_t
	 * structure itself.
	 */
	tmp = d->nf;
	while (tmp) {
		t1 = tmp->next;
		free(tmp);
		tmp = t1;
	}
	d->nf = 0;
	/*
	 * "Later". Free the disk_list_t structures and their
	 * data attached to this portion of the dir_info
	 * structure.
	 */
	t2 = d->list;
	while (t2) {
		if (t2->dtype) {
			free(t2->dtype);
			t2->dtype = NULL;
		}
		if (t2->dsk) {
			free(t2->dsk);
			t2->dsk = NULL;
		}
		if (t2->dname) {
			free(t2->dname);
			t2->dname = NULL;
		}
		t3 = t2->next;
		free(t2);
		t2 = t3;
	}
	d->list = 0;
}

static void
process_dir_ent(char *dent, int curr_type, char *last_snm,
    dir_info_t *dp, ldinfo_t *ptoi)
{
	struct stat	sbuf;
	char	dnmbuf[PATH_MAX + 1];
	char	lnm[NAME_BUFLEN];
	char	snm[NAME_BUFLEN];
	char	*npt;

	snm[0] = NULL;
	if (curr_type == DISK || curr_type == OSA_DISK) {
		/*
		 * get the short name - omitting
		 * the trailing sN or PN
		 */
		(void) strcpy(lnm, dent);
		do_snm(dent, snm);
	} else if (curr_type == MD_DISK) {
		(void) strcpy(lnm, dent);
		(void) strcpy(snm, dent);
	} else {
		/*
		 * don't want all rewind/etc
		 * devices for a tape
		 */
		if (!str_is_digit(dent))
			return;
		(void) snprintf(snm, sizeof (snm), "rmt/%s", dent);
		(void) snprintf(lnm, sizeof (snm), "rmt/%s", dent);
	}
	/*
	 * See if we've already processed an entry for this device.
	 * If so, we're just another partition so we get another
	 * entry.
	 *
	 * last_snm is an optimization to avoid the function call
	 * and lookup since we'll often see partition records
	 * immediately after the disk record.
	 */
	if (dp->skip_lookup == 0) {
		if (strcmp(snm, last_snm) != 0) {
			/*
			 * a zero return means that
			 * no record was found. We'd
			 * return a pointer otherwise.
			 */
			if (look_up_name(snm,
				dp->list) == 0) {
				(void) strcpy(last_snm, snm);
			} else
				return;
		} else
			return;
	}
	/*
	 * Get the real device name for this beast
	 * by following the link into /devices.
	 */
	(void) snprintf(dnmbuf, sizeof (dnmbuf), "%s/%s", dp->name, dent);
	if (lstat(dnmbuf, &sbuf) != -1) {
		if ((sbuf.st_mode & S_IFMT) == S_IFLNK) {
			/*
			 * It's a link. Get the real name.
			 */
			char	nmbuf[PATH_MAX + 1];
			int	nbyr;

			if ((nbyr = readlink(dnmbuf, nmbuf,
			    sizeof (nmbuf))) != 1) {
				npt = nmbuf;
				/*
				 * readlink does not terminate
				 * the string so we have to
				 * do it.
				 */
				nmbuf[nbyr] = NULL;
			} else
				npt = NULL;
		} else
			npt = lnm;
		/*
		 * make an entry in the device list
		 */
		if (npt) {
			disk_list_t *d;

			d = make_an_entry(npt, snm,
			    dnmbuf, dp,
			    curr_type, ptoi);
			insert_into_dlist(dp, d);
		}
	}
}
static void
cleanup_ldinfo(ldinfo_t *list)
{
	ldinfo_t *tmp;
	while (list) {
		tmp = list;
		list = list->next;
		free(tmp->name);
		free(tmp->dtype);
		free(tmp);
	}
}
