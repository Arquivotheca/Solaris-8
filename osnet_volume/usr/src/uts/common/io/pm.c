/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pm.c	1.78	99/12/21 SMI"

/*
 * pm	This driver now only handles the ioctl interface.  The scanning
 *	and policy stuff now lives in common/os/sunpm.c.
 *	Not DDI compliant
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/conf.h>		/* driver flags and functions */
#include <sys/open.h>		/* OTYP_CHR definition */
#include <sys/stat.h>		/* S_IFCHR definition */
#include <sys/pathname.h>	/* name -> dev_info xlation */
#include <sys/kmem.h>		/* memory alloc stuff */
#include <sys/debug.h>
#include <sys/pm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/epm.h>
#include <sys/vfs.h>
#include <sys/mode.h>
#include <sys/mkdev.h>
#include <sys/promif.h>
#include <sys/consdev.h>
#include <sys/ddi_impldefs.h>
#include <sys/poll.h>

/*
 * Minor number is instance<<8 + clone minor from range 1-255; (0 reserved
 * for "original"
 */
#define	PM_MINOR_TO_CLONE(minor) ((minor) & (PM_MAX_CLONE - 1))

#define	PM_NUMCMPTS(dip) (DEVI(dip)->devi_pm_num_components)

#define	PM_IDLEDOWN_TIME	10

/*
 * Locking:  See PM LOCKING in common/os/sunpm.c
 */

/*
 * The soft state of the power manager; but since there is always only
 * one of these, we don't bother with a structure, but just this collection
 * of globals.
 */

/*
 * Single-threads scans and accesses to pm_scan_list
 */
extern kmutex_t	pm_scan_lock;	/* common/os/sunpm.c */
extern pm_scan_t	*pm_scan_list;
extern krwlock_t pm_scan_list_rwlock;

extern kmutex_t	pm_clone_lock;		/* protects pm_clones array */

uchar_t		pm_clones[PM_MAX_CLONE];
extern kcondvar_t	pm_clones_cv[PM_MAX_CLONE];
extern uint_t	pm_poll_cnt[PM_MAX_CLONE];

extern int	pm_idle_down;

/*
 * The number of old style devices that we're power managing.
 * Count is protected by pm_scan_lock;
 */
extern int	bcpm_enabled;

/* Global variables for driver */
static dev_info_t *pm_dip;
static int	pm_instance = -1;
static clock_t	save_min_scan;
static timeout_id_t pm_idledown_id;

extern int	nulldev();
extern int	nodev();
extern void	pm_set_pm_info(dev_info_t *dip, void *value);
extern void	pm_proceed(dev_info_t *, int, int, int, int, int);
extern void	pm_get_timestamps(dev_info_t *, time_t *);

static int	pm_open(dev_t *, int, int, cred_t *);
static int	pm_close(dev_t, int, int, cred_t *);
static int	pm_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int	pm_check_permission(char *, cred_t *);
static int	pm_chpoll(dev_t, short, int, short *, struct pollhead **);

static struct cb_ops pm_cb_ops = {
	pm_open,	/* open */
	pm_close,	/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	pm_ioctl,	/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	pm_chpoll,	/* poll */
	ddi_prop_op,	/* prop_op */
	NULL,		/* streamtab */
	D_NEW | D_MP	/* driver compatibility flag */
};

static int pm_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int pm_identify(dev_info_t *dip);
static int pm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int pm_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

static struct dev_ops pm_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	pm_getinfo,		/* info */
	pm_identify,		/* identify */
	nulldev,		/* probe */
	pm_attach,		/* attach */
	pm_detach,		/* detach */
	nodev,			/* reset */
	&pm_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	NULL			/* power */
};


static struct modldrv modldrv = {
	&mod_driverops,
	"power management driver v1.78",
	&pm_ops
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, 0
};

/* Local functions */
#ifdef DEBUG
static void		print_info(dev_info_t *);
#define	PM_ADDR(dip)	(ddi_get_name_addr(dip) ? ddi_get_name_addr(dip) : "")
#endif

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
pm_identify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "pm") == 0) {
		return (DDI_IDENTIFIED);
	} else
		return (DDI_NOT_IDENTIFIED);
}

