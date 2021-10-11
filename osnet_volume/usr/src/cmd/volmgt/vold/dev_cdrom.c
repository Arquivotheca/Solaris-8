/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dev_cdrom.c	1.67	99/03/08 SMI"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mkdev.h>
#include	<sys/dkio.h>
#include	<sys/cdio.h>
#include	<sys/vtoc.h>
#if defined(_FIRMWARE_NEEDS_FDISK)
#include	<sys/dktp/fdisk.h>
#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */
#include	<errno.h>
#include	<signal.h>
#include	<string.h>
#include	<dirent.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<thread.h>
#include	<synch.h>
#include	"vold.h"

#if defined(_FIRMWARE_NEEDS_FDISK)
#undef _FIRMWARE_NEEDS_FDISK
#endif

#define	S_ID 's'	/* using to ident. slice in a dev name */

#if !defined(P0_WA)
#define	P0_WA	1	/* want to always check this if fdisk comes to sparc */
#endif
/*
 * following defs are used to support the sparc solaris S partitions
 * and are used to support the P partitions found on x86 Solaris
 * and that may be used on sparc if it should support fdisk.
 */
#define	CDMAX_P_PART	5	/* max number of P partitions */
#define	CDMAX_S_PART	V_MAXPART /* max number of S partitions */
#define	S_AND_P		2	/* number of partition types supported */
#define	S_TYPE		0	/* defn for S partitions */
#define	P_TYPE		1	/* defn for P partitions */
/*
 * determine which type of partition have the greatest
 * number. Currently this is always S Partitions, but to
 * be sure any changes in the future does not break this
 * code.
 */
#if CDMAX_S_PART > CDMAX_P_PART
#define	CDMAX_PARTS	CDMAX_S_PART
#else
#define	CDMAX_PARTS	CDMAX_P_PART
#endif

static bool_t	cdrom_use(char *, char *);
static bool_t	cdrom_error(struct ve_error *);
static int	cdrom_getfd(dev_t);
#ifdef CDROM_POLL
static void	cdrom_poll(dev_t);
#endif
static void	cdrom_devmap(vol_t *, int, int);
static void	cdrom_close(char *path);
static void	cdrom_thread_wait(struct devs *dp);
static bool_t	cdrom_testpath(char *);
static int	part_to_hack(struct vtoc *);


static struct devsw cdromdevsw = {
	cdrom_use,		/* d_use */
	cdrom_error,		/* d_error */
	cdrom_getfd,		/* d_getfd */
#ifdef CDROM_POLL
	cdrom_poll,		/* d_poll */
#else
	NULL,			/* d_poll */
#endif
	cdrom_devmap,		/* d_devmap */
	cdrom_close,		/* d_close */
	NULL, 			/* d_eject */
	NULL, 			/* d_find */
	NULL,			/* d_check */
	CDROM_MTYPE,		/* d_mtype */
	DRIVE_CLASS,		/* d_dtype */
	D_RDONLY|D_POLL,	/* d_flags */
	(uid_t)0,		/* d_uid */
	(gid_t)0,		/* d_gid */
	(mode_t)0,		/* d_mode */
	cdrom_testpath		/* d_test */
};



bool_t
dev_init()
{
	extern void	dev_new(struct devsw *dsw);

	dev_new(&cdromdevsw);
	return (TRUE);
}


static struct cd_priv {
	char	*cd_rawpath[S_AND_P][CDMAX_PARTS];
	mutex_t	cd_killmutex;	/* mutex for killing thread */
#ifdef CDROM_POLL
	mutex_t	cd_waitmutex;	/* mutex for waiting until ejection */
	cond_t	cd_waitcv;
	bool_t	cd_inserted;
#endif
	int	cd_tid;		/* thread id */
	int	cd_fd[S_AND_P][CDMAX_PARTS];
	int	cd_p_type;	/* id's what partition type we are accessing */
	int	cd_defpart;
#ifdef	P0_WA
	char	*cd_blk_p0_path;		/* the p0 blk name */
	int	cd_p0_part;
#endif
#if defined(_FIRMWARE_NEEDS_FDISK)
	int	cd_raw_pfd[FD_NUMPART+1];	/* char fdisk-partition fds */
#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */
};

#define	CDROM_NAMEPROTO_DEFD	"%sd0s%d"
#define	CDROM_PBASEPART		0
#define	CDROM_SBASEPART		DEFAULT_PARTITION

