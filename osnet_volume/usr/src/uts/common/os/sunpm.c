/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sunpm.c	1.8	99/11/15 SMI"

/*
 * sunpm.c builds sunpm.o	"power management framework"
 *	kernel-resident power management code.  Implements power management
 *	policy
 *	Assumes: all backwards compat. device components wake up on &
 *		 the pm_info pointer in dev_info is initially NULL
 *
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/callb.h>		/* callback registration during CPR */
#include <sys/conf.h>		/* driver flags and functions */
#include <sys/open.h>		/* OTYP_CHR definition */
#include <sys/stat.h>		/* S_IFCHR definition */
#include <sys/pathname.h>	/* name -> dev_info xlation */
#include <sys/ddi_impldefs.h>	/* dev_info node fields */
#include <sys/kmem.h>		/* memory alloc stuff */
#include <sys/debug.h>
#include <sys/pm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunpm.h>
#include <sys/epm.h>
#include <sys/vfs.h>
#include <sys/mode.h>
#include <sys/mkdev.h>
#include <sys/promif.h>
#include <sys/consdev.h>
#include <sys/esunddi.h>
#include <sys/modctl.h>
#include <sys/fs/ufs_fs.h>
#include <sys/note.h>
#include <sys/taskq.h>

#define	PM_MIN_SCAN	((clock_t)15)	/* Minimum scan interval in seconds */
#define	PM_RESTART_SCAN	((clock_t)-1)	/* restart scan immediately */
#define	bTOBRU(bits)	((bits + 7) >> 3)	/* bits to Bytes, log2(NBBY) */
#define	PM_ADDR(dip)	(ddi_get_name_addr(dip) ? ddi_get_name_addr(dip) : "")
#define	PM_NAME(dip)	(ddi_binding_name(dip))
#define	PM_CURPOWER(dip, comp) cur_power(&DEVI(dip)->devi_pm_components[comp])

clock_t pm_min_scan = PM_MIN_SCAN;

/*
 * PM LOCKING  XXX this text is still under construction
 *	The list of locks:
 * Global pm mutex locks.
 *
 * e_pm_power_lock:
 *		This is a helper to protect the reference counts used in the
 *		re-entrant per-dip lock implemented by pm_lock_power().
 * pm_scan_lock:
 *		It protects the timeout id of the scan thread, and the values
 *		of autopm_enabled and bcpm_enabled.
 *
 * pm_clone_lock:	Protects the clone list for the pm driver.
 *
 * pm_rsvp_lock:
 *		Used to synchronize the data structures used for processes
 *		to rendevous with state change information when doing direct PM.
 *
 * pmlloglock:	DEBUG kernels only,  Protects the trace log used by
 *		pm_lock_power() and pm_lock_dip().
 *
 * ppm_lock:	protects the list of ppm drivers while adding a ppm driver to it
 *
 * Global PM rwlocks
 *
 * pm_scan_list_rwlock:
 *		Protect the list of devices to be scanned.  Scan hold this
 *		as a reader.  Any thread modifying pmi_dev_pm_state must hold
 *		this as a writer.
 * pm_thresh_rwlock:
 *		Protects the list of thresholds recorded for future use (when
 *		devices attach).
 * pm_pscc_direct_rwlock:
 *		Protects the list that maps devices being directly power
 *		managed to the processes that manage them.
 * pm_pscc_interest_rwlock;
 *		Protects the list that maps state change events to processes
 *		that want to know about them.
 *
 * per-dip locks:
 *
 * Each node has these per-dip locks, which are only used if the device is
 * a candidate for power management (e.g. has pm components)
 *
 * devi_pm_lock:
 *		Protects all power management state of the node except for
 *		power level, which is protected by devi_pm_power_lock.
 *
 * devi_pm_power_lock:
 *		Since changing power level is possibly a slow operation (30
 *		seconds to spin up a disk drive), this is locked separately.
 *		Since a call into the driver to change the power level of one
 *		component may result in a call back into the framework to change
 *		the power level of another, this lock allows re-entrancy by
 *		the same thread (see pm_lock_power()).
 *
 * devi_pm_busy_lock:
 *		This lock protects the integrety of the busy count.  It is
 *		only taken by pm_busy_component() and pm_idle_component.  It
 *		is per-dip to keep from single-threading all the disk drivers
 *		on a system.  It could be per component instead, but most
 *		devices have only one.
 *
 */

static kmutex_t	e_pm_power_lock;
kmutex_t	pm_scan_lock;
timeout_id_t	pm_scan_id;
taskq_t		*pm_tq;		/* rescan task queue */
callb_id_t	pm_cpr_cb_id;
callb_id_t	pm_panic_cb_id;
callb_id_t	pm_halt_cb_id;
int		pm_devices;	/* number of managed devices */

/*
 * Autopm  must be turned on by a PM_START_PM ioctl, so we don't end up
 * power managing things in single user mode that have been suppressed via
 * power.conf entries.  Protected by pm_scan_lock.
 */
int		autopm_enabled;

/*
 * This counter (protected by pm_scan_lock) indicates whether we need to
 * run scans because of backwards compatible pm devices.  If zero, then we
 * don't
 */
int		bcpm_enabled;

int		pm_scanning, pm_scan_again;

#ifdef	DEBUG

/*
 * see common/sys/epm.h for PMD_* values
 */

uint_t		pm_debug = 0;

void prpscc(pscc_t *p);
void prpsce(psce_t *p);
void prpsc(pm_state_change_t *p);
void prrsvp(pm_rsvp_t *p);
void prdirect(char *);
void printerest(char *);
void prblocked(void);
void prthresh(char *);
void prdeps(char *);
#endif

/* Globals */

/*
 * List of automatically power managed devices
 */
pm_scan_t	*pm_scan_list;
krwlock_t	pm_scan_list_rwlock;

/*
 * List of recorded thresholds and dependencies
 */
pm_thresh_rec_t *pm_thresh_head;
krwlock_t pm_thresh_rwlock;

pm_pdr_t *pm_dep_head;
krwlock_t pm_dep_rwlock;
static int pm_unresolved_deps = 0;

int pm_default_idle_threshold = PM_DEFAULT_SYS_IDLENESS;
int pm_system_idle_threshold;
/*
 * By default nexus has 0 threshold, and depends on its children to keep it up
 */
int pm_default_nexus_threshold = 0;

/*
 * Data structures shared with common/io/pm.c
 */
kmutex_t	pm_clone_lock;
kcondvar_t	pm_clones_cv[PM_MAX_CLONE];
uint_t		pm_poll_cnt[PM_MAX_CLONE];
unsigned char	pm_interest[PM_MAX_CLONE];
struct pollhead	pm_pollhead;

int		pm_idle_down;

extern int	hz;
void		pm_set_pm_info(dev_info_t *dip, void *value);
void		pm_get_timestamps(dev_info_t *dip, time_t *valuep);
int		pm_watchers(void);
static void	pm_reset_timestamps(dev_info_t *);
extern int	pm_get_normal_power(dev_info_t *, int);
extern char	*platform_pm_module_list[];
static void	e_pm_set_cur_pwr(dev_info_t *, pm_component_t *, int);

/* Local functions */
int			pm_cur_power(pm_component_t *);
static int		dev_is_needed(dev_info_t *, int, int, int);
static clock_t		start_scan();
static time_t		scan(dev_info_t *, int, int);
static int		power_dev(dev_info_t *, int, int, int);
static dev_info_t	*find_dip(dev_info_t *, char *, int);
static boolean_t	pm_cpr_callb(void *, int);
static boolean_t	pm_panic_callb(void *, int);
static boolean_t	pm_halt_callb(void *, int);
static int		cur_power(pm_component_t *);
static int		cur_threshold(dev_info_t *, int);
static int		bring_parent_wekeeps_up(dev_info_t *, dev_info_t *,
			    pm_info_t *, int, int, int);
static int		power_val_to_index(pm_component_t *, int);
static char		*power_val_to_string(pm_component_t *, int);
static void		e_pm_create_components(dev_info_t *, int);
static int		pm_block(dev_info_t *, int, int, int);
static int		e_pm_valid_power(pm_component_t *, int);
static void		pm_dequeue_pscc(pscc_t *p, pscc_t **list);
psce_t 			*pm_psc_dip_to_direct(dev_info_t *, pscc_t **);
psce_t			*pm_psc_clone_to_direct(int);
pm_rsvp_t		*pm_rsvp_lookup(dev_info_t *, int);
void			pm_enqueue_notify(int, dev_info_t *, int, int, int);
static void		psc_entry(int, psce_t *, dev_info_t, int, int, int);
psce_t			*psc_interest(void **, pscc_t **);
static int		pm_default_ctlops(dev_info_t *dip, dev_info_t *rdip,
			    ddi_ctl_enum_t ctlop, void *arg, void *result);
static int		pm_dequeue_scan(dev_info_t *dip);
static char		*pm_parsenum(char *, int *);
static void		e_pm_set_max_power(dev_info_t *, int, int);
static void		e_pm_destroy_components(dev_info_t *);
static void		pm_run_scan(void *ignored);
static void		pm_unkepts(int, dev_info_t *, dev_info_t **);
static void		pm_unkeeps(int, dev_info_t *, dev_info_t **);

kmutex_t pm_rsvp_lock;
krwlock_t pm_pscc_direct_rwlock;
krwlock_t pm_pscc_interest_rwlock;

pscc_t *pm_pscc_interest;
pscc_t *pm_pscc_direct;

#define	PM_NUMCMPTS(dip) (DEVI(dip)->devi_pm_num_components)
#define	PM_LEVEL_UPONLY (-2)	/* only raise power level */
#define	PM_MAJOR(dip) ddi_name_to_major(ddi_binding_name(dip))
#define	PM_IS_NEXUS(dip) NEXUS_DRV(devopsp[PM_MAJOR(dip)])

#ifdef DEBUG
kmutex_t pmlloglock;
#endif
/*
 * This needs to be called after the root and platform drivers are loaded
 * and be single-threaded with respect to driver attach/detach
 */
void
pm_init(void)
{
	char **mod;
	extern pri_t minclsyspri;

	pm_scan_id = 0;
	pm_devices = 0;
	pm_system_idle_threshold = pm_default_idle_threshold;
	mutex_init(&pm_scan_lock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&pm_rsvp_lock, NULL, MUTEX_DRIVER, NULL);
#ifdef DEBUG
	mutex_init(&pmlloglock, NULL, MUTEX_DRIVER, NULL);
#endif
	rw_init(&pm_scan_list_rwlock, NULL, RW_DEFAULT, NULL);
	rw_init(&pm_thresh_rwlock, NULL, RW_DEFAULT, NULL);
	/*
	 * Set up a task queue to do rescan processing
	 */
	pm_tq = taskq_create("pm rescan queue", 2, minclsyspri, 2, 2,
	    TASKQ_PREPOPULATE);

	pm_cpr_cb_id = callb_add(pm_cpr_callb, (void *)NULL,
	    CB_CL_CPR_PM, "pm_cpr");
	pm_panic_cb_id = callb_add(pm_panic_callb, (void *)NULL,
		    CB_CL_PANIC, "pm_panic");
	pm_halt_cb_id = callb_add(pm_halt_callb, (void *)NULL,
		    CB_CL_HALT, "pm_halt");

	for (mod = platform_pm_module_list; *mod; mod++)
		if (ddi_hold_installed_driver(ddi_name_to_major(*mod)) == NULL)
			cmn_err(CE_WARN, "!cannot load platform pm driver %s\n",
			    *mod);
}

/*
 * Given a pointer to a component struct, return the current power level
 * (struct contains index unless it is a continuous level).
 * Located here in hopes of geting both this and dev_is_needed into the
 * cache together
 */
static int
cur_power(pm_component_t *cp)
{
	if (cp->pmc_cur_pwr == PM_LEVEL_UNKNOWN)
		return (cp->pmc_cur_pwr);

	PMD(PMD_LEVEL, ("cur_pwr: %s, rets %x\n", cp->pmc_comp.pmc_name,
	    cp->pmc_comp.pmc_lvals[cp->pmc_cur_pwr]))
	return (cp->pmc_comp.pmc_lvals[cp->pmc_cur_pwr]);
}

/*
 * Internal guts of ddi_dev_is_needed and pm_raise/lower_power
 */
static int
dev_is_needed(dev_info_t *dip, int cmpt, int level, int old_level)
{
	pm_info_t	*info;
	struct pm_component *cp;
	int curpwr;
	int ret;
	int e_pm_manage(dev_info_t *, int);

	/*
	 * Check if the power manager is active and if the
	 * device is power managed if not, the device must be on.
	 * To make the common case (device is power managed already)
	 * fast, we check without the lock.  If device is not already
	 * power managed, then we take the lock and the long route through
	 * go get it managed.  Devices never go unmanaged until they
	 * detach.
	 */
	info = PM_GET_PM_INFO(dip);
	if (!info) {
		if (!DEVI_IS_ATTACHING(dip)) {
			return (DDI_FAILURE);
		}
		/* now check again under the lock */
		PM_LOCK_DIP(dip);
		info = PM_GET_PM_INFO(dip);
		if (!info) {
			ret = e_pm_manage(dip, 2);
			if (ret != DDI_SUCCESS) {
				PM_UNLOCK_DIP(dip);
				return (DDI_FAILURE);
			} else {
				info = PM_GET_PM_INFO(dip);
				ASSERT(info);
			}
		}
		PM_UNLOCK_DIP(dip);
	}
	if (cmpt < 0 || cmpt >= DEVI(dip)->devi_pm_num_components) {
		return (DDI_FAILURE);
	}
	cp = &DEVI(dip)->devi_pm_components[cmpt];

	if (!e_pm_valid_power(cp, level)) {
		return (DDI_FAILURE);
	}

	/*
	 * If an app (SunVTS or Xsun) has taken control, then block until it
	 * gives it up
	 */
	if (info->pmi_dev_pm_state & PM_DIRECT_NEW) {
		curpwr = cur_power(cp);
		switch (pm_block(dip, cmpt, level, curpwr)) {
			case PMP_RELEASE:
				break;			/* bring it up */
			case PMP_SUCCEED:
				return (DDI_SUCCESS);
			case PMP_FAIL:
				return (DDI_FAILURE);
		}
	}

	PM_LOCK_POWER(dip, cmpt);
	curpwr = cur_power(cp);
	PMD(PMD_DIN, ("dev_is_needed: %s@%s cmpt=%d, old_level=%d, level=%d, "
	    "curpwr=%d\n", PM_NAME(dip), PM_ADDR(dip), cmpt,
	    old_level, level, curpwr))
	if (old_level == PM_LEVEL_UPONLY) {
		if (curpwr >= level) {
			PM_UNLOCK_POWER(dip, cmpt);
			return (DDI_SUCCESS);
		}
	} else {
		if (old_level != PM_LEVEL_UNKNOWN && curpwr != old_level) {
			PM_UNLOCK_POWER(dip, cmpt);
			return (DDI_FAILURE);
		}
		if (curpwr == level) {
			PM_UNLOCK_POWER(dip, cmpt);
			return (DDI_SUCCESS);
		}
	}
	/* Let's do it */
	if (pm_set_power(dip, cmpt, level,  curpwr, info, 1) != DDI_SUCCESS) {
		if (old_level == PM_LEVEL_UPONLY) {
			cmn_err(CE_WARN, "Device %s%d failed to power up.",
			    ddi_node_name(dip), ddi_get_instance(dip));
			cmn_err(CE_WARN,
			    "Please see your system administrator or reboot.");
		}
		PM_UNLOCK_POWER(dip, cmpt);
		return (DDI_FAILURE);
	}
	PM_UNLOCK_POWER(dip, cmpt);
	if (pm_watchers()) {
		mutex_enter(&pm_rsvp_lock);
		pm_enqueue_notify(PSC_HAS_CHANGED, dip, cmpt, level, curpwr);
		mutex_exit(&pm_rsvp_lock);
	}
	PMD(PMD_RESCAN | PMD_DIN, ("dev_is_needed rescan\n"))
	pm_rescan(0);
	return (DDI_SUCCESS);
}

/*
 * Start another scan later
 */
void
pm_rescan(int canblock)
{
	static void pm_async_rescan(void *);

	ASSERT(!mutex_owned(&pm_scan_lock));
	mutex_enter(&pm_scan_lock);
	/*
	 * If scan is running and will check before stopping, then we just need
	 * to make sure it knows to run again.
	 */
	if (pm_scanning) {
		if (!pm_scan_again) {
			pm_scan_again = 1;
		}
		mutex_exit(&pm_scan_lock);
		return;
	}
	mutex_exit(&pm_scan_lock);
	/*
	 * If can't schedule the task, then it is because it is already running
	 * so we'll do a timeout ourselves just to cover the gap
	 */
	if (!taskq_dispatch(pm_tq, pm_async_rescan, (void *)NULL,
	    (canblock ? KM_SLEEP : KM_NOSLEEP))) {
		PMD((PMD_RESCAN | PMD_FAIL),
		    ("pm_rescan, could not queue task\n"))
		(void) timeout(pm_run_scan, NULL, pm_min_scan * hz);
	}
}

static void
pm_async_rescan(void *ignored)
{
	_NOTE(ARGUNUSED(ignored))
	timeout_id_t scanid;
	/*
	 * Cancel previous scan and start another one to take this
	 * change into account (in a little while, to let things settle
	 * down).  If we drop the lock we may have multiple outstanding
	 * timeouts with only the most recent recorded, but it won't hurt
	 * us to have an extra one (and may help)
	 */
	mutex_enter(&pm_scan_lock);
	if (pm_scan_id) {
		scanid = pm_scan_id;
		pm_scan_id = 0;
		mutex_exit(&pm_scan_lock);
		(void) untimeout(scanid);
		mutex_enter(&pm_scan_lock);
		if (pm_scan_id) {	/* someone else did it */
			mutex_exit(&pm_scan_lock);
			PMD(PMD_RESCAN, ("pm_rescan: missed it\n"))
			return;
		}
	}
	if (autopm_enabled || bcpm_enabled) {
		pm_scan_id = timeout(pm_run_scan, NULL, pm_min_scan * hz);
		mutex_exit(&pm_scan_lock);
		PMD(PMD_RESCAN, ("pm_rescan: %lxs\n", pm_min_scan))
	} else {
		mutex_exit(&pm_scan_lock);
		PMD(PMD_RESCAN, ("pm_rescan: no pm\n"))
	}
}

/*
 * We want to keep scanning as long as somebody has asked us to (pm_scan_again)
 * by calling pm_rescan
 */
static void
pm_run_scan(void *ignored)
{
	_NOTE(ARGUNUSED(ignored))
	clock_t nextscan;

	mutex_enter(&pm_scan_lock);
	if (pm_scanning) {		/* already running */
		mutex_exit(&pm_scan_lock);
		return;
	}
	pm_scanning = 1;
	do {
		ASSERT(pm_scanning);
		pm_scan_again = 0;
		mutex_exit(&pm_scan_lock);
		nextscan = start_scan();
		mutex_enter(&pm_scan_lock);
	} while (pm_scan_again);
	pm_scanning = 0;
	/* Schedule for next idle check. */
	if (nextscan != LONG_MAX) {
		if (pm_scan_id) {
			timeout_id_t scanid;

			scanid = pm_scan_id;
			pm_scan_id = 0;
			mutex_exit(&pm_scan_lock);
			/* XXX would deadlock if this were our id */
			(void) untimeout(scanid);
			mutex_enter(&pm_scan_lock);
		}
		if (pm_scan_id) {
			/*
			 * somebody did a rescan while we had dropped the lock
			 */
			mutex_exit(&pm_scan_lock);
			return;
		}
		if (nextscan > (LONG_MAX / hz)) {
			/*
			 * XXX prevent wrap to negative timeout
			 */
			nextscan = (LONG_MAX - 1) / hz;
		}
		if (autopm_enabled || bcpm_enabled) {
			PMD(PMD_RESCAN, ("next scan in %lx\n", nextscan))
			pm_scan_id = timeout(pm_run_scan, (caddr_t)0,
			    (clock_t)(nextscan * hz));
		} else {
			PMD(PMD_RESCAN, ("no active pm, scan not restarted\n"))
		}
	}
	mutex_exit(&pm_scan_lock);
}

/*
 * Walk the list of power managed devices looking for idle devices to power
 * manage.
 *
 * We have all power managed devices in a list (pm_scan_list).
 * We walk the list (skipping over any node that has children, since automatic
 * power-down operations (which is what we're about) always inititate from
 * the bottom up).
 * If the leaf device is ready (and not kept up by another), then we power
 * it down and scan up the tree to see if we can turn off its ancestors or
 * any that it keeps up.
 */

static clock_t
start_scan(void)
{
	clock_t	nextscan, scantime;
	pm_scan_t	*cur;
	pm_info_t	*info;
	static	int 	pm_has_children(dev_info_t *);

	nextscan = LONG_MAX;

	if (!bcpm_enabled && !autopm_enabled) {
		PMD(PMD_SCAN, ("start_scan, no pm enabled\n"))
		return (nextscan);
	}
	/*
	 * Go through scan list to see if any leaf node is ready to be
	 * shut down.
	 */
	rw_enter(&pm_scan_list_rwlock, RW_READER);
	for (cur = pm_scan_list; cur != NULL; cur = cur->ps_next) {
		/*
		 * We only scan from the leaves up, except that involved
		 * parents look just like kids to us (they promise to reflect
		 * their childrens dependencies on them in their idle/busy
		 * state)
		 */
		if (pm_has_children(cur->ps_dip) &&
		    !(DEVI(cur->ps_dip)->devi_pm_flags & PMC_WANTS_NOTIFY))
			continue;
		info = PM_GET_PM_INFO(cur->ps_dip);
		ASSERT(info);
		if ((info->pmi_dev_pm_state & PM_SCAN) == 0) {
			PMD(PMD_SCAN, ("[s_s: %s@%s !PM_SCAN]\n",
			    PM_NAME(cur->ps_dip), PM_ADDR(cur->ps_dip)))
			continue;
		}
		scantime = scan(cur->ps_dip, 0, 1);
		if (scantime == 0) /* device is now off */
			continue;
		nextscan = min(scantime, nextscan);
	}
	rw_exit(&pm_scan_list_rwlock);
	return (max(nextscan, pm_min_scan));
}

/*
 * This routine returns the time left before the device becomes power
 * managable.  There are two special values.
 * LONG_MAX means we can't manage the device ever, 0 means it is powered down.
 * If device is "kept up by", then our result depends
 * on the time that other devices have been idle.
 * Held indicates that the node is already locked by the caller
 * See comment block before start_scan for description of algorithm
 * We treate direct (PM_ISDIRECT(dip)) devices differently.  We only scan
 * them to see if they are off so we can turn their ancestors off.
 */
