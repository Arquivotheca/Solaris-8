/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vold_dev.c	1.69	96/10/10 SMI"

#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<errno.h>
#include	<dirent.h>
#include	<dlfcn.h>
#include	<sys/types.h>
#include	<sys/mkdev.h>
#include	<sys/stat.h>
#include	<sys/dkio.h>
#include	<sys/vol.h>
#include	<thread.h>
#include	<synch.h>
#include	"vold.h"
#include	"dev.h"
#include	"label.h"


/*
 * For each unit that we manage there is a "struct devs". It keeps
 * track of where everything is.
 */

static bool_t	dev_unmap(vol_t *);

#define	DEV_HASH_SIZE		64
static struct q		dev_q_hash[DEV_HASH_SIZE];

#define	DEV_ALLOC_CHUNK		10

static struct devsw	**devsw = NULL;
static int		ndevs = 0;
static int		nallocdevs = 0;


/*
 * This is the structure in which we maintain the list of all "paths" we've
 * seen for a particular device.  We use this to keep track of which
 * names we are managing so we can un manage them when someone
 * changes the configuration file.
 */
struct devpathl {
	struct q	q;
	char		*dpl_path;
	bool_t		dpl_seen;
};



/*
 * Driver calls this to add his dsw into the chain.  Called at
 * initialization time.
 */
void
dev_new(struct devsw *dsw)
{
	int		i, na;
	struct devsw	**ndevsw;


	for (i = 0; i < ndevs; i++) {
		if (devsw[i] == dsw) {
			return;
		}
	}

	if (ndevs == nallocdevs) {
		if (devsw == 0) {
			nallocdevs = DEV_ALLOC_CHUNK;
			devsw = (struct devsw **)calloc(nallocdevs,
				sizeof (struct devsw *));
		} else {
			na = nallocdevs;
			nallocdevs += DEV_ALLOC_CHUNK;
			ndevsw = (struct devsw **)calloc(nallocdevs,
				sizeof (struct devsw *));
			for (i = 0; i < na; i++) {
				ndevsw[i] = devsw[i];
			}
			free(devsw);
			devsw = ndevsw;
		}
	}
	devsw[ndevs++] = dsw;
}


/*
 * Call the driver specific routine to return an fd for a
 * device.
 */
int
dev_getfd(dev_t dev)
{
	struct devs 	*dp;
	struct devsw 	*dsw;


	dp = dev_getdp(dev);

	if ((dev == 0) || (dp == NULL)) {
		debug(1, "getfd: there's no mapping for device (%d.%d)\n",
		    major(dev), minor(dev));
		return (NULL);
	}
	dsw = dp->dp_dsw;

	return ((*dsw->d_getfd)(dev));
}


/*
 * Return the /vol/dev path to a device
 */
char *
dev_getpath(dev_t dev)
{
	struct devs 	*dp;


	dp = dev_getdp(dev);

	if ((dev == 0) || (dp == NULL)) {
		debug(1, "getpath: there's no mapping for device (%d.%d)\n",
		    major(dev), minor(dev));
		return (NULL);
	}
	return (path_make(dp->dp_rvn));
}


/*
 * Return the symbolic name of a device.
 */
char *
dev_symname(dev_t dev)
{
	struct devs	*dp;


	if ((dp = dev_getdp(dev)) == NULL) {
		return (NULL);
	}
	return (dp->dp_symname);
}


/*
 * Called from the config file processing code, when it hits
 * a 'use' tag.
 */
