/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)x_call.c	1.44	99/04/13 SMI"

/*
 * Facilities for cross-processor subroutine calls using "mailbox" interrupts.
 */

#include <sys/types.h>

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/x_call.h>
#include <sys/spl.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/promif.h>
#include <sys/panic.h>

struct xc_mbox xc_mboxes[X_CALL_LEVELS];
static kmutex_t xc_mbox_lock[X_CALL_LEVELS];

u_int doing_capture_release = 0;

int sync_cpus = 0;
int xc_level_ignore = 0;	/* 0 == hi intr level x-calls checking */

static void xc_common(int (*)(), int, int, int, int, cpuset_t, int);
static void xc_grab_mutex(kmutex_t *mutex, int pri);

void
xc_init(void)
{
	/*
	 * By making these mutexes type MUTEX_DRIVER, the ones below
	 * LOCK_LEVEL will be implemented as adaptive mutexes, and the
	 * ones above LOCK_LEVEL will be spin mutexes.
	 */
	mutex_init(&xc_mbox_lock[0], NULL, MUTEX_DRIVER,
	    (void *)ipltospl(XC_LO_PIL));
	mutex_init(&xc_mbox_lock[1], NULL, MUTEX_DRIVER,
	    (void *)ipltospl(XC_MED_PIL));
	mutex_init(&xc_mbox_lock[2], NULL, MUTEX_DRIVER,
	    (void *)ipltospl(XC_HI_PIL));
}

/*
 * X-call interrupt service routine.
 *
 * Perform x-call session operations requested by the CPU that invoked
 * this x-call session.  This code gets called upon a level-N soft
 * interrupt.
 * CONTEXT BORROWING MUST BE ARRANGED BY SERVICE ROUTINE.
 *
 * We're protected against changing CPUs by the interrupt thread or by
 * being in a high-priority interrupt.
 *
 * NOTE:  Module specific code can provide an XC_MED_PRI fast trap
 *	  handler to inline heavily used functions.  For these
 *	  routines, the xc_serv() function will not be called.
 *	  Any change in the cross-call protocol should also be
 *	  made to these handler functions.  The variable
 *	  "level13_fasttrap_handler" is set for these modules.
 */

void
xc_serv(int pri)
{
	int	op;
	struct cpu *cpup = CPU;

	if (cpup->cpu_m.xc_pend[pri] == 0) {
		return;		/* interupt not due to x-call */
	}

	cpup->cpu_m.xc_pend[pri] = 0;

	/*
	 * If the setup_panic() routine captures a cpu while it is
	 * holding the OBP prom lock, the panic cpu will deadlock
	 * on its calls to the prom.  Since OBP allows the cpu
	 * holding the lock to recursively enter, a dummy call
	 * here will release the lock and allow the panic to
	 * continue.
	 */
	if (panicstr && doing_capture_release && pri == X_CALL_MEDPRI &&
	    cpup->cpu_id != panic_cpu.cpu_id) {
		(void) prom_mayput('\n');
	}

	/*
	 * Perform x-call session operations until the initiator of
	 * the x-call has indicated that the x-call session is done.
	 */
	while (cpup->cpu_m.xc_state[pri] != XC_DONE) {
		if (doing_capture_release) {
			/*
			 * Don't go on until we have something to do.
			 */
			while (cpup->cpu_m.xc_state[pri] == XC_HOLD ||
				cpup->cpu_m.xc_state[pri] == XC_WAIT)
				continue;

			/*
			 * After doing a hold, if there are no new operations
			 * to perform, then simply return.
			 */
			if (cpup->cpu_m.xc_state[pri] == XC_DONE) {
				/*
				 * Acknowledge we are exiting this
				 * x-call session.
				 */
				cpup->cpu_m.xc_ack[pri] = 1;
				return;
			}
		}

		op = cpup->cpu_m.xc_state[pri];

		/*
		 * Don't invoke a null function.
		 */
		if (xc_mboxes[pri].func != NULL)
			cpup->cpu_m.xc_retval[pri] = (*xc_mboxes[pri].func)
			    (xc_mboxes[pri].arg1, xc_mboxes[pri].arg2,
			    xc_mboxes[pri].arg3);
		else
			cpup->cpu_m.xc_retval[pri] = 0;

		/*
		 * Acknowledge that we have completed the x-call
		 * operation.
		 */
		cpup->cpu_m.xc_ack[pri] = 1;

		if (op == XC_SYNC_OP) {
			/*
			 * Wait for the initiator of the x-call to indicate
			 * that all CPUs involved can proceed.
			 */
			while (cpup->cpu_m.xc_wait[pri] != 0)
				continue;
			/*
			 * Inform caller that the wait bit has been seen
			 */
			cpup->cpu_m.xc_state[pri] = XC_WAIT;
		} else if (op == XC_CALL_OP) {
			/* all done */
			return;
		}
	}
}

