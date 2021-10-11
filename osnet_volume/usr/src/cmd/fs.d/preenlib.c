#pragma ident	"@(#)preenlib.c	1.5	96/04/18 SMI"
/*
 * Copyright (c) 1992,1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * common routines for parallelization (used by both fsck and quotacheck)
 */
#include <stdio.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <macros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mntent.h>
#include <sys/dkio.h>

/*
 * data structures for parallelization
 */
static struct driver {
	char 	*name;			/* driver name (from DKIOCINFO) */
	u_int	mapsize;		/* size of `busymap' */
	u_int	*busymap;		/* bitmask of active units */
	int	(*choosefunc)();	/* driver specific chooser */
	void	*data;			/* driver private data */
};

static struct onedev {
	int	drvid;			/* index in driver array */
	u_int	mapsize;		/* size of `unitmap' */
	u_int	*unitmap;		/* unit #'s (from DKIOCINFO) */
	struct onedev *nxtdev;
};

static struct rawdev {
	char	*devname;		/* name passed to preen_addev */
	struct	onedev *alldevs;	/* info about each component device */
	struct rawdev *nxtrd;		/* next entry in list */
};

static int debug = 0;

/*
 * defines used in building shared object names
 */

/* the directory where we find shared objects */
#define	OBJECT_DIRECTORY	"/usr/lib/drv"

/* a shared object name is OBJECT_PREFIX || driver_name */
#define	OBJECT_PREFIX		"preen_"

/* the version of the driver interface we support */
#define	OBJECT_VERSION		1

/* the "build" entry point for a driver specific object is named this */
#define	BUILD_ENTRY		preen_build_devs
#define	BUILD_NAME		"preen_build_devs"

#define	DRIVER_ALLOC	10
static int ndrivers, ndalloc;
static struct driver *dlist;

static struct rawdev *unchecked, *active, *get_runnable();
static struct onedev *alloc_dev();
static int chooseone();

#define	WORDSIZE	(NBBY * sizeof (u_int))

void 	preen_addunit(void *, char *, int (*)(), void *, u_int);
int 	preen_subdev(char *, struct dk_cinfo *, void *);

static int 	alloc_driver(char *, int (*)(), void *);
static void 	addunit(struct onedev *, u_int);
static void	makebusy(struct onedev *);
static void	notbusy(struct rawdev *);

/*
 * add the given device to the list of devices to be checked
 */
preen_addev(char *devnm)
{
	struct rawdev *rdp;
	int fd;
	struct dk_cinfo dki;
	extern char *strdup();

	if ((fd = open64(devnm, O_RDONLY)) == -1) {
		perror(devnm);
		return (-1);
	}
	if (ioctl(fd, DKIOCINFO, &dki) == -1) {
		perror("DKIOCINFO");
		fprintf(stderr, "device: `%s'\n", devnm);
		(void) close(fd);
		return (-1);
	}
	(void) close(fd);
	if ((rdp = (struct rawdev *)malloc(sizeof (struct rawdev))) == NULL) {
		(void) fprintf(stderr, "out of memory in preenlib\n");
		return (-1);
	}
	if ((rdp->devname = strdup(devnm)) == NULL) {
		(void) fprintf(stderr, "out of memory in preenlib\n");
		return (-1);
	}
	rdp->alldevs = NULL;
	rdp->nxtrd = NULL;

	if (preen_subdev(devnm, &dki, (void *)rdp)) {
		preen_addunit(rdp, dki.dki_dname, NULL, NULL, dki.dki_unit);
	}

	rdp->nxtrd = unchecked;
	unchecked = rdp;
	return (0);
}

preen_subdev(char *name, struct dk_cinfo *dkiop, void *dp)
{
	char modname[255];
	void *dlhandle;
	int (*fptr)();

	(void) sprintf(modname, "%s/%s%s.so.%d",
	    OBJECT_DIRECTORY, OBJECT_PREFIX, dkiop->dki_dname, OBJECT_VERSION);
	dlhandle = dlopen(modname, RTLD_LAZY);
	if (dlhandle == NULL) {
		if (debug)
			(void) fprintf(stderr, "preen_subdev: %s\n", dlerror());
		return (1);
	}
	fptr = (int (*)())dlsym(dlhandle, BUILD_NAME);
	if (fptr == NULL) {
		if (debug)
			(void) fprintf(stderr, "preen_subdev: %s\n", dlerror());
		return (1);
	}
	(*fptr)(name, dkiop, dp);
	return (0);
}

