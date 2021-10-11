/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sfdr_quiesce.c	1.37	99/09/21 SMI"

/*
 * A CPR derivative specifically for starfire
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/ddi.h>
#define	SUNDDI_IMPL
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/devctl.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <nfs/lm.h>
#include <sys/ddi_impldefs.h>
#include <sys/obpdefs.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/callb.h>
#include <sys/clock.h>
#include <sys/x_call.h>
#include <sys/cpuvar.h>
#include <sys/epm.h>
#include <sys/vfs.h>

#include <sys/cpu_sgnblk_defs.h>
#include <sys/dr.h>
#include <sys/sfdr.h>

#include <sys/promif.h>
#include <sys/conf.h>
#include <sys/cyclic.h>

extern void	e_ddi_enter_driver_list(struct devnames *dnp, int *listcnt);
extern void	e_ddi_exit_driver_list(struct devnames *dnp, int listcnt);
extern int	is_pseudo_device(dev_info_t *dip);
extern kmutex_t	class_lock;
extern char	*rt_name;

extern kmutex_t	cpu_lock;
extern sfdr_unsafe_devs_t sfdr_unsafe_devs;

#ifdef notused
static int	sfdr_is_hotpluggable_by_name(char *name);
static int	sfdr_is_hotpluggable_by_major(major_t major);
#endif /* notused */
static int	sfdr_is_real_device(dev_info_t *dip);
static int	sfdr_is_unsafe_major(major_t major);
static int	sfdr_bypass_device(char *dname);
static int	sfdr_return_dev_state(dev_info_t *dip, uint_t *ret_state);
static void	sfdr_check_dip(dev_info_t *dip, int *refcount,
				dr_handle_t *handle);
static int	sfdr_resolve_devname(dev_info_t *dip, char *buffer,
				char *alias);

int sfdr_test_suspend(dr_handle_t *hp, sfdr_ioctl_arg_t *iap);

#define	SR_STATE(srh)			((srh)->sr_suspend_state)
#define	SR_SET_STATE(srh, state)	(SR_STATE((srh)) = (state))
#define	SR_FAILED_DIP(srh)		((srh)->sr_failed_dip)

#define	SR_FLAG_WATCHDOG	0x1
#define	SR_CHECK_FLAG(srh, flag)	((srh)->sr_flags & (flag))
#define	SR_SET_FLAG(srh, flag)		((srh)->sr_flags |= (flag))
#define	SR_CLEAR_FLAG(srh, flag)	((srh)->sr_flags &= ~(flag))

/*
 * XXX
 * This hack will go away before RTI.  Just for testing.
 * List of drivers to bypass when performing a suspend.
 */
static char *sfdr_bypass_list[] = {
	""
};


#if 0
static int	pstate_save;
static uint_t	sfdr_gate[NCPU];
#endif /* 0 */
static int	sfdr_skip_kernel_threads = 1;	/* "TRUE" */
#define		SKIP_SYNC	/* bypass sync ops in sfdr_suspend */


#define	SFDR_CPU_LOOP_MSEC	1000

#if 0
static void
sfdr_grab_cpus(void)
{
	int		i;
	cpuset_t	others;
	extern cpuset_t	cpu_ready_set;
	extern void	sysctrl_freeze(void);
	uint64_t	sfdr_tick_limit;
	uint64_t	sfdr_current_tick;
	uint64_t	sfdr_tick_deadline;
	static fn_t	f = "sfdr_grab_cpus";

	extern u_longlong_t	gettick(void);

	for (i = 0; i < NCPU; i++)
		sfdr_gate[i] = 0;

	/* tell other cpus to go quiet and wait for continue signal */
	others = cpu_ready_set;
	CPUSET_DEL(others, CPU->cpu_id);
	xt_some(others, (xcfunc_t *)sysctrl_freeze, (uint64_t)sfdr_gate,
		(uint64_t)(&sfdr_gate[CPU->cpu_id]));

	sfdr_tick_limit =
		((uint64_t)cpu_tick_freq * SFDR_CPU_LOOP_MSEC) / 1000;

	/* wait for each cpu to check in */
	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(others, i))
			continue;

		/*
		 * Get current tick value and calculate the deadline tick
		 */
		sfdr_current_tick = gettick();
		sfdr_tick_deadline = sfdr_current_tick + sfdr_tick_limit;

		while (sfdr_gate[i] == 0) {
			/* If in panic, we just return */
			if (panicstr)
				break;

			/* Panic the system if cpu not responsed by deadline */
			sfdr_current_tick = gettick();
			if (sfdr_current_tick >= sfdr_tick_deadline) {
				cmn_err(CE_PANIC,
					"sfdr:%s: cpu %d not responding to "
					"quiesce command", f, i);
			}
		}
	}

	/* now even our interrupts are disabled -- really quiet now */
	pstate_save = disable_vec_intr();
}