static int
pm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		i;

	switch (cmd) {

	case DDI_ATTACH:
		if (pm_instance != -1)		/* Only allow one instance */
			return (DDI_FAILURE);
		pm_instance = ddi_get_instance(dip);
		if (ddi_create_minor_node(dip, "pm", S_IFCHR,
		    (pm_instance << 8) + 0, DDI_PSEUDO, 0) != DDI_SUCCESS) {
			return (DDI_FAILURE);
		}
		pm_dip = dip;		/* pm_init and getinfo depend on it */

		for (i = 0; i < PM_MAX_CLONE; i++)
			cv_init(&pm_clones_cv[i], NULL, CV_DEFAULT, NULL);

		ddi_report_dev(dip);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/* ARGSUSED */
static int
pm_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int i;

	switch (cmd) {

	case DDI_DETACH:

		for (i = 0; i < PM_MAX_CLONE; i++)
			cv_destroy(&pm_clones_cv[i]);

		/*
		 * Don't detach while idledown timeout is pending.  Note that
		 * we already know we're not in pm_ioctl() due to framework
		 * synchronization, so this is a sufficient test
		 */
		if (pm_idledown_id)
			return (DDI_FAILURE);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
pm_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (pm_instance == -1)
			return (DDI_FAILURE);
		*result = pm_dip;
		return (DDI_SUCCESS);

	case DDI_INFO_DEVT2INSTANCE:
		if (pm_instance == -1)
			return (DDI_FAILURE);

		*result = (void *)pm_instance;
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}


/*ARGSUSED1*/
static int
pm_open(dev_t *devp, int flag, int otyp, cred_t *cr)
{
	int		clone;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	mutex_enter(&pm_clone_lock);
	for (clone = 1; clone < PM_MAX_CLONE; clone++)
		if (!pm_clones[clone])
			break;

	if (clone == PM_MAX_CLONE) {
		mutex_exit(&pm_clone_lock);
		return (ENXIO);
	}

	*devp = makedevice(getmajor(*devp), (pm_instance << 8) + clone);
	pm_clones[clone] = 1;
	mutex_exit(&pm_clone_lock);

	return (0);
}

/*ARGSUSED1*/
static int
pm_close(dev_t dev, int flag, int otyp, cred_t *cr)
{
	int		clone;
	pm_info_t	*info;
	pm_scan_t	*cur;
	int		do_rescan;
	extern void pm_deregister_watcher(int, dev_info_t *);
	extern int pm_keeper_is_up(dev_info_t *dip, pm_info_t *info);
	int pm_all_to_normal(dev_info_t *, int, int);

	if (otyp != OTYP_CHR)
		return (EINVAL);

	clone = PM_MINOR_TO_CLONE(getminor(dev));
	PMD(PMD_CLOSE, ("pm_close: minor %x, clone %x ", getminor(dev), clone))
	rw_enter(&pm_scan_list_rwlock, RW_WRITER);	/* pmi_dev_pm_state */
	for (cur = pm_scan_list; cur != NULL; cur = cur->ps_next) {
		if (clone == cur->ps_clone) {
			info = PM_GET_PM_INFO(cur->ps_dip);
			ASSERT(info);
			PMD(PMD_CLOSE, ("found %s@%s ",
			    ddi_get_name(cur->ps_dip),
			    PM_ADDR(cur->ps_dip)))
			if (info->pmi_dev_pm_state & PM_DIRECT_NEW)
				pm_proceed(cur->ps_dip, PMP_RELEASE,
				    -1, -1, -1, clone);
			info->pmi_dev_pm_state &=
			    ~(PM_DIRECT_OLD | PM_DIRECT_NEW);
			if (pm_keeper_is_up(cur->ps_dip, info)) {
				PM_LOCK_POWER(cur->ps_dip, PMC_ALLCOMPS);
				if (pm_all_to_normal(cur->ps_dip, 1, 0) !=
				    DDI_SUCCESS) {
					PMD(PMD_KEEPS | PMD_ERROR, ("pm_close "
					    "could not bring kept %s@%s to "
					    "normal\n",
					    ddi_binding_name(cur->ps_dip),
					    PM_ADDR(cur->ps_dip)));
				}
			}
			info->pmi_clone = 0;
			cur->ps_clone = 0;
			do_rescan = 1;
		}
	}
	rw_exit(&pm_scan_list_rwlock);
	pm_clones[clone] = 0;
	PMD(PMD_IOCTL, ("pm_close: clearing pm_poll_cnt[%d] (%d)\n", clone,
	    pm_poll_cnt[clone]))
	pm_poll_cnt[clone] = 0;
	mutex_enter(&pm_clone_lock);
	pm_deregister_watcher(clone, NULL);
	mutex_exit(&pm_clone_lock);
	PMD(PMD_CLOSE, ("pm_close: rescan\n"))
	if (do_rescan)
		pm_rescan(1);
	return (0);
}

#define	PM_REQUEST	1
#define	PM_REQ		2
#define	NOSTRUCT	3
#define	OLDDIP		4
#define	NEWDIP		5
#define	NODIP		6
#define	OLDDEP		7
#define	NODEP		9
#define	NEWDEP		10
#define	PM_PSC		11

#define	CHECKPERMS	0x001
#define	SU		0x002
#define	SG		0x004
#define	OWNER		0x008

#define	INWHO		0x001
#define	INDATAINT	0x002
#define	INDATASTRING	0x004
#define	INDEP		0x008
#define	INDATAOUT	0x010
#define	INDATA	(INDATAOUT | INDATAINT | INDATASTRING | INDEP)

static struct pm_cmd_info {
	int cmd;		/* command code */
	char *name;		/* printable string */
	int supported;		/* true if still supported */
	int str_type;		/* PM_REQUEST, PM_REQ or NOSTRUCT */
	int inargs;		/* INWHO, INDATAINT, INDATASTRING, INDEP, */
				/* INDATAOUT */
	int diptype;		/* OLDDIP, NEWDIP or NODIP */
	int deptype;		/* OLDDEP, NEWDEP or NODEP */
	int permission;		/* SU, GU, or CHECKPERMS */
};

#ifdef DEBUG
char *pm_cmd_string;
int pm_cmd;
#endif

/*
 * Returns true if permission granted by credentials
 */
static int
pm_perms(int perm, cred_t *cr)
{
	if (perm == 0)			/* no restrictions */
		return (1);
	if (perm == CHECKPERMS)		/* ok for now (is checked later) */
		return (1);
	if ((perm & SU) && suser(cr))	/* root is ok */
		return (1);
	if ((perm & SG) && (cr->cr_gid == 0))	/* group 0 is ok */
		return (1);
	return (0);
}

/*ARGSUSED*/
static int
pm_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cr, int *rval_p)
{
	struct pm_cmd_info *pc_info(int);
	struct pm_cmd_info *pcip = pc_info(cmd);
	pm_request	request;
	pm_req_t	req;
	dev_info_t	*dip = NULL;
	dev_info_t	*ddip = NULL;
	dev_info_t	**new_dips;
	pm_info_t	*info = NULL, *dep_info = NULL;
	int		clone;
	size_t		length;
	int		icount = 0;
	int		i, found;
	int		comps;
	size_t		lencopied;
	int		ret = ENOTTY;
	int		curpower;
	char		who[MAXNAMELEN];
	size_t		wholen;			/* copyinstr length */
	size_t		deplen;
	char		*dep;
	pm_scan_t	*cur;
	struct pm_component *cp;
#ifdef	_MULTI_DATAMODEL
	pm_state_change32_t		*pscp32;
	pm_state_change32_t		psc32;
#endif
	pm_state_change_t		*pscp;
	pm_state_change_t		psc;
	extern int	autopm_enabled;
	extern int	pm_default_idle_threshold;
	extern int	pm_system_idle_threshold;
	extern void	pm_record_thresh(pm_thresh_rec_t *);
	extern int	pm_devices;
	extern int	pm_all_to_normal(dev_info_t *, int, int);
	static char	*pm_decode_cmd(int);
	psce_t		*pm_psc_clone_to_direct(int);
	psce_t		*pm_psc_clone_to_interest(int);
	extern	void	pm_register_watcher(int, dev_info_t *);
	extern	int	pm_get_current_power(dev_info_t *, int, int *);
	extern	int	pm_interest_registered(int);
	extern	int	pm_watchers(void);
	extern	void	pm_enqueue_notify(int, dev_info_t *, int, int,
			    int);
	extern	clock_t pm_min_scan;
	extern	void	pm_all_to_default_thresholds(void);
	extern	int	pm_current_threshold(dev_info_t *, int, int *);
	static	void	pm_end_idledown(void *);
	timeout_id_t	to_id;
	extern	kmutex_t pm_rsvp_lock;
	extern void	pm_deregister_watcher(int, dev_info_t *);
	extern void	pm_unrecord_threshold(char *);
	static void	pm_discard_entries(int);
	extern int pm_keeper_is_up(dev_info_t *dip, pm_info_t *info);
#ifdef DEBUG
	extern void	prkeeps(dev_info_t *);
#endif

	PMD(PMD_IOCTL, ("pm_ioctl: %s", pm_decode_cmd(cmd)))

#ifdef DEBUG
	if (cmd == 666) {
		rw_enter(&pm_scan_list_rwlock, RW_READER);
		pm_walk_devs(pm_scan_list, print_info);
		rw_exit(&pm_scan_list_rwlock);
		return (0);
	}
	ret = 0x0badcafe;			/* sanity checking */
	pm_cmd = cmd;				/* for ASSERT debugging */
	pm_cmd_string = pm_decode_cmd(cmd);	/* for ASSERT debugging */
#endif


	if (pcip == NULL) {
		PMD(PMD_ERROR, ("pm: unknown command %d\n", cmd))
		return (ENOTTY);
	}
	if (pcip == NULL || pcip->supported == 0) {
		PMD(PMD_ERROR, ("pm: command %s no longer supported\n",
		    pcip->name))
		return (ENOTTY);
	}

	wholen = 0;
	dep = NULL;
	deplen = 0;
	if (!pm_perms(pcip->permission, cr)) {
		ret = EPERM;
		return (ret);
	}
	switch (pcip->str_type) {
	case PM_REQUEST:
		/*
		 * The old original (obsolete) commands
		 */
#ifdef	_MULTI_DATAMODEL
		if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
			pm_request32	request32;

			if (ddi_copyin((caddr_t)arg, &request32,
			    sizeof (request32), mode) != 0) {
				PMD(PMD_ERROR, ("%s ddi_copyin EFAULT\n\n",
				    pm_decode_cmd(cmd)))
				return (EFAULT);
			}
			request.select = request32.select;
			request.level = request32.level;
			request.size = request32.size;
			if (pcip->inargs & INWHO) {
				ret = copyinstr((char *)request32.who, who,
				    MAXNAMELEN, &wholen);
				if (ret) {
					PMD(PMD_ERROR, ("%s copyinstr fails "
					    "returning %d\n",
					    pm_decode_cmd(cmd), ret))
					return (ret);
				}
				request.who = who;
			}
			PMD(PMD_IOCTL, (" %s\n", request.who))
			switch (pcip->diptype) {
			case OLDDIP:
				if (!(dip =
				    pm_name_to_dip(request.who, 0))) {
					PMD(PMD_ERROR, ("pm_name_to_dip %s "
					    "failed\n", request.who))
					return (ENODEV);
				}
				if (pcip->permission == CHECKPERMS) {
					if (ret =
					    pm_check_permission(request.who,
						cr)) {
						PMD(PMD_ERROR | PMD_DPM,
						    ("%s pm_check_permission "
						    "fails\n",
						    pm_decode_cmd(cmd)))
						break;
					}
				}
				break;
			default:
				/*
				 * Internal error, invalid ioctl description
				 */
				cmn_err(CE_PANIC, "old struct but not old dip? "
				    "cmd %d (%s)\n", cmd, pcip->name);
				/* NOTREACHED */
			}
			if (pcip->inargs & INDATASTRING) {
				ASSERT(!(pcip->inargs & INDATAINT));
				if (request32.dependent != NULL) {
					size_t dummy;
					caddr_t r32dep =
					    (caddr_t)request32.dependent;
					deplen = MAXNAMELEN;
					dep = kmem_alloc(deplen, KM_SLEEP);
					if (copyinstr(r32dep, dep, deplen,
					    &dummy)) {
						PMD(PMD_ERROR, (" 0x%p dep "
						    "size %x, EFAULT\n",
						    (void *)r32dep, deplen))
						kmem_free(dep, deplen);
						return (EFAULT);
					}
#ifdef DEBUG
					else {
						PMD(PMD_DEP, ("dep %s\n", dep))
					}
#endif
					/*
					 * there is only one consumer of char
					 * string data using the old pm_request
					 * struct, and that is for dependent
					 */
					ASSERT(pcip->deptype == OLDDEP);
					if (!(ddip = pm_name_to_dip(dep, 0))) {
						PMD(PMD_ERROR, ("pm_name_to_dip"
						    " %s failed\n", dep))
						kmem_free(dep, deplen);
						return (ENODEV);
					}
					kmem_free(dep, deplen);
					request.dependent = NULL;
					req.datasize = 0;
				} else {
					PMD(PMD_ERROR, ("no dependent\n"))
					return (EINVAL);
				}
			}
		} else
#endif /* _MULTI_DATAMODEL */
		{
			if (ddi_copyin((caddr_t)arg, &request,
			    sizeof (request), mode) != 0) {
				PMD(PMD_ERROR, ("%s ddi_copyin EFAULT\n\n",
				    pm_decode_cmd(cmd)))
				return (EFAULT);
			}
			if (pcip->inargs & INWHO) {
				ret = copyinstr((char *)request.who, who,
				    MAXNAMELEN, &wholen);
				if (ret) {
					PMD(PMD_ERROR, ("%s copyinstr fails "
					    "returning %d\n",
					    pm_decode_cmd(cmd), ret))
					return (ret);
				}
				request.who = who;
			}
			PMD(PMD_IOCTL, (" %s\n", request.who))
			switch (pcip->diptype) {
			case OLDDIP:
				if (!(dip =
				    pm_name_to_dip(request.who, 0))) {
					PMD(PMD_ERROR, ("pm_name_to_dip %s "
					    "failed\n", request.who))
					return (ENODEV);
				}
				break;
			default:
				/*
				 * Internal error, invalid ioctl description
				 */
				cmn_err(CE_PANIC, "old struct needs new dip? "
				    "cmd %d (%s)\n", cmd, pcip->name);
				/* NOTREACHED */
			}
			if (pcip->inargs & INDATASTRING) {
				ASSERT(!(pcip->inargs & INDATAINT));
				if (request.dependent != NULL) {
					size_t dummy;
					caddr_t rdep = request.dependent;
					deplen = MAXNAMELEN;
					dep = kmem_alloc(deplen, KM_SLEEP);
					if (copyinstr(rdep, dep, deplen,
					    &dummy)) {
						PMD(PMD_ERROR, (" 0x%p dep "
						    "size %lu, EFAULT\n",
						    (void *)rdep, deplen))
						kmem_free(dep, deplen);
						return (EFAULT);
					}
#ifdef DEBUG
					else {
						PMD(PMD_DEP, ("dep %s\n", dep))
					}
#endif
					/*
					 * there is only one consumer of char
					 * string data using the old pm_request
					 * struct, and that is for dependent
					 */
					ASSERT(pcip->deptype == OLDDEP);
					if (!(ddip = pm_name_to_dip(dep, 0))) {
						PMD(PMD_ERROR, ("pm_name_to_dip"
						    " %s failed\n", dep))
						kmem_free(dep, deplen);
						return (ENODEV);
					}
					kmem_free(dep, deplen);
					request.dependent = NULL;
					req.datasize = 0;
				} else {
					PMD(PMD_ERROR, ("no dependent\n"))
					return (EINVAL);
				}
			}
		}
		/*
		 * Now we have all the common input handled for the old obsolete
		 * commands, we can do the actual work
		 */
		switch (cmd) {
		case PM_DISABLE_AUTOPM:
		    {
			ASSERT(dip);
			PMD(PMD_DPM, ("%s %s\n", pm_decode_cmd(cmd),
			    request.who))
			if (ret = pm_check_permission(request.who, cr)) {
				PMD(PMD_ERROR | PMD_DPM,
				    ("%s pm_check_permission fails\n",
				    pm_decode_cmd(cmd)))
				break;
			}

			clone = PM_MINOR_TO_CLONE(getminor(dev));
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PMD(PMD_ERROR | PMD_DPM, ("%s ENODEV\n",
				    pm_decode_cmd(cmd)))
				ret = ENODEV;
				break;
			}
			if (PM_ISDIRECT(dip)) {
				PMD(PMD_ERROR | PMD_DPM, ("%s EBUSY\n",
				    pm_decode_cmd(cmd)))
				ret = EBUSY;
				break;
			}
			/*
			 * Record the clone index against the entry on
			 * pm_scan_list; hold writers lock because we modify
			 * pmi_dev_pm_state
			 */
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				if (cur->ps_dip == dip) {
					if (cur->ps_clone) {
						PMD(PMD_ERROR | PMD_DPM,
						    ("%s: %s@%s: EBUSY(2)\n",
						    pm_decode_cmd(cmd),
						    ddi_binding_name(dip),
						    PM_ADDR(dip)))
						ret = EBUSY;
						break;
					}
					cur->ps_clone = clone;
					break;
				}
			}
			if (ret) {		/* error'd out of for loop */
				rw_exit(&pm_scan_list_rwlock);
				break;
			}
			info->pmi_dev_pm_state |= PM_DIRECT_OLD;
			info->pmi_clone = clone;
			rw_exit(&pm_scan_list_rwlock);
			ret = 0;
			break;
		}

		case PM_GET_NORM_PWR:
		{
			int normal;
			ASSERT(dip);
			PMD(PMD_NORM, ("%s %s component %d ",
			    pm_decode_cmd(cmd), request.who, request.select))

			/*
			 * Because the man page commits us to this return value
			 */
			if (DEVI(dip)->devi_pm_num_components == 0) {
				PMD(PMD_ERROR, ("returns EIO"))
				ret = EIO;
				break;
			}

			normal =  pm_get_normal_power(dip, request.select);

			if (normal == DDI_FAILURE) {
				PMD(PMD_ERROR, ("returns EINVAL"))
				ret = EINVAL;
				break;
			}
			PMD(PMD_NORM, ("returns %d", normal))
			*rval_p = normal;
			ret = 0;
			break;
		}
		case PM_GET_CUR_PWR:
			PMD(PMD_IOCTL, ("component %d ", request.select))
			if (pm_get_current_power(dip, request.select,
			    rval_p) != DDI_SUCCESS) {
				PMD(PMD_ERROR | PMD_DPM, ("%s EINVAL\n",
				    pm_decode_cmd(cmd)))
				ret = EINVAL;
				break;
			}
			PMD(PMD_IOCTL, ("returns %d ", *rval_p))
			PMD(PMD_DPM, ("%s %s comp %d returns %d\n",
			    pm_decode_cmd(cmd), request.who, request.select,
			    *rval_p))
			if (*rval_p == PM_LEVEL_UNKNOWN)
				ret = EAGAIN;
			else
				ret = 0;
			break;

		case PM_SET_CUR_PWR:
			PMD(PMD_DPM, ("%s %s componet %d to level %d\n",
			    pm_decode_cmd(cmd), request.who, request.select,
			    request.level))
			if (ret = pm_check_permission(request.who, cr)) {
				PMD(PMD_ERROR, ("pm_check_permissions fails "
				    "for %s on %s\n", pm_decode_cmd(cmd),
				    request.who))
				break;
			}

			if (0 > request.select ||
			    request.select >=
			    DEVI(dip)->devi_pm_num_components ||
			    request.level < 0) {
				PMD(PMD_ERROR | PMD_DPM, ("%s(select range %d) "
				    "%s fails\n", pm_decode_cmd(cmd),
				    request.select, request.who))
				ret = EINVAL;
				break;
			}

			if (!pm_valid_power(dip,
			    request.select, request.level)) {
				PMD(PMD_ERROR | PMD_DPM, ("%s: level %d not "
				    "valid for comp %d of %s\n",
				    pm_decode_cmd(cmd), request.level,
				    request.select, request.who))
				ret = EINVAL;
				break;
			}
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PMD(PMD_ERROR | PMD_DPM, ("%s ENODEV\n",
				    pm_decode_cmd(cmd)))
				ret = ENODEV;
				break;
			}
			PM_LOCK_POWER(dip, request.select);
			if (pm_get_current_power(dip, request.select,
			    &curpower) != DDI_SUCCESS) {
				PM_UNLOCK_POWER(dip, request.select);
				PMD(PMD_ERROR | PMD_DPM, ("%s EINVAL\n",
				    pm_decode_cmd(cmd)))
				ret = EINVAL;
				break;
			}
			if (curpower == request.level) {
				PMD(PMD_DPM, ("%s component %d already at "
				    "level %d\n", request.who, request.select,
				    request.level));
				/*
				 * See Bug id 4265596 for why we don't do this
				 * for old style devices too.
				 */
				if (!PM_ISBC(dip)) {
					PM_UNLOCK_POWER(dip, request.select);
					ret = 0;
					break;
				}
			}
			if (pm_set_power(dip, request.select,
			    request.level, curpower, info, 1) != DDI_SUCCESS) {
				PM_UNLOCK_POWER(dip, request.select);
				PMD(PMD_ERROR | PMD_DPM, ("%s(pm_set_power) %s "
				    "fails\n", pm_decode_cmd(cmd), request.who))
				ret = EINVAL;
				break;
			}
			PM_UNLOCK_POWER(dip, request.select);
			if (pm_watchers()) {
				mutex_enter(&pm_rsvp_lock);
				pm_enqueue_notify(PSC_HAS_CHANGED, dip,
				    request.select, request.level, curpower);
				mutex_exit(&pm_rsvp_lock);
			}
			PMD(PMD_RESCAN | PMD_DPM, ("pm_ioctl: SET_CUR_PWR "
			    "rescan\n"))
			pm_rescan(1);
			*rval_p = 0;
			ret = 0;
			break;

		case PM_REENABLE_AUTOPM:
		{
			PMD(PMD_DPM, ("%s %s\n", pm_decode_cmd(cmd),
			    request.who))
			clone = PM_MINOR_TO_CLONE(getminor(dev));
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PMD(PMD_ERROR | PMD_DPM, ("%s ENODEV\n",
				    pm_decode_cmd(cmd)))
				ret = ENODEV;
				break;
			}
			if (!(info->pmi_dev_pm_state & PM_DIRECT_OLD) ||
			    info->pmi_clone != clone) {
				PMD(PMD_ERROR | PMD_DPM, ("%s(not direct or "
				    "owner) %s fails; clone %d, owner %d\n",
				    pm_decode_cmd(cmd), request.who,
				    clone, info->pmi_clone))
				ret = EINVAL;
				break;
			}
			/*
			 * We change pmi_dev_pm_state, so must be a writer
			 */
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				if (cur->ps_dip == dip) {
					if (cur->ps_clone != clone) {
						PMD(PMD_ERROR | PMD_DPM,
						    ("%s: (%s@%s) EINVAL\n",
						    pm_decode_cmd(cmd),
						    ddi_binding_name(dip),
						    PM_ADDR(dip)))
						ret = EINVAL;
						break;
					} else
						cur->ps_clone = 0;
					break;
				}
			}
			if (ret) {		/* error'd out of for loop */
				rw_exit(&pm_scan_list_rwlock);
				break;
			}
			info->pmi_dev_pm_state &= ~PM_DIRECT_OLD;
			info->pmi_clone = 0;
			if (pm_keeper_is_up(dip, info)) {
				PM_LOCK_POWER(dip, PMC_ALLCOMPS);
				if (pm_all_to_normal(cur->ps_dip, 1, 0) !=
				    DDI_SUCCESS) {
					PMD(PMD_KEEPS | PMD_ERROR, ("%s could "
					    "not bring kept %s@%s to normal\n",
					    pm_decode_cmd(cmd),
					    ddi_binding_name(dip),
					    PM_ADDR(dip)));
				}
			}
			rw_exit(&pm_scan_list_rwlock);
			PMD(PMD_RESCAN | PMD_DPM, ("pm_ioctl: %s rescan\n",
			    pm_decode_cmd(cmd)))
			pm_rescan(1);
			ret = 0;
			break;
		}

		default:
			/*
			 * Internal ioctl description error
			 */
			cmn_err(CE_PANIC, "unknown old command %d\n", cmd);
			/* NOTREACHED */

		case PM_ADD_DEP:
		{
			major_t major;
			if (dip == ddip) {
				PMD(PMD_ERROR, ("self dependency--EBUSY\n"))
				ret = EBUSY;
				break;
			}
			/*
			 * Dependency information is protected by
			 * pm_scan_list_rwlock
			 */
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			PM_LOCK_DIP(dip);
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PM_UNLOCK_DIP(dip);
				rw_exit(&pm_scan_list_rwlock);
				PMD(PMD_ERROR, ("%s no info\n",
				    pm_decode_cmd(cmd)))
				ret = EINVAL;
				break;
			}
			if ((dep_info = PM_GET_PM_INFO(ddip)) == NULL) {
				PM_UNLOCK_DIP(dip);
				rw_exit(&pm_scan_list_rwlock);
				PMD(PMD_ERROR | PMD_DEP, ("dependent not power "
				    "managed\n"))
				ret = ENODEV;
				break;
			}

			for (i = 0, found = 0; i < info->pmi_nkeptupby; i++) {
				if (info->pmi_kupbydips[i] == ddip) {
					found = 1;
					break;
				}
			}
			if (found) {
				PM_UNLOCK_DIP(dip);
				rw_exit(&pm_scan_list_rwlock);
				PMD(PMD_ERROR, ("%s %s EBUSY\n",
				    pm_decode_cmd(cmd), request.who))
				ret = EBUSY;
				break;
			}
			major = ddi_name_to_major(ddi_get_name(dip));
			ASSERT(major != (major_t)-1);