bool_t
dev_use(char *mtype, char *dtype, char *path,
    char *symname, char *user, char *group, char *mode,
    bool_t temp_flag, bool_t force_flag)
{
	static bool_t	dev_checksymname(char *);
	static void	dev_usepath(struct devsw *, char *);
	extern uid_t	network_uid(char *);
	extern gid_t	network_gid(char *);
	struct devsw	*dsw;
	int		i;			/* devsw index */
	int		j;			/* match index */
	char		**pl = NULL;
	static int	hitcnt = 0;
	static char	*media_type = NULL;
	int		match_found = 0;
	char		symbuf[MAXNAMELEN];
	bool_t		res = FALSE;



	debug(10,
	"dev_use: %s %s at %s, %s@, u/g=%s/%s, temp_flag=%s, force_flag=%s\n",
	    mtype, dtype, path, symname, user, group,
	    temp_flag ? "TRUE" : "FALSE", force_flag ? "TRUE" : "FALSE");

	/*
	 * keep track of the media type -- this is so that "use ..." lines
	 * ca be continued on the next line, e.g.:
	 *
	 *	use floppy drive /dev/rdiskette0 ... floppy%d
	 *	use floppy drive /dev/rdiskette1 ... floppy%d
	 *
	 * will work (as long as all lines of a certain drive type are
	 * grouped together)
	 */
	if (media_type == NULL) {
		/* first time here */
		media_type = strdup(mtype);
	} else if (strcmp(media_type, mtype) != 0) {
		/* new media type */
		free(media_type);
		media_type = strdup(mtype);
		hitcnt = 0;
	}

	/* scan each device type */
	for (i = 0; i < ndevs; i++) {

		dsw = devsw[i];

#ifdef	DEBUG_DEV_USE
		debug(11, "dev_use: comparing with %s %s\n", dsw->d_mtype,
		    dsw->d_dtype);
#endif
		if ((strcmp(dtype, dsw->d_dtype) == 0) &&
		    (strcmp(mtype, dsw->d_mtype) == 0)) {

			/*
			 * make sure the symname doesn't have
			 * more than one % in it.
			 */
			if (dev_checksymname(symname) == FALSE) {
				warning(gettext("dev_use: bad symname %s\n"),
				    symname);
				goto dun;
			}

			/* get a list of paths that match */
			if ((pl = match_path(path, dsw->d_test)) == NULL) {
				debug(5, "find_paths %s failed\n", path);
				break;
			}

			/* for each match in the list call its driver */
			for (j = 0; pl[j] != NULL; j++) {

				(void) sprintf(symbuf, symname, hitcnt);

				if ((*dsw->d_use)(pl[j], symbuf) != FALSE) {
					hitcnt++;
					match_found++;
					dev_usepath(dsw, pl[j]);
					dsw->d_uid = network_uid(user);
					dsw->d_gid = network_gid(group);
					dsw->d_mode = strtol(mode, NULL, 0);
					if (temp_flag) {
						dsw->d_flags |= D_RMONEJECT;
					}
				}
			}

			if (match_found > 0) {
				res = TRUE;
				goto dun;	/* found at least one match */
			}
		}
	}

	if (force_flag) {
		res = TRUE;
		debug(10,
		    "dev_use: returning TRUE since \"force_flag\" set\n");
	} else {
		warning(gettext(
"either couldn't find a driver for %s \"%s\", or it's already managed\n"),
			    mtype, path);
	}
dun:
	if (pl != NULL) {
		for (j = 0; pl[j] != NULL; j++) {
			free(pl[j]);
		}
		free(pl);
	}
	return (res);
}


/*
 * Check to make sure the symbolic name doesn't have more than one %d
 * in it.
 */
static bool_t
dev_checksymname(char *s)
{
	char	*p;
	int	cnt;


	for (cnt = 0, p = s; *p != '\0'; p++) {
		if (*p == '%') {
			/* deal with escaped % case */
			if (*(p+1) == '%') {
				p++;
				continue;
			}
			cnt++;
		}
	}

	if (cnt > 1) {
		return (FALSE);
	}
	return (TRUE);
}


/*
 * When we start to read the config file, we clear out all the
 * seen flags.
 */
void
dev_configstart()
{
	int		i;
	struct devsw	*dsw;
	struct devpathl	*dpl;


	/* clear all the "seen" bits */
	for (i = 0; i < ndevs; i++) {
		dsw = devsw[i];
		for (dpl = HEAD(struct devpathl, dsw->d_pathl);
		    dpl != NULL;
		    dpl = NEXT(struct devpathl, dpl)) {
			dpl->dpl_seen = FALSE;
		}
	}
}


/*
 * After we've read the config file, we check to see if we've
 * set all the seen flags.  If there any that we haven't set,
 * we call dsw_close on them to stop using that device.
 */
void
dev_configend()
{
	int			i;
	struct devsw		*dsw;
	struct devpathl		*dpl;
	struct devpathl		*dpl_next = NULL;


	for (i = 0; i < ndevs; i++) {

		dsw = devsw[i];

		for (dpl = HEAD(struct devpathl, dsw->d_pathl);
		    dpl != NULL;
		    dpl = dpl_next) {

			dpl_next = NEXT(struct devpathl, dpl);

			/* if this guy wasn't seen this time close him down */
			if (dpl->dpl_seen == FALSE) {
				debug(1, "dev_endconfig: closing dev %s\n",
				    dpl->dpl_path);
				(*dsw->d_close)(dpl->dpl_path);
				REMQUE(dsw->d_pathl, dpl);
				free(dpl->dpl_path);
				free(dpl);
			}
		}
	}
}


/*
 * Maintain a list of "paths" which a particular driver is using.
 * This allows us to just reprocess the config file then see what
 * isn't being used anymore.
 */