static time_t
scan(dev_info_t *dip, int held, int climb)
{
	time_t	*timestamp, now, timeleft = 0;
	time_t		scantime = 0;
	time_t		thresh;
	int		i, component_is_on = 0;
	size_t		size;
	pm_info_t	*info;
	pm_component_t *cp;
	time_t comptime;
	int nxtpwr, curpwr;
	static int pm_next_lower_power(pm_component_t *);
	int cur_threshold(dev_info_t *, int);
	dev_info_t *pdip;
	pm_info_t *pinfo;

	ASSERT(!MUTEX_HELD(&pm_scan_lock));
	ASSERT(dip);
	PMD(PMD_SCAN, ("scan: %s@%s, %s%s",
	    ddi_get_name(dip), PM_ADDR(dip), (held ? " held" : ""),
	    (climb ? " climb" : "")))
	ASSERT(DDI_CF2(dip) && !DDI_DRV_UNLOADED(dip));
	info = PM_GET_PM_INFO(dip);
	ASSERT(info);		/* else should not be on this list! */
	/*
	 * If this dip is not already held by the caller and is currently
	 * undergoing a power level change then we want to just skip over it
	 * and try to continue the scan with the next item on the list.
	 */
	if (!held && !PM_TRY_LOCK_POWER(dip, PMC_ALLCOMPS)) {
		PMD(PMD_SCAN, (" not held and can't lock power, retuns "
		    "pm_min_scan\n"))
		return (pm_min_scan);
	}

	if ((info->pmi_dev_pm_state & PM_SCAN) == 0 ||
	    info->pmi_kidsupcnt != 0) {
		if (!held)
			PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
		PMD(PMD_SCAN, (" %s return LONG_MAX\n",
		    ((info->pmi_dev_pm_state & PM_SCAN) == 0 ?
		    "PM_SCAN clear" : "pmi_kidsupcnt nonzero"),
		    PM_NAME(dip), PM_ADDR(dip)))
		return (LONG_MAX);
	}
	/*
	 * If autopm is disabled and this is not a backwards compatible device
	 * then we say wait forever, since it will not change again until
	 * autopm is restarted, which will also rescan
	 */
	if (!autopm_enabled && !PM_ISBC(dip)) {
		if (!held)
			PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
		PMD(PMD_SCAN, ("not autopm and not BC; returns LONG_MAX\n"))
		return (LONG_MAX);
	}
	PMD(PMD_SCAN, ("\n"))
	/*
	 * If the device is not directly managed and is being kept up by some
	 * other(s), then we check again when the other(s) are ready
	 */
	if (!PM_ISDIRECT(dip)) {
		for (i = 0; i < info->pmi_nkeptupby; i++) {
			scantime = scan(info->pmi_kupbydips[i], 0, 0);
			if (scantime == 0)
				continue;
			timeleft = max(timeleft, max(scantime, pm_min_scan));
		}
		if (timeleft) {
			if (!held)
				PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
			PMD(PMD_SCAN, ("scan: %s@%s kept up for %lx\n",
			    PM_NAME(dip), PM_ADDR(dip), timeleft))
			return (timeleft);
		}
	}
	now = hrestime.tv_sec;
	size = PM_NUMCMPTS(dip) * sizeof (time_t);
	timestamp = kmem_alloc(size, KM_SLEEP);
	pm_get_timestamps(dip, timestamp);
	ASSERT(PM_IAM_LOCKING_POWER(dip, PMC_ALLCOMPS));
	/*
	 * Note that we deal with components 1-n first, then consider
	 * component 0
	 */
	for (i = 1; i < PM_NUMCMPTS(dip); i++) {
		time_t	comptime;

		PMD(PMD_SCAN, ("scan: comp %d of %s@%s", i, ddi_get_name(dip),
		    PM_ADDR(dip)))

		/*
		 * If already off (an optimization, perhaps)
		 */
		cp = &DEVI(dip)->devi_pm_components[i];
		curpwr = cur_power(cp);
		if (curpwr == 0) {
			PMD(PMD_SCAN, (" off\n"))
			continue;
		} else {
			/*
			 * We give up early for direct pm'd devices, since
			 * we're only interested in finding them off so we
			 * can take down their parents
			 */
			if (PM_ISDIRECT(dip)) {
				if (!held)
					PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
				PMD(PMD_SCAN, ("DIRECT; returns LONG_MAX\n"))
				return (LONG_MAX);
			}
		}
		component_is_on++;
		if (cp->pmc_cur_pwr == 0) {
			PMD(PMD_SCAN, (" lowest\n"))
			continue;
		}
		thresh = cur_threshold(dip, i);
		if (timestamp[i] == 0) {		/* busy */
			if (timeleft == 0)
				timeleft = max(thresh, pm_min_scan);
			else
				timeleft = min(timeleft, max(thresh,
				    pm_min_scan));
			PMD(PMD_SCAN, (" busy; thresh %lx, timeleft %lx\n",
			    thresh, timeleft))
			continue;
		}
		comptime = now - timestamp[i];
		if (comptime >= thresh || pm_idle_down) {
			PMD(PMD_SCAN, ("idle\n"))
			nxtpwr = pm_next_lower_power(cp);
			if (pm_set_power(dip, i, nxtpwr, curpwr, info, 0) !=
			    DDI_SUCCESS) {
#ifdef DEBUG
				if (!info->pmi_warned) {
					PMD(PMD_FAIL, ("!pm: can't power down"
					    " %s%s component %d",
					    PM_NAME(dip), PM_ADDR(dip), i))
					info->pmi_warned++;
				}
#endif
				PMD(PMD_SCAN, ("pm_set_power failed: restart "
				    "scan\n"))
				timeleft = pm_min_scan;
				continue;
			} else {
				if (pm_watchers()) {
					mutex_enter(&pm_rsvp_lock);
					pm_enqueue_notify(PSC_HAS_CHANGED, dip,
					    i, nxtpwr, curpwr);
					mutex_exit(&pm_rsvp_lock);
				}
				if (nxtpwr == 0) {	/* component went off */
					component_is_on--;
					continue;
				}
				/*
				 * Threshold could also be 0
				 */
				if (timeleft == 0)
					timeleft = max(cur_threshold(dip, i),
					    pm_min_scan);
				else
					timeleft = min(timeleft,
					    max(cur_threshold(dip, i),
					    pm_min_scan));
				PMD(PMD_SCAN, ("scan: powered down %s@%s[%d], "
				    "timeleft %lx\n", PM_NAME(dip),
				    PM_ADDR(dip), i, timeleft))
			}
		} else {
			if (timeleft == 0)
				timeleft = max(thresh - comptime, pm_min_scan);
			else
				timeleft = min(timeleft, max(thresh - comptime,
				    pm_min_scan));
			PMD(PMD_SCAN, ("scan: %s@%s[%d] wait %lx, timeleft "
			    "%lx\n", PM_NAME(dip), PM_ADDR(dip), i,
			    comptime, timeleft))
		}
	}
	/*
	 * nothing to do to comp 0 of backwards compat device if others are on
	 * "new" style devices don't do dependency on component 0
	 */
	if (PM_ISBC(dip) && component_is_on)
		goto done;
	/*
	 * Now handle component 0
	 */
	cp = &DEVI(dip)->devi_pm_components[0];
	thresh = cur_threshold(dip, 0);
	curpwr = cur_power(cp);
	PMD(PMD_SCAN, ("scan: comp 0 of %s@%s", ddi_get_name(dip),
	    PM_ADDR(dip)))
	/*
	 * We give up early for direct pm'd devices, since
	 * we're only interested in finding them off so we
	 * can take down their parents
	 */
	if (curpwr != 0 && PM_ISDIRECT(dip)) {
		if (!held)
			PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
		PMD(PMD_SCAN, ("DIRECT; returns LONG_MAX\n"))
		return (LONG_MAX);
	}
	/*
	 * See if this lets us power down any ancestors
	 * (unless there are none that can be powered down.
	 * Accounting is done in pm_set_power()).
	 * If parent not power managed (info NULL) we're done.
	 */
	if (curpwr == 0 && climb && !component_is_on &&
	    (pdip = ddi_get_parent(dip)) && (pinfo = PM_GET_PM_INFO(pdip))) {
		PMD(PMD_SCAN, (" off"))
		if (pinfo->pmi_kidsupcnt == 0) {
			ASSERT(timeleft == 0);
			PM_LOCK_POWER(pdip, PMC_ALLCOMPS);
			timeleft = scan(pdip, 1, 1);
			PM_UNLOCK_POWER(pdip, PMC_ALLCOMPS);
		}
#ifdef DEBUG
		else {
			PMD(PMD_KIDSUP, ("did not scan %s@%s because kuc %d\n",
			    PM_NAME(pdip), PM_ADDR(pdip),
			    pinfo->pmi_kidsupcnt));
		}
#endif
		PMD(PMD_SCAN, ("\nscan %s@%s", PM_NAME(dip),
		    PM_ADDR(dip)))
		goto done;
	}

	if (cp->pmc_cur_pwr == 0) {	/* we're done */
		if (curpwr == 0) {		/* component is off */
			ASSERT(timeleft == 0 || !PM_ISBC(dip));
			PMD(PMD_SCAN, (" off"))
			goto done;
		}
		PMD(PMD_SCAN, (" lowest"))
		if (PM_ISBC(dip))	/* if old style device, we're done */
			timeleft = LONG_MAX;	/* nothing more we can do */
		else
			timeleft = max(pm_min_scan, timeleft);
		goto done;
	}
	if (timestamp[0] == 0) {		/* busy */
		PMD(PMD_SCAN, (" busy"))
		timeleft = max(timeleft, max(thresh, pm_min_scan));
		goto done;
	}
	comptime = now - timestamp[0];
	/*
	 * if it is not idle long enough
	 */
	if (comptime < thresh && (!pm_idle_down)) {
		if (timeleft == 0)
			timeleft = max(thresh - comptime, pm_min_scan);
		else
			timeleft = min(timeleft, max(thresh - comptime,
			    pm_min_scan));
		PMD(PMD_SCAN, (" waiting %lx", timeleft))
		goto done;
	}

	PMD(PMD_SCAN, (" idle"))
	nxtpwr = pm_next_lower_power(cp);
	curpwr = cur_power(cp);
	if (pm_set_power(dip, 0, nxtpwr, curpwr, info, 0) != DDI_SUCCESS) {
#ifdef DEBUG
		if (!info->pmi_warned) {
			PMD(PMD_FAIL, ("!pm: can't power down %s%s component "
			    "%d", PM_NAME(dip), PM_ADDR(dip), 0))
			info->pmi_warned++;
		}
#endif
		timeleft = pm_min_scan;
	} else {
		if (pm_watchers()) {
			mutex_enter(&pm_rsvp_lock);
			pm_enqueue_notify(PSC_HAS_CHANGED, dip,
			    0, nxtpwr, curpwr);
			mutex_exit(&pm_rsvp_lock);
		}
		PMD(PMD_SCAN, (" comp down, timeleft %x",
		    timeleft))
		if (nxtpwr == 0) {	/* we turned it off */
			PMD(PMD_SCAN, (" off"))
			/*
			 * See if this lets us power down any ancestors
			 * (unless there are none that can be powered down.
			 * Accounting is done in pm_set_power()).
			 * If parent not power managed (info NULL) we're done.
			 */
			if (climb && !component_is_on &&
			    (pdip = ddi_get_parent(dip)) &&
			    (pinfo = PM_GET_PM_INFO(pdip))) {
				if (pinfo->pmi_kidsupcnt == 0) {
					ASSERT(timeleft == 0);
					PM_LOCK_POWER(pdip, PMC_ALLCOMPS);
					timeleft = scan(pdip, 1, 1);
					PM_UNLOCK_POWER(pdip, PMC_ALLCOMPS);
				}
#ifdef DEBUG
				else {
					PMD(PMD_KIDSUP, ("did not scann %s@%s "
					    "because %s@%s kuc %d\n",
					    PM_NAME(dip), PM_ADDR(dip),
					    PM_NAME(pdip), PM_ADDR(pdip),
					    pinfo->pmi_kidsupcnt));
				}
#endif
				PMD(PMD_SCAN, ("\nscan %s@%s",
				    PM_NAME(dip), PM_ADDR(dip)))
			}
			if (!held)
				PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
			PMD(PMD_SCAN, (" after climb; timeleft %lx\n",
			    timeleft))
			kmem_free(timestamp, size);
			return (timeleft);
		}
		if (timeleft == 0)
			timeleft = max(cur_threshold(dip, 0), pm_min_scan);
		else
			timeleft = min(timeleft, max(cur_threshold(dip, 0),
			    pm_min_scan));
	}

done:
	if (!held)
		PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
	kmem_free(timestamp, size);
	PMD(PMD_SCAN, ("; scan done timeleft %x\n", timeleft))
	return (timeleft);
}

/*
 * Powers a  device, suspending or resuming the device if it is a backward
 * compatible device, notifying parents before and after if necessary
 * Called with the component's power lock held.
 */
static int
power_dev(dev_info_t *dip, int comp, int level, int old_level)
{
	power_req_t power_req;
	int		power_op_ok;	/* DDI_SUCCESS or DDI_FAILURE */
	int		resume_needed = 0;
	int		result;
	struct pm_component *cp = &DEVI(dip)->devi_pm_components[comp];
	int		bc = PM_ISBC(dip);
#ifdef DEBUG
	char *ppmname, *ppmaddr;
#endif

	/*
	 * If this is comp 0 of a backwards compat device and we are
	 * going to take the power away, we need to detach it with
	 * DDI_PM_SUSPEND command.
	 */
	if (bc && comp == 0 && level == 0) {
		if (devi_detach(dip, DDI_PM_SUSPEND) != DDI_SUCCESS) {
			/* We could not suspend before turning cmpt zero off */
			PMD(PMD_ERROR, ("power_dev: could not suspend %s@%s\n",
			    PM_NAME(dip), PM_ADDR(dip)))
			return (DDI_FAILURE);
		} else {
			ASSERT(PM_IAM_LOCKING_POWER(dip, PMC_DONTCARE));
			DEVI(dip)->devi_pm_flags |= PMC_SUSPENDED;
		}
	}

	/*
	 * If parent plays "parental involvement", then we don't
	 * do "parental dependency, we assume parent does
	 * the correct thing for itself
	 */
	if (DEVI(dip)->devi_pm_flags & PMC_NOTIFY_PARENT) {
		PMD(PMD_BRING, (" prepower"))
		if (e_ddi_prepower(dip, comp, level, old_level) !=
		    DDI_SUCCESS) {
		    return (DDI_FAILURE);
		}
	}
	power_req.request_type = PMR_PPM_SET_POWER;
	power_req.req.ppm_set_power_req.who = dip;
	power_req.req.ppm_set_power_req.cmpt = comp;
	power_req.req.ppm_set_power_req.old_level = old_level;
	power_req.req.ppm_set_power_req.new_level = level;

#ifdef DEBUG
	if (DEVI(dip)->devi_pm_ppm) {
		ppmname =
		    PM_NAME((dev_info_t *)DEVI(dip)->devi_pm_ppm);
		ppmaddr =
		    PM_ADDR((dev_info_t *)DEVI(dip)->devi_pm_ppm);

	} else {
		ppmname = "noppm";
		ppmaddr = "0";
	}

	PMD(PMD_PPM, ("power_dev: %s@%s:%s (%d) %s (%d) -> %s (%d) "
	    "via %s@%s\n", PM_NAME(dip), PM_ADDR(dip),
	    cp->pmc_comp.pmc_name, comp,
	    power_val_to_string(cp, old_level), old_level,
	    power_val_to_string(cp, level), level,
	    ppmname, ppmaddr))
#endif

	if ((power_op_ok = pm_ctlops((dev_info_t *)DEVI(dip)->devi_pm_ppm, dip,
	    DDI_CTLOPS_POWER, &power_req, &result)) == DDI_SUCCESS) {
		ASSERT(PM_IAM_LOCKING_POWER(dip, PMC_DONTCARE));
		e_pm_set_cur_pwr(dip, &DEVI(dip)->devi_pm_components[comp],
			    level);
	} else {
		PMD(PMD_FAIL, ("power_dev: Can't set comp %d (%s) of %s@%s to "
		    "level %d (%s).", comp, cp->pmc_comp.pmc_name,
		    PM_NAME(dip), PM_ADDR(dip), level,
		    power_val_to_string(cp, level)));
	}

	if (DEVI(dip)->devi_pm_flags & PMC_NOTIFY_PARENT) {
		PMD(PMD_SET, ("postpower %s\n",
		    power_op_ok == DDI_SUCCESS ? "ok" : "failed"))
		if (e_ddi_postpower(dip, comp, level,
		    old_level, power_op_ok) != DDI_SUCCESS) {
			/* XXX debug only ? */
			PMD(PMD_FAIL, ("pm: postpower call "
			    "to %s@%s failed for %s@%s\n",
			    ddi_get_name(ddi_get_parent(dip)),
			    PM_ADDR(ddi_get_parent(dip)),
			    ddi_get_name(dip),
			    PM_ADDR(dip)))
		}
	}
	/*
	 * We will have to resume the device if the device is backwards compat
	 * device and either of the following cases is true:
	 * case 1:
	 *	This is comp 0 and we have successfully powered it up
	 *	from 0 to a non-zero level.
	 * case 2:
	 *	This is comp 0 and we have failed to power it down from
	 *	a non-zero level to 0. Resume is needed because we have just
	 *	detached it with DDI_PM_SUSPEND command.
	 */
	if (bc && comp == 0) {
		if (power_op_ok == DDI_SUCCESS) {
			if (old_level == 0 && level != 0)
				resume_needed = 1;
		} else {
			if (old_level != 0 && level == 0)
				resume_needed = 1;
		}
	}

	if (resume_needed) {
		ASSERT(DEVI(dip)->devi_pm_flags & PMC_SUSPENDED);
		if ((power_op_ok = devi_attach(dip, DDI_PM_RESUME)) ==
		    DDI_SUCCESS) {
			ASSERT(PM_IAM_LOCKING_POWER(dip, PMC_DONTCARE));
			DEVI(dip)->devi_pm_flags &= ~PMC_SUSPENDED;
		} else
			cmn_err(CE_WARN, "!pm: Can't resume %s@%s",
			    ddi_node_name(dip), PM_ADDR(dip));
	}

	return (power_op_ok);
}

/*
 * pm_set_power: adjusts power level of device.	 Assumes device is power
 * manageable & component exists.
 *
 * Cases which effect parent (or wekeepup) (of backwards compatible device):
 *	component 0 is off and we're bringing it up from 0
 *		bring up parent first
 *		bring up wekeepup first
 *	and recursively when component 0 is off and we bring some other
 *	component up from 0
 * For devices which are not backward compatible, our dependency notion is much
 * simpler.  Unless all components are off, then parent/wekeeps must be on.
 * We don't treat component 0 differently.
 * Canblock is true if this call originated from a thread that should be
 * blocked if a target device is PM_DIRECT_NEW.
 */

int
pm_set_power(dev_info_t *dip, int comp, int level, int old_level,
    pm_info_t *info, int canblock)
{
	int		normal0, current0, i, incr = 0;
	int		doparents = 0;
	struct pm_component *cp = &DEVI(dip)->devi_pm_components[comp];
	dev_info_t	*par = ddi_get_parent(dip);
	int		bc = PM_ISBC(dip);
	pm_info_t	*pinfo;

	ASSERT(!MUTEX_HELD(&pm_scan_lock));
	ASSERT(PM_IAM_LOCKING_POWER(dip, comp));
	ASSERT(info);

	PMD(PMD_SET, ("pm_set_power %s@%s, comp %d, level %d, info %p\n",
	    PM_NAME(dip), PM_ADDR(dip), comp, level, (void *)info))

	/*
	 * If we are not turning a comp off, then we need to check to
	 * see if we need to bring up parents and wekeeps.  The rules are
	 * different for backwards compat devices (strict dependency)
	 */
	if (level) {
		if (bc && comp == 0 && (old_level == 0 ||
		    old_level == PM_LEVEL_UNKNOWN))
			doparents = 1;
		else {
			doparents = 1;		/* assume all are off */
			for (i = 0; i < PM_NUMCMPTS(dip); i++) {
				cp = &DEVI(dip)->devi_pm_components[i];
				if (cur_power(cp)) {
					doparents = 0;
					break;
				}
			}
		}
	}
	/*
	 * Account for kids keeping parents up (in the case where parents not
	 * involved only--involved parents are left with count always 0).
	 * We have to do this before we try to bring up the device, otherwise
	 * the parent might get powered down again if the device takes a long
	 * time.
	 */
	if (par && (pinfo = PM_GET_PM_INFO(par)) &&
	    !(DEVI(par)->devi_pm_flags & PMC_WANTS_NOTIFY) &&
	    (!PM_ISBC(dip) || comp == 0) && old_level == 0 && level != 0) {
		incr = 1;
		PM_LOCK_DIP(par);
		PMD(PMD_KIDSUP, ("pm_set_power %s@%s kuc %d to %d because "
		    "%s@%s[%d] %d->%d\n", PM_NAME(par), PM_ADDR(par),
		    pinfo->pmi_kidsupcnt, pinfo->pmi_kidsupcnt + 1,
		    PM_NAME(dip), PM_ADDR(dip), comp, old_level, level))
		pinfo->pmi_kidsupcnt++;
		PM_UNLOCK_DIP(par);
	}
	if (doparents) {
		/*
		 * we have to bring up (or notify) parent first, and
		 * bring up wekeepups first
		 */
		if (bring_parent_wekeeps_up(dip, par, info,
		    level, old_level, canblock) != DDI_SUCCESS) {
			PMD(PMD_ERROR, ("pm_set_power(%s@%s) can't bring up "
			    "pars and wekeeps\n", PM_NAME(dip), PM_ADDR(dip)))
			goto failed;
		}
	}
	if (bc && comp != 0) {
		/*
		 * If we're turning comp on but comp 0 is off
		 */
		normal0 = pm_get_normal_power(dip, 0);
		current0 = PM_CURPOWER(dip, 0);
		if (level &&
		    (current0 == 0 || current0 == PM_LEVEL_UNKNOWN)) {
			/*
			 * Here we handle the implied dependency on comp 0
			 */
			PM_LOCK_POWER(dip, 0);
			if (pm_set_power(dip, 0, normal0,
			    current0, info, canblock) != DDI_SUCCESS) {
				PM_UNLOCK_POWER(dip, 0);
				return (DDI_FAILURE);
			}
			PM_UNLOCK_POWER(dip, 0);
		}
		return (power_dev(dip, comp, level, old_level));
	}
	if (power_dev(dip, comp, level, old_level) == DDI_SUCCESS) {
		/*
		 * Account for kids keeping parents up (in the
		 * case where parents not involved only--involved
		 * parents are left with count always 0).
		 * We already incremented count before calling power_dev if we
		 * needed to, here we deal with the need to decrement the count
		 */
		if (par && (pinfo = PM_GET_PM_INFO(par)) &&
		    !(DEVI(par)->devi_pm_flags & PMC_WANTS_NOTIFY) &&
		    (!PM_ISBC(dip) || comp == 0) && level == 0 &&
		    old_level != 0) {
			PM_LOCK_DIP(par);
			PMD(PMD_KIDSUP, ("pm_set_power %s@%s kuc %d to %d "
			    "because %s@%s[%d] %d->%d\n", PM_NAME(par),
			    PM_ADDR(par), pinfo->pmi_kidsupcnt,
			    pinfo->pmi_kidsupcnt - 1, PM_NAME(dip),
			    PM_ADDR(dip), comp, old_level, level))
			pinfo->pmi_kidsupcnt--;
			ASSERT(pinfo->pmi_kidsupcnt >= 0);
			if (pinfo->pmi_kidsupcnt == 0)
				pm_rescan(1);
			PM_UNLOCK_DIP(par);
		}
		return (DDI_SUCCESS);
	} else {
		/* if we incremented the count but the op failed */
failed:
		if (incr) {
			PM_LOCK_DIP(par);
			PMD(PMD_KIDSUP, ("pm_set_power %s@%s kuc %d to %d "
			    "because %s@%s[%d] %d->%d failed\n",
			    PM_NAME(par), PM_ADDR(par),
			    pinfo->pmi_kidsupcnt, pinfo->pmi_kidsupcnt - 1,
			    PM_NAME(dip), PM_ADDR(dip), comp,
			    old_level, level))
			pinfo->pmi_kidsupcnt--;
			ASSERT(pinfo->pmi_kidsupcnt >= 0);
			if (pinfo->pmi_kidsupcnt == 0)
				pm_rescan(1);
			PM_UNLOCK_DIP(par);
		}
		return (DDI_FAILURE);
	}
}


/*
 * This function recognizes names in two forms and returns the corresponding
 * dev-info node.
 * The first, old form (physpath == 0) allows he name to be either a pathname
 * leading to a special file, or to specify the device in its name@address form.
 * The name can include the parent(s) name to provide uniqueness.
 * The first matched name is returned.
 *
 * The second, new form (physpath == 1) allows only a (full) physical path
 * (the /devices subpath minus the minor string).
 */
dev_info_t *
pm_name_to_dip(char *pathname, int physpath)
{
	dev_info_t	*dip = NULL;
	vnode_t		*vp;
	char		dev_name[MAXNAMELEN];
	char		node_name[MAXNAMELEN];
	char		*ap, *np;
	major_t		major;

	if (!pathname)
		return (NULL);

	(void) strncpy(dev_name, pathname, MAXNAMELEN);


	if (!physpath) {	/* the old obsolete way to specify a device */
		PMD(PMD_NAMETODIP, ("n2dip dev:%s node:%s\n", dev_name,
		    node_name))
		/*
		 * We can't hold devinfo_tree_lock across a call to
		 * ddi_hold_installed_driver, but we can attempt to attach the
		 * driver for the device we're looking for, then if we find
		 * any nodes that don't have name_addr fields inited in
		 * find_dip we can just prune the tree there.
		 * First, get a null-terminated node name
		 */
		np = strrchr(dev_name, '/');
		if (np != NULL)
			(void) strcpy(node_name, np + 1);
		else
			(void) strcpy(node_name, dev_name);
		if ((ap = strchr(node_name, '@')) != NULL)
			*ap = '\0';
		if ((major = ddi_name_to_major(node_name)) != (major_t)(-1) &&
		    ddi_hold_installed_driver(major) != NULL) {

			rw_enter(&devinfo_tree_lock, RW_READER);

			ddi_rele_driver(major);

			if (dev_name[0] == '/')
				dip = find_dip(ddi_get_child(ddi_root_node()),
				    dev_name + 1, 0);
			else
				dip = find_dip(ddi_root_node(), dev_name, 1);
			rw_exit(&devinfo_tree_lock);

			if (dip) {
				PMD(PMD_NAMETODIP, ("n2dip: ret %s@s",
				    ddi_get_name(dip), PM_ADDR(dip)))
				return (dip);
			}
		}
#ifdef	DEBUG
		else {
			PMD(PMD_NAMETODIP, ("major %d\n", major))
		}
#endif
	} else {	/* physpath true */
		PMD(PMD_NAMETODIP, ("n2dip dev:%s\n", dev_name))
		rw_enter(&devinfo_tree_lock, RW_READER);

		if (dev_name[0] == '/')
			dip = find_dip(ddi_get_child(ddi_root_node()),
			    dev_name + 1, 0);
		else {
			PMD(PMD_NAMETODIP, ("n2dip physpath with unrooted "
			    "search\n"))
			rw_exit(&devinfo_tree_lock);
			return (NULL);
		}
		rw_exit(&devinfo_tree_lock);

		if (dip) {
			PMD(PMD_NAMETODIP, ("n2dip: ret %s@%s",
			    ddi_get_name(dip), PM_ADDR(dip)))
			return (dip);
		}
		PMD(PMD_NAMETODIP, ("n2dip returns NULL\n"))
		return (NULL);
	}

	if (lookupname(dev_name, UIO_SYSSPACE, FOLLOW, NULL, &vp)) {
		PMD(PMD_NAMETODIP, ("lookupname(%s) failed\n", dev_name))
		return (NULL);
	}

	if (vp->v_type != VCHR && vp->v_type != VBLK) {
		VN_RELE(vp);
		PMD(PMD_NAMETODIP, ("v_type (%x)\n", vp->v_type))
		return (NULL);
	}

	major = getmajor(vp->v_rdev);
	PMD(PMD_NAMETODIP, ("major %x\n", major))
	if (ddi_hold_installed_driver(major)) {
		dip = dev_get_dev_info(vp->v_rdev, 0);
		if (dip)
			ddi_rele_driver(major);
#ifdef DEBUG
		else
			PMD(PMD_NAMETODIP, ("dev_get_dev_info (x) failed\n",
			    vp->v_rdev))
#endif
		ddi_rele_driver(major);
	} else {
		PMD(PMD_NAMETODIP, ("hold of %x failed\n", major))
		return (NULL);
	}
	VN_RELE(vp);
	return (dip);
}

static dev_info_t *
find_dip(dev_info_t *dip, char *dev_name, int full_search)
{
	dev_info_t	*ndip;
	char		*child_dev, *addr;
	char		*device;	/* writeable copy of path */
	int		dev_len = strlen(dev_name)+1;

	device = kmem_zalloc(dev_len, KM_SLEEP);
	(void) strcpy(device, dev_name);
	addr = strchr(device, '@');
	child_dev = strchr(device, '/');
	if ((addr != NULL) && (child_dev == NULL || addr < child_dev)) {
		/*
		 * We have device = "name@addr..." form
		 */
		*addr++ = '\0';			/* for strcmp (and skip '@') */
		if (child_dev != NULL)
			*child_dev++ = '\0';	/* for strcmp (and skip '/') */
	} else {
		/*
		 * We have device = "name/..." or "name"
		 */
		addr = "";
		if (child_dev != NULL)
			*child_dev++ = '\0';	/* for strcmp (and skip '/') */
	}
	for (; dip != NULL; dip = ddi_get_next_sibling(dip)) {
		if (strcmp(ddi_node_name(dip), device) == 0) {
			/* If the driver isn't loaded, we prune the search */
			if (!ddi_get_name_addr(dip) || !DDI_CF2(dip) ||
			    DDI_DRV_UNLOADED(dip)) {
				continue;
			}
			if (strcmp(ddi_get_name_addr(dip), addr) == 0) {
				PMD(PMD_NAMETODIP, ("fd: matched %s@%s\n",
				    ddi_node_name(dip), PM_ADDR(dip)))
				if (child_dev != NULL)
					dip = find_dip(ddi_get_child(dip),
					    child_dev, 0);
				kmem_free(device, dev_len);
				return (dip);
			}
		}
		if (full_search) {
			ndip = find_dip(ddi_get_child(dip), dev_name, 1);
			if (ndip) {
				kmem_free(device, dev_len);
				return (ndip);
			}
		}
	}
	kmem_free(device, dev_len);
	return (dip);
}

/*
 * Free the dependency information for a device
 */
void
pm_free_keeps(dev_info_t *dip)
{
	int doprkeeps = 0;
	pm_info_t *info = PM_GET_PM_INFO(dip);
#ifdef DEBUG
	void		prkeeps(dev_info_t *);
	ASSERT(info);

	if ((pm_debug & PMD_KEEPS) &&
	    (info->pmi_nwekeepup || info->pmi_nkeptupby)) {
		PMD(PMD_KEEPS, ("pm_rem_info before: "))
		doprkeeps = 1;
		prkeeps(dip);
	}
#endif
	if (info->pmi_nwekeepup) {
		/*
		 * Remove the reference to this device from each
		 * device we keep up
		 */
		pm_unkeeps(info->pmi_nwekeepup, dip, info->pmi_wekeepdips);
		kmem_free(info->pmi_wekeepdips,
		    info->pmi_nwekeepup * sizeof (dev_info_t *));
		info->pmi_wekeepdips = NULL;
		info->pmi_nwekeepup = 0;
	}
	if (info->pmi_nkeptupby) {
		pm_unkepts(info->pmi_nkeptupby, dip, info->pmi_kupbydips);
		kmem_free(info->pmi_kupbydips, info->pmi_nkeptupby *
		    sizeof (dev_info_t *));
		info->pmi_kupbydips = NULL;
		info->pmi_nkeptupby = 0;
	}
#ifdef	DEBUG
	if (doprkeeps) {
		PMD(PMD_KEEPS, ("pm_rem_info after: "))
		prkeeps(dip);
	}
#endif
}


/*
 * Release the pm resource for this device. If a device is still registered
 * for direct-pm, only the auto-pm specific space will be released, unless
 * forceflag is PM_REM_FORCE, in which case the unit is being detached and
 * so should be completely unmanaged.
 * Returns an errno value.
 */
