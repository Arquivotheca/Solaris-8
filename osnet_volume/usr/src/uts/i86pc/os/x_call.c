/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)x_call.c	1.32	99/12/06 SMI"

/*
 * Facilities for cross-processor subroutine calls using "mailbox" interrupts.
 *
 */

#include <sys/types.h>

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/x_call.h>
#include <sys/psw.h>
#include <sys/sunddi.h>
#include <sys/mmu.h>
#include <sys/debug.h>
#include <sys/machsystm.h>
#include <sys/mutex_impl.h>

static struct	xc_mbox xc_mboxes[X_CALL_LEVELS];
static kmutex_t xc_mbox_lock[X_CALL_LEVELS];
static uint_t 	xc_xlat_xcptoipl[X_CALL_LEVELS] =
		{XC_LO_PIL, XC_MED_PIL, XC_HI_PIL};

static void xc_common(int (*)(), int, int, int, int, cpuset_t, int);

int	xc_initted = 0;
extern ulong_t	cpu_ready_set;
extern void atomic_andl(unsigned long *addr, unsigned long val);

void
xc_init()
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
	xc_initted = 1;		/* for kadb only		*/
}

#define	CAPTURE_CPU_ARG	0xffffffff

/*
 * X-call interrupt service routine.
 *
 * arg == X_CALL_MEDPRI	-  tlbflush broadcast or capture cpus.
 *
 * We're protected against changing CPUs by
 * being a high-priority interrupt.
 */
uint_t
xc_serv(caddr_t arg)
{
	register int	op;
	register int	pri = (int)arg;
	register struct cpu *cpup = CPU;
	uint_t	mask, *argp;
	caddr_t	addr;

	if (pri == X_CALL_MEDPRI) {

		argp = (uint_t *)&xc_mboxes[X_CALL_MEDPRI].arg2;
		if (*argp != CAPTURE_CPU_ARG) {
			mask = cpup->cpu_mask;
			if (!(*argp & mask))
				return (DDI_INTR_UNCLAIMED);
			addr = (caddr_t)xc_mboxes[X_CALL_MEDPRI].arg1;
			atomic_andl((ulong_t *)argp, ~mask);
			if ((int)addr != -1)
				mmu_tlbflush_entry((caddr_t)addr);
			else
				mmu_tlbflush_all();
			return (DDI_INTR_CLAIMED);
		}
		if (cpup->cpu_m.xc_pend[pri] == 0)
			return (DDI_INTR_UNCLAIMED);


		cpup->cpu_m.xc_pend[X_CALL_MEDPRI] = 0;
		cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 1;


		for (;;) {
			if ((cpup->cpu_m.xc_state[X_CALL_MEDPRI] == XC_DONE) ||
				(cpup->cpu_m.xc_pend[X_CALL_MEDPRI]))
				break;
			/*
			 * make sure the compiler will not store the states
			 * in registers.
			 */
			return_instr();
		}
		/*
		 * currently we do capture_release only for flushing
		 * page directory and to reload cr3 with kernel_only_cr3
		 * if this cpu is associated with NULL address space.
		 *
		 */
		mmu_loadcr3(cpup, NULL);
		return (DDI_INTR_CLAIMED);
	}
	if (cpup->cpu_m.xc_pend[pri] == 0)
		return (DDI_INTR_UNCLAIMED);

	cpup->cpu_m.xc_pend[pri] = 0;
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
	 * Acknowledge that we have completed the x-call operation.
	 */
	cpup->cpu_m.xc_ack[pri] = 1;

	if (op == XC_CALL_OP)
		return (DDI_INTR_CLAIMED);

	/*
	 * for (op == XC_SYNC_OP)
	 * Wait for the initiator of the x-call to indicate
	 * that all CPUs involved can proceed.
	 */
	while (cpup->cpu_m.xc_wait[pri])
		return_instr();

	while (cpup->cpu_m.xc_state[pri] != XC_DONE)
		return_instr();
	return (DDI_INTR_CLAIMED);
}


/*
 * xc_call: call specified function on all processors
 * remotes may continue after service
 * we wait here until everybody has completed.
 */
