/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SUN	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)disp_lock.c	1.16	99/03/23 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/vtrace.h>
#include <sys/lockstat.h>
#include <sys/spl.h>

/* ARGSUSED */
void
disp_lock_init(disp_lock_t *lp, char *name)
{
	DISP_LOCK_INIT(lp);
}

/* ARGSUSED */
void
disp_lock_destroy(disp_lock_t *lp)
{
	DISP_LOCK_DESTROY(lp);
}

/*
 * Most of the disp_lock_* routines are implemented as aliases of,
 * or trivial prologues to, the lock_* assembly language routines.
 * We provide C versions here for illustration (and for lint).
 */
#ifdef lint

void
disp_lock_enter_high(disp_lock_t *lp)
{
	lock_set(lp);
}

void
disp_lock_exit_high(disp_lock_t *lp)
{
	lock_clear(lp);
}

void
disp_lock_enter(disp_lock_t *lp)
{
	lock_set_spl(lp, ipltospl(LOCK_LEVEL), &curthread->t_oldspl);
}

void
disp_lock_exit_nopreempt(disp_lock_t *lp)
{
	lock_clear_splx(lp, curthread->t_oldspl);
}

void
disp_lock_exit(disp_lock_t *lp)
{
	if (CPU->cpu_kprunrun) {
		lock_clear_splx(lp, curthread->t_oldspl);
		kpreempt(KPREEMPT_SYNC);
	} else {
		lock_clear_splx(lp, curthread->t_oldspl);
	}
}

#endif	/* lint */

/*
 * Thread_lock() - get the correct dispatcher lock for the thread.
 */
void
thread_lock(kthread_id_t t)
{
	int s = splhigh();

	for (;;) {
		lock_t *volatile *tlpp = &t->t_lockp;
		lock_t *lp = *tlpp;
		if (lock_try(lp)) {
			if (lp == *tlpp) {
				curthread->t_oldspl = (u_short)s;
				return;
			}
			lock_clear(lp);
		} else {
			u_int spin_count = 1;
			/*
			 * Lower spl and spin on lock with non-atomic load
			 * to avoid cache activity.  Spin until the lock
			 * becomes available or spontaneously changes.
			 */
			splx(s);
			while (lp == *tlpp && LOCK_HELD(lp)) {
				if (panicstr) {
					curthread->t_oldspl = splhigh();
					return;
				}
				spin_count++;
			}
			LOCKSTAT_RECORD(LS_THREAD_LOCK, lp, spin_count, 1);
			s = splhigh();
		}
	}
}

/*
 * Thread_lock_high() - get the correct dispatcher lock for the thread.
 *	This version is called when already at high spl.
 */
void
thread_lock_high(kthread_id_t t)
{
	for (;;) {
		lock_t *volatile *tlpp = &t->t_lockp;
		lock_t *lp = *tlpp;
		if (lock_try(lp)) {
			if (lp == *tlpp)
				return;
			lock_clear(lp);
		} else {
			u_int spin_count = 1;
			while (lp == *tlpp && LOCK_HELD(lp)) {
				if (panicstr)
					return;
				spin_count++;
			}
			LOCKSTAT_RECORD(LS_THREAD_LOCK, lp, spin_count, 1);
		}
	}
}

/*
 * Called by THREAD_TRANSITION macro to change the thread state to
 * the intermediate state-in-transititon state.
 */
void
thread_transition(kthread_id_t t)
{
	disp_lock_t	*lp;

	ASSERT(THREAD_LOCK_HELD(t));
	ASSERT(t->t_lockp != &transition_lock);

	lp = t->t_lockp;
	t->t_lockp = &transition_lock;
	disp_lock_exit_high(lp);
}