static void
dev_usepath(struct devsw *dsw, char *path)
{
	struct devpathl	*dpl;
	bool_t		found = FALSE;


	for (dpl = HEAD(struct devpathl, dsw->d_pathl);
	    dpl != NULL;
	    dpl = NEXT(struct devpathl, dpl)) {
		if (strcmp(dpl->dpl_path, path) == 0) {
			found = TRUE;
			dpl->dpl_seen = TRUE;
			break;
		}
	}
	if (found == FALSE) {
		dpl = (struct devpathl *)
		    calloc(1, sizeof (struct devpathl));
		dpl->dpl_path = strdup(path);
		dpl->dpl_seen = TRUE;
		INSQUE(dsw->d_pathl, dpl);
		debug(4, "dev_usepath: new path %s\n", path);
	}
}


/*
 * This interface is currently not well used.
 */
void
dev_error(struct ve_error *viee)
{
	struct devs	*dp = dev_getdp(viee->viee_dev);
	struct devsw	*dsw;


	if (dp != NULL) {
		dsw = dp->dp_dsw;
		(*dsw->d_error)(viee);
		dp->dp_vol->v_clue.c_error = viee;
		(void) action(ACT_ERROR, dp->dp_vol);
	}
}


/*
 * Given a dev, return the dp assocaited with it.
 */
struct devs *
dev_getdp(dev_t	dev)
{
	struct devs	*dp;


	ASSERT(dev != NODEV);
	ASSERT(dev != 0);

	dp = HEAD(struct devs, dev_q_hash[(u_int)dev % DEV_HASH_SIZE]);
	while (dp != NULL) {
		if (dp->dp_dev == dev) {
			return (dp);
		}
		dp = NEXT(struct devs, dp);
	}
	return (NULL);
}


/*
 * Given a dp, return TRUE if there is a piece of media in that
 * device, FALSE otherwise.
 */
bool_t
dev_present(struct devs *dp)
{
	if (dp->dp_vol != NULL) {
		return (TRUE);
	}
	return (FALSE);
}


/*
 * Given a dev, return the associated dsw.
 */
static struct devsw *
dev_getdsw(dev_t dev)
{
	struct devs	*dp;


	if (dev == NODEV) {
		return (NULL);
	}

	dp = dev_getdp(dev);
	ASSERT(dp != NULL);
	return (dp->dp_dsw);
}


/*
 * Check to see if a volume is read-only.
 */
bool_t
dev_rdonly(dev_t dev)
{
	struct devs *dp;


	if (dev == NODEV) {
		return (FALSE);
	}
	dp = dev_getdp(dev);
	if (dp != NULL) {
		return (dp->dp_writeprot);
	}
	return (FALSE);
}


/*
 * Create a new dp, which represents path.
 */
struct devs *
dev_makedp(struct devsw *dsw, char *path)
{
	struct devs	*dp;
	struct stat	sb;


	if (stat(path, &sb) < 0) {
		debug(1, "dev_makedp: %s; %m\n", path);
		return (NULL);
	}
	dp = (struct devs *)calloc(1, sizeof (struct devs));
	dp->dp_path = strdup(path);
	dp->dp_dev = sb.st_rdev;
	dp->dp_dsw = dsw;
	dp->dp_lock = (dp_vol_lock_t *)malloc(sizeof (dp_vol_lock_t));
	(void) mutex_init(&dp->dp_lock->dp_vol_vg_mutex, USYNC_THREAD, 0);
	(void) cond_init(&dp->dp_lock->dp_vol_vg_cv, USYNC_THREAD, 0);
	debug(15, "dev_makedp: just added mapping for %s (%d,%d)\n",
	    path, major(sb.st_rdev), minor(sb.st_rdev));
	/* add it to the hash table */
	INSQUE(dev_q_hash[(u_int)dp->dp_dev % DEV_HASH_SIZE], dp);
	return (dp);
}


/*
 * Free a dp.  Remove it from the hash queue and free storage associated
 * with it.  Assumes that the caller has already done the unhang.
 */
void
dev_freedp(struct devs *dp)
{
	REMQUE(dev_q_hash[dp->dp_dev % DEV_HASH_SIZE], dp);
	free(dp->dp_lock);
	free(dp);
}


/*
 * Do all the work associated with ejecting a volume, telling the
 * eject program the status, etc, etc.  This is called after all
 * the actions have run and returned a verdict.  The last check that
 * can change the results of our ejection is whether the media is
 * mounted with an unsafe filesystem.  This is done with unsafe_check().
 */
