/*
 * Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vold_vol.c	1.69	96/11/19 SMI"

/*
 * Volume Daemon primary interface to the vol driver
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<signal.h>
#include	<string.h>
#include	<locale.h>
#include	<sys/types.h>
#include	<sys/mkdev.h>
#include	<sys/ddi.h>
#include	<sys/errno.h>
#include	<sys/param.h>
#include	<sys/stat.h>
#include	<sys/time.h>
#include	<sys/wait.h>
#include	<sys/mnttab.h>
#include	<sys/dkio.h>
#include	<sys/tiuser.h>
#include	<sys/vol.h>
#include	<rpc/types.h>
#include	<rpc/auth.h>
#include	<rpc/auth_unix.h>
#include	<rpc/xdr.h>
#include	<rpc/clnt.h>
#include	<rpcsvc/nfs_prot.h>
#include	<thread.h>
#include	<synch.h>
#include	"vold.h"
#include	"label.h"
#include	"dev.h"


int	vol_fd = -1;
major_t	vol_major = 0;				/* used in minor_alloc() */

enum read_type { INSERT, NEWLABEL, CONFIRM };

struct volins {
	dev_t		vi_dev;		/* device action occured on */
	void		*vi_stk;	/* stack for the thread */
	vol_t		*vi_v;		/* volume that action occured on */
	enum read_type	vi_act;		/* type of action */
};

static void		vol_insert(struct volins *vid);
static void		vol_missing(struct ve_missing *);

struct alab {
	struct q	q;		/* linked list... */
	dev_t		al_dev;		/* device operating on */
	vol_t		*al_v;		/* volume (maybe) operating on */
	enum laread_res	al_readres;	/* result of label probe */
	label		al_label;	/* label from device */
	enum read_type	al_act;		/* type of operation */
	void		*al_stk;	/* stack of our thread (for free) */
	int		al_tid;
};

static struct q alabq;
static mutex_t	alab_mutex;	/* protect the queue */
static bool_t	alab_work;	/* flag to say there's work to do */


static struct alab 	*vol_readlabel(struct volins *);
static void		vol_forceout(vol_t *);
static int		vol_reaper(struct reap *);

/*
 * thread stack size
 */
#define	VOL_STKSIZE	(32 * 1024)		/* 32k! */

bool_t
vol_init(void)
{
	char		namebuf[MAXPATHLEN+1];
	struct stat	sb;


	(void) sprintf(namebuf, "/dev/%s", VOLCTLNAME);
	if ((vol_fd = open(namebuf, O_RDWR)) < 0) {
		warning(gettext("vol_init: open failed on  %s; %m\n"),
		    namebuf);
		return (FALSE);
	}
	(void) fcntl(vol_fd, F_SETFD, 1);	/* close-on-exec */
	(void) fstat(vol_fd, &sb);
	vol_major = major(sb.st_rdev);

	/* set up the driver */
	if (ioctl(vol_fd, VOLIOCDAEMON, getpid()) != 0) {
		fatal(gettext("vol_init: already a daemon running\n"));
		/*NOTREACHED*/
	}

	/* set up our mutex */
	(void) mutex_init(&alab_mutex, USYNC_THREAD, 0);
	return (TRUE);
}


void
vol_readevents(void)
{
	extern void		vol_event(struct vioc_event *);
	int			err;
	struct vioc_event	vie;
	extern int		errno;



#ifdef	DEBUG
	debug(10, "vol_readevents: scanning for all events\n");
#endif
	/*CONSTCOND*/
	while (1) {
		err = ioctl(vol_fd, VOLIOCEVENT, &vie);
#ifdef	DEBUG
		if ((err != 0) && (errno != EWOULDBLOCK)) {
			debug(10, "vol_readevents: ioctl(VOLIOCEVENT); %m\n",
			    err);
		}
#endif
		if (err != 0) {
			if (errno == EWOULDBLOCK) {
#ifdef	DEBUG
				debug(10, "vol_readevents: no more events\n");
#endif
				return;
			}
			perror("vol_readevents");
			return;
		}

		vol_event(&vie);
	}
	/*NOTREACHED*/
}


