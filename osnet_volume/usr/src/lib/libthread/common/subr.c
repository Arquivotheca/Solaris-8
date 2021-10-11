/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)subr.c	1.82	98/09/16 SMI"

#include "libthread.h"
#include <sys/reg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
 * Global variables
 */
uthread_t *_sched_owner;
uintptr_t _sched_ownerpc;

void
_sched_lock(void)
{
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_SC_LK_ENTER_START,
	    "_sched_lock enter start");
	_sigoff();
	curthread->t_schedlocked = 1;
	_lwp_mutex_lock(&_schedlock);
	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	_sched_owner = curthread;
	_sched_ownerpc = _getcaller();
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_SC_LK_ENTER_END,
	    "_sched_lock enter end");
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_SC_LK_CS_START,
	    "_sched_lock cs start");
}

void
_sched_unlock(void)
{
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_SC_LK_CS_END,
	    "_sched_lock cs end");
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_SC_LK_EXIT_START,
	    "_sched_lock exit start");
	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	ASSERT(_sched_owner == curthread);
	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	ASSERT(_sched_owner == curthread);
	_lwp_mutex_unlock(&_schedlock);
	curthread->t_schedlocked = 0;
	_sigon();
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_SC_LK_EXIT_END,
	    "_sched_lock exit end");
}

void
_sched_lock_nosig(void)
{
	curthread->t_schedlocked = 1;
	_lwp_mutex_lock(&_schedlock);
	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	_sched_owner = curthread;
	_sched_ownerpc = _getcaller();
}

void
_sched_unlock_nosig(void)
{
	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	_lwp_mutex_unlock(&_schedlock);
	curthread->t_schedlocked = 0;
}

#if defined(ITRACE) || defined(UTRACE)
_trace_sched_lock(void)
{
	_sigoff();
	curthread->t_schedlocked = 1;
	_lwp_mutex_lock(&_schedlock);
	_sched_owner = curthread;
	_sched_ownerpc = _getcaller();
}

_trace_sched_unlock(void)
{
	_lwp_mutex_unlock(&_schedlock);
	curthread->t_schedlocked = 0;
	_sigon();
}
#endif


static int _halted = 0;
static void _stackdump(uthread_t *t);
static char *exitmsg = "_signotifywait(): bad return; exiting process\n";

void
_panic(const char *s)
{
	extern _totalthreads;
	sigset_t ss;
	char buf[500];
	char *bp = buf;
	int sig;
	uthread_t *t = curthread;

	_halted = 1;
	sprintf(bp, "libthread panic: %s (PID: %d LWP %d)\n",
	    s, _getpid(), LWPID(curthread));
	_write(2, bp, strlen(bp));
	t->t_fp = _manifest_thread_state();
	_stackdump(t);
	_sigfillset(&ss);
	_sigdelset(&ss, SIGINT);
	if (curthread->t_schedlocked)
		curthread->t_hold = ss;
	else
		__thr_sigsetmask(SIG_SETMASK, &ss, NULL);
	/*
	 * Keep process idle until sent a SIGINT signal and then
	 * exit.
	 */
	while (1) {
		/*
		 * If this is the aslwp daemon thread that has panic'ed, then
		 * wait using _signotifywait().
		 */
		if (curthread->t_tid == __dynamic_tid) {
			sig = _signotifywait();
			if (sig <= 0) {
				write(2, exitmsg, strlen(exitmsg));
				exit(1);
			} else if (sig == SIGINT)
				exit(1);
			else
				/*
				 * Try to redirect. It might not have any
				 * effect since this is the aslwp and it has
				 * panic'ed implying that the signal could be
				 * directed to a thread which can never run
				 * because there are no lwps available and
				 * the aslwp has panic'ed so they cannot be
				 * created. But try anyway.
				 */
				_sigredirect(sig);
		} else {
			sig = _lwp_sigtimedwait(&ss, NULL, NULL, NULL);
			if (sig == SIGINT)
				exit(1);
		}
	}
}

int
_assfail(char *a, char *f, int l)
{
	char buf[128];
	char *bp = buf;

	sprintf(bp, "libthread assertion failed: %s, file: %s, line:%d\n",
		a, f, l);
	_write(2, bp, strlen(bp));
	_panic("assertion failed");
	return (0);
}

static void
_stackdump(uthread_t *t)
{
	unsigned long addr;
	char buf[20], *bp;
	struct frame *sp = (struct frame *)((char *)t->t_fp + STACK_BIAS);

	_write(2, "stacktrace:\n", 12);
	while (sp) {
		addr = (unsigned long) sp->fr_savpc;
		buf[19] = '\0';
		bp = &buf[19];
		do {
			*--bp = "0123456789abcdef"[addr%0x10];
			addr /= 0x10;
		} while (addr);
		_write(2, "\t", 1);
		_write(2, bp, strlen(bp));
		_write(2, "\n", 1);
		sp = (struct frame *)((char *)sp->fr_savfp + STACK_BIAS);
	}
}