#define	CDROM_NAMEPROTO_P	"%sp%d"
#define	CDROM_NAMEPROTO_S	"%ss%d"

#ifdef	P0_WA
#define	CDROM_NAMEPROTO_P_ALL	0
#endif

/*
 * thread stack size
 */
#define	CDROM_STKSIZE	(32 * 1024)	/* 32k! */

/*
 * cdrom_use -- this routine expects either a raw or block path that
 *		points to a cdrom.  It further expects that the supplied
 *		path starts with "/dev/dsk/" for block devices or
 *		"/dev/rdsk" for character devices.  It finds
 *		the complimentary device by switching this segment, e.g.
 *		if you supply "/dev/dsk/c0t6" for a group of block devices,
 *		then this routine will expect the raw devices to be at
 *		"/dev/rdsk/c0t6".
 *
 *		A thread is created which will handle this new group of
 *		interfaces to a device. A devs struct is filled in and
 *		passed on to the thread.
 */
static bool_t
cdrom_use(char *path, char *symname)
{
	struct stat	statbuf;
	char		namebuf1[MAXPATHLEN];
	char		full_path[MAXPATHLEN+1];
	char		*path_trunc = path;
	char		namebuf[MAXPATHLEN];
	struct devs	*dp;	/* XXX: can we pass this to a thread ??? */
	struct cd_priv 	*cdp;
	char		*s;
	char		*p;
	vvnode_t	*bvn;
	vvnode_t	*rvn;
	int		i, n;



	info(gettext("cdrom_use: %s\n"), path);

#if defined(_FIRMWARE_NEEDS_FDISK)
	debug(5, "Firmware needs fdisk fix ****\n");
#else
	debug(5, "Firmware does not need fdisk fix ***\n");
#endif

	/*
	 * we don't do an open for the cdrom because it returns ENODEV
	 * if there isn't a device there.  Instead, we just stat the
	 * device and make sure it's there and is a reasonable type.
	 */

	/* just take a path if they hand it to us. */
	if (stat(path, &statbuf) < 0) {
		/*
		 * we can accept a path of the form:
		 *
		 * 	/dev/{dsk,rdsk}/cNtN
		 *
		 * we fill in the rest by appending "d0sN"
		 */
		(void) sprintf(full_path, CDROM_NAMEPROTO_DEFD, path,
		    CDROM_SBASEPART);
		if (stat(full_path, &statbuf) < 0) {
			/* can't even find it with "d0sN" appended! */
			debug(1, "cdrom: %s; %m\n", full_path);
			return (FALSE);
		}
	} else {
		/*
		 * the supplied path is complete -- truncate at the "slice"
		 * part of the name
		 *
		 * XXX: assume all CD-ROM pathnames end in "sN"
		 */
		(void) strcpy(full_path, path);
		if ((s = strrchr(path, S_ID)) != 0) {
			/* XXX: should make sure a slice number follows */
			*s = '\0';		/* truncate at the "sN" */
		} else {
			/* the full path didn't have an "s" in it! */
			warning(gettext("cdrom: %s is an invalid path\n"),
			    full_path);
			return (FALSE);
		}
	}

	/*
	 * Check to see if this guy is already configured.
	 */
	if (dev_getdp(statbuf.st_rdev)) {
		debug(1, "cdrom %s already in use\n", full_path);
		return (FALSE);
	}

	/*
	 * Check the modes to make sure that the path is either
	 * a block or a character device
	 */
	if (!S_ISCHR(statbuf.st_mode) && !S_ISBLK(statbuf.st_mode)) {
		warning(gettext(
		    "cdrom: %s not block or char device (mode 0x%x)\n"),
		    full_path, statbuf.st_mode);
		return (FALSE);
	}

	/* create en "empty" 'cd-private' data struct */
	cdp = (struct cd_priv *)calloc(1, sizeof (struct cd_priv));
	/* n accounts for P and S partitions */
	for (n = 0; n < S_AND_P; n++) {
		/* i accounts for max partitions, different between */
		/* S and P so with P we loose a little memory */
		for (i = 0; i < CDMAX_PARTS; i++) {
			cdp->cd_fd[n][i] = -1;
		}
	}
	cdp->cd_defpart = -1;
	/*
	 * we default to S types as a default, this will change
	 * if we later find valid instances of c#t#d#p#
	 */
	cdp->cd_p_type = S_TYPE;
#ifdef	P0_WA
	cdp->cd_p0_part = -1;
#endif

	/* stick some good stuff in the device hierarchy */
	if ((s = strstr(path_trunc, "rdsk")) != 0) {

		/* he gave us a raw path (i.e. "rdsk" in it) */

		/* save a pointer to the raw vv-node */
		rvn = dev_dirpath(path_trunc);

		/* create the names for rawpath for type p partitions */
		for (i = 0; i < CDMAX_P_PART; i++) {
			(void) sprintf(namebuf1, CDROM_NAMEPROTO_P,
			    path_trunc, i);
			debug(9, "CDROM_NAMEPROTO=%s; path_trunc=%s\n",
					CDROM_NAMEPROTO_P, path_trunc);
			cdp->cd_rawpath[P_TYPE][i] = strdup(namebuf1);
			debug(9, "namebuf1=%s; rawpth=%s\n",
				namebuf1, cdp->cd_rawpath[P_TYPE][i]);
		}

		/* create the names for rawpath for type s partitions */
		for (i = 0; i < CDMAX_S_PART; i++) {
			(void) sprintf(namebuf1, CDROM_NAMEPROTO_S,
			    path_trunc, i);
			debug(9, "CDROM_NAMEPROTO_S=%s; path_trunc=%s\n",
					CDROM_NAMEPROTO_S, path_trunc);
			cdp->cd_rawpath[S_TYPE][i] = strdup(namebuf1);
			debug(9, "namebuf1=%s; rawpth=%s\n",
				namebuf1, cdp->cd_rawpath[S_TYPE][i]);
		}

		/* get the block path now from the raw one */

		/* skip past "rdsk/" */
		if ((p = strchr(s, '/')) != 0) {
			p++;
			(void) sprintf(namebuf, "/dev/dsk/%s", p);
		} else {
			/* no slash after rdsk? */
			debug(1, "cdrom: malformed pathname '%s'\n",
			    path_trunc);
			/* what else can we do? */
			(void) strcpy(namebuf, path_trunc);
		}

		/* get the block vv-node */
		bvn = dev_dirpath(namebuf);

#ifdef	P0_WA
		/* set up the p0 block pathname */
		(void) sprintf(namebuf1, CDROM_NAMEPROTO_P, namebuf,
		    CDROM_NAMEPROTO_P_ALL);
		cdp->cd_blk_p0_path = strdup(namebuf1);
		debug(5, "cdp->cd_blk_p0_path = %s\n", cdp->cd_blk_p0_path);
#endif

	} else if (s = strstr(path_trunc, "dsk")) {

		/* he gave us the block path */

		/* save pointer to block vv-node */
		bvn = dev_dirpath(path_trunc);

#ifdef	P0_WA
		(void) sprintf(namebuf1, CDROM_NAMEPROTO_P, path_trunc,
		    CDROM_NAMEPROTO_P_ALL);
		cdp->cd_blk_p0_path = strdup(namebuf1);
		debug(6, "cdp->cd_blk_p0_path = %s\n", cdp->cd_blk_p0_path);
#endif

		/* skip past "dsk/" */
		if ((p = strchr(s, '/')) != 0) {
			p++;
			(void) sprintf(namebuf, "/dev/rdsk/%s", p);
		} else {
			/* no slash after "dsk"? */
			debug(1, "cdrom: malformed path name '%s'\n", path);
			/* what else can we do? */
			(void) strcpy(namebuf, path_trunc);
		}

		/* save a pointer to the raw vv-node */
		rvn = dev_dirpath(namebuf);

		/* create the names for rawpath for type p partitions */
		for (i = 0; i < CDMAX_P_PART; i++) {
			(void) sprintf(namebuf1, CDROM_NAMEPROTO_P,
			    namebuf, i);
			debug(9, "CDROM_NAMEPROTO=%s; namebuf=%s\n",
					CDROM_NAMEPROTO_P, namebuf);
			cdp->cd_rawpath[P_TYPE][i] = strdup(namebuf1);
			debug(9, "namebuf1=%s; rawpth=%s\n",
				namebuf1, cdp->cd_rawpath[P_TYPE][i]);
		}

		/* create the names for rawpath for type s partitions */
		for (i = 0; i < CDMAX_S_PART; i++) {
			(void) sprintf(namebuf1, CDROM_NAMEPROTO_S,
			    namebuf, i);
			debug(9, "CDROM_NAMEPROTO_S=%s; namebuf=%s\n",
					CDROM_NAMEPROTO_S, namebuf);
			cdp->cd_rawpath[S_TYPE][i] = strdup(namebuf1);
			debug(9, "namebuf1=%s; rawpth=%s\n",
				namebuf1, cdp->cd_rawpath[S_TYPE][i]);
		}


	} else {
		debug(1, "cdrom: malformed path name '%s'\n", path_trunc);
		return (FALSE);
	}

#if	defined(P0_WA) && defined(DEBUG)
		debug(6, "cdrom_use: p0 block path is \"%s\"\n",
		    cdp->cd_blk_p0_path);
#endif

	if ((dp = dev_makedp(&cdromdevsw,
	    cdp->cd_rawpath[S_TYPE][CDROM_SBASEPART])) == NULL) {
		debug(1, "cdrom_use: dev_makedp failed for %s\n",
		    cdp->cd_rawpath[S_TYPE][CDROM_SBASEPART]);
		return (FALSE);
	}

#if defined(_FIRMWARE_NEEDS_FDISK)
	/*
	 * serious hackery --  open the p? interfaces (so others can't
	 *	get around us)
	 */
	cdrom_open_exclusive(cdp, namebuf, path);
#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */

	dp->dp_priv = (void *)cdp;		/* ptr to our private data */
	dp->dp_writeprot = TRUE;		/* always */
	dp->dp_symname = strdup(symname);	/* symbolic name */
	dp->dp_bvn = bvn;			/* ptr to block vv-node */
	dp->dp_rvn = rvn;			/* ptr to raw vv-node */

	(void) mutex_init(&cdp->cd_killmutex, USYNC_THREAD, 0);
#ifdef	CDROM_POLL
	(void) mutex_init(&cdp->cd_waitmutex, USYNC_THREAD, 0);
	(void) cond_init(&cdp->cd_waitcv, USYNC_THREAD, 0);
#endif
	if (thr_create(0, CDROM_STKSIZE,
	    (void *(*)(void *))cdrom_thread_wait, (void *)dp, THR_BOUND,
	    (thread_t *)&cdp->cd_tid) < 0) {
		warning(gettext("cdrom thread create failed; %m\n"));
		return (FALSE);
	}
#ifdef	DEBUG
	debug(6, "cdrom_use: cdrom_thread_wait id %d created\n", cdp->cd_tid);
#endif
	return (TRUE);
}


