/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)map.c	1.42	99/09/02 SMI"

/*LINTLIBRARY*/

/*
 * I18N message number ranges
 *  This file: 12000 - 12499
 *  Shared common messages: 1 - 1999
 */

/*
 *	This module is part of the Fibre Channel Interface library.
 */

/* #define		_POSIX_SOURCE 1 */


/*	Includes	*/
#include	<stdlib.h>
#include	<stdio.h>
#include	<sys/file.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mkdev.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<string.h>
#include	<sys/scsi/scsi.h>
#include	<dirent.h>		/* for DIR */
#include	<sys/vtoc.h>
#include	<nl_types.h>
#include	<strings.h>
#include	<errno.h>
#include	<sys/ddi.h>		/* for max */
#include	<fnmatch.h>
#include	<l_common.h>
#include	<stgcom.h>
#include	<l_error.h>
#include	<g_state.h>
#include	<sys/fibre-channel/ulp/fcp_util.h>
#include	<sys/fibre-channel/impl/fc_error.h>
#include	<sys/socalio.h>

/* Some forward declarations of static functions */
static int	g_get_inq_dtype(int, la_wwn_t, uchar_t *);
static int	g_get_dev_list(int, fc_port_dev_t **, int *, int);
static int	g_issue_fcp_ioctl(int, struct fcp_ioctl *, int);
static int	g_set_port_state(char *, int);

/*	Defines 	*/
#define	VERBPRINT	if (verbose) (void) printf

uchar_t g_switch_to_alpa[] = {
	0xef, 0xe8, 0xe4, 0xe2, 0xe1, 0xe0, 0xdc, 0xda, 0xd9, 0xd6,
	0xd5, 0xd4, 0xd3, 0xd2, 0xd1, 0xce, 0xcd, 0xcc, 0xcb, 0xca,
	0xc9, 0xc7, 0xc6, 0xc5, 0xc3, 0xbc, 0xba, 0xb9, 0xb6, 0xb5,
	0xb4, 0xb3, 0xb2, 0xb1, 0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9,
	0xa7, 0xa6, 0xa5, 0xa3, 0x9f, 0x9e, 0x9d, 0x9b, 0x98, 0x97,
	0x90, 0x8f, 0x88, 0x84, 0x82, 0x81, 0x80, 0x7c, 0x7a, 0x79,
	0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x6e, 0x6d, 0x6c, 0x6b,
	0x6a, 0x69, 0x67, 0x66, 0x65, 0x63, 0x5c, 0x5a, 0x59, 0x56,
	0x55, 0x54, 0x53, 0x52, 0x51, 0x4e, 0x4d, 0x4c, 0x4b, 0x4a,
	0x49, 0x47, 0x46, 0x45, 0x43, 0x3c, 0x3a, 0x39, 0x36, 0x35,
	0x34, 0x33, 0x32, 0x31, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29,
	0x27, 0x26, 0x25, 0x23, 0x1f, 0x1e, 0x1d, 0x1b, 0x18, 0x17,
	0x10, 0x0f, 0x08, 0x04, 0x02, 0x01
};