#ifdef DEBUG
			ASSERT(ddi_hold_installed_driver(major));
#else
			ddi_hold_installed_driver(major);
#endif
			PMD(PMD_KEEPS, ("PM_ADD_DEP held %s (%s@%s)\n",
			    ddi_major_to_name(major), ddi_binding_name(dip),
			    PM_ADDR(dip)))
			length = info->pmi_nkeptupby * sizeof (dev_info_t *);
			new_dips = kmem_alloc(length + sizeof (dev_info_t *),
			    KM_SLEEP);
			if (info->pmi_nkeptupby) {
				bcopy(info->pmi_kupbydips,  new_dips, length);
				kmem_free(info->pmi_kupbydips, length);
			}
			new_dips[info->pmi_nkeptupby] = ddip;
			info->pmi_kupbydips = new_dips;
			info->pmi_nkeptupby++;

			length = dep_info->pmi_nwekeepup *
			    sizeof (dev_info_t *);
			new_dips = kmem_alloc(length  + sizeof (dev_info_t *),
			    KM_SLEEP);
			if (dep_info->pmi_nwekeepup) {
				bcopy(dep_info->pmi_wekeepdips, new_dips,
				    length);
				kmem_free(dep_info->pmi_wekeepdips, length);
			}
			new_dips[dep_info->pmi_nwekeepup] = dip;
			dep_info->pmi_wekeepdips = new_dips;
			dep_info->pmi_nwekeepup++;
#ifdef DEBUG
			if (pm_debug & PMD_KEEPS) {
				PMD(PMD_KEEPS, ("after PM_ADD_DEP %s@%s "
				    "(dep %s@%s)\n", ddi_binding_name(dip),
				    PM_ADDR(dip), ddi_binding_name(ddip),
				    PM_ADDR(ddip)))
				prkeeps(dip);
				prkeeps(ddip);
			}
#endif
			PM_UNLOCK_DIP(dip);
			rw_exit(&pm_scan_list_rwlock);
			*rval_p = 0;
			ret = 0;
			break;
		}

		case PM_SET_THRESHOLD:
		    {
			extern krwlock_t pm_dep_rwlock;
			int pm_dep_specd(dev_info_t *, int);
			/*
			 * Take charge of it
			 */
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				/*
				 * This is what the old implementation did.
				 * We are bug compatible.
				 */
				ret = pm_manage(dip, 1);
				if (ret) {
					PMD(PMD_ERROR, ("%s: pm_manage %s "
					    "returns %d, ioctl rets ENODEV\n",
					    pm_decode_cmd(cmd), request.who,
					    ret))
					return (ENODEV);
				}
			}
			info = PM_GET_PM_INFO(dip);
			ASSERT(info);	/* because pm_manage returned success */
			if (0 > request.select ||
			    (comps = PM_NUMCMPTS(dip)) < request.select) {
				PMD(PMD_ERROR, ("%s EINVAL (request.select %d, "
				    "comps %d)\n", pm_decode_cmd(cmd),
				    request.select, comps))
				return (EINVAL);
			}
			/*
			 * We only do this to old-style drivers
			 */
			if (!PM_ISBC(dip)) {
				PMD(PMD_ERROR, ("PM_SET_THRESHOLD: %s not BC\n",
				    request.who))
				return (EINVAL);
			}
			/*
			 * We don't record old style device thresholds for
			 * later, we just do them now
			 */
			i = request.select;
			cp = &DEVI(dip)->devi_pm_components[i];
			PMD(PMD_THRESH, ("Setting threshold 1 for %s@%s[%d] to "
			    "%x\n", ddi_binding_name(dip),
			    PM_ADDR(dip), i, request.level))
			cp->pmc_comp.pmc_thresh[0] = INT_MAX;
			cp->pmc_comp.pmc_thresh[1] = request.level;
			/* Not on list yet, so don't need to lock dip */
			rw_enter(&pm_dep_rwlock, RW_READER);
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			info->pmi_dev_pm_state |= PM_SCAN;
			ret = pm_enqueue_scan(dip);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				if (cur->ps_dip == dip)
					continue;
				(void) pm_dep_specd(cur->ps_dip, 0);
			}
			rw_exit(&pm_scan_list_rwlock);
			rw_exit(&pm_dep_rwlock);
			mutex_enter(&pm_scan_lock);
			bcpm_enabled++;
			mutex_exit(&pm_scan_lock);
			PMD(PMD_RESCAN, ("pm_ioctl: pm_manage() rescan\n"))
			pm_rescan(1);
			if (!ret)
				pm_devices++;
			*rval_p = 0;
			ret = 0;
			break;
		    }
		}
		break;

	case PM_REQ:
		/*
		 * The new commands including some with almost identical
		 * functionality as the old ones
		 */
#ifdef	_MULTI_DATAMODEL
		if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
			pm_req32_t	req32;

			if (ddi_copyin((caddr_t)arg, &req32,
			    sizeof (req32), mode) != 0) {
				PMD(PMD_ERROR, ("%s ddi_copyin EFAULT\n\n",
				    pm_decode_cmd(cmd)))
				return (EFAULT);
			}
			req.component = req32.component;
			req.value = req32.value;
			req.datasize = req32.datasize;
			if (pcip->inargs & INWHO) {
				ret = copyinstr((char *)req32.physpath, who,
				    MAXNAMELEN, &wholen);
				if (ret) {
					PMD(PMD_ERROR, ("%s copyinstr fails "
					    "returning %d\n",
					    pm_decode_cmd(cmd), ret))
					return (ret);
				}
				req.physpath = who;
			}
			PMD(PMD_IOCTL, (" %s\n", req.physpath))
			if (pcip->inargs & INDATA) {
				req.data = (void *)req32.data;
				req.datasize = req32.datasize;
			} else {
				req.data = NULL;
				req.datasize = 0;
			}
			switch (pcip->diptype) {
			case NEWDIP:
				if (!(dip =
				    pm_name_to_dip(req.physpath, 1))) {
					PMD(PMD_ERROR, ("pm_name_to_dip %s "
					    "failed\n", req.physpath))
					return (ENODEV);
				}
				break;
			case NODIP:
				break;
			default:
				/*
				 * Internal error, invalid ioctl description
				 */
				cmn_err(CE_PANIC, "new struct needs old dip? "
				    "cmd %d (%s)\n", cmd, pcip->name);
				/* NOTREACHED */
			}
			if (pcip->inargs & INDATAINT) {
				int32_t int32buf;
				int32_t *i32p;
				int *ip;
				icount = req32.datasize / sizeof (int32_t);
				if (icount <= 0) {
					PMD(PMD_ERROR, ("%s datasize 0 or neg "
					    "EFAULT\n\n", pm_decode_cmd(cmd)))
					return (EFAULT);
				}
				ASSERT(!(pcip->inargs & INDATASTRING));
				req.datasize = icount * sizeof (int);
				req.data = kmem_alloc(req.datasize, KM_SLEEP);
				ip = req.data;
				for (i = 0, i32p = (int32_t *)req32.data;
				    i < icount; i++, i32p++) {
					if (ddi_copyin((void *)i32p, &int32buf,
					    sizeof (int32_t), mode)) {
						kmem_free(req.data,
						    req.datasize);
						PMD(PMD_ERROR, ("%s entry %d "
						    "EFAULT\n",
						    pm_decode_cmd(cmd), i))
						return (EFAULT);
					}
					*ip++ = (int)int32buf;
				}
			}
			if (pcip->inargs & INDATASTRING) {
				ASSERT(!(pcip->inargs & INDATAINT));
				ASSERT(pcip->deptype == NEWDEP);
				if (req32.data != NULL) {
					size_t dummy;
					deplen = MAXNAMELEN;
					dep = kmem_alloc(deplen, KM_SLEEP);
					if (copyinstr((caddr_t)req32.data,
					    dep, deplen, &dummy)) {
						PMD(PMD_ERROR, (" 0x%p dep "
						    "size %x, EFAULT\n",
						    (void *)req.data, deplen))
						kmem_free(dep, deplen);
						return (EFAULT);
					}
#ifdef DEBUG
					else {
						PMD(PMD_DEP, ("dep %s\n", dep))
					}
#endif
				} else {
					PMD(PMD_ERROR, ("no dependent\n"))
					return (EINVAL);
				}
			}
		} else
