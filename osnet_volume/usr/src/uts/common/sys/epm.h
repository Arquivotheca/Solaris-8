/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_EPM_H
#define	_SYS_EPM_H

#pragma ident	"@(#)epm.h	1.16	99/11/15 SMI"

#include <sys/pm.h>
#include <sys/dditypes.h>
#include <sys/ddi_impldefs.h>
#ifdef	DEBUG
#include <sys/promif.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * epm.h:	Function prototypes and data structs for kernel pm functions.
 */

void e_pm_props(dev_info_t *);
int e_new_pm_props(dev_info_t *);

/*
 * Values used by e_pm_props and friends, found in devi_pm_flags
 */
#define	PMC_NEEDS_SR		0x00001	/* do suspend/resume despite no "reg" */
#define	PMC_NO_SR		0x00002	/* don't suspend/resume despite "reg" */
#define	PMC_PARENTAL_SR		0x00004	/* call up tree to suspend/resume */
#define	PMC_WANTS_NOTIFY	0x00008	/* notify if child pwr level changes */
#define	PMC_NOTIFY_PARENT	0x00010	/* optimization, notify parent */
#define	PMC_BC			0x00020	/* no pm-components, backwards compat */
#define	PMC_COMPONENTS_DONE	0x00040 /* parsed pm-components */
#define	PMC_COMPONENTS_FAILED	0x00080 /* failed parsing pm-components */
#define	PMC_SUSPENDED		0x00100 /* device has been suspended */
#define	PMC_DEF_THRESH		0x00200 /* thresholds are default */
#define	PMC_DEV_THRESH		0x00400 /* SET_THRESHOLD ioctl seen */
#define	PMC_COMP_THRESH		0x00800 /* relative threshold set */
#define	PMC_NEXDEF_THRESH	0x01000 /* relative threshold set for nexus */
#define	PMC_NOPMKID		0x02000 /* non-pm'd child of pm'd parent */

#define	PMC_THRESH_ALL	(PMC_DEF_THRESH | PMC_DEV_THRESH | \
    PMC_COMP_THRESH | PMC_NEXDEF_THRESH)
#define	PMC_THRESH_NONE	~(PMC_THRESH_ALL)

/*
 * Structure used for linked list of auto power managed devices (pm_scan_list).
 * Protected by pm_scan_rwlock.
 */

typedef struct pm_scan {
	dev_info_t	*ps_dip;
	struct pm_scan	*ps_next;
	int		ps_clone;
	size_t		ps_size;
	char		*ps_path;	/* for looking up recorded specs */
} pm_scan_t;

/*
 * Power management component definitions, used for tracking idleness of
 * devices.  An array of these hangs off the devi_pm_components member of the
 * dev_info struct (if initialized by driver and/or auto-pm)
 * The array of these structs is followed in the same kmem_zalloc'd chunk by
 * the names pointed to by the structs.
 */

/*
 * This (sub-)struct contains all the info extracted from the pm-components
 * property for each component (name of component, names and values of power
 * levels supported).  It is in a separate structure to allow it to be handled
 * as a struct assignment.
 */
typedef struct pm_comp {
	char 	*pmc_name;		/* name of component */
	int	pmc_numlevels;		/* number of power levels supported */
	int	*pmc_lvals;		/* numerical values of levels */
	int	*pmc_thresh;		/* thresholds in secs, last INT_MAX */
	char	**pmc_lnames;		/* human readable names of levels */
	/*
	 * This part is just bookkeeping for the storage space involved above
	 * used for copying and freeing the struct members.  This because C
	 * is really an assembler at heart.
	 */
	size_t	pmc_name_sz;		/* size of name string		*/
	char	*pmc_lname_buf;		/* buffer holding *pmc_lnames	*/
	size_t	pmc_lnames_sz;		/* total size of pmc_lname_buf	*/
} pm_comp_t;

/*
 * Here we have the rest of what we need to know about a component.
 */
typedef struct pm_component {
	uint_t pmc_flags;		/* flags this component */
	uint_t pmc_busycount;		/* for nesting busy calls */
	time_t pmc_timestamp;		/* timestamp */
	uint_t pmc_norm_pwr;		/* normal power index (or value) */
	int pmc_cur_pwr;		/* current power index (or value)  */
	pm_comp_t pmc_comp;		/* component description */
} pm_component_t;

/*
 * All members of this struct are protected by PM_LOCK_DIP(dip).
 *
 * kidsupcnt counts (the number of components of new-style children at non-zero
 * level (unknown counts as non-zero)) + (the number of old-style children with
 * component 0 at non-zero level) for parents that have not asked for
 * notifcation.  When kidsupcnt is 0 for a nexus node, then pm scans it,
 * otherwise it leaves it alone.
 * Parents that ask for notification always get get scanned,
 * so we keep their kidsupcnt at zero.
 */
