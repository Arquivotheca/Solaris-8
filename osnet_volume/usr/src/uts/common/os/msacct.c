/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)msacct.c	1.2	94/01/29 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/debug.h>
#include <sys/msacct.h>

/*
 * Initialize the microstate level and the
 * associated accounting information for an LWP.
 */
void
init_mstate(
	register kthread_t	*t,
	register int		init_state)
{
	register struct mstate *ms;
	register klwp_t *lwp;
	hrtime_t curtime;

	ASSERT(init_state != LMS_WAIT_CPU);
	ASSERT((unsigned)init_state < NMSTATES);

	if ((lwp = ttolwp(t)) != NULL) {
		ms = &lwp->lwp_mstate;
		curtime = gethrtime();
		ms->ms_prev = LMS_SYSTEM;
		ms->ms_start = curtime;
		ms->ms_term = 0;
		ms->ms_state_start = curtime;
		t->t_mstate = init_state;
		t->t_waitrq = 0;
		bzero((caddr_t)&ms->ms_acct[0], sizeof (ms->ms_acct));
	}
}

/*
 * Change the microstate level for the LWP and update the
 * associated accounting information.  Return the previous
 * LWP state.
 */
int
new_mstate(
	register kthread_t	*t,
	register int		new_state)
{
	register struct mstate *ms;
	register unsigned state;
	register hrtime_t *mstimep;
	register hrtime_t curtime;
	klwp_t *lwp;

	ASSERT(new_state != LMS_WAIT_CPU);
	ASSERT((unsigned)new_state < NMSTATES);

	if ((lwp = ttolwp(t)) == NULL)
		return (LMS_SYSTEM);

	ms = &lwp->lwp_mstate;
	switch (state = t->t_mstate) {
	case LMS_TFAULT:
	case LMS_DFAULT:
	case LMS_KFAULT:
	case LMS_USER_LOCK:
		mstimep = &ms->ms_acct[LMS_SYSTEM];
		break;
	default:
		mstimep = &ms->ms_acct[state];
		break;
	}
	curtime = gethrtime();
	*mstimep += curtime - ms->ms_state_start;
	t->t_mstate = new_state;
	ms->ms_state_start = curtime;
	/*
	 * Remember the previous running microstate.
	 */
	if (state != LMS_SLEEP && state != LMS_STOPPED)
		ms->ms_prev = state;

	return (ms->ms_prev);
}

static long waitrqis0 = 0;

/*
 * Restore the LWP microstate to the previous runnable state.
 * Called from disp() with the newly selected lwp.
 */
void
restore_mstate(register kthread_t *t)
{
	register struct mstate *ms;
	register hrtime_t *mstimep;
	register klwp_t *lwp;
	register hrtime_t curtime;
	register hrtime_t waitrq;

	if ((lwp = ttolwp(t)) == NULL)
		return;

	ms = &lwp->lwp_mstate;
	ASSERT((unsigned)t->t_mstate < NMSTATES);
	switch (t->t_mstate) {
	case LMS_SLEEP:
		/*
		 * Update the timer for the current sleep state.
		 */
		ASSERT((unsigned)ms->ms_prev < NMSTATES);
		switch (ms->ms_prev) {
		case LMS_TFAULT:
		case LMS_DFAULT:
		case LMS_KFAULT:
		case LMS_USER_LOCK:
			mstimep = &ms->ms_acct[ms->ms_prev];
			break;
		default:
			mstimep = &ms->ms_acct[LMS_SLEEP];
			break;
		}
		/*
		 * Return to the previous run state.
		 */
		t->t_mstate = ms->ms_prev;
		break;
	case LMS_STOPPED:
		mstimep = &ms->ms_acct[LMS_STOPPED];
		/*
		 * Return to the previous run state.
		 */
		t->t_mstate = ms->ms_prev;
		break;
	case LMS_TFAULT:
	case LMS_DFAULT:
	case LMS_KFAULT:
	case LMS_USER_LOCK:
		mstimep = &ms->ms_acct[LMS_SYSTEM];
		break;
	default:
		mstimep = &ms->ms_acct[t->t_mstate];
		break;
	}
	waitrq = t->t_waitrq;	/* hopefully atomic */
	t->t_waitrq = 0;
	curtime = gethrtime();
	if (waitrq == 0) {	/* should only happen during boot */
		waitrq = curtime;
		waitrqis0++;
	}
	*mstimep += waitrq - ms->ms_state_start;
	/*
	 * Update the WAIT_CPU timer.
	 */
	ms->ms_acct[LMS_WAIT_CPU] += curtime - waitrq;
	ms->ms_state_start = curtime;
}

/*
 * Copy lwp microstate accounting and resource usage information
 * to the process.  (lwp is terminating)
 */
void
term_mstate(register kthread_t *t)
{
	register proc_t *p = ttoproc(t);
	register klwp_t *lwp = ttolwp(t);
	register int i;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if (t->t_proc_flag & TP_MSACCT) {
		register struct mstate *ms;

		ms = &lwp->lwp_mstate;
		(void) new_mstate(t, LMS_STOPPED);
		ms->ms_term = ms->ms_state_start;
		p->p_mlreal += ms->ms_term - ms->ms_start;
		for (i = 0; i < NMSTATES; i++)
			p->p_acct[i] += ms->ms_acct[i];
	}
	p->p_ru.minflt   += lwp->lwp_ru.minflt;
	p->p_ru.majflt   += lwp->lwp_ru.majflt;
	p->p_ru.nswap    += lwp->lwp_ru.nswap;
	p->p_ru.inblock  += lwp->lwp_ru.inblock;
	p->p_ru.oublock  += lwp->lwp_ru.oublock;
	p->p_ru.msgsnd   += lwp->lwp_ru.msgsnd;
	p->p_ru.msgrcv   += lwp->lwp_ru.msgrcv;
	p->p_ru.nsignals += lwp->lwp_ru.nsignals;
	p->p_ru.nvcsw    += lwp->lwp_ru.nvcsw;
	p->p_ru.nivcsw   += lwp->lwp_ru.nivcsw;
	p->p_ru.sysc	 += lwp->lwp_ru.sysc;
	p->p_ru.ioch	 += lwp->lwp_ru.ioch;
	p->p_defunct++;
}