/*
 * xc_call: call specified function on all processors
 * remotes may continue after service
 * we wait here until everybody has completed.
 */
void
xc_call(int arg1, int arg2, int arg3, int pri, cpuset_t set, int (*func)())
{
	/*
	 * Cross-calls at high interrupt levels should never happen.
	 * Attempting a cross-call at or above level13 is a panic situation.
	 */
	ASSERT((getpil() < XC_MED_PIL) || xc_level_ignore);

	/*
	 * If the pri indicates a low priority lock (below LOCK_LEVEL),
	 * we must disable preemption to avoid migrating to another CPU
	 * during the call.
	 */
	if (pri == X_CALL_LOPRI)
		kpreempt_disable();
	xc_grab_mutex(&xc_mbox_lock[pri], pri);
	xc_common(func, arg1, arg2, arg3, pri, set, 0);
	mutex_exit(&xc_mbox_lock[pri]);
	if (pri == X_CALL_LOPRI)
		kpreempt_enable();
}

/*
 * xc_sync: call specified function on all processors
 * after doing work, each remote waits until we let
 * it continue; send the contiunue after everyone has
 * informed us that they are done.
 */
void
xc_sync(int arg1, int arg2, int arg3, int pri, cpuset_t set, int (*func)())
{
	register int cix;
	register struct cpu *cpup;

	/*
	 * Cross-calls at high interrupt levels should never happen.
	 * Attempting a cross-call at or above level13 is a panic situation.
	 */
	ASSERT((getpil() < XC_MED_PIL) || xc_level_ignore);

	/*
	 * If the pri indicates a low priority lock (below LOCK_LEVEL),
	 * we must disable preemption to avoid migrating to another CPU
	 * during the call.
	 */
	if (pri == X_CALL_LOPRI)
		kpreempt_disable();
	xc_grab_mutex(&xc_mbox_lock[pri], pri);

	/* CPU cannot change now */
	xc_common(func, arg1, arg2, arg3, pri, set, 1);
	/*
	 * Now that all CPU's have done the call we can release them
	 */
	for (cix = 0; cix < NCPU; cix++) {
		cpup = cpu[cix];
		if (cpup != NULL && (cpup->cpu_flags & CPU_READY)) {
			cpup->cpu_m.xc_wait[pri] = 0;
			cpup->cpu_m.xc_state[pri] = XC_DONE;
		}
	}
	mutex_exit(&xc_mbox_lock[pri]);
	if (pri == X_CALL_LOPRI)
		kpreempt_enable();
}

/*
 * The routines xc_prolog, xc_sync_cache, and xc_epilog are used for
 * flushing the caches (TLB and VAC) on a set of CPUs.  Unlike the
 * three corresponding routines xc_capture_cpus, xc_vector_ops, and
 * xc_release_cpus, the three routines here do not capture the CPUs
 * over the entire session to implement a critical section.  Thus HAT
 * layer races must be dealt with differently.
 */

/*
 * Obtain mutually exclusive access to the medium level x-call mailbox
 * and store the specified set of CPUs to be involved in the x-call.
 */