static void
sfdr_release_cpus(void)
{
	/* let the other cpus go */
	sfdr_gate[CPU->cpu_id] = 1;

	/* restore our interrupts too */
	enable_vec_intr(pstate_save);
}
#endif /* 0 */

static void
sfdr_stop_intr(void)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	kpreempt_disable();
	cyclic_suspend();
}

static void
sfdr_enable_intr(void)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	(void) spl0();
	cyclic_resume();
	kpreempt_enable();
}

sfdr_sr_handle_t *
sfdr_get_sr_handle(dr_handle_t *hp)
{
	sfdr_sr_handle_t *srh;

	srh = GETSTRUCT(sfdr_sr_handle_t, 1);
	srh->sr_dr_handlep = hp;

	return (srh);
}

void
sfdr_release_sr_handle(sfdr_sr_handle_t *srh)
{
	FREESTRUCT(srh, sfdr_sr_handle_t, 1);
}

#ifdef notused
static int
sfdr_is_hotpluggable_by_major(major_t major)
{

	struct devnames *dnp = NULL;
	struct dev_ops *ops;

	int listcnt;
	int rv;

	/* check for a valid major number */
	if (ddi_major_to_name(major) == NULL) {
		PR_QR("sfdr_is_hotpluggable_by_major: invalid major # %d\n",
			major);
		return (-1);
	}

	dnp = &(devnamesp[major]);

	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &listcnt);
	ops = devopsp[major];
	INCR_DEV_OPS_REF(ops);
	rv = DRV_HOTPLUGABLE(ops);
	e_ddi_exit_driver_list(dnp, listcnt);
	DECR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	return (rv);
}
#endif /* notused */

#ifdef notused
static int
sfdr_is_hotpluggable_by_name(char *name)
{
	major_t	major;

	major = (major_t)-1;

	if ((name != (char *)NULL) &&
	    (major = ddi_name_to_major(name))
		!= (major_t)-1)
			return (sfdr_is_hotpluggable_by_major(major));
	else
		return (0);
}
#endif /* notused */

static int
sfdr_is_real_device(dev_info_t *dip)
{
	struct regspec *regbuf = NULL;
	int length = 0;
	int rc;

	if (ddi_get_driver(dip) == NULL)
		return (0);

	if (DEVI(dip)->devi_pm_flags & (PMC_NEEDS_SR|PMC_PARENTAL_SR))
		return (1);
	if (DEVI(dip)->devi_pm_flags & PMC_NO_SR)
		return (0);

	/*
	 * now the general case
	 */
	rc = ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS, "reg",
		(caddr_t)&regbuf, &length);
	ASSERT(rc != DDI_PROP_NO_MEMORY);
	if (rc != DDI_PROP_SUCCESS) {
		return (0);
	} else {
		if ((length > 0) && (regbuf != NULL))
			kmem_free(regbuf, length);
		return (1);
	}
}

static int
sfdr_is_unsafe_major(major_t major)
{
	char	*dname, **cpp;
	int	i, ndevs;

	if ((dname = ddi_major_to_name(major)) == NULL) {
		PR_QR("sfdr_is_unsafe_major: invalid major # %d\n", major);
		return (0);
	}

	ndevs = sfdr_unsafe_devs.ndevs;
	for (i = 0, cpp = sfdr_unsafe_devs.devnames; i < ndevs; i++) {
		if (strcmp(dname, *cpp++) == 0)
			return (1);
	}
	return (0);
}

static int
sfdr_bypass_device(char *dname)
{
	int i;
	char **lname;
	/* check the bypass list */
	for (i = 0, lname = &sfdr_bypass_list[i]; **lname != '\0'; lname++) {
		if (strcmp(dname, sfdr_bypass_list[i++]) == 0)
			return (1);
	}
	return (0);
}

