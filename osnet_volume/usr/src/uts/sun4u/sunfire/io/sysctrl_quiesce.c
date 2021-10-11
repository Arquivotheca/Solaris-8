/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sysctrl_quiesce.c	1.18	99/08/17 SMI"

/*
 * This workaround inhibits prom_printf after the cpus are grabbed.
 * This can be removed when 4154263 is corrected.
 */
#define	Bug_4154263

/*
 * A CPR derivative specifically for sunfire
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/ddi.h>
#define	SUNDDI_IMPL
#include <sys/sunddi.h>
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
#include <sys/fhc.h>
#include <sys/sysctrl.h>
#include <sys/promif.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/cyclic.h>

static enum sysctrl_suspend_state {
	SYSC_STATE_BEGIN = 0,
	SYSC_STATE_USER,
	SYSC_STATE_DAEMON,
	SYSC_STATE_DRIVER,
	SYSC_STATE_FULL } suspend_state;

static int	pstate_save;
static uint_t	sysctrl_gate[NCPU];
int	sysctrl_quiesce_debug = FALSE;
static int	sysctrl_skip_kernel_threads = TRUE;

static int	sysc_watchdog_suspended;

extern int	sysctrl_enable_detach_suspend;
static int	sysc_lastval;

#define	DEBUGP(p) { if (sysctrl_quiesce_debug) p; }
#define	errp	prom_printf

#define	SYSC_CPU_LOOP_MSEC	1000

static void
sysctrl_grab_cpus(void)
{
	int		i;
	cpuset_t	others;
	extern cpuset_t	cpu_ready_set;
	extern void	sysctrl_freeze(void);
	uint64_t	sysc_tick_limit;
	uint64_t	sysc_current_tick;
	uint64_t	sysc_tick_deadline;

	extern u_longlong_t	gettick(void);

	for (i = 0; i < NCPU; i++)
		sysctrl_gate[i] = 0;

	/* tell other cpus to go quiet and wait for continue signal */
	others = cpu_ready_set;
	CPUSET_DEL(others, CPU->cpu_id);
	xt_some(others, (xcfunc_t *)sysctrl_freeze, (uint64_t)sysctrl_gate,
		(uint64_t)(&sysctrl_gate[CPU->cpu_id]));

	sysc_tick_limit =
		((uint64_t)cpu_tick_freq * SYSC_CPU_LOOP_MSEC) / 1000;

	/* wait for each cpu to check in */
	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(others, i))
			continue;

		/*
		 * Get current tick value and calculate the deadline tick
		 */
		sysc_current_tick = gettick();
		sysc_tick_deadline = sysc_current_tick + sysc_tick_limit;

		while (sysctrl_gate[i] == 0) {
			/* If in panic, we just return */
			if (panicstr)
				break;

			/* Panic the system if cpu not responsed by deadline */
			sysc_current_tick = gettick();
			if (sysc_current_tick >= sysc_tick_deadline) {
			    cmn_err(CE_PANIC, "sysctrl: cpu %d not "
				"responding to quiesce command", i);
			}
		}
	}

	/* now even our interrupts are disabled -- really quiet now */
	pstate_save = disable_vec_intr();
}

static void
sysctrl_release_cpus(void)
{
	/* let the other cpus go */
	sysctrl_gate[CPU->cpu_id] = 1;

	/* restore our interrupts too */
	enable_vec_intr(pstate_save);
}

static void
sysctrl_stop_intr(void)
{
	mutex_enter(&cpu_lock);
	kpreempt_disable();
	cyclic_suspend();
}

static void
sysctrl_enable_intr(void)
{
	cyclic_resume();
	(void) spl0();
	kpreempt_enable();
	mutex_exit(&cpu_lock);
}

static int
sysctrl_is_real_device(dev_info_t *dip)
{
	struct regspec *regbuf;
	int length;
	int rc;

	if (ddi_get_driver(dip) == NULL)
		return (FALSE);

	if (DEVI(dip)->devi_pm_flags & (PMC_NEEDS_SR|PMC_PARENTAL_SR))
		return (TRUE);
	if (DEVI(dip)->devi_pm_flags & PMC_NO_SR)
		return (FALSE);

	/*
	 * now the general case
	 */
	rc = ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS, "reg",
		(caddr_t)&regbuf, &length);
	ASSERT(rc != DDI_PROP_NO_MEMORY);
	if (rc != DDI_PROP_SUCCESS) {
		return (FALSE);
	} else {
		kmem_free(regbuf, length);
		return (TRUE);
	}
}

static dev_info_t *failed_driver;
static char device_path[MAXPATHLEN];