void
xc_prolog(cpuset_t set)
{
	/*
	 * Cross-calls at high interrupt levels should never happen.
	 * Attempting a cross-call at or above level13 is a panic situation.
	 */
	ASSERT((getpil() < XC_MED_PIL) || xc_level_ignore);

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.xcalls, 1);

	/*
	 * Prevent deadlocks where we take an interrupt and are waiting
	 * for a mutex owned by one of the CPUs that is captured for
	 * the x-call, while that CPU is waiting for some x-call signal
	 * to be set by us.
	 *
	 * This mutex also prevents preemption, since it raises SPL above
	 * LOCK_LEVEL (it is a spin-type driver mutex).
	 */
	xc_grab_mutex(&xc_mbox_lock[X_CALL_MEDPRI], X_CALL_MEDPRI);

	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(CPU->cpu_flags & CPU_READY))
		return;
	/*
	 * Store the set of CPUs involved in the x-call session, so that
	 * xc_release_cpus will know what CPUs to act
	 * upon.
	 */
	xc_mboxes[X_CALL_MEDPRI].set = set;
}

/*
 * Perform X-call TLB/CACHE flush operation.
 */
void
xc_sync_cache(int arg1, int arg2, int arg3, int (*func)())
{
	register int cix;
	register int lcx = (int)(CPU->cpu_id);
	cpuset_t set = xc_mboxes[X_CALL_MEDPRI].set;
	register struct cpu *cpup;

	/*
	 * The medium-priority lock holds the spl above LOCK_LEVEL.  This
	 * prevents preemption and migration, so the thread's CPU pointer
	 * cannot change.
	 */
	ASSERT(MUTEX_HELD(&xc_mbox_lock[X_CALL_MEDPRI]));

	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(CPU->cpu_flags & CPU_READY)) {
		if (func != NULL)
			CPU->cpu_m.xc_retval[X_CALL_MEDPRI] =
			    (*func)(arg1, arg2, arg3);
		else
			CPU->cpu_m.xc_retval[X_CALL_MEDPRI] = 0;
		return;
	}

	/*
	 * Set up the service definition mailbox.
	 */
	xc_mboxes[X_CALL_MEDPRI].func = func;
	xc_mboxes[X_CALL_MEDPRI].arg1 = arg1;
	xc_mboxes[X_CALL_MEDPRI].arg2 = arg2;
	xc_mboxes[X_CALL_MEDPRI].arg3 = arg3;

	/*
	 * Request service on all remote processors.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL ||
		    (cpup->cpu_flags & CPU_READY) == 0) {
			/*
			 * In case CPU wasn't ready, but becomes ready later,
			 * take the CPU out of the set now.
			 */
			CPUSET_DEL(set, cix);
			continue;
		}
		if ((cix != lcx) && CPU_IN_SET(set, cix)) {
			cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 0;
			if (doing_capture_release) {
				cpup->cpu_m.xc_wait[X_CALL_MEDPRI] = 1;
				cpup->cpu_m.xc_state[X_CALL_MEDPRI]
					= XC_SYNC_OP;
			} else {
				cpup->cpu_m.xc_wait[X_CALL_MEDPRI] = 0;
				cpup->cpu_m.xc_state[X_CALL_MEDPRI]
					= XC_CALL_OP;
				/* Ordering is important! Must set pend last */
				cpup->cpu_m.xc_pend[X_CALL_MEDPRI] = 1;
				send_dirint(cix, XC_MED_PIL);
			}
		} else {
			cpup->cpu_m.xc_pend[X_CALL_MEDPRI] = 0;
			cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 1;
		}
	}

	/*
	 * Run service locally.
	 */
	if (CPU_IN_SET(set, lcx) && func != NULL)
		CPU->cpu_m.xc_retval[X_CALL_MEDPRI] = (*func)(arg1, arg2, arg3);

	/*
	 * Wait here until all remote calls to complete.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if (cix != lcx && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			while (cpup->cpu_m.xc_ack[X_CALL_MEDPRI] == 0)
				continue;
			cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 0;
			if (!doing_capture_release)
				cpup->cpu_m.xc_state[X_CALL_MEDPRI] = XC_DONE;
		}
	}
	if (!doing_capture_release)
		return;

	/*
	 * Let callee's continue to XC_WAIT state
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if ((cix != lcx) && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 0;
			cpup->cpu_m.xc_wait[X_CALL_MEDPRI] = 0;
		}
	}
	/*
	 * Don't start another operation until all CPU's have 'seen'
	 * the wait bit turned off
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if ((cix != lcx) && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			while (cpup->cpu_m.xc_state[X_CALL_MEDPRI] != XC_WAIT)
				continue;
		}
	}
}

/*
 * xc_epilog releases exclusive access to the medium priority x-call
 * mailbox.
 */