void
dev_eject(vol_t *v, bool_t ans)
{
	extern int		vol_fd;
	int			err;
	struct devs		*dp;
	struct devsw 		*dsw;
	struct vioc_eject	viej;
	dev_t			voldev;
	bool_t			force;


#ifdef	DEBUG
	debug(11, "dev_eject: ans = %s for %s\n", ans ? "TRUE" : "FALSE",
	    v->v_obj.o_name);
#endif
	dp = dev_getdp(v->v_basedev);
	if (dp == NULL) {
		debug(1, "volume %s already ejected!\n", v->v_obj.o_name);
		return;
	}

	dsw = dp->dp_dsw;

	force = v->v_ej_force;
	/*
	 * check to see if we have an "unsafe" file system mounted.
	 * Returns TRUE if volume is "unsafe" to eject.
	 */
	if ((ans != FALSE) && (force == FALSE)) {
		if (unsafe_check(v) != FALSE) {
			ans = FALSE;
		}
	}

	debug(1, "%sing ejection for \"%s\"\n", ans ? "approv" : "deny",
	    v->v_obj.o_name);

	viej.viej_unit = v->v_clue.c_volume;

	if (ans != FALSE) {
		viej.viej_state = VEJ_YES;
	} else {
		viej.viej_state = VEJ_NO;
	}

	if (force == FALSE) {
		if (ioctl(vol_fd, VOLIOCEJECT, &viej) < 0) {
			warning(gettext("ejection failed; %m\n"));
		}
	}

	/*
	 * Change our in progress flag...
	 */
	v->v_ej_inprog = FALSE;

	if ((ans == FALSE) && (force == FALSE)) {
		return;
	}

	voldev = v->v_basedev;

	if (dsw->d_eject != NULL) {
		debug(5, "dev_eject: calling dev-specific eject routine\n");
		(*dsw->d_eject)(dp);
	}

	/*
	 * Remove the mapping for the device.
	 */
	(void) dev_unmap(v);

	/*
	 * Update the database entry.
	 */
	change_location((obj_t *)v, "");
	(void) db_update((obj_t *)v);

	/*
	 * If this is a polling device, start up the polling
	 * again.
	 */
	if ((dsw->d_flags & D_POLL) && (dsw->d_poll != NULL)) {
		(*dsw->d_poll)(voldev);
	}
	/*
	 * Remove the volume from the name space.
	 */
	dev_unhangvol(dp);

	dp->dp_writeprot = FALSE;

	if (v->v_flags & V_RMONEJECT) {
		node_remove((obj_t *)v, TRUE, (u_int *)&err);
	}

	/* signal anything waiting */
	(void) mutex_lock(&dp->dp_lock->dp_vol_vg_mutex);
#ifdef	DEBUG
	debug(5, "dev_eject: signalling that eject's done on unit %d\n",
	    viej.viej_unit);
#endif
	(void) cond_broadcast(&dp->dp_lock->dp_vol_vg_cv);
	(void) mutex_unlock(&dp->dp_lock->dp_vol_vg_mutex);
}


/*
 * This is for the error cases...  Something bad has happened, so
 * we just spit the thing back out at the user.
 */
void
dev_hard_eject(struct devs *dp)
{
	int	fd = dev_getfd(dp->dp_dev);


	(void) ioctl(fd, DKIOCEJECT, 0);
}


/*
 * Clean up the devmap associated with this volume.
 */
bool_t
dev_devmapfree(vol_t *v)
{
	extern void	minor_clrvol(minor_t);
	u_int		i;



	if (v->v_devmap == NULL) {
		return (TRUE);
	}

	if (dev_unmap(v) == FALSE) {
		return (FALSE);
	}

	for (i = 0; i < v->v_ndev; i++) {
		/*
		 * If the driver still has a mapping for this minor
		 * number, we can't reuse it.  We just mark the
		 * minor number as being an orphan.
		 */
		if (v->v_flags & V_UNMAPPED) {
			minor_free(minor(v->v_devmap[i].dm_voldev));
		} else {
			minor_clrvol(minor(v->v_devmap[i].dm_voldev));
		}
		if (v->v_devmap[i].dm_path != NULL) {
			free(v->v_devmap[i].dm_path);
		}
	}
	v->v_ndev = 0;
	free(v->v_devmap);
	v->v_devmap = 0;
	return (TRUE);
}


/*
 * Build the devmap for this volume.
 */
