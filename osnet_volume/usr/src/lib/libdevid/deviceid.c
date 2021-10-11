/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#ident   "@(#)deviceid.c 1.15     99/06/04 SMI"

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ftw.h>
#include <string.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/modctl.h>
#include "libdevid.h"

/*
 * System call prototype
 */
int modctl(int, ...);

/*
 * Get Device Id
 */
int
devid_get(int fd, ddi_devid_t *devid)
{
	int		len = 0;
	dev_t		dev;
	struct stat	stat;
	ddi_devid_t	mydevid;

	if (fstat(fd, &stat) != 0)
		return (-1);

	/* If not char or block device, then error */
	if (!S_ISCHR(stat.st_mode) && !S_ISBLK(stat.st_mode))
		return (-1);

	/* Get the device id size */
	dev = stat.st_rdev;
	if (modctl(MODSIZEOF_DEVID, dev, &len) != 0)
		return (-1);

	/* Allocate space to return device id */
	if ((mydevid = (ddi_devid_t)malloc(len)) == NULL)
		return (-1);

	/* Get the device id */
	if (modctl(MODGETDEVID, dev, len, mydevid) != 0) {
		free(mydevid);
		return (-1);
	}

	/* Return the device id copy */
	*devid = mydevid;
	return (0);
}

/*
 * Free a Device Id
 */
void
devid_free(ddi_devid_t devid)
{
	free(devid);
}

/*
 * Get the minor name
 */
int
devid_get_minor_name(int fd, char **minor_name)
{
	int		len = 0;
	dev_t		dev;
	int		spectype;
	char		*myminorname;
	struct stat	stat;

	if (fstat(fd, &stat) != 0)
		return (-1);

	/* If not a char or block device, then return an error */
	if (!S_ISCHR(stat.st_mode) && !S_ISBLK(stat.st_mode))
		return (-1);

	spectype = stat.st_mode & S_IFMT;
	dev = stat.st_rdev;

	/* Get the minor name size */
	if (modctl(MODSIZEOF_MINORNAME, dev, spectype, &len) != 0)
		return (-1);

	/* Allocate space for the minor name */
	if ((myminorname = (char *)malloc(len)) == NULL)
		return (-1);

	/* Get the minor name */
	if (modctl(MODGETMINORNAME, dev, spectype, len, myminorname) != 0) {
		free(myminorname);
		return (-1);
	}

	/* return the minor name copy */
	*minor_name = myminorname;
	return (0);
}

/*
 * Return the sizeof a device id
 */
size_t
devid_sizeof(ddi_devid_t devid)
{
	impl_devid_t	*id = (impl_devid_t *)devid;

	return (sizeof (*id) + DEVID_GETLEN(id) - sizeof (char));
}

/*
 * Compare two device id's.
 *	-1 - less than
 *	0  - equal
 * 	1  - greater than
 */
int
devid_compare(ddi_devid_t id1, ddi_devid_t id2)
{
	u_char		*cp1 = (u_char *)id1;
	u_char		*cp2 = (u_char *)id2;
	size_t		len1 = devid_sizeof(id1);
	u_int		i;
	impl_devid_t	*id = (impl_devid_t *)id1;
	int		skip_offset;

	/*
	 * The driver name is not a part of the equality
	 */
	skip_offset = ((uintptr_t)&id->did_driver) - (uintptr_t)id;

	/*
	 * The length is part if the ddi_devid_t,
	 * so if they are different sized ddi_devid_t's then
	 * the loop will stop before we run off the end of
	 * one of the device id's.
	 */
	for (i = 0; i < len1; i++) {
		int diff;

		if (i == skip_offset) {
			/* Skip over the hint and continue */
			i += DEVID_HINT_SIZE;
			if (i >= len1)
				break;
		}
		diff = cp1[i] - cp2[i];

		if (diff < 0)
			return (-1);
		if (diff > 0)
			return (1);
	}
	return (0);
}

/*
 * Globals for file tree walk (ftw), since ftw
 * does not allow arguments to be passed in.
 * NOTE: The ftw code is single-threaded with
 *	 the devid_mx mutex.
 */