void
vol_event(struct vioc_event *vie)
{
	int			err = 0;
	struct volins		*vid;
	vol_t			*v;
	thread_t		id;
	dev_t			dev;
	minor_t			mnr;
#ifdef	DEBUG
	char			event_str[50];
	char			*event_names[] = {
					"VIE_MISSING",
					"VIE_EJECT",
					"VIE_DEVERR",
					"VIE_CLOSE",
					"VIE_CANCEL",
					"VIE_NEWLABEL",
					"VIE_INSERT",
					"VIE_GETATTR",
					"VIE_SETATTR",
					"VIE_INUSE",
					"VIE_CHECK",
					"VIE_REMOVED",
					"VIE_SYMNAME",
					"VIE_SYMDEV"};


	if ((vie->vie_type < 0) || (vie->vie_type > VIE_SYMDEV)) {
		(void) sprintf(event_str, "VIE_??? (%d)", vie->vie_type);
	} else {
		(void) strcpy(event_str, event_names[vie->vie_type]);
	}
	debug(1, "vol_event: %d (%s)\n", vie->vie_type, event_str);
#else	/* DEBUG */
	debug(1, "vol_event: %d\n", vie->vie_type);
#endif	/* DEBUG */

	switch (vie->vie_type) {
	case VIE_CHECK: {
		extern int	dev_check(dev_t);
		int		rval;

		rval = dev_check(vie->vie_check.viec_dev);
		/*
		 * dev_check returns:
		 * 0 if it didn't find anything
		 * 1 if it foudn something and we already knew about it
		 * 2 if it found something and we generated an insert event
		 */
		if (rval == 0) {
			(void) ioctl(vol_fd, VOLIOCDCHECK, ENXIO);
		} else if (rval == 1) {
			(void) ioctl(vol_fd, VOLIOCDCHECK, 0);
		}

		/*
		 * If there was something there, a flag was set
		 * in the dp structure saying that a response needs to
		 * be made "as late as possible".  In other words,
		 * if there are no actions to be done, do the response
		 * after the name space has been built.  If there are
		 * actions to be done, respond after they have completed.
		 */
		break;
	}

	case VIE_SYMNAME: {
		struct vol_str	vs;


		dev = vie->vie_symname.vies_dev;
		if ((vs.data = dev_symname(dev)) == NULL) {
			vs.data_len = 0;
		} else {
			vs.data_len = strlen(vs.data);
		}

		debug(11,
		    "vol_event: VIE_SYMNAME: (%d,%d) -> \"%s\" (len %d)\n",
		    major(dev), minor(dev),
		    vs.data ? vs.data : "<null ptr>", vs.data_len);

		(void) ioctl(vol_fd, VOLIOCDSYMNAME, &vs);
		break;
	}

	case VIE_SYMDEV: {
		extern char	*symname_to_dev(char *);
		char		*symname;
		struct vol_str	vs;


		symname = vie->vie_symdev.vied_symname;

		/*
		 * be sure symname is not NULL and symname_to_dev does not
		 * return NULL
		 */
		if (symname != NULL) {
			if ((vs.data = symname_to_dev(symname)) == NULL) {
				vs.data_len = 0;
			} else {
				vs.data_len = strlen(vs.data);
			}
		} else {
			vs.data_len = 0;
			vs.data = NULL;
		}

		debug(11, "vol_event: VIE_SYMDEV: \"%s\" -> \"%s\"\n",
		    symname ? symname : "<null ptr>",
		    vs.data ? vs.data : "<null ptr>");

		(void) ioctl(vol_fd, VOLIOCDSYMDEV, &vs);
		break;
	}

	case VIE_INUSE: {
		extern int	dev_inuse(dev_t);
		extern bool_t	mount_complete;
		bool_t		rval;


		if (!mount_complete) {
#ifdef	DEBUG
			debug(5,
			    "vol_event: VIE_INUSE: poll NOT being done yet\n");
#endif
			err = ENXIO;
		} else {

			dev = vie->vie_inuse.vieu_dev;
#ifdef	DEBUG
			debug(5,
			    "vol_event: VIE_INUSE: check for (%d,%d) use\n",
			    major(dev), minor(dev));
#endif
			if ((dev == makedev(vol_major, 0)) == 0) {
				rval = (bool_t)dev_inuse(
				    vie->vie_inuse.vieu_dev);
				/*
				 * dev_inuse returns TRUE if the device
				 * is managed, and FALSE if it isn't
				 */
				if (rval == FALSE) {
					err = ENXIO;
				}
			}
		}
#ifdef	DEBUG
		debug(5, "vol_event: returning err=%d\n", err);
#endif
		(void) ioctl(vol_fd, VOLIOCDINUSE, err);
		break;
	}

	case VIE_INSERT: {
		dev = vie->vie_insert.viei_dev;

		debug(2, "vol_event: insert into (%d,%d)\n",
		    major(dev), minor(dev));
		vid = (struct volins *)malloc(sizeof (struct volins));
		vid->vi_stk = 0;
		vid->vi_dev = dev;
		vid->vi_act = INSERT;
		if (thr_create(0, VOL_STKSIZE,
		    (void *(*)(void *))vol_insert, (void *)vid, THR_BOUND,
		    &id) < 0) {
			warning(gettext("can't create thread; %m\n"));
#ifdef	DEBUG
		} else {
		debug(6,
	"vol_event: created vol_insert() tid %d (%d,%d) (for INSERT)\n",
		    (int)id, major(dev), minor(dev));
#endif
		}
		break;
	}

	case VIE_MISSING:
		vol_missing(&vie->vie_missing);
		break;

	case VIE_EJECT:
		if (vie->vie_eject.viej_force) {
			debug(1, "vol_event: got a forced ejection\n");
			/*
			 * Is it already gone?
			 */
			v = minor_getvol((minor_t)vie->vie_eject.viej_unit);
			if ((v == NULL) || (v->v_confirmed == FALSE)) {
#ifdef	DEBUG
				debug(5, "vol_event: unit %d already gone!\n",
				    vie->vie_eject.viej_unit);
#endif
				break;
			}
			v->v_ej_force = TRUE;
		} else {
			v = minor_getvol((minor_t)vie->vie_eject.viej_unit);
			if (v == NULL) {
				debug(1, "eject on strange unit %d\n",
				    vie->vie_eject.viej_unit);
				break;
			}
			v->v_ej_force = FALSE;
		}
		if (v->v_ej_inprog) {
			/* ejection in progress... ignore */
			debug(6, "vol_event: ignoring dup eject on %s\n",
			    v->v_obj.o_name);
			break;
		}
		v->v_ej_inprog = TRUE;
		v->v_clue.c_volume = vie->vie_eject.viej_unit;
		v->v_clue.c_uid = vie->vie_eject.viej_user;
		v->v_clue.c_tty = vie->vie_eject.viej_tty;
		v->v_ejfail = FALSE;

		/*
		 * If we're ejecting a piece of media that a new label
		 * has been written to -- read the new label off before
		 * proceeding with the ejection.
		 */
		if ((v->v_flags & V_NEWLABEL) && (v->v_basedev != NODEV)) {
			debug(1, "vol_event: need to read new label on %s\n",
			    v->v_obj.o_name);
			vid = (struct volins *)
			    malloc(sizeof (struct volins));
			vid->vi_stk = 0;
			vid->vi_dev = v->v_basedev;
			vid->vi_v = v;
			vid->vi_act = NEWLABEL;
			if (thr_create(0, VOL_STKSIZE,
			    (void *(*)(void *))vol_insert, (void *)vid,
			    THR_BOUND, &id) < 0) {
				warning(gettext("can't create thread; %m\n"));
#ifdef	DEBUG
			} else {
				debug(6,
	"vol_event: created vol_insert() tid %d (%d,%d) (for EJ w/new lab)\n",
				    (int)id, major(vid->vi_dev),
				    minor(vid->vi_dev));
#endif
			}
			v->v_flags &= ~V_NEWLABEL;
		} else {
			if (action(ACT_EJECT, v) == 0) {
				dev_eject(v, TRUE);
			}
		}

		break;

	case VIE_DEVERR:
		dev_error(&vie->vie_error);
		debug(1, "device error %d on (%d,%d)\n",
		    vie->vie_error.viee_errno,
		    major(vie->vie_error.viee_dev),
		    minor(vie->vie_error.viee_dev));
		break;

	case VIE_CLOSE: {
		mnr = vie->vie_close.viecl_unit;
		debug(5, "close on unit %d\n", mnr);
		v = minor_getvol(mnr);
		if ((v != NULL) && (v->v_flags & V_NEWLABEL)) {
			debug(1, "need to read new label on %s\n",
			    v->v_obj.o_name);
			if (v->v_basedev == NODEV) {
				debug(1, "error: no device for %s\n",
				    v->v_obj.o_name);
				break;
			}
			vid = (struct volins *)
			    malloc(sizeof (struct volins));
			vid->vi_stk = 0;
			vid->vi_dev = v->v_basedev;
			vid->vi_v = v;
			vid->vi_act = NEWLABEL;
			if (thr_create(0, VOL_STKSIZE,
			    (void *(*)(void *))vol_insert, (void *)vid,
			    THR_BOUND, &id) < 0) {
				warning(gettext("can't create thread; %m\n"));
#ifdef	DEBUG
			} else {
				debug(6,
	"vol_event: created vol_insert() tid %d (%d,%d) (for CLOSE)\n",
				    (int)id, major(mnr), minor(mnr));
#endif
			}
			v->v_flags &= ~V_NEWLABEL;
		}
		break;
	}

	case VIE_CANCEL:
		mnr = vie->vie_cancel.viec_unit;
		if ((v = minor_getvol(mnr)) == NULL) {
			debug(5, "cancel on unit %d: unit already gone\n",
			    mnr);
			break;
		}

		debug(5, "cancel on unit %d (vol %s)\n", mnr, v->v_obj.o_name);

		/* if we have a device then clean up after it */
		if (v->v_confirmed == FALSE) {
			vol_forceout(v);
		}

#ifdef	IT_ALL_WORKED
		/*
		 * it'd be nice to "unmount" (i.e. run rmmount, the
		 *	std eject-action handler), but when vol_reaper()
		 *	calls dev_eject(), which calls dev_getdp(), the
		 *	assertion that dev != NODEV in dev_getdp() pukes!
		 *	so, for now, if the user cancels i/o (via volcancel),
		 *	they leave the vol mounted!  -- wld
		 */
		/* now try to run the eject action */
		if (v->v_ej_inprog) {
			/* ejection in progress... ignore */
			debug(6, "ignoring dup eject on %s\n",
			    v->v_obj.o_name);
			break;
		}
		v->v_ej_inprog = TRUE;
		v->v_clue.c_volume = mnr;
		v->v_clue.c_uid = DEFAULT_TOP_UID;
		v->v_clue.c_tty = 0;
		v->v_ejfail = FALSE;

		(void) action(ACT_EJECT, v);
#endif	/* IT_ALL_WORKED */
		break;

	case VIE_NEWLABEL:
		mnr =  vie->vie_newlabel.vien_unit;
		debug(5, "newlabel on unit %d\n", mnr);
		if ((v = minor_getvol(mnr)) != NULL) {
			v->v_flags |= V_NEWLABEL;
		}
		break;

	case VIE_GETATTR: {
		char			*value;
		char			*props;
		struct vioc_dattr	vda;


		mnr = vie->vie_attr.viea_unit;
		if ((v = minor_getvol(mnr)) == NULL) {
			debug(5, "getattr on unit %d: unit already gone\n",
			    mnr);
			break;
		}
		props = props_get(v);
		value = prop_attr_get(props, vie->vie_attr.viea_attr);
		if (value != NULL) {
			(void) strncpy(vda.vda_value, value, MAX_ATTR_LEN);
			free(value);
			vda.vda_errno = 0;
		} else {
			vda.vda_errno = ENOENT;
		}
		if (props != NULL) {
			free(props);
		}
		vda.vda_unit = mnr;
		(void) ioctl(vol_fd, VOLIOCDGATTR, &vda);
		break;
	}

	case VIE_SETATTR: {
		extern bool_t		props_check(vol_t *, struct ve_attr *);
		char			*props;
		char			*nprops;
		struct vioc_dattr	vda;


		mnr = vie->vie_attr.viea_unit;
		if ((v = minor_getvol(mnr)) == NULL) {
			debug(5, "setattr on unit %d: unit already gone\n",
			    mnr);
			break;
		}
		if (props_check(v, &vie->vie_attr)) {
			props = props_get(v);
			/* this will free "props" */
			nprops = prop_attr_put(props, vie->vie_attr.viea_attr,
			    vie->vie_attr.viea_value);
			props_set(v, nprops);
			if (nprops != NULL) {
				free(nprops);
			}
			vda.vda_errno = 0;
			change_flags((obj_t *)v);
			(void) db_update((obj_t *)v);
		} else {
			vda.vda_errno = EPERM;
		}
		vda.vda_unit = mnr;
		(void) ioctl(vol_fd, VOLIOCDSATTR, &vda);
		break;
	}

	case VIE_REMOVED:
		if ((v = minor_getvol(vie->vie_rm.virm_unit)) != NULL) {
			debug(1, "volume %s was removed from the drive\n",
			    v->v_obj.o_name);
			vol_forceout(v);	/* get rid of that baby */
		}
		break;

	default:
		warning(gettext("unknown message type %d from driver\n"),
		    vie->vie_type);
		break;
	}
}