/*
 * select a device from the "unchecked" list, and add it to the
 * active list.
 */
preen_getdev(char *devnm)
{
	struct rawdev *rdp;
	register struct onedev *dp;

	if (unchecked == NULL)
		return (0);

	rdp = get_runnable(&unchecked);

	if (rdp) {
		for (dp = rdp->alldevs; dp; dp = dp->nxtdev) {
			makebusy(dp);
		}
		rdp->nxtrd = active;
		active = rdp;
		(void) strcpy(devnm, rdp->devname);
		return (1);
	} else {
		return (2);
	}
}

preen_releasedev(char *name)
{
	register struct rawdev *dp, *ldp;

	for (ldp = NULL, dp = active; dp != NULL; ldp = dp, dp = dp->nxtrd) {
		if (strcmp(dp->devname, name) == 0)
			break;
	}

	if (dp == NULL)
		return (-1);
	if (ldp != NULL) {
		ldp->nxtrd = dp->nxtrd;
	} else {
		active = dp->nxtrd;
	}

	notbusy(dp);
	/*
	free(dp->devname);
	free(dp);
	*/
	return (0);
}

static
struct rawdev *
get_runnable(struct rawdev **devlist)
{
	register struct rawdev *last, *rdp;
	register struct onedev *devp;
	register struct driver *drvp;
	int rc = 1;

	for (last = NULL, rdp = *devlist; rdp; last = rdp, rdp = rdp->nxtrd) {
		for (devp = rdp->alldevs; devp != NULL; devp = devp->nxtdev) {
			drvp = &dlist[devp->drvid];
			rc = (*drvp->choosefunc)(devp->mapsize, devp->unitmap,
			    drvp->mapsize, drvp->busymap);
			if (rc != 0)
				break;
		}
		if (rc == 0)
			break;
	}

	/*
	 * remove from list...
	 */
	if (rdp) {
		if (last) {
			last->nxtrd = rdp->nxtrd;
		} else {
			*devlist = rdp->nxtrd;
		}
	}

	return (rdp);
}

/*
 * add the given driver/unit reference to the `rawdev' structure identified
 * by `cookie'
 * If a new `driver' structure needs to be created, associate the given
 * choosing function and driver private data with it.
 */
void
preen_addunit(
	void    *cookie,
	char	*dname,		/* driver name */
	int	(*cf)(),	/* candidate choosing function */
	void	*datap,		/* driver private data */
	u_int	unit)		/* unit number */
{
	register int drvid;
	register struct driver *dp;
	register struct onedev *devp;
	struct rawdev *rdp = (struct rawdev *)cookie;

	/*
	 * locate the driver struct
	 */
	dp = NULL;
	for (drvid = 0; drvid < ndrivers; drvid++) {
		if (strcmp(dlist[drvid].name, dname) == 0) {
			dp = &dlist[drvid];
			break;
		}
	}

	if (dp == NULL) {
		/*
		 * driver struct doesn't exist yet -- create one
		 */
		if (cf == NULL)
			cf = chooseone;
		drvid = alloc_driver(dname, cf, datap);
		dp = &dlist[drvid];
	}

	for (devp = rdp->alldevs; devp != NULL; devp = devp->nxtdev) {
		/*
		 * see if this device already references the given driver
		 */
		if (devp->drvid == drvid)
			break;
	}

	if (devp == NULL) {
		/*
		 * allocate a new `struct onedev' and chain it in
		 * rdp->alldevs...
		 */
		devp = alloc_dev(drvid);
		devp->nxtdev = rdp->alldevs;
		rdp->alldevs = devp;
	}

	/*
	 * add `unit' to the unitmap in devp
	 */
	addunit(devp, unit);
}