typedef struct pm_info {
	uint_t		pmi_dev_pm_state; /* PM state of a device */
	int		pmi_nwekeepup;	/* number of devices we keep up */
	dev_info_t	**pmi_wekeepdips; /* array of dev_info_ts of them */
	int		pmi_nkeptupby;	/* number devs that keep this one up */
	dev_info_t	**pmi_kupbydips; /* array of dips that keep us up */
	int		pmi_kidsupcnt; /* count of kids who are keeping us up */
	int		pmi_warned;	/* message has been printed */
	int		pmi_loopwarned;	/* message has been printed */
	int		pmi_clone;	/* owner for direct pm'd devs */
} pm_info_t;

/*
 * The power request struct uses for the DDI_CTLOPS_POWER busctl.
 */
typedef enum {
	PMR_SET_POWER = 1,		/* called ddi_power (obsolete)	*/
	PMR_SUSPEND,			/* parental suspend		*/
	PMR_RESUME,			/* parental resume		*/
	PMR_PRE_SET_POWER,		/* parent's "pre" notification	*/
	PMR_POST_SET_POWER,		/* parent's "post" notification	*/
	PMR_PPM_SET_POWER,		/* platform pm set power	*/
	PMR_PPM_ATTACH,			/* platform pm attach notify	*/
	PMR_PPM_DETACH,			/* platform pm detach notify	*/
	PMR_PPM_POWER_CHANGE_NOTIFY,	/* ppm level change notify	*/
	PMR_REPORT_PMCAP,		/* report pm capability		*/
	PMR_CHANGED_POWER		/* parent's power_has_changed notif. */
} pm_request_type;

typedef struct power_req {
	pm_request_type request_type;
	union req {
		struct set_power_req {
			dev_info_t	*who;
			int		cmpt;
			int		level;
		} set_power_req;
		struct suspend_req {
			dev_info_t	*who;
			ddi_detach_cmd_t cmd;
		} suspend_req;
		struct resume_req {
			dev_info_t	*who;
			ddi_attach_cmd_t cmd;
		} resume_req;
		struct pre_set_power_req {
			dev_info_t	*who;
			int		cmpt;
			int		old_level;
			int		new_level;
		} pre_set_power_req;
		struct post_set_power_req {
			dev_info_t	*who;
			int		cmpt;
			int		old_level;
			int		new_level;
			int		result;		/* driver's return */
		} post_set_power_req;
		struct ppm_set_power_req {
			dev_info_t	*who;
			int		cmpt;
			int		old_level;
			int		new_level;
		} ppm_set_power_req;
		struct ppm_notify_level_req {
			dev_info_t	*who;
			int		cmpt;
			int		old_level;
			int		new_level;
		} ppm_notify_level_req;
		struct report_pmcap_req {
			dev_info_t	*who;
			int		cap;
			void 		*arg;
		} report_pmcap_req;
		struct changed_power_req {
			dev_info_t	*who;
			int		cmpt;
			int		old_level;
			int		new_level;
			int		result;
		} changed_power_req;
	} req;
} power_req_t;


/*
 * This struct records one dependency for a device
 * pdr_size includes size of strings.
 */
typedef struct pm_dep_rec {
	char *pdr_keeper;		/* physpath of device keeping up */
	char *pdr_kept;			/* physpath of device kept up */
	struct pm_dep_rec *pdr_next;	/* next kept up device */
	size_t pdr_size;		/* size to kmem_free */
	major_t pdr_major;		/* major of kept driver */
} pm_pdr_t;

/*
 * This struct records threshold information about a single component
 */
typedef struct pm_thresh_entry {
	int pte_numthresh;
	int *pte_thresh;
} pm_pte_t;

/*
 * Note that this header and its array of entry structs with their arrays
 * of thresholds and string storage for physpath are all kmem_alloced in one
 * chunk for easy freeing ptr_size is the size of that chunk
 */

typedef struct pm_thresh_rec {
	char			*ptr_physpath;	/* identifies node */
	struct pm_thresh_rec	*ptr_next;
	int			ptr_numcomps;	/* number of components */
	size_t			ptr_size;	/* total size for kmem_free */
	pm_pte_t 		*ptr_entries;
} pm_thresh_rec_t;

/*
 * The following state bits are used to denote a device's pm state (in
 * pmi_dev_pm_state).
 * PM_SCAN and (PM_DIRECT_OLD or PM_DIRECT_NEW) can be set,  PM_DIRECT_* takes
 * precedence, i.e. device with PM_DIRECT_* set will be ignored by scan()
 */

/*
 * a direct-pm device of the old (PM_DISABLE_AUTOPM) style
 */