/*
 * Media has been removed from the drive.  Do all the required
 * cleanup.
 */
static void
vol_forceout(vol_t *v)
{
	debug(4, "vol_forceout: forced eject on %s\n", v->v_obj.o_name);
	if (v->v_confirmed == FALSE) {
		debug(5, "vol_forceout: already gone!\n");
		return;
	}

	if (v->v_ej_inprog != FALSE) {
		debug(1, "vol_forceout: v->v_ej_inprog == TRUE\n");
	}

	v->v_ej_force = TRUE;
	v->v_ej_inprog = TRUE;
	dev_eject(v, TRUE);
}


static struct q missingq;

struct missing {
	struct	q	q;
	minor_t		devmin;
};


static void
vol_missing(struct ve_missing *miss)
{
	struct missing	*mq;
	vol_t		*v;



#ifdef	DEBUG
	debug(11, "vol_missing: called (unit %d)\n", miss->viem_unit);
#endif

	v = minor_getvol(miss->viem_unit);
	if (v == NULL) {
		debug(1, "missing on strange unit %d\n", miss->viem_unit);
		return;
	}

	debug(2, "missing volume %s\n", v->v_obj.o_name);

	if (dev_map(v, miss->viem_ndelay) != FALSE) {
		/* it's no longer missing */
		return;
	}

	/* check to see if we've already been here before */
	if (v->v_flags & V_MISSING) {
		return;
	}

	/* okay, it's really missing -- create a notify event */
	mq = (struct missing *)calloc(1, sizeof (struct missing));
	mq->devmin = miss->viem_unit;
	INSQUE(missingq, mq);
	info(gettext("can't find the %s volume, please go find it for me!\n"),
	    v->v_obj.o_name);
	v->v_clue.c_uid = miss->viem_user;
	v->v_clue.c_tty = miss->viem_tty;
	v->v_flags |= V_MISSING;		/* so we don't do this again */
	(void) action(ACT_NOTIFY, v);
}