int
pm_rem_info(dev_info_t *dip, int held)
{
	int		i, found = 0;
	pm_info_t	*pinfo, *info = PM_GET_PM_INFO(dip);
	dev_info_t 	*pdip;

	ASSERT(info);
	ASSERT(PM_IAM_LOCKING_DIP(dip));
	if (!held) {
		rw_enter(&pm_scan_list_rwlock, RW_WRITER);
	}
	found = pm_dequeue_scan(dip);
	ASSERT(found || (info->pmi_dev_pm_state & PM_SCAN) == 0);
	/*
	 * Now adjust parent's pmi_kidsupcnt.  BC nodes we check only comp 0,
	 * Others we check all components.  BC node that has already
	 * called pm_destroy_components() has zero component count.
	 * Parents that get notification are not adjusted because their
	 * kidsupcnt is always 0.
	 */
	if (PM_NUMCMPTS(dip) && ((pdip = ddi_get_parent(dip)) != NULL) &&
	    ((pinfo = PM_GET_PM_INFO(pdip)) != NULL) &&
	    !(DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY)) {
		int incr = 0;
		PMD(PMD_KIDSUP, ("pm_rem_info %s@%s has %d components\n",
		    PM_NAME(dip), PM_ADDR(dip),
		    DEVI(dip)->devi_pm_num_components))

		if (PM_ISBC(dip)) {
			incr = (PM_CURPOWER(dip, 0) != 0);
		} else {
			for (i = 0;
			    i < DEVI(dip)->devi_pm_num_components; i++) {
				incr += (PM_CURPOWER(dip, i) != 0);
			}
		}
		PM_LOCK_DIP(pdip);
		PMD(PMD_KIDSUP, ("pm_rem_info %s@%s kuc %d to %d because "
		    "%s@%s (%s) being unmanaged\n", PM_NAME(pdip),
		    PM_ADDR(pdip), pinfo->pmi_kidsupcnt,
		    pinfo->pmi_kidsupcnt - incr, PM_NAME(dip),
		    PM_ADDR(dip), (PM_ISBC(dip) ? "BC" : "not BC")))
		if (incr) {
			pinfo->pmi_kidsupcnt -= incr;
			ASSERT(pinfo->pmi_kidsupcnt >= 0);
			if (pinfo->pmi_kidsupcnt == 0)
				pm_rescan(1);
		}
		PM_UNLOCK_DIP(pdip);
	}
	pm_free_keeps(dip);

	if (!held)
		rw_exit(&pm_scan_list_rwlock);
	/*
	 * If the device is also a direct-pm'd device, we're done.
	 */
	if (PM_ISDIRECT(dip)) {
		PMD(PMD_REMINFO, ("rem_info %s@%s direct\n", ddi_get_name(dip),
		    PM_ADDR(dip)))
		return (0);
	}
	/*
	 * We are here because the device is not directly power managed.
	 * Release *all* its pm resources.
	 *
	 * Once we clear the info pointer, it looks like it is not power
	 * managed to everybody else.
	 */
	pm_set_pm_info(dip, NULL);
	kmem_free(info, sizeof (pm_info_t));
	pm_devices--;
	return (0);
}

static boolean_t
pm_cpr_callb(void *arg, int code)
{
	_NOTE(ARGUNUSED(arg))
	static int bc_save, auto_save;

	switch (code) {
	case CB_CODE_CPR_CHKPT:
		/*
		 * Cancel scan or wait for scan in progress to finish
		 * Other threads may be trying to restart the scan, so we
		 * have to keep at it unil it sticks
		 */
		mutex_enter(&pm_scan_lock);
		bc_save = bcpm_enabled;
		bcpm_enabled = 0;
		auto_save = autopm_enabled;
		autopm_enabled = 0;
		while (pm_scan_id) {
			timeout_id_t scanid;

			scanid = pm_scan_id;
			pm_scan_id = 0;
			mutex_exit(&pm_scan_lock);
			(void) untimeout(scanid);
			mutex_enter(&pm_scan_lock);
		}
		mutex_exit(&pm_scan_lock);
		break;

	case CB_CODE_CPR_RESUME:
		ASSERT(pm_scan_id == 0);	/* scan stopped at checkpoint */
		ASSERT(!bcpm_enabled && !autopm_enabled);
		/*
		 * Call pm_reset_timestamps to reset timestamps of each
		 * device to the time when the system is resumed so that their
		 * idleness can be re-calculated. That's to avoid devices from
		 * being powered down right after resume if the system was in
		 * suspended mode long enough.
		 */
		rw_enter(&pm_scan_list_rwlock, RW_READER);
		pm_walk_devs(pm_scan_list, pm_reset_timestamps);
		rw_exit(&pm_scan_list_rwlock);

		bcpm_enabled = bc_save;
		autopm_enabled = auto_save;
		/*
		 * If there is any auto-pm device, get the scanning
		 * going. Otherwise don't bother.
		 */
		if (pm_scan_list) {
			PMD(PMD_RESCAN, ("CPR_RESUME rescan"))
			pm_rescan(1);
		}
		break;
	}
	return (B_TRUE);
}

/*
 * This callback routine is called when there is a system panic.  If the
 * console has a frame buffer device and the device is not in its normal
 * power, then power it up.
 */
static boolean_t
pm_panic_callb(void *arg, int code)
{
	_NOTE(ARGUNUSED(arg, code))
	dev_info_t	*dip = NULL;
	pm_info_t	*info;
	int		*power;
	size_t		size;
	int		i;

	if (prom_stdout_is_framebuffer() && (fbdev != NODEV))
		dip = dev_get_dev_info(fbdev, 0);

	if (!dip)
		return (B_TRUE);

	/* we're panic'ing, so mutexen are useless */
	if ((info = PM_GET_PM_INFO(dip)) == NULL) {
		return (B_TRUE);
	}

	if (pm_get_norm_pwrs(dip, &power, &size) == DDI_SUCCESS) {
		PM_LOCK_DIP(dip);	/* blows through */
		for (i = 0; i < PM_NUMCMPTS(dip); i++) {
			int curpower = PM_CURPOWER(dip, i);
			if (curpower != power[i])
				(void) pm_set_power(dip, i, power[i],
				    curpower, info, 0);
		}
		PM_UNLOCK_DIP(dip);
		kmem_free(power, size);
	}
	return (B_TRUE);
}

static boolean_t
pm_halt_callb(void *arg, int code)
{
	_NOTE(ARGUNUSED(arg, code))
	return (B_TRUE);	/* XXX for now */
}

int
pm_get_norm_pwrs(dev_info_t *dip, int **valuep, size_t *length)
{
	int components = PM_NUMCMPTS(dip);
	int *bufp;
	size_t size;
	int i;

	if (components <= 0) {
		cmn_err(CE_NOTE, "!pm: %s@%s has no components, can't get "
		    "normal power values\n", PM_NAME(dip), PM_ADDR(dip));
		return (DDI_FAILURE);
	} else {
		size = components * sizeof (int);
		bufp = kmem_alloc(size, KM_SLEEP);
		for (i = 0; i < components; i++) {
			bufp[i] = pm_get_normal_power(dip, i);
		}
	}
	*length = size;
	*valuep = bufp;
	return (DDI_SUCCESS);
}

void
pm_get_timestamps(dev_info_t *dip, time_t *valuep)
{
	int components = PM_NUMCMPTS(dip);
	int i;


	ASSERT(components > 0);
	PM_LOCK_BUSY(dip);	/* so we get a consistent view */
	for (i = 0; i < components; i++) {
		valuep[i] =
		    DEVI(dip)->devi_pm_components[i].pmc_timestamp;
	}
	PM_UNLOCK_BUSY(dip);
}

static void
pm_reset_timestamps(dev_info_t *dip)
{
	int components;
	int	i;
	struct pm_component *cp;
	pm_info_t *info = PM_GET_PM_INFO(dip);

	ASSERT(info);
	PM_LOCK_DIP(dip);
	components = PM_NUMCMPTS(dip);
	ASSERT(components > 0);
	PM_LOCK_BUSY(dip);
	for (i = 0; i < components; i++) {
		/*
		 * If the component was not marked as busy,
		 * reset its timestamp to now.
		 */
		cp = &DEVI(dip)->devi_pm_components[i];
		if (cp->pmc_timestamp) {
			cp->pmc_timestamp = hrestime.tv_sec;
		}
	}
	PM_UNLOCK_BUSY(dip);
	PM_UNLOCK_DIP(dip);
}

void
pm_set_pm_info(dev_info_t *dip, void *value)
{
	DEVI(dip)->devi_pm_info = value;
}


void
pm_walk_devs(pm_scan_t *dev, void (*f)(dev_info_t *))
{
	pm_scan_t *cur, *next;

	cur = dev;
	while (cur != NULL) {
		next = cur->ps_next;
		(void) (*f)(cur->ps_dip);
		cur = next;
	}
}

/*
 * This is the default method of setting the power of a device if no ppm
 * driver has claimed it.
 */
int
pm_power(dev_info_t *dip, int comp, int level)
{
	struct dev_ops	*ops;
	int		(*fn)(dev_info_t *, int, int);
	struct pm_component *cp = &DEVI(dip)->devi_pm_components[comp];

	PMD(PMD_KIDSUP, ("pm_power(%s@%s, %d, %d)\n", PM_NAME(dip),
	    PM_ADDR(dip), comp, level))
	if (!(ops = ddi_get_driver(dip))) {
		PMD(PMD_FAIL, ("pm_power no ops\n"))
		return (DDI_FAILURE);
	}
	if ((ops->devo_rev < 2) || !(fn = ops->devo_power)) {
		PMD(PMD_FAIL, ("pm_power%s%s\n",
		    (ops->devo_rev < 2 ? " wrong devo_rev" : ""),
		    (!fn ? " devo_power NULL" : "")))
		return (DDI_FAILURE);
	}

	if ((*fn)(dip, comp, level) == DDI_SUCCESS) {
		e_pm_set_cur_pwr(dip, &DEVI(dip)->devi_pm_components[comp],
		    level);
		return (DDI_SUCCESS);
	}
	PMD(PMD_FAIL, ("pm_power: Can't set comp %d (%s) of %s@%s to level %d "
	    "(%s).\n", comp, cp->pmc_comp.pmc_name, ddi_node_name(dip),
	    PM_ADDR(dip), level, power_val_to_string(cp, level)));
	return (DDI_FAILURE);
}

int
pm_unmanage(dev_info_t *dip, int held)
{
	power_req_t power_req;
	int result, retval = 0;

	PMD(PMD_REMDEV | PMD_KIDSUP, ("pm_unmanage %s@%s\n",
	    PM_NAME(dip), PM_ADDR(dip)))
	power_req.request_type = PMR_PPM_DETACH;
	if ((dev_info_t *)DEVI(dip)->devi_pm_ppm != NULL)
		retval = pm_ctlops((dev_info_t *)DEVI(dip)->devi_pm_ppm, dip,
		    DDI_CTLOPS_POWER, &power_req, &result);
	ASSERT(retval == DDI_SUCCESS);
	(void) pm_rem_info(dip, held);
	return (retval);
}

int
pm_raise_power(dev_info_t *dip, int comp, int level)
{
	if (level < 0)
		return (DDI_FAILURE);
	return (dev_is_needed(dip, comp, level, PM_LEVEL_UPONLY));
}

int
pm_lower_power(dev_info_t *dip, int comp, int level)
{
	pm_info_t	*info;
	struct pm_component *cp;
	int ret;
	int e_pm_manage(dev_info_t *, int);

	if (level < 0 || !DEVI_IS_DETACHING(dip))
		return (DDI_FAILURE);

	/*
	 * If we don't care about saving power, then this is a no-op
	 */
	if (!autopm_enabled)
		return (DDI_SUCCESS);
	/*
	 * Check if the power manager is active and if the
	 * device is power managed if not, the device must be on.
	 * To make the common case (device is power managed already)
	 * fast, we check without the lock.  If device is not already
	 * power managed, then we take the lock and the long route through
	 * go get it managed.  Devices never go unmanaged until they
	 * detach.
	 */
	info = PM_GET_PM_INFO(dip);
	if (!info) {
		if (!DEVI_IS_ATTACHING(dip)) {
			return (DDI_FAILURE);
		}
		/* now check again under the lock */
		PM_LOCK_DIP(dip);
		info = PM_GET_PM_INFO(dip);
		if (!info) {
			ret = e_pm_manage(dip, 2);
			if (ret != DDI_SUCCESS) {
				PM_UNLOCK_DIP(dip);
				return (DDI_FAILURE);
			} else {
				info = PM_GET_PM_INFO(dip);
				ASSERT(info);
			}
		}
		PM_UNLOCK_DIP(dip);
	}
	if (comp < 0 || comp >= DEVI(dip)->devi_pm_num_components) {
		return (DDI_FAILURE);
	}
	cp = &DEVI(dip)->devi_pm_components[comp];

	if (!e_pm_valid_power(cp, level)) {
		return (DDI_FAILURE);
	}
	return (dev_is_needed(dip, comp, level,
	    PM_CURPOWER(dip, comp)));
}

int
pm_power_has_changed(dev_info_t *dip, int comp, int level)
{
	pm_info_t *info;
	power_req_t power_req;
	int ret;
	int e_pm_manage(dev_info_t *, int);
	dev_info_t *pdip = ddi_get_parent(dip);
	pm_info_t *pinfo;
	int result;
	int old_level;
	static int e_ddi_haschanged(dev_info_t *, int, int, int, int);

	if (level < 0) {
		PMD(PMD_FAIL, ("pm_power_has_changed: bad level %d\n", level))
		return (DDI_FAILURE);
	}
	PMD(PMD_KIDSUP, ("pm_powr_has_changed(%s@%s, %d, %d)\n",
	    PM_NAME(dip), PM_ADDR(dip), comp, level))
	/*
	 * In order to not slow the common case (info already set) we check
	 * first without taking the lock (which we only need to hold if we are
	 * going to put the device into power management because we were
	 * called from the device's attach routine).
	 */
	info = PM_GET_PM_INFO(dip);
	if (!info) {
		PM_LOCK_DIP(dip);
		if (!DEVI_IS_ATTACHING(dip)) {
			PM_UNLOCK_DIP(dip);
			PMD(PMD_FAIL, ("pm_power_has_changed: no info, not "
			    "attaching\n"))
			return (DDI_FAILURE);
		}
		/* now check again under the lock */
		info = PM_GET_PM_INFO(dip);
		if (!info) {
			ret = e_pm_manage(dip, 2);
			PM_UNLOCK_DIP(dip);
			if (ret != DDI_SUCCESS) {
				PMD(PMD_FAIL, ("pm_power_has_changed: no info "
				    "after e_pm_manage\n"))
				return (DDI_FAILURE);
			} else {
				info = PM_GET_PM_INFO(dip);
				ASSERT(info);
			}
		} else {
			PM_UNLOCK_DIP(dip);
		}
	}
	if (comp < 0 || comp >= DEVI(dip)->devi_pm_num_components) {
		PMD(PMD_FAIL, ("pm_power_has_changed: comp %d out of range "
		    "%d\n",  comp, DEVI(dip)->devi_pm_num_components))
		return (DDI_FAILURE);
	}

	if (!e_pm_valid_power(&DEVI(dip)->devi_pm_components[comp], level)) {
		PMD(PMD_FAIL, ("pm_power_has_changed: level %d not valid for "
		    "component %d of %s@%s\n", level, comp, PM_NAME(dip),
		    PM_ADDR(dip)))
		return (DDI_FAILURE);
	}

	/*
	 * Tell ppm about this.
	 */
	power_req.request_type = PMR_PPM_POWER_CHANGE_NOTIFY;
	power_req.req.ppm_notify_level_req.who = dip;
	power_req.req.ppm_notify_level_req.cmpt = comp;
	power_req.req.ppm_notify_level_req.new_level = level;
	if (pm_ctlops((dev_info_t *)DEVI(dip)->devi_pm_ppm, dip,
	    DDI_CTLOPS_POWER, &power_req, &result) == DDI_FAILURE) {
		PMD(PMD_FAIL, ("pm_power_has_changed: pm_ctlops failed\n",
		    level))
		return (DDI_FAILURE);
	}
	PM_LOCK_DIP(dip);
	old_level = PM_CURPOWER(dip, comp);
	/*
	 * For bc nodes: if component 0 goes on adjust kidsupcnt of
	 * parent.  For others, if any component goes on adjust it.
	 * Parents that get notification are left with kidsupcnt always 0.
	 * We have to process count beforehand if incrementing, afterwards
	 * if decrementing.
	 */
	if (pdip && ((pinfo = PM_GET_PM_INFO(pdip)) != NULL) &&
	    !(DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY) &&
	    (!PM_ISBC(dip) || comp == 0) && level != 0 && old_level == 0) {
		PM_LOCK_DIP(pdip);
		PMD(PMD_KIDSUP, ("pm_p_has kuc of %s@%s %d to %d "
		    "because %s@%s comp %d %d->%d\n",
		    PM_NAME(pdip), PM_ADDR(pdip), pinfo->pmi_kidsupcnt,
		    pinfo->pmi_kidsupcnt + 1, PM_NAME(dip),
		    PM_ADDR(dip), comp, old_level, level))
		pinfo->pmi_kidsupcnt++;
		PM_UNLOCK_DIP(pdip);
	}
	e_pm_set_cur_pwr(dip, &DEVI(dip)->devi_pm_components[comp], level);
	if (pdip != NULL && (DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY)) {
		(void) e_ddi_haschanged(dip, comp, level, old_level, result);
	}
	/*
	 * For bc nodes: if component 0 goes off adjust kidsupcnt of
	 * parent.  For others, if any component goes off adjust it.
	 */
	if (pdip && pinfo && !(DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY) &&
	    (!PM_ISBC(dip) || comp == 0) && level == 0 && old_level != 0) {
		PM_LOCK_DIP(pdip);
		PMD(PMD_KIDSUP, ("pm_p_has kuc of %s@%s %d to %d "
		    "because %s@%s comp %d %d->%d\n",
		    PM_NAME(pdip), PM_ADDR(pdip), pinfo->pmi_kidsupcnt,
		    pinfo->pmi_kidsupcnt - 1, PM_NAME(dip),
		    PM_ADDR(dip), comp, old_level, level))
		pinfo->pmi_kidsupcnt--;
		ASSERT(pinfo->pmi_kidsupcnt >= 0);
		if (pinfo->pmi_kidsupcnt == 0)
			pm_rescan(1);
		PM_UNLOCK_DIP(pdip);
	}
	/*
	 * No inter-component dependencies any more except for old devices
	 */
	if (PM_ISBC(dip) && comp == 0 && old_level == 0 && level != 0) {
		ASSERT(DEVI(dip)->devi_pm_flags & PMC_SUSPENDED);
		if (devi_attach(dip, DDI_PM_RESUME) != DDI_SUCCESS)
			/* XXX Should we mark it resumed, */
			/* even though it failed? */
			cmn_err(CE_WARN, "!pm: Can't resume %s@%s",
			    ddi_node_name(dip), PM_ADDR(dip));
		DEVI(dip)->devi_pm_flags &= ~PMC_SUSPENDED;
	}
	ASSERT(PM_IAM_LOCKING_DIP(dip));
	PM_UNLOCK_DIP(dip);
	if (pm_watchers()) {
		mutex_enter(&pm_rsvp_lock);
		pm_enqueue_notify(PSC_HAS_CHANGED, dip, comp, level, old_level);
		mutex_exit(&pm_rsvp_lock);
	}
	PMD(PMD_RESCAN, ("pm_power_has_changed: rescan\n"))
	pm_rescan(0);
	return (DDI_SUCCESS);
}


/*
 * This function is called at startup time to notify pm of the existence
 * of any platform power managers for this platform.  As a result of
 * this registation, each function provided will be called each time
 * a device node is attached, until one returns true, and it must claim the
 * device node (by returning non-zero) if it wants to be involved in the
 * node's power management.  If it does claim the node, then it will
 * subsequently be notified of attach and detach events.
 *
 */

#define	MAX_PPM_HANDLERS	4

kmutex_t ppm_lock;	/* in case we ever do multi-threaded startup */

struct	ppm_callbacks {
	int (*ppmc_func)(dev_info_t);
	dev_info_t	*ppmc_dip;
} ppm_callbacks[MAX_PPM_HANDLERS + 1];

int
pm_register_ppm(int (*func)(dev_info_t), dev_info_t *dip)
{
	struct ppm_callbacks *ppmcp;
	int i;

	mutex_enter(&ppm_lock);
	ppmcp = ppm_callbacks;
	for (i = 0; i < MAX_PPM_HANDLERS; i++, ppmcp++) {
		if (ppmcp->ppmc_func == NULL) {
			ppmcp->ppmc_func = func;
			ppmcp->ppmc_dip = dip;
			break;
		}
	}
	mutex_exit(&ppm_lock);

	if (i >= MAX_PPM_HANDLERS)
		return (DDI_FAILURE);
	return (DDI_SUCCESS);
}

/*
 * Call the ppm's that have registered and adjust the devinfo struct as
 * appropriate.  Return's true if device is claimed by a ppm.
 * First one to claim it gets it.  The sets of devices claimed by each ppm
 * are assumed to be disjoint.
 */
int
pm_ppm_claimed(dev_info_t dip)
{
	struct ppm_callbacks *ppmcp;

	mutex_enter(&ppm_lock);
	for (ppmcp = ppm_callbacks; ppmcp->ppmc_func; ppmcp++) {
		if ((*ppmcp->ppmc_func)(dip)) {
			DEVI(dip)->devi_pm_ppm =
			    (struct dev_info *)ppmcp->ppmc_dip;
			mutex_exit(&ppm_lock);
			return (1);
		}
	}
	mutex_exit(&ppm_lock);
	/*
	 * No ppm driver claimed it
	 */
	DEVI(dip)->devi_pm_ppm = NULL;	/* handled by default code */
	return (0);
}

/*
 * Node is being detached so stop autopm until we see if it succeds, in which
 * case pm_stop will be called.  For backwards compatible devices we bring the
 * device up to full power on the assumption the detach will succeed.
 */
void
pm_detaching(dev_info_t *dip)
{
	int pm_all_to_normal(dev_info_t *, int, int);
	pm_info_t *info = PM_GET_PM_INFO(dip);
	int was_scanning = 0;

	PMD(PMD_REMDEV, ("pm_detaching %s@%s info %p, %d components\n",
	    PM_NAME(dip), PM_ADDR(dip), (void *)info, PM_NUMCMPTS(dip)))
	if (info == NULL)
		return;
	ASSERT(DEVI_IS_DETACHING(dip));
	rw_enter(&pm_scan_list_rwlock, RW_WRITER);	/* pmi_dev_pm_state */
	if (info->pmi_dev_pm_state & PM_SCAN) {
		info->pmi_dev_pm_state &= ~PM_SCAN;
		info->pmi_dev_pm_state |= PM_DETACHING;
		if (PM_ISBC(dip)) {
			was_scanning = 1;
			info->pmi_dev_pm_state |= PM_SCAN_DEFERRED;
		}
	}
	rw_exit(&pm_scan_list_rwlock);
	if (was_scanning) {
		mutex_enter(&pm_scan_lock);
		ASSERT(bcpm_enabled);
		bcpm_enabled--;
		mutex_exit(&pm_scan_lock);
	}
	if (PM_ISBC(dip)) {
		PM_LOCK_POWER(dip, PMC_ALLCOMPS);
		(void) pm_all_to_normal(dip, 0, 0);
		/* pm_all_to_normal drops lock */
	}
}

/*
 * Node failed to detach.  If it used to be autopm'd, make it so again.
 * The device should still be idle and go down soon enough if it was a
 * backwards compatible device.
 */
void
pm_detach_failed(dev_info_t *dip)
{
	pm_info_t *info = PM_GET_PM_INFO(dip);
	int was_deferred = 0;

	if (info == NULL)
		return;
	rw_enter(&pm_scan_list_rwlock, RW_WRITER);
	ASSERT(DEVI_IS_DETACHING(dip));
	if (info->pmi_dev_pm_state & PM_DETACHING) {
		info->pmi_dev_pm_state &= ~PM_DETACHING;
		info->pmi_dev_pm_state |= PM_SCAN;
		if (info->pmi_dev_pm_state & PM_SCAN_DEFERRED) {
			was_deferred = 1;
			info->pmi_dev_pm_state &= ~PM_SCAN_DEFERRED;
		}
	}
	rw_exit(&pm_scan_list_rwlock);
	if (was_deferred) {
		mutex_enter(&pm_scan_lock);
		bcpm_enabled++;
		mutex_exit(&pm_scan_lock);
		pm_rescan(1);
	}
}

/* generic Backwards Compatible component */
static char *bc_names[] = {"off", "on"};

static pm_comp_t bc_comp = {"unknown", 2, NULL, NULL, &bc_names[0]};

static void
e_pm_default_levels(dev_info_t *dip, pm_component_t *cp, int norm)
{
	pm_comp_t *pmc;
	pmc = &cp->pmc_comp;
	pmc->pmc_numlevels = 2;
	pmc->pmc_lvals[0] = 0;
	pmc->pmc_lvals[1] = norm;
	e_pm_set_cur_pwr(dip, cp, norm);
}

static void
e_pm_default_components(dev_info_t *dip, int cmpts)
{
	int i;
	pm_component_t *p = DEVI(dip)->devi_pm_components;

	p = DEVI(dip)->devi_pm_components;
	for (i = 0; i < cmpts; i++, p++) {
		p->pmc_comp = bc_comp;	/* struct assignment */
		p->pmc_comp.pmc_lvals = kmem_zalloc(2 * sizeof (int),
		    KM_SLEEP);
		p->pmc_comp.pmc_thresh = kmem_alloc(2 * sizeof (int),
		    KM_SLEEP);
		p->pmc_comp.pmc_numlevels = 2;
		p->pmc_comp.pmc_thresh[0] = INT_MAX;
		p->pmc_comp.pmc_thresh[1] = INT_MAX;
	}
}

/*
 * Called from functions that require components to exist already to allow
 * for their creation by parsing the pm-components property.
 * Device will not be power managed as a result of this call
 * Backwards values mean: (XXX put these into epm.h with names)
 * 0	definitely not an old style device
 * 1	definitely is an old style device
 * 2	don't know yet
 */