#define	PM_DIRECT_OLD	0x1

/*
 * device will be processed by scan unless PM_DIRECT_OLD or PM_DIRECT_NEW
 * is set
 */
#define	PM_SCAN		0x2

/*
 * A device which had PM_SCAN set is detaching.  If detach succeeds we'll
 * unmanage it, else we'll continue to manage it if detach fails
 */
#define	PM_DETACHING	0x4	/* an auto-pm device is detaching */

/*
 * A direct pm device of the new (PM_DIRECT_PM) style
*/
#define	PM_DIRECT_NEW	0x8

/*
 * A BC device had scan stopped due to detaching
 */
#define	PM_SCAN_DEFERRED	0x10

#define	PM_GET_PM_INFO(dip) (DEVI(dip)->devi_pm_info)

/*
 * Returns true if the device specified by dip is directly power managed
 */
#define	PM_ISDIRECT(dip) \
	(((pm_info_t *)PM_GET_PM_INFO(dip))->pmi_dev_pm_state & \
	(PM_DIRECT_NEW | PM_DIRECT_OLD))

/*
 * Returns true if the device specified by dip is an old node for which we
 * provide backwards compatible behavior (e.g. no pm-components property).
 */
#define	PM_ISBC(dip) (DEVI(dip)->devi_pm_flags & PMC_BC)

#ifdef	DEBUG
/*
 * Flags passed to PMD to enable debug printfs.  If the same flag is set in
 * pm_debug below then the message is printed.  The most generally useful
 * ones are the first 3 or 4.
 */
#define	PMD_ERROR	0x0000001
#define	PMD_FAIL	0x0000002
#define	PMD_IOCTL	0x0000004
#define	PMD_SCAN	0x0000008
#define	PMD_RESCAN	0x0000010
#define	PMD_REMINFO	0x0000020
#define	PMD_NAMETODIP	0x0000040
#define	PMD_CLOSE	0x0000080
#define	PMD_DIN		0x0000100	/* Dev Is Needed */
#define	PMD_PMC		0x0000200	/* for testing with sun4m pmc driver */
#define	PMD_PPM		0x0000400
#define	PMD_DEP		0x0000800	/* dependency processing */
#define	PMD_IDLEDOWN	0x0001000
#define	PMD_SET		0x0002000
#define	PMD_BRING	0x0004000
#define	PMD_ALLNORM	0x0008000
#define	PMD_REMDEV	0x0010000
#define	PMD_LEVEL	0x0020000
#define	PMD_THRESH	0x0040000
#define	PMD_DPM		0x0080000	/* Direct Power Management */
#define	PMD_NORM	0x0100000
#define	PMD_STATS	0x0200000
#define	PMD_DEREG	0x0400000
#define	PMD_KEEPS	0x0800000
#define	PMD_KIDSUP	0x1000000
#define	PMD_TCHECK	0x2000000

extern uint_t	pm_debug;

#define	PMD(level, arglist) {if (pm_debug & (level)) prom_printf arglist; }
#else
#define	PMD(level, arglist)
#endif

extern void	pm_detaching(dev_info_t *);
extern void	pm_detach_failed(dev_info_t *);
extern int	pm_start(dev_info_t *);
extern int	pm_stop(dev_info_t *);
extern int	pm_power(dev_info_t *, int, int);
extern int	pm_manage(dev_info_t *, int);
extern int	pm_unmanage(dev_info_t *, int);
extern int	pm_rem_info(dev_info_t *, int);
extern int	pm_enqueue_scan(dev_info_t *);
extern void	pm_walk_devs(pm_scan_t *, void (*)(dev_info_t *));
extern int	pm_get_norm_pwrs(dev_info_t *, int **, size_t *);
extern int	pm_set_power(dev_info_t *, int, int, int, pm_info_t *, int);
extern void	pm_rescan(int);
extern dev_info_t *pm_name_to_dip(char *, int);
extern int	pm_power_up(dev_info_t *, int, int, int, pm_info_t *);
extern void	*pm_powerup_console(void);
extern void	pm_restore_console(void *);
extern		int pm_default_idle_threshold;
extern void	pm_set_device_threshold(dev_info_t *, int, int, int);
extern int	pm_valid_power(dev_info_t *, int, int);

extern void	pm_lock_power(dev_info_t *, int);
extern void	pm_unlock_power(dev_info_t *, int);
extern int	pm_try_locking_power(dev_info_t *, int);
extern int	pm_iam_locking_power(dev_info_t *, int);
extern int	pm_isbc(dev_info_t *dip);
extern int	pm_isdirect(dev_info_t *dip);

/*
 * See PM LOCKING discussion in common/os/sunpm.c for details
 */
