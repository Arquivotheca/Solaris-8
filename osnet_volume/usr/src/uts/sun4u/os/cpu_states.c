/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpu_states.c	1.24	99/07/31 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/stack.h>
#include <sys/varargs.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/thread.h>
#include <sys/callb.h>
#include <sys/cpuvar.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/procset.h>
#include <sys/consdev.h>
#include <sys/uadmin.h>
#include <sys/panic.h>
#include <sys/reboot.h>
#include <sys/bootconf.h>
#include <sys/autoconf.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/async.h>
#include <sys/ddi_implfuncs.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/debug/debug.h>
#include <sys/vmsystm.h>
#include <sys/vm_machparam.h>
#include <sys/ftrace.h>
#include <sys/x_call.h>
#include <sys/membar.h>
#include <sys/intr.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/hat_sfmmu.h>

#include <sys/cpu_sgnblk_defs.h>

#ifdef	TRAPTRACE
#include <sys/traptrace.h>
u_longlong_t panic_tick;
#endif /* TRAPTRACE */

/*
 * Boot hands us the _dvec, in start().
 */
struct debugvec *dvec = (struct debugvec *)0;

/*
 * abort_seq_handler required by sysctrl.
 */
static void reboot_machine(char *);
void debug_enter(char *);
void (*abort_seq_handler)(char *) = debug_enter;

/*
 * If bootstring contains a device path, we need to convert to a format
 * the prom will understand and return this new bootstring in buf.
 */
static char *
convert_boot_device_name(char *cur_path)
{
	char *ret = cur_path;
	char *ptr, *buf;

	if ((buf = kmem_alloc(MAXPATHLEN, KM_NOSLEEP)) == NULL)
		return (cur_path);
	if ((ptr = strchr(cur_path, ' ')) != NULL)
		*ptr = '\0';
	if (i_devname_to_promname(cur_path, buf) == 0) {
		/* the conversion worked */
		if (ptr != NULL) {
			*ptr = ' ';
			(void) strcat(buf, ptr);
			ptr = NULL;
		}
		ret = buf;
	} else if (ptr != NULL) {	 /* the conversion failed */
		kmem_free(buf, MAXPATHLEN);
		*ptr = ' ';
		ret = cur_path;
	}
	return (ret);
}

/*
 * Machine dependent code to reboot.
 * "mdep" is interpreted as a character pointer; if non-null, it is a pointer
 * to a string to be used as the argument string when rebooting.
 */
/*ARGSUSED*/
void
mdboot(int cmd, int fcn, char *bootstr)
{
	int s;

	/*
	 * XXX - rconsvp is set to NULL to ensure that output messages
	 * are sent to the underlying "hardware" device using the
	 * monitor's printf routine since we are in the process of
	 * either rebooting or halting the machine.
	 */
	rconsvp = NULL;

	s = spl6();
	reset_leaves(); 		/* try and reset leaf devices */
	if (fcn == AD_HALT) {
		halt((char *)NULL);
		fcn &= ~RB_HALT;
		/* MAYBE REACHED */
	} else if (fcn == AD_POWEROFF) {
		power_down(NULL);
		/* NOTREACHED */
	} else {
		if (bootstr == NULL) {
			switch (fcn) {

			case AD_BOOT:
				bootstr = "";
				break;

			case AD_IBOOT:
				bootstr = "-a";
				break;

			case AD_SBOOT:
				bootstr = "-s";
				break;

			case AD_SIBOOT:
				bootstr = "-sa";
				break;
			default:
				cmn_err(CE_WARN,
				    "mdboot: invalid function %d", fcn);
				bootstr = "";
				break;
			}
		} else if (*bootstr == '/') {
			/* take care of any devfs->prom device name mappings */
			bootstr = convert_boot_device_name(bootstr);
		}
		reboot_machine(bootstr);
		/*NOTREACHED*/
	}
	splx(s);
}

/*
 *	Machine dependent abort sequence handling
 */
void
abort_sequence_enter(char *msg)
{
	if (abort_enable != 0)
		(*abort_seq_handler)(msg);
}