int
pm_premanage(dev_info_t *dip, int backwards)
{
	pm_comp_t	*pcp, *compp;
	int		cmpts, i, norm, error;
	pm_component_t *p = DEVI(dip)->devi_pm_components;
	pm_comp_t *pm_autoconfig(dev_info_t *, int *);

	ASSERT(PM_IAM_LOCKING_DIP(dip));
	/*
	 * If this dip has already been processed, don't mess with it
	 */
	if (DEVI(dip)->devi_pm_flags & PMC_COMPONENTS_DONE)
		return (DDI_SUCCESS);
	if (DEVI(dip)->devi_pm_flags & PMC_COMPONENTS_FAILED) {
		PMD(PMD_FAIL, ("pm_premanage %s@%s PMC_COMPONENTS_FAILED\n",
		    PM_NAME(dip), PM_ADDR(dip)))
		return (DDI_FAILURE);
	}
	/*
	 * Look up pm-components property and create components accordingly
	 * If that fails, fall back to backwards compatibility
	 */
	if ((compp = pm_autoconfig(dip, &error)) == NULL) {
		/*
		 * If error is set, the property existed but was not well formed
		 */
		if (error || (backwards == 0)) {
			DEVI(dip)->devi_pm_flags |= PMC_COMPONENTS_FAILED;
			PMD(PMD_ERROR, ("pm_autconfig fails !backwards %s@%s\n",
			    PM_NAME(dip), PM_ADDR(dip)))
			return (DDI_FAILURE);
		}
		/*
		 * If they don't have the pm-components property, then we
		 * want the old "no pm until PM_SET_DEVICE_THRESHOLDS ioctl"
		 * behavior driver must have called pm_create_components, and
		 * we need to flesh out dummy components
		 */
		if ((cmpts = PM_NUMCMPTS(dip)) == 0) {
			if (backwards == 1) {	/* must know normal power */
				PMD(PMD_ERROR, ("pm_premanage: %s@%s: backwards"
				    " but no components\n",
				    PM_NAME(dip), PM_ADDR(dip)))
				return (EINVAL);
			} else {
				/*
				 * Not really failure, but we don't want the
				 * caller to treat it as success
				 */
				return (DDI_FAILURE);
			}
		}
		DEVI(dip)->devi_pm_flags |= PMC_BC;
		e_pm_default_components(dip, cmpts);
		for (i = 0; i < cmpts; i++) {
			/*
			 * if normal power not set yet, we don't really know
			 * what *ANY* of the power values are.  If normal
			 * power is set, then we assume for this backwards
			 * compatible case that the values are 0, normal power.
			 */
			norm = pm_get_normal_power(dip, i);
			if (norm == (uint_t)-1) {
				PMD(PMD_ERROR, ("pm_manage, no normal power "
				    "for %s@%s[%d]\n", PM_NAME(dip),
				    PM_ADDR(dip), i))
				return (DDI_FAILURE);
			}
			e_pm_default_levels(dip,
			    &DEVI(dip)->devi_pm_components[i], norm);
		}
	} else {
		/*
		 * e_pm_create_components was called from pm_autoconfig(), it
		 * creates components with no descriptions (or known levels)
		 */
		cmpts = PM_NUMCMPTS(dip);
		ASSERT(cmpts != 0);
		pcp = compp;
		p = DEVI(dip)->devi_pm_components;
		for (i = 0; i < cmpts; i++, p++) {
			p->pmc_comp = *pcp++;   /* struct assignment */
			e_pm_set_cur_pwr(dip, &DEVI(dip)->devi_pm_components[i],
			    PM_LEVEL_UNKNOWN);
		}
		pm_set_device_threshold(dip, cmpts,
		    pm_system_idle_threshold, PMC_DEF_THRESH);
		kmem_free(compp, cmpts * sizeof (pm_comp_t));
	}
	return (DDI_SUCCESS);
}

/*
 * Called from during or after the device's attach to let us know it is ready
 * to play autopm.   Look up the pm model and manage the device accordingly.
 * Also called when old-style set threshold ioctl is being processed.
 * Returns system call errno value.
 * If DDI_ATTACH and DDI_DETACH were in same namespace, this would be
 * a little cleaner
 * Backwards values mean: (XXX put these into epm.h with names)
 * 0	definitely not an old style device
 * 1	definitely is an old style device
 * 2	don't know yet
 */
int
pm_manage(dev_info_t *dip, int backwards)
{
	int		ret;
	int		e_pm_manage(dev_info_t *, int);

	PM_LOCK_DIP(dip);
	/*
	 * If this dip has already been processed, don't mess with it
	 */
	if (PM_GET_PM_INFO(dip)) {
		PM_UNLOCK_DIP(dip);
		return (0);
	}
	PMD(PMD_REMDEV, ("pm_manage(%s@%s, %d)\n", PM_NAME(dip),
	    PM_ADDR(dip), backwards))
	ret = e_pm_manage(dip, backwards);
	PM_UNLOCK_DIP(dip);
	PMD(PMD_REMDEV, ("pm_manage %s@%s returns %d\n", PM_NAME(dip),
	    PM_ADDR(dip), ret))
	return (ret);
}


int
e_pm_manage(dev_info_t *dip, int backwards)
{
	pm_info_t	*info, *pinfo;
	int		result;
	power_req_t	power_req;
	int	pm_thresh_specd(dev_info_t *, int);
	int	pm_dep_specd(dev_info_t *, int);
	dev_info_t 	*pdip;

	ASSERT(PM_IAM_LOCKING_DIP(dip));
	if (pm_premanage(dip, backwards) != DDI_SUCCESS) {
		return (EINVAL);
	}
	PMD(PMD_KIDSUP, ("e_pm_manage(%s@%s)\n", PM_NAME(dip), PM_ADDR(dip)))

	/* start code taken from add_info */
	ASSERT(PM_GET_PM_INFO(dip) == NULL);
	info = kmem_zalloc(sizeof (pm_info_t), KM_SLEEP);
	/*
	 * Now set up parent's pmi_kidsupcnt.  BC nodes are assumed to start
	 * out at their normal power, so they are "up", others start out
	 * unknown, which is effectively "up".  Parent which want notification
	 * get kidsupcnt of 0 always.
	 */
	if (((pdip = ddi_get_parent(dip)) != NULL) &&
	    ((pinfo = PM_GET_PM_INFO(pdip)) != NULL) &&
	    !(DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY)) {
		int incr;
		ASSERT(pinfo->pmi_kidsupcnt >= 0);
		if (PM_ISBC(dip))
			incr = 1;
		else
			incr = PM_NUMCMPTS(dip);
		PM_LOCK_DIP(pdip);
		PMD(PMD_KIDSUP, ("e_pm_man %s@%s %s kuc %d to %d because %s@%s "
		    "being managed (%d comps)\n", PM_NAME(pdip), PM_ADDR(pdip),
		    (PM_ISBC(dip) ? "BC" : "not BC"), pinfo->pmi_kidsupcnt,
		    pinfo->pmi_kidsupcnt + incr, PM_NAME(dip),
		    PM_ADDR(dip), PM_NUMCMPTS(dip)))
		pinfo->pmi_kidsupcnt += incr;
		PM_UNLOCK_DIP(pdip);
	}
	pm_set_pm_info(dip, info);
	/*
	 * Apply any recorded thresholds
	 */
	(void) pm_thresh_specd(dip, 1);

	rw_enter(&pm_dep_rwlock, RW_READER);
	rw_enter(&pm_scan_list_rwlock, RW_WRITER);
	(void) pm_dep_specd(dip, 1);
	rw_exit(&pm_scan_list_rwlock);
	rw_exit(&pm_dep_rwlock);

	power_req.request_type = PMR_PPM_ATTACH;
	if (pm_ppm_claimed(dip)) {	/* if ppm driver claims the node */
		ASSERT(DEVI(dip)->devi_pm_ppm != NULL);
		(void) pm_ctlops((dev_info_t *)DEVI(dip)->devi_pm_ppm, dip,
		    DDI_CTLOPS_POWER, &power_req, &result);
	}
#ifdef DEBUG
	else {
		ASSERT(DEVI(dip)->devi_pm_ppm == NULL);
	}
#endif
	/*
	 * If not a backwards-compatible node start doing pm on it
	 */
	if (!PM_ISBC(dip)) {
		rw_enter(&pm_scan_list_rwlock, RW_WRITER);
		info->pmi_dev_pm_state |= PM_SCAN;
		result = pm_enqueue_scan(dip);
		rw_exit(&pm_scan_list_rwlock);
		ASSERT(result == 0);	/* not already on queue */
		PMD(PMD_RESCAN, ("e_pm_manage() rescan\n"))
		pm_rescan(0);
		pm_devices++;
	}
	return (0);
}

/*
 * enqueue a pm_scan struct that points to the given dip on the pm_scan_list
 */
int
pm_enqueue_scan(dev_info_t *dip)
{
	int found = 0;
	pm_scan_t	*prev_dev, *cur_dev, *new_dev;
	char pathbuf[MAXPATHLEN];
	char *path;

	PMD(PMD_REMDEV, ("pm_enqueue_scan %s@%s ", PM_NAME(dip), PM_ADDR(dip)))

	ASSERT(!mutex_owned(&pm_scan_lock));
	/*
	 * Add the device to the end of our scan list if
	 * it's not already there.
	 */
	found = 0;
	prev_dev = NULL;
	for (cur_dev = pm_scan_list; cur_dev; cur_dev = cur_dev->ps_next) {
		if (cur_dev->ps_dip == dip) {
			found = 1;
			break;
		}
		prev_dev = cur_dev;
	}
	if (!found) {
		size_t size;
		path = ddi_pathname(dip, pathbuf);
		size = sizeof (pm_scan_t) + strlen(path) + 1;
		new_dev = kmem_zalloc(size, KM_SLEEP);
		new_dev->ps_size = size;
		new_dev->ps_dip = dip;
		new_dev->ps_next = NULL;
		new_dev->ps_path = (char *)((intptr_t)new_dev +
		    sizeof (pm_scan_t));
		(void) strcpy(new_dev->ps_path, path);
		if (pm_scan_list)
			prev_dev->ps_next = new_dev;
		else
			pm_scan_list = new_dev;
	}
	PMD(PMD_REMDEV, ("found %d\n", found))
	return (found);
}

static int
pm_dequeue_scan(dev_info_t *dip)
{
	pm_scan_t *cur_dev, *last_dev;
	int found = 0;

	PMD(PMD_REMDEV, ("pm_dequeue_scan %s@%s ", PM_NAME(dip), PM_ADDR(dip)))
	cur_dev = pm_scan_list;
	last_dev = cur_dev;
	while (cur_dev != NULL) {
		if (dip == cur_dev->ps_dip) {
			found = 1;
			break;
		}
		last_dev = cur_dev;
		cur_dev = cur_dev->ps_next;
	}

	if (found) {
		if (pm_scan_list == cur_dev) {
			pm_scan_list = cur_dev->ps_next;
		} else {
			last_dev->ps_next = cur_dev->ps_next;
		}
		kmem_free(cur_dev, cur_dev->ps_size);
	}
	PMD(PMD_REMDEV, ("found %d\n", found))
	return (found);
}

/*
 * This is the obsolete exported interface for a driver to find out its
 * "normal" (max) power.
 * We only get components destroyed while no power management is
 * going on (and the device is detached), so we don't need a mutex here
 */
int
pm_get_normal_power(dev_info_t *dip, int comp)
{

	if (comp >= 0 && comp < DEVI(dip)->devi_pm_num_components) {
		PMD(PMD_LEVEL | PMD_NORM, ("get_norm of %s@%s[%d] yields %x\n",
		    PM_NAME(dip), PM_ADDR(dip), comp,
		    DEVI(dip)->devi_pm_components[comp].pmc_norm_pwr))
		return (DEVI(dip)->devi_pm_components[comp].pmc_norm_pwr);
	}
	return (DDI_FAILURE);
}

/*
 * Fetches the current power level.  Return DDI_SUCCESS or DDI_FAILURE.
 */
int
pm_get_current_power(dev_info_t *dip, int comp, int *levelp)
{
	if (comp >= 0 &&
	    comp < DEVI(dip)->devi_pm_num_components) {
		*levelp = PM_CURPOWER(dip, comp);
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*
 * Internal function to set current power level
 */
static void
e_pm_set_cur_pwr(dev_info_t *dip, pm_component_t *cp, int level)
{
	int i;
	int limit = cp->pmc_comp.pmc_numlevels;
	int *ip = cp->pmc_comp.pmc_lvals;

	if (level == PM_LEVEL_UNKNOWN) {
		cp->pmc_cur_pwr = level;
		return;
	}

	for (i = 0; i < limit; i++) {
		if (level == *ip++) {
			PMD(PMD_LEVEL, ("set_cur: NORM %s@%s[%d] to %x\n",
			    PM_NAME(dip), PM_ADDR(dip),
			    cp - DEVI(dip)->devi_pm_components, level))
			cp->pmc_cur_pwr = i;
			return;
		}
	}
	cmn_err(CE_PANIC, "pm_set_cur_pwr: level %d not found for device "
	    "%s@%s\n", level, PM_NAME(dip), PM_ADDR(dip));
	/*NOTREACHED*/
}

/*
 * Returns current threshold of indicated component
 */
static int
cur_threshold(dev_info_t *dip, int comp)
{
	extern int pm_system_idle_threshold;
	pm_component_t *cp = &DEVI(dip)->devi_pm_components[comp];
	int pwr;

	if (PM_ISBC(dip)) {
		/*
		 * backwards compatible nodes only have one threshold
		 */
		return (cp->pmc_comp.pmc_thresh[1]);
	}
	pwr = cp->pmc_cur_pwr;
	if (pwr == PM_LEVEL_UNKNOWN) {
		int thresh;
		if (DEVI(dip)->devi_pm_flags & PMC_NEXDEF_THRESH)
			thresh = pm_default_nexus_threshold;
		else
			thresh = pm_system_idle_threshold;
		return (thresh);
	}
	ASSERT(cp->pmc_comp.pmc_thresh);
	return (cp->pmc_comp.pmc_thresh[pwr]);
}

/*
 * Compute next power level down for idle component
 */
static int
pm_next_lower_power(pm_component_t *cp)
{
	int pwr = cp->pmc_cur_pwr;
	int nxt_pwr;

	if (pwr == PM_LEVEL_UNKNOWN) {
		nxt_pwr = cp->pmc_comp.pmc_lvals[0];
	} else {
		pwr -= 1;
		ASSERT(pwr >= 0);
		nxt_pwr = cp->pmc_comp.pmc_lvals[pwr];
	}
	PMD(PMD_LEVEL, ("next_pwr pwr %x\n", nxt_pwr))
	return (nxt_pwr);
}

/*
 * Bring all components of device to normal power
 * NOTE, this routine assumes PM_IAM_LOCKING_POWER(dip), and does
 * PM_UNLOCK_POWER(dip) before returning
 * If ukok (unknownok) is true, then components which are at unknown power
 * levels are left alone
 */
int
pm_all_to_normal(dev_info_t *dip, int canblock, int ukok)
{
	int		*normal;
	int		*changed;
	int		*prev;
	int		i, ncomps;
	size_t		size;
	pm_info_t	*info = PM_GET_PM_INFO(dip);
	int		somechanged = 0;
	int		changefailed = 0;

	ASSERT(PM_IAM_LOCKING_POWER(dip, PMC_DONTCARE));
	ASSERT(info);
	PMD(PMD_ALLNORM, ("pm_all_to_normal %s@%s\n", PM_NAME(dip),
	    PM_ADDR(dip)))
	if (PM_ISDIRECT(dip)) {
		cmn_err(CE_NOTE, "!pm: could not bring %s@%s to normal power "
		    "because it is directly managed\n", PM_NAME(dip),
		    PM_ADDR(dip));
		PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
		return (DDI_FAILURE);
	}
	if (pm_get_norm_pwrs(dip, &normal, &size) != DDI_SUCCESS) {
		PMD(PMD_ALLNORM, (" can't get norm pwrs\n"))
		PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
		return (DDI_FAILURE);
	}
	changed = kmem_zalloc(size, KM_SLEEP);
	prev = kmem_zalloc(size, KM_SLEEP);

	ncomps = PM_NUMCMPTS(dip);
	for (i = 0; i < ncomps; i++) {
		int curpwr = PM_CURPOWER(dip, i);
		if (curpwr == PM_LEVEL_UNKNOWN && ukok)
			continue;
		if (normal[i] > curpwr) {
			if (pm_set_power(dip, i, normal[i],
			    curpwr, info, canblock) != DDI_SUCCESS) {
				changefailed++;
			} else {
				somechanged++;
				changed[i] = 1;
				prev[i] = curpwr;
			}
		}
	}
	PM_UNLOCK_POWER(dip, PMC_ALLCOMPS);
	if (somechanged && pm_watchers()) {
		mutex_enter(&pm_rsvp_lock);
		for (i = 0; i < ncomps; i++) {
			if (changed[i])
				pm_enqueue_notify(PSC_HAS_CHANGED, dip, i,
				    normal[i], prev[i]);
		}
		mutex_exit(&pm_rsvp_lock);
	}
	kmem_free(normal, size);
	kmem_free(changed, size);
	kmem_free(prev, size);
	if (changefailed) {
		PMD(PMD_FAIL, ("pm_all_to_normal: failed to set %d components "
		    "of %s@%s to full power\n", changefailed,
		    ddi_get_name(dip), PM_ADDR(dip), changefailed))
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/*
 * Returns true if all components of device are at normal power
 */
static int
all_at_normal(dev_info_t *dip, int *normp, int *curp, int *compp)
{
	int		*normal;
	int		i;
	size_t		size;
	pm_info_t	*info = PM_GET_PM_INFO(dip);

	ASSERT(info);
	PMD(PMD_ALLNORM, ("all_at_normal %s@%s\n", PM_NAME(dip), PM_ADDR(dip)))
	if (pm_get_norm_pwrs(dip, &normal, &size) != DDI_SUCCESS) {
		PMD(PMD_ALLNORM, (" can't get norm pwrs\n"))
		return (DDI_FAILURE);
	}
	for (i = 0; i < PM_NUMCMPTS(dip); i++) {
		int current = PM_CURPOWER(dip, i);
		if (normal[i] > current) {
			*compp = i;
			*normp = normal[i];
			*curp = current;
			break;
		}
	}
	kmem_free(normal, size);
	if (i != PM_NUMCMPTS(dip)) {
		return (0);
	}
	return (1);
}

/*
 * Bring up the parent of "dip" node, and any wekeepups it has
 */
static int
bring_parent_wekeeps_up(dev_info_t *dip, dev_info_t *par, pm_info_t *info,
    int level, int curpwr, int canblock)
{
	pm_info_t *par_info, *wku_info;
	int i;
	dev_info_t **wekeepup;
	int comp, norm, cur;

	PMD(PMD_BRING, ("bpwu: %s@%s, lvl %d, curpwr %d, par:%s@%s\n",
	    PM_NAME(dip), PM_ADDR(dip), level, curpwr, PM_NAME(par),
	    PM_ADDR(par)))
	if (par && (par_info = PM_GET_PM_INFO(par))) {
		PMD(PMD_BRING, ("bpwu: par %p, par_infl %p\n",
		    (void *)par, (void *)par_info))
		/*
		 * Before we block, we should see if there is anything
		 * that we have to do
		 */
		if (canblock && (par_info->pmi_dev_pm_state & PM_DIRECT_NEW) &&
		    !all_at_normal(dip, &norm, &cur, &comp)) {
			switch (pm_block(par, comp, norm, cur)) {
			case PMP_RELEASE:
				break;		/* bring it up */
			case PMP_SUCCEED:
				goto freeride;	/* consider it up */
			case PMP_FAIL:
				return (DDI_FAILURE);
			}
		}
		if (PM_ISDIRECT(par)) {
			cmn_err(CE_WARN, "!pm: parent (%s@%s) of %s@%s is "
			    "directly power managed, cannot change "
			    "power level of child.", PM_NAME(par),
			    PM_ADDR(par), PM_NAME(dip), PM_ADDR(dip));
			return (DDI_FAILURE);
		}
		/*
		 * If parent plays "parental involvement", then we don't
		 * do "parental dependency, we assume parent does
		 * the correct thing for itself
		 */
		if (!(DEVI(dip)->devi_pm_flags & PMC_NOTIFY_PARENT)) {
			PM_LOCK_POWER(par, PMC_ALLCOMPS);
			if (pm_all_to_normal(par, canblock, 0) != DDI_SUCCESS) {
				/* dropped PM_UNLOCK_POWER(par); */
				return (DDI_FAILURE);
			}
			/* pm_all_to_normal drops lock PM_UNLOCK_POWER(par); */
		}
	}
freeride:
	if (info->pmi_nwekeepup) {
		PMD(PMD_BRING, ("bpwu: pmi_nwekeepup %d\n",
		    info->pmi_nwekeepup))
		wekeepup = info->pmi_wekeepdips;
		PMD(PMD_BRING, ("bpwu: wekeepup %p\n", (void *)wekeepup))
		for (i = 0; i < info->pmi_nwekeepup; i++) {
			PMD(PMD_BRING, ("bpwu: %s@%s ",
			    PM_NAME(*wekeepup), PM_ADDR(*wekeepup)))
			wku_info = PM_GET_PM_INFO(*wekeepup);
			PMD(PMD_BRING, ("wku_info %p\n", (void *)wku_info))
			ASSERT(wku_info);
			if (canblock &&
			    (wku_info->pmi_dev_pm_state & PM_DIRECT_NEW) &&
			    !all_at_normal(*wekeepup, &norm, &cur, &comp))
				switch (pm_block(*wekeepup, comp, norm, cur)) {
				case PMP_RELEASE:
					break;		/* bring it up */
				case PMP_SUCCEED:
					continue;	/* consider it up */
				case PMP_FAIL:
					return (DDI_FAILURE);
				}
			if (PM_ISDIRECT(*wekeepup)) {
				PMD(PMD_BRING, ("pm: cannot bring %s@%s "
				    "(which depends on %s@%s) up because it "
				    " is directly power managed, so cannot "
				    "change power level of device it depends "
				    "on either.", PM_NAME(*wekeepup),
				    PM_ADDR(*wekeepup), PM_NAME(dip),
				    (PM_ADDR(dip))))
				continue;
			}
			PM_LOCK_POWER(*wekeepup, PMC_ALLCOMPS);
			(void) pm_all_to_normal(*wekeepup, canblock, 0);
			wekeepup++;
		}
	}
	return (DDI_SUCCESS);
}

/*
 * Converts a power level value to its index
 */
static int
power_val_to_index(pm_component_t *cp, int val)
{
	int limit, i, *ip;

	ASSERT(val != PM_LEVEL_UPONLY);
	/*  convert power value into index (i) */
	limit = cp->pmc_comp.pmc_numlevels;
	ip = cp->pmc_comp.pmc_lvals;
	for (i = 0; i < limit; i++)
		if (val == *ip++)
			return (i);
	return (-1);
}

/*
 * Converts a numeric power level to a printable string
 */
static char *
power_val_to_string(pm_component_t *cp, int val)
{
	int index;

	if (val == PM_LEVEL_UPONLY)
		return ("<UPONLY>");

	if (val == PM_LEVEL_UNKNOWN ||
	    (index = power_val_to_index(cp, val)) == -1)
		return ("<LEVEL_UNKNOWN>");

	return (cp->pmc_comp.pmc_lnames[index]);
}

/*
 * A bunch of stuff that belongs only to the next routine (or two)
 */

static const char namestr[] = "NAME=";
static const int nameln = sizeof (namestr) - 1;
static const char pmcompstr[] = "pm-components";

struct pm_comp_pkg {
	pm_comp_t		*comp;
	struct pm_comp_pkg	*next;
};

#define	isdigit(ch)	((ch) >= '0' && (ch) <= '9')

#define	isxdigit(ch)	(isdigit(ch) || ((ch) >= 'a' && (ch) <= 'f') || \
			((ch) >= 'A' && (ch) <= 'F'))

/*
 * Rather than duplicate this code ...
 * (this code excerpted from the function that follows it)
 */
#define	FINISH_COMP { \
	ASSERT(compp); \
	compp->pmc_lnames_sz = size; \
	tp = compp->pmc_lname_buf = kmem_alloc(size, KM_SLEEP); \
	compp->pmc_numlevels = level; \
	compp->pmc_lnames = kmem_alloc(level * sizeof (char *), KM_SLEEP); \
	compp->pmc_lvals = kmem_alloc(level * sizeof (int), KM_SLEEP); \
	compp->pmc_thresh = kmem_alloc(level * sizeof (int), KM_SLEEP); \
	/* copy string out of prop array into buffer */ \
	for (j = 0; j < level; j++) { \
		compp->pmc_thresh[j] = INT_MAX;		/* only [0] sticks */ \
		compp->pmc_lvals[j] = lvals[j]; \
		(void) strcpy(tp, lnames[j]); \
		compp->pmc_lnames[j] = tp; \
		tp += lszs[j]; \
	} \
	ASSERT(tp > compp->pmc_lname_buf && tp <= \
	    compp->pmc_lname_buf + compp->pmc_lnames_sz); \
	}

/*
 * Read the pm-components property (if there is one) and use it to set up
 * components.  Returns a pointer to an array of component structures if
 * pm-components found and successfully parsed, else returns NULL.
 * Sets error return *errp to true to indicate a failure (as opposed to no
 * property being present).
 */
pm_comp_t *
pm_autoconfig(dev_info_t *dip, int *errp)
{
	uint_t nelems;
	char **pp;
	pm_comp_t *compp = NULL;
	int i, j, level, components = 0;
	size_t size = 0;
	struct pm_comp_pkg *p, *ptail;
	struct pm_comp_pkg *phead = NULL;
	int *lvals = NULL;
	int *lszs = NULL;
	int *np = NULL;
	int npi = 0;
	char **lnames = NULL;
	char *cp, *tp;
	pm_comp_t *ret = NULL;

	*errp = 0;	/* assume success */
	if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    (char *)pmcompstr, &pp, &nelems) != DDI_PROP_SUCCESS)
		return (NULL);

	if (nelems < 3) {	/* need at least one name and two levels */
		PMD(PMD_ERROR, ("pm_autoconfig: less than 3 elements\n"))
		goto errout;
	}

	/*
	 * pm_create_components is no longer allowed
	 */
	if (PM_NUMCMPTS(dip) != 0) {
		PMD(PMD_ERROR, ("%s@%s has %d components in pm_autoconfig\n",
		    PM_NAME(dip), PM_ADDR(dip), PM_NUMCMPTS(dip)))
		goto errout;
	}

	lvals = kmem_alloc(nelems * sizeof (int), KM_SLEEP);
	lszs = kmem_alloc(nelems * sizeof (int), KM_SLEEP);
	lnames = kmem_alloc(nelems * sizeof (char *), KM_SLEEP);
	np = kmem_alloc(nelems * sizeof (int), KM_SLEEP);

	level = 0;
	phead = NULL;
	for (i = 0; i < nelems; i++) {
		cp = pp[i];
		if (!isdigit(*cp)) {	/*  must be name */
			if (strncmp(cp, namestr, nameln) != 0) {
				PMD(PMD_ERROR, ("%s not %s\n", cp, namestr))
				goto errout;
			}
			if (i != 0) {
				if (level == 0) {	/* no level spec'd */
					PMD(PMD_ERROR, ("no level specd\n"))
					goto errout;
				}
				np[npi++] = lvals[level - 1];
				/* finish up previous component levels */
				FINISH_COMP;
			}
			cp += nameln;
			if (!*cp) {
				PMD(PMD_ERROR, ("pm_autoconfig: NAME stands "
				    "alone\n"))
				goto errout;
			}
			p = kmem_zalloc(sizeof (*phead), KM_SLEEP);
			if (phead == NULL) {
				phead = ptail = p;
			} else {
				ptail->next = p;
				ptail = p;
			}
			compp = p->comp = kmem_zalloc(sizeof (pm_comp_t),
			    KM_SLEEP);
			compp->pmc_name_sz = strlen(cp) + 1;
			compp->pmc_name = kmem_zalloc(compp->pmc_name_sz,
			    KM_SLEEP);
			(void) strncpy(compp->pmc_name, cp, compp->pmc_name_sz);
			components++;
			level = 0;
		} else {	/* better be power level <num>=<name> */
#ifdef DEBUG
			tp = cp;
#endif
			if (i == 0 ||
			    (cp = pm_parsenum(cp, &lvals[level])) == NULL) {
				PMD(PMD_ERROR, ("pm_parsenum(%s) fails\n",
				    tp))
				goto errout;
			}
#ifdef DEBUG
			tp = cp;
#endif
			if (*cp++ != '=' || !*cp) {
				PMD(PMD_ERROR, ("expected =, got %s\n", tp))
				goto errout;
			}

			lszs[level] = strlen(cp) + 1;
			size += lszs[level];
			lnames[level] = cp;	/* points into prop string */
			level++;
		}
	}
	np[npi++] = lvals[level - 1];
	if (level == 0) {	/* ended with a name */
		PMD(PMD_ERROR, ("ended with name %s\n", np[npi -1]))
		goto errout;
	}
	FINISH_COMP;


	/*
	 * Now we have a list of components--we have to return instead an
	 * array of them, but we can just copy the top level and leave
	 * the rest as is
	 */
	(void) e_pm_create_components(dip, components);
	for (i = 0; i < components; i++)
		e_pm_set_max_power(dip, i, np[i]);

	ret = kmem_zalloc(components * sizeof (pm_comp_t), KM_SLEEP);
	for (i = 0, p = phead; i < components; i++) {
		ASSERT(p);
		/*
		 * Now sanity-check values:  levels must be monotonically
		 * increasing
		 */
		if (p->comp->pmc_numlevels < 2) {
			PMD(PMD_ERROR, ("component %s of %s@%s has "
			    "only %d levels\n", p->comp->pmc_name,
			    PM_NAME(dip), PM_ADDR(dip),
			    p->comp->pmc_numlevels))
			goto errout;
		}
		for (j = 0; j < p->comp->pmc_numlevels; j++) {
			if ((p->comp->pmc_lvals[j] < 0) || ((j > 0) &&
			    (p->comp->pmc_lvals[j] <=
			    p->comp->pmc_lvals[j - 1]))) {
				PMD(PMD_ERROR, ("component %s of %s@%s not "
				    "mono. increasing, value %d follows "
				    "%d\n", p->comp->pmc_name, PM_NAME(dip),
				    PM_ADDR(dip), p->comp->pmc_lvals[j],
				    p->comp->pmc_lvals[j - 1]))
				goto errout;
			}
		}
		ret[i] = *p->comp;	/* struct assignment */
		for (j = 0; j < i; j++) {
			/*
			 * Test for unique component names
			 */
			if (strcmp(ret[j].pmc_name, ret[i].pmc_name) == 0) {
				PMD(PMD_ERROR, ("component name %s of %s#%s"
				    " is not unique\n", ret[j].pmc_name,
				    PM_NAME(dip), PM_ADDR(dip)))
				goto errout;
			}
		}
		ptail = p;
		p = p->next;
		phead = p;	/* errout depends on phead making sense */
		kmem_free(ptail->comp, sizeof (*ptail->comp));
		kmem_free(ptail, sizeof (*ptail));
	}
out:
	ddi_prop_free(pp);
	if (lvals)
		kmem_free(lvals, nelems * sizeof (int));
	if (lszs)
		kmem_free(lszs, nelems * sizeof (int));
	if (lnames)
		kmem_free(lnames, nelems * sizeof (char *));
	if (np)
		kmem_free(np, nelems * sizeof (int));
	return (ret);

errout:
	e_pm_destroy_components(dip);
	*errp = 1;	/* signal failure */
	cmn_err(CE_CONT, "!pm: %s property ", pmcompstr);
	for (i = 0; i < nelems - 1; i++)
		cmn_err(CE_CONT, "!'%s', ", pp[i]);
	if (nelems != 0)
		cmn_err(CE_CONT, "!'%s'", pp[nelems - 1]);
	cmn_err(CE_CONT, "! for %s@%s is ill-formed.\n",
	    ddi_get_name(dip), PM_ADDR(dip));
	for (p = phead; p; ) {
		pm_comp_t *pp;
		int n;

		ptail = p;
		/*
		 * Free component data structures
		 */
		pp = p->comp;
		n = pp->pmc_numlevels;
		if (pp->pmc_name_sz) {
			kmem_free(pp->pmc_name, pp->pmc_name_sz);
		}
		if (pp->pmc_lnames_sz) {
			kmem_free(pp->pmc_lname_buf, pp->pmc_lnames_sz);
		}
		if (pp->pmc_lnames) {
			kmem_free(pp->pmc_lnames, n * (sizeof (char *)));
		}
		if (pp->pmc_thresh) {
			kmem_free(pp->pmc_thresh, n * (sizeof (int)));
		}
		if (pp->pmc_lvals) {
			kmem_free(pp->pmc_lvals, n * (sizeof (int)));
		}
		p = ptail->next;
		kmem_free(ptail, sizeof (*ptail));
	}
	if (ret != NULL)
		kmem_free(ret, components * sizeof (pm_comp_t));
	ret = NULL;
	goto out;
}