static int
sfdr_resolve_devname(dev_info_t *dip, char *buffer, char *alias)
{
	major_t	devmajor;
	char	*aka, *name;

	*buffer = *alias = 0;

	if (dip == NULL)
		return (-1);

	if ((name = ddi_get_name(dip)) == NULL)
		name = "<null name>";

	aka = name;

	if ((devmajor = ddi_name_to_major(aka)) != -1)
		aka = ddi_major_to_name(devmajor);

	strcpy(buffer, name);

	if (strcmp(name, aka))
		strcpy(alias, aka);
	else
		*alias = 0;

	return (0);
}

static void
sfdr_check_dip(dev_info_t *dip, int *refcount, dr_handle_t *handle)
{
	major_t		major;
	sfdr_error_t	*sep;
	char		*dname;

	if (dip == NULL)
		return;

	sep = HD2MACHERR(handle);
	ASSERT(sep);

	if (!sfdr_is_real_device(dip))
		return;

	dname = ddi_binding_name(dip);

	if (sfdr_bypass_device(dname))
		return;

	if ((DEVI(dip)->devi_binding_name != NULL) &&
		((major = ddi_name_to_major(dname)) != (major_t)-1)) {
		int	rv;
		uint_t	devstate;

		rv = sfdr_return_dev_state(dip, &devstate);
		if ((rv == NDI_SUCCESS) && (devstate & DEVICE_BUSY)) {
			if (refcount)
				(*refcount)++;

			PR_QR("\n  %s (major# %d) is referenced\n",
				dname, major);
		}

		if (sfdr_is_unsafe_major(major) && DDI_CF2(dip)) {
			PR_QR("\n  %s (major# %d) not hotplugable\n",
				dname, major);

			SFDR_SET_ERR_INT(HD2MACHERR(handle),
					SFDR_ERR_UNSAFE, major);
		}
	}
}

void
sfdr_check_devices(dev_info_t *dip, int *refcount, dr_handle_t *handle)
{
	/*
	 * We only care about the given dip and its
	 * descendants.  We do _not_ want to know
	 * about his siblings.  That's taken care of
	 * by the caller, if necessary (during recursion).
	 */
	if (dip == NULL)
		return;

	sfdr_check_dip(dip, refcount, handle);

	for (dip = ddi_get_child(dip); dip; dip = ddi_get_next_sibling(dip))
		sfdr_check_devices(dip, refcount, handle);
}

static int
sfdr_suspend_devices(dev_info_t *dip, sfdr_sr_handle_t *srh)
{
	major_t	major;
	char	*dname;

	for (; dip != NULL; dip = ddi_get_next_sibling(dip)) {
		char	d_name[40], d_alias[40], *d_info;

		if (sfdr_suspend_devices(ddi_get_child(dip), srh))
			return (ENXIO);

		if (!sfdr_is_real_device(dip))
				continue;

		major = (major_t)-1;
		if ((dname = DEVI(dip)->devi_binding_name) != NULL)
			major = ddi_name_to_major(dname);

		if (sfdr_bypass_device(dname)) {
			PR_QR(" bypassed suspend of %s (major# %d)\n", dname,
				major);
			continue;
		}

		if ((d_info = ddi_get_name_addr(dip)) == NULL)
			d_info = "<null>";

		d_name[0] = 0;
		if (sfdr_resolve_devname(dip, d_name, d_alias) == 0) {
			if (d_alias[0] != 0) {
				prom_printf("\tsuspending %s@%s (aka %s)\n",
					d_name, d_info, d_alias);
			} else {
				prom_printf("\tsuspending %s@%s\n",
					d_name, d_info);
			}
		} else {
			prom_printf("\tsuspending %s@%s\n", dname, d_info);
		}

		if (devi_detach(dip, DDI_SUSPEND) != DDI_SUCCESS) {
			prom_printf("\tFAILED to suspend %s@%s\n",
				d_name[0] ? d_name : dname, d_info);
			SFDR_SET_ERR_INT(HD2MACHERR(MACHSRHD2HD(srh)),
					SFDR_ERR_SUSPEND, major);
			SR_FAILED_DIP(srh) = dip;
			return (DDI_FAILURE);
		}
	}

	return (DDI_SUCCESS);
}