uchar_t g_sf_alpa_to_switch[] = {
	0x00, 0x7d, 0x7c, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x7a, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x79, 0x78, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x77, 0x76, 0x00, 0x00, 0x75, 0x00, 0x74,
	0x73, 0x72, 0x00, 0x00, 0x00, 0x71, 0x00, 0x70, 0x6f, 0x6e,
	0x00, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x00, 0x00, 0x67,
	0x66, 0x65, 0x64, 0x63, 0x62, 0x00, 0x00, 0x61, 0x60, 0x00,
	0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5e, 0x00, 0x5d,
	0x5c, 0x5b, 0x00, 0x5a, 0x59, 0x58, 0x57, 0x56, 0x55, 0x00,
	0x00, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x00, 0x00, 0x4e,
	0x4d, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4b,
	0x00, 0x4a, 0x49, 0x48, 0x00, 0x47, 0x46, 0x45, 0x44, 0x43,
	0x42, 0x00, 0x00, 0x41, 0x40, 0x3f, 0x3e, 0x3d, 0x3c, 0x00,
	0x00, 0x3b, 0x3a, 0x00, 0x39, 0x00, 0x00, 0x00, 0x38, 0x37,
	0x36, 0x00, 0x35, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x33, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x31, 0x30, 0x00, 0x00, 0x2f, 0x00, 0x2e, 0x2d, 0x2c,
	0x00, 0x00, 0x00, 0x2b, 0x00, 0x2a, 0x29, 0x28, 0x00, 0x27,
	0x26, 0x25, 0x24, 0x23, 0x22, 0x00, 0x00, 0x21, 0x20, 0x1f,
	0x1e, 0x1d, 0x1c, 0x00, 0x00, 0x1b, 0x1a, 0x00, 0x19, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x17, 0x16, 0x15,
	0x00, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0f, 0x00, 0x00, 0x0e,
	0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x00, 0x00, 0x08, 0x07, 0x00,
	0x06, 0x00, 0x00, 0x00, 0x05, 0x04, 0x03, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



/*
 * Check if device is in the map.
 *
 * PARAMS:
 *	map - loop map returned from sf
 *	tid - device ID
 *
 * RETURNS:
 *	 1 if device present in the map.
 *	 0 otherwise.
 */
int
g_device_in_map(sf_al_map_t *map, int tid)
{
	int i, j;

	for (i = 0; i < map->sf_count; i++) {
		if (map->sf_addr_pair[i].sf_al_pa ==
			(int)g_switch_to_alpa[tid]) {
			/* Does not count if WWN == 0 */
			for (j = 0; j < WWN_SIZE; j++)
				if (map->sf_addr_pair[i].sf_port_wwn[j] != 0)
					return (1);
		}
	}
	return (0);
}


/*
 * Create a linked list of all the WWN's for all FC_AL disks that
 * are attached to this host.
 *
 * RETURN VALUES: 0 O.K.
 *
 * wwn_list pointer:
 *			NULL: No devices found.
 *			!NULL: Devices found
 *                      wwn_list points to a linked list of wwn's.
 */
int
g_get_wwn_list(struct wwn_list_struct **wwn_list_ptr, int verbose)
{
char		*dev_name;
DIR		*dirp;
struct dirent	*entp;
char		namebuf[MAXPATHLEN];
char		disk_path[MAXPATHLEN];
char		last_disk_path[MAXPATHLEN];
struct stat	sb;
char		*result = NULL;
WWN_list	*wwn_list, *l1, *l2;
char		*char_ptr;
uchar_t		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa;
hrtime_t	start_time, end_time;


	start_time = gethrtime();


	if (verbose) {
		(void) fprintf(stdout,
			MSGSTR(12000,
			"  Building list of WWN's of all FC_AL disks"
			" attached to the system.\n"));
	}
	P_DPRINTF("  g_get_wwn_list: Building list of WWN's of all "
		"FC_AL disks attached to the system\n");

	wwn_list = *wwn_list_ptr = NULL;
	if ((dev_name = (char *)g_zalloc(sizeof ("/dev/rdsk"))) == NULL) {
		return (L_MALLOC_FAILED);
	}
	(void) 	sprintf(dev_name, "/dev/rdsk");

	if (verbose) {
		(void) fprintf(stdout,
			MSGSTR(12001,
			"  Searching directory %s for links to devices\n"),
			dev_name);
	}
	if ((dirp = opendir(dev_name)) == NULL) {
		(void) g_destroy_data(dev_name);
		P_DPRINTF("  g_get_wwn_list: No disks found\n");
		return (L_NO_DISK_DEV_FOUND);
	}

	last_disk_path[0] = '\0';
	while ((entp = readdir(dirp)) != NULL) {
		/*
		 * Ignore current directory and parent directory
		 * entries. In addition, note that this directory has
		 * 8 links for each disk device - one for each
		 * partition. To get the WWN for the device, we need
		 * to look at only one of these partitions. Therefore,
		 * we ignore all entries except those terminating with
		 * "s2" - the partition referring to the entire disk.
		 */
		if ((strcmp(entp->d_name, ".") == 0) ||
		    (strcmp(entp->d_name, "..") == 0) ||
		    (fnmatch("*s2", entp->d_name, 0) != 0))
			continue;

		(void) memset(namebuf, 0, MAXPATHLEN);
		(void) sprintf(namebuf, "%s/%s",
				dev_name, entp->d_name);

		if ((lstat(namebuf, &sb)) < 0) {
			ER_DPRINTF("Warning: Cannot stat %s\n",
					namebuf);
			continue;
		}

		if (!S_ISLNK(sb.st_mode)) {
			ER_DPRINTF("Warning: %s is not a symbolic link\n",
					namebuf);
			continue;
		}
		if ((result = g_get_physical_name_from_link(namebuf)) == NULL) {
			ER_DPRINTF("  Warning: Get physical name from"
				" link failed. Link=%s\n", namebuf);
			continue;
		}

		/* Found a disk. */
		(void) strcpy(disk_path, result);
		if ((char_ptr = strrchr(disk_path, ':')) == NULL) {
			g_destroy_data(result);
			continue;	/* Skip error */
		}
		*char_ptr = '\0';	/* Strip partition information */

		if (strcmp(disk_path, last_disk_path) == 0) {
			g_destroy_data(result);
			continue;	/* skip other partitions */
		}
		(void) strcpy(last_disk_path, disk_path);

		if (g_get_wwn(result, port_wwn, node_wwn,
			&al_pa, verbose)) {
			g_destroy_data(result);
			continue;	/* Skip error */
		}

		/*
		 * Add information to the list.
		 */
		if ((l2 = (struct  wwn_list_struct *)
			g_zalloc(sizeof (struct  wwn_list_struct)))
			== NULL) {
			g_destroy_data(result);
			g_destroy_data(dev_name);
			return (L_MALLOC_FAILED);
		}
		if ((l2->physical_path = (char *)
			g_zalloc(strlen(result) +1)) == NULL) {
			g_destroy_data(result);
			g_destroy_data(dev_name);
			return (L_MALLOC_FAILED);
		}
		if ((l2->logical_path = (char *)
			g_zalloc(strlen(namebuf) + 1)) == NULL) {
			g_destroy_data(result);
			g_destroy_data(dev_name);
			return (L_MALLOC_FAILED);
		}

		/* Fill in structure */
		bcopy((void *)node_wwn, (void *)l2->w_node_wwn, WWN_SIZE);
		(void) sprintf(l2->port_wwn_s,
		"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);
		(void) sprintf(l2->node_wwn_s,
		"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			node_wwn[0], node_wwn[1], node_wwn[2], node_wwn[3],
			node_wwn[4], node_wwn[5], node_wwn[6], node_wwn[7]);

		(void) strcpy(l2->physical_path, result);
		(void) strcpy(l2->logical_path, namebuf);
		l2->device_type = DTYPE_DIRECT;

		P_DPRINTF("  g_get_wwn_list: Found disk: port=%s "
		"Logical path=%s\n", l2->port_wwn_s, l2->logical_path);

		if (wwn_list == NULL) {
			l1 = wwn_list = l2;
		} else {
			l2->wwn_prev = l1;
			l1 = l1->wwn_next = l2;
		}
		g_destroy_data(result);
	}
	(void) g_destroy_data(dev_name);
	(void) closedir(dirp);


	if (getenv("_LUX_T_DEBUG") != NULL) {
		end_time = gethrtime();
		(void) fprintf(stdout,
			"      g_get_wwn_list: "
			"\t\tTime = %lld millisec\n",
			(end_time - start_time)/1000000);
	}

	*wwn_list_ptr = wwn_list; /* pass back ptr to list */
	return (0);
}



void
g_free_wwn_list(struct wwn_list_struct **wwn_list)
{
WWN_list	*next = NULL;

	for (; *wwn_list != NULL; *wwn_list = next) {
		next = (*wwn_list)->wwn_next;
		if ((*wwn_list)->physical_path != NULL)
			(void) g_destroy_data((*wwn_list)->physical_path);
		if ((*wwn_list)->logical_path != NULL)
			(void) g_destroy_data((*wwn_list)->logical_path);
		(void) g_destroy_data(*wwn_list);
	}
	wwn_list = NULL;
}



/*
 * Get the limited map for FC4 devices.
 * This function is specific to FC4
 * devices and doesn't work for FC (leadville) devices.
 *
 * RETURN VALUES:
 *	0	 O.K.
 *	non-zero otherwise
 *
 * lilpmap *map_ptr:
 *		NULL: No devices found
 *		!NULL: if devices found
 */
int
g_get_limited_map(char *path, struct lilpmap *map_ptr, int verbose)
{
int	fd, i;
char	drvr_path[MAXPATHLEN];
struct	stat	stbuf;


	/* initialize map */
	(void) memset(map_ptr, 0, sizeof (struct lilpmap));

	(void) strcpy(drvr_path, path);
	/*
	 * Get the path to the :devctl driver
	 *
	 * This assumes the path looks something like this:
	 * /devices/sbus@1f,0/SUNW,socal@1,0:1
	 * or
	 * /devices/sbus@1f,0/SUNW,socal@1,0
	 * or
	 * a 1 level PCI type driver
	 */
	if (stat(drvr_path, &stbuf) < 0) {
		return (L_LSTAT_ERROR);
	}
	if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
		/* append a port. Just try 0 since they did not give us one */
		(void) strcat(drvr_path, ":0");
	}

	P_DPRINTF("  g_get_limited_map: Geting drive map from:"
		" %s\n", drvr_path);

	/* open controller */
	if ((fd = g_object_open(drvr_path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);

	if (ioctl(fd, FCIO_GETMAP, map_ptr) != 0) {
		I_DPRINTF("  FCIO_GETMAP ioctl failed\n");
		(void) close(fd);
		return (L_FCIO_GETMAP_IOCTL_FAIL);
	}
	(void) close(fd);

	/*
	 * Check for reasonableness.
	 */
	if ((map_ptr->lilp_length > 126) || (map_ptr->lilp_magic != 0x1107)) {
		return (L_INVALID_LOOP_MAP);
	}
	for (i = 0; i < (uint_t)map_ptr->lilp_length; i++) {
		if ((map_ptr->lilp_list[i] > 0xef) ||
		    (map_ptr->lilp_list[i] < (uchar_t)0x0)) {
			return (L_INVALID_LOOP_MAP);
		}
	}

	return (0);
}


/*
 * For leadville specific HBA's ONLY.
 * Get the host specific parameters,
 * al_pa, hard address, node/port WWN etc.
 *
 * OUTPUT:
 *	fc_port_dev_t structure.
 *
 * RETURNS:
 *	0	if  OK
 *	non-zero in case of error.
 */
int
g_get_host_params(int fd, fc_port_dev_t *host_val, int verbose)
{
int		err;
fcio_t		fcio;

	/* initialize structure */
	(void) memset(host_val, 0, sizeof (struct fc_port_dev));

	fcio.fcio_cmd = FCIO_GET_HOST_PARAMS;
	fcio.fcio_xfer = FCIO_XFER_READ;
	fcio.fcio_obuf = (caddr_t)host_val;
	fcio.fcio_olen = sizeof (fc_port_dev_t);

	if (g_issue_fcio_ioctl(fd, &fcio, verbose) != 0) {
		I_DPRINTF(" FCIO_GET_HOST_PARAMS ioctl failed.\n");
		return (L_FCIO_GET_HOST_PARAMS_FAIL);
	}

	/* get the inquiry information for the leadville HBA. */
	if ((err = g_get_inq_dtype(fd, host_val->dev_pwwn,
				&host_val->dev_dtype)) != 0) {
		return (err);
	}
	return (0);
}



/*
 * Issue FCIO ioctls to the port(fp) driver.
 * FCIO ioctl needs to be retried when it
 * is returned with an EINVAL error, wait
 * time between retries should be atleast
 * MAX_WAIT_TIME (too much of a time to wait!!)
 * XXX: Need to investigate further and find out
 * the best wait time.
 *
 * OUTPUT:
 *	fcio_t structure
 *
 * RETURNS:
 *	0	 if O.K.
 *	non-zero otherwise.
 */
int
g_issue_fcio_ioctl(int fd, fcio_t *fcio, int verbose)
{
int	ntries;

	for (ntries = 0; ntries < MAX_RETRIES; ntries++) {
		if (ioctl(fd, FCIO_CMD, fcio) != 0) {
			if ((errno == EAGAIN) &&
				(ntries+1 < MAX_RETRIES)) {
				/* wait MAX_WAIT_TIME */
				(void) sleep(MAX_WAIT_TIME);
				continue;
			}
			I_DPRINTF("FCIO ioctl failed.\n"
				"Error: %s. fc_error = %d (0x%x)\n",
			strerror(errno), fcio->fcio_errno, fcio->fcio_errno);
			if (errno == EINVAL) {
				return (errno);
			}
			/*
			 * When port is offlined, usoc
			 * returns the FC_OFFLINE error and errno
			 * is set to EIO.
			 * We do want to ignore this error,
			 * especially when an enclosure is
			 * removed from the loop.
			 */
			if (fcio->fcio_errno == FC_OFFLINE)
				break;
			return (-1);
		}
		break;
	}

	return (0);
}

/*
 * This function issues the FCP_TGT_INQUIRY ioctl to
 * the fcp module
 *
 * OUTPUT:
 *	fcp_ioctl structure in fcp_data is filled in by fcp
 *
 * RETURN VALUES :
 *	0 on Success
 *	Non-zero otherwise
 */
static int
g_issue_fcp_ioctl(int fd, struct fcp_ioctl *fcp_data, int verbose)
{
int num_tries = 0;

	/*
	 * Issue the ioctl to FCP
	 * The retries are required because the driver may
	 * need some time to respond at times.
	 */
	while ((num_tries++ < MAX_RETRIES) &&
		(ioctl(fd, FCP_TGT_INQUIRY, fcp_data) == -1)) {
		if ((errno != EAGAIN) || (num_tries == MAX_RETRIES)) {
			return (L_FCP_TGT_INQUIRY_FAIL);
		}
		if (errno == EAGAIN) {
			(void) sleep(MAX_WAIT_TIME);
		}
	}
	return (0);
}

/*
 * Get the number of devices and also
 * a list of devices accessible through
 * the device's port as specified by its
 * file descriptor (fd). The calling function
 * is responsible for freeing the dev_list.
 *
 * dev_list:
 *	NULL:	  No devices found, in case of an error
 *	Non-NULL: Devices found.
 * ndevs:
 *	set to the number of devices
 *	accessible through the port.
 *
 * RETURNS:
 *	0	 if O.K.
 *	non-zero otherwise
 */
static int
g_get_dev_list(int fd, fc_port_dev_t **dev_list, int *ndevs, int verbose)
{
int		num_devices;
int		err, new_count = 0;
fcio_t		fcio;
fc_port_dev_t	*dlist;

	*dev_list = dlist = NULL;
	/*
	 * Get the device list from port driver
	 */
	fcio.fcio_cmd = FCIO_GET_NUM_DEVS;
	fcio.fcio_olen = sizeof (num_devices);
	fcio.fcio_xfer = FCIO_XFER_READ;
	fcio.fcio_obuf = (caddr_t)&num_devices;
	if (g_issue_fcio_ioctl(fd, &fcio, verbose) != 0) {
		I_DPRINTF(" FCIO_GET_NUM_DEVS ioctl failed.\n");
		return (L_FCIO_GET_NUM_DEVS_FAIL);
	}
	if (num_devices == 0) {
		return (L_NO_DEVICES_FOUND);
	}

	if ((dlist = (fc_port_dev_t *)calloc(num_devices,
				sizeof (fc_port_dev_t))) == NULL) {
		return (L_MALLOC_FAILED);
	}
	bzero((caddr_t)&fcio, sizeof (fcio));
	/* Get the device list */
	fcio.fcio_cmd = FCIO_GET_DEV_LIST;
	/* Information read operation */
	fcio.fcio_xfer = FCIO_XFER_READ;
	fcio.fcio_olen = num_devices * sizeof (fc_port_dev_t);
	fcio.fcio_obuf = (caddr_t)dlist;
	/* new device count */
	fcio.fcio_alen = sizeof (new_count);
	fcio.fcio_abuf = (caddr_t)&new_count;
	if ((err = g_issue_fcio_ioctl(fd, &fcio, verbose)) != 0) {
		if (err == EINVAL) {
			I_DPRINTF(" Device count was %d"
				" should have been %d\n",
				num_devices, new_count);
			free(dlist);
			return (L_INVALID_DEVICE_COUNT);
		} else {
			I_DPRINTF(" FCIO_GET_DEV_LIST ioctl failed.");
			free(dlist);
			return (L_FCIO_GET_DEV_LIST_FAIL);
		}
	}
	*ndevs = num_devices;
	*dev_list = dlist;
	return (0);
}

/* Constant used by g_get_inq_dtype() */
#define	FCP_PATH	"/devices/pseudo/fcp@0:fcp"

/*
 * Gets the inq_dtype for devices on the fabric FC driver
 * through an ioctl to the FCP module.
 *
 * OUTPUT:
 *	inq_dtype is set to the dtype on success
 *
 * RETURN VALUES:
 *	0 on Success
 *	Non-zero on error
 */
static int
g_get_inq_dtype(int fd, la_wwn_t pwwn, uchar_t *inq_dtype)
{
	int			err, fcp_fd;
	struct fcp_ioctl	fcp_data;
	struct device_data	inq_data;
	struct stat		sbuf;

	if (fstat(fd, &sbuf) == -1) {
		return (L_FSTAT_ERROR);
	}

	if ((fcp_fd = open(FCP_PATH, O_RDONLY)) == -1) {
		return (L_OPEN_PATH_FAIL);
	}

	/* Get the minor number for an fp instance */
	fcp_data.fp_minor = minor(sbuf.st_rdev);

	fcp_data.listlen = 1;
	inq_data.dev_pwwn = pwwn;	/* The port WWN as passed */
	fcp_data.list = (caddr_t)&inq_data;

	if (err = g_issue_fcp_ioctl(fcp_fd, &fcp_data, 0)) {
		close(fcp_fd);
		return (err);
	}
	*inq_dtype = inq_data.dev0_type;
	close(fcp_fd);
	return (0);
}

/*
 * Gets device map from nexus driver
 *
 * PARAMS:
 *	path -	must be the physical path to a device
 *	map  -	loop map returned from sf.
 *	verbose - options.
 *
 * RETURNS:
 *	0	: if OK
 *	non-zero: otherwise
 */
int
g_get_dev_map(char *path, sf_al_map_t *map_ptr, int verbose)
{
int		fd, i, j, num_devices = 0, err;
char		drvr_path[MAXPATHLEN];
char		*char_ptr;
struct stat	stbuf;
fc_port_dev_t	*dev_list, *dlistptr;
uint_t		dev_type;


	/* initialize map */
	(void) memset(map_ptr, 0, sizeof (struct sf_al_map));

	(void) strcpy(drvr_path, path);
	/*
	 * Get the path to the :devctl driver
	 *
	 * This assumes the path looks something like this:
	 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@e,0:0
	 * or
	 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0
	 * or
	 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0:devctl
	 * or
	 * a 1 level PCI type driver but still :devctl
	 * (or "usoc" in the place of "socal" and "fp" for "sf")
	 */
	if (strstr(drvr_path, DRV_NAME_SSD) || strstr(drvr_path, SES_NAME)) {
		if ((char_ptr = strrchr(drvr_path, '/')) == NULL) {
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate sting  */
		/* append controller */
		(void) strcat(drvr_path, FC_CTLR);
	} else {
		if (stat(drvr_path, &stbuf) < 0) {
			return (L_LSTAT_ERROR);
		}
		if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
			/* append controller */
			(void) strcat(drvr_path, FC_CTLR);
		}
	}

	P_DPRINTF("  g_get_dev_map: Geting drive map from:"
		" %s\n", drvr_path);

	/* open controller */
	if ((fd = g_object_open(drvr_path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);

	if ((dev_type = g_get_path_type(drvr_path)) == 0) {
		(void) close(fd);
		return (L_INVALID_PATH);
	}

	/* for FC devices. */
	if (dev_type & FC_FCA_MASK) {
		if ((err = g_get_dev_list(fd, &dev_list,
				&num_devices, verbose)) != 0) {
			(void) close(fd);
			return (err);
		}
		dlistptr = dev_list;
		/* Map the (new->old) structures here */
		map_ptr->sf_count = (short)num_devices;
		/*
		 * XXX: Checking (i < SF_NUM_ENTRIES_IN_MAP) just to
		 * make sure that we don't overrun the map structure
		 * since it can hold data for upto 126 devices.
		 * Its a temporary check for private loops and will
		 * removed once the luxadm is ported to work on fabric devices.
		 */
		for (i = 0; (i < num_devices && i < SF_NUM_ENTRIES_IN_MAP);
							i++, dev_list++) {
			/*
			 * XXX: Out of 24 bits of port_id, copy only 8 bits
			 * to al_pa. This works okay for devices that're
			 * on a private loop. But, all 24 bits are required
			 * to identify a device on a public loop (e.g.device
			 * connected through a switch.
			 */
			map_ptr->sf_addr_pair[i].sf_al_pa =
				(uchar_t)dev_list->dev_did.port_id;
			map_ptr->sf_addr_pair[i].sf_hard_address =
				(uchar_t)dev_list->dev_hard_addr.hard_addr;
			for (j = 0; j < FC_WWN_SIZE; j++) {
				map_ptr->sf_addr_pair[i].sf_node_wwn[j] =
					dev_list->dev_nwwn.raw_wwn[j];
				map_ptr->sf_addr_pair[i].sf_port_wwn[j] =
					dev_list->dev_pwwn.raw_wwn[j];
			}
			/*
			 * Since the sf_inq_dtype is still uninitialized
			 * at this stage, we will get it from the FCP
			 * module.
			 */
			if ((err = g_get_inq_dtype(fd, dev_list->dev_pwwn,
					&dev_list->dev_dtype)) != 0) {
				(void) close(fd);
				(void) free(dlistptr);
				return (err);
			}
			map_ptr->sf_addr_pair[i].sf_inq_dtype =
						dev_list->dev_dtype;
		}
		(void) free(dlistptr);

	} else {	/* sf and fc4/pci devices */
		if (ioctl(fd, SFIOCGMAP, map_ptr) != 0) {
			I_DPRINTF("  SFIOCGMAP ioctl failed.\n");
			(void) close(fd);
			return (L_SFIOCGMAP_IOCTL_FAIL);
		}
		/* Check for reasonableness. */
		if ((map_ptr->sf_count > 126) || (map_ptr->sf_count < 0)) {
			return (L_INVALID_LOOP_MAP);
		}
		for (i = 0; i < map_ptr->sf_count; i++) {
			if ((map_ptr->sf_addr_pair[i].sf_al_pa > 0xef) ||
			(map_ptr->sf_addr_pair[i].sf_al_pa < (uchar_t)0x0)) {
				return (L_INVALID_LOOP_MAP);
			}
		}
	}

	(void) close(fd);
	return (0);
}



/*
 * Read the extended link error status block
 * from the specified device and Host Adapter.
 *
 * PARAMS:
 *	path_phys - physical path to an FC device
 *	rls_ptr   - pointer to read link state structure
 *
 * RETURNS:
 *	0	: if OK
 *	non-zero: otherwise
 */
int
g_rdls(char *path_phys, struct al_rls **rls_ptr, int verbose)
{
char		nexus_path[MAXPATHLEN], *nexus_path_ptr;
int		fd, fp_fd, err, length, exp_map_flag = 0;
struct lilpmap	map;
AL_rls		*rls, *c1 = NULL, *c2 = NULL;
uchar_t		i;
sf_al_map_t	exp_map;
char		*charPtr, fp_path[MAXPATHLEN];
uint_t		dev_type;
struct stat	stbuf;
fcio_t		fcio;
fc_port_dev_t	*dlistptr;
fc_portid_t	rls_req;
fc_rls_acc_t	rls_payload;
fc_port_dev_t	*dev_list;
int		num_devices = 0;

	/* intialize pointers */
	dev_list = NULL;
	*rls_ptr = rls = NULL;
	(void) memset(&map, 0, sizeof (struct lilpmap));

	/* Get map of devices on this loop. */
	if ((dev_type = g_get_path_type(path_phys)) == 0) {
		return (L_INVALID_PATH);
	}
	if (dev_type & FC_FCA_MASK) {
		(void) strcpy(fp_path, path_phys);
		if (strstr(fp_path, DRV_NAME_SSD) ||
				strstr(fp_path, SES_NAME)) {
			if ((charPtr = strrchr(fp_path, '/')) == NULL) {
				return (L_INVALID_PATH);
			}
			*charPtr = '\0';
			/* append devctl to the path */
			(void) strcat(fp_path, FC_CTLR);
		} else {
			if (stat(fp_path, &stbuf) < 0) {
				return (L_LSTAT_ERROR);
			}
			if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
				/* append devctl to the path */
				(void) strcat(fp_path, FC_CTLR);
			}
		}
		if ((fp_fd = open(fp_path, O_RDONLY | O_EXCL)) < 0) {
			return (L_OPEN_PATH_FAIL);
		}

		if ((err = g_get_dev_list(fp_fd, &dev_list,
				&num_devices, verbose)) != 0) {
			(void) close(fp_fd);
			return (err);
		}
		length = num_devices;

		dlistptr = dev_list;

	} else {
		if ((err = g_get_nexus_path(path_phys,
					    &nexus_path_ptr)) != 0) {
			return (err);
		}
		(void) strcpy(nexus_path, nexus_path_ptr);
		g_destroy_data(nexus_path_ptr);

		/* open driver */
		if ((fd = g_object_open(nexus_path,
				O_NDELAY | O_RDONLY)) == -1)
			return (L_OPEN_PATH_FAIL);

		/*
		 * First try using the socal version of the map.
		 * If that fails get the expanded vesion.
		 */
		if (ioctl(fd, FCIO_GETMAP, &map) != 0) {
			I_DPRINTF("  FCIO_GETMAP ioctl failed.\n");
			if (ioctl(fd, SFIOCGMAP, &exp_map) != 0) {
				I_DPRINTF("  SFIOCGMAP ioctl failed.\n");
				(void) close(fd);
				return (L_SFIOCGMAP_IOCTL_FAIL);
			}
			/* Check for reasonableness. */
			if ((exp_map.sf_count > 126) ||
					(exp_map.sf_count < 0)) {
				(void) close(fd);
				return (L_INVALID_LOOP_MAP);
			}
			for (i = 0; i < exp_map.sf_count; i++) {
				if ((exp_map.sf_addr_pair[i].sf_al_pa > 0xef) ||
			(exp_map.sf_addr_pair[i].sf_al_pa < (uchar_t)0x0)) {
					(void) close(fd);
					return (L_INVALID_LOOP_MAP);
				}
			}
			length = exp_map.sf_count;
			exp_map_flag++;
		} else {
			I_DPRINTF("  g_rdls:"
				" FCIO_GETMAP ioctl returned %d entries.\n",
				map.lilp_length);
			/* Check for reasonableness. */
			if (map.lilp_length > sizeof (map.lilp_list)) {
				(void) close(fd);
				return (L_FCIOGETMAP_INVLD_LEN);
			}
			length = map.lilp_length;
		}
	}
	for (i = 0; i < length; i++) {
		if ((c2 = (struct al_rls *)
			g_zalloc(sizeof (struct al_rls))) == NULL) {
			(void) close(fd);
			return (L_MALLOC_FAILED);
		}
		if (rls == NULL) {
			c1 = rls = c2;
		} else {
			for (c1 = rls; c1->next; c1 =  c1->next) {};
			c1 = c1->next = c2;
		}
		if (dev_type & FC4_FCA_MASK) {
			(void) strcpy(c1->driver_path, nexus_path);
			if (exp_map_flag) {
				c1->payload.rls_portno = c1->al_ha =
					exp_map.sf_addr_pair[i].sf_al_pa;
			} else {
				c1->payload.rls_portno = c1->al_ha =
					map.lilp_list[i];
			}
			c1->payload.rls_linkfail =
				(uint_t)0xff000000; /* get LESB for this port */
			I_DPRINTF("  g_rdls:"
				" al_pa 0x%x\n", c1->payload.rls_portno);

			if (ioctl(fd, FCIO_LINKSTATUS, &c1->payload) != 0) {
				/*
				 * The ifp driver will return ENXIO when rls
				 * is issued for same initiator on loop when
				 * there is more than one on the loop.
				 * Rather than completely fail, continue on.
				 * Set values in the payload struct to -1 as
				 * this is what socal is currently doing for
				 * the case of same initiator rls.
				 */
				if ((dev_type & FC4_PCI_FCA) &&
					(errno == ENXIO)) {
					c1->payload.rls_linkfail =
					c1->payload.rls_syncfail =
					c1->payload.rls_sigfail =
					c1->payload.rls_primitiverr =
					c1->payload.rls_invalidword =
					c1->payload.rls_invalidcrc =
						(uint_t)0xffffffff;
				} else {
					I_DPRINTF("  FCIO_LINKSTATUS ioctl"
						" failed.\n");
					(void) close(fd);
					return (L_FCIO_LINKSTATUS_FAILED);
				}
			}
			I_DPRINTF("  g_rdls: al_pa returned by ioctl 0x%x\n",
				c1->payload.rls_portno);
		}
		if (dev_type & FC_FCA_MASK) {
			/*
			 * fp uses different input/output structures for
			 * rls. Load the values returned for the fp ioctl
			 * into the structure passed back to the caller
			 * Note: I do not see the reason for the path
			 * to be loaded into AL_rls as is done for socal/ifp
			 * above.
			 */
			/* Set the al_pa here */
			c1->al_ha = rls_req.port_id = dev_list->dev_did.port_id;
			fcio.fcio_cmd = FCIO_LINK_STATUS;
			fcio.fcio_ibuf = (caddr_t)&rls_req;
			fcio.fcio_ilen = sizeof (rls_req);
			fcio.fcio_xfer = FCIO_XFER_RW;
			fcio.fcio_flags = 0;
			fcio.fcio_cmd_flags = FCIO_CFLAGS_RLS_DEST_NPORT;
			fcio.fcio_obuf = (caddr_t)&rls_payload;
			fcio.fcio_olen = sizeof (rls_payload);

			if (g_issue_fcio_ioctl(fp_fd, &fcio, verbose) != 0) {
				free(dlistptr);
				(void) close(fp_fd);
				return (L_FCIO_GET_LINK_STATUS_FAIL);
			}
			/*
			 * Load the values into the struct passed
			 * back to the caller
			 */
			c1->payload.rls_linkfail =
				rls_payload.rls_link_fail;
			c1->payload.rls_syncfail =
				rls_payload.rls_sync_loss;
			c1->payload.rls_sigfail =
				rls_payload.rls_sig_loss;
			c1->payload.rls_primitiverr =
				rls_payload.rls_prim_seq_err;
			c1->payload.rls_invalidword =
				rls_payload.rls_invalid_word;
			c1->payload.rls_invalidcrc =
				rls_payload.rls_invalid_crc;

			/* Increment the device list pointer */
			if (i+1 < length) {
				dev_list++;
			}
		}
	}
	*rls_ptr = rls;	/* Pass back pointer */

	if (dev_type & FC_FCA_MASK) {
		(void) close(fp_fd);
		free(dlistptr);
		return (0);
	}
	(void) close(fd);
	return (0);
}



/*
 * Get device World Wide Name and AL_PA for device at path
 *
 * RETURN: 0 O.K.
 *
 * INPUTS:
 *	- path_phys must be of a device, either an IB or disk.
 */
int
g_get_wwn(char *path_phys, uchar_t port_wwn[], uchar_t node_wwn[],
	int *al_pa, int verbose)
{
sf_al_map_t	map;
int		i, j, nu, err;
char		*char_ptr, *ptr;
int		found = 0, name_id;

	P_DPRINTF("  g_get_wwn: Getting device WWN"
			" and al_pa for device: %s\n",
			path_phys);

	if (err = g_get_dev_map(path_phys, &map, verbose)) {
		return (err);
	}

	/*
	 * Get the loop identifier (switch setting) from the path.
	 *
	 * This assumes the path looks something like this:
	 * /devices/.../SUNW,socal@3,0/SUNW,sf@0,0/SUNW,ssd@x,0
	 * or
	 * /devices/.../SUNW,usoc@3,0/SUNW,fp@0,0/SUNW,ssd@x,0
	 */
	if ((char_ptr = strrchr(path_phys, '@')) == NULL) {
		return (L_INVLD_PATH_NO_ATSIGN_FND);
	}
	char_ptr++;	/* point to the loop identifier or WWN */

	/* get the architecture of the machine */
	if ((err = g_get_machineArch(&name_id)) != 0) {
		return (err);
	}
	if (name_id) {
		nu = strtol(char_ptr, &ptr, 16);
		if (ptr == char_ptr) {
			return (L_INVLD_ID_FOUND);
		}
		if ((nu > 0x7e) || (nu < 0)) {
			return (L_INVLD_ID_FOUND);
		}
		for (i = 0; i < map.sf_count; i++) {
			*al_pa = map.sf_addr_pair[i].sf_al_pa;
			if (g_switch_to_alpa[nu] == *al_pa) {
				found = 1;
				break;
			}
		}
	} else {
		unsigned long long pwwn;
		uchar_t	*byte_ptr;

		/* Format of WWN is ssd@w2200002037000f96,0:a,raw */
		if (*char_ptr != 'w') {
			return (L_INVLD_WWN_FORMAT);
		}
		char_ptr++;
		pwwn = strtoull(char_ptr, &ptr, 16);
		if (ptr == char_ptr) {
			return (L_NO_WWN_FOUND_IN_PATH);
		}
		P_DPRINTF("  g_get_wwn:  Looking for WWN "
		"0x%llx\n", pwwn);
		for (i = 0; i < map.sf_count; i++) {
			byte_ptr = (uchar_t *)&pwwn;
			for (j = 0; j < 8; j++) {
				if (map.sf_addr_pair[i].sf_port_wwn[j] !=
					*byte_ptr++) {
					found = 0;
					break;
				} else {
					found = 1;
				}
			}
			if (found) {
				break;
			}
		}
	}

	if (!found) {
		return (L_NO_LOOP_ADDRS_FOUND);
	}
	for (j = 0; j < 8; j++) {
		port_wwn[j] = map.sf_addr_pair[i].sf_port_wwn[j];
		node_wwn[j] = map.sf_addr_pair[i].sf_node_wwn[j];
	}
	*al_pa = map.sf_addr_pair[i].sf_al_pa;

	return (0);
}


int
g_get_inquiry(char *path, L_inquiry *l_inquiry)
{
int	    fd, status;

	P_DPRINTF("  g_get_inquiry: path: %s\n", path);
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);
	status = g_scsi_inquiry_cmd(fd,
		(uchar_t *)l_inquiry, sizeof (struct l_inquiry_struct));
	(void) close(fd);
	return (status);
}


int
g_get_perf_statistics(char *path, uchar_t *perf_ptr)
{
int	fd;

	P_DPRINTF("  g_get_perf_statistics: Get Performance Statistics:"
		"\n  Path:%s\n",
		path);

	/* initialize tables */
	(void) memset(perf_ptr, 0, sizeof (int));

	/* open controller */
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);


	/* update parameters in the performance table */

	/* get the period in seconds */


	(void) close(fd);

	return (0);
}


int
g_start(char *path)
{
int	status;
int	fd;

	P_DPRINTF("  g_start: Start: Path %s\n", path);
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);
	status = g_scsi_start_cmd(fd);
	(void) close(fd);
	return (status);
}

int
g_stop(char *path, int immediate_flag)
{
int	status, fd;

	P_DPRINTF("  g_stop: Stop: Path %s\n", path);
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);
	status = g_scsi_stop_cmd(fd, immediate_flag);
	(void) close(fd);
	return (status);
}