/*
 * Parse hex or decimal value from char string
 */
static char *
pm_parsenum(char *cp, int *valp)
{
	int ch, offset;
	char numbuf[256];
	char *np = numbuf;
	int value = 0;

	ch = *cp++;
	if (isdigit(ch)) {
		if (ch == '0') {
			if ((ch = *cp++) == 'x' || ch == 'X') {
				ch = *cp++;
				while (isxdigit(ch)) {
					*np++ = (char)ch;
					ch = *cp++;
				}
				*np = 0;
				cp--;
				goto hexval;
			} else {
				goto digit;
			}
		} else {
digit:
			while (isdigit(ch)) {
				*np++ = (char)ch;
				ch = *cp++;
			}
			*np = 0;
			cp--;
			goto decval;
		}
	} else
		return (NULL);

hexval:
	for (np = numbuf; *np; np++) {
		if (*np >= 'a' && *np <= 'f')
			offset = 'a' - 10;
		else if (*np >= 'A' && *np <= 'F')
			offset = 'A' - 10;
		else if (*np >= '0' && *np <= '9')
			offset = '0';
		value *= 16;
		value += *np - offset;
	}
	*valp = value;
	return (cp);

decval:
	offset = '0';
	for (np = numbuf; *np; np++) {
		value *= 10;
		value += *np - offset;
	}
	*valp = value;
	return (cp);
}

/*
 * Set threshold values for a devices components by dividing the target
 * threshold (base) by the number of transistions and assign each transition
 * that threshold.  This will get the entire device down in the target time if
 * all components are idle and even if there are dependencies among components.
 *
 * Devices may well get powered all the way down before the target time, but
 * at least the EPA will be happy.
 */
void
pm_set_device_threshold(dev_info_t *dip, int ncomp, int base, int flag)
{
	int target_threshold = (base * 95) / 100;
	int level, comp;		/* loop counters */
	int transitions = 0;
	int thresh;
	pm_comp_t *pmc;

	/*
	 * First we handle the easy one.  If we're setting the default
	 * threshold for a node with children, then we set it to the
	 * default nexus threshold (currently 0) and mark it as default
	 * nexus threshold instead
	 */
	if (flag == PMC_DEF_THRESH && PM_IS_NEXUS(dip)) {
		PMD(PMD_THRESH, ("[%s@%s NEXDEF]\n", PM_NAME(dip),
		    PM_ADDR(dip)))
		thresh = pm_default_nexus_threshold;
		for (comp = 0; comp < ncomp; comp++) {
			pmc = &DEVI(dip)->devi_pm_components[comp].pmc_comp;
			for (level = 1; level < pmc->pmc_numlevels; level++) {
				pmc->pmc_thresh[level] = thresh;
			}
		}
		ASSERT(PM_IAM_LOCKING_DIP(dip));
		DEVI(dip)->devi_pm_dev_thresh = pm_default_nexus_threshold;
		DEVI(dip)->devi_pm_flags &= PMC_THRESH_NONE;
		DEVI(dip)->devi_pm_flags |= PMC_NEXDEF_THRESH;
		return;
	}
	/*
	 * Compute the total number of transitions for all components
	 * of the device.  Distribute the threshold evenly over them
	 */
	for (comp = 0; comp < ncomp; comp++) {
		pmc = &DEVI(dip)->devi_pm_components[comp].pmc_comp;
		ASSERT(pmc->pmc_numlevels > 1);
		transitions += pmc->pmc_numlevels - 1;
	}
	ASSERT(transitions);
	thresh = target_threshold / transitions;

	for (comp = 0; comp < ncomp; comp++) {
		pmc = &DEVI(dip)->devi_pm_components[comp].pmc_comp;
		for (level = 1; level < pmc->pmc_numlevels; level++) {
			pmc->pmc_thresh[level] = thresh;
		}
	}
	ASSERT(PM_IAM_LOCKING_DIP(dip));
	DEVI(dip)->devi_pm_dev_thresh = base;
	DEVI(dip)->devi_pm_flags &= PMC_THRESH_NONE;
	DEVI(dip)->devi_pm_flags |= flag;
}

/*
 * Called when there is no old-style platform power management driver
 */
static int
ddi_no_platform_power(power_req_t *req)
{
	_NOTE(ARGUNUSED(req))
	return (DDI_FAILURE);
}

/*
 * This function calls the entry point supplied by the platform-specific
 * pm driver to bring the device component 'pm_cmpt' to power level 'pm_level'.
 * The use of global for getting the  function name from platform-specific
 * pm driver is not ideal, but it is simple and efficient.
 * The previous property lookup was being done in the idle loop on swift
 * systems without pmc chips and hurt deskbench performance as well as
 * violating scheduler locking rules
 */
int	(*pm_platform_power)(power_req_t *) = ddi_no_platform_power;

/*
 * Old obsolete interface for a device to request a power change (but only
 * an increase in power)
 */
int
ddi_dev_is_needed(dev_info_t *dip, int pm_cmpt, int pm_level)
{
	pm_component_t *cp;

	/*
	 * Fast path for drivers that really shouldn't be doing this:
	 * If device already power managed and at the requested power
	 * level, then we're done
	 */
	if (pm_level > 0 && PM_GET_PM_INFO(dip) &&
	    pm_cmpt >= 0 && pm_cmpt < PM_NUMCMPTS(dip)) {
		cp = &DEVI(dip)->devi_pm_components[pm_cmpt];
		if (pm_level == cp->pmc_comp.pmc_lvals[cp->pmc_cur_pwr]) {
			PMD(PMD_IOCTL, ("ddi_dev_is_needed(%s@%s, cmpt %d, "
			    "level %d) fast path\n", PM_NAME(dip),
			    PM_ADDR(dip), pm_cmpt, pm_level))
			return (DDI_SUCCESS);
		}
	}
	PMD(PMD_IOCTL, ("ddi_dev_is_needed(%s@%s, cmpt %d, level %d) slow "
	    "path\n", PM_NAME(dip), PM_ADDR(dip),  pm_cmpt, pm_level))

	if (pm_level <= 0)
		return (DDI_FAILURE);

	return (dev_is_needed(dip, pm_cmpt, pm_level, PM_LEVEL_UPONLY));
}

/*
 * The old obsolete interface to platform power management.  Only used by
 * Gypsy platform and APM on X86.
 */
int
ddi_power(dev_info_t *dip, int pm_cmpt, int pm_level)
{
	power_req_t	request;

	request.request_type = PMR_SET_POWER;
	request.req.set_power_req.who = dip;
	request.req.set_power_req.cmpt = pm_cmpt;
	request.req.set_power_req.level = pm_level;
	return (ddi_ctlops(dip, dip, DDI_CTLOPS_POWER, &request, NULL));
}

/*
 * Returns true if a device indicates that its parent handles suspend/resume
 * processing for it.
 */
int
e_ddi_parental_suspend_resume(dev_info_t *dip)
{
	return (DEVI(dip)->devi_pm_flags & PMC_PARENTAL_SR);
}

/*
 * Called for devices which indicate that their parent does suspend/resume
 * handling for them
 */
int
e_ddi_suspend(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	power_req_t	request;
	request.request_type = PMR_SUSPEND;
	request.req.suspend_req.who = dip;
	request.req.suspend_req.cmd = cmd;
	return (ddi_ctlops(dip, dip, DDI_CTLOPS_POWER, &request, NULL));
}

/*
 * Called for devices which indicate that their parent does suspend/resume
 * handling for them
 */
int
e_ddi_resume(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	power_req_t	request;
	request.request_type = PMR_RESUME;
	request.req.resume_req.who = dip;
	request.req.resume_req.cmd = cmd;
	return (ddi_ctlops(dip, dip, DDI_CTLOPS_POWER, &request, NULL));
}

/*
 * Called to notify parent of proposed change of power level of child
 * (In support of busses which require parent to participate in power level
 * changes)
 */
int
e_ddi_prepower(dev_info_t *dip, int pm_cmpt,
    int pm_newlevel, int pm_oldlevel)
{
	power_req_t	request;
	request.request_type = PMR_PRE_SET_POWER;
	request.req.pre_set_power_req.who = dip;
	request.req.pre_set_power_req.cmpt = pm_cmpt;
	request.req.pre_set_power_req.old_level = pm_oldlevel;
	request.req.pre_set_power_req.new_level = pm_newlevel;
	return (ddi_ctlops(dip, dip, DDI_CTLOPS_POWER, &request, NULL));
}

/*
 * Called to notify parent of accomplished change of power level of child
 * (In support of busses which require parent to participate in power level
 * changes)
 */
int
e_ddi_postpower(dev_info_t *dip, int pm_cmpt,
    int pm_newlevel, int pm_oldlevel, int result)
{
	power_req_t	request;
	request.request_type = PMR_POST_SET_POWER;
	request.req.post_set_power_req.who = dip;
	request.req.post_set_power_req.cmpt = pm_cmpt;
	request.req.post_set_power_req.old_level = pm_oldlevel;
	request.req.post_set_power_req.new_level = pm_newlevel;
	request.req.post_set_power_req.result = result;
	return (ddi_ctlops(dip, dip, DDI_CTLOPS_POWER, &request, NULL));
}

/*
 * Called to notify parent of accomplished change of power level of child
 * (In support of busses which require parent to participate in power level
 * changes)
 */
static int
e_ddi_haschanged(dev_info_t *dip, int pm_cmpt,
    int pm_newlevel, int pm_oldlevel, int result)
{
	power_req_t	request;
	request.request_type = PMR_CHANGED_POWER;
	request.req.changed_power_req.who = dip;
	request.req.changed_power_req.cmpt = pm_cmpt;
	request.req.changed_power_req.old_level = pm_oldlevel;
	request.req.changed_power_req.new_level = pm_newlevel;
	request.req.changed_power_req.result = result;
	return (ddi_ctlops(dip, dip, DDI_CTLOPS_POWER, &request, NULL));
}

/*
 * Create (empty) component data strucutures.
 */
static void
e_pm_create_components(dev_info_t *dip, int num_components)
{
	struct pm_component *compp, *ocompp;
	int i, size = 0;

	ASSERT(PM_IAM_LOCKING_DIP(dip));
	ASSERT(!DEVI(dip)->devi_pm_components);
	ASSERT(!(DEVI(dip)->devi_pm_flags & PMC_COMPONENTS_DONE));
	size = sizeof (struct pm_component) * num_components;

	compp = kmem_zalloc(size, KM_SLEEP);
	ocompp = compp;
	DEVI(dip)->devi_pm_comp_size = size;
	DEVI(dip)->devi_pm_num_components = num_components;
#ifdef DEBUG
	DEVI(dip)->devi_pm_plockmask = kmem_zalloc(bTOBRU(num_components),
	    KM_SLEEP);
#endif
	PM_LOCK_BUSY(dip);
	for (i = 0; i < num_components;  i++) {
		compp->pmc_timestamp = hrestime.tv_sec;
		compp->pmc_norm_pwr = (uint_t)-1;
		PMD(PMD_NORM, ("epcc: %s@%s, comp %d, level (-1)\n",
		    PM_NAME(dip), PM_ADDR(dip), i))
		compp++;
	}
	PM_UNLOCK_BUSY(dip);
	DEVI(dip)->devi_pm_components = ocompp;
	DEVI(dip)->devi_pm_flags |= PMC_COMPONENTS_DONE;
}

/*
 * Old obsolete exported interface for drivers to create components.
 * This is now handled by exporting the pm-components property.
 */
int
pm_create_components(dev_info_t *dip, int num_components)
{
	if (num_components < 1)
		return (DDI_FAILURE);

	PM_LOCK_DIP(dip);

	if (DEVI(dip)->devi_pm_components) {
		PM_UNLOCK_DIP(dip);
		PMD(PMD_ERROR, ("pm_create_components %s@%s already has %d "
		    "components\n", PM_NAME(dip), PM_ADDR(dip),
		    PM_NUMCMPTS(dip)))
		return (DDI_FAILURE);
	}
	PMD(PMD_REMDEV, ("pm_create_components(%s@%s, %d)\n",
	    PM_NAME(dip), PM_ADDR(dip), num_components))
	e_pm_create_components(dip, num_components);
	DEVI(dip)->devi_pm_flags |= PMC_BC;
	e_pm_default_components(dip, num_components);
	PM_UNLOCK_DIP(dip);
	return (DDI_SUCCESS);
}

/*
 * Internal routine for destroying components
 */
static void
e_pm_destroy_components(dev_info_t *dip)
{

	int size;
	struct pm_component *cp;

	PMD(PMD_REMDEV, ("e_pm_destroy_components %s@%s\n",
	    PM_NAME(dip), PM_ADDR(dip)))
	ASSERT(PM_IAM_LOCKING_DIP(dip));
	cp = DEVI(dip)->devi_pm_components;
	if (cp) {
#ifdef DEBUG
		int comps = DEVI(dip)->devi_pm_num_components;
		ASSERT(DEVI(dip)->devi_pm_plockmask);
		kmem_free(DEVI(dip)->devi_pm_plockmask, bTOBRU(comps));
		DEVI(dip)->devi_pm_plockmask = NULL;
#endif
		DEVI(dip)->devi_pm_components = NULL;
		DEVI(dip)->devi_pm_num_components = 0;
		size = DEVI(dip)->devi_pm_comp_size;
		kmem_free(cp, size);
	}
	DEVI(dip)->devi_pm_flags &=
	    ~(PMC_COMPONENTS_DONE | PMC_COMPONENTS_FAILED);
}

/*
 * Obsolete interface previosly called by drivers to destroy their components
 * at detach time.  This is now done automatically.  However, we need to keep
 * this for the old drivers.
 */
void
pm_destroy_components(dev_info_t *dip)
{
	pm_info_t *info, *pinfo;
	dev_info_t 	*pdip;
	int was_scanning = 0;

	PMD(PMD_REMDEV | PMD_KIDSUP, ("pm_destroy_components %s@%s\n",
	    PM_NAME(dip), PM_ADDR(dip)))
#ifdef DEBUG
	if (!PM_ISBC(dip))
		cmn_err(CE_WARN, "!driver exporting pm-components property "
		    "(%s@%s) calls pm_destroy_components", ddi_get_name(dip),
		    PM_ADDR(dip));
#endif
	/*
	 * We ignore this unless this is an old-style driver, except for
	 * printing the message above
	 */
	if (PM_NUMCMPTS(dip) == 0 || !PM_ISBC(dip)) {
		PMD(PMD_REMDEV, ("ignoring %s@%s\n", PM_NAME(dip),
		    PM_ADDR(dip)))
		return;
	}
	info = PM_GET_PM_INFO(dip);
	ASSERT(info);
	/*
	 * make sure scan isn't scanning this one
	 */
	rw_enter(&pm_scan_list_rwlock, RW_WRITER);
	if (info->pmi_dev_pm_state & PM_SCAN) {
		was_scanning = 1;
		info->pmi_dev_pm_state &= ~PM_SCAN;
	}
	(void) pm_dequeue_scan(dip);
	ASSERT(!pm_dequeue_scan(dip));	/* assert not in list twice */
	/*
	 * pm_unmanage will clear info pointer later, after dealing with
	 * dependencies
	 */
	rw_exit(&pm_scan_list_rwlock);
	PM_LOCK_DIP(dip);
	/*
	 * Now adjust parent's pmi_kidsupcnt.  We check only comp 0.
	 * Parents that get notification are not adjusted because their
	 * kidsupcnt is always 0.
	 */
	if (((pdip = ddi_get_parent(dip)) != NULL) &&
	    ((pinfo = PM_GET_PM_INFO(pdip)) != NULL) &&
	    !(DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY)) {
		if (PM_CURPOWER(dip, 0) != 0) {
			PMD(PMD_KIDSUP, ("pm_destroy_comps %s@%s kuc %d to %d "
			    "because %s@%s being unmanaged\n",
			    PM_NAME(pdip), PM_ADDR(pdip), pinfo->pmi_kidsupcnt,
			    pinfo->pmi_kidsupcnt - 1, PM_NAME(dip),
			    PM_ADDR(dip)))
			pinfo->pmi_kidsupcnt--;
			ASSERT(pinfo->pmi_kidsupcnt >= 0);
			if (pinfo->pmi_kidsupcnt == 0)
				pm_rescan(1);
		}
#ifdef DEBUG
		else {
			PMD(PMD_KIDSUP, ("pm_destroy_comps %s@%s kuc unchanged "
			    "at %d because %s@%s components gone\n",
			    PM_NAME(pdip), PM_ADDR(pdip), pinfo->pmi_kidsupcnt,
			    PM_NAME(dip), PM_ADDR(dip)))
		}
#endif
	}
	/*
	 * Forget we ever knew anything about the components of this  device
	 */
	DEVI(dip)->devi_pm_flags &=
	    ~(PMC_BC | PMC_COMPONENTS_DONE | PMC_COMPONENTS_FAILED);
	if (was_scanning) {
		mutex_enter(&pm_scan_lock);
		ASSERT(bcpm_enabled);
		bcpm_enabled--;
		mutex_exit(&pm_scan_lock);
	}
	e_pm_destroy_components(dip);
	PM_UNLOCK_DIP(dip);
}

/*
 * Exported interface for a driver to set a component busy.
 */
int
pm_busy_component(dev_info_t *dip, int cmpt)
{
	struct pm_component *cp;
	int ret;
	pm_info_t	*info;

	ASSERT(dip != NULL);
	/*
	 * In order to not slow the common case (info already set) we check
	 * first without taking the lock (which we only need to hold if we are
	 * going to put the device into power management because we were
	 * called from the device's attach routine).
	 */
	info = PM_GET_PM_INFO(dip);
	if (!info) {
		PM_LOCK_DIP(dip);
		if (!DEVI_IS_ATTACHING(dip)) {
			PM_UNLOCK_DIP(dip);
			return (DDI_FAILURE);
		}
		/* now check again under the lock */
		info = PM_GET_PM_INFO(dip);
		if (!info) {
			ret = e_pm_manage(dip, 2);
			PM_UNLOCK_DIP(dip);
			if (ret != DDI_SUCCESS) {
				return (DDI_FAILURE);
			} else {
				info = PM_GET_PM_INFO(dip);
				ASSERT(info);
			}
		} else {
			PM_UNLOCK_DIP(dip);
		}
	}
	if (cmpt < 0 || cmpt >= DEVI(dip)->devi_pm_num_components) {
		return (DDI_FAILURE);
	}
	cp = &DEVI(dip)->devi_pm_components[cmpt];
	PM_LOCK_BUSY(dip);
	cp->pmc_busycount++;
	cp->pmc_timestamp = 0;
	PM_UNLOCK_BUSY(dip);
	return (DDI_SUCCESS);
}

/*
 * Exported interface for a driver to set a component idle.
 */
int
pm_idle_component(dev_info_t *dip, int cmpt)
{
	struct pm_component *cp;
	int ret;
	pm_info_t	*info;

	ASSERT(dip != NULL);
	/*
	 * In order to not slow the common case (info already set) we check
	 * first without taking the lock (which we only need to hold if we are
	 * going to put the device into power management because we were
	 * called from the device's attach routine).
	 */
	info = PM_GET_PM_INFO(dip);
	if (!info) {
		PM_LOCK_DIP(dip);
		if (!DEVI_IS_ATTACHING(dip)) {
			PM_UNLOCK_DIP(dip);
			return (DDI_FAILURE);
		}
		/* now check again under the lock */
		info = PM_GET_PM_INFO(dip);
		if (!info) {
			ret = e_pm_manage(dip, 2);
			PM_UNLOCK_DIP(dip);
			if (ret != DDI_SUCCESS) {
				return (DDI_FAILURE);
			} else {
				info = PM_GET_PM_INFO(dip);
				ASSERT(info);
			}
		} else {
			PM_UNLOCK_DIP(dip);
		}
	}
	if (cmpt < 0 || cmpt >= DEVI(dip)->devi_pm_num_components) {
		return (DDI_FAILURE);
	}
	cp = &DEVI(dip)->devi_pm_components[cmpt];
	PM_LOCK_BUSY(dip);
	if (cp->pmc_busycount) {
		if (--(cp->pmc_busycount) == 0)
			cp->pmc_timestamp = hrestime.tv_sec;
	} else {
		cp->pmc_timestamp = hrestime.tv_sec;
	}
	PM_UNLOCK_BUSY(dip);
	return (DDI_SUCCESS);
}

/*
 * Set max (previously documentd as "normal") power.
 */
static void
e_pm_set_max_power(dev_info_t *dip, int component_number, int level)
{
	struct pm_component *cp =
	    &DEVI(dip)->devi_pm_components[component_number];
	cp->pmc_norm_pwr = level;
	PMD(PMD_NORM, ("epsnp: %s%s, comp %d, level %d\n", PM_NAME(dip),
	    PM_ADDR(dip), component_number, level))
}

/*
 * This is the old  obsolete interface called by drivers to set their normal
 * power.  Thus we can't fix its behavior or return a value.
 * This functionality is replaced by the pm-component property.
 * We'll only get components destroyed while no power management is
 * going on (and the device is detached), so we don't need a mutex here
 */
void
pm_set_normal_power(dev_info_t *dip, int comp, int level)
{
#ifdef DEBUG
	if (!PM_ISBC(dip))
		cmn_err(CE_WARN, "!call to pm_set_normal_power() by %s@%s "
		    "(driver exporting pm-components property) ignored",
		    ddi_get_name(dip), PM_ADDR(dip));
#endif
	if (PM_ISBC(dip)) {
		e_pm_set_max_power(dip, comp, level);
		e_pm_default_levels(dip,
		    &DEVI(dip)->devi_pm_components[comp], level);
	}
}

/*
 * The node is the subject of a reparse pm props ioctl. Throw away the old
 * info and start over
 */
int
e_new_pm_props(dev_info_t *dip)
{
	PM_LOCK_DIP(dip);
	if ((PM_GET_PM_INFO(dip) != NULL) &&
	    ddi_prop_exists(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "pm-components")) {
		if (pm_unmanage(dip, 0) != DDI_SUCCESS) {
			PM_UNLOCK_DIP(dip);
			return (DDI_FAILURE);
		}
		e_pm_destroy_components(dip);
		if (e_pm_manage(dip, 0) != DDI_SUCCESS) {
			PM_UNLOCK_DIP(dip);
			return (DDI_FAILURE);
		}
	}
	PM_UNLOCK_DIP(dip);
	e_pm_props(dip);
	return (DDI_SUCCESS);
}

/* XXX move to header file */
#define	CP(dip, comp) (&DEVI(dip)->devi_pm_components[comp])
/*
 * Device has been attached, so process its pm  properties, such as
 * "pm-hardware-state"
 */