static void
sfdr_resume_devices(dev_info_t *start, sfdr_sr_handle_t *srh)
{
	dev_info_t	*dip, *next, *last = NULL;
	major_t		major;
	char		*bn;
	static char	device_path[MAXPATHLEN];

	major = (major_t)-1;

	/* attach in reverse device tree order */
	while (last != start) {
		dip = start;
		next = ddi_get_next_sibling(dip);
		while (next != last && dip != SR_FAILED_DIP(srh)) {
			dip = next;
			next = ddi_get_next_sibling(dip);
		}
		if (dip == SR_FAILED_DIP(srh))
			SR_FAILED_DIP(srh) = NULL;
		else if (sfdr_is_real_device(dip) &&
				SR_FAILED_DIP(srh) == NULL) {

			(void) ddi_pathname(dip, device_path);

			if (DEVI(dip)->devi_binding_name != NULL) {
				bn = ddi_binding_name(dip);
				major = ddi_name_to_major(bn);
			}
			if (!sfdr_bypass_device(bn)) {
				char	d_name[40], d_alias[40], *d_info;

				d_name[0] = 0;
				d_info = ddi_get_name_addr(dip);
				if (d_info == NULL)
					d_info = "<null>";

				if (!sfdr_resolve_devname(dip, d_name,
								d_alias)) {
					if (d_alias[0] != 0) {
						prom_printf("\tresuming "
							"%s@%s (aka %s)\n",
							d_name, d_info,
							d_alias);
					} else {
						prom_printf("\tresuming "
							"%s@%s\n",
							d_name, d_info);
					}
				} else {
					prom_printf("\tresuming %s@%s\n",
						bn, d_info);
				}

				if (devi_attach(dip, DDI_RESUME) !=
							DDI_SUCCESS) {
					/*
					 * Print a console warning,
					 * set an errno of SFDR_ERR_RESUME,
					 * and save the driver major
					 * number in the e_str.
					 */

					prom_printf("\tFAILED to resume "
						"%s@%s\n",
						d_name[0] ? d_name : bn,
						d_info);
					SFDR_SET_ERR_INT(HD2MACHERR(\
					    MACHSRHD2HD(srh)),
						SFDR_ERR_RESUME, major);
				}
			}
		}
		sfdr_resume_devices(ddi_get_child(dip), srh);
		last = dip;
	}
}