void
xc_epilog(void)
{
	ASSERT(MUTEX_HELD(&xc_mbox_lock[X_CALL_MEDPRI]));
	mutex_exit(&xc_mbox_lock[X_CALL_MEDPRI]);
}

/*
 * The routines, xc_capture_cpus, and xc_release_cpus
 * can be used in place of xc_sync in order to implement a critical
 * code section where all CPUs in the system can be controlled.  This
 * is useful, for example, in the hat layer when mappings are being changed
 * while another CPU has started using these mappings.
 * Xc_capture_cpus is used to start the critical code section, and
 * xc_release_cpus is used to end the critical code section.
 */

/*
 * Capture the CPUs specified in order to start a x-call session,
 * and/or to begin a critical section.
 */
void
xc_capture_cpus(cpuset_t set)
{
	register int cix;
	register int lcx;
	register struct cpu *cpup;

	/*
	 * Cross-calls at high interrupt levels should never happen.
	 * Attempting a cross-call at or above level13 is a panic situation.
	 */
	ASSERT((getpil() < XC_MED_PIL) || xc_level_ignore);

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.xcalls, 1);

	/*
	 * Prevent deadlocks where we take an interrupt and are waiting
	 * for a mutex owned by one of the CPUs that is captured for
	 * the x-call, while that CPU is waiting for some x-call signal
	 * to be set by us.
	 *
	 * This mutex also prevents preemption, since it raises SPL above
	 * LOCK_LEVEL (it is a spin-type driver mutex).
	 */
	xc_grab_mutex(&xc_mbox_lock[X_CALL_MEDPRI], X_CALL_MEDPRI);
	lcx = CPU->cpu_id;	/* now we're safe */


	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(CPU->cpu_flags & CPU_READY))
		return;

	doing_capture_release = 1;

	/*
	 * Build the set of CPUs involved in the x-call session, so that
	 * xc_release_cpus will know what CPUs to act upon.
	 *
	 * This must only be cpus that we have actually captured, since
	 * others may become ready later.
	 */
	if (CPU_IN_SET(set, lcx))
		xc_mboxes[X_CALL_MEDPRI].set = CPUSET(lcx);
	else
		xc_mboxes[X_CALL_MEDPRI].set = 0;

	/*
	 * Now capture each CPU in the set and cause it to go into a
	 * holding pattern.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL)
			continue;
		if ((cpup->cpu_flags & CPU_READY) &&
			(cix != lcx) && CPU_IN_SET(set, cix)) {
			CPUSET_ADD(xc_mboxes[X_CALL_MEDPRI].set, cix);
			cpup->cpu_m.xc_state[X_CALL_MEDPRI] = XC_HOLD;
			/* Ordering is important! Must set pend last */
			cpup->cpu_m.xc_pend[X_CALL_MEDPRI] = 1;
			send_dirint(cix, XC_MED_PIL);
		}
	}
}

/*
 * Release the CPUs captured by xc_capture_cpus, thus terminating the
 * x-call session and exiting the critical section.
 */
void
xc_release_cpus(void)
{
	register int cix;
	register int lcx = (int)(CPU->cpu_id);
	cpuset_t set = xc_mboxes[X_CALL_MEDPRI].set;
	register struct cpu *cpup;

	ASSERT(MUTEX_HELD(&xc_mbox_lock[X_CALL_MEDPRI]));
	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(CPU->cpu_flags & CPU_READY)) {
		mutex_exit(&xc_mbox_lock[X_CALL_MEDPRI]);
		return;
	}

	/*
	 * Allow each CPU to exit its holding pattern.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL)
			continue;
		if ((cpup->cpu_flags & CPU_READY) &&
		    (cix != lcx) && CPU_IN_SET(set, cix)) {
			/*
			 * Clear xc_ack since we will be waiting for it
			 * to be set again after we set XC_DONE.
			 */
			cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 0;
			cpup->cpu_m.xc_state[X_CALL_MEDPRI] = XC_DONE;
		}
	}

	doing_capture_release = 0;
	mutex_exit(&xc_mbox_lock[X_CALL_MEDPRI]);
}