void
e_pm_props(dev_info_t *dip)
{
	char *pp;
	int ret, len;
	dev_info_t *parent = ddi_get_parent(dip);

	/*
	 * It doesn't matter if we do this more than once, we should always
	 * get the same answers, and if not, then the last one in is the
	 * best one.
	 */
	PM_LOCK_DIP(dip);
	ret = ddi_getlongprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP, "pm-hardware-state",
	    (caddr_t)&pp, &len);
	if (ret == DDI_PROP_SUCCESS) {
		if (strcmp(pp, "needs-suspend-resume") == 0) {
			DEVI(dip)->devi_pm_flags |= PMC_NEEDS_SR;
		} else if (strcmp(pp, "no-suspend-resume") == 0) {
			DEVI(dip)->devi_pm_flags |= PMC_NO_SR;
		} else if (strcmp(pp, "parental-suspend-resume") == 0) {
			DEVI(dip)->devi_pm_flags |= PMC_PARENTAL_SR;
		} else {
			cmn_err(CE_NOTE, "!device %s@%s has unrecognized "
			    "%s property value '%s'", PM_NAME(dip),
			    PM_ADDR(dip), "pm-hardware-state", pp);
		}
		kmem_free(pp, len);
	}

	/*
	 * This next segment (PMC_WANTS_NOTIFY and PMC_NOTIFY_PARENT) is in
	 * support of nexus drivers which will want to be involved in
	 * (or at least notified of) their child node's power level transitions.
	 * "pm-want-child-notification?" is defined by the parent.  If we're
	 * processing a child which has this set in the parent,
	 * then we set a flag in the child to optimise access at power level
	 * change time
	 */
	if (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP,
	    "pm-want-child-notification?", 0) == 1)
		DEVI(dip)->devi_pm_flags |= PMC_WANTS_NOTIFY;

	/*
	 * We depend on top-down attachment here
	 */
	if (parent && DEVI(parent)->devi_pm_flags & PMC_WANTS_NOTIFY)
		DEVI(dip)->devi_pm_flags |= PMC_NOTIFY_PARENT;

	PM_UNLOCK_DIP(dip);
}
/*
 * We overload the bus_ctl ops here--perhaps we ought to have a distinct
 * power_ops struct for this functionality instead?
 * However, we only ever do this on a ppm driver.
 */
int
pm_ctlops(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t op, void *a, void *v)
{
	register int (*fp)();

	if (d == NULL) {	/* no ppm handler, call the default routine */
		return (pm_default_ctlops(d, r, op, a, v));
	}
	if (!d || !r)
		return (DDI_FAILURE);
	ASSERT(DEVI(d)->devi_ops && DEVI(d)->devi_ops->devo_bus_ops &&
		DEVI(d)->devi_ops->devo_bus_ops->bus_ctl);

	fp = DEVI(d)->devi_ops->devo_bus_ops->bus_ctl;
	return ((*fp)(d, r, op, a, v));
}

/*
 * Called on an attached driver to tell us it is ready to start doing pm
 * (Device may not be power manageable at all).
 */
int
pm_start(dev_info_t *dip)
{
	int ret;
	pm_info_t *pinfo;
	dev_info_t *pdip;
	int incr = 0;

	/*
	 * Have to increment kidsupcnt on spec so parent doesn't get powered
	 * down after child gets power managed (or not) but before parents
	 * count gets incremented, then adjust count afterwards depending on
	 * the result of the pm_manage call
	 */
	if ((pdip = ddi_get_parent(dip)) != NULL &&
	    ((pinfo = PM_GET_PM_INFO(pdip)) != NULL) &&
	    !(DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY)) {
		incr = 1;
		PM_LOCK_DIP(pdip);
		ASSERT(pinfo->pmi_kidsupcnt >= 0);
		PMD(PMD_KIDSUP, ("pm_start %s@%s kuc %d to %d on spec; "
		    "child %s@%s attaching\n", PM_NAME(pdip), PM_ADDR(pdip),
		    pinfo->pmi_kidsupcnt, pinfo->pmi_kidsupcnt + 1,
		    PM_NAME(dip), PM_ADDR(dip)))
		pinfo->pmi_kidsupcnt++;
		PM_UNLOCK_DIP(pdip);
	}
	ret = pm_manage(dip, 2);
	if (incr) {
		if (PM_GET_PM_INFO(dip) == NULL) {
			/*
			 * keep the kidsupcount increment as is
			 */
			PM_LOCK_DIP(dip);
			DEVI(dip)->devi_pm_flags |= PMC_NOPMKID;
			PM_UNLOCK_DIP(dip);
		} else {
			/*
			 * Undo the speculative increment, as pm_manage
			 * will have accounted for the real count
			 */
			PM_LOCK_DIP(pdip);
			ASSERT(pinfo->pmi_kidsupcnt >= 0);
			PMD(PMD_KIDSUP, ("pm_start %s@%s kuc %d to %d because "
			    "pm'd child %s@%s attached\n", PM_NAME(pdip),
			    PM_ADDR(pdip), pinfo->pmi_kidsupcnt,
			    pinfo->pmi_kidsupcnt - 1, PM_NAME(dip),
			    PM_ADDR(dip)))
			pinfo->pmi_kidsupcnt--;
			PM_UNLOCK_DIP(pdip);
		}
	}
	return (ret);
}

/*
 * Called on a successfully detached driver to free pm resources
 */
int
pm_stop(dev_info_t *dip)
{
	int ret = DDI_SUCCESS;
	pm_info_t *pinfo;
	dev_info_t *pdip;

	PMD(PMD_REMDEV, ("pm_stop %s@%s info %p, %d components\n",
	    PM_NAME(dip), PM_ADDR(dip), (void *)PM_GET_PM_INFO(dip),
	    PM_NUMCMPTS(dip)))
	PM_LOCK_DIP(dip);
	if (PM_GET_PM_INFO(dip) != NULL) {
		if ((ret = pm_unmanage(dip, 0)) == DDI_SUCCESS) {
			e_pm_destroy_components(dip);
		} else {
			PMD(PMD_FAIL, ("pm_stop could not pm_unmanage %s@%s\n",
			    PM_NAME(dip), PM_ADDR(dip)))
		}
	} else {
		if (PM_NUMCMPTS(dip))
			e_pm_destroy_components(dip);
		else {
			if (DEVI(dip)->devi_pm_flags & PMC_NOPMKID) {
				pdip = ddi_get_parent(dip);
				ASSERT(pdip != NULL);
				pinfo = PM_GET_PM_INFO(pdip);
				ASSERT(pinfo != NULL);
				ASSERT(!(DEVI(pdip)->devi_pm_flags &
				    PMC_WANTS_NOTIFY));
				ASSERT(PM_IAM_LOCKING_DIP(dip));
				DEVI(dip)->devi_pm_flags &= ~PMC_NOPMKID;
				PM_LOCK_DIP(pdip);
				PMD(PMD_KIDSUP, ("pm_stop %s@%s kuc %d to %d "
				    "because non-pm'd child %s@%s detached\n",
				    PM_NAME(pdip), PM_ADDR(pdip),
				    pinfo->pmi_kidsupcnt,
				    pinfo->pmi_kidsupcnt - 1,
				    PM_NAME(dip),
				    PM_ADDR(dip)))
				pinfo->pmi_kidsupcnt--;
				ASSERT(pinfo->pmi_kidsupcnt >= 0);
				PM_UNLOCK_DIP(pdip);
			}
		}
	}
	PM_UNLOCK_DIP(dip);
	return (ret);
}

/*
 * Keep a list of recorded thresholds.  For now we just keep a list and
 * search it linearly.  We don't expect too many entries.  Can always hash it
 * later if we need to.
 */
void
pm_record_thresh(pm_thresh_rec_t *rp)
{
	extern pm_thresh_rec_t *pm_thresh_head;
	pm_thresh_rec_t *pptr, *ptr;

	ASSERT(*rp->ptr_physpath);
#ifdef DEBUG
	if (pm_debug & PMD_THRESH)
		prthresh("pm_record_thresh entry");
#endif
	rw_enter(&pm_thresh_rwlock, RW_WRITER);
	for (pptr = NULL, ptr = pm_thresh_head; ptr; ptr = ptr->ptr_next) {
		PMD(PMD_THRESH, ("p_r_t: check %s, %s\n", rp->ptr_physpath,
		    ptr->ptr_physpath))
		if (strcmp(rp->ptr_physpath, ptr->ptr_physpath) == 0) {
			/* replace this one */
			rp->ptr_next = ptr->ptr_next;
			if (pptr) {
				pptr->ptr_next = rp;
			} else {
				pm_thresh_head = rp;
			}
			rw_exit(&pm_thresh_rwlock);
			kmem_free(ptr, ptr->ptr_size);
#ifdef DEBUG
			if (pm_debug & PMD_THRESH)
				prthresh("pm_record_thresh exit1");
#endif
			return;
		}
		continue;
	}
	/*
	 * There was not a match in the list, insert this one in front
	 */
	if (pm_thresh_head) {
		rp->ptr_next = pm_thresh_head;
		pm_thresh_head = rp;
	} else {
		rp->ptr_next = NULL;
		pm_thresh_head = rp;
	}
#ifdef DEBUG
	if (pm_debug & PMD_THRESH)
		prthresh("pm_record_thresh exit2");
#endif
	rw_exit(&pm_thresh_rwlock);
}

/*
 * Create a new dependency record and hang a new dependency entry off of it
 */
pm_pdr_t *
newpdr(char *kept, char *keeps, major_t major)
{
	size_t size = strlen(kept) + strlen(keeps) + 2 + sizeof (pm_pdr_t);
	pm_pdr_t *p = kmem_zalloc(size, KM_SLEEP);
	p->pdr_size = size;
	p->pdr_major = major;
	p->pdr_kept = (char *)((intptr_t)p + sizeof (pm_pdr_t));
	(void) strcpy(p->pdr_kept, kept);
	p->pdr_keeper = (char *)((intptr_t)p->pdr_kept + strlen(kept) + 1);
	(void) strcpy(p->pdr_keeper, keeps);
	ASSERT((intptr_t)p->pdr_keeper + strlen(p->pdr_keeper) + 1 <=
	    (intptr_t)p + size);
	ASSERT((intptr_t)p->pdr_kept + strlen(p->pdr_kept) + 1 <=
	    (intptr_t)p + size);
	return (p);
}

/*
 * Keep a list of recorded dependencies.  We only need to keep the
 * keeper -> kept list, since we reject the ioctl if the kept node is not
 * attached (after ddi_hold_installed_driver).  Since we hold the device,
 * we don't need to worry about it going away and coming back.  If a
 * PM_RESET_PM happens, then we tear down and forget the dependencies, and it
 * is up to the user to issue the ioctl again if they want it (e.g. pmconfig)
 * Returns true if dependency already exists in the list.
 */
int
pm_record_dep(char *kept, char *keeper, major_t major)
{
	extern pm_pdr_t *pm_dep_head;
	pm_pdr_t *npdr, *ppdr, *pdr;

	PMD(PMD_KEEPS, ("pm_record_dep: kept %s, keeper %s ", kept, keeper))
	ASSERT(kept && keeper);
#ifdef DEBUG
	if (pm_debug & PMD_KEEPS)
		prdeps("pm_record_deps entry");
#endif
	rw_enter(&pm_dep_rwlock, RW_WRITER);
	for (ppdr = NULL, pdr = pm_dep_head; pdr;
	    ppdr = pdr, pdr = pdr->pdr_next) {
		PMD(PMD_KEEPS, ("p_r_d: check kept %s, %s\n", kept,
		    pdr->pdr_kept))
		if (strcmp(kept, pdr->pdr_kept) == 0 &&
		    strcmp(keeper, pdr->pdr_keeper) == 0) {
			PMD(PMD_KEEPS, ("p_r_d: found match\n"))
			rw_exit(&pm_dep_rwlock);
			return (1);
		}
	}
	/*
	 * We did not find any match, so we have to make an entry
	 */
	npdr = newpdr(kept, keeper, major);
	if (ppdr) {
		ASSERT(ppdr->pdr_next == NULL);
		ppdr->pdr_next = npdr;
	} else {
		ASSERT(pm_dep_head == NULL);
		pm_dep_head = npdr;
	}
#ifdef DEBUG
	if (pm_debug & PMD_KEEPS)
		prdeps("pm_record_dep after new record");
#endif
	pm_unresolved_deps++;
	rw_exit(&pm_dep_rwlock);
	return (0);
}

/*
 * Look up this device in the set of devices we've seen ioctls for
 * to see if we are holding a threshold spec for it.  If so, make it so.
 * At ioctl time, we were given the physical path of the device.
 */
int
pm_thresh_specd(dev_info_t *dip, int held)
{
	extern pm_thresh_rec_t *pm_thresh_head;
	extern krwlock_t pm_thresh_rwlock;
	extern void pm_apply_recorded_thresh(dev_info_t *, pm_thresh_rec_t *,
	    int);
	char *path = 0;
	char pathbuf[MAXNAMELEN];
	pm_thresh_rec_t *rp;

	path = ddi_pathname(dip, pathbuf);

	PMD(PMD_THRESH, ("pm_thresh_specd: %s@%s: %s\n", PM_NAME(dip),
	    PM_ADDR(dip), path))
	rw_enter(&pm_thresh_rwlock, RW_READER);
	for (rp = pm_thresh_head; rp; rp = rp->ptr_next) {
		PMD(PMD_THRESH, ("pm_t_s: checking %s\n", rp->ptr_physpath))
		if (strcmp(rp->ptr_physpath, path) != 0)
			continue;
		pm_apply_recorded_thresh(dip, rp, held);
		rw_exit(&pm_thresh_rwlock);
		return (1);
	}
	rw_exit(&pm_thresh_rwlock);
	return (0);
}

/*
 * Should this device keep up another device?
 * Caller must hold pm_dep_rwlock as reader, pm_scan_list_rwlock as writer.
 * Look up this device in the set of devices we've seen ioctls for
 * to see if we are holding a dependency spec for it.  If so, make it so.
 * Because we require the kept device to be attached already in order to
 * make the list entry (and hold it), we only need to look for keepers.
 * At ioctl time, we were given the physical path of the device.
 */
int
pm_dep_specd(dev_info_t *dip, int held)
{
	extern pm_pdr_t *pm_dep_head;
	extern int pm_apply_recorded_dep(dev_info_t *, pm_pdr_t *, int);
	char *path;
	char pathbuf[MAXPATHLEN];
	pm_pdr_t *dp;
	int ret = 0;

	if (!pm_unresolved_deps) {
		return (0);
	}
	path = ddi_pathname(dip, pathbuf);
	PMD(PMD_KEEPS, ("pm_d_s: %s\n", path))
	for (dp = pm_dep_head; dp && pm_unresolved_deps; dp = dp->pdr_next) {
		PMD(PMD_KEEPS, ("pm_d_s: checking %s\n", dp->pdr_keeper))
		if (strcmp(dp->pdr_keeper, path) == 0) {
			ret += pm_apply_recorded_dep(dip, dp, held);
		}
	}
	return (ret);
}

/*
 * Apply a recorded dependency.  dp specifies the dependency, and
 * keeper is already known to be the device that keeps up the other (kept) one.
 * We have to search pm_scan_list for the "kept" device, then apply
 * the dependency (which may already be applied).
 * Caller has pm_scan_list locked as a writer.
 */

int
pm_apply_recorded_dep(dev_info_t *keeper, pm_pdr_t *dp, int held)
{
	pm_scan_t *cur;
	dev_info_t *kept;
	pm_info_t *kept_info, *keeper_info;
	int ret = 0;
	int found = 0;
	int i;
	dev_info_t **new_dips;
	size_t length;
	void		prkeeps(dev_info_t *);

	PMD(PMD_KEEPS, ("pm_apply_recorded_dep: keeper %s@%s (%s), kept %s\n",
	    PM_NAME(keeper), PM_ADDR(keeper), dp->pdr_keeper, dp->pdr_kept))
	ASSERT(!held || PM_IAM_LOCKING_DIP(keeper));
	if (!held)
		PM_LOCK_DIP(keeper);
	for (cur = pm_scan_list; cur != NULL; cur = cur->ps_next) {
		if (cur->ps_dip == keeper)
			continue;
		PMD(PMD_KEEPS, ("p_a_r_d: %s %s\n", cur->ps_path, dp->pdr_kept))
		if (strcmp(cur->ps_path, dp->pdr_kept) == 0) {
			kept = cur->ps_dip;
			PMD(PMD_KEEPS, ("p_a_r_d: kept by %s@%s\n",
			    PM_NAME(kept), PM_ADDR(kept)))
			/*
			 * Found it, make it so.
			 * Dependency information is protected by
			 * pm_scan_list_rwlock
			 */
			PM_LOCK_DIP(kept);
			if ((keeper_info = PM_GET_PM_INFO(keeper)) == NULL) {
				cmn_err(CE_CONT, "!device %s@%s depends on "
				    "device %s@%s, but the latter is not "
				    "power managed", PM_NAME(keeper),
				    PM_ADDR(keeper), PM_NAME(kept),
				    PM_ADDR(kept));
				PMD((PMD_FAIL | PMD_KEEPS), ("p_a_r_d: keeper "
				    "%s not power managed\n", dp->pdr_keeper))
				PM_UNLOCK_DIP(kept);
				if (!held)
					PM_UNLOCK_DIP(keeper);
				ASSERT(!ret);
				return (0);
			}
			kept_info = PM_GET_PM_INFO(kept);
			ASSERT(kept_info);

			for (i = 0, found = 0;
			    i < kept_info->pmi_nkeptupby; i++) {
				if (kept_info->pmi_kupbydips[i] == keeper) {
					found = 1;
					break;
				}
			}
			if (found) {
				PM_UNLOCK_DIP(kept);
				PMD(PMD_KEEPS, ("%s already "
				    "keeps up %s\n", dp->pdr_keeper,
				    dp->pdr_kept))
				continue;
			}
			length = kept_info->pmi_nkeptupby *
			    sizeof (dev_info_t *);
			new_dips = kmem_alloc(length + sizeof (dev_info_t *),
			    KM_SLEEP);
			if (kept_info->pmi_nkeptupby) {
				bcopy(kept_info->pmi_kupbydips,  new_dips,
				    length);
				kmem_free(kept_info->pmi_kupbydips, length);
			}
			new_dips[kept_info->pmi_nkeptupby] = keeper;
			kept_info->pmi_kupbydips = new_dips;
			kept_info->pmi_nkeptupby++;

			length = keeper_info->pmi_nwekeepup *
			    sizeof (dev_info_t *);
			new_dips = kmem_alloc(length  + sizeof (dev_info_t *),
			    KM_SLEEP);
			if (keeper_info->pmi_nwekeepup) {
				bcopy(keeper_info->pmi_wekeepdips, new_dips,
				    length);
				kmem_free(keeper_info->pmi_wekeepdips, length);
			}
			new_dips[keeper_info->pmi_nwekeepup] = kept;
			keeper_info->pmi_wekeepdips = new_dips;
			keeper_info->pmi_nwekeepup++;
#ifdef DEBUG
			if (pm_debug & PMD_KEEPS) {
				PMD(PMD_KEEPS, ("after PM_ADD_DEPENDENT %s@%s "
				    "(dep %s@%s)\n", PM_NAME(keeper),
				    PM_ADDR(keeper), PM_NAME(kept),
				    PM_ADDR(kept)))
				prkeeps(keeper);
				prkeeps(kept);
			}
#endif
			ASSERT(pm_unresolved_deps);
			pm_unresolved_deps--;
			ret++;
			PM_UNLOCK_DIP(kept);
		}
	}
	if (!held)
		PM_UNLOCK_DIP(keeper);
	return (ret);
}

struct pm_console_state {
	dev_info_t	*pcs_dip;
	pm_info_t	*pcs_info;
	int		*pcs_full_power;
	int		*pcs_old_power;
	int		*pcs_changed;
	int		pcs_nc;
	size_t		pcs_size;
};

int pm_no_console_powerup = 1;

/*
 * Bring up the console, we're about to enter the prom.
 */
void *
pm_powerup_console(void)
{
	int		i;
	struct pm_console_state *pcs = NULL;
	extern int modrootloaded;

	if (!modrootloaded || pm_no_console_powerup)
		return (NULL);

	pcs = kmem_alloc(sizeof (*pcs), KM_SLEEP);
	/*
	 * save and restore console power state if power managed frame buffer
	 */
	if (prom_stdout_is_framebuffer() &&
	    (fbdev != NODEV) &&
	    (pcs->pcs_dip = dev_get_dev_info(fbdev, 0)) &&
	    (pcs->pcs_info = PM_GET_PM_INFO(pcs->pcs_dip)) &&
	    pm_get_norm_pwrs(pcs->pcs_dip,
	    &pcs->pcs_full_power, &pcs->pcs_size) == DDI_SUCCESS) {
		dev_info_t *dip = pcs->pcs_dip;
		pcs->pcs_nc = PM_NUMCMPTS(dip);
		ASSERT(pcs->pcs_size == (pcs->pcs_nc * sizeof (int *)));
		pcs->pcs_old_power = (int *)kmem_alloc(pcs->pcs_size, KM_SLEEP);
		pcs->pcs_changed = (int *)kmem_alloc(pcs->pcs_size, KM_SLEEP);
		PM_LOCK_POWER(dip, PMC_ALLCOMPS);
		for (i = 0; i < pcs->pcs_nc; i++) {
			int curpower = PM_CURPOWER(dip, i);
			pcs->pcs_old_power[i] = curpower;
			if (curpower != pcs->pcs_full_power[i]) {
				pcs->pcs_changed[i]++;
				(void) pm_set_power(dip, i,
				    pcs->pcs_full_power[i],
				    curpower, pcs->pcs_info, 0);
			}
		}
		return ((void *)pcs);
	} else {
		kmem_free(pcs, sizeof (*pcs));
		return (NULL);
	}
}

/*
 * Return the console to its power level at the time pm_powerup_console
 * was called
 */
void
pm_restore_console(void *cookie)
{
	struct pm_console_state *pcs = (struct pm_console_state *)cookie;
	int i;

	if (pcs) {
		for (i = pcs->pcs_nc - 1; i >= 0; i--) {
			if (pcs->pcs_changed[i]) {
				(void) pm_set_power(pcs->pcs_dip, i,
				    pcs->pcs_old_power[i],
				    pcs->pcs_full_power[i], pcs->pcs_info, 0);
			}
		}
		PM_UNLOCK_POWER(pcs->pcs_dip, PMC_ALLCOMPS);
		kmem_free(pcs->pcs_full_power, pcs->pcs_size);
		kmem_free(pcs->pcs_old_power, pcs->pcs_size);
		kmem_free(pcs->pcs_changed, pcs->pcs_size);
		kmem_free(pcs, sizeof (*pcs));
	}
}

/*
 * Called from common/io/pm.c
 */
int
pm_cur_power(pm_component_t *cp)
{
	return (cur_power(cp));
}

/*
 * External interface to sanity-check a power level.
 */
int
pm_valid_power(dev_info_t *dip, int comp, int level)
{
	if (comp >= 0 && comp < PM_NUMCMPTS(dip) && level >= 0)
		return (e_pm_valid_power(&DEVI(dip)->devi_pm_components[comp],
		    level));
	else {
		PMD(PMD_FAIL, ("pm_valid_power fails: comp %d,  ncomps %d, "
		    "level %d\n", comp, PM_NUMCMPTS(dip), level))
		return (0);
	}
}

/*
 * Returns true if level is a possible (valid) power level for component
 */
static int
e_pm_valid_power(pm_component_t *cp, int level)
{
	int i;
	int *ip = cp->pmc_comp.pmc_lvals;
	int limit = cp->pmc_comp.pmc_numlevels;

	for (i = 0; i < limit; i++) {
		if (level == *ip++)
			return (1);
	}
#if DEBUG
	if (pm_debug & PMD_FAIL) {
		ip = cp->pmc_comp.pmc_lvals;

		for (i = 0; i < limit; i++)
			prom_printf("%d: %d\n", i, *ip++);
	}
#endif
	return (0);
}

pm_rsvp_t *pm_blocked_list;

/*
 * Called when a device which is direct power managed (or the parent or
 * dependent of such a device) changes power, or when a pm clone is closed
 * that was direct power managing a device.  This call results in pm_blocked()
 * (below) returning.  Clone is ignored except in the PMP_RELEASE case.
 */
void
pm_proceed(dev_info_t *dip, int cmd, int comp, int newlevel, int oldlevel,
	int clone)
{
	_NOTE(ARGUNUSED(oldlevel))
	pm_rsvp_t *found = NULL;
	pm_rsvp_t *p;

	mutex_enter(&pm_rsvp_lock);
	switch (cmd) {
	/*
	 * we're giving up control, let any pending op continue
	 */
	case PMP_RELEASE:
		for (p = pm_blocked_list; p; p = p->pr_next) {
			if (dip == p->pr_dip) {
				p->pr_retval = PMP_RELEASE;
				mutex_enter(&pm_clone_lock);
				PMD(PMD_IOCTL, ("proceed: pm_poll_cnt[%d] %d "
				    "before assert\n", clone,
				    pm_poll_cnt[clone]))
				ASSERT(pm_poll_cnt[clone]);
				pm_poll_cnt[clone]--;
				mutex_exit(&pm_clone_lock);
				cv_signal(&p->pr_cv);
			}
		}
		break;

	/*
	 * process has done PM_SET_CUR_PWR, let a matching request succeed,
	 * and a non-matching request for the same device fail
	 */
	case PMP_SETPOWER:
		found = pm_rsvp_lookup(dip, comp);
		if (!found)	/* if driver not waiting */
			break;
		if (found->pr_newlevel == newlevel)
			found->pr_retval = PMP_SUCCEED;
		else
			found->pr_retval = PMP_FAIL;
		cv_signal(&found->pr_cv);
		break;

	default:
		cmn_err(CE_PANIC, "pm_proceed unknown cmd %d\n", cmd);
	}
	mutex_exit(&pm_rsvp_lock);
}

/*
 * Called when a device that is direct power managed needs to change state.
 * This routine arranges to block the request until the process managing
 * the device makes the change (or some other incompatible change) or
 * the process closes /dev/pm.
 */
int
pm_block(dev_info_t *dip, int comp, int newpower, int oldpower)
{
	pm_rsvp_t *new = kmem_zalloc(sizeof (*new), KM_SLEEP);
	int ret;
	void pm_dequeue_blocked(pm_rsvp_t *);
	void pm_enqueue_blocked(pm_rsvp_t *);

	new->pr_dip = dip;
	new->pr_comp = comp;
	new->pr_newlevel = newpower;
	new->pr_oldlevel = oldpower;
	cv_init(&new->pr_cv, NULL, CV_DEFAULT, NULL);
	mutex_enter(&pm_rsvp_lock);
	pm_enqueue_blocked(new);
	pm_enqueue_notify(PSC_PENDING_CHANGE, dip, comp, newpower, oldpower);
	/*
	 * Normally there will be no user context involved, but if there is
	 * (e.g. we are here via an ioctl call to a driver) then we should
	 * allow the process to abort the request, or we get an unkillable
	 * process if the same thread does PM_DIRECT_PM and
	 * pm_request_power_change
	 */
	if (cv_wait_sig(&new->pr_cv, &pm_rsvp_lock) == 0)
		ret = PMP_FAIL;
	else
		ret = new->pr_retval;
	pm_dequeue_blocked(new);
	mutex_exit(&pm_rsvp_lock);
	cv_destroy(&new->pr_cv);
	kmem_free(new, sizeof (*new));
	return (ret);
}


/*
 * Create an entry for a process to pick up indicating a power level change.
 */