static int
sfdr_stop_user_threads(dr_handle_t *handle)
{
	char		*rtName = "RT";
	int		count;
	char		cache_psargs[PSARGSZ];
	kthread_id_t	cache_tp;
	uint_t		cache_t_state;
	int		bailout;
	int		force;
	sfdr_error_t	*sep;
	sfdr_handle_t	*shp = HD2MACHHD(handle);
	static fn_t	f = "sfdr_stop_user_threads";

	extern void add_one_utstop();
	extern void utstop_timedwait(clock_t);
	extern void utstop_init(void);

#define	SFDR_UTSTOP_RETRY	4
#define	SFDR_UTSTOP_WAIT	hz

	id_t		rt_cid;
	kthread_id_t 	tp;
	int		iarr_idx, errsav;

	sep = HD2MACHERR(handle);
	ASSERT(sep);

	force = (shp->sh_iap && (shp->sh_iap->i_flags & SFDR_FLAG_FORCE));

	/* if no force is set, get the RT class id for RT thread test */
	if (!(force)) {
		mutex_enter(&class_lock);
		if (getcidbyname(rtName, &rt_cid) != EINVAL) {
			mutex_exit(&class_lock);
			/* walk the threadlist */
			for (tp = curthread->t_next; tp != curthread;
				tp = tp->t_next) {
				proc_t	*p = ttoproc(tp);

				if (tp->t_cid == rt_cid) {
					/*
					 * this is an RT thread, set error
					 * and save the PID
					 */
					cmn_err(CE_WARN,
						"sfdr:%s: no quiesce while "
						"real-time pid (%d) present",
						f, p->p_pidp->pid_id);
					SFDR_SET_ERR_INT(sep,
						SFDR_ERR_RTTHREAD,
						p->p_pidp->pid_id);
				}
			}
		}
		else
			mutex_exit(&class_lock);

		/* return DDI_FAILURE if RT threads were found */
		if (SFDR_GET_ERR(sep) == SFDR_ERR_RTTHREAD)
			return (DDI_FAILURE);
	}

	/*
	 * save the error int array index in case we need to undo some
	 * stores of pids
	 */

	iarr_idx = SFDR_ERR_INT_IDX(sep);
	errsav = SFDR_GET_ERR(sep);

	utstop_init();

	/* we need to try a few times to get past fork, etc. */
	for (count = 0; count < SFDR_UTSTOP_RETRY; count++) {
		SFDR_ERR_INT_IDX(sep) = iarr_idx;
		SFDR_SET_ERR(sep, errsav);
		/* walk the entire threadlist */
		mutex_enter(&pidlock);
		for (tp = curthread->t_next; tp != curthread; tp = tp->t_next) {
			proc_t *p = ttoproc(tp);

			/* handle kernel threads separately */
			if (p->p_as == &kas || p->p_stat == SZOMB)
				continue;

			mutex_enter(&p->p_lock);
			thread_lock(tp);

			if (tp->t_state == TS_STOPPED) {
				/* add another reason to stop this thread */
				tp->t_schedflag &= ~TS_RESUME;
			} else {
				tp->t_proc_flag |= TP_CHKPT;

				thread_unlock(tp);
				mutex_exit(&p->p_lock);
				add_one_utstop();
				mutex_enter(&p->p_lock);
				thread_lock(tp);

				aston(tp);

				if (tp->t_state == TS_SLEEP &&
				    (tp->t_flag & T_WAKEABLE)) {
					setrun_locked(tp);
				}

			}

			/* grab thread if needed */
			if (tp->t_state == TS_ONPROC && tp->t_cpu != CPU)
				poke_cpu(tp->t_cpu->cpu_id);


			thread_unlock(tp);
			mutex_exit(&p->p_lock);
		}
		mutex_exit(&pidlock);


		/* let everything catch up */
		utstop_timedwait(count * count * SFDR_UTSTOP_WAIT);


		/* now, walk the threadlist again to see if we are done */
		mutex_enter(&pidlock);
		for (tp = curthread->t_next, bailout = 0;
			tp != curthread; tp = tp->t_next) {
			proc_t *p = ttoproc(tp);

			/* handle kernel threads separately */
			if (p->p_as == &kas || p->p_stat == SZOMB)
				continue;

			/* did this thread stop? */
			thread_lock(tp);
			if (!CPR_ISTOPPED(tp)) {

				/* nope, cache the details for later */
				bcopy(p->p_user.u_psargs, cache_psargs,
					sizeof (cache_psargs));
				cache_tp = tp;
				cache_t_state = tp->t_state;
				bailout = 1;
				/*
				 * set an error code into the handle and
				 * save the pid into the e_str array
				 */
				SFDR_SET_ERR_INT(sep, SFDR_ERR_UTHREAD,
						p->p_pidp->pid_id);
			}
			thread_unlock(tp);
		}
		mutex_exit(&pidlock);

		/* were all the threads stopped? */
		if (!bailout)
			break;
	}

	/* were we unable to stop all threads after a few tries? */
	if (bailout) {
		PR_QR("process: %s id: %x state: %x\n",
			cache_psargs, (int)cache_tp, cache_t_state);

		return (ESRCH);
	}

	return (DDI_SUCCESS);
}

static int
sfdr_stop_kernel_threads(dr_handle_t *handle)
{
	caddr_t		name;
	kthread_id_t	tp;

	callb_lock_table();	/* Note: we unlock the table in resume. */
	if (sfdr_skip_kernel_threads) {
		return (DDI_SUCCESS);
	}
	name = callb_execute_class(CB_CL_CPR_DAEMON, CB_CODE_CPR_CHKPT);
	if (name != NULL) {
		SFDR_SET_ERR_STR(HD2MACHERR(handle), SFDR_ERR_KTHREAD, name);
		return (EBUSY);
	}

	/*
	 * Verify that all threads are accounted for
	 */
	mutex_enter(&pidlock);
	for (tp = curthread->t_next; tp != curthread; tp = tp->t_next) {
		proc_t	*p = ttoproc(tp);

		if (p->p_as != &kas)
			continue;

		if (tp->t_flag & T_INTR_THREAD)
			continue;

		if (!callb_is_stopped(tp, &name)) {
			mutex_exit(&pidlock);
			SFDR_SET_ERR_STR(HD2MACHERR(handle),
					SFDR_ERR_KTHREAD, name);
			return (EBUSY);
		}
	}

	mutex_exit(&pidlock);
	return (DDI_SUCCESS);
}