void
dev_devmap(vol_t *v)
{
	struct devsw 	*dsw;
	int		n, p;
	u_long		fpart;
	struct stat	sb;


	dsw = dev_getdsw(v->v_basedev);

	fpart = v->v_parts;

	/*
	 * This can be considered an error case.  The device hasn't
	 * told us about any partitions, so we'll just use the default
	 * partition
	 */
	if (fpart == 0L) {
		debug(1, "dev_devmap: no partitions for %s (using s%d)\n",
		    v->v_obj.o_name, DEFAULT_PARTITION);
		if (v->v_devmap == NULL) {
			v->v_devmap = (devmap_t *)calloc(1, sizeof (devmap_t));
			v->v_ndev = 1;
			if (v->v_devmap[0].dm_voldev == 0) {
				v->v_devmap[0].dm_voldev = minor_alloc(v);
#ifdef	DEBUG
				debug(5,
				"dev_devmap: dm_voldev[0] set to (%d,%d)\n",
				    major(v->v_devmap[0].dm_voldev),
				    minor(v->v_devmap[0].dm_voldev));
#endif
			}
		}

		if ((dsw != NULL) && (dsw->d_devmap != NULL)) {
			(*dsw->d_devmap)(v, DEFAULT_PARTITION, 0);
			if (stat(v->v_devmap[0].dm_path, &sb) < 0) {
				debug(1, "dev_devmap: %s; %m\n",
				    v->v_devmap[0].dm_path);
				(void) dev_devmapfree(v);
				return;
			}
			/*
			 * We just store dm_realdev here to cache it
			 * so we don't spend our life doing stats of the
			 * path.
			 */
			v->v_devmap[0].dm_realdev = sb.st_rdev;
		}
		return;
	}

	/*
	 * Allocate our devmap.
	 */
	if (v->v_devmap == NULL) {
		v->v_devmap = (devmap_t *)calloc(V_MAXPART, sizeof (devmap_t));
		for (n = 0; n < (int)v->v_ndev; n++) {
			if (v->v_devmap[n].dm_voldev == 0) {
				v->v_devmap[n].dm_voldev = minor_alloc(v);
			}
		}
	}

	/*
	 * Have the driver tell us what device a partitular
	 * partition is.
	 */
	for (p = 0, n = 0; p < V_MAXPART; p++) {
		if (fpart & (1<<p)) {
			if ((dsw != NULL) && (dsw->d_devmap != NULL)) {
				(*dsw->d_devmap)(v, p, n);
				if ((v->v_devmap[n].dm_path == NULL) ||
				    (stat(v->v_devmap[n].dm_path, &sb) < 0)) {
					debug(1, "dev_devmap: %s; %m\n",
					    v->v_devmap[n].dm_path);
					(void) dev_devmapfree(v);
					return;
				}
				/*
				 * We just store dm_realdev here to cache it
				 * so we don't spend our life doing stats of
				 * the path.
				 */
				v->v_devmap[n].dm_realdev = sb.st_rdev;
			}
			n++;
		}
	}
}


/*
 * Load the devmap for a volume down into the vol driver.  If the
 * location of the volume hasn't been "confirmed", we bag out...
 * unless ndelay is set.  The ndelay business is to support non-blocking
 * opens.  This was required so that you could eject a volume without
 * having to read or write it first.
 */
bool_t
dev_map(vol_t *v, bool_t ndelay)
{
	extern int	vol_fd;			/* from /dev/volctl */
	struct vioc_map	vim;			/* for VOLIOCMAP ioctl */
	u_int		i;			/* dev linked list index */
	minor_t		volume;			/* minor dev # index */
	bool_t		res = FALSE;		/* resturn result */


#ifdef	DEBUG
	debug(11, "dev_map: entering for %s (ndelay = %s)\n",
	    v->v_obj.o_name, (ndelay ? "TRUE" : "FALSE"));
#endif

	if ((v->v_confirmed == FALSE) && (ndelay == FALSE)) {
		/* no location yet, and ??? (XXX: what is this) */
#ifdef	DEBUG
		debug(11, "dev_map: svcs not needed\n");
#endif
		goto dun;
	}

	/*
	 * always do this to ensure that dm_path is correct
	 */
	dev_devmap(v);

	/* scan all nodes for this volume */
	for (i = 0; i < v->v_ndev; i++) {

		volume = minor(v->v_devmap[i].dm_voldev);

		(void) memset(&vim, 0, sizeof (struct vioc_map));

		/* has the location been confirmed ?? */
		if (v->v_confirmed) {
			vim.vim_basedev = v->v_basedev;
			vim.vim_dev = v->v_devmap[i].dm_realdev;
			vim.vim_path = v->v_devmap[i].dm_path;
			vim.vim_pathlen = strlen(vim.vim_path);
			/* clear the missing flag */
			v->v_flags &= ~V_MISSING;
#ifdef	DEBUG
			debug(11, "dev_map: clearing missing flag, unit %d\n",
			    volume);
#endif
		} else {
			vim.vim_basedev = NODEV;
			vim.vim_dev = NODEV;
			vim.vim_path = NULL;
			vim.vim_pathlen = 0;
		}
		vim.vim_unit = volume;
		vim.vim_id = v->v_obj.o_id;

		/* check for read-only */
		if (dev_rdonly(v->v_basedev) || (v->v_flags & V_RDONLY)) {
			vim.vim_flags |= VIM_RDONLY;
			debug(5, "dev_map: set RDONLY flag for mapping\n");
		}

		/* the driver needs to know if this is a floppy device */
		if (strcmp(v->v_mtype, FLOPPY_MTYPE) == 0) {
			vim.vim_flags |= VIM_FLOPPY;
		}

#ifdef	DEBUG_MINOR
		debug(7, "dev_map: telling driver to MAP unit %d\n",
		    vim.vim_unit);
#endif
		if (ioctl(vol_fd, VOLIOCMAP, &vim) < 0) {
			debug(1, "dev_map: VOLIOCMAP; %m\n");
			goto dun;
		}

	}

	res = TRUE;
dun:
#ifdef	DEBUG
	debug(11, "dev_map: returning %s\n", res ? "TRUE" : "FALSE");
#endif
	return (res);
}