#ifdef CDROM_POLL

/*ARGSUSED*/
static void
cdrom_poll(dev_t dev)
{
	struct devs	*dp;
	struct cd_priv	*cdp;


	dp = dev_getdp(dev);
	ASSERT(dp != NULL);
	cdp = (struct cd_priv *)dp->dp_priv;
	(void) mutex_lock(&cdp->cd_waitmutex);
	cdp->cd_inserted = FALSE;
	(void) cond_broadcast(&cdp->cd_waitcv);
	(void) mutex_unlock(&cdp->cd_waitmutex);
}
#endif	/* CDROM_POLL */


/*ARGSUSED*/
static void
cdrom_devmap(vol_t *v, int part, int off)
{
	struct devs	*dp;
	struct cd_priv	*cdp;

	debug(9, "Entering cdrom_devmap: part = %d, off = %d\n", part, off);
	dp = dev_getdp(v->v_basedev);
	cdp = (struct cd_priv *)dp->dp_priv;

	/*
	 * Well for P partitions this is a hack!
	 * What we do is mount p0 as s2 under /vol/dev/.../rdsk
	 * and /vol/dev/aliases/cdrom#
	 * HSFS can be mounted only from p0, the name change is
	 * for historical reasons
	 */
#ifdef DEBUG
	debug(6, "do we hack? cdp->cd_p0_part = %d, part = %d\n",
		cdp->cd_p0_part, part);
#endif
	if (cdp->cd_p0_part == part) {
		/* hack! use p0 instead of the requested slice */
		v->v_devmap[off].dm_path =
			strdup(cdp->cd_rawpath[P_TYPE][CDROM_PBASEPART]);
#ifdef DEBUG
		debug(6, "cdrom_devmap: hacking path for p0 workaround\n");
#endif
	} else {
		/* return the actual 's' slice requested */
		v->v_devmap[off].dm_path =
			strdup(cdp->cd_rawpath[S_TYPE][part]);
	}

#ifdef	DEBUG
	debug(6, "++++++ v->v_devmap[off].dm_path = %s\n",
		v->v_devmap[off].dm_path);
	debug(5, "cdrom_devmap460: part = %d\n", part);
	debug(9, "cdrom_devmap: returning (part %d, off %d): \"%s\"\n",
	    part, off, v->v_devmap[off].dm_path);
#endif
}