/*
 * Something wonderful has just happened to vid->vi_dev.  Either
 * a new piece of media was inserted into the drive (act == INSERT),
 * someone just wrote a new label over the old one (act == NEWLABEL),
 * or we are being asked to read the label to confirm what was
 * believed to be in the drive (act == CONFIRM).
 */
static void
vol_insert(struct volins *vid)
{
	extern mutex_t	polling_mutex;
	struct devs	*dp;
	struct alab	*al = NULL;
#ifdef	TWO_SIDED_DEVICE
	struct alab	*al1 = NULL;
#endif



	debug(2, "vol_insert: thread for handling (%d,%d)\n",
	    major(vid->vi_dev), minor(vid->vi_dev));

	dp = dev_getdp(vid->vi_dev);
	if (dp == NULL) {
		debug(1, "no mapping for (%d,%d)!\n", major(vid->vi_dev),
		    minor(vid->vi_dev));
	}

	al = vol_readlabel(vid);

#ifdef TWO_SIDED_DEVICE
	/* if it's a two sided device, we only have one poller per slot */
	if ((vid->vi_act == INSERT) &&
	    ((dp != NULL) && (dp->dp_dsw->d_flags & D_2SIDED))) {
		debug(2, "2sided: other side of (%d,%d) is (%d,%d)\n",
		    major(vid->vi_dev), minor(vid->vi_dev),
		    major(dp->dp_otherside), minor(dp->dp_otherside));
		vid->vi_dev = dp->dp_otherside;
		vid->vi_stk = 0;
		al1 = vol_readlabel(vid);
	}
#endif

	if (al != NULL) {
		al->al_v = vid->vi_v;	/* only on NEWLABEL && CONFIRM */
		al->al_act = vid->vi_act;
	}

	(void) mutex_lock(&alab_mutex);
	if (al != NULL) {
		INSQUE(alabq, al);
		alab_work = TRUE;
#ifdef	DEBUG
		debug(7, "vol_insert: alab_work set to TRUE\n");
#endif
	}
#ifdef	TWO_SIDED_DEVICE
	if (al1 != NULL) {
		INSQUE(alabq, al1);
		alab_work = TRUE;
	}
#endif
	(void) mutex_unlock(&alab_mutex);

	if (mutex_trylock(&polling_mutex) == 0) {
		/* we only want to send the signal while he's in a poll */
		if (alab_work) {
#ifdef	DEBUG
			debug(7, "vol_insert: sending SIGUSR2 to tid 1\n");
#endif
			(void) thr_kill(1, SIGUSR2);
		}
		(void) mutex_unlock(&polling_mutex);
	}

	free(vid);

	debug(7, "vol_insert: thread exiting\n");

	/* this thread is all done */
	thr_exit(NULL);
}