void
pm_enqueue_notify(int cmd, dev_info_t *dip, int comp,
    int newlevel, int oldlevel)
{
	pscc_t	*pscc;
	psce_t	*psce;
	void		*cookie = NULL;
	pm_info_t	*info;

	ASSERT(MUTEX_HELD(&pm_rsvp_lock));
	switch (cmd) {
	case PSC_PENDING_CHANGE:	/* only for controlling process */
		PMD(PMD_DPM, ("p_e_n PENDING %s@%s, comp %d, new %d, old %x\n",
		    PM_NAME(dip), PM_ADDR(dip), comp, newlevel,
		    oldlevel))
		psce = pm_psc_dip_to_direct(dip, &pscc);
		ASSERT(psce);
		mutex_enter(&pm_clone_lock);
		PMD(PMD_IOCTL, ("PENDING: (%s@%s) pm_poll_cnt[%d] %d "
		    "before ++\n", PM_NAME(dip), PM_ADDR(dip),
		    pscc->pscc_clone, pm_poll_cnt[pscc->pscc_clone]))
		pm_poll_cnt[pscc->pscc_clone]++;
		mutex_exit(&pm_clone_lock);
		psc_entry(cmd, psce, dip, comp, newlevel, oldlevel);
		mutex_enter(&pm_clone_lock);
		PMD(PMD_DPM, ("signal for clone %d, pscc %p\n",
		    pscc->pscc_clone, (void *)pscc))
		cv_signal(&pm_clones_cv[pscc->pscc_clone]);
		pollwakeup(&pm_pollhead, (POLLRDNORM | POLLIN));
		mutex_exit(&pm_clone_lock);
		break;
	case PSC_HAS_CHANGED:
		PMD(PMD_DPM, ("p_e_n HAS %s@%s, comp %d, new %d, old %x\n",
		    PM_NAME(dip), PM_ADDR(dip), comp, newlevel,
		    oldlevel))
		info = PM_GET_PM_INFO(dip);
		if (info->pmi_dev_pm_state & PM_DIRECT_NEW) {
			psce = pm_psc_dip_to_direct(dip, &pscc);
			mutex_enter(&pm_clone_lock);
			PMD(PMD_IOCTL, ("HAS: (%s@%s) pm_poll_cnt[%d] %d "
			    "before ++\n", PM_NAME(dip), PM_ADDR(dip),
			    pscc->pscc_clone, pm_poll_cnt[pscc->pscc_clone]))
			pm_poll_cnt[pscc->pscc_clone]++;
			mutex_exit(&pm_clone_lock);
			psc_entry(cmd, psce, dip, comp, newlevel, oldlevel);
			PMD(PMD_DPM, ("signal for clone %d pscc %p\n",
			    pscc->pscc_clone, (void *)pscc))
			mutex_enter(&pm_clone_lock);
			cv_signal(&pm_clones_cv[pscc->pscc_clone]);
			pollwakeup(&pm_pollhead, (POLLRDNORM | POLLIN));
			mutex_exit(&pm_clone_lock);
		}
		rw_enter(&pm_pscc_interest_rwlock, RW_READER);
		while ((psce = psc_interest(&cookie, &pscc)) != NULL) {
			psc_entry(cmd, psce, dip, comp, newlevel, oldlevel);
			mutex_enter(&pm_clone_lock);
			cv_signal(&pm_clones_cv[pscc->pscc_clone]);
			mutex_exit(&pm_clone_lock);
		}
		rw_exit(&pm_pscc_interest_rwlock);
		break;
#ifdef DEBUG
	default:
		ASSERT(0);
#endif
	}
}

/*
 * Write an entry indicating a power level change (to be passed to a process
 * later) in the given psce.
 */
static void
psc_entry(int event, psce_t *psce, dev_info_t dip, int comp, int new, int old)
{
	char	buf[MAXNAMELEN];
	pm_state_change_t *p;

	ASSERT(MUTEX_HELD(&psce->psce_lock));
	p = psce->psce_in;
	if (p->size) {			/* overrun, mark the previous entry */
		kmem_free(p->physpath, p->size);
		if (p == psce->psce_first)
			psce->psce_last->event |= PSC_EVENT_LOST;
		else
			(p - 1)->event |= PSC_EVENT_LOST;
		p->size = 0;
		p->physpath = NULL;
	}
	(void) ddi_pathname(dip, buf);
	p->size = strlen(buf) + 1;
	p->physpath = kmem_alloc(p->size, KM_SLEEP);
	(void) strcpy(p->physpath, buf);
	p->event = event;
	p->timestamp = hrestime.tv_sec;
	p->component = comp;
	p->old_level = old;
	p->new_level = new;
	if (p == psce->psce_last)
		psce->psce_in = psce->psce_first;
	else
		psce->psce_in = ++p;
	mutex_exit(&psce->psce_lock);
}

/*
 * Returns true if the process is interested in power level changes (has issued
 * PM_GET_STATE_CHANGE ioctl).
 */
int
pm_interest_registered(int clone)
{
	ASSERT(clone >= 0 && clone < PM_MAX_CLONE - 1);
	return (pm_interest[clone]);
}

/*
 * Process with clone has just done PM_DIRECT_PM on dip, or has asked to
 * watch all state transitions (dip == NULL).  Set up data
 * structs to communicate with process about state changes.
 */
void
pm_register_watcher(int clone, dev_info_t *dip)
{
	pscc_t	*p;
	psce_t	*psce;
	static void pm_enqueue_pscc(pscc_t *, pscc_t **);

	/*
	 * We definitely need a control struct, then we have to search to see
	 * there is already an entries struct (in the dip != NULL case).
	 */
	pscc_t	*pscc = kmem_zalloc(sizeof (*pscc), KM_SLEEP);
	pscc->pscc_clone = clone;
	pscc->pscc_dip = dip;

#ifdef DEBUG
	if (pm_debug & PMD_DEREG) {
		printerest("pm_register_watcher before");
		prdirect("pm_register_watcher before");
	}
#endif

	if (dip) {
		int found = 0;
		rw_enter(&pm_pscc_direct_rwlock, RW_WRITER);
		for (p = pm_pscc_direct; p; p = p->pscc_next) {
			/*
			 * Already an entry for this clone, so just use it
			 * for the new one (for the case where a single
			 * process is watching multiple devices)
			 */
			if (p->pscc_clone == clone) {
				ASSERT(p->pscc_dip != dip);
				pscc->pscc_entries = p->pscc_entries;
				pscc->pscc_entries->psce_references++;
				found++;
			}
		}
		if (!found) {		/* create a new one */
			psce = kmem_zalloc(sizeof (psce_t), KM_SLEEP);
			mutex_init(&psce->psce_lock, NULL, MUTEX_DEFAULT, NULL);
			psce->psce_first =
			    kmem_zalloc(sizeof (pm_state_change_t) * PSCCOUNT,
			    KM_SLEEP);
			psce->psce_in = psce->psce_out = psce->psce_first;
			psce->psce_last = &psce->psce_first[PSCCOUNT - 1];
			psce->psce_references = 1;
			pscc->pscc_entries = psce;
		}
		pm_enqueue_pscc(pscc, &pm_pscc_direct);
		rw_exit(&pm_pscc_direct_rwlock);
	} else {
		ASSERT(!pm_interest_registered(clone));
		rw_enter(&pm_pscc_interest_rwlock, RW_WRITER);
#ifdef DEBUG
		for (p = pm_pscc_interest; p; p = p->pscc_next) {
			/*
			 * Should not be an entry for this clone!
			 */
			ASSERT(p->pscc_clone != clone);
		}
#endif
		psce = kmem_zalloc(sizeof (psce_t), KM_SLEEP);
		psce->psce_first = kmem_zalloc(sizeof (pm_state_change_t) *
		    PSCCOUNT, KM_SLEEP);
		psce->psce_in = psce->psce_out = psce->psce_first;
		psce->psce_last = &psce->psce_first[PSCCOUNT - 1];
		psce->psce_references = 1;
		pscc->pscc_entries = psce;
		if ((p = pm_pscc_interest) != NULL) {
			pscc->pscc_next = p;
			p->pscc_prev = p;
		}
		pm_pscc_interest = pscc;
		pm_interest[clone] = 1;
		rw_exit(&pm_pscc_interest_rwlock);
	}
#ifdef DEBUG
	if (pm_debug & PMD_DEREG) {
		printerest("pm_register_watcher after");
		prdirect("pm_register_watcher after");
	}
#endif
}

/*
 * If dip is NULL, process is closing "clone" clean up all its registrations.
 * Otherwise only clean up those for dip because process is just giving up
 * control of a direct device.
 */
void
pm_deregister_watcher(int clone, dev_info_t *dip)
{
	pscc_t	*p, *pn;
	psce_t	*psce;
	int found = 0;

#ifdef DEBUG
	if (pm_debug & PMD_DEREG) {
		prdirect("pm_deregister_watcher before: ");
		printerest("pm_deregister_watcher before: ");
	}
#endif
	if (dip == NULL) {
		rw_enter(&pm_pscc_interest_rwlock, RW_WRITER);
		for (p = pm_pscc_interest; p; p = pn) {
			pn = p->pscc_next;
			if (p->pscc_clone == clone) {
				pm_dequeue_pscc(p, &pm_pscc_interest);
				psce = p->pscc_entries;
				ASSERT(psce->psce_references == 1);
				mutex_destroy(&psce->psce_lock);
				kmem_free(psce->psce_first,
				    sizeof (pm_state_change_t) * PSCCOUNT);
				kmem_free(p, sizeof (*p));
			}
		}
		pm_interest[clone] = 0;
		rw_exit(&pm_pscc_interest_rwlock);
	}
	found = 0;
	rw_enter(&pm_pscc_direct_rwlock, RW_WRITER);
	for (p = pm_pscc_direct; p; p = pn) {
		pn = p->pscc_next;
		if ((dip && p->pscc_dip == dip) ||
		    (dip == NULL && clone == p->pscc_clone)) {
			ASSERT(clone == p->pscc_clone);
			found++;
			/*
			 * Remove from control list
			 */
			pm_dequeue_pscc(p, &pm_pscc_direct);
#ifdef DEBUG
			if (pm_debug & PMD_DPM)
				prdirect("pm_deregister_watcher middle: ");
#endif
			/*
			 * If we're the last reference, free the
			 * entries struct.
			 */
			psce = p->pscc_entries;
			ASSERT(psce);
			if (psce->psce_references == 1) {
				kmem_free(psce->psce_first,
				    PSCCOUNT * sizeof (pm_state_change_t));
				kmem_free(psce, sizeof (*psce));
			} else {
				psce->psce_references--;
			}
			kmem_free(p, sizeof (*p));
		}
	}
	ASSERT(dip == NULL || found);
	rw_exit(&pm_pscc_direct_rwlock);
#ifdef DEBUG
	if (pm_debug & PMD_DEREG) {
		prdirect("pm_deregister_watcher after");
		printerest("pm_deregister_watcher after");
	}
#endif
}


/*
 * Find the entries struct for a given dip in the blocked list, return it locked
 */
psce_t *
pm_psc_dip_to_direct(dev_info_t *dip, pscc_t **psccp)
{
	pscc_t *p;
	psce_t *psce;

	rw_enter(&pm_pscc_direct_rwlock, RW_READER);
	for (p = pm_pscc_direct; p; p = p->pscc_next) {
		if (p->pscc_dip == dip) {
			*psccp = p;
			psce = p->pscc_entries;
			mutex_enter(&psce->psce_lock);
			ASSERT(psce);
			rw_exit(&pm_pscc_direct_rwlock);
			return (psce);
		}
	}
	rw_exit(&pm_pscc_direct_rwlock);
	cmn_err(CE_PANIC, "sunpm: no entry for dip %p in direct list\n",
	    (void *)dip);
	/*NOTREACHED*/
}

/*
 * Find the next entry on the interest list.  We keep a pointer to the item we
 * last returned in the user's cooke.  Returns a locked entries struct.
 */
psce_t *
psc_interest(void **cookie, pscc_t **psccp)
{
	pscc_t *pscc;
	pscc_t **cookiep = (pscc_t **)cookie;

	if (*cookiep == NULL)
		pscc = pm_pscc_interest;
	else
		pscc = (*cookiep)->pscc_next;
	if (pscc) {
		*cookiep = pscc;
		*psccp = pscc;
		mutex_enter(&pscc->pscc_entries->psce_lock);
		return (pscc->pscc_entries);
	} else {
		return (NULL);
	}
}

/*
 * Look up an entry in the blocked list by dip and component
 */
pm_rsvp_t *
pm_rsvp_lookup(dev_info_t *dip, int comp)
{
	pm_rsvp_t *p;
	ASSERT(MUTEX_HELD(&pm_rsvp_lock));
	for (p = pm_blocked_list; p; p = p->pr_next)
		if (p->pr_dip == dip && p->pr_comp == comp) {
			return (p);
		}
	return (NULL);
}

/*
 * Search the indicated list for an entry that matches clone, and return a
 * pointer to it.  To be interesting, the entry must have something ready to
 * be passed up to the controlling process.
 * The returned entry will be locked upon return from this call.
 */
static psce_t *
pm_psc_find_clone(int clone, pscc_t *list, krwlock_t *lock)
{
	pscc_t	*p;
	psce_t	*psce;
	rw_enter(lock, RW_READER);
	for (p = list; p; p = p->pscc_next) {
		if (clone == p->pscc_clone) {
			psce = p->pscc_entries;
			mutex_enter(&psce->psce_lock);
			if (psce->psce_out->size) {
				rw_exit(lock);
				return (psce);
			} else {
				mutex_exit(&psce->psce_lock);
			}
		}
	}
	rw_exit(lock);
	return (NULL);
}

/*
 * Find an entry for a particular clone in the direct list.
 */
psce_t *
pm_psc_clone_to_direct(int clone)
{
	static psce_t *pm_psc_find_clone(int, pscc_t *, krwlock_t *);
	return (pm_psc_find_clone(clone, pm_pscc_direct,
	    &pm_pscc_direct_rwlock));
}

/*
 * Find an entry for a particular clone in the interest list.
 */
psce_t *
pm_psc_clone_to_interest(int clone)
{
	static psce_t *pm_psc_find_clone(int, pscc_t *, krwlock_t *);
	return (pm_psc_find_clone(clone, pm_pscc_interest,
	    &pm_pscc_interest_rwlock));
}

/*
 * Put the given entry at the head of the blocked list
 */
void
pm_enqueue_blocked(pm_rsvp_t *p)
{
	extern pm_rsvp_t *pm_blocked_list;
	ASSERT(MUTEX_HELD(&pm_rsvp_lock));
	ASSERT(p->pr_next == NULL);
	ASSERT(p->pr_prev == NULL);
	if (pm_blocked_list != NULL) {
		p->pr_next = pm_blocked_list;
		ASSERT(pm_blocked_list->pr_prev == NULL);
		pm_blocked_list->pr_prev = p;
		pm_blocked_list = p;
	} else {
		pm_blocked_list = p;
	}
}

/*
 * Remove the given entry from the blocked list
 */
void
pm_dequeue_blocked(pm_rsvp_t *p)
{
	extern pm_rsvp_t *pm_blocked_list;
	ASSERT(MUTEX_HELD(&pm_rsvp_lock));
	if (pm_blocked_list == p) {
		ASSERT(p->pr_prev == NULL);
		if (p->pr_next != NULL)
			p->pr_next->pr_prev = NULL;
		pm_blocked_list = p->pr_next;
	} else {
		ASSERT(p->pr_prev != NULL);
		p->pr_prev->pr_next = p->pr_next;
		if (p->pr_next != NULL)
			p->pr_next->pr_prev = p->pr_prev;
	}
}

/*
 * Remove the given control struct from the given list
 */
static void
pm_dequeue_pscc(pscc_t *p, pscc_t **list)
{
	if (*list == p) {
		ASSERT(p->pscc_prev == NULL);
		if (p->pscc_next != NULL)
			p->pscc_next->pscc_prev = NULL;
		*list = p->pscc_next;
	} else {
		ASSERT(p->pscc_prev != NULL);
		p->pscc_prev->pscc_next = p->pscc_next;
		if (p->pscc_next != NULL)
			p->pscc_next->pscc_prev = p->pscc_prev;
	}
}

/*
 * Stick the control struct specified on the front of the list
 */
static void
pm_enqueue_pscc(pscc_t *p, pscc_t **list)
{
	pscc_t *h;	/* entry at head of list */
	if ((h = *list) == NULL) {
		*list = p;
		ASSERT(p->pscc_next == NULL);
		ASSERT(p->pscc_prev == NULL);
	} else {
		p->pscc_next = h;
		ASSERT(h->pscc_prev == NULL);
		h->pscc_prev = p;
		ASSERT(p->pscc_prev == NULL);
		*list = p;
	}
}

/*
 * Tests outside the lock to see if we should bother to enqueue an entry
 * for any watching process.  If yes, then caller will take the lock and
 * do the full protocol
 */
int
pm_watchers()
{
	return (pm_pscc_direct || pm_pscc_interest);
}

/*
 * Sets every power managed device back to its default threshold
 */
void
pm_all_to_default_thresholds(void)
{
	pm_scan_t	*cur;

	rw_enter(&pm_scan_list_rwlock, RW_READER);
	for (cur = pm_scan_list; cur != NULL; cur = cur->ps_next) {
		PM_LOCK_DIP(cur->ps_dip);
		pm_set_device_threshold(cur->ps_dip, PM_NUMCMPTS(cur->ps_dip),
		    pm_system_idle_threshold, PMC_DEF_THRESH);
		PM_UNLOCK_DIP(cur->ps_dip);
	}
	rw_exit(&pm_scan_list_rwlock);
}

/*
 * Returns the current threshold value (in seconds) for the indicated component
 */
int
pm_current_threshold(dev_info_t *dip, int comp, int *threshp)
{
	if (comp < 0 || comp >= DEVI(dip)->devi_pm_num_components) {
		return (DDI_FAILURE);
	} else {
		*threshp = cur_threshold(dip, comp);
		return (DDI_SUCCESS);
	}
}

/*
 * Lock taken when changing the power level of a component of a device.  It
 * allows the same thread to re-enter with a different component, so that
 * the device's power(9E) code can call back into the framework to change the
 * power level of a component that has a dependency on the one being changed
 * by the original power(9E) call.
 */
void
pm_lock_power(dev_info_t *dip, int comp)
{
#ifdef DEBUG
	char *mask = DEVI(dip)->devi_pm_plockmask;

	ASSERT(mask);
#endif
	/* we assume here that interrupts run as sep. threads */
	if (mutex_owner(&DEVI(dip)->devi_pm_power_lock) != curthread) {
		mutex_enter(&DEVI(dip)->devi_pm_power_lock);
#ifdef DEBUG
		ASSERT((comp == -1) || isclr(mask, comp));
		if (comp != -1) {
			setbit(mask, comp);
		} else {
			DEVI(dip)->devi_pm_allcompcount++;
		}
#endif
		mutex_enter(&e_pm_power_lock);
		if (DEVI(dip)->devi_pm_power_lock_ref == 0)
			DEVI(dip)->devi_pm_power_lock_ref = 1;
		mutex_exit(&e_pm_power_lock);
	} else {
#ifdef DEBUG
		ASSERT((comp == -1) || isclr(mask, comp));
		if (comp != -1) {
			setbit(mask, comp);
		} else {
			DEVI(dip)->devi_pm_allcompcount++;
		}
#endif
		mutex_enter(&e_pm_power_lock);
		DEVI(dip)->devi_pm_power_lock_ref++;
		mutex_exit(&e_pm_power_lock);
	}
}

/*
 * Drop the lock on the component's power state
 */
void
pm_unlock_power(dev_info_t *dip, int comp)
{
#ifdef DEBUG
	char *mask = DEVI(dip)->devi_pm_plockmask;

	ASSERT(mask);
	if (comp != -1) {
		ASSERT(isset(mask, comp));
		clrbit(mask, comp);
	} else {
		DEVI(dip)->devi_pm_allcompcount -= 1;
		ASSERT(DEVI(dip)->devi_pm_allcompcount >= 0);
	}
#endif /* DEBUG */
	mutex_enter(&e_pm_power_lock);
	if (DEVI(dip)->devi_pm_power_lock_ref == 1) {
		DEVI(dip)->devi_pm_power_lock_ref = 0;
		mutex_exit(&DEVI(dip)->devi_pm_power_lock);
	} else {
		DEVI(dip)->devi_pm_power_lock_ref--;
	}
	mutex_exit(&e_pm_power_lock);
}

/*
 * Try to take the lock for changing the power level of a component.  This
 * is used by scan, which will just skip the device if its power level
 * is changing.
 */
int
pm_try_locking_power(dev_info_t *dip, int comp)
{
#ifdef DEBUG
	char *mask = DEVI(dip)->devi_pm_plockmask;

	ASSERT(mask);

	ASSERT(comp == PMC_ALLCOMPS ||
	    comp  < (sizeof (u_longlong_t) *  8) - 1);
#endif
	/* we assume here that interrupts run as sep. threads */
	if (mutex_owner(&DEVI(dip)->devi_pm_power_lock) != curthread) {
		if (mutex_tryenter(&DEVI(dip)->devi_pm_power_lock)) {
#ifdef DEBUG
			if (comp != -1) {
				ASSERT(isclr(mask, comp));
				setbit(mask, comp);
			} else {
				ASSERT(DEVI(dip)->devi_pm_allcompcount == 0);
				DEVI(dip)->devi_pm_allcompcount++;
			}
#endif /* DEBUG */
			mutex_enter(&e_pm_power_lock);
			if (DEVI(dip)->devi_pm_power_lock_ref == 0)
				DEVI(dip)->devi_pm_power_lock_ref = 1;
			mutex_exit(&e_pm_power_lock);
			return (1);
		} else {
			return (0);
		}
	} else {
#ifdef DEBUG
		if (comp != -1) {
			ASSERT(isclr(mask, comp));
			setbit(mask, comp);
		} else {
			ASSERT(DEVI(dip)->devi_pm_allcompcount == 0);
			DEVI(dip)->devi_pm_allcompcount++;
		}
#endif /* DEBUG */
		mutex_enter(&e_pm_power_lock);
		DEVI(dip)->devi_pm_power_lock_ref++;
		mutex_exit(&e_pm_power_lock);
		return (1);
	}
}

#ifdef	DEBUG
/*
 * Returns true if any bit is set in the bitmask
 */
static int
anyset(char *mask, int bits)
{
	int i;
	for (i = 0; i < bits; i++)
		if (isset(mask, i))
			return (1);
	return (0);
}

/*
 * This interface used only in ASSERT.  Returns true if the current thread
 * has the power lock on the component (or on some component if comp arg.
 * is PMC_DONTCARE).
 */
int
pm_iam_locking_power(dev_info_t *dip, int comp)
{
	char *mask = DEVI(dip)->devi_pm_plockmask;
	int ret;

	if (mutex_owner(&DEVI(dip)->devi_pm_power_lock) != curthread) {
		PMD(PMD_FAIL, ("pm_iam_locking %p owns mutex, cur %p\n",
		    (void *)mutex_owner(&DEVI(dip)->devi_pm_power_lock),
		    (void *)curthread))
		return (0);
	}
	if (comp == PMC_DONTCARE) {
		int comps = PM_NUMCMPTS(dip);
		ret = (DEVI(dip)->devi_pm_allcompcount || anyset(mask, comps));
		if (!ret) {
			/*
			 * This will lead to an assertion failure, so might as
			 * well get as much info out of it as possible
			 */
			prom_printf("pm_iam_locking_power DONTCARE all %d, "
			    "anyset %d, comps %d\n",
			    DEVI(dip)->devi_pm_allcompcount,
			    anyset(mask, comps), comps);
			if (pm_debug & PMD_FAIL)
				debug_enter("iam_locking_power");
		}
		return (ret);
	}
	/* If all components held, then any component is held */
	ASSERT(comp >= 0);
	ret = (isset(mask, comp) || DEVI(dip)->devi_pm_allcompcount);
	if (!ret) {
		/*
		 * This will lead to an assertion failure, so might as
		 * well get as much info out of it as possible
		 */
		prom_printf("pm_iam_locking_power comp %d, isset %d, all %d\n",
		    comp, isset(mask, comp), DEVI(dip)->devi_pm_allcompcount);
		if (pm_debug & PMD_FAIL)
			debug_enter("iam_locking_power 2");
	}
	return (ret);
}
#endif

#ifdef DEBUG
typedef struct pmllog {
	char 		*dip;
	int		take;
	int		line;
	char		*file;
	char		*dummy;
} pmll_t;

pmll_t pmll[1024], *pmllp = pmll;
char pmlnbuf[1024*64];
/*
 * Implements a circular log of takers and releasers of locks.
 */
void
pmllog(dev_info_t *dip, int take, int line, char *file)
{
	char *cp;

	mutex_enter(&pmlloglock);
	if (pmllp == &pmll[1023])
		pmllp = pmll;
	else
		pmllp++;
	cp = &pmlnbuf[(pmllp - pmll) * 64];
	(void) sprintf(cp, "%s@%s", PM_NAME(dip), PM_ADDR(dip));
	pmllp->dip = cp;
	pmllp->take = take;
	pmllp->line = line;
	pmllp->file = file;
	mutex_exit(&pmlloglock);
}

/*
 * The following are used only to print out data structures for debugging
 */
void
prpscc(pscc_t *p)
{
	prom_printf("\npscc_t %p: clone %d, dip %p, entries %p, next %p, "
	    "prev %p\n", (void *)p, p->pscc_clone, (void *)p->pscc_dip,
	    (void *)p->pscc_entries, (void *)p->pscc_next,
	    (void *)p->pscc_prev);
	prpsce(p->pscc_entries);
	prom_printf("\n");
}

void
prpsce(psce_t *p)
{
	prom_printf("\tpsce_t %p: first %p, in %p, out %p, last %p, refs %d, "
	    "%s\n", (void *)p, (void *)p->psce_first, (void *)p->psce_in,
	    (void *)p->psce_out, (void *)p->psce_last, p->psce_references,
	    mutex_owner(&p->psce_lock) == NULL ? "free" : "locked");
}

void
prpsc(pm_state_change_t *p)
{
	prom_printf("\t\tpm_state_change_t %p: dip %s, event %x, timestamp %lx,"
	    "comp %d, old %d, new %d, size %lx\n", (void *)p, p->physpath,
	    p->event, p->timestamp, p->component, p->old_level, p->new_level,
	    p->size);
}

void
prrsvp(pm_rsvp_t *p)
{
	prom_printf("pm_rsvp_t %p: dip %p, comp %d, new %x, old %x, "
	    "ret %x, next %p, prev %p\n", (void *)p, (void *)p->pr_dip,
	    p->pr_comp, p->pr_newlevel, p->pr_oldlevel, p->pr_retval,
	    (void *)p->pr_next, (void *)p->pr_prev);
}

void
prdirect(char *caller)
{
	pscc_t *p;
	prom_printf("\n%s pm_pscc_direct %p\n", caller, (void *)pm_pscc_direct);
	for (p = pm_pscc_direct; p; p = p->pscc_next)
		prpscc(p);
	prom_printf("\n");
}

void
printerest(char *caller)
{
	pscc_t *p;
	prom_printf("\npm_pscc_interest %s %p\n", caller,
	    (void *)pm_pscc_interest);
	for (p = pm_pscc_interest; p; p = p->pscc_next)
		prpscc(p);
	prom_printf("\n");
}

void
prblocked(void)
{
	pm_rsvp_t *p;
	prom_printf("pm_blocked_list %p\n", (void *)pm_blocked_list);
	for (p = pm_blocked_list; p; p = p->pr_next)
		prrsvp(p);

}

void
prthresh(char *caller)
{
	struct pm_thresh_rec *rp;
	struct pm_thresh_entry *ep;
	int i, j;

	prom_printf("pm_thresh_head %s %p\n", caller, (void *)pm_thresh_head);
	for (rp = pm_thresh_head; rp; rp = rp->ptr_next) {
		ep = rp->ptr_entries;
		prom_printf("%p: physpath %s, next %p entries %p\n",
		    (void *)rp, rp->ptr_physpath, (void *)rp->ptr_next,
		    (void *)ep);
		for (i = 0; i < rp->ptr_numcomps; i++) {
			prom_printf("	component %d ", i);
			for (j = 0; j < ep->pte_numthresh; j++)
				prom_printf("%x ", ep->pte_thresh[j]);
			prom_printf("\n");
			ep++;
		}
	}
}