#endif /* _MULTI_DATAMODEL */
		{
			if (ddi_copyin((caddr_t)arg,
			    &req, sizeof (req), mode) != 0) {
				PMD(PMD_ERROR, ("%s ddi_copyin EFAULT\n\n",
				    pm_decode_cmd(cmd)))
				return (EFAULT);
			}
			if (pcip->inargs & INWHO) {
				ret = copyinstr((char *)req.physpath, who,
				    MAXNAMELEN, &wholen);
				if (ret) {
					PMD(PMD_ERROR, ("%s copyinstr fails "
					    "returning %d\n",
					    pm_decode_cmd(cmd), ret))
					return (ret);
				}
				req.physpath = who;
			}
			PMD(PMD_IOCTL, (" %s\n", req.physpath))
			if (!(pcip->inargs & INDATA)) {
				req.data = NULL;
				req.datasize = 0;
			}
			switch (pcip->diptype) {
			case NEWDIP:
				if (!(dip =
				    pm_name_to_dip(req.physpath, 1))) {
					PMD(PMD_ERROR, ("pm_name_to_dip %s "
					    "failed\n", req.physpath))
					return (ENODEV);
				}
				break;
			case NODIP:
				break;
			default:
				cmn_err(CE_PANIC, "old struct needs new dip? "
				    "cmd %d (%s)\n", cmd, pcip->name);
				ASSERT(0);
			}
			if (pcip->inargs & INDATAINT) {
				int *ip;

				ASSERT(!(pcip->inargs & INDATASTRING));
				ip = req.data;
				icount = req.datasize / sizeof (int);
				if (icount <= 0) {
					PMD(PMD_ERROR, ("%s datasize 0 or neg "
					    "EFAULT\n\n", pm_decode_cmd(cmd)))
					return (EFAULT);
				}
				req.data = kmem_alloc(req.datasize, KM_SLEEP);
				if (ddi_copyin((caddr_t)ip, req.data,
				    req.datasize, mode) != 0) {
					PMD(PMD_ERROR, ("%s ddi_copyin "
					    "EFAULT\n\n", pm_decode_cmd(cmd)))
					return (EFAULT);
				}
			}
			if (pcip->inargs & INDATASTRING) {
				ASSERT(!(pcip->inargs & INDATAINT));
				ASSERT(pcip->deptype == NEWDEP);
				if (req.data != NULL) {
					size_t dummy;
					deplen = MAXNAMELEN;
					dep = kmem_alloc(deplen, KM_SLEEP);
					if (copyinstr((caddr_t)req.data,
					    dep, deplen, &dummy)) {
						PMD(PMD_ERROR, (" 0x%p dep "
						    "size %lu, EFAULT\n",
						    (void *)req.data, deplen))
						kmem_free(dep, deplen);
						return (EFAULT);
					}
#ifdef DEBUG
					else {
						PMD(PMD_DEP, ("dep %s\n", dep))
					}
#endif
				} else {
					PMD(PMD_ERROR, ("no dependent\n"))
					return (EINVAL);
				}
			}
		}
		/*
		 * Now we've got all the args in for the commands that
		 * use the new pm_req struct.
		 */
		switch (cmd) {
		case PM_REPARSE_PM_PROPS:
		{
			struct dev_ops	*drv;
			struct cb_ops	*cb;
			void		*propval;
			int length;
			/*
			 * This ioctl is provided only for the ddivs pm test.
			 * We only do it to a driver which explicitly allows
			 * us to do so by exporting a pm-reparse-ok property.
			 * We only care whether the property exists or not.
			 */
			if ((drv = ddi_get_driver(dip)) == NULL) {
				ret = EINVAL;
				break;
			}
			if ((cb = drv->devo_cb_ops) != NULL) {
				if ((*cb->cb_prop_op)(DDI_DEV_T_ANY, dip,
				    PROP_LEN_AND_VAL_ALLOC, (DDI_PROP_CANSLEEP |
				    DDI_PROP_DONTPASS | DDI_PROP_NOTPROM),
				    "pm-reparse-ok", (caddr_t)&propval,
				    &length) != DDI_SUCCESS) {
					ret = EINVAL;
					break;
				}
			} else if (ddi_prop_op(DDI_DEV_T_ANY, dip,
			    PROP_LEN_AND_VAL_ALLOC, (DDI_PROP_CANSLEEP |
			    DDI_PROP_DONTPASS | DDI_PROP_NOTPROM),
			    "pm-reparse-ok", (caddr_t)&propval,
			    &length) != DDI_SUCCESS) {
				ret = EINVAL;
				break;
			}
			kmem_free(propval, length);
			ret =  e_new_pm_props(dip);
			break;
		}

		case PM_GET_DEVICE_THRESHOLD:
			PM_LOCK_DIP(dip);
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PM_UNLOCK_DIP(dip);
				PMD(PMD_ERROR, ("%s ENODEV\n",
				    pm_decode_cmd(cmd)))
				return (ENODEV);
			}
			*rval_p = DEVI(dip)->devi_pm_dev_thresh;
			PM_UNLOCK_DIP(dip);
			ret = 0;
			break;

		case PM_DIRECT_PM:
		{
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PMD(PMD_ERROR | PMD_DPM, ("%s ENODEV\n",
				    pm_decode_cmd(cmd)))
				return (ENODEV);
			}
			if (PM_ISDIRECT(dip)) {
				PMD(PMD_ERROR | PMD_DPM, ("%s EBUSY\n",
				    pm_decode_cmd(cmd)))
				return (EBUSY);
			}
			if (ret = pm_check_permission(req.physpath, cr)) {
				PMD(PMD_ERROR | PMD_DPM,
				    ("%s pm_check_permission fails\n",
				    pm_decode_cmd(cmd)))
				return (ret);
			}
			clone = PM_MINOR_TO_CLONE(getminor(dev));
			/*
			 * Record the clone index against the entry on
			 * pm_scan_list;  We are a writer because we
			 * may modify pmi_dev_pm_state
			 */
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				if (cur->ps_dip == dip) {
					if (cur->ps_clone) {
						rw_exit(&pm_scan_list_rwlock);
						PMD(PMD_ERROR | PMD_DPM,
						    ("%s: %s@%s: EBUSY\n",
						    pm_decode_cmd(cmd),
						    ddi_binding_name(dip),
						    PM_ADDR(dip)))
						return (EBUSY);
					}
					cur->ps_clone = clone;
					break;
				}
			}
			info->pmi_dev_pm_state |= PM_DIRECT_NEW;
			info->pmi_clone = clone;
			PMD(PMD_DPM, ("info %p, pmi_clone %d\n", (void *)info,
			    clone))
			rw_exit(&pm_scan_list_rwlock);
			mutex_enter(&pm_clone_lock);
			pm_register_watcher(clone, dip);
			mutex_exit(&pm_clone_lock);
			return (0);
		}

		case PM_RELEASE_DIRECT_PM:
		{
			clone = PM_MINOR_TO_CLONE(getminor(dev));
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PMD(PMD_ERROR | PMD_DPM, ("%s ENODEV\n",
				    pm_decode_cmd(cmd)))
				return (ENODEV);
			}
			if (!(info->pmi_dev_pm_state & PM_DIRECT_NEW) ||
			    info->pmi_clone != clone) {
				PMD(PMD_ERROR | PMD_DPM,
				    ("%s(not direct or owner) "
				    "%s fails; clone %d, owner %d\n",
				    pm_decode_cmd(cmd), req.physpath,
				    clone, info->pmi_clone))
				return (EINVAL);
			}
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				if (cur->ps_dip == dip) {
					if (cur->ps_clone != clone) {
						rw_exit(&pm_scan_list_rwlock);
						PMD(PMD_ERROR | PMD_DPM,
						    ("%s: (%s@%s) EINVAL\n",
						    pm_decode_cmd(cmd),
						    ddi_binding_name(dip),
						    PM_ADDR(dip)))
						ret = EINVAL;
						break;
					} else
						cur->ps_clone = 0;
					break;
				}
			}
			if (ret) {		/* error'd out of for loop */
				rw_exit(&pm_scan_list_rwlock);
				break;
			}
			info->pmi_dev_pm_state &= ~PM_DIRECT_NEW;
			info->pmi_clone = 0;
			if (pm_keeper_is_up(dip, info)) {
				PM_LOCK_POWER(dip, PMC_ALLCOMPS);
				if (pm_all_to_normal(cur->ps_dip, 1, 0) !=
				    DDI_SUCCESS) {
					PMD(PMD_KEEPS | PMD_ERROR, ("%s could "
					    "not bring kept %s@%s to normal\n",
					    pm_decode_cmd(cmd),
					    ddi_binding_name(dip),
					    PM_ADDR(dip)));
				}
			}
			rw_exit(&pm_scan_list_rwlock);
			pm_discard_entries(clone);
			pm_deregister_watcher(clone, dip);
			pm_proceed(dip, PMP_RELEASE, -1, -1, -1, clone);
			PMD(PMD_RESCAN | PMD_DPM, ("pm_ioctl: %s rescan\n",
			    pm_decode_cmd(cmd)))
			pm_rescan(1);
			return (0);
		}


		case PM_SET_CURRENT_POWER:
			PMD(PMD_DPM, ("%s %s componet %d to value %d\n",
			    pm_decode_cmd(cmd), req.physpath, req.component,
			    req.value))
			if (0 > req.component || req.component >=
			    DEVI(dip)->devi_pm_num_components ||
			    req.value < 0) {
				PMD(PMD_ERROR | PMD_DPM,
				    ("%s(component range %d) %s fails\n",
				    pm_decode_cmd(cmd),
				    req.component, req.physpath))
				ret = EINVAL;
				break;
			}

			clone = PM_MINOR_TO_CLONE(getminor(dev));
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PMD(PMD_ERROR | PMD_DPM, ("%s ENODEV\n",
				    pm_decode_cmd(cmd)))
				return (ENODEV);
			}
			if (!(info->pmi_dev_pm_state & PM_DIRECT_NEW) ||
			    info->pmi_clone != clone) {
				PMD(PMD_ERROR | PMD_DPM,
				    ("%s(not direct or owner) "
				    "%s fails; clone %d, owner %d\n",
				    pm_decode_cmd(cmd), req.physpath,
				    clone, info->pmi_clone))
				return (EINVAL);
			}

			if (!pm_valid_power(dip,
			    req.component, req.value)) {
				PMD(PMD_ERROR | PMD_DPM, ("%s: value %d not "
				    "valid for comp %d of %s\n",
				    pm_decode_cmd(cmd), req.value,
				    req.component, req.physpath))
				ret = EINVAL;
				break;
			}

			PM_LOCK_POWER(dip, req.component);
			if (pm_get_current_power(dip, req.component,
			    &curpower) != DDI_SUCCESS) {
				PM_UNLOCK_POWER(dip, req.component);
				PMD(PMD_ERROR | PMD_DPM, ("%s EINVAL\n",
				    pm_decode_cmd(cmd)))
				ret = EINVAL;
				break;
			}
			if (curpower == req.value) {
				PMD(PMD_DPM, ("%s component %d already at "
				    "value %d\n", req.physpath, req.component,
				    req.value));
				/*
				 * See Bug id 4265596 for why we don't do this
				 * for old style devices too.
				 */
				if (!PM_ISBC(dip)) {
					PM_UNLOCK_POWER(dip, req.component);
					ret = 0;
					break;
				}
			}
			if (pm_set_power(dip, req.component,
			    req.value, curpower, info, 1) != DDI_SUCCESS) {
				PM_UNLOCK_POWER(dip, req.component);
				PMD(PMD_ERROR | PMD_DPM, ("%s(pm_set_power) %s "
				    "fails\n", pm_decode_cmd(cmd),
				    req.physpath))
				ret = EINVAL;
				break;
			}
			PM_UNLOCK_POWER(dip, req.component);
			pm_proceed(dip, PMP_SETPOWER, req.component, req.value,
			    curpower, -1);
			if (pm_watchers()) {
				mutex_enter(&pm_rsvp_lock);
				pm_enqueue_notify(PSC_HAS_CHANGED, dip,
				    req.component, req.value, curpower);
				mutex_exit(&pm_rsvp_lock);
			}
			PMD(PMD_RESCAN | PMD_DPM, ("pm_ioctl: SET_CUR_PWR "
			    "rescan\n"))
			pm_rescan(1);
			*rval_p = 0;
			ret = 0;
			break;

		case PM_GET_FULL_POWER:
		{
			int normal;
			ASSERT(dip);
			PMD(PMD_NORM, ("%s %s component %d ",
			    pm_decode_cmd(cmd), req.physpath, req.component))
			normal =  pm_get_normal_power(dip, req.component);

			if (normal == DDI_FAILURE) {
				PMD(PMD_ERROR | PMD_NORM, ("returns EINVAL"))
				ret = EINVAL;
				break;
			}
			*rval_p = normal;
			PMD(PMD_NORM, ("returns %d", normal))
			ret = 0;
			break;
		}

		case PM_GET_CURRENT_POWER:
			if (pm_get_current_power(dip, req.component,
			    rval_p) != DDI_SUCCESS) {
				PMD(PMD_ERROR | PMD_DPM, ("%s EINVAL\n",
				    pm_decode_cmd(cmd)))
				ret = EINVAL;
				break;
			}
			PMD(PMD_DPM, ("%s %s comp %d returns %d\n",
			    pm_decode_cmd(cmd), req.physpath, req.component,
			    *rval_p))
			if (*rval_p == PM_LEVEL_UNKNOWN)
				ret = EAGAIN;
			else
				ret = 0;
			break;

		case PM_GET_TIME_IDLE:
		{
			time_t timestamp;
			int comp = req.component;
			if (comp < 0 || comp > PM_NUMCMPTS(dip) - 1) {
				PMD(PMD_ERROR, ("%s %s@%s component %d "
				    "> numcmpts - 1 %d--EINVAL\n",
				    pm_decode_cmd(cmd),
				    ddi_binding_name(dip),
				    PM_ADDR(dip), comp, PM_NUMCMPTS(dip) - 1))
				ret = EINVAL;
				break;
			}
			timestamp =
			    DEVI(dip)->devi_pm_components[comp].pmc_timestamp;
			if (timestamp) {
				time_t now;
				(void) drv_getparm(TIME, &now);
				*rval_p = (now - timestamp);
			} else {
				*rval_p = 0;
			}
			return (0);
		}

		case PM_ADD_DEPENDENT:
		{
			extern krwlock_t pm_dep_rwlock;
			major_t major;
			char *driver, *at, *cp;
			size_t size;
			int pm_record_dep(char *, char *, major_t);
			int pm_dep_specd(dev_info_t *, int);

			/*
			 * look up a dip, allowing it to not have a driver
			 * attached
			 */
			PMD(PMD_KEEPS, ("req.data %p\n", req.data))
			if (dep == NULL || dep[0] == '\0') {
				PMD(PMD_ERROR, ("dep NULL or null\n"))
				ret = EINVAL;
				return (ret);
			}
			driver = strrchr(req.physpath, '/');
			if (driver == NULL) {
				PMD(PMD_ERROR, ("no / in physpath %s\n",
				    req.physpath))
				ret = EINVAL;
				return (ret);
			}
			driver++;
			at = strchr(driver, '@');
			if (at) {
				size = (intptr_t)at - (intptr_t)driver + 1;
				cp = kmem_zalloc(size, KM_SLEEP);
				(void) strncpy(cp, driver, size - 1);
				driver = cp;
			}
			if (!driver[0]) {
				PMD(PMD_ERROR, ("req.physpath null\n"))
				if (at)
					kmem_free(cp, size);
				ret = EINVAL;
				return (ret);
			}
			major = ddi_name_to_major(driver);
			if (major == (major_t)-1) {
				PMD(PMD_ERROR, ("no major for %s\n", driver))
				if (at)
					kmem_free(cp, size);
				ret = ENODEV;
				break;
			}
			if (at)
				kmem_free(cp, size);
			if (ddi_hold_installed_driver(major) == NULL) {
				PMD(PMD_ERROR, ("can't hold %s (major %x)\n",
				    ddi_major_to_name(major), major))
				ret = ENODEV;
				break;
			}
			PMD(PMD_KEEPS, ("PM_ADD_DEPENDENT held %s\n",
			    ddi_major_to_name(major)))
			dip = pm_name_to_dip(req.physpath, 1);
			if (dip == NULL) {
				ddi_rele_driver(major);
				PMD(PMD_KEEPS,
				    ("PM_ADD_DEPENDENT released %s\n",
				    ddi_major_to_name(major)))
				PMD(PMD_ERROR, ("no dip for %s\n",
				    req.physpath))
				ret = ENODEV;
				break;
			}
			/*
			 * If we already had this one recorded
			 */
			if (pm_record_dep(req.physpath, (char *)dep, major)) {
				ddi_rele_driver(major);
				PMD(PMD_KEEPS,
				    ("PM_ADD_DEPENDENT released %s; "
				    "already recorded\n",
				    ddi_major_to_name(major)))
			}
			kmem_free(dep, deplen);
			rw_enter(&pm_dep_rwlock, RW_READER);
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				if (cur->ps_dip == dip)
					continue;
				(void) pm_dep_specd(cur->ps_dip, 0);
			}
			rw_exit(&pm_scan_list_rwlock);
			rw_exit(&pm_dep_rwlock);
			*rval_p = 0;
			return (0);
		}

		case PM_SET_DEVICE_THRESHOLD:
		{
			pm_thresh_rec_t *rp;
			pm_pte_t *ep;	/* threshold header storage */
			int *tp;	/* threshold storage */
			size_t size;
			extern int pm_thresh_specd(dev_info_t *, int);

			/*
			 * The header struct plus one entry struct plus one
			 * threshold plus the length of the string
			 */
			size = sizeof (pm_thresh_rec_t) +
			    (sizeof (pm_pte_t) * 1) +
			    (1 * sizeof (int)) +
			    strlen(req.physpath) + 1;

			rp = kmem_zalloc(size, KM_SLEEP);
			rp->ptr_size = size;
			rp->ptr_numcomps = 0;	/* means device threshold */
			ep = (pm_pte_t *)((intptr_t)rp + sizeof (*rp));
			rp->ptr_entries = ep;
			tp = (int *)((intptr_t)ep +
			    (1 * sizeof (pm_pte_t)));
			ep->pte_numthresh = 1;
			ep->pte_thresh = tp;
			*tp++ = req.value;
			(void) strcat((char *)tp, req.physpath);
			rp->ptr_physpath = (char *)tp;
			ASSERT((intptr_t)tp + strlen(req.physpath) + 1 ==
			    (intptr_t)rp + rp->ptr_size);
			pm_record_thresh(rp);
			/*
			 * Don't free rp, pm_record_thresh() keeps it.
			 * We don't try to apply it ourselves because we'd need
			 * to know too much about locking.  Since we don't
			 * hold a lock the entry could be removed before
			 * we get here
			 */
			ASSERT(dip == NULL);
			ret = 0;		/* can't fail now */
			if (!(dip = pm_name_to_dip(req.physpath, 1))) {
				break;
			}
			(void) pm_thresh_specd(dip, 0);
			break;
		}

		case PM_RESET_DEVICE_THRESHOLD:
		{
			/*
			 * This only applies to a currently attached and power
			 * managed node
			 */
			/*
			 * We don't do this to old-style drivers
			 */
			info = PM_GET_PM_INFO(dip);
			if (info == NULL) {
				PMD(PMD_ERROR, ("%s not power managed\n",
				    req.physpath))
				ret = EINVAL;
				break;
			}
			if (PM_ISBC(dip)) {
				PMD(PMD_ERROR, ("%s is BC\n", req.physpath))
				ret = EINVAL;
				break;
			}
			PM_LOCK_DIP(dip);
			pm_unrecord_threshold(req.physpath);
			pm_set_device_threshold(dip, PM_NUMCMPTS(dip),
			    pm_system_idle_threshold, PMC_DEF_THRESH);
			PM_UNLOCK_DIP(dip);
			return (0);
		}

		case PM_GET_NUM_COMPONENTS:
			*rval_p = PM_NUMCMPTS(dip);
			return (0);

		case PM_GET_DEVICE_TYPE:
			PM_LOCK_DIP(dip);
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PM_UNLOCK_DIP(dip);
				PMD(PMD_ERROR, ("%s PM_NO_PM_COMPONENTS\n",
				    pm_decode_cmd(cmd)))
				*rval_p = PM_NO_PM_COMPONENTS;
				return (0);
			}
			if (PM_ISBC(dip)) {
				*rval_p = PM_CREATE_COMPONENTS;
			} else {
				*rval_p = PM_AUTOPM;
			}
			PM_UNLOCK_DIP(dip);
			return (0);

		case PM_SET_COMPONENT_THRESHOLDS:
		{
			int comps = 0;
			int *end = (int *)req.data + icount;
			pm_thresh_rec_t *rp;
			pm_pte_t *ep;	/* threshold header storage */
			int *tp;	/* threshold storage */
			int *ip;
			int j;
			size_t size;
			extern int pm_thresh_specd(dev_info_t *, int);
			extern int pm_valid_thresh(dev_info_t *,
			    pm_thresh_rec_t *);

			for (ip = req.data; *ip; ip++) {
				if (ip >= end) {
					ret = EFAULT;
					break;
				}
				comps++;
				/* skip over indicated number of entries */
				for (j = *ip; j; j--) {
					if (++ip >= end) {
						ret = EFAULT;
						break;
					}
				}
				if (ret)
					break;
			}
			if (ret)
				break;
			if ((intptr_t)ip != (intptr_t)end - sizeof (int)) {
				/* did not exactly fill buffer */
				ret = EINVAL;
				break;
			}
			if (comps == 0) {
				PMD(PMD_ERROR, ("%s %s 0 components--EINVAL\n",
				    pm_decode_cmd(cmd), req.physpath))
				ret = EINVAL;
				break;
			}
			/*
			 * The header struct plus one entry struct per component
			 * plus the size of the lists minus the counts
			 * plus the length of the string
			 */
			size = sizeof (pm_thresh_rec_t) +
			    (sizeof (pm_pte_t) * comps) + req.datasize -
			    ((comps + 1) * sizeof (int)) +
			    strlen(req.physpath) + 1;

			rp = kmem_zalloc(size, KM_SLEEP);
			rp->ptr_size = size;
			rp->ptr_numcomps = comps;
			ep = (pm_pte_t *)((intptr_t)rp + sizeof (*rp));
			rp->ptr_entries = ep;
			tp = (int *)((intptr_t)ep +
			    (comps * sizeof (pm_pte_t)));
			for (ip = req.data; *ip; ep++) {
				ep->pte_numthresh = *ip;
				ep->pte_thresh = tp;
				for (j = *ip++; j; j--) {
					*tp++ = *ip++;
				}
			}
			(void) strcat((char *)tp, req.physpath);
			rp->ptr_physpath = (char *)tp;
			ASSERT((intptr_t)end == (intptr_t)ip + sizeof (int));
			ASSERT((intptr_t)tp + strlen(req.physpath) + 1 ==
			    (intptr_t)rp + rp->ptr_size);

			ASSERT(dip == NULL);
			/*
			 * If this is not a currently power managed node,
			 * then we can't check for validity of the thresholds
			 */
			if (!(dip = pm_name_to_dip(req.physpath, 1))) {
				/* don't free rp, pm_record_thresh uses it */
				pm_record_thresh(rp);
				PMD(PMD_ERROR, ("%s pm_name_to_dip %s failed\n",
				    pm_decode_cmd(cmd),  req.physpath))
				ret = 0;
				break;
			}

			if (!pm_valid_thresh(dip, rp)) {
				PMD(PMD_ERROR, ("invalid thresh for %s@%s\n",
				    ddi_binding_name(dip), PM_ADDR(dip)))
				kmem_free(rp, size);
				ret = EINVAL;
				break;
			}
			/*
			 * We don't just apply it ourselves because we'd need
			 * to know too much about locking.  Since we don't
			 * hold a lock the entry could be removed before
			 * we get here
			 */
			pm_record_thresh(rp);
			(void) pm_thresh_specd(dip, 0);
			ret = 0;
			break;
		}

		case PM_GET_COMPONENT_THRESHOLDS:
		{
			int musthave;
			int numthresholds = 0;
			int wordsize;
			int numcomps;
			caddr_t uaddr = req.data;	/* user address */
			int val;	/* int value to be copied out */
			int32_t val32;	/* int32 value to be copied out */
			caddr_t vaddr;	/* address to copyout from */
			int j;

#ifdef	_MULTI_DATAMODEL
			if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
				wordsize = sizeof (int32_t);
			} else