static struct alab *
vol_readlabel(struct volins *vid)
{
	extern int		dev_getfd(dev_t);
	extern enum laread_res	label_scan(int, char *, label *,
				    struct devs *);
	extern void		dev_hard_eject(struct devs *);
	struct devs		*dp;
	label			la;
	dev_t			dev = vid->vi_dev;
	enum laread_res		res;
	struct alab		*al;
	int			fd;


	/*
	 * have the driver specific code tell us how to get to the
	 * raw device.
	 */
	if ((fd = dev_getfd(dev)) == -1) {
		return (NULL);
	}

	la.l_label = 0;

	if ((dp = dev_getdp(dev)) == NULL) {
		return (NULL);
	}

	/*
	 * walk through the label types, trying to read the labels.
	 */
	if ((res = label_scan(fd, dp->dp_dsw->d_mtype, &la, dp)) == L_ERROR) {
		/*
		 * If the label routines couldn't read the device,
		 * get it outta here!
		 */
		dev_hard_eject(dp);
		return (NULL);
	}

	al = (struct alab *)calloc(1, sizeof (struct alab));
	al->al_dev = dev;
	al->al_stk = vid->vi_stk;
	al->al_readres = res;
	al->al_label.l_type = la.l_type;
	al->al_label.l_label = la.l_label;
	al->al_tid = thr_self();
	return (al);
}