void
prkeeps(dev_info_t *dip)
{
	int i;
	pm_info_t *info = PM_GET_PM_INFO(dip);

	ASSERT(info);

	prom_printf(" %s@%s is kept up by %d\n", PM_NAME(dip),
	    PM_ADDR(dip), info->pmi_nkeptupby);
	for (i = 0; i < info->pmi_nkeptupby; i++) {
		if (info->pmi_kupbydips[i])
			prom_printf("	%s@%s\n",
			    PM_NAME(info->pmi_kupbydips[i]),
			    PM_ADDR(info->pmi_kupbydips[i]));
		else
			prom_printf("NULL\n");
	}
	prom_printf(" %s@%s keeps up %d\n", PM_NAME(dip),
	    PM_ADDR(dip), info->pmi_nwekeepup);
	for (i = 0; i < info->pmi_nwekeepup; i++) {
		if (info->pmi_wekeepdips[i])
			prom_printf("	%s@%s\n",
			    PM_NAME(info->pmi_wekeepdips[i]),
			    PM_ADDR(info->pmi_wekeepdips[i]));
		else
			prom_printf("NULL\n");
	}
}

void
prdeps(char *msg)
{
	pm_pdr_t *rp;

	prom_printf("pm_dep_head %s %p\n", msg, (void *)pm_dep_head);
	for (rp = pm_dep_head; rp; rp = rp->pdr_next) {
		prom_printf("%p: keeper %s, kept %s, next %p\n",
		    (void *)rp, rp->pdr_keeper, rp->pdr_kept,
		    (void *)rp->pdr_next);
	}
}
#endif

/*
 * Attempt to apply the thresholds indicated by rp to the node specified by
 * dip.  Held indicates whether we are already holding the dip lock or not.
 */
void
pm_apply_recorded_thresh(dev_info_t *dip, pm_thresh_rec_t *rp, int held)
{
	int i, j;
	int comps = PM_NUMCMPTS(dip);
	struct pm_component *cp;
	pm_pte_t *ep;
	int pm_valid_thresh(dev_info_t *, pm_thresh_rec_t *);

	ASSERT(!held || PM_IAM_LOCKING_DIP(dip));
	PMD(PMD_THRESH, ("%s: %s@%s, rp %p, %s held %d\n",
	    "pm_apply_recorded_thresh", PM_NAME(dip), PM_ADDR(dip),
	    (void *)rp, rp->ptr_physpath, held))
	if (!held)
		PM_LOCK_DIP(dip);
	if (!PM_GET_PM_INFO(dip) || PM_ISBC(dip) || !pm_valid_thresh(dip, rp)) {
		PMD(PMD_FAIL, ("%s: %s@%s PM_GET_PM_INFO %p\n",
		    "pm_apply_recorded_thresh", PM_NAME(dip),
		    PM_ADDR(dip), (void*)PM_GET_PM_INFO(dip)))
		PMD(PMD_FAIL, ("%s: %s@%s PM_ISBC %d\n",
		    "pm_apply_recorded_thresh", PM_NAME(dip),
		    PM_ADDR(dip), PM_ISBC(dip)))
		PMD(PMD_FAIL, ("%s: %s@%s pm_valid_thresh %d\n",
		    "pm_apply_recorded_thresh", PM_NAME(dip),
		    PM_ADDR(dip), pm_valid_thresh(dip, rp)))
		if (!held)
			PM_UNLOCK_DIP(dip);
		return;
	}

	ep = rp->ptr_entries;
	/*
	 * Here we do the special case of a device threshold
	 */
	if (rp->ptr_numcomps == 0) {	/* PM_SET_DEVICE_THRESHOLD product */
		ASSERT(ep && ep->pte_numthresh == 1);
		PMD(PMD_THRESH, ("Setting device threshold for %s@%s to 0x%x\n",
		    PM_NAME(dip), PM_ADDR(dip), ep->pte_thresh[0]))
		pm_set_device_threshold(dip, PM_NUMCMPTS(dip),
		    ep->pte_thresh[0], PMC_DEV_THRESH);
		if (!held)
			PM_UNLOCK_DIP(dip);
		return;
	}
	for (i = 0; i < comps; i++) {
		cp = &DEVI(dip)->devi_pm_components[i];
		for (j = 0; j < ep->pte_numthresh; j++) {
			PMD(PMD_THRESH, ("Setting threshold %d for %s@%s[%d] "
			    "to %x\n", j, PM_NAME(dip),
			    PM_ADDR(dip), i, ep->pte_thresh[j]))
			cp->pmc_comp.pmc_thresh[j + 1] = ep->pte_thresh[j];
		}
		ep++;
	}
	DEVI(dip)->devi_pm_flags &= PMC_THRESH_NONE;
	DEVI(dip)->devi_pm_flags |= PMC_COMP_THRESH;
	if (!held)
		PM_UNLOCK_DIP(dip);
	pm_rescan(0);
}

/*
 * Returns true if the threshold specified by rp could be applied to dip
 * (that is, the number of components and transitions are the same)
 */
int
pm_valid_thresh(dev_info_t *dip, pm_thresh_rec_t *rp)
{
	int comps, i;
	struct pm_component *cp;
	pm_pte_t *ep;

	if (!PM_GET_PM_INFO(dip) || PM_ISBC(dip)) {
		PMD(PMD_ERROR, ("%s no info for %s or BC\n", "pm_valid_thresh",
		    rp->ptr_physpath))
		return (0);
	}
	/*
	 * Special case: we represent the PM_SET_DEVICE_THRESHOLD case by
	 * an entry with numcomps == 0, (since we don't know how many
	 * components there are in advance).  This is always a valid
	 * spec.
	 */
	if (rp->ptr_numcomps == 0) {
		ASSERT(rp->ptr_entries && rp->ptr_entries->pte_numthresh == 1);
		return (1);
	}
	if (rp->ptr_numcomps != (comps = PM_NUMCMPTS(dip))) {
		PMD(PMD_ERROR, ("pm_valid_thresh: component number mismatch "
		    "(dip has %d cmd has %d) for %s@\n", PM_NUMCMPTS(dip),
		    rp->ptr_numcomps, rp->ptr_physpath))
		return (0);
	}
	ep = rp->ptr_entries;
	for (i = 0; i < comps; i++) {
		cp = &DEVI(dip)->devi_pm_components[i];
		if ((ep + i)->pte_numthresh !=
		    cp->pmc_comp.pmc_numlevels - 1) {
			PMD(PMD_ERROR, ("pm_valid_thresh: component %d of %s "
			    "dip has %d thres, cmd %d\n", i, rp->ptr_physpath,
			    cp->pmc_comp.pmc_numlevels - 1,
			    (ep + i)->pte_numthresh))
			return (0);
		}
	}
	return (1);
}

/*
 * Remove any recorded threshold for device physpath
 * We know there will be at most one.
 */
void
pm_unrecord_threshold(char *physpath)
{
	pm_thresh_rec_t *pptr, *ptr;

	rw_enter(&pm_thresh_rwlock, RW_WRITER);
	for (pptr = NULL, ptr = pm_thresh_head; ptr; ptr = ptr->ptr_next) {
		if (strcmp(physpath, ptr->ptr_physpath) == 0) {
			if (pptr) {
				pptr->ptr_next = ptr->ptr_next;
			} else {
				ASSERT(pm_thresh_head == ptr);
				pm_thresh_head = ptr->ptr_next;
			}
			kmem_free(ptr, ptr->ptr_size);
			break;
		}
		pptr = ptr;
	}
	rw_exit(&pm_thresh_rwlock);
}

/*
 * Discard all recorded thresholds.  We are returning to the default pm state.
 */
void
pm_discard_thresholds(void)
{
	pm_thresh_rec_t *rp;
	rw_enter(&pm_thresh_rwlock, RW_WRITER);
	while (pm_thresh_head) {
		rp = pm_thresh_head;
		pm_thresh_head = rp->ptr_next;
		kmem_free(rp, rp->ptr_size);
	}
	rw_exit(&pm_thresh_rwlock);
}

/*
 * Discard all recorded dependencies.  We are returning to the default pm state.
 * Also go through all pm'd devices and reset dependency information.
 */
void
pm_discard_dependencies(void)
{
	pm_pdr_t *rp;
	pm_scan_t	*cur;

	rw_enter(&pm_dep_rwlock, RW_WRITER);
	while (pm_dep_head) {
		rp = pm_dep_head;
		ddi_rele_driver(rp->pdr_major);
		PMD(PMD_KEEPS, ("p_d_d released %s (%s)\n",
		    ddi_major_to_name(rp->pdr_major), rp->pdr_kept))
		pm_dep_head = rp->pdr_next;
		kmem_free(rp, rp->pdr_size);
	}
	rw_exit(&pm_dep_rwlock);
	rw_enter(&pm_scan_list_rwlock, RW_WRITER);
	for (cur = pm_scan_list; cur != NULL; cur = cur->ps_next) {
		pm_free_keeps(cur->ps_dip);
	}
	pm_unresolved_deps = 0;
	rw_exit(&pm_scan_list_rwlock);
}

/*
 * Device dip is being un power managed, it  keeps up count other devices.
 * We need to remove the reference from each device
 * Caller must hold pm_scan_list_rwlock as RW_WRITER;
 */
static void
pm_unkeeps(int count, dev_info_t *dip, dev_info_t **keeps)
{
	int i, j;
	pm_info_t *info;
	int found = 0;

	ASSERT(RW_WRITE_HELD(&pm_scan_list_rwlock));
	PMD(PMD_KEEPS, ("pm_unkeeps(%d, %p, %p)\n", count, (void *)dip,
	    (void *)keeps))
	for (i = 0; i < count; i++, keeps++) {
		/*
		 * data struct is not removed until empty, there may be
		 * some holes in the list.  The count is just the size
		 * of the list
		 */
		if (*keeps == NULL) {
			PMD(PMD_KEEPS, ("skipping hole at %d\n"))
			continue;
		}
		PMD(PMD_KEEPS, ("processing %s@s [%d]\n", PM_NAME(*keeps),
		    PM_ADDR(*keeps), i))
		info = PM_GET_PM_INFO(*keeps);
		ASSERT(info);
		found = 0;
		for (j = 0; j < info->pmi_nkeptupby; j++) {
			if (info->pmi_kupbydips[j] == dip) {
				info->pmi_kupbydips[j] = NULL;
				pm_unresolved_deps++;
				found++;
			}
		}
		ASSERT(found);
		found = 0;
		/*
		 * See if we removed the last one
		 */
		for (j = 0; j < info->pmi_nkeptupby; j++) {
			if (info->pmi_kupbydips[j] != NULL) {
#ifdef DEBUG
				dev_info_t *fdip = info->pmi_kupbydips[j];
				PMD(PMD_KEEPS, ("found %s@%s\n",
				    PM_NAME(fdip), PM_ADDR(fdip)))
#endif
				found++;
			}
		}
		if (!found) {
			kmem_free(info->pmi_kupbydips,
			    info->pmi_nkeptupby * sizeof (int *));
			info->pmi_nkeptupby = 0;
			info->pmi_kupbydips = NULL;
		}
	}
}

/*
 * Device dip is being un power managed, it is kept up by other devices.
 * We need to remove the back reference from each device
 * Caller must hold pm_scan_list_rwlock as RW_WRITER;
 */
static void
pm_unkepts(int count, dev_info_t *dip, dev_info_t **kepts)
{
	int i, j;
	pm_info_t *info;
	int found = 0;

	ASSERT(RW_WRITE_HELD(&pm_scan_list_rwlock));
	PMD(PMD_KEEPS, ("pm_unkepts(%d, %p, %p)\n", count, (void *)dip,
	    (void *)kepts))
	found = 0;
	for (i = 0; i < count; i++, kepts++) {
		/*
		 * data struct is not removed until empty, there may be
		 * some holes in the list.  The count is just the size
		 * of the list
		 */
		if (*kepts == NULL) {
			PMD(PMD_KEEPS, ("skipping hole at %d\n", i))
			continue;
		}
		PMD(PMD_KEEPS, ("processing %s@%s [%d]\n",
		    PM_NAME(*kepts), PM_ADDR(*kepts), i))
		info = PM_GET_PM_INFO(*kepts);
		ASSERT(info);
		for (j = 0; j < info->pmi_nwekeepup; j++) {
			if (info->pmi_wekeepdips[j] == dip) {
				info->pmi_wekeepdips[j] = NULL;
				found++;
			}
		}
		ASSERT(found);
		found = 0;
		/*
		 * See if we removed the last one
		 */
		for (j = 0; j < info->pmi_nwekeepup; j++) {
			if (info->pmi_wekeepdips[j] != NULL) {
#ifdef DEBUG
				dev_info_t *fdip = info->pmi_wekeepdips[j];
				PMD(PMD_KEEPS, ("found %s@%s\n",
				    PM_NAME(fdip), PM_ADDR(fdip)))
#endif
				found++;
			}
		}
		if (!found) {
			kmem_free(info->pmi_wekeepdips,
			    info->pmi_nwekeepup * sizeof (int *));
			info->pmi_nwekeepup = 0;
			info->pmi_wekeepdips = NULL;
		}
	}
}

/*
 * This is the DDI_CTLOPS_POWER handler that is used when there is no ppm
 * driver which has claimed a node.
 * Sets old_power in arg struct.
 */
static int
pm_default_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	_NOTE(ARGUNUSED(dip, result))
	power_req_t *reqp = (power_req_t *)arg;
	int retval;
	dev_info_t *target_dip;
	int new_level, old_level, cmpt;

	/*
	 * The interface for doing the actual power level changes is now
	 * through the DDI_CTLOPS_POWER bus_ctl, so that we can plug in
	 * different platform-specific power control drivers.
	 *
	 * This driver implements the "default" version of this interface.
	 * If no ppm driver has been installed then this interface is called
	 * instead.
	 */
	ASSERT(dip == NULL);
	switch (ctlop) {
	case DDI_CTLOPS_POWER:
		switch (reqp->request_type) {
		case PMR_PPM_SET_POWER:
		{
			PMD(PMD_PPM, ("pm: PPM_SET_POWER"))
			target_dip = reqp->req.ppm_set_power_req.who;
			ASSERT(target_dip == rdip);
			new_level = reqp->req.ppm_set_power_req.new_level;
			cmpt = reqp->req.ppm_set_power_req.cmpt;
			/* pass back old power for the PM_LEVEL_UNKNOWN case */
			old_level = PM_CURPOWER(target_dip, cmpt);
			reqp->req.ppm_set_power_req.old_level = old_level;
			PMD(PMD_PPM, (" %s@%s[%d] %d -> %d",
			    PM_NAME(target_dip), PM_ADDR(target_dip), cmpt,
			    old_level, new_level))
			retval = pm_power(target_dip, cmpt, new_level);
			PMD(PMD_PPM, (" %s\n", (retval == DDI_SUCCESS ? "chd" :
			    "no chg")))
			return (retval);
		}

		case PMR_PPM_ATTACH:
			PMD(PMD_PPM, ("pm: attach %s@%s\n", PM_NAME(rdip),
			    PM_ADDR(rdip)))
			return (DDI_SUCCESS);

		case PMR_PPM_DETACH:
			PMD(PMD_PPM, ("pm: detach %s@%s\n", PM_NAME(rdip),
			    PM_ADDR(rdip)))
			return (DDI_SUCCESS);

		case PMR_PPM_POWER_CHANGE_NOTIFY:
			/*
			 * Nothing for us to do
			 */
			ASSERT(reqp->req.ppm_notify_level_req.who == rdip);
			PMD(PMD_PPM, ("pmd: notify change  %s@%s[%d] %d -> "
			    "%d\n", PM_NAME(reqp->req.ppm_notify_level_req.who),
			    PM_ADDR(reqp->req.ppm_notify_level_req.who),
			    reqp->req.ppm_notify_level_req.cmpt,
			    PM_CURPOWER(reqp->req.ppm_notify_level_req.who,
			    cmpt), reqp->req.ppm_notify_level_req.new_level))
			return (DDI_SUCCESS);

		default:
			PMD(PMD_ERROR, ("pm_default_ctlops: default request "
			    "type!\n"))
			return (DDI_FAILURE);
		}

	default:
		PMD(PMD_ERROR, ("pm_default_ctlops: unknown ctlop\n"))
		return (DDI_FAILURE);
	}
}

/*
 * Set the power level of the indicated device to unknown (if it is not a
 * backwards compatible device), as it has just been resumed, and it won't
 * know if the power was removed or not. Adjust parent's kidsupcnt if necessary.
 */
void
pm_forget_power_level(dev_info_t *dip)
{
	int i;
	pm_info_t *pinfo;
	dev_info_t *pdip;
	int incr = 0;

	if (!PM_ISBC(dip)) {
		if (((pdip = ddi_get_parent(dip)) != NULL) &&
		    ((pinfo = PM_GET_PM_INFO(pdip)) != NULL) &&
		    !(DEVI(pdip)->devi_pm_flags & PMC_WANTS_NOTIFY)) {
			for (i = 0; i < PM_NUMCMPTS(dip); i++) {
				if (PM_CURPOWER(dip, i) == 0)
					incr++;
			}
		}
		if (incr) {
			PM_LOCK_DIP(pdip);
			PMD(PMD_KIDSUP, ("pm_forget %s@%s kuc %d to %d because "
			    "forgetting %s@%s\n", PM_NAME(pdip), PM_ADDR(pdip),
			    pinfo->pmi_kidsupcnt, pinfo->pmi_kidsupcnt + incr,
			    PM_NAME(dip), PM_ADDR(dip)))
			pinfo->pmi_kidsupcnt += incr;
			ASSERT(pinfo->pmi_kidsupcnt >= 0);
			PM_UNLOCK_DIP(pdip);
		}
		for (i = 0; i < DEVI(dip)->devi_pm_num_components; i++) {
			DEVI(dip)->devi_pm_components[i].pmc_cur_pwr =
			    PM_LEVEL_UNKNOWN;
		}
	}
}

/*
 * This function advises the caller whether it should make a power-off
 * transition at this time or not.  If the transition is not advised
 * at this time, the time that the next power-off transition can
 * be made from now is returned through "intervalp" pointer.
 * This function returns:
 *
 *  1  power-off advised
 *  0  power-off not advised, intervalp will point to seconds from
 *	  now that a power-off is advised.  If it is passed the number
 *	  of years that policy specifies the device should last,
 *	  a large number is returned as the time interval.
 *  -1  error
 */
int
pm_trans_check(struct pm_trans_data *datap, time_t *intervalp)
{
	char dbuf[DC_SCSI_MFR_LEN];
	struct pm_scsi_cycles *scp;
	int service_years, service_weeks, full_years;
	time_t now, service_seconds, tdiff;
	time_t within_year, when_allowed;
	char *ptr;
	int lower_bound_cycles, upper_bound_cycles, cycles_allowed;
	int cycles_diff, cycles_over;

	if (datap == NULL)
		return (-1);

	if (datap->format == DC_SCSI_FORMAT) {
		/*
		 * Power cycles of the scsi drives are distributed
		 * over 5 years with the following percentage ratio:
		 *
		 *	30%, 25%, 20%, 15%, and 10%
		 *
		 * The power cycle quota for each year is distributed
		 * linearly through out the year.  The equation for
		 * determining the expected cycles is:
		 *
		 *	e = a * (n / y)
		 *
		 * e = expected cycles
		 * a = allocated cycles for this year
		 * n = number of seconds since beginning of this year
		 * y = number of seconds in a year
		 *
		 * Note that beginning of the year starts the day that
		 * the drive has been put on service.
		 *
		 * If the drive has passed its expected cycles, we
		 * can determine when it can start to power cycle
		 * again to keep it on track to meet the 5-year
		 * life expectancy.  The equation for determining
		 * when to power cycle is:
		 *
		 *	w = y * (c / a)
		 *
		 * w = when it can power cycle again
		 * y = number of seconds in a year
		 * c = current number of cycles
		 * a = allocated cycles for the year
		 *
		 */
		char pcnt[DC_SCSI_NPY] = { 30, 55, 75, 90, 100 };

		scp = &datap->un.scsi_cycles;
		PMD(PMD_TCHECK, ("format=%d, lifemax=%d, ncycles=%d, "
		    "svc_data=%s, flag=%d\n", datap->format, scp->lifemax,
		    scp->ncycles, scp->svc_date, scp->flag))
		if (scp->ncycles < 0 || scp->flag != 0)
			return (-1);

		if (scp->ncycles > scp->lifemax) {
			*intervalp = (LONG_MAX / hz);
			return (0);
		}

		/*
		 * convert service date to time_t
		 */
		bcopy(scp->svc_date, dbuf, DC_SCSI_YEAR_LEN);
		dbuf[DC_SCSI_YEAR_LEN] = '\0';
		ptr = dbuf;
		service_years = stoi(&ptr) - EPOCH_YEAR;
		bcopy(&scp->svc_date[DC_SCSI_YEAR_LEN], dbuf,
		    DC_SCSI_WEEK_LEN);
		dbuf[DC_SCSI_WEEK_LEN] = '\0';

		/*
		 * scsi standard does not specify WW data,
		 * could be (00-51) or (01-52)
		 */
		ptr = dbuf;
		service_weeks = stoi(&ptr);
		if (service_years < 0 ||
		    service_weeks < 0 || service_weeks > 52)
			return (-1);

		/*
		 * calculate service date in seconds-since-epoch,
		 * adding one day for each leap-year.
		 *
		 * (years-since-epoch + 2) fixes integer truncation,
		 * example: (8) leap-years during [1972, 2000]
		 * (2000 - 1970) = 30;  and  (30 + 2) / 4 = 8;
		 */
		service_seconds = (service_years * DC_SPY) +
		    (service_weeks * DC_SPW) +
		    (((service_years + 2) / 4) * DC_SPD);

		now = hrestime.tv_sec;
		if (service_seconds > now)
			return (-1);

		tdiff = now - service_seconds;
		PMD(PMD_TCHECK, ("Age of disk is %ld seconds\n", tdiff))

		/*
		 * NOTE - Leap years are not considered in the calculations
		 * below.
		 */
		full_years = (tdiff / DC_SPY);
		if ((full_years >= DC_SCSI_NPY) &&
		    (scp->ncycles <= scp->lifemax))
			return (1);

		/*
		 * Determine what is the normal cycle usage for the
		 * device at the beginning and the end of this year.
		 */
		lower_bound_cycles = (!full_years) ? 0 :
		    ((scp->lifemax * pcnt[full_years - 1]) / 100);
		upper_bound_cycles = (scp->lifemax * pcnt[full_years]) / 100;

		if (scp->ncycles <= lower_bound_cycles)
			return (1);

		/*
		 * The linear slope that determines how many cycles
		 * are allowed this year is number of seconds
		 * passed this year over total number of seconds in a year.
		 */
		cycles_diff = (upper_bound_cycles - lower_bound_cycles);
		within_year = (tdiff % DC_SPY);
		cycles_allowed = lower_bound_cycles +
		    (((uint64_t)cycles_diff * (uint64_t)within_year) / DC_SPY);
		PMD(PMD_TCHECK, ("Disk has lived %d years and %ld seconds\n",
		    full_years, within_year))
		PMD(PMD_TCHECK, ("Cycles allowed at this time is %d\n",
		    cycles_allowed))

		if (scp->ncycles <= cycles_allowed)
			return (1);

		/*
		 * The transition is not advised now but we can
		 * determine when the next transition can be made.
		 *
		 * Depending on how many cycles the device has been
		 * over-used, we may need to skip years with
		 * different percentage quota in order to determine
		 * when the next transition can be made.
		 */
		cycles_over = (scp->ncycles - lower_bound_cycles);
		while (cycles_over > cycles_diff) {
			full_years++;
			if (full_years >= DC_SCSI_NPY) {
				*intervalp = (LONG_MAX / hz);
				return (0);
			}
			cycles_over -= cycles_diff;
			lower_bound_cycles = upper_bound_cycles;
			upper_bound_cycles =
			    (scp->lifemax * pcnt[full_years]) / 100;
			cycles_diff = (upper_bound_cycles - lower_bound_cycles);
		}

		/*
		 * The linear slope that determines when the next transition
		 * can be made is the relative position of used cycles within a
		 * year over total number of cycles within that year.
		 */
		when_allowed = service_seconds + (full_years * DC_SPY) +
		    (((uint64_t)DC_SPY * (uint64_t)cycles_over) / cycles_diff);
		*intervalp = (when_allowed - now);
		if (*intervalp > (LONG_MAX / hz))
			*intervalp = (LONG_MAX / hz);
		PMD(PMD_TCHECK, ("Next transition can be made in %ld seconds\n",
		    *intervalp))
		return (0);
	}

	return (-1);
}

/*
 * returns true if any component of any device that would keep this device up
 * is at non-zero power level (to recover when device is no longer directly
 * power managed in case direct pm left it powered off in violation of
 * dependencies (i.e. fb vs. keyboard))
 */
int
pm_keeper_is_up(dev_info_t *dip, pm_info_t *info)
{
#ifndef DEBUG
	_NOTE(ARGUNUSED(dip))
#endif
	int i, j;
	dev_info_t **kp;
	PMD(PMD_KEEPS, ("pmiu: %s@%s", PM_NAME(dip), PM_ADDR(dip)))
	if (info->pmi_nkeptupby == 0) {
		PMD(PMD_KEEPS, (" no keepers\n"))
		return (0);
	}
	kp = info->pmi_kupbydips;
	for (j = 0; j < info->pmi_nkeptupby; j++) {
		if (*kp == NULL) {
			PMD(PMD_KEEPS, (" NULL"))
			continue;
		}
		PMD(PMD_KEEPS, (" %s@%s", PM_NAME(*kp), PM_ADDR(*kp)))
		for (i = 1; i < PM_NUMCMPTS(*kp); i++) {
			if (PM_CURPOWER(*kp, i) != 0) {
				PMD(PMD_KEEPS, (" comp %d is up\n", i))
				return (1);
			}
		}
		PMD(PMD_KEEPS, (" all off  "))
		kp++;
	}
	PMD(PMD_KEEPS, ("\n"))
	return (0);
}

/*
 * Bring parent of a node that is about to be probed up to full power, and
 * arrange for it to stay up until pm_pos_probe() is called
 */
void
pm_pre_probe(dev_info_t *pdip, pm_info_t *pinfo)
{
	PM_LOCK_DIP(pdip);
	PMD(PMD_KIDSUP, ("pm_pre_probe %s@%s kuc %d to %d\n", PM_NAME(pdip),
	    PM_ADDR(pdip), pinfo->pmi_kidsupcnt, pinfo->pmi_kidsupcnt + 1))
	ASSERT(pinfo->pmi_kidsupcnt >= 0);
	pinfo->pmi_kidsupcnt += 1;
	PM_UNLOCK_DIP(pdip);
	PM_LOCK_POWER(pdip, PMC_ALLCOMPS);
	(void) pm_all_to_normal(pdip, 1, 0);	/* drops power lock */
}

/*
 * Decrement kidsupcnt so scan can turn the parent back off if it is idle
 */
void
pm_post_probe(dev_info_t *pdip, pm_info_t *pinfo)
{
	PM_LOCK_DIP(pdip);
	ASSERT(pinfo->pmi_kidsupcnt);
	PMD(PMD_KIDSUP, ("pm_post_probe %s@%s kuc %d to %d\n", PM_NAME(pdip),
	    PM_ADDR(pdip), pinfo->pmi_kidsupcnt, pinfo->pmi_kidsupcnt - 1))
	pinfo->pmi_kidsupcnt -= 1;
	ASSERT(pinfo->pmi_kidsupcnt >= 0);
	if (pinfo->pmi_kidsupcnt == 0)
		pm_rescan(1);
	PM_UNLOCK_DIP(pdip);
}

/*
 * Returns true if node has attached children, as evidence by ops pointer
 */
static int
pm_has_children(dev_info_t *dip)
{
	dev_info_t *cdip = ddi_get_child(dip);
	while (cdip && !DEVI(cdip)->devi_ops)
		cdip = ddi_get_next_sibling(cdip);
	return (cdip != NULL);
}