#endif /* _MULTI_DATAMODEL */
			{
				wordsize = sizeof (int);
			}

			ASSERT(dip);

			numcomps = PM_NUMCMPTS(dip);
			for (i = 0; i < numcomps; i++) {
				cp = &DEVI(dip)->devi_pm_components[i];
				numthresholds += cp->pmc_comp.pmc_numlevels - 1;
			}
			musthave = (numthresholds + numcomps + 1) *  wordsize;
			if (req.datasize < musthave) {
				PMD(PMD_ERROR, ("%s size %ld, need %d"
				    "--EINVAL\n", pm_decode_cmd(cmd),
				    req.datasize, musthave))
				ret = EINVAL;
				break;
			}
			PM_LOCK_DIP(dip);
			for (i = 0; i < numcomps; i++) {
				int *thp;
				cp = &DEVI(dip)->devi_pm_components[i];
				thp = cp->pmc_comp.pmc_thresh;
				/* first copyout the count */
				if (wordsize == sizeof (int32_t)) {
					val32 = cp->pmc_comp.pmc_numlevels - 1;
					vaddr = (caddr_t)&val32;
				} else {
					val = cp->pmc_comp.pmc_numlevels - 1;
					vaddr = (caddr_t)&val;
				}
				if (ddi_copyout(vaddr, (void *)uaddr,
				    wordsize, mode) != 0) {
					PM_UNLOCK_DIP(dip);
					PMD(PMD_ERROR, ("%s: %s@%s vaddr %p "
					    "EFAULT\n", pm_decode_cmd(cmd),
					    ddi_binding_name(dip),
					    PM_ADDR(dip), (void*)vaddr))
					ret = EFAULT;
					break;
				}
				vaddr = uaddr;
				vaddr += wordsize;
				uaddr = (caddr_t)vaddr;
				/* then copyout each threshold value */
				for (j = 0; j < cp->pmc_comp.pmc_numlevels - 1;
				    j++) {
					if (wordsize == sizeof (int32_t)) {
						val32 = thp[j + 1];
						vaddr = (caddr_t)&val32;
					} else {
						val = thp[i + 1];
						vaddr = (caddr_t)&val;
					}
					if (ddi_copyout(vaddr, (void *) uaddr,
					    wordsize, mode) != 0) {
						PM_UNLOCK_DIP(dip);
						PMD(PMD_ERROR, ("%s: %s@%s "
						    "uaddr %p EFAULT\n",
						    pm_decode_cmd(cmd),
						    ddi_binding_name(dip),
						    PM_ADDR(dip),
						    (void *)uaddr))
						ret = EFAULT;
						break;
					}
					vaddr = uaddr;
					vaddr += wordsize;
					uaddr = (caddr_t)vaddr;
				}
			}
			if (ret)
				break;
			/* last copyout a terminating 0 count */
			if (wordsize == sizeof (int32_t)) {
				val32 = 0;
				vaddr = (caddr_t)&val32;
			} else {
				ASSERT(wordsize == sizeof (int));
				val = 0;
				vaddr = (caddr_t)&val;
			}
			if (ddi_copyout(vaddr, uaddr, wordsize, mode) != 0) {
				PM_UNLOCK_DIP(dip);
				PMD(PMD_ERROR, ("%s: %s@%s vaddr %p (0 count) "
				    "EFAULT\n", pm_decode_cmd(cmd),
				    ddi_binding_name(dip),
				    PM_ADDR(dip), (void *)vaddr))
				ret = EFAULT;
				break;
			}
			/* finished, so don't need to increment addresses */
			PM_UNLOCK_DIP(dip);
			ret = 0;
			break;
		}

		case PM_GET_STATS:
		{
			time_t now;
			time_t *timestamp;
			extern int pm_cur_power(pm_component_t *cp);
			int musthave;
			int wordsize;

#ifdef	_MULTI_DATAMODEL
			if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
				wordsize = sizeof (int32_t);
			} else