/*
 * Remove a mapping for a volume from the driver.  Normally called
 * on ejection.
 */
static bool_t
dev_unmap(vol_t *v)
{
	u_int		i;
	extern int	vol_fd;
	minor_t		volume;


	v->v_flags |= V_UNMAPPED;

	/* scan all nodes for this volume */
	for (i = 0; i < v->v_ndev; i++) {

		volume = minor(v->v_devmap[i].dm_voldev);

		/*
		 * the V_ENXIO flag used to be checked for, here, but
		 * that didn't allow the setting of "s-enxio" to have
		 * an immediate effect, so that's now done when the property
		 * is set
		 */

		/*
		 * if it's an unlabeled device, we must cancel
		 * any pending i/o, because we'll never be able
		 * to give it back to them.
		 */
		if (v->v_flags & V_UNLAB) {
#ifdef	DEBUG_MINOR
			debug(7,
			    "dev_unmap: telling driver to UNMAP minor# %d\n",
			    minor(v->v_devmap[0].dm_voldev));
#endif
			if (ioctl(vol_fd, VOLIOCCANCEL, &volume) < 0) {
				debug(1, "dev_unmap: cancel err on %s; %m\n",
				    v->v_obj.o_name);
			}
		}

		/*
		 * Do the actual unmapping.
		 */
		if (ioctl(vol_fd, VOLIOCUNMAP, &volume) != 0) {
			/*
			 * set the flag to say "don't reuse this minor
			 * number".  the purpose of this is to assign
			 * the minor number to some other piece of media
			 * while the driver is still mapping it (to
			 * return errors, for example.
			 *
			 * the minor_* code will garbage collect the
			 * minor numbers for us.
			 */
			v->v_flags &= ~V_UNMAPPED;
			debug(1, "dev_unmap: VOLIOCUNMAP (%d) of \"%s\"; %m\n",
			    vol_fd, v->v_obj.o_name);
		}
		v->v_devmap[i].dm_realdev = 0;
		free(v->v_devmap[i].dm_path);
		v->v_devmap[i].dm_path = 0;
	}
	v->v_confirmed = FALSE;
	change_location((obj_t *)v, "");
	return (TRUE);
}


/*
 * Built an arbitrary path in /vol/dev and return the vvnode pointing
 * to the lowest node.  This is used by the drivers who want to build
 * paths in /vol/dev and hang things off them.
 *
 * for each component in the path
 *	- check to see if we already have it
 *	- stat it in /dev
 *	- get the modes, et. al.
 *	- add it in.
 */
vvnode_t *
dev_dirpath(char *path)
{
	char		**ps;
	int		found = 0;
	int		comp;
	vvnode_t	*vn, *pvn;
	char		namebuf[MAXPATHLEN];
	char		devnamebuf[MAXPATHLEN];
	extern vvnode_t	*devroot;
	u_int		err;


#ifdef	DEBUG
	debug(11, "dev_dirpath: entering for \"%s\"\n", path);
#endif
	ps = path_split(path);
	for (comp = 0; ps[comp] != NULL; comp++) {
		if (strcmp(ps[comp], "dev") == 0) {
			found++;
			break;
		}
	}
	comp++;
	if (found == 0) {
		/* this should mostly be a debug aid */
		fatal(gettext("dev_dirpath: %s does not have 'dev' in it!\n"),
		    path);
	}
	(void) strcpy(namebuf, "dev/");
	(void) strcpy(devnamebuf, "/dev/");
	pvn = devroot;
	for (; ps[comp] != NULL; comp++) {
		mode_t		mode;
		uid_t		uid;
		gid_t		gid;
		dirat_t		*da;

		(void) strcat(namebuf, ps[comp]);
		(void) strcat(devnamebuf, ps[comp]);
		if ((vn = node_lookup(namebuf)) == NULL) {
			/*
			 * XXX: here's where we need to create the /dev
			 * pathname and stat it to get the modes, uid
			 * and gid.
			 */
			mode = DEFAULT_ROOT_MODE;
			uid = DEFAULT_TOP_UID;
			gid = DEFAULT_TOP_GID;

			da = node_mkdirat(ps[comp], uid, gid, mode);
			if (pvn == NULL) {
				/*
				 * yes, this is ugly and irritating,
				 * but devroot will not get set until the
				 * node_lookup the first time through.
				 */
				pvn = devroot;
			}
			vn = node_mkobj(pvn, (obj_t *)da, NODE_TMPID,  &err);
			if (err != 0) {
				debug(3, "dev_dirpath: err %d on %s of %s\n",
					err, ps[comp], path);
				break;
			}
		}
		pvn = vn;
		(void) strcat(devnamebuf, "/");
		(void) strcat(namebuf, "/");
	}
	path_freeps(ps);
	return (vn);
}