static vol_t *
vol_foundlabel(dev_t dev, label *la, enum read_type act, enum laread_res rres)
{
	extern void		dev_hard_eject(struct devs *);
	extern void		dev_hangvol(struct devs *, vol_t *);
#ifdef	DEBUG
	const char		*laread_res_to_str(enum laread_res);
	static const char	*read_type_to_str(enum read_type);
#endif
	vvnode_t		*vn;
	struct devs		*dp = dev_getdp(dev);
	struct missing		*mq;
	struct missing		*mq_next = NULL;
	vol_t			*v;
	int			nacts = 0;



#ifdef	DEBUG
	debug(3, "vol_foundlabel: entering for (%d.%d) (rres=%s, act=%s)\n",
	    major(dev), minor(dev), laread_res_to_str(rres),
	    read_type_to_str(act));
#else	/* DEBUG */
	debug(3, "vol_foundlabel: entering for (%d,%d) (rres=%d, act=%d)\n",
	    major(dev), minor(dev), (int)rres, (int)act);
#endif	/* DEBUG */

	/*
	 * go look for it in the namespace, etc.  If it isn't there,
	 * build us a new one.
	 */
	switch (rres) {
	default:
		/*
		 * just drop thru here -- dev_unlabeled will take
		 * care of it for us.
		 */
	case L_UNFORMATTED:
	case L_NOTUNIQUE:
	case L_UNRECOG:
		v = dev_unlabeled(dp, rres, la);
		break;
	case L_ERROR:
	case L_FOUND:
		if ((vn = node_findlabel(dp, la)) == NULL) {
#ifdef	DEBUG
			debug(3,
		"vol_foundlabel: node_findlabel() failed: ejecting\n");
#endif
			dev_hard_eject(dp);
			return (NULL);
		}

		v = vn->vn_vol;
		break;
	}

	dev_hangvol(dp, v);

	change_atime((obj_t *)v, &current_time);

	change_location((obj_t *)v, dp->dp_path);
	v->v_confirmed = TRUE;
	debug(1, "found volume \"%s\" in %s (%d,%d)\n",
	    v->v_obj.o_name, dp->dp_path, major(dev), minor(dev));
	if (!(v->v_flags & V_UNLAB)) {
		/*
		 * if it's not an unlabeled thing, write changes
		 * back to the database.
		 */
		(void) db_update((obj_t *)v);
	}
	if (act == INSERT) {
		nacts = action(ACT_INSERT, v);
		if ((dp->dp_checkresp != FALSE) && (nacts != 0)) {
			v->v_checkresp = TRUE;
			dp->dp_checkresp = FALSE;
		}
	}

	/* build a mapping for all the guys that are waiting on us. */
	for (mq = HEAD(struct missing, missingq); mq; mq = mq_next) {

		mq_next = NEXT(struct missing, mq);
#ifdef	DEBUG
		debug(1,
"vol_foundlabel: scanning \"missing\" vol \"%s\" (looking for \"%s\")\n",
		    minor_getvol(mq->devmin)->v_obj.o_name,
		    v->v_obj.o_name);
#endif
		if (minor_getvol(mq->devmin) == v) {
			info(gettext(
			    "Thank you for inserting the %s volume\n"),
			    v->v_obj.o_name);
			(void) dev_map(v, FALSE);
			REMQUE(missingq, mq);
		}
	}

	/* see if the s-enxio property needs to be cleared for this volume */
	if ((v->v_flags & V_ENXIO) && (act == INSERT) && (rres == L_FOUND)) {
		u_int		i;

		/* clear s-enxio for each device on this volume */
		for (i = 0; i < v->v_ndev; i++) {
			struct vioc_flags	vfl;

			/*
			 * use the VOLIOCFLAGS ioctl to tell the driver to
			 * quit handling enxio
			 */
			vfl.vfl_unit = minor(v->v_devmap[i].dm_voldev);
			vfl.vfl_flags = 0;
#ifdef	DEBUG
			debug(1,
			"vol_foundlabel: calling VOLIOCFLAGS(0), unit %d\n",
			    vfl.vfl_unit);
#endif
			if (ioctl(vol_fd, VOLIOCFLAGS, &vfl) < 0) {
				debug(1, "vol_foundlabel: VOLIOCFLAGS; %m\n");
			}
		}
		/* clear the volume's s-enxio flag */
		v->v_flags &= ~V_ENXIO;
	}

	/* if someone is waiting on the check ioctl, wake them up. */
	if ((dp->dp_checkresp != FALSE) && (nacts == 0)) {
		(void) ioctl(vol_fd, VOLIOCDCHECK, 0);
		dp->dp_checkresp = FALSE;
	}

	return (v);
}

#ifdef	DEBUG

const char *
laread_res_to_str(enum laread_res rres)
{
	const char	*res = NULL;
	static char	res_buf[10];


	switch (rres) {
	case L_UNRECOG:
		res = "L_UNRECOG";
		break;
	case L_UNFORMATTED:
		res = "L_UNFORMATTED";
		break;
	case L_NOTUNIQUE:
		res = "L_NOTUNIQUE";
		break;
	case L_ERROR:
		res = "L_ERROR";
		break;
	case L_FOUND:
		res = "L_FOUND";
		break;
	default:
		(void) sprintf(res_buf, "unknown (%d)", (int)rres);
		res = (const char *)res_buf;
		break;
	}

	return (res);
}


static const char
*read_type_to_str(enum read_type act)
{
	const char	*res = NULL;
	static char	res_buf[10];

enum read_type { INSERT, NEWLABEL, CONFIRM };

	switch (act) {
	case INSERT:
		res = "INSERT";
		break;
	case NEWLABEL:
		res = "NEWLABEL";
		break;
	case CONFIRM:
		res = "CONFIRM";
		break;
	default:
		(void) sprintf(res_buf, "unknown (%d)", (int)act);
		res = (const char *)res_buf;
		break;
	}

	return (res);
}

#endif	/* DEBUG */


