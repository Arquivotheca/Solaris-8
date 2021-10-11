#ident	"@(#)sigpending.c 1.5 96/03/28"

#ifdef	__STDC__
#pragma weak	_ti_sigpending = sigpending
#endif /* __STDC__ */

#include "../../common/libthread.h"
#include <signal.h>
#include <errno.h>

int
sigpending(sigset_t *set)
{
	sigset_t ppsigs;
	uthread_t *t = curthread;

	/*
	 * Get directed pending signals
	 */
	_sched_lock();
	sigandset(set, &t->t_hold, &t->t_psig);
	_sched_unlock();
	/*
	 * Get non-directed (to process) pending signals
	 */
	__mt_sigpending(&ppsigs);
	_lwp_mutex_lock(&_pmasklock);
	sigorset(&ppsigs, &_pmask);
	_lwp_mutex_unlock(&_pmasklock);
	sigandset(&ppsigs, &t->t_hold, &ppsigs);
	sigorset(set, &ppsigs);
	return (0);
}