/*
 * Given a dp, associate a volume with it in the name space.  This
 * just makes the block and character nodes appear in the /vol/dev
 * part of the name space, as specified by the driver. (and, it also
 * makes the symlink in .../dev/aliases)
 */
void
dev_hangvol(struct devs *dp, vol_t *v)
{
	vvnode_t	*vn;
	u_int		err;
	u_int		flgs = 0;
	char		*path;


	if (v->v_flags & V_UNLAB) {
		flgs = NODE_TMPID;
	}

	if (dp->dp_rvn != NULL) {
		vn = node_mkobj(dp->dp_rvn, (obj_t *)v, NODE_CHR|flgs, &err);
		if (err != 0) {
			debug(1,
		"dev_hangvol: node_mkobj (chr) failed for \"%s\" (err = %d)\n",
			    v->v_obj.o_name, err);
		}
		if (dp->dp_symname != NULL) {
			path = path_make(vn);
			dp->dp_symvn = node_symlink(
			    dev_dirpath("/dev/aliases"), dp->dp_symname,
			    path, NODE_TMPID, NULL);
			free(path);
		}
	}

	if (dp->dp_bvn != NULL) {
		(void) node_mkobj(dp->dp_bvn, (obj_t *)v, NODE_BLK|flgs, &err);
		if (err != 0) {
			debug(1,
		"dev_hangvol: node_mkobj (blk) failed \"%s\" (err = %d)\n",
			    v->v_obj.o_name, err);
		}
	}

	dp->dp_vol = v;
}


/*
 * Remove any names in the name space that are associated with this
 * dp.
 */
void
dev_unhangvol(struct devs *dp)
{
	extern void	obj_free(obj_t *);
	vol_t		*v;
	obj_t		*obj;



#ifdef	DEBUG
	debug(7, "dev_unhangvol: entering for \"%s\"\n",
	    dp->dp_vol->v_obj.o_name);
#endif
	if (dp->dp_rvn != NULL) {
		node_unlink(dp->dp_rvn->vn_child);
	}

	if (dp->dp_bvn != NULL) {
		node_unlink(dp->dp_bvn->vn_child);
	}

	if (dp->dp_symvn != NULL) {
		obj = dp->dp_symvn->vn_obj;
		node_unlink(dp->dp_symvn);
		obj_free(obj);
		dp->dp_symvn = NULL;
	}

	v = dp->dp_vol;
	if ((v != NULL) && (v->v_flags & V_UNLAB)) {
		/* free the minor number used by this device */
		(void) dev_devmapfree(v);
		/* free up the unlabeled name */
		free(v->v_obj.o_name);
		if (v->v_devmap != NULL) {
			free(v->v_devmap);
		}
		if (v->v_location != NULL) {
			free(v->v_location);
		}
		free(v);
	}
	dp->dp_vol = 0;
}


/*
 * dev_rename: take care of aliases if we have a volume and
 * if it's been renamed.
 */
void
dev_rename(vol_t *v)
{
	extern void	obj_free(obj_t *);
	struct devs	*dp;
	char		*path;
	obj_t		*obj;


	if ((v->v_basedev == NODEV) || (v->v_basedev == 0)) {
		return;
	}

	if ((dp = dev_getdp(v->v_basedev)) == NULL) {
		debug(1, "dev_rename: basedev 0x%x, no dp!\n", v->v_basedev);
		return;
	}
	if ((dp->dp_symvn != NULL) &&
	    (dp->dp_rvn != NULL) &&
	    (dp->dp_rvn->vn_child != NULL)) {
		obj = dp->dp_symvn->vn_obj;
		node_unlink(dp->dp_symvn);
		obj_free(obj);
		path = path_make(dp->dp_rvn->vn_child);
		dp->dp_symvn = node_symlink(
		    dev_dirpath("/dev/aliases"), dp->dp_symname,
		    path, 0, NULL);
		free(path);
	}
}


/*
 * Unlabeled media was just inserted in *dp, create a "fake"
 * vol_t to represent it.
 */