static mutex_t		devid_mx = DEFAULTMUTEX;
static ddi_devid_t	devid_wanted;
static char		*minorname_wanted;
static int		list_nitems = 0;
static devid_nmlist_t	*list_returned = NULL;

/*
 * matching tuple - returns 0 if the device passed in
 * has the same device id/minor name tuple.
 */
static int
matching_tuple(const char *devname, ddi_devid_t devid, char *minor_name)
{
	int		fd;
	ddi_devid_t	this_devid;
	char		*this_minorname;
	int		rval = -1;

	if ((fd = open(devname, O_RDONLY|O_NDELAY)) < 0)
		return (-1);

	if (devid_get(fd, &this_devid) != 0) {
		(void) close(fd);
		return (-1);
	}

	if (devid_compare(this_devid, devid) != 0) {
		devid_free(this_devid);
		(void) close(fd);
		return (-1);
	}

	/* Free this device id copy */
	devid_free(this_devid);

	if (devid_get_minor_name(fd, &this_minorname) != 0) {
		(void) close(fd);
		return (-1);
	}

	(void) close(fd);

	/* If the minor name matches as well, then we have a matching tuple */
	if (strcmp(this_minorname, minor_name) == 0)
		rval = 0;

	/* Free this minor name copy */
	free(this_minorname);
	return (rval);
}

/*
 * Add and entry to the global array
 */
static int
add_list_item(const char *path, dev_t dev)
{
	if (list_returned == NULL)
		list_nitems = 2; /* one extra for null termination */
	else
		list_nitems++;

	/* Make the array larger */
	list_returned = realloc(list_returned,
	    sizeof (devid_nmlist_t) * list_nitems);

	if (list_returned == NULL)
		return (-1);

	/* Tack entry on the end */
	list_returned[list_nitems-2].devname = strdup(path);
	list_returned[list_nitems-2].dev = dev;
	list_returned[list_nitems-1].devname = NULL;
	list_returned[list_nitems-1].dev = NODEV;
	return (0);
}

/*
 * File tree walk helper function
 */
static int
map_ftw_devid_list(
	const char 		*path,
	const struct stat	*statp,
	int			type)
{
	/* Ignore non-files (e.g., directories) */
	if (type != FTW_F)
		return (0);

	/* If non char or block device, then ignore */
	if (!S_ISCHR(statp->st_mode) && !S_ISBLK(statp->st_mode))
		return (0);

	/* If matching tuple, add to list */
	if (matching_tuple(path, devid_wanted, minorname_wanted) == 0)
		if (add_list_item(path, statp->st_rdev) != 0)
			return (-1);
	return (0);
}

/*
 * Device id to name list
 */
int
devid_deviceid_to_nmlist(
	char		*search_path,
	ddi_devid_t	devid,
	char		*minor_name,
	devid_nmlist_t	**retlist)
{
	int	retval = -1;

	/* Single-Thread - Since ftw forces use of globals */
	mutex_lock(&devid_mx);

	/* Initialize globals */
	devid_wanted = devid;
	minorname_wanted = minor_name;
	list_nitems = 0;
	list_returned = NULL;

	/* Do the file tree walk */
	if (ftw(search_path, map_ftw_devid_list, 5) == 0) {
		if (retlist != NULL)
			*retlist = list_returned;
		/* If we got a list, then success */
		if (list_returned != NULL)
			retval = 0;
		else
			errno = EINVAL;
	}
	list_nitems = 0;
	list_returned = NULL;
	if (retval == 0)
		errno = 0;
	mutex_unlock(&devid_mx);
	return (retval);
}

/*
 * Free Device Id Name List
 */
void
devid_free_nmlist(devid_nmlist_t *list)
{
	devid_nmlist_t *p = list;

	if (list == NULL)
		return;

	/* Free all the device names */
	while (p->devname != NULL) {
		free(p->devname);
		p++;
	}

	/* Free the array */
	free(list);
}