static int
cdrom_getfd(dev_t dev)
{
	struct devs	*dp;
	struct cd_priv	*cdp;

	dp = dev_getdp(dev);
	ASSERT(dp != NULL);
	cdp = (struct cd_priv *)dp->dp_priv;
	ASSERT(cdp->cd_defpart != -1);
	ASSERT(cdp->cd_fd[cdp->cd_p_type][cdp->cd_defpart] != -1);
	return (cdp->cd_fd[cdp->cd_p_type][cdp->cd_defpart]);
}


/*ARGSUSED*/
static bool_t
cdrom_error(struct ve_error *vie)
{
	debug(1, "cdrom_error\n");
	return (TRUE);
}


/*
 * State that must be cleaned up:
 *	name in the name space
 *	the "dp"
 *	any pointers to the media
 *	eject any existing media
 *	the priv structure
 */

/*
 * XXX: a bug still exists here.  we have a thread polling on this
 * XXX: device in the kernel, we need to get rid of this also.
 * XXX: since we're going to move the waiter thread up to the
 * XXX: user level, it'll be easier to kill off as part of the
 * XXX: cleanup of the device private data.
 */
static void
cdrom_close(char *path)
{
	char		namebuf[MAXPATHLEN];
	struct	stat	sb;
	struct devs	*dp;
	struct cd_priv	*cdp;
	int		i, n;



	debug(1, "cdrom_close %s\n", path);

	(void) sprintf(namebuf, CDROM_NAMEPROTO_S, path, CDROM_SBASEPART);
	if (stat(namebuf, &sb) < 0) {
		warning(gettext("cdrom_close: %s; %m\n"), namebuf);
		return;
	}

	dp = dev_getdp(sb.st_rdev);
	if (dp == NULL) {
		debug(1, "cdrom_close: %s not in use\n", path);
		return;
	}

	/* get our private data */
	cdp = (struct cd_priv *)dp->dp_priv;

	/*
	 * Take care of the listner thread.
	 */
	(void) mutex_lock(&cdp->cd_killmutex);
	(void) thr_kill(cdp->cd_tid, SIGUSR1);
	(void) mutex_unlock(&cdp->cd_killmutex);
	(void) thr_join(cdp->cd_tid, 0, 0);
	debug(1, "cdrom thread id %d reaped (killed/joined)\n", cdp->cd_tid);

	/*
	 * If there is a volume inserted in this device...
	 */
	if (dp->dp_vol) {
		/*
		 * Clean up the name space and the device maps
		 * to remove references to any volume that might
		 * be in the device right now.
		 * This crap with the flags is to keep the
		 * "poll" from being relaunched by this function.
		 * yes, its a hack and there should be a better way.
		 */
		if (dp->dp_dsw->d_flags & D_POLL) {
			dp->dp_dsw->d_flags &= ~D_POLL;
			dev_eject(dp->dp_vol, TRUE);
			dp->dp_dsw->d_flags |= D_POLL;
		} else {
			dev_eject(dp->dp_vol, TRUE);
		}

		/* do the eject work */
		(void) ioctl(cdp->cd_fd[cdp->cd_p_type][CDROM_SBASEPART],
			DKIOCEJECT, 0);
	}

	/*
	 * Clean up the names in the name space.
	 */
	node_unlink(dp->dp_bvn);
	node_unlink(dp->dp_rvn);

	/*
	 * free the private data we've allocated.
	 */
	for (n = 0; n < S_AND_P; n++) {
		for (i = 0; i < CDMAX_PARTS; i++) {
			if (cdp->cd_rawpath[n][i]) {
				free(cdp->cd_rawpath[n][i]);
			}
			if (cdp->cd_fd[n][i] != -1) {
				(void) close(cdp->cd_fd[n][i]);
			}
		}
	}
#if defined(_FIRMWARE_NEEDS_FDISK)
	for (i = 0; i < (FD_NUMPART+1); i++) {
		if (cdp->cd_raw_pfd[i]) {
			(void) close(cdp->cd_raw_pfd[i]);
		}
	}
#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */
	free(cdp);

	/*
	 * Free the dp, so no one points at us anymore.
	 */
	dev_freedp(dp);
}