static int
sysctrl_suspend_devices(dev_info_t *dip, sysc_cfga_pkt_t *pkt)
{
	failed_driver = NULL;
	for (; dip != NULL; dip = ddi_get_next_sibling(dip)) {
		if (sysctrl_suspend_devices(ddi_get_child(dip), pkt))
			return (ENXIO);
		if (!sysctrl_is_real_device(dip))
			continue;
		(void) ddi_pathname(dip, device_path);
		DEBUGP(errp(" suspending device %s\n", device_path));
		if (devi_detach(dip, DDI_SUSPEND) != DDI_SUCCESS) {
			DEBUGP(errp("  unable to suspend device %s\n",
				device_path));

			(void) strncpy(pkt->errbuf, device_path,
				SYSC_OUTPUT_LEN);
			SYSC_ERR_SET(pkt, SYSC_ERR_SUSPEND);
			failed_driver = dip;
			return (ENXIO);
		}
	}

	return (DDI_SUCCESS);
}

static void
sysctrl_resume_devices(dev_info_t *start, sysc_cfga_pkt_t *pkt)
{
	dev_info_t	*dip, *next, *last = NULL;

	/* attach in reverse device tree order */
	while (last != start) {
		dip = start;
		next = ddi_get_next_sibling(dip);
		while (next != last && dip != failed_driver) {
			dip = next;
			next = ddi_get_next_sibling(dip);
		}
		if (dip == failed_driver)
			failed_driver = NULL;
		else if (sysctrl_is_real_device(dip) && failed_driver == NULL) {
			(void) ddi_pathname(dip, device_path);
			DEBUGP(errp(" resuming device %s\n", device_path));
			if (devi_attach(dip, DDI_RESUME) != DDI_SUCCESS) {
				/*
				 * XXX - if in the future we decide not to
				 * panic the system, we need to set the error
				 * SYSC_ERR_RESUME here and also change the
				 * cfgadm platform library.
				 */
				cmn_err(CE_PANIC, "Unable to resume device %s",
					device_path);
			}
		}
		sysctrl_resume_devices(ddi_get_child(dip), pkt);
		last = dip;
	}
}

static int
sysctrl_stop_user_threads(sysc_cfga_pkt_t *pkt)
{
	int		count;
	char		cache_psargs[PSARGSZ];
	kthread_id_t	cache_tp;
	uint_t		cache_t_state;
	int		bailout;
	pid_t		pid;

	extern void add_one_utstop();
	extern void utstop_timedwait(clock_t);
	extern void utstop_init(void);

#define	SYSCTRL_UTSTOP_RETRY	4
#define	SYSCTRL_UTSTOP_WAIT	hz

	utstop_init();

	/* we need to try a few times to get past fork, etc. */
	for (count = 0; count < SYSCTRL_UTSTOP_RETRY; count++) {
		kthread_id_t tp;

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
		utstop_timedwait(count * count * SYSCTRL_UTSTOP_WAIT);


		/* now, walk the threadlist again to see if we are done */
		mutex_enter(&pidlock);
		for (tp = curthread->t_next, bailout = 0;
		    bailout == 0 && tp != curthread; tp = tp->t_next) {
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
				pid = p->p_pidp->pid_id;
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
		(void) sprintf(pkt->errbuf, "process: %s id: %d state: %x"
		    " thread descriptor: %p",
		    cache_psargs, (int)pid, cache_t_state,
			(void *)cache_tp);

		SYSC_ERR_SET(pkt, SYSC_ERR_UTHREAD);

		return (ESRCH);
	}

	return (DDI_SUCCESS);
}

static int
sysctrl_stop_kernel_threads(sysc_cfga_pkt_t *pkt)
{
	caddr_t		name;
	kthread_id_t	tp;

	callb_lock_table();	/* Note: we unlock the table in resume. */
	if (sysctrl_skip_kernel_threads) {
		return (DDI_SUCCESS);
	}
	if ((name = callb_execute_class(CB_CL_CPR_DAEMON,
	    CB_CODE_CPR_CHKPT)) != (caddr_t)NULL) {

		(void) strncpy(pkt->errbuf, name, SYSC_OUTPUT_LEN);
		SYSC_ERR_SET(pkt, SYSC_ERR_KTHREAD);
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
			(void) strncpy(pkt->errbuf, name, SYSC_OUTPUT_LEN);
			SYSC_ERR_SET(pkt, SYSC_ERR_KTHREAD);
			return (EBUSY);
		}
	}

	mutex_exit(&pidlock);
	return (DDI_SUCCESS);
}