void
xc_call(int arg1, int arg2, int arg3, int in_pri, cpuset_t set, int (*func)())
{
	register int pri;

	/*
	 * If the pri indicates a low priority lock (below LOCK_LEVEL),
	 * we must disable preemption to avoid migrating to another CPU
	 * during the call.
	 */
	if (in_pri == X_CALL_LOPRI) {
		kpreempt_disable();
		pri = X_CALL_LOPRI;
	} else
		pri = X_CALL_HIPRI;

	/* always grab highest mutex to avoid deadlock */
	mutex_enter(&xc_mbox_lock[X_CALL_HIPRI]);
	xc_common(func, arg1, arg2, arg3, pri, set, 0);
	mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
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
xc_sync(int arg1, int arg2, int arg3, int in_pri, cpuset_t set, int (*func)())
{
	register int cix;
	register struct cpu *cpup;
	register int pri;

	/*
	 * If the pri indicates a low priority lock (below LOCK_LEVEL),
	 * we must disable preemption to avoid migrating to another CPU
	 * during the call.
	 */
	if (in_pri == X_CALL_LOPRI) {
		kpreempt_disable();
		pri = X_CALL_LOPRI;
	} else
		pri = X_CALL_HIPRI;

	/* always grab highest mutex to avoid deadlock */
	mutex_enter(&xc_mbox_lock[X_CALL_HIPRI]);
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
	mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
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
	/* always grab highest mutex to avoid deadlock */
	mutex_enter(&xc_mbox_lock[X_CALL_HIPRI]);
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
 * xc_epilog releases exclusive access to the medium priority x-call
 * mailbox.
 */
void
xc_epilog(void)
{
	ASSERT(MUTEX_HELD(&xc_mbox_lock[X_CALL_HIPRI]));
	/* always grab highest mutex to avoid deadlock */
	mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
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
	int	i;
	caddr_t	addr;
	volatile uint_t	*iptr;

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
	/* always grab highest mutex to avoid deadlock */
	mutex_enter(&xc_mbox_lock[X_CALL_HIPRI]);
	lcx = CPU->cpu_id;	/* now we're safe */

	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(CPU->cpu_flags & CPU_READY)) {
		mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
		return;
	}


	/*
	 * Wait for all cpu's to respod to tlbflush broadcast.
	 */
	iptr = (uint_t *)&xc_mboxes[X_CALL_MEDPRI].arg2;
	if (*iptr & CPU->cpu_mask) {
		addr = (caddr_t)xc_mboxes[X_CALL_MEDPRI].arg1;
		if ((int)addr != -1)
			mmu_tlbflush_entry(addr);
		else
			mmu_tlbflush_all();
		atomic_andl((ulong_t *)iptr, ~CPU->cpu_mask);
	}
	while (*iptr & cpu_ready_set) {
	}

	/*
	 * Store the set of CPUs involved in the x-call session, so that
	 * xc_release_cpus will know what CPUs to act upon.
	 */
	xc_mboxes[X_CALL_MEDPRI].set = set;
	xc_mboxes[X_CALL_MEDPRI].arg2 = CAPTURE_CPU_ARG;

	/*
	 * Now capture each CPU in the set and cause it to go into a
	 * holding pattern.
	 */
	i = 0;
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
			cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 0;
			cpup->cpu_m.xc_state[X_CALL_MEDPRI] = XC_HOLD;
			cpup->cpu_m.xc_pend[X_CALL_MEDPRI] = 1;
			send_dirint(cix, XC_MED_PIL);
		}
		i++;
		if (i >= ncpus)
			break;
	}

	/*
	 * Wait here until all remote calls to complete.
	 */
	i = 0;
	for (cix = 0; cix < NCPU; cix++) {
		if (lcx != cix && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			while (cpup->cpu_m.xc_ack[X_CALL_MEDPRI] == 0) {
				/* dummy call to avoid the compiler not */
				/* fetching */
				return_instr();
			}
			cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 0;
		}
		i++;
		if (i >= ncpus)
			break;
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
	int	i;

	ASSERT(MUTEX_HELD(&xc_mbox_lock[X_CALL_HIPRI]));

	/*
	 * Allow each CPU to exit its holding pattern.
	 */
	i = 0;
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL)
			continue;
		if ((cpup->cpu_flags & CPU_READY) &&
		    (cix != lcx) && CPU_IN_SET(set, cix)) {
			/*
			 * Clear xc_ack since we will be waiting for it
			 * to be set again after we set XC_DONE.
			 */
			cpup->cpu_m.xc_state[X_CALL_MEDPRI] = XC_DONE;
		}
		i++;
		if (i >= ncpus)
			break;
	}

	xc_mboxes[X_CALL_MEDPRI].arg2 = 0;
	mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
}