/*
 * Enter debugger.  Called when the user types L1-A or break or whenever
 * code wants to enter the debugger and possibly resume later.
 * If the debugger isn't present, enter the PROM monitor.
 */
void
debug_enter(char *msg)
{
	label_t old_pcb;
	int s;
	void *cookie;
	extern void *pm_powerup_console(void);
	extern void pm_restore_console(void *cookie);

	/*
	 * If we're in the panic code, StarFire needs to set the signature
	 * block to OS and the state to exiting for all CPUs.  When the
	 * panic code enters the debugger, we temporary null out the exit
	 * code so that the SSP won't be fooled into thinking we're exiting.
	 */
#ifdef _STARFIRE
	if (panicstr)
		SGN_UPDATE_ALL_OS_RUN_NULL();
#endif

	if (msg)
		prom_printf("%s\n", msg);

	s = splzs();
	old_pcb = curthread->t_pcb;
	(void) setjmp(&curthread->t_pcb);

	if ((vx_entered == 0) && (boothowto & RB_DEBUG)) {
		cookie = pm_powerup_console();
		(*(vfunc_t)(&dvec->dv_entry))();
		pm_restore_console(cookie);
	} else {
		prom_enter_mon();
	}

	curthread->t_pcb = old_pcb;
	splx(s);

#ifdef _STARFIRE
	if (panicstr)
		SGN_UPDATE_ALL_OS_RUN_PANIC2();
#endif
}

/*
 * Halt the machine and return to the monitor
 */
void
halt(char *s)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */

	if (s)
		prom_printf("(%s) ", s);

	/*
	 * StarFire needs to set the signature block to OS and
	 * the state to exiting for all the processors.
	 */
	SGN_UPDATE_ALL_OS_EXIT_HALT();
	prom_exit_to_mon();
	/*NOTREACHED*/
}

/*
 * Halt the machine and then reboot with the device
 * and arguments specified in bootstr.
 */
static void
reboot_machine(char *bootstr)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */
	prom_printf("rebooting...\n");
	/*
	 * The StarFire needs to set the signature block to OS and
	 * the state to exiting for all the processors.
	 */
	SGN_UPDATE_ALL_OS_EXIT_REBOOT();
	prom_reboot(bootstr);
	/*NOTREACHED*/
}

/*
 * Halt the machine and power off the system.
 */
void
power_down(const char *s)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */

	if (s != NULL)
		prom_printf("(%s) ", s);

	prom_power_off();
	/*
	 * If here is reached, for some reason prom's power-off command failed.
	 * Prom should have already printed out error messages. Exit to
	 * firmware.
	 */
	prom_exit_to_mon();
	/*
	 * For StarFire, all we want to do is to set up the signature blocks
	 * to indicate that we have an environmental interrupt request
	 * to power down, and then exit to the prom monitor.
	 */
	SGN_UPDATE_ALL_OS_EXIT_ENVIRON();
	/*NOTREACHED*/
}

void
do_shutdown(void)
{
	proc_t *initpp;

	/*
	 * If we're still booting and init(1) isn't set up yet, simply halt.
	 */
	mutex_enter(&pidlock);
	initpp = prfind(P_INITPID);
	mutex_exit(&pidlock);
	if (initpp == NULL) {
		extern void halt(char *);
		prom_power_off();
		halt("Power off the System");	/* just in case */
	}

	/*
	 * else, graceful shutdown with inittab and all getting involved
	 */
	psignal(initpp, SIGPWR);
}

/*
 * Notify kadb that a cpu does not respond to cross-traps.
 */
void
kadb_cpu_off(int cpuid)
{
	ASSERT(cpuid < NCPU);
	if (dvec != NULL && dvec->dv_version >= DEBUGVEC_VERSION_1 &&
	    dvec->dv_cpu_change != NULL) {
		(*dvec->dv_cpu_change)(cpuid, KADB_CPU_XCALL, 0);
	}
}