static void
vol_newlabel(vol_t *v, dev_t dev, label *la)
{
	extern void	dev_hangvol(struct devs *, vol_t *);
	vol_t		*nv = 0;
	struct devs 	*dp;
	u_int		err;
	bool_t		doej;
	minor_t		c_vol;
	uid_t		c_uid;
	dev_t		c_tty;
	devmap_t	*dm;




#ifdef	DEBUG
	debug(11, "vol_newlabel: entered for \"%s\"\n", v->v_obj.o_name);
#endif

	/*
	 * unlabeled -> labeled
	 */
	if ((la->l_label != NULL) && (v->v_flags & V_UNLAB)) {

#ifdef	DEBUG
		debug(5, "vol_newlabel: unlabeled -> labeled\n");
#endif

		if (v->v_ej_inprog) {
			c_vol = v->v_clue.c_volume;
			c_uid = v->v_clue.c_uid;
			c_tty = v->v_clue.c_tty;
			debug(5,
			"vol_newlabel: clearing devmap using devmapfree\n");
			(void) dev_devmapfree(v);
			doej = TRUE;
		} else {
			doej = FALSE;
		}

		/* unhang the old unlabeled stuff */
		dev_unhangvol(dev_getdp(dev));
		/* v is gone now */

		/* recognize it in the cannonical way */
		nv = vol_foundlabel(dev, la, NEWLABEL, L_FOUND);

		/* must clear "cancel" flag on vol before unmapping */
		(void) dev_map(nv, FALSE);

		if (doej && (nv != NULL)) {
			nv->v_clue.c_volume = c_vol;
			nv->v_clue.c_uid = c_uid;
			nv->v_clue.c_tty = c_tty;
			nv->v_ejfail = FALSE;
			nv->v_ej_force = FALSE;
			nv->v_ej_inprog = TRUE;
			debug(5, "vol_newlabel: creating devmap\n");
			dev_devmap(nv);
			if (action(ACT_EJECT, nv) == 0) {
				dev_eject(nv, TRUE);
			}
		}
		return;
	}

	/*
	 * labeled -> labeled
	 */
	if ((la->l_label != NULL) && !(v->v_flags & V_UNLAB)) {

#ifdef	DEBUG
		debug(5, "vol_newlabel: labeled -> labeled\n");
#endif

		/* just rewrote the label on a normal name */
		change_label((obj_t *)v, la);
		(void) db_update((obj_t *)v);
		if (v->v_ej_inprog && (action(ACT_EJECT, v) == 0)) {
			dev_eject(v, TRUE);
		}
		return;
	}

	/*
	 * labeled -> unlabeled
	 */
	if ((la->l_label == NULL) && !(v->v_flags & V_UNLAB)) {
#ifdef	DEBUG
		debug(5, "vol_newlabel: labeled -> unlabeled\n");
#endif
		/* out with the old */
		dp = dev_getdp(dev);
		(void) dev_devmapfree(v);
		if (v->v_ej_inprog) {
			c_vol = v->v_clue.c_volume;
			c_uid = v->v_clue.c_uid;
			c_tty = v->v_clue.c_tty;
			dm = v->v_devmap;
			v->v_devmap = 0;
			doej = TRUE;
		} else {
			doej = FALSE;
		}
		dev_unhangvol(dp);
		node_remove((obj_t *)v, TRUE, &err);
		/* v is gone now */

		/* in with the new */
		nv = dev_unlabeled(dp, L_UNRECOG, la);
		dev_hangvol(dp, nv);
		change_atime((obj_t *)nv, &current_time);
		change_location((obj_t *)nv, dp->dp_path);
		nv->v_confirmed = TRUE;
		if (doej) {
			/*
			 * If we need to eject him, copy the
			 * eject information.
			 */
			nv->v_devmap = dm;
			nv->v_clue.c_volume = c_vol;
			nv->v_clue.c_uid = c_uid;
			nv->v_clue.c_tty = c_tty;
			nv->v_ejfail = FALSE;
			nv->v_ej_force = FALSE;
			nv->v_ej_inprog = TRUE;
			if (action(ACT_EJECT, nv) == 0) {
				dev_eject(nv, TRUE);
			}
		}
		/*
		 * must map device in case someboy is in a hurry to
		 * access it
		 */
		dev_devmap(nv);
		return;
	}

	/*
	 * unlabeled -> unlabeled
	 */
	if ((la->l_label == 0) && (v->v_flags & V_UNLAB)) {
#ifdef	DEBUG
		debug(5, "vol_newlabel: unlabeled -> unlabeled\n");
#endif
		if (v->v_ej_inprog && (action(ACT_EJECT, v) == 0)) {
			dev_eject(v, TRUE);
		}
		return;
	}

	/* shouldn't reach here */
	ASSERT(0);
}


/*
 * called from main vold loop to handle asynchronous events:
 *	- async label reads
 *	- reap process status (e.g. from rmmount)
 */
void
vol_async(void)
{
	struct alab	*al;
	struct alab	*al_next = NULL;


#ifdef	DEBUG
	debug(12, "vol_async: entering (alab_work=%s)\n",
	    alab_work ? "TRUE" : "FALSE");
#endif
	/*
	 * Results from async label reads.
	 */
	if (alab_work) {

		(void) mutex_lock(&alab_mutex);

		for (al = HEAD(struct alab, alabq); al != NULL; al = al_next) {

			al_next = NEXT(struct alab, al);

			switch (al->al_act) {
			case INSERT:
				(void) vol_foundlabel(al->al_dev,
				    &al->al_label, INSERT, al->al_readres);
				break;
			case NEWLABEL:
				vol_newlabel(al->al_v, al->al_dev,
				    &al->al_label);
				break;
			case CONFIRM:
			default:
				debug(1, "vol_async: funny work %d\n",
					al->al_act);
			}

			REMQUE(alabq, al);

			/* wait for the thread */
			if (al->al_tid != 0) {
				(void) thr_join(al->al_tid, 0, 0);
			}
			if (al->al_stk != NULL) {
				free(al->al_stk); /* free the thread stack */
			}
			free(al);		/* free the alab */
		}

		alab_work = FALSE;

		(void) mutex_unlock(&alab_mutex);
	}

	/*
	 * Results from action processes (eject specifically)
	 */
	if (HEAD(struct reap, reapq) != NULL) {
		struct reap	*r;
		struct reap	*r_next = NULL;


		for (r = HEAD(struct reap, reapq); r != NULL; r = r_next) {

			r_next = NEXT(struct reap, r);

			if (vol_reaper(r) == 1) {
				/* respond to the check, if it's pending */
				if (r->r_v->v_checkresp != FALSE) {
					(void) ioctl(vol_fd, VOLIOCDCHECK, 0);
					r->r_v->v_checkresp = FALSE;
				}
				/* free this thing */
				REMQUE(reapq, r);
				free(r->r_hint);
				free(r);
			}
		}
	}
#ifdef	DEBUG
	debug(12, "vol_async: no more async label work\n");
#endif
}