vol_t *
dev_unlabeled(struct devs *dp, enum laread_res rres, label *la)
{
	vol_t	*v;


	v = (vol_t *)calloc(1, sizeof (vol_t));

	switch (rres) {
	case L_UNRECOG:
		v->v_obj.o_name = strdup(DEFAULT_UNLAB);
		break;
	case L_UNFORMATTED:
		v->v_obj.o_name = strdup(DEFAULT_UNFORMAT);
		break;
	case L_NOTUNIQUE:
		v->v_obj.o_name = strdup(DEFAULT_NOTUNIQUE);
		break;
	default:
		v->v_obj.o_name = strdup("unknown_label_type");
		debug(1, "dev_unlabeled error: laread_res == %d\n", rres);
		break;
	}
	v->v_obj.o_dir = strdup("");
	v->v_obj.o_type = VV_CHR;
	v->v_obj.o_uid = default_uid;
	v->v_obj.o_gid = default_gid;
	v->v_obj.o_mode = DEFAULT_MODE;
	v->v_obj.o_atime = current_time;
	v->v_obj.o_ctime = current_time;
	v->v_obj.o_mtime = current_time;
	v->v_mtype = strdup(dp->dp_dsw->d_mtype);
	v->v_flags |= V_UNLAB;
	v->v_basedev = NODEV;

	/* set up properties */
	if (dp->dp_dsw->d_flags & D_RMONEJECT) {
		v->v_flags |= V_RMONEJECT;
	}
	if (dp->dp_dsw->d_flags & D_MEJECTABLE) {
		v->v_flags |= V_MEJECTABLE;
	}

	/* ensure the "label type" index is "none" */
	la->l_type = -1;

	/* return pointer to vol structure created */
	return (v);
}


/*
 * Find the driver that controls this device and ask it to check
 * and see if something is there.  The driver is responsible for
 * generating the check event.
 * dev_check returns:
 * 0 if it didn't find anything
 * 1 if it found something and we already knew about it
 * 2 if it found something and we generated an insert event
 */
int
dev_check(dev_t dev)
{
	struct devs	*dp;
	int		rval = 0, nrval;
	int		i;


	if (dev == NODEV) {
		/*
		 * wildcard -- loop through all the
		 * devices that have a check routine.  If anyone
		 * returns true, we return true.  It's too bad we
		 * have to wander through the hash table to iterate
		 * through the devs... oh well.
		 */
		for (i = 0; i < DEV_HASH_SIZE; i++) {
			dp = HEAD(struct devs, dev_q_hash[i]);
			while (dp != NULL) {
				if (dp->dp_dsw->d_check != NULL) {
					debug(4,
					    "dev_check: check device %d.%d\n",
					    major(dp->dp_dev),
					    minor(dp->dp_dev));
					nrval = (*dp->dp_dsw->d_check)(dp);
					debug(10, "dev_check: check -> %d\n",
					    nrval);
					if (nrval != 0) {
						dp->dp_checkresp = TRUE;
					}
					if (nrval > rval) {
						rval = nrval;
					}
				}
				dp = NEXT(struct devs, dp);
			}
		}
		return (rval);
	}

	dp = dev_getdp(dev);
	if (dp == NULL) {
		warning(gettext("check device %d.%d: device not managed\n"),
			major(dev), minor(dev));
		return (0);
	}
	debug(4, "dev_check: check device %d.%d\n", major(dev), minor(dev));
	if (dp->dp_dsw->d_check != NULL) {
		rval = (*dp->dp_dsw->d_check)(dp);
	}

	if (rval != 0) {
		dp->dp_checkresp = TRUE;
	}

	debug(10, "dev_check: check -> %d\n", rval);

	return (rval);
}


/*
 * Return true if a device is being managed by volume management.
 */
int
dev_inuse(dev_t dev)
{

	if (dev == NODEV) {
		warning(gettext("inuse; NODEV: device not managed\n"));
		return (FALSE);
	}

	if (dev_getdp(dev) == NULL) {
		warning(gettext("inuse; %d.%d: device not managed\n"),
			major(dev), minor(dev));
		return (FALSE);
	}
	return (TRUE);
}


/*
 * Return the /dev pathname given the symbolic name of a device.
 */
char *
symname_to_dev(char *symname)
{
	int		i;
	struct devs	*dp;
	char		*res = NULL;


	for (i = 0; (i < DEV_HASH_SIZE) && (res == NULL); i++) {
		for (dp = HEAD(struct devs, dev_q_hash[(u_int)i]);
		    dp != NULL;
		    dp = NEXT(struct devs, dp)) {

			/* just in case something doesn't have any symname */
			if (dp->dp_symname == NULL) {
				continue;	/* try the next one */
			}

			if (strcmp(dp->dp_symname, symname) == 0) {
				/* found it */
				res = dp->dp_path;
				break;		/* get outta here */
			}
		}
	}
	return (res);
}