static
int
alloc_driver(char *name, int (*cf)(), void *datap)
{
	register struct driver *dp;
	extern char *strdup();

	if (ndrivers == ndalloc) {
		dlist = ndalloc ?
		    (struct driver *)
		    realloc(dlist, sizeof (struct driver) * DRIVER_ALLOC) :
		    (struct driver *)
		    malloc(sizeof (struct driver) * DRIVER_ALLOC);
		if (dlist == NULL) {
			(void) fprintf(stderr, "out of memory in preenlib\n");
			exit(1);
		}
		ndalloc += DRIVER_ALLOC;
	}

	dp = &dlist[ndrivers];
	dp->name = strdup(name);
	if (dp->name == NULL) {
		(void) fprintf(stderr, "out of memory in preenlib\n");
		exit(1);
	}
	dp->choosefunc = cf;
	dp->data = datap;
	dp->mapsize = 0;
	dp->busymap = NULL;
	return (ndrivers++);
}

static
struct onedev *
alloc_dev(int did)
{
	register struct onedev *devp;

	devp = (struct onedev *)malloc(sizeof (struct onedev));
	if (devp == NULL) {
		(void) fprintf(stderr, "out of memory in preenlib\n");
		exit(1);
	}
	devp->drvid = did;
	devp->mapsize = 0;
	devp->unitmap = NULL;
	devp->nxtdev = NULL;
	return (devp);
}

static
void
addunit(struct onedev *devp, u_int unit)
{
	u_int newsize;

	newsize = howmany(unit+1, WORDSIZE);
	if (devp->mapsize < newsize) {
		devp->unitmap = devp->mapsize ?
		    (u_int *)realloc(devp->unitmap, newsize * sizeof (u_int)) :
		    (u_int *)malloc(newsize * sizeof (u_int));
		if (devp->unitmap == NULL) {
			(void) fprintf(stderr, "out of memory in preenlib\n");
			exit(1);
		}
		(void) memset((char *)&devp->unitmap[devp->mapsize], 0,
		    (u_int) ((newsize - devp->mapsize) * sizeof (u_int)));
		devp->mapsize = newsize;
	}
	devp->unitmap[unit / WORDSIZE] |= (1 << (unit % WORDSIZE));
}

static
chooseone(int devmapsize, u_long *devmap, int drvmapsize, u_long *drvmap)
{
	register int i;

	for (i = 0; i < min(devmapsize, drvmapsize); i++) {
		if (devmap[i] & drvmap[i])
			return (1);
	}
	return (0);
}

/*
 * mark the given driver/unit pair as busy.  This is called from
 * preen_getdev.
 */
static
void
makebusy(struct onedev *dev)
{
	struct driver *drvp = &dlist[dev->drvid];
	int newsize = dev->mapsize;
	register int i;

	if (drvp->mapsize < newsize) {
		drvp->busymap = drvp->mapsize ?
		    (u_int *)realloc(drvp->busymap, newsize * sizeof (u_int)) :
		    (u_int *)malloc(newsize * sizeof (u_int));
		if (drvp->busymap == NULL) {
			(void) fprintf(stderr, "out of memory in preenlib\n");
			exit(1);
		}
		(void) memset((char *)&drvp->busymap[drvp->mapsize], 0,
		    (u_int) ((newsize - drvp->mapsize) * sizeof (u_int)));
		drvp->mapsize = newsize;
	}

	for (i = 0; i < newsize; i++)
		drvp->busymap[i] |= dev->unitmap[i];
}

/*
 * make each device in the given `rawdev' un-busy.
 * Called from preen_releasedev
 */
static
void
notbusy(struct rawdev *rd)
{
	struct onedev *devp;
	struct driver *drvp;
	register int i;

	for (devp = rd->alldevs; devp; devp = devp->nxtdev) {
		drvp = &dlist[devp->drvid];
		for (i = 0; i < devp->mapsize; i++)
			drvp->busymap[i] &= ~(devp->unitmap[i]);
	}
}