uint_t		tlb_flush_gen;
void
xc_broadcast_tlbflush(cpuset_t set, caddr_t addr, uint_t *gen)
{
	register int cix;
	register struct cpu *cpup;
	int	i;
	volatile uint_t	*iptr;
	caddr_t	paddr;

	/*
	 * Need to ensure that we do mmu_tlbflush on the same CPU
	 * without migrating
	 */
	kpreempt_disable();

	cpup = CPU;

	CPU_STAT_ADDQ(cpup, cpu_sysinfo.xcalls, 1);

	/* always grab highest mutex to avoid deadlock */
	mutex_enter(&xc_mbox_lock[X_CALL_HIPRI]);

	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(cpup->cpu_flags & CPU_READY)) {
		*gen = tlb_flush_gen;
		mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
		kpreempt_enable();
		return;
	}

	iptr = (uint32_t *)&xc_mboxes[X_CALL_MEDPRI].arg2;
	if (*iptr & cpup->cpu_mask) {
		paddr = (caddr_t)xc_mboxes[X_CALL_MEDPRI].arg1;
		atomic_andl((ulong_t *)iptr, ~cpup->cpu_mask);
		if ((int)paddr != -1)
			mmu_tlbflush_entry(paddr);
		else
			mmu_tlbflush_all();
	}
	while (*iptr & cpu_ready_set) {
	}
	xc_mboxes[X_CALL_MEDPRI].arg1 = (int)addr;
	set = xc_mboxes[X_CALL_MEDPRI].arg2 =  (set & cpu_ready_set)
			& ~cpup->cpu_mask;
	tlb_flush_gen += 2;
	*gen = tlb_flush_gen;
	i = 0;
	for (cix = 0; cix < NCPU; cix++) {
		if ((cpup = cpu[cix]) == NULL ||
		    (cpup->cpu_flags & CPU_READY) == 0) {
			continue;
		}
		if (CPU_IN_SET(set, cix)) {
			send_dirint(cix, XC_MED_PIL);
		}
		i++;
		if (i >= ncpus)
			break;
	}
	mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
	/* do the flush on our own CPU */
	if ((int)addr != -1)
		mmu_tlbflush_entry(addr);
	else
		mmu_tlbflush_all();
	kpreempt_enable();
}



void
xc_waitfor_tlbflush(uint_t gen)
{
	volatile uint_t	*iptr;


	if (tlb_flush_gen != gen)
		return;
	iptr = (uint_t *)&xc_mboxes[X_CALL_MEDPRI].arg2;
	while ((tlb_flush_gen == gen) && (*iptr & cpu_ready_set));
	return;

}

/*
 * Common code for xc_call, xc_sync to call a specified
 * function on a set of processors.  If sync is set, wait until all remote
 * calls are completed.
 */
