/*
 * Copyright (c) 1993-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_main.c	1.107	99/10/25 SMI"


/*
 * This module contains the guts of checkpoint-resume mechanism.
 * All code in this module is platform independent.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/callb.h>
#include <sys/processor.h>
#include <sys/machsystm.h>
#include <sys/clock.h>
#include <sys/vfs.h>
#include <sys/kmem.h>
#include <nfs/lm.h>
#include <sys/systm.h>
#include <sys/cpr.h>
#include <sys/bootconf.h>
#include <sys/cyclic.h>

extern void cpr_convert_promtime(cpr_time_t *);
extern int cpr_alloc_statefile(void);
extern void flush_windows(void);
extern int setjmp(label_t *);
extern void cpr_abbreviate_devpath(char *, char *);

static int cpr_suspend(void);
static int cpr_resume(void);
static struct cprinfo *ci;
static void cpr_suspend_init();

extern struct cpr_terminator cpr_term;

cpr_time_t wholecycle_tv;
int cpr_suspend_succeeded;

static char done_str[] = "done\n";


/*
 * save or restore abort_enable;  this prevents a drop
 * to kadb or prom during cpr_resume_devices() when
 * there is no kbd present;  see abort_sequence_enter()
 */
static void
cpr_sae(int stash)
{
	static int saved_ae = -1;

	if (stash) {
		saved_ae = abort_enable;
		abort_enable = 0;
	} else if (saved_ae != -1) {
		abort_enable = saved_ae;
		saved_ae = -1;
	}
}


/*
 * The main switching point for cpr, this routine starts the ckpt
 * and state file saving routines; on resume the control is
 * returned back to here and it then calls the resume routine.
 */
int
cpr_main()
{
	label_t saveq = ttolwp(curthread)->lwp_qsav;
	int rc = 0;

	ci = kmem_zalloc(sizeof (struct cprinfo), KM_SLEEP);
	/*
	 * Remember where we are for resume
	 */
	if (!setjmp(&ttolwp(curthread)->lwp_qsav)) {
		/*
		 * try to checkpoint the system, if failed return back
		 * to userland, otherwise power off.
		 */
		rc = cpr_suspend();
		if (rc || cpr_reusable_mode) {
			/*
			 * We don't really want to go down, or
			 * something went wrong in suspend, do what we can
			 * to put the system back to an operable state then
			 * return back to userland.
			 */
			(void) cpr_resume();
		}
	} else {
		/*
		 * This is the resumed side of longjmp, restore the previous
		 * longjmp pointer if there is one so this will be transparent
		 * to the world.
		 */
		ttolwp(curthread)->lwp_qsav = saveq;
		CPR->c_flags &= ~C_SUSPENDING;
		CPR->c_flags |= C_RESUMING;

		/*
		 * resume the system back to the original state
		 */
		rc = cpr_resume();
	}
	kmem_free(ci, sizeof (struct cprinfo));

	return (rc);
}

/*
 * Take the system down to a checkpointable state and write
 * the state file, the following are sequentially executed:
 *
 *    - Request all user threads to stop themselves
 *    - push out and invalidate user pages
 *    - bring statefile inode incore to prevent a miss later
 *    - request all daemons to stop
 *    - check and make sure all threads are stopped
 *    - sync the file system
 *    - suspend all devices
 *    - block intrpts
 *    - dump system state and memory to state file
 */

