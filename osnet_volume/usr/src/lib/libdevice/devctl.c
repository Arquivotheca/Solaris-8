/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devctl.c	1.14	99/04/16 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "libdevice.h"

/*LINTLIBRARY*/

#ifdef	DEBUG
static int libdevice_db = 0;
#define	dprintf(args) if (libdevice_db) (void) printf(args)
#else
#define	dprintf(args)
#endif

static const char *devctl_minordev = ":devctl";

struct devctl_hdl {
	char	*opath;
	int	fd;
	uint_t	flags;
	uint_t	num_components;
	uint_t	cur_component;
	char	**components;
};
#define	DCP(x)	((struct devctl_hdl *)(x))

#define	DC_BUSONLY	1
#define	DC_APONLY	2

typedef enum { DEVCTL_BUS, DEVCTL_DEVICE, DEVCTL_AP } devctl_path_type_t;

static uint_t dn_path_to_components(char *, char ***names);
static char *dn_components_to_path(uint_t, char **);
static void dn_parse_component(char *, char **, char **, char **);
static void free_components(int, char **);
static int dc_childcmd(uint_t, struct devctl_hdl *, uint_t *);
static int dc_buscmd(uint_t, struct devctl_hdl *, uint_t *);
static devctl_hdl_t dc_makehandle(devctl_path_type_t type, char *devfs_path,
    uint_t c_flags);


/*
 * release the devctl handle allocated by devctl_acquire()
 */
void
devctl_release(devctl_hdl_t hdl)
{
	if (DCP(hdl)->fd != 0 && DCP(hdl)->fd != -1)
		(void) close(DCP(hdl)->fd);

	if (DCP(hdl)->num_components != 0)
		free_components(DCP(hdl)->num_components, DCP(hdl)->components);

	if (DCP(hdl)->opath != NULL)
		free(DCP(hdl)->opath);

	free(hdl);
}

/*
 * given a devfs (/devices) pathname to a leaf device, access the
 * bus nexus device exporting the ":devctl" device control interface
 * and return a handle to be passed to the devctl_bus_XXX()
 * functions.
 */
devctl_hdl_t
devctl_bus_acquire(char *devfs_path, uint_t flags)
{
	return (dc_makehandle(DEVCTL_BUS, devfs_path, flags));
}

devctl_hdl_t
devctl_device_acquire(char *devfs_path, uint_t flags)
{
	return (dc_makehandle(DEVCTL_DEVICE, devfs_path, flags));
}

/*
 * given a devfs (/devices) pathname to an attachment point device,
 * access the device and return a handle to be passed to the
 * devctl_ap_XXX() functions.
 */
devctl_hdl_t
devctl_ap_acquire(char *devfs_path, uint_t flags)
{
	return (dc_makehandle(DEVCTL_AP, devfs_path, flags));
}