/*
 * Notify kadb that a cpu responds to cross-traps.
 * Call this routine only if kadb_cpu_off() was called
 * for the same cpu earlier.
 */
void
kadb_cpu_on(int cpuid)
{
	ASSERT(cpuid < NCPU);
	if (dvec != NULL && dvec->dv_version >= DEBUGVEC_VERSION_1 &&
	    dvec->dv_cpu_change != NULL) {
		(*dvec->dv_cpu_change)(cpuid, KADB_CPU_XCALL, 1);
	}
}

/*
 * For CPR. When resuming, the PROM state is wiped out; in order for
 * kadb to work, the defer word that allows kadb's trap mechanism to
 * work must be downloaded to the PROM.
 */

/*
 * Access the words which kadb defines to the prom.
 */
void
kadb_format(caddr_t *arg)
{
	if (dvec != NULL && dvec->dv_version >= DEBUGVEC_VERSION_1 &&
	    dvec->dv_format != NULL) {
		(*dvec->dv_format)(arg);
	}
}

/*
 * Arm the prom to use the words which kadb defines to the prom.
 */
void
kadb_arm(void)
{
	if (dvec != NULL && dvec->dv_version >= DEBUGVEC_VERSION_1 &&
	    dvec->dv_arm != NULL) {
		(*dvec->dv_arm)();
	}
}

/*
 * We use the x-trap mechanism and idle_stop_xcall() to stop the other CPUs
 * in a manner similar to stop_other_cpus(); once in panic_idle() they raise
 * spl, record their location, and spin.
 */
static void
panic_idle(void)
{
	(void) spl7();

	debug_flush_windows();
	(void) setjmp(&curthread->t_pcb);

	CPU->cpu_m.in_prom = 1;
	membar_stld();

	for (;;);
}

/*
 * Force the other CPUs to trap into panic_idle(), and then remove them
 * from the cpu_ready_set so they will no longer receive cross-calls.
 */
void
panic_stopcpus(cpu_t *cp, kthread_t *t, int spl)
{
	cpuset_t cps;
	int i;

	/*
	 * If we've panicked at level 14, we examine t_panic_trap to see if
	 * a fatal trap occurred.  If so, we disable further %tick_cmpr
	 * interrupts.  If not, an explicit call to panic was made and so
	 * we re-enqueue an interrupt request structure to allow further
	 * level 14 interrupts to be processed once we lower PIL.
	 */
	if (spl == ipltospl(PIL_14)) {
		uint_t opstate = disable_vec_intr();

		if (t->t_panic_trap != NULL) {
			tickcmpr_disable();
			tickcmpr_dequeue_req();
		} else if (!tickcmpr_disabled())
			tickcmpr_enqueue_req();

		/*
		 * Clear SOFTINT<14> and SOFTINT<0> (TICK_INT) to indicate
		 * that the current level 14 has been serviced.
		 */
		clear_soft_intr(PIL_14);
		clear_soft_intr(0);

		enable_vec_intr(opstate);
	}

	(void) splzs();
	CPUSET_ALL_BUT(cps, cp->cpu_id);
	xt_some(cps, (xcfunc_t *)idle_stop_xcall, (uint64_t)&panic_idle, NULL);

	for (i = 0; i < NCPU; i++) {
		if (i != cp->cpu_id && CPU_XCALL_READY(i)) {
			int ntries = 0x10000;

			while (!cpu[i]->cpu_m.in_prom && ntries) {
				DELAY(50);
				ntries--;
			}

			if (!cpu[i]->cpu_m.in_prom)
				printf("panic: failed to stop cpu%d\n", i);

			cpu[i]->cpu_flags &= ~CPU_READY;
			cpu[i]->cpu_flags |= CPU_QUIESCED;
			CPUSET_DEL(cpu_ready_set, cpu[i]->cpu_id);
		}
	}
}

/*
 * Miscellaneous hardware-specific code to execute after panicstr is set
 * by the panic code: we also print and record PTL1 panic information here.
 */