/*
 * Common code for xc_call, xc_sync to call a specified
 * function on a set of processors.  If sync is set, wait until all remote
 * calls are completed.
 */
static void
xc_common(int (*func)(), int arg1, int arg2, int arg3, int pri, cpuset_t set,
    int sync)
{
	register int cix;
	register int lcx = (int)(CPU->cpu_id);
	register struct cpu *cpup;

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.xcalls, 1);
	ASSERT(MUTEX_HELD(&xc_mbox_lock[pri]));
	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(CPU->cpu_flags & CPU_READY)) {
		if (func != NULL)
			CPU->cpu_m.xc_retval[pri] = (*func)(arg1, arg2, arg3);
		else
			CPU->cpu_m.xc_retval[pri] = 0;
		return;
	}

	/*
	 * Set up the service definition mailbox.
	 */
	xc_mboxes[pri].func = func;
	xc_mboxes[pri].arg1 = arg1;
	xc_mboxes[pri].arg2 = arg2;
	xc_mboxes[pri].arg3 = arg3;

	/*
	 * Request service on all remote processors.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL ||
		    (cpup->cpu_flags & CPU_READY) == 0) {
			/*
			 * In case CPU wasn't ready, but becomes ready later,
			 * take the CPU out of the set now.
			 */
			CPUSET_DEL(set, cix);
			continue;
		}
		if (cix != lcx && CPU_IN_SET(set, cix)) {

			cpup->cpu_m.xc_ack[pri] = 0;
			cpup->cpu_m.xc_wait[pri] = sync;
			if (sync)
				cpup->cpu_m.xc_state[pri] = XC_SYNC_OP;
			else
				cpup->cpu_m.xc_state[pri] = XC_CALL_OP;
			/* Ordering is important! Must set pend last */
			cpup->cpu_m.xc_pend[pri] = 1;
			if (pri == (int)X_CALL_HIPRI)
				send_dirint(cix, XC_HI_PIL);
			else if (pri == (int)X_CALL_MEDPRI)
				send_dirint(cix, XC_MED_PIL);
			else
				send_dirint(cix, XC_LO_PIL);
		} else {
			cpup->cpu_m.xc_pend[pri] = 0;
			cpup->cpu_m.xc_ack[pri] = 1;
		}
	}

	/*
	 * Run service locally.
	 */
	if (CPU_IN_SET(set, lcx) && func != NULL)
		CPU->cpu_m.xc_retval[pri] = (*func)(arg1, arg2, arg3);

	/*
	 * Wait here until all remote calls to complete.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if (lcx != cix && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			while (cpup->cpu_m.xc_ack[pri] == 0) {
				continue;
			}
			cpup->cpu_m.xc_ack[pri] = 0;
		}
	}

	/*
	 * Let everyone continue in holding pattern if doing a sync call
	 */
	if (sync) {
		for (cix = 0; cix < NCPU; cix++) {
			cpup = cpu[cix];
			if (cpup != NULL && (cpup->cpu_flags & CPU_READY)) {
				/* TSO only. Won't work PSO */
				cpup->cpu_m.xc_state[X_CALL_MEDPRI] = XC_HOLD;
				cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 0;
				cpup->cpu_m.xc_wait[X_CALL_MEDPRI] = 0;
			}
		}
	}
}

static void
xc_grab_mutex(kmutex_t *mutex, int pri)
{
	int s;
	int pri_spl;
	int spl = 0;

	switch (pri) {
	case X_CALL_LOPRI:
		pri_spl = 1; break;
	case X_CALL_MEDPRI:
		pri_spl = 13; break;
	case X_CALL_HIPRI:
		pri_spl = 15; break;
	default:
		pri_spl = 0; break;
	}

	spl = ipltospl(pri_spl);
	while (mutex_tryenter(mutex) == 0) {
		/*
		 * If our priority is equal to or above the desired
		 * xcall level, call service function directly.
		 * Otherwise, let interrupt handler do it.
		 */
		if (getpil() >= pri_spl) {
			s = splr(spl);
			xc_serv(pri);
			splx(s);
		}
		if (panicstr) {
			/*
			 * Do spl bookkeeping (panicstr keeps from blocking)
			 */
			mutex_enter(mutex);
			return;
		}
	}
}