static devctl_hdl_t
dc_makehandle(devctl_path_type_t type, char *devfs_path, uint_t c_flags)
{
	char *iocpath;
	struct devctl_hdl *dcp;
	uint_t otype = O_RDWR;

	/*
	 * perform basic sanity checks on the parameters
	 * the only flag we expect is the exclusive access flag (DC_EXCL)
	 */
	if ((devfs_path == NULL) ||
	    ((c_flags != 0) && (c_flags != DC_EXCL))) {
		errno = EINVAL;
		return (NULL);
	}

	if (strlen(devfs_path) > MAXPATHLEN - 1) {
		errno = EINVAL;
		return (NULL);
	}

	dprintf(("dc_makehandle: dev (%s) flags (0x%x)\n", devfs_path,
	    c_flags));

	if ((dcp = calloc(1, sizeof (*dcp))) == NULL) {
		dprintf(("dc_makehandle: calloc failure\n"));
		errno = ENOMEM;
		return (NULL);
	}

	if (c_flags & DC_EXCL)
		otype |= O_EXCL;

	/*
	 * take the devfs pathname and break it into the individual
	 * components.
	 */
	dcp->num_components = dn_path_to_components(devfs_path,
	    &dcp->components);
	if (dcp->num_components == 0) {
		dprintf(("dc_makehandle: pathname parse failed\n"));
		devctl_release((devctl_hdl_t)dcp);
		errno = EINVAL;
		return (NULL);
	}

	/* save copy of the original path */
	if ((dcp->opath = strdup(devfs_path)) == NULL) {
		devctl_release((devctl_hdl_t)dcp);
		errno = ENOMEM;
		return (NULL);
	}

	/*
	 * construct a pathname to the bus nexus driver that exports
	 * the ":devctl" control interface
	 */
	switch (type) {
	case DEVCTL_BUS:
		iocpath = dn_components_to_path(dcp->num_components,
		    dcp->components);
		if (iocpath == NULL) {
			devctl_release((devctl_hdl_t)dcp);
			errno = EINVAL;
			return (NULL);
		}
		if ((strlen(iocpath) + strlen(devctl_minordev)) >
			MAXPATHLEN - 1) {
			free(iocpath);
			devctl_release((devctl_hdl_t)dcp);
			errno = EINVAL;
			return (NULL);
		}
		(void) strcat(iocpath, devctl_minordev);
		dcp->flags = DC_BUSONLY;
		break;
	case DEVCTL_DEVICE:
		iocpath = dn_components_to_path(dcp->num_components - 1,
		    dcp->components);
		if (iocpath == NULL) {
			devctl_release((devctl_hdl_t)dcp);
			errno = EINVAL;
			return (NULL);
		}
		if ((strlen(iocpath) + strlen(devctl_minordev)) >
			MAXPATHLEN - 1) {
			free(iocpath);
			devctl_release((devctl_hdl_t)dcp);
			errno = EINVAL;
			return (NULL);
		}
		(void) strcat(iocpath, devctl_minordev);
		break;
	case DEVCTL_AP:
		iocpath = dn_components_to_path(dcp->num_components,
		    dcp->components);
		if (iocpath == NULL) {
			devctl_release((devctl_hdl_t)dcp);
			errno = EINVAL;
			return (NULL);
		}
		dcp->flags = DC_APONLY;
		break;
	default:
		devctl_release((devctl_hdl_t)dcp);
		errno = EINVAL;
		return (NULL);
	}

	dprintf(("dc_makehandle: ioctl device path (%s)\n", iocpath));

	/*
	 * Open the bus nexus ":devctl" interface or AP minor node (for AP
	 * only). We can fail because:
	 *	1) no such device (ENXIO)
	 *	2) Already open   (EBUSY)
	 *	3) No permission  (EPERM)
	 */
	dcp->fd = open(iocpath, otype);
	if (dcp->fd == -1) {
		dprintf(("dc_makehandle: open of (%s) failed\n", iocpath));
		free(iocpath);
		devctl_release((devctl_hdl_t)dcp);
		return (NULL);
	}

	/*
	 * release the space allocated for the pathname and return the
	 * handle to the caller
	 */
	free(iocpath);
	return ((devctl_hdl_t)dcp);
}

/*
 * place device "device_path" online
 */
int
devctl_device_online(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_childcmd(DEVCTL_DEVICE_ONLINE, DCP(dcp), NULL);
	return (rv);
}

/*
 * take device "device_path" offline
 */
int
devctl_device_offline(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_childcmd(DEVCTL_DEVICE_OFFLINE, DCP(dcp), NULL);
	return (rv);
}

/*
 * take device "device_path" offline and remove the dev_info node
 */
int
devctl_device_remove(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_childcmd(DEVCTL_DEVICE_REMOVE, DCP(dcp), NULL);
	return (rv);
}


int
devctl_bus_quiesce(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_QUIESCE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_bus_unquiesce(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_UNQUIESCE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_bus_reset(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_RESET, DCP(dcp), NULL);
	return (rv);
}

int
devctl_bus_resetall(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_RESETALL, DCP(dcp), NULL);
	return (rv);
}