void
panic_quiesce_hw(panic_data_t *pdp)
{
#ifdef TRAPTRACE
	/*
	 * Turn off TRAPTRACE and save the current %tick value in panic_tick.
	 */
	if (!panic_tick)
		panic_tick = gettick();
	TRAPTRACE_FREEZE;
#endif
	/*
	 * StarFire needs to set the signature block to OS and the state to
	 * exiting for all the processors.  We set the exit code to EXIT_PANIC1
	 * to indicate to the SSP that we've entered the panic flow.
	 */
	SGN_UPDATE_ALL_OS_EXIT_PANIC1();

	/*
	 * De-activate ECC functions and disable the watchdog timer now that
	 * we've made it through the critical part of the panic code.
	 */
	if (watchdog_enable)
		(void) tod_ops.tod_clear_watchdog_timer();
	error_disable();

	/*
	 * If ptl1_panic() was called, we need to print and record the
	 * information saved into the ptl1_dat[] array.
	 */
	if (ptl1_panic_cpu) {
		short i, maxtl = ptl1_dat[0].d.ptl1_tl;
		panic_nv_t *pnv = PANICNVGET(pdp);
		char name[PANICNVNAMELEN];

		printf("panic: ptl1 trap reason 0x%x\n", ptl1_panic_tr);

		for (i = maxtl - 1; i >= 0; i--) {
			PTL1_DAT *ptp = &ptl1_dat[i];

			(void) snprintf(name, sizeof (name), "tl[%d]", i);
			PANICNVADD(pnv, name, ptp->d.ptl1_tl);
			(void) snprintf(name, sizeof (name), "tt[%d]", i);
			PANICNVADD(pnv, name, ptp->d.ptl1_tt);
			(void) snprintf(name, sizeof (name), "tick[%d]", i);
			PANICNVADD(pnv, name, ptp->d.ptl1_tick);
			(void) snprintf(name, sizeof (name), "tpc[%d]", i);
			PANICNVADD(pnv, name, ptp->d.ptl1_tpc);
			(void) snprintf(name, sizeof (name), "tnpc[%d]", i);
			PANICNVADD(pnv, name, ptp->d.ptl1_tnpc);
			(void) snprintf(name, sizeof (name), "tstate[%d]", i);
			PANICNVADD(pnv, name, ptp->d.ptl1_tstate);

			printf("TL=0x%x TT=0x%x TICK=0x%llx\n",
			    ptp->d.ptl1_tl, ptp->d.ptl1_tt, ptp->d.ptl1_tick);
			printf("\tTPC=0x%llx TnPC=0x%llx TSTATE=0x%llx\n",
			    ptp->d.ptl1_tpc, ptp->d.ptl1_tnpc,
			    ptp->d.ptl1_tstate);
		}

		PANICNVSET(pdp, pnv);
	}

	/*
	 * Redirect all interrupts to the current CPU.
	 */
	mutex_enter(&cpu_lock);
	intr_redist_all_cpus(INTR_CURRENT_CPU);
	mutex_exit(&cpu_lock);
}

/*
 * StarFire needs to set the signature block to OS and the state to exiting for
 * all CPUs. EXIT_PANIC2 indicates that we're about to write the crash dump,
 * which tells the SSP to begin a timeout routine to reboot the machine if the
 * dump never completes, and allows hung EXIT_PANIC1's to time out.  If we
 * are re-entering the panic path from the level 14 deadman() cyclic, we must
 * again re-enqueue an interrupt structure and clear the SOFTINT register.
 */
void
panic_dump_hw(int spl)
{
	if (spl == ipltospl(PIL_14)) {
		uint_t opstate = disable_vec_intr();

		if (!tickcmpr_disabled())
			tickcmpr_enqueue_req();

		/*
		 * Clear SOFTINT<14> and SOFTINT<0> (TICK_INT) to indicate
		 * that the current level 14 has been serviced.
		 */
		clear_soft_intr(PIL_14);
		clear_soft_intr(0);

		enable_vec_intr(opstate);
	}

	SGN_UPDATE_ALL_OS_EXIT_PANIC2();
}