#endif /* _MULTI_DATAMODEL */
			{
				wordsize = sizeof (int);
			}


			comps = PM_NUMCMPTS(dip);
			if (comps == 0 || PM_GET_PM_INFO(dip) == NULL) {
				PMD(PMD_ERROR, ("%s: %s no components or not "
				    "power managed--EINVAL\n",
				    pm_decode_cmd(cmd), req.physpath))
				ret = EINVAL;
				break;
			}
			musthave = comps * 2 * wordsize;
			if (req.datasize < musthave) {
				PMD(PMD_ERROR, ("%s size %lu, need %d"
				    "--EINVAL\n", pm_decode_cmd(cmd),
				    req.datasize, musthave))
				ret = EINVAL;
				break;
			}

			PM_LOCK_DIP(dip);
			(void) drv_getparm(TIME, &now);
			timestamp = kmem_zalloc(comps * sizeof (time_t),
			    KM_SLEEP);
			pm_get_timestamps(dip, timestamp);
			/*
			 * First the current power levels
			 */
			for (i = 0; i < comps; i++) {
				int curpwr;
				int32_t curpwr32;
				caddr_t cpaddr;

				cp = &DEVI(dip)->devi_pm_components[i];
				if (wordsize == sizeof (int)) {
					curpwr = pm_cur_power(cp);
					cpaddr = (caddr_t)&curpwr;

				} else {
					ASSERT(wordsize == sizeof (int32_t));
					curpwr32 = pm_cur_power(cp);
					cpaddr = (caddr_t)&curpwr32;
				}
				if (ddi_copyout(cpaddr, (void *) req.data,
				    wordsize, mode) != 0) {
					PM_UNLOCK_DIP(dip);
					PMD(PMD_ERROR, ("%s: %s@%s req.data %p "
					    "EFAULT", pm_decode_cmd(cmd),
					    ddi_binding_name(dip),
					    PM_ADDR(dip), (void *)req.data))
					return (EFAULT);
				}
				cpaddr = (caddr_t)req.data;
				cpaddr += wordsize;
				req.data = cpaddr;
			}
			/*
			 * Then the times remaining
			 */
			for (i = 0; i < comps; i++) {
				int retval;
				int32_t retval32;
				caddr_t rvaddr;

				cp = &DEVI(dip)->devi_pm_components[i];
				if (cp->pmc_cur_pwr == 0 || timestamp[i] == 0) {
					PMD(PMD_STATS, ("cur_pwer %x, "
					    "timestamp %lx\n",
					    cp->pmc_cur_pwr, timestamp[i]))
					retval = INT_MAX;
				} else {
					int thresh;
					(void) pm_current_threshold(dip, i,
					    &thresh);
					retval = thresh - (now - timestamp[i]);
					PMD(PMD_STATS, ("current thresh %x, "
					    "now %lx, timestamp %lx, retval "
					    "%x\n", thresh, now,
					    timestamp[i], retval))
				}
				if (wordsize == sizeof (int)) {
					rvaddr = (caddr_t)&retval;
				} else {
					ASSERT(wordsize == sizeof (int32_t));
					retval32 = retval;
					rvaddr = (caddr_t)&retval32;
				}
				if (ddi_copyout(rvaddr, (void *) req.data,
				    wordsize, mode) != 0) {
					PM_UNLOCK_DIP(dip);
					PMD(PMD_ERROR, ("%s: %s@%s req.data "
					    "%p EFAULT\n", pm_decode_cmd(cmd),
					    ddi_binding_name(dip),
					    PM_ADDR(dip),
					    (void *)req.data))
					return (EFAULT);
				}
				rvaddr = (caddr_t)req.data;
				rvaddr += wordsize;
				req.data = (int *)rvaddr;
			}
			PM_UNLOCK_DIP(dip);
			*rval_p = comps;
			kmem_free(timestamp, comps * sizeof (time_t));
			return (0);
		}

		case PM_GET_COMPONENT_NAME:
			ASSERT(dip);
			if (req.component < 0 ||
			    req.component > PM_NUMCMPTS(dip) - 1) {
				PMD(PMD_ERROR, ("%s %s@%s component %d > "
				    "numcmpts - 1 %d--EINVAL\n",
				    pm_decode_cmd(cmd), ddi_binding_name(dip),
				    PM_ADDR(dip), req.component,
				    PM_NUMCMPTS(dip) - 1))
				ret = EINVAL;
				break;
			}
			cp = &DEVI(dip)->devi_pm_components[req.component];
			if (ret = copyoutstr(cp->pmc_comp.pmc_name,
			    (char *)req.data, req.datasize, &lencopied)) {
				PMD(PMD_ERROR, ("%s %s@%s copyoutstr "
				    "%p failed--EFAULT\n", pm_decode_cmd(cmd),
				    ddi_binding_name(dip),
				    PM_ADDR(dip), (void *)req.data))
				break;
			}
			*rval_p = lencopied;
			ret = 0;
			break;

		case PM_GET_POWER_NAME:
		{
			int i;

			ASSERT(dip);
			if (req.component < 0 ||
			    req.component > PM_NUMCMPTS(dip) - 1) {
				PMD(PMD_ERROR, ("%s %s@%s component %d "
				    "> numcmpts - 1 %d--EINVAL\n",
				    pm_decode_cmd(cmd), ddi_binding_name(dip),
				    PM_ADDR(dip), req.component,
				    PM_NUMCMPTS(dip) - 1))
				ret = EINVAL;
				break;
			}
			cp = &DEVI(dip)->devi_pm_components[req.component];
			if ((i = req.value) < 0 ||
			    i > cp->pmc_comp.pmc_numlevels - 1) {
				PMD(PMD_ERROR, ("%s %s@%s value %d "
				    "> num_levels - 1 %d--EINVAL\n",
				    pm_decode_cmd(cmd), ddi_binding_name(dip),
				    PM_ADDR(dip), req.value,
				    cp->pmc_comp.pmc_numlevels - 1))
				ret = EINVAL;
				break;
			}
			dep = cp->pmc_comp.pmc_lnames[req.value];
			if (ret = copyoutstr(dep,
			    req.data, req.datasize, &lencopied)) {
				PMD(PMD_ERROR, ("%s %s@%s copyoutstr "
				    "%p failed--EFAULT\n",
				    pm_decode_cmd(cmd), ddi_binding_name(dip),
				    PM_ADDR(dip), (void *)req.data))
				break;
			}
			*rval_p = lencopied;
			ret = 0;
			break;
		}

		case PM_GET_POWER_LEVELS:
		{
			int musthave;
			int numlevels;
			int wordsize;

#ifdef	_MULTI_DATAMODEL
			if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
				wordsize = sizeof (int32_t);
			} else
#endif /* _MULTI_DATAMODEL */
			{
				wordsize = sizeof (int);
			}
			ASSERT(dip);

			if (PM_NUMCMPTS(dip) - 1 < req.component) {
				PMD(PMD_ERROR, ("%s: %s@%s has %d components, "
				    "component %d requested--EINVAL\n",
				    pm_decode_cmd(cmd), ddi_binding_name(dip),
				    PM_ADDR(dip), PM_NUMCMPTS(dip),
				    req.component))
				return (EINVAL);
			}
			cp = &DEVI(dip)->devi_pm_components[req.component];
			numlevels = cp->pmc_comp.pmc_numlevels;
			musthave = numlevels *  wordsize;
			if (req.datasize < musthave) {
				PMD(PMD_ERROR, ("%s size %lu, need %d"
				    "--EINVAL\n", pm_decode_cmd(cmd),
				    req.datasize, musthave))
				return (EINVAL);
			}
			PM_LOCK_DIP(dip);
			for (i = 0; i < numlevels; i++) {
				int level;
				int32_t level32;
				caddr_t laddr;

				if (wordsize == sizeof (int)) {
					level = cp->pmc_comp.pmc_lvals[i];
					laddr = (caddr_t)&level;
				} else {
					level32 = cp->pmc_comp.pmc_lvals[i];
					laddr = (caddr_t)&level32;
				}
				if (ddi_copyout(laddr, (void *) req.data,
				    wordsize, mode) != 0) {
					PM_UNLOCK_DIP(dip);
					PMD(PMD_ERROR, ("%s: %s@%s laddr %p "
					    "EFAULT\n", pm_decode_cmd(cmd),
					    ddi_binding_name(dip),
					    PM_ADDR(dip), (void *)laddr))
					return (EFAULT);
				}
				laddr = (caddr_t)req.data;
				laddr += wordsize;
				req.data = (int *)laddr;
			}
			PM_UNLOCK_DIP(dip);
			*rval_p = numlevels;
			ret = 0;
			break;
		}


		case PM_GET_NUM_POWER_LEVELS:
			if (req.component < 0 ||
			    req.component > PM_NUMCMPTS(dip) - 1) {
				PMD(PMD_ERROR, ("%s %s@%s component %d "
				    "> numcmpts - 1 %d--EINVAL\n",
				    pm_decode_cmd(cmd),
				    ddi_binding_name(dip),
				    PM_ADDR(dip),
				    req.component, PM_NUMCMPTS(dip) - 1))
				return (EINVAL);
			}
			cp = &DEVI(dip)->devi_pm_components[req.component];
			*rval_p = cp->pmc_comp.pmc_numlevels;
			ret = 0;
			break;

		case PM_GET_DEVICE_THRESHOLD_BASIS:
			PM_LOCK_DIP(dip);
			if ((info = PM_GET_PM_INFO(dip)) == NULL) {
				PM_UNLOCK_DIP(dip);
				PMD(PMD_ERROR, ("%s PM_NO_PM_COMPONENTS\n",
				    pm_decode_cmd(cmd)))
				*rval_p = PM_NO_PM_COMPONENTS;
				return (0);
			}
			if (PM_ISDIRECT(dip)) {
				PM_UNLOCK_DIP(dip);
				*rval_p = PM_DIRECTLY_MANAGED;
				return (0);
			}
			switch (DEVI(dip)->devi_pm_flags & PMC_THRESH_ALL) {
			case PMC_DEF_THRESH:
			case PMC_NEXDEF_THRESH:
				*rval_p = PM_DEFAULT_THRESHOLD;
				break;
			case PMC_DEV_THRESH:
				*rval_p = PM_DEVICE_THRESHOLD;
				break;
			case PMC_COMP_THRESH:
				*rval_p = PM_COMPONENT_THRESHOLD;
				break;
			default:
				if (PM_ISBC(dip)) {
					*rval_p = PM_OLD_THRESHOLD;
					break;
				}
				PM_UNLOCK_DIP(dip);
				PMD(PMD_ERROR, ("%s default, not BC--EINVAL",
				    pm_decode_cmd(cmd)))
				return (EINVAL);
			}
			PM_UNLOCK_DIP(dip);
			ret = 0;
			break;
		}
		break;

	case PM_PSC:
		PMD(PMD_IOCTL, ("\n"))
		/*
		 * Commands that require pm_state_change_t as arg
		 */
#ifdef	_MULTI_DATAMODEL
		if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
			pscp32 = (pm_state_change32_t *)arg;
			if (ddi_copyin((caddr_t)arg, &psc32,
			    sizeof (psc32), mode) != 0) {
				PMD(PMD_ERROR, ("%s ddi_copyin EFAULT\n\n",
				    pm_decode_cmd(cmd)))
				return (EFAULT);
			}
			psc.physpath = (caddr_t)psc32.physpath;
			psc.size = psc32.size;
		} else