int
devctl_device_reset(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_childcmd(DEVCTL_DEVICE_RESET, DCP(dcp), NULL);
	return (rv);
}

int
devctl_device_getstate(devctl_hdl_t dcp, uint_t *devstate)
{
	int  rv;
	uint_t device_state;

	rv = dc_childcmd(DEVCTL_DEVICE_GETSTATE, DCP(dcp), &device_state);

	if (rv == -1)
		*devstate = 0;
	else
		*devstate = device_state;

	return (rv);
}

int
devctl_bus_getstate(devctl_hdl_t dcp, uint_t *devstate)
{
	int  rv;
	uint_t device_state;

	rv = dc_buscmd(DEVCTL_BUS_GETSTATE, DCP(dcp), &device_state);

	if (rv == -1)
		*devstate = 0;
	else
		*devstate = device_state;

	return (rv);
}

int
devctl_bus_configure(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_CONFIGURE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_bus_unconfigure(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_UNCONFIGURE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_ap_connect(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_AP_CONNECT, DCP(dcp), NULL);
	return (rv);
}

int
devctl_ap_disconnect(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_AP_DISCONNECT, DCP(dcp), NULL);
	return (rv);
}

int
devctl_ap_insert(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_AP_INSERT, DCP(dcp), NULL);
	return (rv);
}

