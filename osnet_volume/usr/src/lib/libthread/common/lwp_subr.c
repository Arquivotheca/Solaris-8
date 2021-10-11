/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)lwp_subr.c	1.35	98/10/25 SMI"

#include "libthread.h"

/*
 * the thread library uses this routine when it needs another LWP.
 */
int
_new_lwp(struct thread *t, void (*func)(), int door)
{
	uthread_t *aging = NULL;
	int flags;
	int ret;

	flags = LWP_DETACHED|LWP_SUSPENDED;
	if (t == NULL) {
		int size;
		if (door)
			size = DOOR_STACK;
		else
			size = DAEMON_STACK;
		/*
		 * increase the pool of LWPs on which threads execute.
		 */
		if ((aging = _idle_thread_create(size, func)) == NULL)
			return (EAGAIN);
		if (door)
			aging->t_flag |= T_DOORSERVER;
		else {
			_sched_lock();
			_nlwps++;
			_sched_unlock();
		}
		ret = _lwp_exec(aging, (uintptr_t)_thr_exit,
		    (caddr_t)aging->t_sp, _lwp_start, flags, &aging->t_lwpid);
		if (ret) {
			/* if failed */
			_thread_free(aging);
			if (!door) {
				_sched_lock();
				_nlwps--;
				_sched_unlock();
			}
			return (ret);
		}
		ASSERT(aging->t_lwpid != NULL);
		_lwp_continue(aging->t_lwpid);
		return (0);
	} else {
		ASSERT(t->t_usropts & THR_BOUND);
		/* _thread_start will call _sc_setup */
		ret = _lwp_exec(t, (uintptr_t)_thr_exit,
		    (caddr_t)t->t_sp, func, flags, &t->t_lwpid);
		if (ret)
			return (ret);
		_sc_add(t);
		if ((t->t_usropts & THR_SUSPENDED) == 0) {
			_sched_lock();
			t->t_state = TS_ONPROC;
			_sched_unlock();
			ASSERT(t->t_lwpid != 0);
			_lwp_continue(t->t_lwpid);
		}
		return (0);
	}
}

void
_lwp_start(void)
{
	uthread_t *t = curthread;

	_sc_setup(-1, 1);
	(*(void (*)())(t->t_startpc))();
	_thr_exit(NULL);
}