#endif /* _MULTI_DATAMODEL */
		{
			pscp = (pm_state_change_t *)arg;
			if (ddi_copyin((caddr_t)arg, &psc,
			    sizeof (psc), mode) != 0) {
				PMD(PMD_ERROR, ("%s ddi_copyin EFAULT\n\n",
				    pm_decode_cmd(cmd)))
				return (EFAULT);
			}
		}
		switch (cmd) {

		case PM_GET_STATE_CHANGE:
		case PM_GET_STATE_CHANGE_WAIT:
		{
			psce_t	*pscep;
			pm_state_change_t	*p;

			/*
			 * We want to know if any device has changed state.
			 * We look up by clone.  In case we have another thread
			 * from the same process, we loop.
			 * pm_psc_clone_to_interest() returns a locked entry.
			 */
			clone = PM_MINOR_TO_CLONE(getminor(dev));
			mutex_enter(&pm_clone_lock);
			if (!pm_interest_registered(clone))
				pm_register_watcher(clone, NULL);
			while ((pscep =
			    pm_psc_clone_to_interest(clone)) == NULL) {
				if (cmd == PM_GET_STATE_CHANGE) {
					PMD(PMD_ERROR, ("%s EWOULDBLOCK\n",
					    pm_decode_cmd(cmd)))
					mutex_exit(&pm_clone_lock);
					return (EWOULDBLOCK);
				} else {
					if (cv_wait_sig(&pm_clones_cv[clone],
					    &pm_clone_lock) == 0) {
						mutex_exit(&pm_clone_lock);
						PMD(PMD_ERROR, ("%s EINTR\n",
						    pm_decode_cmd(cmd)))
						return (EINTR);
					}
				}
			}
			mutex_exit(&pm_clone_lock);
			if (psc.size < strlen(pscep->psce_out->physpath) + 1) {
				PMD(PMD_ERROR, ("%s EFAULT\n",
				    pm_decode_cmd(cmd)))
				mutex_exit(&pscep->psce_lock);
				return (EFAULT);
			}
			if (ret = copyoutstr(pscep->psce_out->physpath,
			    psc.physpath, psc.size, &lencopied)) {
				PMD(PMD_ERROR, ("%s copyoutstr %p failed--"
				    "EFAULT\n", pm_decode_cmd(cmd),
				    (void *)psc.physpath))
				mutex_exit(&pscep->psce_lock);
				return (ret);
			}
			p = pscep->psce_out;
#ifdef	_MULTI_DATAMODEL
			if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
				pm_state_change32_t	 psc32;
				size_t copysize;
#ifdef DEBUG
				size_t usrcopysize;
#endif
				psc32.event = (int32_t)p->event;
				psc32.timestamp = (int32_t)p->timestamp;
				psc32.component = (int32_t)p->component;
				psc32.old_level = (int32_t)p->old_level;
				psc32.new_level = (int32_t)p->new_level;
				copysize = ((long)&psc32.size -
				    (long)&psc32.component);
#ifdef DEBUG
				usrcopysize = ((long)&pscp32->size -
				    (long)&pscp32->component);
				ASSERT(usrcopysize == copysize);
#endif
				if (ddi_copyout(&psc32.component,
				    &pscp32->component, copysize, mode) != 0) {
					PMD(PMD_ERROR, ("%s copyout failed--"
					    "EFAULT\n", pm_decode_cmd(cmd)))
					mutex_exit(&pscep->psce_lock);
					return (EFAULT);
				}
			} else
#endif /* _MULTI_DATAMODEL */
			{
				size_t copysize;
				copysize = ((long)&p->size -
				    (long)&p->component);
				if (ddi_copyout(&p->component, &pscp->component,
				    copysize, mode) != 0) {
					PMD(PMD_ERROR, ("%s copyout failed--"
					    "EFAULT\n", pm_decode_cmd(cmd)))
					mutex_exit(&pscep->psce_lock);
					return (EFAULT);
				}
			}
			kmem_free(p->physpath, p->size);
			p->size = 0;
			if (pscep->psce_out == pscep->psce_last)
				p = pscep->psce_first;
			else
				p++;
			pscep->psce_out = p;
			mutex_exit(&pscep->psce_lock);
			ret = 0;
			break;
		}

		case PM_DIRECT_NOTIFY:
		case PM_DIRECT_NOTIFY_WAIT:
		{
			psce_t	*pscep;
			pm_state_change_t	*p;
			/*
			 * We want to know if any direct device of ours has
			 * something we should know about.  We look up by clone.
			 * In case we have another thread from the same process,
			 * we loop.
			 * pm_psc_clone_to_direct() returns a locked entry.
			 */
			clone = PM_MINOR_TO_CLONE(getminor(dev));
			mutex_enter(&pm_clone_lock);
			while ((pscep =
			    pm_psc_clone_to_direct(clone)) == NULL) {
				if (cmd == PM_DIRECT_NOTIFY) {
					PMD(PMD_ERROR, ("%s EWOULDBLOCK\n",
					    pm_decode_cmd(cmd)))
					mutex_exit(&pm_clone_lock);
					return (EWOULDBLOCK);
				} else {
					if (cv_wait_sig(&pm_clones_cv[clone],
					    &pm_clone_lock) == 0) {
						mutex_exit(&pm_clone_lock);
						PMD(PMD_ERROR, ("%s EINTR\n",
						    pm_decode_cmd(cmd)))
						return (EINTR);
					}
				}
			}
			mutex_exit(&pm_clone_lock);
			if (psc.size < strlen(pscep->psce_out->physpath) + 1) {
				mutex_exit(&pscep->psce_lock);
				PMD(PMD_ERROR, ("%s EFAULT\n",
				    pm_decode_cmd(cmd)))
				return (EFAULT);
			}
			if (ret = copyoutstr(pscep->psce_out->physpath,
			    psc.physpath, psc.size, &lencopied)) {
				PMD(PMD_ERROR, ("%s copyoutstr %p failed"
				    "--EFAULT\n", pm_decode_cmd(cmd),
				    (void *)psc.physpath))
				mutex_exit(&pscep->psce_lock);
				return (ret);
			}
			p = pscep->psce_out;
#ifdef	_MULTI_DATAMODEL
			if ((mode & DATAMODEL_MASK) == DATAMODEL_ILP32) {
				pm_state_change32_t	 psc32;
				size_t copysize;
#ifdef DEBUG
				size_t usrcopysize;
#endif
				psc32.component = (int32_t)p->component;
				psc32.event = (int32_t)p->event;
				psc32.timestamp = (int32_t)p->timestamp;
				psc32.old_level = (int32_t)p->old_level;
				psc32.new_level = (int32_t)p->new_level;
				copysize = ((int)&psc32.size -
				    (int)&psc32.component);
#ifdef DEBUG
				usrcopysize = ((int)&pscp32->size -
				    (int)&pscp32->component);
				ASSERT(usrcopysize == copysize);
#endif
				if (ddi_copyout(&psc32.component,
				    &pscp32->component, copysize, mode) != 0) {
					PMD(PMD_ERROR, ("%s copyout failed"
					    "--EFAULT\n", pm_decode_cmd(cmd)))
					mutex_exit(&pscep->psce_lock);
					return (EFAULT);
				}
			} else
#endif
			{
				size_t copysize;
				copysize = ((int)&p->size - (int)&p->component);
				if (ddi_copyout(&p->component, &pscp->component,
				    copysize, mode) != 0) {
					PMD(PMD_ERROR, ("%s copyout failed"
					    "--EFAULT\n", pm_decode_cmd(cmd)))
					mutex_exit(&pscep->psce_lock);
					return (EFAULT);
				}
			}
			mutex_enter(&pm_clone_lock);
			PMD(PMD_IOCTL, ("NW: pm_poll_cnt[%d] is %d before "
			    "ASSERT\n", clone, pm_poll_cnt[clone]))
			ASSERT(pm_poll_cnt[clone]);
			pm_poll_cnt[clone]--;
			mutex_exit(&pm_clone_lock);
			kmem_free(p->physpath, p->size);
			p->size = 0;
			if (pscep->psce_out == pscep->psce_last)
				p = pscep->psce_first;
			else
				p++;
			pscep->psce_out = p;
			mutex_exit(&pscep->psce_lock);
			ret = 0;
			break;
		}
		default:
			ASSERT(0);
		}
		break;

	case NOSTRUCT:
		PMD(PMD_IOCTL, ("\n"))
		switch (cmd) {
		case PM_REM_DEVICES:
		{
			pm_scan_t *curnext;	/* because *cur goes away */
			/*
			 * This is the old obsolete command, so it only affects
			 * backwards compatible devices
			 * pm_unmanage() upgrades pm_scan_list_rwlock and
			 * deletes the current entry, so we can't depend on
			 * it after the call to pm_unmanaged
			 */
			rw_enter(&pm_scan_list_rwlock, RW_WRITER);
			for (cur = pm_scan_list; cur != NULL; cur = curnext) {
				dev_info_t *dip = cur->ps_dip;
				curnext = cur->ps_next;
				PMD(PMD_REMDEV, ("PM_REM_DEVICES: %s@%s %p\n",
				    ddi_binding_name(dip), PM_ADDR(dip),
				    (void *)dip))
				PMD(PMD_REMDEV, ("PM_REM_DEVICES: info %p\n",
				    (void *)PM_GET_PM_INFO(dip)))
				if (!(PM_ISBC(dip))) {
					PMD(PMD_REMDEV, ("REM %s@%s not BC\n",
					    ddi_binding_name(dip),
					    PM_ADDR(dip)))
					continue;
				}
				/*
				 * Don't mess with it is somebody has control
				 * of it
				 */
				if (PM_ISDIRECT(dip)) {
					PMD(PMD_REMDEV, ("REM %s@%s direct\n",
					    ddi_binding_name(dip),
					    PM_ADDR(dip)))
					continue;
				}
				/*
				 * Forget we ever knew anything about the
				 * components of this  device
				 */
				DEVI(dip)->devi_pm_flags &=
				    ~(PMC_BC | PMC_COMPONENTS_DONE |
				    PMC_COMPONENTS_FAILED);
				/*
				 * This will block scan while we're raising
				 * power of this device, but since we're
				 * removing devices, we don't mind
				 */
				PM_LOCK_POWER(dip, PMC_ALLCOMPS);
				if (pm_all_to_normal(dip, 0, 0) !=
				    DDI_SUCCESS) {
					PMD(PMD_REMDEV | PMD_ERROR, ("%s could "
					    "not bring %s@%s to normal\n",
					    pm_decode_cmd(cmd),
					    ddi_binding_name(dip),
					    PM_ADDR(dip)));
				}
				/* PM_LOCK_POWER() dropped */
				PM_LOCK_DIP(dip);
				(void) pm_unmanage(dip, 1);
				PM_UNLOCK_DIP(dip);
			}
			rw_exit(&pm_scan_list_rwlock);
			ret = 0;
			break;
		}

		case PM_START_PM:
			mutex_enter(&pm_scan_lock);
			if (autopm_enabled) {
				mutex_exit(&pm_scan_lock);
				PMD(PMD_ERROR, ("EBUSY\n"))
				return (EBUSY);
			}
			autopm_enabled = 1;
			mutex_exit(&pm_scan_lock);
			PMD(PMD_RESCAN, ("PM_START_PM rescan\n"))
			pm_rescan(1);
			ret = 0;
			break;

		case PM_RESET_PM:
		{
			extern void pm_discard_thresholds(void);
			extern void pm_discard_dependencies(void);
			pm_system_idle_threshold = pm_default_idle_threshold;
			pm_discard_thresholds();
			pm_all_to_default_thresholds();
			pm_discard_dependencies();
			/* FALLTHROUGH */
		}

		case PM_STOP_PM:
			mutex_enter(&pm_scan_lock);
			if (!autopm_enabled && cmd != PM_RESET_PM) {
				mutex_exit(&pm_scan_lock);
				PMD(PMD_ERROR, ("EINVAL\n"))
				return (EINVAL);
			}
			autopm_enabled = 0;
			mutex_exit(&pm_scan_lock);
			rw_enter(&pm_scan_list_rwlock, RW_READER);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				dip = cur->ps_dip;
				if (!PM_ISBC(dip) && !PM_ISDIRECT(dip)) {
					PM_LOCK_POWER(dip, PMC_ALLCOMPS);
					if (pm_all_to_normal(dip, 0, 1) !=
					    DDI_SUCCESS) {
						PMD(PMD_ERROR, ("%s could not "
						    "bring %s@%s to normal\n",
						    pm_decode_cmd(cmd),
						    ddi_binding_name(dip),
						    PM_ADDR(dip)))
					}
					/* drops PM_LOCK_POWER */
				}
			}
			rw_exit(&pm_scan_list_rwlock);
			return (0);

		case PM_GET_SYSTEM_THRESHOLD:
			*rval_p = pm_system_idle_threshold;
			ret = 0;
			break;

		case PM_GET_DEFAULT_SYSTEM_THRESHOLD:
			*rval_p = pm_default_idle_threshold;
			ret = 0;
			break;

		case PM_SET_SYSTEM_THRESHOLD:
			if ((int)arg < 0) {
				PMD(PMD_ERROR, (" arg 0x%x < 0--EINVAL\n",
				    (int)arg))
				ret = EINVAL;
				return (ret);
			}
			PMD(PMD_IOCTL, ("0x%x 0t%d\n", (int)arg, (int)arg))
			pm_system_idle_threshold = (int)arg;
			rw_enter(&pm_scan_list_rwlock, RW_READER);
			for (cur = pm_scan_list;
			    cur != NULL; cur = cur->ps_next) {
				dip = cur->ps_dip;
				PM_LOCK_DIP(dip);
				if (!PM_ISBC(dip) && !PM_ISDIRECT(dip)) {
					switch (DEVI(dip)->devi_pm_flags &
					    PMC_THRESH_ALL) {
					case PMC_DEF_THRESH:
						PMD(PMD_IOCTL, ("set %s@%s "
						    "default thresh to 0t%d\n",
						    ddi_get_name(dip),
						    PM_ADDR(dip),
						    pm_system_idle_threshold))
						pm_set_device_threshold(dip,
						    PM_NUMCMPTS(dip),
						    pm_system_idle_threshold,
						    PMC_DEF_THRESH);
						break;
					default:
						break;
					}
				}
				PM_UNLOCK_DIP(dip);
			}
			rw_exit(&pm_scan_list_rwlock);
			PMD(PMD_RESCAN, ("PM_SET_SYSTEM_THRESHOLD rescan\n"))
			ret = 0;
			break;

		case PM_IDLE_DOWN:
			mutex_enter(&pm_scan_lock);
			if (pm_idledown_id) {
				to_id = pm_idledown_id;
				pm_idledown_id = 0;
				mutex_exit(&pm_scan_lock);
				(void) untimeout(to_id);
				mutex_enter(&pm_scan_lock);
				if (pm_idledown_id) {
					mutex_exit(&pm_scan_lock);
					PMD(PMD_IDLEDOWN,
					    ("Another caller got it!\n"))
					ret = EBUSY;
					break;
				}
			}
			pm_idle_down = 1;
			save_min_scan = pm_min_scan;
			pm_min_scan = 1;
			mutex_exit(&pm_scan_lock);
			pm_rescan(1);

			/*
			 * Keep idle-down in effect for a length of time.
			 */
			mutex_enter(&pm_scan_lock);
			if (!pm_idledown_id) {
				pm_idledown_id = timeout(pm_end_idledown, NULL,
				    (int)arg * hz);
			}
			mutex_exit(&pm_scan_lock);
			ret = 0;
			break;

		case PM_GET_PM_STATE:
			if (autopm_enabled) {
				*rval_p = PM_SYSTEM_PM_ENABLED;
			} else {
				*rval_p = PM_SYSTEM_PM_DISABLED;
			}
			ret = 0;
			break;
		}
		break;


	default:
		PMD(PMD_IOCTL, ("\n"))
		/*
		 * Internal error, invalid ioctl description
		 */
		cmn_err(CE_PANIC, "pm: invalid str_type %d for cmd %d (%s)\n",
		    pcip->str_type, cmd, pcip->name);
		/* NOTREACHED */
	}
	ASSERT(ret != 0x0badcafe);	/* some cmd in wrong case! */
	return (ret);
}