static int
cpr_suspend()
{
	int rc = 0;
	extern void cpr_send_notice();

	cpr_suspend_init();

	cpr_save_time();

	cpr_tod_get(&wholecycle_tv);
	CPR_STAT_EVENT_START("Suspend Total");

	/*
	 * read needed current openprom info.
	 */
	if (rc = cpr_get_bootinfo(ci))
		return (rc);

	if (!cpr_reusable_mode) {
		/*
		 * We need to validate cprinfo file before fs functionality
		 * is disabled.
		 */
		if (rc = cpr_validate_cprinfo(ci, 0))
			return (rc);
	}

	i_cpr_save_machdep_info();

	if (rc = cpr_mp_offline())
		return (rc);

	/* cpr_signal_user(SIGFREEZE); */

	/*
	 * Ask the user threads to stop by themselves, but
	 * if they don't or can't after 3 retires, we give up on CPR.
	 * The 3 retry is not a random number because 2 is possible if
	 * a thread has been forked before the parent thread is stopped.
	 */
	DEBUG1(errp("\nstopping user threads..."));
	CPR_STAT_EVENT_START("  stop users");
	cpr_set_substate(C_ST_USER_THREADS);
	if (rc = cpr_stop_user_threads())
		return (rc);
	CPR_STAT_EVENT_END("  stop users");
	DEBUG1(errp(done_str));

	/*
	 * User threads are stopped.  We will start communicating with the
	 * user via prom_printf (some debug output may have already happened)
	 * so let anybody who cares know about this (bug 4096122)
	 */
	(void) callb_execute_class(CB_CL_CPR_PROMPRINTF, CB_CODE_CPR_CHKPT);
	cpr_send_notice();
	if (cpr_debug)
		errp("\n");

	(void) callb_execute_class(CB_CL_CPR_POST_USER, CB_CODE_CPR_CHKPT);

	/*
	 * Stop all daemon activities
	 */
	DEBUG1(errp("stopping kernel daemons..."));
	cpr_set_substate(C_ST_KERNEL_THREADS);
	if (rc = cpr_stop_kernel_threads())
		return (rc);
	DEBUG1(errp(done_str));

	/*
	 * Use sync_all to swap out all user pages and find out how much
	 * extra space needed for user pages that don't have back store
	 * space left.
	 */
	CPR_STAT_EVENT_START("  swapout upages");
	vfs_sync(SYNC_ALL);
	CPR_STAT_EVENT_END("  swapout upages");

	CPR_STAT_EVENT_START("  alloc statefile");
	cpr_set_substate(C_ST_STATEF_ALLOC); /* must be before realloc label */
realloc:
	rc = cpr_alloc_statefile();
	/*
	 * XXX If we jumped to the realloc label because of a cpr_dump()
	 * failure of ENOSPC, substate was set to C_ST_STATEF_ALLOC_RETRY.
	 * This allows us to do a partial resume and also affects the
	 * behavior of cpr_alloc_statefile().  But if we are really going
	 * to fail the suspend in the following "if", we must reset the
	 * substate in order to correctly finish the partially completed
	 * resume.  The code should really be reorganized to obviate the
	 * need such for such pathological practices.  (Bug 4018319)
	 */
	cpr_set_substate(C_ST_STATEF_ALLOC);
	if (rc != 0) {
		errp("realloc failed\n");
		return (rc);
	}

	CPR_STAT_EVENT_END("  alloc statefile");

	/*
	 * Hooks needed by lock manager prior to suspending.
	 * Refer to code for more comments.
	 */
	lm_cprsuspend();

	/*
	 * Sync the filesystem to preserve its integrity.
	 *
	 * This sync is also used to flush out all B_DELWRI buffers (fs cache)
	 * which are mapped and neither dirty nor referened before
	 * cpr_invalidate_pages destroies them. fsflush does similar thing.
	 */
	sync();

	/*
	 * destroy all clean file mapped kernel pages
	 */
	CPR_STAT_EVENT_START("  clean pages");
	DEBUG1(errp("cleaning up mapped pages..."));
	(void) callb_execute_class(CB_CL_CPR_VM, CB_CODE_CPR_CHKPT);
	DEBUG1(errp(done_str));
	CPR_STAT_EVENT_END("  clean pages");

	cpr_set_substate(C_ST_DRIVERS);
	CPR_STAT_EVENT_START("  stop drivers");
	DEBUG1(errp("suspending drivers..."));
	(void) callb_execute_class(CB_CL_CPR_PM, CB_CODE_CPR_CHKPT);
	if (rc = cpr_suspend_devices(ddi_root_node()))
		return (rc);
	DEBUG1(errp(done_str));
	CPR_STAT_EVENT_END("  stop drivers");

	cpr_sae(1);

	(void) callb_execute_class(CB_CL_CPR_CALLOUT, CB_CODE_CPR_CHKPT);

	/*
	 * It's safer to do tod_get before we disable all intr.
	 */
	CPR_STAT_EVENT_START("  write statefile");
	/*
	 * it's time to ignore the outside world, stop the real time
	 * clock and disable any further intrpt activity.
	 */

	i_cpr_handle_xc(1);	/* turn it on to disable xc assertion */

	mutex_enter(&cpu_lock);
	cyclic_suspend();
	mutex_exit(&cpu_lock);

	mon_clock_stop();
	mon_clock_unshare();
	mon_clock_start();

	i_cpr_stop_intr();

	DEBUG1(errp("interrupt is stopped\n"));

	/*
	 * getting ready to write ourself out, flush the register
	 * windows to make sure that our stack is good when we
	 * come back on the resume side.
	 */
	flush_windows();

	/*
	 * FATAL: NO MORE MEMORY ALLOCATION ALLOWED AFTER THIS POINT!!!
	 *
	 * The system is quiesced at this point, we are ready to either dump
	 * to the state file for a extended sleep or a simple shutdown for
	 * systems with non-volatile memory.
	 */
	cpr_set_substate(C_ST_DUMP);

	rc = cpr_dump(C_VP);
	if (rc == ENOSPC) {
		cpr_set_substate(C_ST_STATEF_ALLOC_RETRY);
		(void) cpr_resume();
		goto realloc;
	} else if (rc == 0) {
		char *cp, *sp;
		(void) strcpy(ci->ci_bootfile, CPRBOOT);
		(void) strcpy(ci->ci_autoboot, "true");

		if (cpr_reusable_mode) {
			/*
			 * append -R -S <statefile path> to bootfile
			 * string
			 */
			cp = ci->ci_bootfile + strlen(ci->ci_bootfile);
			ASSERT(cpr_statefile_is_spec());
			(void) strcpy(cp, " -R -S ");
			cp += strlen(" -R -S ");
			sp = cpr_get_statefile_prom_path();
			cpr_abbreviate_devpath(sp, cp);
		} else {
			if (cpr_statefile_is_spec()) {
				/*
				 * append -S <statefile path> to
				 * bootfile string
				 */
				cp = ci->ci_bootfile +
				    strlen(ci->ci_bootfile);
				(void) strcpy(cp, " -S ");
				cp += strlen(" -S ");
				sp = cpr_get_statefile_prom_path();
				cpr_abbreviate_devpath(sp, cp);
			}
		}
		/*
		 * On some versions of the prom, a fully qualified
		 * device path can be truncated when stored in
		 * the boot-device nvram property.  This call
		 * generates the shortest unambiguous equivalent.
		 */
		cpr_abbreviate_devpath(prom_bootpath(), ci->ci_bootdevice);
		(void) strcpy(ci->ci_diagfile, ci->ci_bootfile);
		(void) strcpy(ci->ci_diagdevice, ci->ci_bootdevice);
		rc = cpr_set_properties(ci);
		if (cpr_reusable_mode) {
			cpr_set_substate(C_ST_REUSABLE);
			longjmp(&ttolwp(curthread)->lwp_qsav);
		}
	}
	return (rc);
}