static void
sfdr_start_user_threads(void)
{
	kthread_id_t tp;

	mutex_enter(&pidlock);

	/* walk all threads and release them */
	for (tp = curthread->t_next; tp != curthread; tp = tp->t_next) {
		proc_t *p = ttoproc(tp);

		/* skip kernel threads */
		if (ttoproc(tp)->p_as == &kas)
			continue;

		mutex_enter(&p->p_lock);
		tp->t_proc_flag &= ~TP_CHKPT;
		mutex_exit(&p->p_lock);

		thread_lock(tp);
		if (CPR_ISTOPPED(tp)) {
			/* back on the runq */
			tp->t_schedflag |= TS_RESUME;
			setrun_locked(tp);
		}
		thread_unlock(tp);
	}

	mutex_exit(&pidlock);
}

static void
sfdr_signal_user(int sig)
{
	struct proc *p;

	mutex_enter(&pidlock);

	for (p = practive; p != NULL; p = p->p_next) {
		/* only user threads */
		if (p->p_exec == NULL || p->p_stat == SZOMB ||
		    p == proc_init || p == ttoproc(curthread))
			continue;

		mutex_enter(&p->p_lock);
		sigtoproc(p, NULL, sig);
		mutex_exit(&p->p_lock);
	}

	mutex_exit(&pidlock);

	/* add a bit of delay */
	delay(hz);
}

void
sfdr_resume(sfdr_sr_handle_t *srh)
{
	/*
	 * update the signature block
	 */
	SGN_UPDATE_CPU_OS_RESUME_INPROGRESS_NULL(CPU->cpu_id);

	switch (SR_STATE(srh)) {
	case SFDR_SRSTATE_FULL:

		ASSERT(MUTEX_HELD(&cpu_lock));

		sfdr_enable_intr(); 	/* enable intr & clock */

		/*
		 * release all the other cpus
		 * using start_cpus() vice sfdr_release_cpus()
		 */
		start_cpus();
		mutex_exit(&cpu_lock);

		/*
		 * If we suspended hw watchdog at suspend,
		 * re-enable it now.
		 */

		if (SR_CHECK_FLAG(srh, SR_FLAG_WATCHDOG)) {
			mutex_enter(&tod_lock);
			tod_ops.tod_set_watchdog_timer(
				watchdog_timeout_seconds);
			mutex_exit(&tod_lock);
		}
#if 0
		/*
		 * resume callout
		 */
		(void) callb_execute_class(CB_CL_CPR_RPC, CB_CODE_CPR_RESUME);
		(void) callb_execute_class(CB_CL_CPR_CALLOUT,
			CB_CODE_CPR_RESUME);
#endif

		/* FALLTHROUGH */

	case SFDR_SRSTATE_DRIVER:
		/*
		 * resume drivers
		 */
		sfdr_resume_devices(ddi_root_node(), srh);

		/*
		 * resume the lock manager
		 */
		lm_cprresume();

		/* FALLTHROUGH */

	case SFDR_SRSTATE_DAEMON:
		/*
		 * resume kernel daemons
		 */
		if (!sfdr_skip_kernel_threads) {
			prom_printf("DR: resuming kernel daemons...\n");
			(void) callb_execute_class(CB_CL_CPR_DAEMON,
				CB_CODE_CPR_RESUME);
		}
		callb_unlock_table();

		/* FALLTHROUGH */

	case SFDR_SRSTATE_USER:
		/*
		 * finally, resume user threads
		 */
		prom_printf("DR: resuming user threads...\n");
		sfdr_start_user_threads();
		/* FALLTHROUGH */

	case SFDR_SRSTATE_BEGIN:
	default:
		/*
		 * let those who care know that we've just resumed
		 */
		PR_QR("sending SIGTHAW...\n");
		sfdr_signal_user(SIGTHAW);
		break;
	}

	/*
	 * update the signature block
	 */
	SGN_UPDATE_CPU_OS_RUN_NULL(CPU->cpu_id);

	prom_printf("DR: resume COMPLETED\n");
}