static int
vol_reaper(struct reap *r)
{
	pid_t		err;
	int		stat;


#ifdef	DEBUG
	debug(12, "vol_reaper: entering: pid=%d, act=%d (%s)\n",
	    r->r_pid, r->r_act, actnames[r->r_act]);
#endif
	if ((r->r_pid == 0) && (r->r_act == ACT_EJECT)) {
		/* had an internal error -- ejecting */
		dev_eject(r->r_v, TRUE);
		return (1);
	}

	if ((r->r_pid == -1) && (r->r_act == ACT_EJECT)) {
		/* unsafe -- not ejecting */
		warning(gettext(
		    "volume %s has file system mounted, cannot eject\n"),
		    r->r_v->v_obj.o_name);
		r->r_v->v_eject = 0;
		r->r_v->v_ejfail = TRUE;
		dev_eject(r->r_v, FALSE);
		return (1);
	}

#ifdef	DEBUG
	debug(12, "vol_reaper: waiting (NOHANG) for pid %d\n", r->r_pid);
#endif
	if ((err = waitpid(r->r_pid, &stat, WNOHANG)) == 0) {
		/* process is still working */
		return (0);
	}

	if (err == -1) {
		debug(1, "waitpid for %s action pid %d; %m\n",
		    actnames[r->r_act], r->r_pid);
		return (0);
	}

#ifdef	DEBUG
	debug(12, "vol_reaper: pid %d returned stat 0%o\n", r->r_pid, stat);
#endif

	if (WIFEXITED(stat)) {
		debug(4, "status for pid %d: exited with %d\n",
			r->r_pid, WEXITSTATUS(stat));
	} else if (WIFSIGNALED(stat)) {
		if (WCOREDUMP(stat)) {
			debug(4,
	"status for pid %d: signaled with '%s' and dumped core\n",
			    r->r_pid, strsignal(WTERMSIG(stat)));
		} else {
			debug(4,
			    "status for pid %d: signaled with '%s'\n",
			    r->r_pid, strsignal(WTERMSIG(stat)));
		}
	} else {
		debug(4, "status for pid %d: %#x\n", r->r_pid, stat);
	}

	/*
	 * If the process was an eject action and it exited normally,
	 * we take this path.
	 */
	if (r->r_act == ACT_EJECT) {
#ifdef	DEBUG
		debug(12, "vol_reaper: cleaning up after eject\n");
#endif
		if (r->r_v->v_eject > 0) {
			r->r_v->v_eject--;
		}

		if (WIFEXITED(stat)) {
			if (WEXITSTATUS(stat) == 0) {
				/*
				 * don't eject until all the actions are done.
				 */
				if ((r->r_v->v_eject == 0) &&
				    (r->r_v->v_ejfail == FALSE)) {
					dev_eject(r->r_v, TRUE);
				}
				return (1);
			}
			if (WEXITSTATUS(stat) == 1) {
				/*
				 * If we get a no, say that we are done and
				 * fail the eject.
				 */
				r->r_v->v_eject = 0;
				r->r_v->v_ejfail = TRUE;
				dev_eject(r->r_v, FALSE);
				return (1);
			}

			debug(1,
		gettext("eject action %s retns exit code %d, not ejecting\n"),
			    r->r_hint, WEXITSTATUS(stat));
			r->r_v->v_eject = 0;
			r->r_v->v_ejfail = TRUE;
			dev_eject(r->r_v, FALSE);
			return (1);
		}
		if (WIFSIGNALED(stat)) {
			/*
			 * If the process got its brains blown out,
			 * don't treat that as a denial, just continue
			 * along.
			 */
			if ((r->r_v->v_eject == 0) &&
			    (r->r_v->v_ejfail == FALSE)) {
				dev_eject(r->r_v, TRUE);
			}
			return (1);
		}
	}

	if (WIFEXITED(stat) || WIFSIGNALED(stat)) {
		return (1);
	}

	warning(gettext("%s action process %d hanging around...\n"),
	    actnames[r->r_act], r->r_pid);
	return (0);
}


vol_t *
vol_mkvol(struct devs *dp, label *la)
{
	vol_t	*v;



	v = (vol_t *)calloc(1, sizeof (vol_t));
	v->v_obj.o_type = VV_CHR;
	v->v_mtype = strdup(dp->dp_dsw->d_mtype);
	v->v_basedev = NODEV;
	label_setup(la, v, dp);
	change_atime((obj_t *)v, &current_time);
	return (v);
}