/*
 * Bring the system back up from a checkpoint, at this point
 * the VM has been minimally restored by boot, the following
 * are executed sequentially:
 *
 *    - machdep setup and enable interrupts (mp startup if it's mp)
 *    - resume all devices
 *    - restart daemons
 *    - put all threads back on run queue
 */
static int
cpr_resume()
{
	extern void cpr_statef_close();
	cpr_time_t pwron_tv, *ctp;
	char *str;
	int rc = 0;

	/*
	 * The following switch is used to resume the system
	 * that was suspended to a different level.
	 */
	DEBUG1(errp("\nEntering cpr_resume...\n"));

	switch (CPR->c_substate) {
	case C_ST_DUMP:
	case C_ST_STATEF_ALLOC_RETRY:
		break;

	case C_ST_DRIVERS:
		goto driver;

	case C_ST_STATEF_ALLOC:
		goto alloc;

	case C_ST_USER_THREADS:
		goto user;

	case C_ST_KERNEL_THREADS:
		goto kernel;

	case C_ST_REUSABLE:
		goto back_up;

	default:
		goto others;
	}

	/*
	 * setup debugger trapping.
	 */
	if (cpr_suspend_succeeded)
		i_cpr_set_tbr();

	/*
	 * tell prom to monitor keys before the kernel comes alive
	 */
	mon_clock_start();

	/*
	 * perform platform-dependent initialization
	 */
	if (cpr_suspend_succeeded)
		i_cpr_machdep_setup();


	/*
	 * system did not really go down if we jump here
	 */
back_up:

	/*
	 * IMPORTANT:  SENSITIVE RESUME SEQUENCE
	 *
	 * DO NOT ADD ANY INITIALIZATION STEP BEFORE THIS POINT!!
	 */
	(void) callb_execute_class(CB_CL_CPR_DMA, CB_CODE_CPR_RESUME);
	if (cpr_suspend_succeeded)
		(void) callb_execute_class(CB_CL_CPR_RPC, CB_CODE_CPR_RESUME);

	/*
	 * let the tmp callout catch up.
	 */
	(void) callb_execute_class(CB_CL_CPR_CALLOUT, CB_CODE_CPR_RESUME);

	i_cpr_enable_intr();

	mon_clock_stop();
	mon_clock_share();

	mutex_enter(&cpu_lock);
	cyclic_resume();
	mutex_exit(&cpu_lock);

	mon_clock_start();

	i_cpr_handle_xc(0);	/* turn it off to allow xc assertion */

	/*
	 * statistics gathering
	 */
	if (cpr_suspend_succeeded) {
		cpr_convert_promtime(&pwron_tv);

		ctp = &cpr_term.tm_shutdown;
		CPR_STAT_EVENT_END_TMZ("  write statefile", ctp);
		CPR_STAT_EVENT_END_TMZ("Suspend Total", ctp);

		CPR_STAT_EVENT_START_TMZ("Resume Total", &pwron_tv);

		str = "  prom time";
		CPR_STAT_EVENT_START_TMZ(str, &pwron_tv);
		ctp = &cpr_term.tm_cprboot_start;
		CPR_STAT_EVENT_END_TMZ(str, ctp);

		str = "  read statefile";
		CPR_STAT_EVENT_START_TMZ(str, ctp);
		ctp = &cpr_term.tm_cprboot_end;
		CPR_STAT_EVENT_END_TMZ(str, ctp);
	}

driver:
	DEBUG1(errp("resuming devices..."));
	CPR_STAT_EVENT_START("  start drivers");
	/*
	 * The policy here is to continue resume everything we can if we did
	 * not successfully finish suspend; and panic if we are coming back
	 * from a fully suspended system.
	 */
	rc = cpr_resume_devices(ddi_root_node());
	(void) callb_execute_class(CB_CL_CPR_PM, CB_CODE_CPR_RESUME);

	cpr_sae(0);

	str = "cpr: Failed to resume one or more devices.";
	if (rc && CPR->c_substate == C_ST_DUMP)
		cmn_err(CE_PANIC, str);
	else if (rc)
		cmn_err(CE_WARN, str);
	CPR_STAT_EVENT_END("  start drivers");
	DEBUG1(errp(done_str));

	/*
	 * This resume is due to a retry of allocating the statefile.
	 */
	if (CPR->c_substate == C_ST_STATEF_ALLOC_RETRY) {
		mon_clock_stop();
		return (0);
	}

alloc:
	cpr_statef_close();
	/*
	 * Hooks needed by lock manager prior to resuming.
	 * Refer to code for more comments.
	 */
	lm_cprresume();
	/*
	 * put all threads back to where they belong,
	 * get the kernel daemons straightened up too.
	 */
kernel:
	DEBUG1(errp("starting kernel daemons..."));
	(void) callb_execute_class(CB_CL_CPR_DAEMON, CB_CODE_CPR_RESUME);
	callb_unlock_table();
	DEBUG1(errp(done_str));

	(void) callb_execute_class(CB_CL_CPR_POST_USER, CB_CODE_CPR_RESUME);

user:
	/*
	 * Once user threads are running, the display can be updated by
	 * X, so we let display drivers switch displays back (bug 4096122)
	 */
	(void) callb_execute_class(CB_CL_CPR_PROMPRINTF, CB_CODE_CPR_RESUME);

	DEBUG1(errp("starting user threads..."));
	cpr_start_user_threads();
	DEBUG1(errp(done_str));

others:
	/*
	 * now that all the drivers are going, kernel kbd driver can
	 * take over, turn off prom monitor clock
	 */
	mon_clock_stop();

	if (cpr_suspend_succeeded) {
		cpr_restore_time();
		cpr_stat_record_events();
	}

	if (!cpr_reusable_mode)
		cpr_clear_cprinfo(ci);

	DEBUG1(errp("Sending SIGTHAW..."));
	cpr_signal_user(SIGTHAW);
	DEBUG1(errp(done_str));

	if (cpr_mp_online())
		cmn_err(CE_WARN, "cpr: Failed to online all the processors.");

	CPR_STAT_EVENT_END("Resume Total");

	CPR_STAT_EVENT_START_TMZ("WHOLE CYCLE", &wholecycle_tv);
	CPR_STAT_EVENT_END("WHOLE CYCLE");

	DEBUG1(errp("\nThe system is back where you left!\n"));

	CPR_STAT_EVENT_START("POST CPR DELAY");

#ifdef CPR_STAT
	ctp = &cpr_term.tm_shutdown;
	CPR_STAT_EVENT_START_TMZ("PWROFF TIME", ctp);
	CPR_STAT_EVENT_END_TMZ("PWROFF TIME", &pwron_tv);

	CPR_STAT_EVENT_PRINT();
#endif CPR_STAT
	return (rc);
}

static void
cpr_suspend_init()
{
	extern void cpr_stat_init();
	cpr_time_t *ctp;

	cpr_stat_init();

	/*
	 * If cpr_suspend() failed before cpr_dump() gets a chance
	 * to reinitialize the terminator of the statefile,
	 * the values of the old terminator will still linger around.
	 * Since the terminator contains information that we need to
	 * decide whether suspend succeeded or not, we need to
	 * reinitialize it as early as possible.
	 */
	cpr_term.real_statef_size = 0;
	ctp = &cpr_term.tm_shutdown;
	bzero(ctp, sizeof (*ctp));
	ctp = &cpr_term.tm_cprboot_start;
	bzero(ctp, sizeof (*ctp));
	ctp = &cpr_term.tm_cprboot_end;
	bzero(ctp, sizeof (*ctp));

	cpr_suspend_succeeded = 0;
}