int
devctl_ap_remove(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_AP_REMOVE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_ap_configure(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_AP_CONFIGURE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_ap_unconfigure(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_AP_UNCONFIGURE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_ap_getstate(devctl_hdl_t dcp, devctl_ap_state_t *apstate)
{
	int  rv;
	devctl_ap_state_t ap_state;

	rv = dc_buscmd(DEVCTL_AP_GETSTATE, DCP(dcp), (uint_t *)&ap_state);

	if (rv == -1)
		memset(apstate, 0, sizeof (struct devctl_ap_state));
	else
		*apstate = ap_state;

	return (rv);
}

static int
dc_childcmd(uint_t cmd, struct devctl_hdl *dcp, uint_t *devstate)
{

	struct devctl_iocdata iocdata;
	int  rv;

	if ((dcp == NULL) || (DCP(dcp)->flags & DC_BUSONLY)) {
		errno = EINVAL;
		return (-1);
	}

	(void) memset(&iocdata, 0, sizeof (struct devctl_iocdata));

	dn_parse_component(dcp->components[dcp->num_components - 1],
	    &(iocdata.dev_name), &(iocdata.dev_addr), &(iocdata.dev_minor));

	dprintf(("dc_childcmd: components cn %s ca %s cm %s\n",
		(iocdata.dev_name ? iocdata.dev_name : "NULL"),
		(iocdata.dev_addr ? iocdata.dev_addr : "NULL"),
		(iocdata.dev_minor ? iocdata.dev_minor : "NULL")));

	iocdata.cmd = cmd;
	iocdata.ret_state = devstate;
	rv = ioctl(dcp->fd, cmd, &iocdata);
	return (rv);
}

static int
dc_buscmd(uint_t cmd, struct devctl_hdl *dcp, uint_t *devstate)
{

	struct devctl_iocdata iocdata;
	int  rv;

	if (dcp == NULL) {
		errno = EINVAL;
		return (-1);
	}

	(void) memset(&iocdata, 0, sizeof (struct devctl_iocdata));

	dn_parse_component(dcp->components[dcp->num_components - 1],
	    &(iocdata.dev_name), &(iocdata.dev_addr), &(iocdata.dev_minor));

	dprintf(("dc_buscmd: components cn %s ca %s cm %s\n",
		(iocdata.dev_name ? iocdata.dev_name : "NULL"),
		(iocdata.dev_addr ? iocdata.dev_addr : "NULL"),
		(iocdata.dev_minor ? iocdata.dev_minor : "NULL")));

	iocdata.cmd = cmd;
	iocdata.ret_state = devstate;

	rv = ioctl(dcp->fd, cmd, &iocdata);
	return (rv);
}

/*
 * decompose a pathname into individual components
 */
static uint_t
dn_path_to_components(char *dpath, char ***path_components)
{
	char *parsep;
	char *endp;
	char *dupp;
	int  nc = 0;
	int  i = 0;
	char *dup_path;
	char **components;

	if ((dpath == NULL) || (path_components == NULL))
		return (0);

	/*
	 * calculate the number of components in the pathname
	 */
	if ((dup_path = strdup(dpath)) == NULL)
		return (0);

	parsep = (char *)strtok_r(dup_path, "/", &endp);
	while (parsep != NULL) {
		nc++;
		parsep = (char *)strtok_r(NULL, "/", &endp);
	}

	if (nc == 0) {
		*path_components = NULL;
		free(dup_path);
		return (0);
	}

	(void) strcpy(dup_path, dpath);
	/*
	 * allocate an array of pointers for each component
	 * in the pathname
	 */
	components = (char **)calloc(nc, sizeof (char *));
	if (components == NULL) {
		*path_components = NULL;
		free(dup_path);
		return (0);
	}

	parsep = (char *)strtok_r(dup_path, "/", &endp);
	while (parsep != NULL) {
		if ((dupp = strdup(parsep)) == NULL) {
			free_components(i, components);
			free(dup_path);
			return (0);
		}
		components[i++] = dupp;
		parsep = (char *)strtok_r(NULL, "/", &endp);
	}

	*path_components = components;
	free(dup_path);
	return (nc);
}

/*
 * reconstruct a pathname from a list of components
 */
char *
dn_components_to_path(uint_t num_components, char **components)
{
	char *devpath;
	uint_t i;

	if ((num_components == 0) || (components == NULL))
		return (NULL);

	devpath = (char *)malloc(MAXPATHLEN);
	if (devpath == NULL)
		return (NULL);

	*devpath = '\0';
	for (i = 0; i < num_components; i++) {
		if (components[i] == NULL ||
		    (strlen(devpath) + strlen(components[i]) + 2) >
		    MAXPATHLEN) {
			free(devpath);
			return (NULL);
		}
		(void) strcat(devpath, "/");
		(void) strcat(devpath, components[i]);
	}

	return (devpath);
}

/*
 * split a component of a "/devices" pathname into the driver
 * name, device address, and minor device specifier
 */
static void
dn_parse_component(char *devcomp, char **devname, char **devaddr,
    char **minor_spec)
{
	char *dname;
	char *daddr;
	char *mspec;

	if ((devcomp == NULL) || ((devname == NULL) && (devaddr == NULL) &&
	    (minor_spec == NULL)))
		return;

	dname = devcomp;
	daddr = strchr(devcomp, (int)'@');
	mspec = strchr(devcomp, (int)':');

	if (devname != NULL) {
		if (daddr != NULL)
			*daddr = '\0';
		*devname = strdup(dname);
		if (daddr != NULL)
			*daddr = '@';
	}

	if (devaddr != NULL) {
		if (daddr != NULL) {
			if (mspec != NULL)
				*mspec = '\0';
			*devaddr = strdup(daddr + 1);
			if (mspec != NULL)
				*mspec = ':';
		} else {
			*devaddr = NULL;
		}
	}

	if (minor_spec != NULL) {
		if (mspec != NULL)
			*minor_spec = strdup(mspec + 1);
		else
			*minor_spec = NULL;
	}
}

static void
free_components(int num_components, char **components)
{
	uint_t i;

	if ((num_components == 0) || (components == NULL))
		return;

	/*
	 * free the strings in the string array
	 */
	for (i = 0; i < num_components; i++)
		if (components[i] != NULL)
			free(components[i]);

	/*
	 * free the string array itself
	 */
	free(components);
}