static void
xc_common(int (*func)(), int arg1, int arg2, int arg3, int pri,
    cpuset_t set, int sync)
{
	register int cix;
	register int lcx = (int)(CPU->cpu_id);
	register struct cpu *cpup;

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.xcalls, 1);
	ASSERT(MUTEX_HELD(&xc_mbox_lock[X_CALL_HIPRI]));
	/*
	 * If we are not playing ball with anyone else,
	 * then don't ask them to play ball with us.
	 */
	if (!(CPU->cpu_flags & CPU_READY)) {
		if (CPU_IN_SET(set, lcx) && func != NULL)
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
		} else if (cix != lcx && CPU_IN_SET(set, cix)) {
			cpup->cpu_m.xc_ack[pri] = 0;
			cpup->cpu_m.xc_wait[pri] = sync;
			if (sync == 1)
				cpup->cpu_m.xc_state[pri] = XC_SYNC_OP;
			else
				cpup->cpu_m.xc_state[pri] = XC_CALL_OP;
			cpup->cpu_m.xc_pend[pri] = 1;
			send_dirint(cix, xc_xlat_xcptoipl[pri]);
		}
	}

	/*
	 * Run service locally.
	 */
	if (CPU_IN_SET(set, lcx) && func != NULL)
		CPU->cpu_m.xc_retval[pri] = (*func)(arg1, arg2, arg3);

	if (sync == -1)
		return;

	/*
	 * Wait here until all remote calls to complete.
	 */
	for (cix = 0; cix < NCPU; cix++) {
		if (lcx != cix && CPU_IN_SET(set, cix)) {
			cpup = cpu[cix];
			while (cpup->cpu_m.xc_ack[pri] == 0) {
				/* dummy call to avoid the compiler not */
				/* fetching */
				return_instr();
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
				cpup->cpu_m.xc_state[pri] = XC_HOLD;
				cpup->cpu_m.xc_ack[pri] = 0;
				cpup->cpu_m.xc_wait[pri] = 0;
			}
		}
	}
}

/*
 * xc_call_debug: call specified function on all processors
 * remotes may wait for a long time
 * we continue immediately
 */
void
xc_call_debug(int arg1, int arg2, int arg3, int pri, cpuset_t set,
    int (*func)())
{
	register	int	save_kernel_preemption;
	extern		int	IGNORE_KERNEL_PREEMPTION;

	/*
	 * Just try to grab the mutex.  If this fails, I'm afraid
	 * the other processors are left running while we are in
	 * kadb.  This is better then deadlocking, especially when
	 * another processor might be in the middle of an xc_call,
	 * and wants this processor to finish its request before it
	 * will continue.
	 */

	save_kernel_preemption = IGNORE_KERNEL_PREEMPTION;
	IGNORE_KERNEL_PREEMPTION = 1;
	if (mutex_tryenter(&xc_mbox_lock[X_CALL_HIPRI])) {
		xc_common(func, arg1, arg2, arg3, pri, set, -1);
		mutex_exit(&xc_mbox_lock[X_CALL_HIPRI]);
	}
	IGNORE_KERNEL_PREEMPTION = save_kernel_preemption;
}

/*
 * xc_call_kadb: call specified function on all processors
 * remotes may wait for a long time.
 * we continue immediately.
 *
 * Note: this is called from kadb with interrupts disabled (cli).
 */
void
xc_call_kadb(int arg1, int arg2, int arg3, int pri, cpuset_t set,
    int (*func)())
{
	register	int	save_kernel_preemption;
	extern		int	IGNORE_KERNEL_PREEMPTION;
	register	mutex_impl_t *lp;
	register	int x;

	/*
	 * Just try to grab the mutex.  If this fails, I'm afraid
	 * the other processors are left running while we are in
	 * kadb.  This is better then deadlocking, especially when
	 * another processor might be in the middle of an xc_call,
	 * and wants this processor to finish its request before it
	 * will continue.
	 */

	save_kernel_preemption = IGNORE_KERNEL_PREEMPTION;
	IGNORE_KERNEL_PREEMPTION = 1;

	lp = (mutex_impl_t *)&xc_mbox_lock[X_CALL_HIPRI];
	for (x = 0; x < 0x400000; x++) {
		if (lock_try(&lp->m_spin.m_spinlock)) {
			xc_common(func, arg1, arg2, arg3, pri, set, -1);
			lock_clear(&lp->m_spin.m_spinlock);
			break;
		}
		(void) xc_serv((caddr_t)X_CALL_MEDPRI);
	}
	IGNORE_KERNEL_PREEMPTION = save_kernel_preemption;
}