static void
sysctrl_start_user_threads(void)
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
sysctrl_signal_user(int sig)
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
sysctrl_resume(sysc_cfga_pkt_t *pkt)
{
#ifndef Bug_4154263
	DEBUGP(errp("resume system...\n"));
#endif
	switch (suspend_state) {
	case SYSC_STATE_FULL:
		/*
		 * release all the other cpus
		 */
#ifndef	Bug_4154263
		DEBUGP(errp("release cpus..."));
#endif
		sysctrl_release_cpus();
		DEBUGP(errp("cpus resumed...\n"));

		/*
		 * If we suspended hw watchdog at suspend,
		 * re-enable it now.
		 */
		if (sysc_watchdog_suspended) {
			mutex_enter(&tod_lock);
			tod_ops.tod_set_watchdog_timer(
				watchdog_timeout_seconds);
			mutex_exit(&tod_lock);
		}

		/*
		 * resume callout
		 */
		(void) callb_execute_class(CB_CL_CPR_RPC, CB_CODE_CPR_RESUME);
		(void) callb_execute_class(CB_CL_CPR_CALLOUT,
			CB_CODE_CPR_RESUME);
		sysctrl_enable_intr();
		/* FALLTHROUGH */

	case SYSC_STATE_DRIVER:
		/*
		 * resume drivers
		 */
		DEBUGP(errp("resume drivers..."));
		sysctrl_resume_devices(ddi_root_node(), pkt);
		DEBUGP(errp("done\n"));

		/*
		 * resume the lock manager
		 */
		lm_cprresume();

		/* FALLTHROUGH */

	case SYSC_STATE_DAEMON:
		/*
		 * resume kernel daemons
		 */
		if (!sysctrl_skip_kernel_threads) {
			DEBUGP(errp("starting kernel daemons..."));
			(void) callb_execute_class(CB_CL_CPR_DAEMON,
				CB_CODE_CPR_RESUME);
		}
		callb_unlock_table();
		DEBUGP(errp("done\n"));

		/* FALLTHROUGH */

	case SYSC_STATE_USER:
		/*
		 * finally, resume user threads
		 */
		DEBUGP(errp("starting user threads..."));
		sysctrl_start_user_threads();
		DEBUGP(errp("done\n"));
		/* FALLTHROUGH */

	case SYSC_STATE_BEGIN:
	default:
		/*
		 * let those who care know that we've just resumed
		 */
		DEBUGP(errp("sending SIGTHAW..."));
		sysctrl_signal_user(SIGTHAW);
		DEBUGP(errp("done\n"));
		break;
	}

	/* Restore sysctrl detach/suspend to its original value */
	sysctrl_enable_detach_suspend = sysc_lastval;

	DEBUGP(errp("system state restored\n"));
}

void
sysctrl_suspend_prepare(void)
{
	/*
	 * We use a function, lm_cprsuspend(), in the suspend flow that
	 * is redirected to a module through the modstubs mechanism.
	 * If the module is currently not loaded, modstubs attempts
	 * the modload. The context this happens in below causes the
	 * module load to block forever, so this function must be called
	 * in the normal system call context ahead of time.
	 */
	(void) modload("misc", "klmmod");
}

int
sysctrl_suspend(sysc_cfga_pkt_t *pkt)
{
	int rc = DDI_SUCCESS;

	/* enable sysctrl detach/suspend function */
	sysc_lastval = sysctrl_enable_detach_suspend;
	sysctrl_enable_detach_suspend = 1;

	/*
	 * first, stop all user threads
	 */
	DEBUGP(errp("\nstopping user threads..."));
	suspend_state = SYSC_STATE_USER;
	if (rc = sysctrl_stop_user_threads(pkt)) {
		sysctrl_resume(pkt);
		return (rc);
	}
	DEBUGP(errp("done\n"));

	/*
	 * now stop daemon activities
	 */
	DEBUGP(errp("stopping kernel daemons..."));
	suspend_state = SYSC_STATE_DAEMON;
	if (rc = sysctrl_stop_kernel_threads(pkt)) {
		sysctrl_resume(pkt);
		return (rc);
	}
	DEBUGP(errp("done\n"));

	/*
	 * This sync swap out all user pages
	 */
	vfs_sync(SYNC_ALL);

	/*
	 * special treatment for lock manager
	 */
	lm_cprsuspend();

	/*
	 * sync the file system in case we never make it back
	 */
	sync();

	/*
	 * now suspend drivers
	 */
	DEBUGP(errp("suspending drivers..."));
	suspend_state = SYSC_STATE_DRIVER;
	if (rc = sysctrl_suspend_devices(ddi_root_node(), pkt)) {
		sysctrl_resume(pkt);
		return (rc);
	}
	DEBUGP(errp("done\n"));

	/*
	 * handle the callout table
	 */
	sysctrl_stop_intr();

	(void) callb_execute_class(CB_CL_CPR_CALLOUT, CB_CODE_CPR_CHKPT);

	/*
	 * if watchdog was activated, disable it
	 */
	if (watchdog_activated) {
		mutex_enter(&tod_lock);
		tod_ops.tod_clear_watchdog_timer();
		mutex_exit(&tod_lock);
		sysc_watchdog_suspended = 1;
	} else {
		sysc_watchdog_suspended = 0;
	}

	/*
	 * finally, grab all cpus
	 */
	DEBUGP(errp("freezing all cpus...\n"));
	suspend_state = SYSC_STATE_FULL;
	sysctrl_grab_cpus();
#ifndef	Bug_4154263
	DEBUGP(errp("done\n"));

	DEBUGP(errp("system is quiesced\n"));
#endif

	return (rc);
}