int
g_reserve(char *path)
{
int 	fd, status;

	P_DPRINTF("  g_reserve: Reserve: Path %s\n", path);
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);
	status = g_scsi_reserve_cmd(fd);
	(void) close(fd);
	return (status);
}

int
g_release(char *path)
{
int 	fd, status;

	P_DPRINTF("  g_release: Release: Path %s\n", path);
	if ((fd = g_object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (L_OPEN_PATH_FAIL);
	status = g_scsi_release_cmd(fd);
	(void) close(fd);
	return (status);
}

static char
ctoi(char c)
{
	if ((c >= '0') && (c <= '9'))
		c -= '0';
	else if ((c >= 'A') && (c <= 'F'))
		c = c - 'A' + 10;
	else if ((c >= 'a') && (c <= 'f'))
		c = c - 'a' + 10;
	else
		c = -1;
	return (c);
}

int
g_string_to_wwn(uchar_t *wwn, uchar_t *wwnp)
{
	int	i;
	char	c, c1;

	*wwnp++ = 0;
	*wwnp++ = 0;
	for (i = 0; i < WWN_SIZE - 2; i++, wwnp++) {
		c = ctoi(*wwn++);
		c1 = ctoi(*wwn++);
		if (c == -1 || c1 == -1)
			return (-1);
		*wwnp = ((c << 4) + c1);
	}

	return (0);

}


/*
 * Get multiple paths to a given device port.
 * INPUTS:
 *	port WWN string.
 */
int
g_get_port_multipath(char *port_wwn_s, struct dlist **dlh, int verbose)
{
int		err;
WWN_list	*wwn_list, *wwn_list_ptr;
struct dlist	*dlt, *dl;


	/* Initialize list structures. */
	dl = *dlh  = dlt = (struct dlist *)NULL;
	wwn_list = wwn_list_ptr = NULL;

	H_DPRINTF("  g_get_port_multipath: Looking for multiple paths for"
		" device with\n    port WWW:"
		"%s\n", port_wwn_s);

	if (err = g_get_wwn_list(&wwn_list, verbose)) {
		return (err);
	}

	for (wwn_list_ptr = wwn_list; wwn_list_ptr != NULL;
				wwn_list_ptr = wwn_list_ptr->wwn_next) {
		if (strcmp(port_wwn_s, wwn_list_ptr->port_wwn_s) == 0) {
			if ((dl = (struct dlist *)
				g_zalloc(sizeof (struct dlist))) == NULL) {
				while (*dlh != NULL) {
					dl = (*dlh)->next;
					(void) g_destroy_data(*dlh);
					*dlh = dl;
				}
				(void) g_free_wwn_list(&wwn_list);
				return (L_MALLOC_FAILED);
			}
			H_DPRINTF("  g_get_port_multipath:"
				" Found multipath:\n    %s\n",
				wwn_list_ptr->physical_path);
			dl->dev_path = strdup(wwn_list_ptr->physical_path);
			dl->logical_path = strdup(wwn_list_ptr->logical_path);
			if (*dlh == NULL) {
				*dlh = dlt = dl;
			} else {
				dlt->next = dl;
				dl->prev = dlt;
				dlt = dl;
			}
		}
	}
	(void) g_free_wwn_list(&wwn_list);
	return (0);
}



/*
 * Get multiple paths to a given disk/tape device.
 * The arg: devpath should be the physical path to device.
 *
 * OUTPUT:
 *	multipath_list	points to a list of multiple paths to the device.
 *	NOTE: The caller must free the allocated list (dlist).
 *
 * RETURNS:
 *	0	 if O.K.
 *	non-zero otherwise
 */
int
g_get_multipath(char *devpath, struct dlist **multipath_list,
	struct wwn_list_struct *wwn_list, int verbose)
{
WWN_list	*wwn_list_ptr;
char		node_wwn_s[WWN_S_LEN];
struct dlist	*dl, *dlt;
char		path[MAXPATHLEN], *ptr;
int		len;


	H_DPRINTF("  g_get_multipath: Looking for multiple paths for"
		" device at path: %s\n", devpath);

	/* Strip partition information. */
	if ((ptr = strrchr(devpath, ':')) != NULL) {
		len = strlen(devpath) - strlen(ptr);
		(void) strncpy(path, devpath, len);
		path[len] = '\0';
	} else {
		(void) strcpy(path, devpath);
	}

	*multipath_list = dl = dlt = (struct dlist *)NULL;


	if (wwn_list == NULL) {
		return (L_NULL_WWN_LIST);
	}

	for (*node_wwn_s = NULL, wwn_list_ptr = wwn_list;
				wwn_list_ptr != NULL;
				wwn_list_ptr = wwn_list_ptr->wwn_next) {
		if (strstr(wwn_list_ptr->physical_path, path) != NULL) {
			(void) strcpy(node_wwn_s, wwn_list_ptr->node_wwn_s);
			break;
		}
	}

	if (*node_wwn_s == NULL) {
		H_DPRINTF("node_wwn_s is NULL!\n");
		return (L_NO_NODE_WWN_IN_WWNLIST);
	}

	for (wwn_list_ptr = wwn_list; wwn_list_ptr != NULL;
				wwn_list_ptr = wwn_list_ptr->wwn_next) {
		if (strcmp(node_wwn_s, wwn_list_ptr->node_wwn_s) == 0) {
			if ((dl = (struct dlist *)
				g_zalloc(sizeof (struct dlist))) == NULL) {
				while (*multipath_list != NULL) {
					dl = dlt->next;
					(void) g_destroy_data(dlt);
					dlt = dl;
				}
				return (L_MALLOC_FAILED);
			}
			H_DPRINTF("  g_get_multipath: Found multipath=%s\n",
					wwn_list_ptr->physical_path);
			dl->dev_path = strdup(wwn_list_ptr->physical_path);
			dl->logical_path = strdup(wwn_list_ptr->logical_path);
			if (*multipath_list == NULL) {
				*multipath_list = dlt = dl;
			} else {
				dlt->next = dl;
				dl->prev = dlt;
				dlt = dl;
			}
		}
	}
	return (0);
}



/*
 * Free a multipath list
 *
 */
void
g_free_multipath(struct dlist *dlh)
{
struct dlist	*dl;

	while (dlh != NULL) {
		dl = dlh->next;
		if (dlh->dev_path != NULL)
			(void) g_destroy_data(dlh->dev_path);
		if (dlh->logical_path != NULL)
			(void) g_destroy_data(dlh->logical_path);
		(void) g_destroy_data(dlh);
		dlh = dl;
	}
}



/*
 * Get the path to the nexus (HBA) driver.
 * This assumes the path looks something like this:
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@e,0:0
 * or maybe this
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@1,0
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0:1
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0:devctl
 * (or "usoc" instead of "socal" and "fp" for "sf")
 *
 * Which should resolve to a path like this:
 * /devices/sbus@1f,0/SUNW,socal@1,0:1
 * or
 * /devices/sbus@1f,0/SUNW,usoc@1,0:1
 *
 * or
 * /devices/pci@4,2000/scsi@1/ses@w50800200000000d2,0:0
 * which should resolve to
 * /devices/pci@4,2000/scsi@1:devctl
 */
int
g_get_nexus_path(char *path_phys, char **nexus_path)
{
uchar_t		port = 0;
int		port_flag = 0;
char		*char_ptr;
char		drvr_path[MAXPATHLEN];
char		buf[MAXPATHLEN];
char		temp_buf[MAXPATHLEN];
struct stat	stbuf;
uint_t		path_type;

	*nexus_path = NULL;
	(void) strcpy(drvr_path, path_phys);
	if (strstr(drvr_path, DRV_NAME_SSD) || strstr(drvr_path, SES_NAME)) {
		if ((char_ptr = strrchr(drvr_path, '/')) == NULL) {
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate string  */
	}

	path_type = g_get_path_type(drvr_path);

	if ((path_type & FC4_SF_XPORT) || (path_type & FC_GEN_XPORT)) {

	/*
	 * XXX -- This is not really a nice way to do it, but it works
	 *	for now.
	 */
		/* sf driver in path so capture the port # */
		if ((char_ptr = strstr(drvr_path, "sf@")) == NULL) {
			if ((char_ptr = strstr(drvr_path, "fp@")) == NULL)
				return (L_INVALID_PATH);
		}
		port = atoi(char_ptr + 3);
		if (port > 1) {
			return (L_INVLD_PORT_IN_PATH);
		}

		if ((char_ptr = strrchr(drvr_path, '/')) == NULL) {
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate string  */
		port_flag++;

		if (path_type & FC4_SF_XPORT) {
			L_DPRINTF("  g_get_nexus_path:"
				" sf driver in path so use port #%d.\n",
				port);
		} else { /* blindly assume fp if we made it this far */
			L_DPRINTF("  g_get_nexus_path:"
				" fp driver in path so use port #%d.\n",
				port);
		}
	}

	if (stat(drvr_path, &stbuf) != 0) {
		return (L_LSTAT_ERROR);
	}

	if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
		/*
		 * Found a directory.
		 * Now append a port number or devctl to the path.
		 */
		if (port_flag) {
			/* append port */
			(void) sprintf(buf, ":%d", port);
		} else {
			/* Try adding port 0 and see if node exists. */
			(void) sprintf(temp_buf, "%s:0", drvr_path);
			if (stat(temp_buf, &stbuf) != 0) {
				/*
				 * Path we guessed at does not
				 * exist so it must be a driver
				 * that ends in :devctl.
				 */
				(void) sprintf(buf, ":devctl");
			} else {
				/*
				 * The path that was entered
				 * did not include a port number
				 * so the port was set to zero, and
				 * then checked. The default path
				 * did exist.
				 */
				ER_DPRINTF("Since a complete path"
					" was not supplied "
					"a default path is being"
					" used:\n  %s\n",
					temp_buf);
				(void) sprintf(buf, ":0");
			}
		}

		(void) strcat(drvr_path, buf);
	}
	*nexus_path = g_alloc_string(drvr_path);
	L_DPRINTF("  g_get_nexus_path: Nexus path = %s\n", drvr_path);
	return (0);
}

/*
 * This functions enables or disables a FCA port depending on the
 * argument, cmd, passed to it. If cmd is PORT_OFFLINE, the function
 * tries to disable the port specified by the argument 'phys_path'. If
 * cmd is PORT_ONLINE, the function tries to enable the port specified
 * by the argument 'phys_path'.
 * INPUTS :
 *	nexus_port_ptr - Pointer to the nexus path of the FCA port to
 *			operate on
 *	cmd       - PORT_OFFLINE or PORT_ONLINE
 * RETURNS :
 *	0 on success and non-zero otherwise
 */
static int
g_set_port_state(char *nexus_port_ptr, int cmd)
{
	int	sub_cmd;
	int	path_type, fd;
	fcio_t	fcio;

	if ((path_type = g_get_path_type(nexus_port_ptr)) == 0) {
		return (L_INVALID_PATH);
	}

	if ((fd = open(nexus_port_ptr, O_NDELAY|O_RDONLY)) == -1) {
		return (L_OPEN_PATH_FAIL);
	}

	switch (cmd) {
		case PORT_OFFLINE:
			if (path_type & FC_FCA_MASK) {
				/* Leadville - usoc/fp drivers */
				sub_cmd = FCIO_DIAG_PORT_DISABLE;
				fcio.fcio_xfer = FCIO_XFER_NONE;
				fcio.fcio_cmd = FCIO_DIAG;
				fcio.fcio_flags = fcio.fcio_cmd_flags = 0;
				fcio.fcio_ilen = (size_t)sizeof (sub_cmd);
				fcio.fcio_ibuf = (caddr_t)&sub_cmd;
				fcio.fcio_olen = fcio.fcio_alen = 0;
				fcio.fcio_obuf = fcio.fcio_abuf = NULL;

				/*
				 * Issue the ioctl now. Retries etc are taken
				 * care of by g_issue_fcio_ioctl()
				 */
				if (g_issue_fcio_ioctl(fd, &fcio, 0) != 0) {
					close(fd);
					return (L_PORT_OFFLINE_FAIL);
				}
			} else if (path_type & FC4_SOCAL_FCA) {
				/*
				 * Socal/sf drivers -
				 * The socal driver currently returns EFAULT
				 * even if the ioctl has completed successfully.
				 */
				if (ioctl(fd, FCIO_LOOPBACK_INTERNAL,
							NULL) == -1) {
					close(fd);
					return (L_PORT_OFFLINE_FAIL);
				}
			} else {
				/*
				 * QLogic card - ifp driver
				 * Can't do much here since the driver currently
				 * doesn't support this feature. We'll just fail
				 * for now. Support can be added when the driver
				 * is enabled with the feature at a later date.
				 */
				close(fd);
				return (L_PORT_OFFLINE_UNSUPPORTED);
			}
			break;
		case PORT_ONLINE:
			if (path_type & FC_FCA_MASK) {
				/* Leadville - usoc/fp drivers */
				sub_cmd = FCIO_DIAG_PORT_ENABLE;
				fcio.fcio_xfer = FCIO_XFER_NONE;
				fcio.fcio_cmd = FCIO_DIAG;
				fcio.fcio_flags = fcio.fcio_cmd_flags = 0;
				fcio.fcio_ilen = (size_t)sizeof (sub_cmd);
				fcio.fcio_ibuf = (caddr_t)&sub_cmd;
				fcio.fcio_olen = fcio.fcio_alen = 0;
				fcio.fcio_obuf = fcio.fcio_abuf = NULL;

				/*
				 * Issue the ioctl now. Retries etc are taken
				 * care of by g_issue_fcio_ioctl()
				 */
				if (g_issue_fcio_ioctl(fd, &fcio, 0) != 0) {
					close(fd);
					return (L_PORT_ONLINE_FAIL);
				}
			} else if (path_type & FC4_SOCAL_FCA) {
				/*
				 * Socal/sf drivers
				 * The socal driver currently returns EFAULT
				 * even if the ioctl has completed successfully.
				 */
				if (ioctl(fd, FCIO_NO_LOOPBACK, NULL) == -1) {
					close(fd);
					return (L_PORT_ONLINE_FAIL);
				}
			} else {
				/*
				 * QLogic card - ifp driver
				 * Can't do much here since the driver currently
				 * doesn't support this feature. We'll just fail
				 * for now. Support can be added when the driver
				 * is enabled with the feature at a later date.
				 */
				close(fd);
				return (L_PORT_ONLINE_UNSUPPORTED);
			}
			break;
		default:
			close(fd);
			return (-1);
	}
	close(fd);
	return (0);
}

/*
 * The interfaces defined below (g_port_offline() and g_port_online())
 * are what will be exposed to applications. We will hide g_set_port_state().
 * They have to be functions (as against macros) because making them
 * macros will mean exposing g_set_port_state() and we dont want to do that
 */

int
g_port_offline(char *path)
{
	return (g_set_port_state(path, PORT_OFFLINE));
}

int
g_port_online(char *path)
{
	return (g_set_port_state(path, PORT_ONLINE));
}