int
sfdr_suspend(sfdr_sr_handle_t *srh)
{
	dr_handle_t	*handle;
	sfdr_handle_t	*shp;
	int force;
	int rc = DDI_SUCCESS;

	handle = MACHSRHD2HD(srh);
	shp = HD2MACHHD(handle);

	force = (shp->sh_iap && (shp->sh_iap->i_flags & SFDR_FLAG_FORCE));

	SFDR_ERR_INT_IDX(HD2MACHERR(handle)) = 0;

	/*
	 * if no force flag, check for unsafe drivers
	 */
	if (!force) {
		PR_QR("\nchecking devices...");
		(void) sfdr_check_devices(ddi_root_node(), NULL, handle);
		if (SFDR_GET_ERR(HD2MACHERR(handle)) == SFDR_ERR_UNSAFE) {
			return (DDI_FAILURE);
		}
		PR_QR("done\n");
	}
	else
		PR_QR("\nsfdr_suspend invoked with force flag");
	/*
	 * update the signature block
	 */
	SGN_UPDATE_CPU_OS_QUIESCE_INPROGRESS_NULL(CPU->cpu_id);

	/*
	 * first, stop all user threads
	 */
	prom_printf("DR: suspending user threads...\n");
	SR_SET_STATE(srh, SFDR_SRSTATE_USER);
	if ((rc = sfdr_stop_user_threads(handle)) != DDI_SUCCESS) {
		sfdr_resume(srh);
		return (rc);
	}

	/*
	 * now stop daemon activities
	 */
	prom_printf("DR: suspending kernel daemons...\n");
	SR_SET_STATE(srh, SFDR_SRSTATE_DAEMON);
	if ((rc = sfdr_stop_kernel_threads(handle)) != DDI_SUCCESS) {
		sfdr_resume(srh);
		return (rc);
	}

#ifndef	SKIP_SYNC
	/*
	 * This sync swap out all user pages
	 */
	vfs_sync(SYNC_ALL);
#endif

	/*
	 * special treatment for lock manager
	 */
	lm_cprsuspend();

#ifndef	SKIP_SYNC
	/*
	 * sync the file system in case we never make it back
	 */
	sync();
#endif

	/*
	 * now suspend drivers
	 */
	prom_printf("DR: suspending drivers...\n");
	SR_SET_STATE(srh, SFDR_SRSTATE_DRIVER);

	if ((rc = sfdr_suspend_devices(ddi_root_node(), srh)) != DDI_SUCCESS) {
		sfdr_resume(srh);
		return (rc);
	}

#if 0
	/*
	 * handle the callout table
	 */
	(void) callb_execute_class(CB_CL_CPR_CALLOUT, CB_CODE_CPR_CHKPT);
#endif

	/*
	 * finally, grab all cpus
	 */
	SR_SET_STATE(srh, SFDR_SRSTATE_FULL);

	/*
	 * use pause_cpus() vice sfdr_grab_cpus()
	 */

	mutex_enter(&cpu_lock);
	pause_cpus(NULL);
	sfdr_stop_intr();

	/*
	 * if watchdog was activated, disable it
	 */
	if (watchdog_activated) {
		mutex_enter(&tod_lock);
		tod_ops.tod_clear_watchdog_timer();
		mutex_exit(&tod_lock);
		SR_SET_FLAG(srh, SR_FLAG_WATCHDOG);
	} else {
		SR_CLEAR_FLAG(srh, SR_FLAG_WATCHDOG);
	}

	/*
	 * update the signature block
	 */
	SGN_UPDATE_CPU_OS_QUIESCED_NULL(CPU->cpu_id);

	return (rc);
}