/*
 * return the numnber of the slice that starts at zero and maps the largest
 *	portion of the device.  if none found return a -1
 */
static int
part_to_hack(struct vtoc *v)
{
	int	i;
	int	part_no = -1;
	int	part_start = -1;
	int	part_size = 0;
	int64_t	sz;
	int64_t	st;


	/* scan for lowest starting part that has biggest chunk */
	for (i = 0; i < CDMAX_S_PART; i++) {

		/* get size and start, ignoring this slice if no size */
		if ((sz = v->v_part[i].p_size) <= 0) {
			continue;
		}
		st = v->v_part[i].p_start;

		/*
		 * 3 possible cases of choosing this partition over
		 *	our previous best:
		 *	-> we don't have a previous best
		 *	-> this part starts earlier than previous best
		 *	-> this part starts at same place but is larger
		 */
		if ((part_start < 0) ||
		    (st < part_start) ||
		    ((st == part_start) && (sz > part_size))) {
			part_start = st;
			part_size = sz;
			part_no = i;
			continue;
		}
	}

	/* return part found (or -1 if none) */
	return (part_no);
}


static void
cdrom_thread_wait(struct devs *dp)
{
	extern void	vol_event(struct vioc_event *);
#ifdef	DEBUG
	static char	*state_to_str(enum dkio_state);
#endif

	extern int	vold_running;
	extern cond_t 	running_cv;
	extern mutex_t	running_mutex;

	int		fd = -1;
	struct cd_priv 	*cdp = (struct cd_priv *)dp->dp_priv;
	struct vioc_event vie;
	struct vtoc	vtoc;
	enum dkio_state cd_state;
	int		i, n;
	struct dk_cinfo	dkc;



	(void) mutex_lock(&running_mutex);
	while (vold_running == 0) {
		(void) cond_wait(&running_cv, &running_mutex);
	}
	(void) mutex_unlock(&running_mutex);

	dp->dp_writeprot = 1;

	/*
	 * need to check for p partitions first
	 * while these are only associated with x86 they may
	 * be on sparc in the future (sparc supporting fdisk)
	 */
	for (i = 0; i < CDMAX_P_PART; i++) {
		debug(1, "cdrom_thread_wait: opening \"%s\" RDONLY ...\n",
		    cdp->cd_rawpath[P_TYPE][i]);
		if ((fd = open(cdp->cd_rawpath[P_TYPE][i],
		    O_RDONLY|O_NONBLOCK|O_EXCL)) < 0) {
			debug(1, "cdrom: %s; %m\n",
				cdp->cd_rawpath[P_TYPE][i]);
			break;
		}
		(void) fcntl(fd, F_SETFD, FD_CLOEXEC);	/* close-on-exec */
		cdp->cd_fd[P_TYPE][i] = fd;
	}


	for (i = 0; i < CDMAX_S_PART; i++) {
		debug(1, "cdrom_thread_wait: opening \"%s\" RDONLY ...\n",
		    cdp->cd_rawpath[S_TYPE][i]);
		if ((fd = open(cdp->cd_rawpath[S_TYPE][i],
		    O_RDONLY|O_NONBLOCK|O_EXCL)) < 0) {
			noise("cdrom: %s; %m\n", cdp->cd_rawpath[S_TYPE][i]);
			goto errout;
		}
		(void) fcntl(fd, F_SETFD, FD_CLOEXEC);	/* close-on-exec */
		cdp->cd_fd[S_TYPE][i] = fd;
	}

	/* Check to make sure device is a CD-ROM, use s2 part */
	if (ioctl(cdp->cd_fd[S_TYPE][CDROM_SBASEPART],
			DKIOCINFO, &dkc) < 0) {
		noise("cdrom: %s DKIOCINFO; %m\n",
			cdp->cd_rawpath[S_TYPE][CDROM_SBASEPART]);
		goto errout;
	}
	if (dkc.dki_ctype != DKC_CDROM) {
		noise("cdrom: %s is not a CD-ROM drive\n",
			cdp->cd_rawpath[CDROM_SBASEPART]);
		goto errout;
	}

	cd_state = DKIO_NONE;

	/*
	 * the base partition number is dependent on if we have
	 * a p partition(0) or a s partition(2)
	 * ALWAYS check for P0 first to set defaults
	 */
	if (cdp->cd_fd[P_TYPE][0] >= 0) {
		cdp->cd_p_type = P_TYPE;
		cdp->cd_defpart = CDROM_PBASEPART;
	} else {
		cdp->cd_p_type = S_TYPE;
		cdp->cd_defpart = CDROM_SBASEPART;
	}

	/*CONSTCOND*/
	while (1) {

		fd = cdp->cd_fd[cdp->cd_p_type][cdp->cd_defpart];

		/*
		 * this ioctl blocks until state changes.
		 */
#ifdef	DEBUG
		debug(3,
		    "cdrom_thread_wait: ioctl(DKIOCSTATE, \"%s\") on \"%s\"\n",
		    state_to_str(cd_state),
		    cdp->cd_rawpath[cdp->cd_p_type][cdp->cd_defpart]);
#else
		debug(3, "cdrom_thread_wait: ioctl(DKIOCSTATE) on \"%s\"\n",
		    cdp->cd_rawpath[cdp->cd_p_type][cdp->cd_defpart]);
#endif
		if (ioctl(fd, DKIOCSTATE, &cd_state) < 0) {
			debug(1, "cdrom: DKIOCSTATE of \"%s\"; %m\n",
			    cdp->cd_rawpath[cdp->cd_p_type][cdp->cd_defpart]);
			if (errno == ENOTTY) {
				goto errout;
			}
			(void) sleep(1);
			continue;
		}
#ifdef	DEBUG
		debug(5, "cdrom_thread_wait: new state = \"%s\"\n",
		    state_to_str(cd_state));
#endif
		if (cd_state == DKIO_NONE) {
			continue;		/* steady state -- ignore */
		}

		(void) memset(&vie, 0, sizeof (struct vioc_event));

		(void) mutex_lock(&cdp->cd_killmutex);
		/*
		 * We have media in the drive
		 */
		if (cd_state == DKIO_INSERTED) {

			/*
			 * If we already know about the media in the
			 * device, just ignore the information.
			 */
			if (dp->dp_vol != NULL) {
				(void) mutex_unlock(&cdp->cd_killmutex);
				continue;
			}

			/*
			 * If this is defaulted to use Slices we need to
			 * find out the lowest partition that maps the
			 * beginning of the cdrom
			 *
			 * If it is P partitions we must determine the
			 * slice number (s#) to map the p0 to.
			 */

			if (ioctl(fd, DKIOCGVTOC, &vtoc) == 0) {
				if (cdp->cd_p_type == S_TYPE) {
				    cdp->cd_defpart = partition_low(&vtoc);
				    debug(1,
					"cd_thread_wait: cd_defpart now %d\n",
					cdp->cd_defpart);
				} else {
				    cdp->cd_p0_part = part_to_hack(&vtoc);
				    debug(1,
					"cdrom_thread_wait:hack part = %d\n",
					cdp->cd_p0_part);
				}
			}


			/* generate an "insert" event */
			vie.vie_type = VIE_INSERT;
			vie.vie_insert.viei_dev = dp->dp_dev;
			vol_event(&vie);
		}

		/*
		 * We have NO media in the drive (it's just been ejected)
		 */
		if (cd_state == DKIO_EJECTED) {

		struct vol	*v = dp->dp_vol;

			/*
			 * If we already know about the ejection,
			 * just continue in our happy loop.
			 */
			if (dp->dp_vol == NULL) {
				(void) mutex_unlock(&cdp->cd_killmutex);
				continue;
			}

			/*
			 * Generate an eject event (if we have a unit)
			 *
			 * XXX: this doesn't work because the DKIOCSTATE ioctl
			 * for CD-ROMs *never* returns DKIO_EJECTED
			 */
			for (i = 0; i < (int)v->v_ndev; i++) {
				if (v->v_devmap[i].dm_voldev == dp->dp_dev) {
					vie.vie_type = VIE_EJECT;
					vie.vie_eject.viej_force = TRUE;
					vie.vie_eject.viej_unit =
					    minor(v->v_devmap[i].dm_voldev);
					vol_event(&vie);
					break;
				}
			}
		}
		(void) mutex_unlock(&cdp->cd_killmutex);
	}

errout:
	/* close all the open fd's (p and s types) */
	for (n = 0; n < S_AND_P; n++) {
		for (i = 0; i < CDMAX_PARTS; i++) {
			/* if fd is open close it otherwise continue */
			if (cdp->cd_fd[n][i] >= 0) {
				(void) close(cdp->cd_fd[n][i]);
				cdp->cd_fd[n][i] = -1;
			}
		}
	}
}