#ifdef DEBUG
static void
print_info(dev_info_t *dip)
{
	pm_info_t	*info;
	int		i, j;
	struct pm_component *cp;
	extern int pm_cur_power(pm_component_t *cp);

	info = PM_GET_PM_INFO(dip);
	cmn_err(CE_CONT, "pm_info for %s\n", ddi_node_name(dip));
	for (i = 0; i < DEVI(dip)->devi_pm_num_components; i++) {
		cp = &DEVI(dip)->devi_pm_components[i];
		cmn_err(CE_CONT, "\tThresholds[%d] =",  i);
		for (j = 0; j < cp->pmc_comp.pmc_numlevels; j++)
			cmn_err(CE_CONT, " %d", cp->pmc_comp.pmc_thresh[i]);
		cmn_err(CE_CONT, "\n");
		cmn_err(CE_CONT, "\tCurrent power[%d] = %d\n", i,
		    pm_cur_power(cp));
	}
	for (i = 0; i < info->pmi_nkeptupby; i++) {
		cmn_err(CE_CONT, "\tKeptUpBy[%d] = %s\n", i,
		    ddi_node_name(info->pmi_kupbydips[i]));
	}
	for (i = 0; i < info->pmi_nwekeepup; i++) {
		cmn_err(CE_CONT, "\tWeKeepUp[%d] = %s\n", i,
		    ddi_node_name(info->pmi_wekeepdips[i]));
	}
	if (info->pmi_dev_pm_state & PM_DIRECT_NEW)
		cmn_err(CE_CONT, "\tDirect power management\n");
	if (info->pmi_dev_pm_state & PM_DIRECT_OLD)
		cmn_err(CE_CONT, "\tOld Direct power management\n");
	if (info->pmi_dev_pm_state & PM_SCAN)
		cmn_err(CE_CONT, "\tWill be scanned\n");
}
#endif

static int
pm_check_permission(char *dev_name, cred_t *cr)
{
	vnode_t		*vp;
	register int	error = 0;
	struct vattr	vattr;

	/* 4135731 */
	if (suser(cr))
		return (0);

	if (!dev_name)
		return (EINVAL);

	if (error = lookupname(dev_name, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp)) {
		cmn_err(CE_WARN, "pm: Can't find device file %s ", dev_name);
		return (error);
	}

	vattr.va_mask = AT_UID;
	if (error = VOP_GETATTR(vp, &vattr, 0, CRED())) {
		cmn_err(CE_WARN, "pm: Can't get owner uid of device file %s",
			dev_name);
		VN_RELE(vp);
		return (error);
	}

	if (vattr.va_uid != cr->cr_ruid)
		error = EPERM;

	VN_RELE(vp);
	return (error);
}

/*ARGSUSED*/
static void
pm_end_idledown(void *ignore)
{
	extern	clock_t pm_min_scan;

	mutex_enter(&pm_scan_lock);
	pm_idle_down = 0;
	if (pm_min_scan == 1)
		pm_min_scan = save_min_scan;
	pm_idledown_id = 0;
	mutex_exit(&pm_scan_lock);
	PMD(PMD_IDLEDOWN, ("idle-down period is over.\n"))
}

static struct pm_cmd_info pmci[] = {
	{PM_SCHEDULE, "PM_SCHEDULE", 0},
	{PM_GET_IDLE_TIME, "PM_GET_IDLE_TIME", 0},
	{PM_GET_NUM_CMPTS, "PM_GET_NUM_CMPTS", 0},
	{PM_GET_THRESHOLD, "PM_GET_THRESHOLD", 0},
	{PM_SET_THRESHOLD, "PM_SET_THRESHOLD", 1, PM_REQUEST, INWHO, OLDDIP,
	    NODEP, SU},
	{PM_GET_NORM_PWR, "PM_GET_NORM_PWR", 1, PM_REQUEST, INWHO, OLDDIP,
	    NODEP},
	{PM_SET_CUR_PWR, "PM_SET_CUR_PWR", 1, PM_REQUEST, INWHO, OLDDIP,
	    NODEP, SU},
	{PM_GET_CUR_PWR, "PM_GET_CUR_PWR", 1, PM_REQUEST, INWHO, OLDDIP,
	    NODEP},
	{PM_GET_NUM_DEPS, "PM_GET_NUM_DEPS", 0},
	{PM_GET_DEP, "PM_GET_DEP", 0},
	{PM_ADD_DEP, "PM_ADD_DEP", 1, PM_REQUEST, INWHO | INDATASTRING, OLDDIP,
	    OLDDEP, SU},
	{PM_REM_DEP, "PM_REM_DEP", 0},
	{PM_REM_DEVICE, "PM_REM_DEVICE", 0},
	{PM_REM_DEVICES, "PM_REM_DEVICES", 1, NOSTRUCT, 0, 0, 0, SU},
	{PM_REPARSE_PM_PROPS, "PM_REPARSE_PM_PROPS", 1, PM_REQ, INWHO, NEWDIP,
	    NODEP},
	{PM_DISABLE_AUTOPM, "PM_DISABLE_AUTOPM", 1, PM_REQUEST, INWHO, OLDDIP,
	    NODEP, CHECKPERMS},
	{PM_REENABLE_AUTOPM, "PM_REENABLE_AUTOPM", 1, PM_REQUEST, INWHO, OLDDIP,
	    NODEP, CHECKPERMS},
	{PM_SET_NORM_PWR, "PM_SET_NORM_PWR", 0 },
	{PM_SET_DEVICE_THRESHOLD, "PM_SET_DEVICE_THRESHOLD", 1, PM_REQ,
	    INWHO, NODIP, NODEP, SU},
	{PM_GET_SYSTEM_THRESHOLD, "PM_GET_SYSTEM_THRESHOLD", 1, NOSTRUCT},
	{PM_GET_DEFAULT_SYSTEM_THRESHOLD, "PM_GET_DEFAULT_SYSTEM_THRESHOLD",
	    1, NOSTRUCT},
	{PM_SET_SYSTEM_THRESHOLD, "PM_SET_SYSTEM_THRESHOLD", 1, NOSTRUCT,
	    0, 0, 0, SU},
	{PM_START_PM, "PM_START_PM", 1, NOSTRUCT, 0, 0, 0, SU},
	{PM_STOP_PM, "PM_STOP_PM", 1, NOSTRUCT, 0, 0, 0, SU},
	{PM_RESET_PM, "PM_RESET_PM", 1, NOSTRUCT, 0, 0, 0, SU},
	{PM_GET_STATS, "PM_GET_STATS", 1, PM_REQ, INWHO | INDATAOUT,
	    NEWDIP, NODEP},
	{PM_GET_DEVICE_THRESHOLD, "PM_GET_DEVICE_THRESHOLD", 1, PM_REQ, INWHO,
	    NEWDIP, NODEP},
	{PM_GET_POWER_NAME, "PM_GET_POWER_NAME", 1, PM_REQ, INWHO | INDATAOUT,
	    NEWDIP, NODEP},
	{PM_GET_POWER_LEVELS, "PM_GET_POWER_LEVELS", 1, PM_REQ,
	    INWHO | INDATAOUT, NEWDIP, NODEP},
	{PM_GET_NUM_COMPONENTS, "PM_GET_NUM_COMPONENTS", 1, PM_REQ, INWHO,
	    NEWDIP, NODEP},
	{PM_GET_COMPONENT_NAME, "PM_GET_COMPONENT_NAME", 1, PM_REQ,
	    INWHO | INDATAOUT, NEWDIP, NODEP},
	{PM_GET_NUM_POWER_LEVELS, "PM_GET_NUM_POWER_LEVELS", 1, PM_REQ, INWHO,
	    NEWDIP, NODEP},
	{PM_GET_STATE_CHANGE, "PM_GET_STATE_CHANGE", 1, PM_PSC},
	{PM_GET_STATE_CHANGE_WAIT, "PM_GET_STATE_CHANGE_WAIT", 1, PM_PSC},
	{PM_DIRECT_PM, "PM_DIRECT_PM", 1, PM_REQ, INWHO, NEWDIP, NODEP,
	    (SU | SG)},
	{PM_RELEASE_DIRECT_PM, "PM_RELEASE_DIRECT_PM", 1, PM_REQ, INWHO,
	    NEWDIP, NODEP},
	{PM_DIRECT_NOTIFY, "PM_DIRECT_NOTIFY", 1, PM_PSC},
	{PM_DIRECT_NOTIFY_WAIT, "PM_DIRECT_NOTIFY_WAIT", 1, PM_PSC},
	{PM_RESET_DEVICE_THRESHOLD, "PM_RESET_DEVICE_THRESHOLD", 1, PM_REQ,
	    INWHO, NEWDIP, NODEP, SU},
	{PM_GET_PM_STATE, "PM_GET_PM_STATE", 1, NOSTRUCT},
	{PM_GET_DEVICE_TYPE, "PM_GET_DEVICE_TYPE", 1, PM_REQ, INWHO,
	    NEWDIP, NODEP},
	{PM_SET_COMPONENT_THRESHOLDS, "PM_SET_COMPONENT_THRESHOLDS", 1, PM_REQ,
	    INWHO | INDATAINT, NODIP, NODEP, SU},
	{PM_GET_COMPONENT_THRESHOLDS, "PM_GET_COMPONENT_THRESHOLDS", 1, PM_REQ,
	    INWHO | INDATAOUT, NEWDIP, NODEP},
	{PM_IDLE_DOWN, "PM_IDLE_DOWN", 1, NOSTRUCT, 0, 0, 0, SU},
	{PM_GET_DEVICE_THRESHOLD_BASIS, "PM_GET_DEVICE_THRESHOLD_BASIS", 1,
	    PM_REQ, INWHO, NEWDIP, NODEP},
	{PM_SET_CURRENT_POWER, "PM_SET_CURRENT_POWER", 1, PM_REQ, INWHO, NEWDIP,
	    NODEP, CHECKPERMS},
	{PM_GET_CURRENT_POWER, "PM_GET_CURRENT_POWER", 1, PM_REQ, INWHO, NEWDIP,
	    NODEP},
	{PM_GET_FULL_POWER, "PM_GET_FULL_POWER", 1, PM_REQ, INWHO, NEWDIP,
	    NODEP},
	{PM_ADD_DEPENDENT, "PM_ADD_DEPENDENT", 1, PM_REQ, INWHO | INDATASTRING,
	    NODIP, NEWDEP, SU},
	{PM_GET_TIME_IDLE, "PM_GET_TIME_IDLE", 1, PM_REQ, INWHO, NEWDIP, NODEP},
	{0, NULL}
};

struct pm_cmd_info *
pc_info(int cmd)
{
	struct pm_cmd_info *pcip;

	for (pcip = pmci; pcip->name; pcip++) {
		if (cmd == pcip->cmd)
			return (pcip);
	}
	return (NULL);
}

static char *
pm_decode_cmd(int cmd)
{
	static char invbuf[64];
	struct pm_cmd_info *pcip = pc_info(cmd);
	if (pcip != NULL)
		return (pcip->name);
	(void) sprintf(invbuf, "pm_ioctl: invalid command %d\n", cmd);
	return (invbuf);
}

static int
pm_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
	struct pollhead **phpp)
{
	extern struct pollhead pm_pollhead;	/* common/os/sunpm.c */
	int	clone;

	clone = PM_MINOR_TO_CLONE(getminor(dev));
	PMD(PMD_IOCTL, ("pm_chpoll clone %d ", clone))
	if ((events & (POLLIN | POLLRDNORM)) && pm_poll_cnt[clone]) {
		*reventsp |= (POLLIN | POLLRDNORM);
		PMD(PMD_IOCTL, ("reventsp set\n"))
	} else {
		*reventsp = 0;
		if (!anyyet) {
			PMD(PMD_IOCTL, ("not anyyet\n"))
			*phpp = &pm_pollhead;
		}
#ifdef DEBUG
		else {
			PMD(PMD_IOCTL, ("anyyet\n"))
		}
#endif
	}
	return (0);
}

/*
 * Discard entries for this clone, decrementing pm_poll_cnt in the process
 */
static void
pm_discard_entries(int clone)
{
	psce_t	*pscep;
	pm_state_change_t	*p;
	psce_t			*pm_psc_clone_to_direct(int);

	if ((pscep = pm_psc_clone_to_direct(clone)) != NULL) {
		p = pscep->psce_out;
		mutex_enter(&pm_clone_lock);
		while (p->size) {
			PMD(PMD_IOCTL, ("discard: pm_poll_cnt[%d] is %d before"
			    " ASSERT\n", clone, pm_poll_cnt[clone]))
			ASSERT(pm_poll_cnt[clone]);
			pm_poll_cnt[clone]--;
			kmem_free(p->physpath, p->size);
			p->size = 0;
			if (pscep->psce_out == pscep->psce_last)
				p = pscep->psce_first;
			else
				p++;
		}
		mutex_exit(&pm_clone_lock);
		pscep->psce_out = pscep->psce_first;
		pscep->psce_in = pscep->psce_first;
		mutex_exit(&pscep->psce_lock);
	}
}
