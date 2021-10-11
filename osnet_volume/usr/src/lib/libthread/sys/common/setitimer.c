#ident	"@(#)setitimer.c 1.3 95/12/05"

#include "../../common/libthread.h"
#include <sys/time.h>
#include <errno.h>

#pragma weak setitimer = _setitimer
#pragma weak alarm = _alarm

#pragma	weak _ti_alarm = _alarm
#pragma	weak _ti_setitimer = _setitimer

/*
 */

int
_setitimer(int which, const struct itimerval *it, struct itimerval *oit)
{
	register uthread_t *t = curthread;
	extern void _t_setitimer();
	int ret = 0;

	if (which == ITIMER_REAL) {
		/*
		 * For both bound and unbound threads, ITIMER_REAL causes
		 * a call to the kernel directly. The kernel trap currently
		 * supports a per-lwp semantic. For both posix and solaris
		 * bound and unbound threads, this semantic is retained.
		 * For unbound threads, this is clearly broken, but not
		 * anymore than it has always been. Also, for bound threads
		 * this does not completely work with respect to masking,
		 * i.e. thr_sigsetmask(3t). Wherease the latter problem is
		 * easily fixed, as in the case of the other flags below,
		 * this bug is deliberately not being fixed. These two
		 * bugs currently exist and fixing them for a semantic that
		 * is going to be EOL'ed does not make sense.
		 * In short, in the future, setitimer(ITIMER_REAL, ...) will
		 * offer a per-process semantic at which point both these
		 * bugs will be fixed.
		 */
			return(__setitimer(which, it, oit));
	} else {
		if (ISBOUND(t)) {
			if (t->t_flag & T_LWPDIRSIGS == 0) {
				t->t_flag |= T_LWPDIRSIGS;
				/*
				 * This may be optimized later by turning off
				 * the flag in the handler for the signals in
				 * __setitimersigs (VTALRM, PROF) if the 
				 * interval value is zero.
				 */
				if (sigand(&t->t_hold, &__lwpdirsigs))
					__sigprocmask(SIG_SETMASK, &t->t_hold,
					    NULL);
			}
			ret = __setitimer(which, it, oit);
			return (ret);
		} else {
			/*
	 		 * An unbound thread can now call into setitimer() for
			 * ITIMER_PROF or ITIMER_REALPROF with the following
			 * restrictions:
	 		 *	- it can be called only before any threads are
			 *	  created.
	 		 *	- a thread's mask will be silently ignored for
			 *	  this signal, i.e. the signal will be received
			 *	  without looking at the receiving thread's
			 *	  mask. See sigacthandler() in sigaction.c
	 		 * This is especially to support the MT collector tool.
			 * We are not documenting this in the man pages, since
			 * in the future, for unbound threads, we might want to
			 * change the signal to be sent to the process, if we
			 * support this for unb threads at all.
			 * Since no-one is screaming for it, why introduce a
			 * semantic in the man pages that we might need to
			 * withdraw later? 
			 * NOTE:
			 * But if this changes, talk to devpro FIRST, for the
			 * MT collector tool which depends on this working for
			 * unbound threads in this manner.
			 *
	 		 * For bound threads, there are no such restrictions as
			 * above.
			 */
			if (_first_thr_create != 0) {
				errno = EACCES;
				return(-1);
			} else {
				ret = __setitimer(which, it, oit);
				return (ret);
			}
		}
	}
}


void
_t_setitimer(arg)
        int arg;
{
        thread_t tid = (thread_t)arg;
        uthread_t *t;
        struct itimerval *itv;
        struct timeval interval;
        extern mutex_t _calloutlock;
        int ix;
 
        if (_thr_kill(tid, SIGALRM) == ESRCH) {
                /* if _thr_kill() is done to a non-existent tid */
                return;
        }
        mutex_lock(&_calloutlock);
        _lock_bucket((ix = HASH_TID(tid)));
        if ((t = THREAD(tid)) == (uthread_t *)-1) {
                /* if thread has disappeared by this time */
                _unlock_bucket(ix);
                return;
        }
        itv = &t->t_realitimer;
        interval = itv->it_interval;
        mutex_unlock(&_calloutlock);
        if (interval.tv_sec > 0 || interval.tv_usec > 0) {
                _setcallout(&t->t_itimer_callo, tid, &itv->it_interval,
                    _t_setitimer, tid);
	}
        _unlock_bucket(ix);
}

unsigned
_alarm(unsigned sec)
{
	if (_libpthread_loaded != 0)
		return(__alarm(sec));
	else
		return(__lwp_alarm(sec));
}