#define	PMC_ALLCOMPS -1
#define	PMC_DONTCARE -1
#ifdef DEBUG
extern void pmllog(dev_info_t *, int, int, char *);
/*
 * The debug versions of these routines log attempts to take locks and
 * successes as well.  PM_IAM_LOCKING_POWER is used only in ASSERT().
 */
#define	PM_LOCK_DIP(dip)	{if (mutex_tryenter(&DEVI(dip)->devi_pm_lock)) \
    pmllog(dip, 2, __LINE__, __FILE__);\
    else { pmllog(dip, 1, __LINE__, __FILE__); \
    mutex_enter(&DEVI(dip)->devi_pm_lock); } }
#define	PM_UNLOCK_DIP(dip)	{pmllog(dip, 0, __LINE__, __FILE__); \
    mutex_exit(&DEVI(dip)->devi_pm_lock); }
#define	PM_IAM_LOCKING_POWER(dip, comp)	pm_iam_locking_power(dip, comp)
#else
#define	PM_LOCK_DIP(dip)	mutex_enter(&DEVI(dip)->devi_pm_lock)
#define	PM_UNLOCK_DIP(dip)	mutex_exit(&DEVI(dip)->devi_pm_lock)
#endif

/*
 * These are the same DEBUG or not
 */
#define	PM_LOCK_BUSY(dip)	mutex_enter(&DEVI(dip)->devi_pm_busy_lock)
#define	PM_UNLOCK_BUSY(dip)	mutex_exit(&DEVI(dip)->devi_pm_busy_lock)
#define	PM_LOCK_POWER(dip, comp)	pm_lock_power(dip, comp)
#define	PM_UNLOCK_POWER(dip, comp)	pm_unlock_power(dip, comp)
#define	PM_TRY_LOCK_POWER(dip, comp)	pm_try_locking_power(dip, comp)
#define	PM_IAM_LOCKING_DIP(dip)	(mutex_owned(&DEVI(dip)->devi_pm_lock))

#define	PM_DEFAULT_SYS_IDLENESS	1800	/* 30 minutes */

/*
 * Codes put into the pr_retval field of pm_rsvp_t that tell pm_block()
 * how to proceed
 */
#define	PMP_SUCCEED	0x1	/* return success, the process did it */
#define	PMP_FAIL	0x2	/* return fail, process did something else */
#define	PMP_RELEASE	0x3	/* let it go, the process has lost interest */
				/* also arg to pm_proceed to signal this */
/*
 * Arg passed to pm_proceed that results in PMP_SUCCEED or PMP_FAIL being set
 * in pr_retval depending on what is pending
 */
#define	PMP_SETPOWER	0x4

#define	PM_MAX_CLONE	256

typedef struct pm_rsvp {
	dev_info_t	*pr_dip;
	int		pr_comp;
	int		pr_newlevel;
	int		pr_oldlevel;
	kcondvar_t	pr_cv;		/* a place to sleep */
	int		pr_retval;	/* what to do when you wake up */
	struct pm_rsvp	*pr_next;
	struct pm_rsvp	*pr_prev;
} pm_rsvp_t;

typedef struct psce {	/* pm_state_change_entries */
	struct pm_state_change		*psce_first;
	struct pm_state_change		*psce_in;
	struct pm_state_change		*psce_out;
	struct pm_state_change		*psce_last;
	int				psce_overruns;
	int				psce_references;
	kmutex_t			psce_lock;
} psce_t;

typedef struct pscc {			/* pm_state_change_control */
	int		pscc_clone;
	dev_info_t	*pscc_dip;
	psce_t		*pscc_entries;
	struct pscc	*pscc_next;
	struct pscc	*pscc_prev;
} pscc_t;

#define	PSCCOUNT 128	/* number of state change entries kept per process */

/*
 * These defines are used by pm_trans_check() to calculate time.
 * Mostly copied from "tzfile.h".
 */
#define	EPOCH_YEAR		1970
#define	SECSPERMIN		60
#define	MINSPERHOUR		60
#define	HOURSPERDAY		24
#define	DAYSPERWEEK		7
#define	DAYSPERNYEAR		365
#define	SECSPERHOUR		(SECSPERMIN * MINSPERHOUR)
#define	SECSPERDAY		(SECSPERHOUR * HOURSPERDAY)
#define	DC_SPY			(SECSPERDAY * DAYSPERNYEAR)
#define	DC_SPW			(SECSPERDAY * DAYSPERWEEK)
#define	DC_SPD			SECSPERDAY

#define	DC_SCSI_YEAR_LEN	4		/* YYYY */
#define	DC_SCSI_WEEK_LEN	2		/* WW */
#define	DC_SCSI_NPY		5		/* # power-cycle years */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_EPM_H */