/*ARGSUSED*/
int
sfdr_test_suspend(dr_handle_t *hp, sfdr_ioctl_arg_t *iap)
{
	sfdr_error_t    *sep;
	sfdr_sr_handle_t *srh;
	int		err;
	uint_t		psmerr;
	static fn_t	f = "sfdr_test_suspend";

	PR_QR("%s...\n", f);

	sep = HD2MACHERR(hp);

	SFDR_SET_ERR(sep, 0);	/* handles get re-used.. */

	srh = sfdr_get_sr_handle(hp);
	if ((err = sfdr_suspend(srh)) == DDI_SUCCESS) {
		sfdr_resume(srh);
		if ((psmerr = SFDR_GET_ERR(sep)) != 0) {
			PR_QR("%s: error on sfdr_resume()", f);
			switch (psmerr) {
				int m;
			case SFDR_ERR_RESUME:
				PR_QR("Couldn't resume devices:");
				for (m = 1; m <= SFDR_ERR_INT_IDX(sep);
					m++) {
					PR_ALL(" %d",
					    SFDR_GET_ERR_INT(sep, m));
				}
				PR_ALL(" \n");
				break;

			case SFDR_ERR_KTHREAD:
				PR_ALL("psmerr is SFDR_ERR_KTHREAD\n");
				break;
			default:
				PR_ALL("Resume error unknown = %d\n",
					psmerr);
				break;
			}
		}
	} else {
		PR_ALL("%s: sfdr_suspend() failed, err = 0x%x\n",
			f, err);
		psmerr = SFDR_GET_ERR(sep);
		switch (psmerr) {
				int m;

		case SFDR_ERR_UNSAFE:
			PR_ALL("Unsafe devices (major #):");
			for (m = 1; m <= SFDR_ERR_INT_IDX(sep); m++) {
				PR_ALL(" %d", SFDR_GET_ERR_INT(sep, m));
			}
			PR_ALL(" \n");
			break;

		case SFDR_ERR_RTTHREAD:
			PR_ALL("RT threads (PIDs):");
			for (m = 1; m <= SFDR_ERR_INT_IDX(sep); m++) {
				PR_ALL(" %d", SFDR_GET_ERR_INT(sep, m));
			}
			PR_ALL(" \n");
			break;

		case SFDR_ERR_UTHREAD:
			PR_ALL("User threads (PIDs):");
			for (m = 1; m <= SFDR_ERR_INT_IDX(sep); m++) {
				PR_ALL(" %d", SFDR_GET_ERR_INT(sep, m));
			}
			PR_ALL(" \n");
			break;

		case SFDR_ERR_SUSPEND:
			PR_ALL("Non-suspendable devices (major #):");
			for (m = 1; m <= SFDR_ERR_INT_IDX(sep); m++) {
				PR_ALL(" %d", SFDR_GET_ERR_INT(sep, m));
			}
			PR_ALL(" \n");
			break;

		case SFDR_ERR_RESUME:
			PR_ALL("Could not resume devices (major #):");
			for (m = 1; m <= SFDR_ERR_INT_IDX(sep); m++) {
				PR_ALL(" %d", SFDR_GET_ERR_INT(sep, m));
			}
			PR_ALL(" \n");
			break;

		case SFDR_ERR_KTHREAD:
			PR_ALL("psmerr is SFDR_ERR_KTHREAD\n");
			break;
		default:
			PR_ALL("Unknown error psmerr = %d\n", psmerr);
			break;
		}
	}
	sfdr_release_sr_handle(srh);


	return (0);
}

/*
 * XXX - This is kludgy.  We had to dup the ndi_dc_return_dev_state()
 *	 function because it performs a copyout, so when called from
 *	 the kernel we get a NDI_FAULT.
 */
int
sfdr_return_dev_state(dev_info_t *dip, uint_t *ret_state)
{
	uint_t devstate = 0;
	struct devnames *dnp;
#if 0
	struct dev_ops	*ops;
#endif /* 0 */
	major_t maj;
	int listcnt;

	if ((dip == NULL) || (ret_state == NULL) ||
				(ddi_binding_name(dip) == NULL))
		return (NDI_FAILURE);

	maj = ddi_name_to_major(ddi_binding_name(dip));

	if (maj == (major_t)-1)
		return (NDI_FAILURE);

	dnp = &(devnamesp[maj]);
	LOCK_DEV_OPS(&(dnp->dn_lock));
	e_ddi_enter_driver_list(dnp, &listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
	mutex_enter(&(DEVI(dip)->devi_lock));
	if (DEVI_IS_DEVICE_OFFLINE(dip)) {
		devstate = DEVICE_OFFLINE;
	} else {
		if (DEVI_IS_DEVICE_DOWN(dip)) {
			devstate = DEVICE_DOWN;
		} else {
			devstate = DEVICE_ONLINE;
			if (devi_stillreferenced(dip) == DEVI_REFERENCED)
				devstate |= DEVICE_BUSY;
		}
	}
	mutex_exit(&(DEVI(dip)->devi_lock));
	LOCK_DEV_OPS(&(dnp->dn_lock));

#if 0
	ops = devopsp[maj];
	if ((DRV_UNLOADABLE(ops) == 0) && (devstate == DEVICE_ONLINE))
		devstate |= DEVICE_BUSY;
#endif /* 0 */

	e_ddi_exit_driver_list(dnp, listcnt);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	*ret_state = devstate;

	return (NDI_SUCCESS);
}