#ifdef	DEBUG

static char *
state_to_str(enum dkio_state st)
{
	static char		state_buf[30];


	switch (st) {
	case DKIO_NONE:
		(void) sprintf(state_buf, "DKIO_NONE");
		break;
	case DKIO_INSERTED:
		(void) sprintf(state_buf, "DKIO_INSERTED");
		break;
	case DKIO_EJECTED:
		(void) sprintf(state_buf, "DKIO_EJECTED");
		break;
	default:
		(void) sprintf(state_buf, "?unknown? (%d)", (int)st);
		break;
	}

	return (state_buf);
}
#endif	/* DEBUG */


static bool_t
cdrom_testpath(char *path)
{
	int			fd;
	struct dk_cinfo		dkc;
	struct stat		sb;
	char			*rp;



	/* check to see if we're already using it */
	if (stat(path, &sb) != 0) {
		/* something's seriously wrong */
		debug(5, "cdrom(probing): %s; %m\n", path);
		return (FALSE);
	}

	if (dev_getdp(sb.st_rdev)) {
		debug(5, "cdrom(probing): %s already in use\n", path);
		return (FALSE);			/* this one's legit */
	}

	/* make sure our path is a raw device */
	if ((rp = rawpath(path)) == NULL) {
		debug(5, "cdrom(probing): can't rawpath %s\n", path);
		return (FALSE);
	}

	debug(10, "path = %s and rawpath = %s\n", path, rp);

	/*
	 * if we can't open it, assume that it's because it's busy or
	 * something else is wrong.  in any event, dev_use couldn't
	 * open it either, so it's not worth trying to use the device.
	 */
	if ((fd = open(rp, O_RDONLY|O_NONBLOCK)) < 0) {
		debug(5, "cdrom(probing): %s; %m\n", rp);
		free(rp);
		return (FALSE);
	}

	/* check to make sure device is a CD-ROM */
	if (ioctl(fd, DKIOCINFO, &dkc) < 0) {
		debug(5, "cdrom(probing): %s DKIOCINFO; %m\n", rp);
		(void) close(fd);
		free(rp);
		return (FALSE);
	}
	if (dkc.dki_ctype != DKC_CDROM) {
		debug(5, "cdrom(probing): %s is not a CD-ROM drive\n", rp);
		debug(5, "cdrom(probing2):dkc.dki_ctype = %x; DKC_CDROM = %x\n",
			dkc.dki_ctype, DKC_CDROM);
		(void) close(fd);
		free(rp);
		return (FALSE);
	}

	/* all done */
	(void) close(fd);
	free(rp);
	return (TRUE);

}


#if defined(_FIRMWARE_NEEDS_FDISK)

/*
 * serious hackery -- attempt to open the p? interfaces of the specified
 *		device -- just to keep users from getting around volmgt
 *		(e.g. "eject /dev/dsk/c0t6d0p0" -- oops)
 *
 *		If this fails, just ignore it.
 *
 *		The supplied params path1 and path2 will be the block
 *		and char prototype paths (but not necessarily in that
 *		order).
 */
static void
cdrom_open_exclusive(struct cd_priv *cdp, char *path1, char *path2)
{
	char	namebuf[MAXPATHLEN];
	int	i;
	char	*raw_proto;			/* for the "rdsk" path */


	/* initialized all of the fds */
	for (i = 0; i < (FD_NUMPART+1); i++) {
		cdp->cd_raw_pfd[i] = -1;
	}

	/* find out which one is the raw path prototype */
	if (strstr(path1, "rdsk")) {
		raw_proto = path1;
	} else if (strstr(path1, "dsk")) {
		raw_proto = path2;
	} else {
		return;
	}

	/* (attempt to) open each p device */
	for (i = 0; i < (FD_NUMPART+1); i++) {
		/* do the raw device */
		(void) sprintf(namebuf, CDROM_NAMEPROTO_P, raw_proto, i);
		cdp->cd_raw_pfd[i] = open(namebuf, O_RDONLY|O_EXCL|O_NDELAY);
#ifdef	DEBUG
		debug(6, "cdrom_open_exclusive: open(\"%s\") -> %d\n",
		    namebuf, cdp->cd_raw_pfd[i]);
#endif
	}

}

#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */
